# MSB Stat File — Runner Tracking (proposal)

Status: proposal only, no code written. Branch `RunnerStats` (off 2.2.0, `24154dd7b3`).

Sources: the decomp at `mssb-dtk` (`src/game/baserunning/runner.c`,
`include/game/baserunning/runner_types.h`, `src/game/data_only/gameplay_tables.c`),
`Source/Core/Core/MSB_StatTracker.cpp/.h` on this branch, and ProjectRio-web `develop`
(`df9754d`, Alembic head `a7c3e9f10b24`). Everything below comes from reading code. Nothing has
been run against a live game yet, so section 9 lists what must be confirmed first.

Goals, in the order they drove the design:

1. A play can be recreated afterwards from the stat file.
2. Sprint and baserunning efficiency can be calculated from the stat file alone.
3. The addition to the stat file is as small as possible.

---

## 1. Summary

- Add one event-level block, `"Baserunning"`, holding a short **state timeline** per runner: a
  row each time the runner changes what they are doing (start, brake, stop, reverse, pass a
  base, slide, out, score). A row is five integers.
- Two of those integers are sprint accumulators (frames at top speed, and how many of those
  frames were fully sprinted). They are enough to calculate sprint efficiency and frames lost.
- Runner movement in this game is one-dimensional along the base path and deterministic given
  the character's speed, so a timeline of state changes recreates the run without per-frame
  position samples.
- Estimated cost: about 15–20 KB per game on top of today's ~240 KB (6–8%). Per-frame position
  sampling would cost about 30 KB per play.
- The existing `Runner Batter` / `Runner 1B` / `Runner 2B` / `Runner 3B` blocks keep their exact
  shape. The server depends on them being byte-identical across the pitches of a plate
  appearance (section 7.1), so nothing new goes inside them.
- Backend: two new tables and one new endpoint, no changes to existing tables.

---

## 2. What exists today

The tracker runs once per frame (`PatchEngineCallback` → `Core::RunRioFunctions` →
`StatTracker::Run`), so per-frame runner reads are available. One `Event` is created per pitch.

Runner data today is the `Runner` struct (`MSB_StatTracker.h:695`): roster loc, char id, initial
base, out type, out location, result base, steal. It is read by `logRunnerInfo` once at event
start and `logRunnerEvents` per frame in two states only.

Problems worth fixing as part of this work, whatever happens to the rest of the proposal:

| # | Problem | Fix |
|---|---|---|
| 1 | Runners are not read at all in `CONTACT_RESULT`, the stretch between contact and the ball landing or being caught. The batter is not read in `PITCH_RESULT` either. | Sample in every live state. |
| 2 | `MONITOR_RUNNERS` skips the runner read on the frame the play ends, so `result_base` is one frame stale. | Read on the transition frame too. |
| 3 | A runner who scores ends with `Runner Result Base` 4 on some paths and 0 on others (the 2.1.1 sample has both). 0 is also what a batter who never left home shows. | Use `runnerStatus` (3 or 4 = scored) and always write 4. |
| 4 | `aRunner_BasepathPercentage` (`0x8088EE7C`) is not a percentage. It is `fractionalBasesRan`: cumulative bases as a float, 0.0 at the plate to 4.0 back at the plate. | Rename; read as a float. |
| 5 | `aRunner_OutLoc` (`0x8088EF3F`) is `baseRunningTowards` and is never used; `Out Location` actually reads `currentBase`. | Rename the constant. Leave the output as is. |
| 6 | `runner_batter.value()` and `pitch->contact->` are dereferenced without checks (`.cpp:411, 911, 943`). | Guard them. |
| 7 | `"Event Num"` is written through a `u8` (`.cpp:1124`) and wraps after 255 events. | Write the `u16`. |

Item 3 changes a value the server reads (`result_base == 4` means scored), in the direction the
server already expects.

---

## 3. Engine facts the design relies on

Runner array: `g_Runners = 0x8088EE18`, four slots of `0x154` bytes. Slot 0 is the batter-runner;
slots 1–3 are the runners who **started the play** on 1B/2B/3B. A runner keeps their slot for the
whole play and is only moved to a new slot between plays. An empty slot has `runnerStatus == 0`.

### 3.1 Position is one number

`fractionalBasesRan` holds everything about where a runner is: the integer part is the last base
passed and the fraction is progress along the current leg. A runner standing on a base reads
`base + 0.03`; a runner at full lead-off reads `base + 0.13`. World position is a straight-line
interpolation between the two base coordinates, except while rounding a base, where the game
blends toward a fixed curve.

Base coordinates (x, z): home (0.4, 0.3), 1B (18.8, 19.4), 2B (0.0, 38.7), 3B (−18.5, 19.3).
A leg is about 26.5–27 units.

### 3.2 Speed model

Per frame, in `running_calcSpeedVariables` (`runner.c:2788-2872`):

```
v   += acceleration                       // sign follows the commanded direction
cap  = vmax * staminaMult * (0.66 if a steal is active, else 1.0)
v    = clamp(v, -cap, +cap)
if v hit the cap:  update sprint meter, then v += 0.02 * meter
else:              meter = 0
fractionalBasesRan += v / legLength
```

| Quantity | Value |
|---|---|
| `vmax` | Lookup by the character's speed stat, every 10 points from 0 to 210: 0.132, 0.135, 0.137, 0.139, 0.141, 0.143, 0.145, 0.148, 0.151, 0.154, **0.157** (speed 100), 0.160, 0.164, 0.168, 0.173, 0.176, 0.179, 0.182, 0.185, 0.188, 0.190, 0.190. Linear between entries. |
| Acceleration | 0.010 at speed 0 falling linearly to 0.005 at speed 100, then 0.005. Braking and turning always use 0.005. |
| Stamina | Starts at 4000 each play, −1 per frame while moving, −3 more per frame while turning. `staminaMult = 1 − 0.3 × (1 − stamina/4000)`. About 2% slower after 300 frames of running. |
| Batter | Cannot move until 30 frames after contact. The first leg is automatic apart from sprint. |
| Other runners | Forced to start forward 5 frames after contact. Player commands are ignored for the first 20 live frames (10 on a high fly). |
| Direction change | A new command is ignored within 30 frames of the previous change. A runner must brake to zero before reversing: about 31 frames from top speed at speed 100. |
| Stop at a base | Velocity is zeroed. Restarting costs the whole acceleration ramp, about 16 frames lost at speed 100. |
| Slide | Automatic, not a player choice. Arrival time is unchanged, but the runner is frozen for a per-character recovery (25 frames for the first table row) before they can go on. |

No captain, star, chemistry or difficulty term appears in the runner speed code.

### 3.3 Sprint

Sprint is a mash meter, not a held button (`runner.c:2848-2872`, `4244-4249`).

- A new press of A, B, X or Y adds 0.2 to the meter (maximum one press per frame, meter capped
  at 1.0, so five presses fill it).
- After a press the meter holds for 6 frames, then decays by 0.05 per frame.
- The bonus is `0.02 × meter` units per frame: +12.7% at speed 100, about 19 frames saved per leg.
- **The meter only exists at top speed.** On any frame the runner is below the speed cap
  (accelerating, braking, stopped, restarting) the meter is reset to zero.
- **Presses only count with the control stick neutral.** Holding the stick to command a single
  runner takes a code path that never registers a sprint press.
- The meter is wiped whenever the runner leaves top speed, so reading it at the end of a leg
  says nothing. It has to be accumulated frame by frame during the play.

Consequence for the design: on every frame at top speed the runner's speed is exactly
`cap + 0.02 × meter`. Two totals per segment describe sprint completely: the number of frames
at the cap, and the sum of the meter over those frames.

---

## 4. Proposed stat-file structure

### 4.1 The block

One new key per event, written after the `Runner` blocks and omitted when no runner has a row:

```json
"Baserunning": {
  "Contact Frame": 48,
  "Runners": [
    {"Runner": 0, "Path": [[78,0,1,140,96],[232,1004,11,0,0],[310,1030,7,0,0]]},
    {"Runner": 1, "Path": [[53,1130,1,131,118],[201,2004,1,139,139],[356,2960,8,0,0],[372,3030,7,0,0]]}
  ]
}
```

| Key | Meaning |
|---|---|
| `Contact Frame` | The event-clock frame on which contact happened. Omitted when there was no contact. Fielding and hazard events use frames-since-contact, so `Contact Frame + their frame` puts all three lists on one clock. |
| `Runner` | Slot 0–3: the same slot as `Runner Batter` / `1B` / `2B` / `3B` in the same event, and the same value the FielderStats `Outs` list uses. Identity (roster loc, char id) is read from that block, not repeated here. |
| `Path` | The runner's state timeline. One row per change, in frame order. |

A file-level `"Baserunning Version": 1` goes in the header so the row format can change later.

### 4.2 A path row

`[frame, bases, state, cap frames, sprint frames]`

| Column | Meaning |
|---|---|
| frame | Event clock: frames since the pitch or pickoff that started this event. |
| bases | `fractionalBasesRan × 1000`, rounded. 1130 is a runner at full lead off first. A scored runner is always written as 4000. |
| state | What the runner is doing **from this row until their next row** (codes below). |
| cap frames | Frames at the speed cap during this state. |
| sprint frames | Sum of the sprint meter over those frames, rounded: the number of full-sprint frames it was worth. Always ≤ cap frames. |

State codes (tracker-defined; 0 is unused so a zeroed row is detectable):

| Code | State | Engine condition |
|---|---|---|
| 1 | Forward | `runningDirectionCode` forward, nothing pending |
| 2 | Back | Running backward by command |
| 3 | Forced Back | Running backward because a fly was caught (`tagUpStatus` = must return) |
| 4 | Braking | A stop or reversal has been accepted and the runner is slowing |
| 5 | Stopped | At rest off a base |
| 6 | Held | At rest because the game blocked movement (`movementRestrictions` ≠ 0) |
| 7 | On Base | `baseStandingOn` ≥ 0 |
| 8 | Slide | `actionCode` 1, until arrival |
| 9 | Body Check Won | `actionCode` 2 |
| 10 | Body Check Failed | `actionCode` 3 |
| 11 | Overrun | Running through 1B or overrunning 2B/3B |
| 12 | Out | `runnerStatus` 2 (the out type stays in the `Runner` block) |
| 13 | Scored | `runnerStatus` 3 or 4 |

When to write a row:

- The state changes.
- The runner passes a base without changing state (the whole number of `bases` changes). This
  gives a split time per leg when a runner rounds a base.
- The event ends and the runner's last row is not On Base, Out or Scored.

A runner who never leaves their base gets no entry. Cap at 24 rows per runner per event, with
`"Truncated": 1` on the runner object if it is hit (long rundowns).

Proposed: write nothing on foul balls (runners are reset) or on home runs (the trot is automatic
and carries no decision).

In the decoded file the state column is written as its label; the file sent to the server keeps
the number, like every other enum.

### 4.3 Why positional rows

The rest of the stat file spells out every key on every line, which is why a seven-field runner
block costs about 230 bytes. A path row written that way would be about 150 bytes; as a
five-integer array it is about 22. This is the one place the proposal departs from the file's
key-per-line convention, and it is the main reason the addition stays under 10%.

### 4.4 Per-character block

In `Character Game Stats → Roster N`, a new `"Baserunning Stats"` object. Only one value in it
cannot be derived from the paths:

| Key | Meaning |
|---|---|
| `Run Speed` | The speed stat the game used for this character (`runner.speed`, after any superstar boost), captured at their first plate appearance. Needed to compute ideal times offline. |

Game totals (section 5) are computed by the server at ingest and by any offline tool from the
paths. Writing them in the stat file too would add about 6 KB for no new information. If totals
are wanted in the decoded file for people reading it by hand, they can be added to that file only.

### 4.5 Size

Estimated from the two sample files (94 and 101 events, about 2.2 runners per ball in play), not
measured:

| Approach | Per ball in play | Per game |
|---|---|---|
| This proposal | ~330 B | ~15–20 KB |
| Same rows with spelled-out keys | ~2 KB | ~100 KB |
| Position sample every frame, 4 runners | ~30 KB | over 1 MB |

---

## 5. Stats this makes possible

Recorded directly: the path rows, `Contact Frame`, `Run Speed`. Everything below is derived.

| Stat | From |
|---|---|
| Runs scored, exact, per runner | A `Scored` row. Replaces the server's current guess from RBI order. |
| Home-to-first, first-to-third, second-to-home times | Frames between base-crossing rows |
| Sprint efficiency | sprint frames ÷ cap frames |
| Frames lost to missed sprint | Section 6 |
| Start delay | First `Forward` row versus the earliest possible frame |
| Tag-up reaction | Frames from the catch (fielding events) to the next `Forward` row |
| Steal jump | Departure frame, with the existing `Steal` level (normal or perfect) |
| Steal attempts, successes, caught stealing | `Steal` plus the final row |
| Hesitations and reversals | Count and duration of `Braking`, `Stopped` and `Back` rows off a base |
| Rundowns | Two or more reversals in one path |
| Extra bases taken | Bases gained versus the batter's hit type |
| Outs on the bases by type | `Out` row plus `Out Type` |
| Doubled off | `Forced Back` followed by `Out` |
| Slides, body checks won and lost | Rows 8–10 |
| Safe or out margin | Arrival frame versus the throw's arrival (needs FielderStats events) |
| Time blocked by the game | `Held` and `Forced Back` rows, so they are not counted against the player |

---

## 6. Calculating how optimal a run was

Inputs: the path, `Run Speed`, and the constants in section 3.

For each row and the row after it (one segment):

- **Actual time** = difference in frames. **Distance** = difference in bases × leg length.
- **Sprint efficiency** = sprint frames ÷ cap frames.
- **Frames lost to sprint** ≈ `0.02 × (cap frames − sprint frames − 2) ÷ (cap + 0.02)`.
  The 2 is the unavoidable cost of filling the meter, charged once each time the runner reaches
  top speed.
- **Ideal time** for the segment: run the section 3.2 loop from the segment's starting speed with
  the meter filled as fast as allowed. This is about 20 lines of code and needs nothing that is
  not in the file.

Where the lost frames went:

| Cause | How to measure it |
|---|---|
| Late start | First `Forward` frame minus the earliest allowed: `Contact Frame + 5` for runners, `+ 30` for the batter, the catch frame for a tag-up, the pitch frame for a steal |
| Missed sprint | Formula above |
| Sprint lost to the stick | Cap frames with zero sprint frames on a play where the player was issuing per-runner commands. Not separable from "did not mash" without recording inputs (section 9, question 7). |
| Hesitation | Frames in `Braking`, `Stopped` and `Back`, plus the acceleration ramp after each restart (`vmax ÷ 2a`, about 16 frames at speed 100) |
| Stopping at a base that was then left | The same ramp |
| Slide recovery | Per-character constant, when the runner continued afterwards |
| Not the player's doing | `Held` and `Forced Back` |

---

## 7. Backend (ProjectRio-web)

### 7.1 Constraint that shaped the stat-file design

`process_game` reuses a `runner` row when an event's `Runner X` dict is equal to the previous
event's dict for the same slot, and it counts a plate appearance each time a **new**
`Runner Batter` row is created (`app/views/populate_db.py:596-625`). Adding varying data inside
those dicts would make every pitch look like a new plate appearance on any server that has not
been updated. Hence the separate `"Baserunning"` key. Unknown event-level keys are ignored, so
old servers accept new files safely.

Other facts that matter:

- The server receives the non-decoded file and does not keep it: the file is deleted after
  ingest. New blocks are detected by key presence, not by version string.
- The server numbers events by their index in the array and ignores `"Event Num"`.
- A slot resolves to that event's `runner_on_<slot>` row; a roster loc plus `Half Inning`
  resolves to a `character_game_summary` row.
- `/events/` inner-joins `pitch_summary`, so events with no pitch (pickoffs, some steals) are
  invisible to every endpoint built on it.

### 7.2 Tables

New tables only. Existing tables are untouched, matching the FielderStats backend plan.

**`runner_play`** — one row per runner per event that has a path.

| Column | Notes |
|---|---|
| `event_id` | FK, indexed, not null |
| `character_game_summary_id` | FK, indexed, not null |
| `runner_id` | FK to `runner.id` via `runner_on_<slot>`, nullable |
| `runner_slot` | 0–3. Unique with `event_id`, which also guards double ingest. |
| `start_bases`, `end_bases` | ×1000, from the first and last rows |
| `result` | State code of the last row |
| `start_frame`, `end_frame`, `contact_frame` | |
| `cap_frames`, `sprint_frames` | Summed over the path |
| `brakes`, `reversals` | Counted from the path |
| `path` | The row array as JSON text, about 90 bytes |

**`character_baserunning_summary`** — zero or one row per `character_game_summary` (the child
holds a unique FK). Integer totals computed at ingest: `run_speed`, `runs_scored`,
`bases_advanced`, `extra_bases_taken`, `steal_attempts`, `steals`, `caught_stealing`,
`outs_on_bases`, `doubled_off`, `cap_frames`, `sprint_frames`, `frames_run`, `brakes`. `/stats/`
sums these instead of scanning event-sized tables.

A per-leg table (`runner_leg`: from base, to base, split frames, sprint totals) is the natural
third table for queries such as "first-to-third time by character". It can wait: because the path
is stored, legs can be backfilled later without new client data.

Storage: about 130 `runner_play` rows per game, about 12 KB of path text. At 50,000 games that is
roughly 6.5 million rows and 600 MB, the same order as `event`. If that is too much, drop the
`path` column and archive the raw stat files (gzip instead of delete) so replays stay possible.

### 7.3 Ingest

- Read `event.get("Baserunning")`; absent means no rows. Old clients keep working unchanged.
- Wrap the new inserts in `db.session.begin_nested()` so a bad block cannot fail the game.
- Fix the stale `pitch_summary` variable used for events with no `Pitch`
  (`populate_db.py:735, 773`). Steal and pickoff events are exactly where this data matters.
- Add the new tables to `/delete_game/` and `/init_db/` ahead of `event` and
  `character_game_summary`.
- One Alembic revision. The FielderStats tables, the contact-result data migration and the
  `classicCommunities` branch all want to sit on `a7c3e9f10b24`, so agree a single order first.

### 7.4 Endpoints

- New `GET /runner_data/`: takes the usual game and event filters or `events=`, returns per event
  the runners with their summary fields, and the path with `include_path=1`. It must not go
  through the pitch inner join.
- `/stats/`: a Baserunning category from `character_baserunning_summary`, plus a count of tracked
  appearances so date ranges mixing old and new games are not diluted. Switch runs scored to the
  exact value when the row exists.
- `/events/` filters (`runner_char`, `leg_result`, `steal`) as `EXISTS` subqueries.
- Ongoing games: nothing in the first version.

---

## 8. Implementation steps (emulator)

One step at a time, each testable on its own.

1. **Fix the existing reads** (section 2 table). No new output.
2. **Add the addresses and a per-frame sampler** called in `PITCH_RESULT`, `CONTACT_RESULT`,
   `MONITOR_RUNNERS` and on the play-over frame. Print to console only.
3. **Add the event clock** and `Contact Frame`. Check it against frames-since-contact
   (`0x8089269E`).
4. **Build the state machine** that turns samples into path rows, without sprint columns. Write
   the `"Baserunning"` block in `getStatJSON` and `getHUDJSON`.
5. **Add the sprint accumulators.**
6. **Add `Run Speed`** and the header version key.
7. **Backend**, in the order given in section 7.

New address constants, slot 0, add `slot × 0x154`:

| Field | Address | Type |
|---|---|---|
| `fractionalBasesRan` | `0x8088EE7C` | f32 |
| `groundVelocity[0]` | `0x8088EE9C` | f32 |
| `maximumBaseVelocity` | `0x8088EEB8` | f32 |
| `mashPercent` | `0x8088EEE0` | f32 |
| `staminaMult` | `0x8088EEEC` | f32 |
| `baseStandingOn` | `0x8088EEFE` | s16 |
| `movementRestrictions` | `0x8088EF08` | s16 |
| `speed` | `0x8088EF38` | u8 |
| `runnerStatus` | `0x8088EF3B` | u8 |
| `tagUpStatus` | `0x8088EF40` | u8 |
| `nextDirectionBeingProcessed` | `0x8088EF4E` | u8 |
| `runningDirectionCode` | `0x8088EF4F` | u8 |
| `actionCode` | `0x8088EF52` | u8 |
| `overrun1BStage` | `0x8088EF58` | u8 |
| `overrunBaseStage` | `0x8088EF5A` | u8 |

"At the cap" is not stored by the game. Detect it as `mashPercent > 0`, or
`|groundVelocity[0]| ≥ 0.999 × maximumBaseVelocity × staminaMult × (0.66 if stealingStatus ≠ 0)`,
with no slide in progress.

---

## 9. Open questions and what to confirm in testing

1. **Positional rows.** Section 4.3 breaks the file's key-per-line style to save about 80 KB per
   game. This is the main decision to make before coding.
2. **The clock.** The tracker counts its own calls. Confirm that is one game frame each, and what
   happens if the game is paused or lags mid-play. After contact, `frame − Contact Frame` must
   equal the game's frames-since-contact.
3. **Steal timing.** The perfect and normal steal departures are at pitch-counter frames 15 and
   40. How that counter lines up with the event clock was not traced.
4. **Scored runners.** `fractionalBasesRan` ends at 4.0 after a slide and wraps to 0.03 otherwise.
   The tracker should write 4000 from `runnerStatus`; confirm both paths.
5. **Batter's first leg.** It starts from the batter's position in the box, not the plate, so
   its length varies slightly. Confirm what `fractionalBasesRan` reads before the batter moves,
   and whether the box position needs recording.
6. **Sprint read timing.** `mashPercent` is assumed stable when the tracker reads it. Check by
   comparing against speed above the cap on the same frame.
7. **Controller inputs.** Without them, "did not mash" and "mashed with the stick held" look the
   same. Recording a press count per segment would separate them at one more column.
8. **Auto-run and CPU.** A human team can turn on auto-running, where the AI picks directions
   and the player only sprints. The flag (`g_GameLogic + 0x148`, per side) should be recorded
   once per game; its address and indexing are unverified.
9. **Two-dimensional accuracy.** Straight lines between bases are exact except while rounding.
   If a replay needs the true curve, apply the game's rounding table offline before considering
   recording x/z.
10. **FielderStats overlap.** Out order, out frame and putout credit belong to that plan's `Outs`
    list. Both plans need the same event-level placement and the same `Runner` slot value.
