# MSB Fielder Stats — Rio-web backend plan

Written 2026-11-14. Companion to `docs/MSB_FielderStats_Plan.md` (the emulator plan),
which stops at "decide with the Rio server owners" in its step 7. This document is that
decision, worked out against the server code.

**Nothing here has been implemented.** It is a plan for the `ProjectRio-web` repo, read
at `develop` (`df9754d`). `app/models.py` and `app/views/populate_db.py` are identical on
`develop`, `main` and the local `Landing_new_Fields` branch, so line numbers below hold
for all three. The Alembic head is `a7c3e9f10b24`.

## 1. Summary

- **Five new tables, no changes to existing stat tables.** The existing
  `fielding_summary` ("First Fielder") keeps being filled, so every current reader works
  unchanged for old and new games.
- **Three nullable JSON columns on `ongoing_game`** for live fielding totals and a
  last-play summary.
- **Ingest** reads the new blocks by key presence, never by version string, and isolates
  them so a malformed fielding block cannot lose a game.
- **Existing endpoints.** `/delete_game/` and `/init_db/` fail for new games unless they
  are updated. `/stats/`, `/events/`, `/landing_data/`, the two recreate endpoints and
  `/box_score/` should expose the new data (§7).
- **One new endpoint**, `/fielding_data/`, is worth adding (§8).
- **Seven contract points** in the emulator plan's JSON need settling before either side
  is coded (§3). As drafted, the first one leaves part of the data impossible to resolve
  to a character.

## 2. What the server does today

Only the parts this work touches.

**End-of-game ingest.** The client posts the non-decoded stat file
(`getStatJSON(false, false)`, `MSB_StatTracker.cpp:581`) to `POST /populate_db/`, which
validates the players and writes the JSON to disk. A background job runs
`process_game()` on each file once a minute. So every enum arrives as a raw number, not
the decoded label shown in the emulator plan's examples.

**Relational shape.** `process_game()` builds two chains:

```
game
 ├─ character_game_summary (18 per game)  ──▶ character_position_summary (1:1)
 └─ event (1 per pitch / steal / pickoff)
      ├─ runner ×1–4
      └─ pitch_summary ──▶ contact_summary ──▶ fielding_summary ──▶ character_game_summary
                                               (First Fielder only)
```

Two conventions matter for the new tables:

- Every one-to-many link has the child holding the foreign key (`event.game_id`,
  `character_game_summary.game_id`, `runner.runner_character_game_summary_id`,
  `fielding_summary.fielder_character_game_summary_id`).
- A fielder is never stored as a roster location. Ingest resolves the roster location to
  a `character_game_summary.id` using the fielding team for that half inning
  (`populate_db.py:700-703`: half inning 0 means the away team bats, so the home roster
  fields).

**Who reads fielding data.**

| Reader | What it uses |
|---|---|
| `/stats/` → `query_detailed_fielding_stats` (`stat_retrieval.py:1097`) | sums of `character_position_summary`, `big_plays`, and counts over `fielding_summary` (jumps, dives, wall jumps, swaps, bobbles) |
| `/events/` → `_build_event_filters` (`stat_retrieval.py:271`) | `fielder_char`, `fielder_pos` filters on the first fielder |
| `/landing_data/` (`stat_retrieval.py:499`) | first-fielder position, action, jump, coordinates, manual select |
| `/recreate_stat_file/`, `/recreate_events/` | rebuilds `First Fielder` |
| `/delete_game/`, `/init_db/` | delete or drop each stat table by name, in dependency order |

**Ongoing games.** `POST /populate_db/ongoing_game/` (`populate_db.py:16`) keeps one
`ongoing_game` row per live game. The first post creates it; later posts overwrite the
live fields. Arrays and stat snapshots are stored verbatim in JSON columns
(`away_fielding_positions`, `pitcher_stats`, `batter_stats`, …). Every field is read
with `request.json['Key']`, so a missing key is a 500. The row is deleted when the final
stat file is processed.

## 3. Contract points to settle with the emulator plan first

These are about the JSON in `MSB_FielderStats_Plan.md` §5. Fixing them in the plan is
cheap; fixing them after stat files exist is not.

1. **Every fielder reference must be a roster location.** In the §5.1 example, `Outs`
   uses roster locations (`"Putout": 2`, `"Assists": [6]`), but `Fielding Summary` uses
   the position code (`"Credited Fielder": 5`, `"Big Play": {"Fielder": 5}`: the
   shortstop is position 5, roster location 6). The server can only resolve a roster
   location to a character. Make `Credited Fielder`, `Big Play.Fielder` and
   `Error.Charged To` roster locations.
2. **Rename the `Outs` array.** `"Outs"` already exists at event level as the out count
   before the play (`populate_db.py:584`). The array is safe only while it stays inside
   `Contact`, and point 3 moves it out. Call it `"Fielding Outs"`.
3. **Hang the three blocks off the event, not the contact.** Strikeouts, steals and
   pickoffs have no `Contact` (the 2.1.1 sample in `app/tests/data` has two events with
   no `Pitch` at all). The emulator plan wants strikeout putouts credited to the catcher
   (§4.4) and leaves pickoff defense open (§6, last note). With `Fielding Summary`,
   `Fielding Events` and `Fielding Outs` as direct children of the event, all of those
   have somewhere to live. The server will accept either location (§5).
4. **Fix numbers for the three enums the tracker invents.** Engine bytes can be sent
   as-is, but `Event`, `Chemistry` and `Error.Attribution` have no engine value.
   Proposed:

   | Field | Values |
   |---|---|
   | `Event` | 1 Catch, 2 Pickup, 3 Receive, 4 Throw, 5 Bobble, 6 Drop, 7 Knockout, 8 Wall Splat, 9 Clamber, 10 Wall Jump, 11 Body Check, 12 Tag (0 unused, so a zeroed struct is detectable) |
   | `Chemistry` | 0 none, 1 chemistry, 2 chemistry capped, 3 anti-chemistry (folds `IsChemistryThrow` and `IsAntichemistryThrow` into one field) |
   | `Error.Attribution` | 0 last thrower (the game's own rule), 1 first handler |

   Everything else is the raw engine value the emulator plan already documents:
   `Catch Type` 1–8, `Action` 0/2/3, `Bobble` 0–4 and 16, `Catch Height` and
   `Catch Side` 0–2, `Thrown To` −1/0/1/2/3/5/6, `Out Type` 1/2/3/4/16, `Error.Type`
   1–3, positions 0–8.
5. **Add positions to out records.** Add `"Putout Position"` and `"Assist Positions"`
   (parallel to `Assists`). The server cannot always derive them: a strikeout putout has
   no fielding event for the catcher, and characters change position mid-game.
6. **Say what `On Base` is.** The example has `"On Base": 1` for a first baseman, which
   reads as either a flag or base code 1. Send the base code from `baseCurrentlyOn`
   (−1 when not on a base).
7. **Use one key for the roster location.** `First Fielder` uses
   `"Fielder Roster Location"`; the new events use `"Fielder Roster Loc"`. Either is
   fine, but the new blocks should use one spelling throughout.

## 4. Schema

All five tables are new. Nothing is added to `event`, `contact_summary`,
`character_game_summary` or `fielding_summary`, so the migration creates tables and
never alters the large ones.

```
character_game_summary ◀── character_fielding_summary   (0..1 per character per game)

event ◀── fielding_play_summary                         (0..1 per event)
event ◀── fielding_event                                (0..N per event, ordered by sequence)
event ◀── fielding_out ◀── fielding_assist              (0..3 outs per event, 0..N assists per out)

fielding_play_summary, fielding_event, fielding_out and fielding_assist
each point at character_game_summary for the fielder(s) involved.
```

Enums are plain `Integer` columns, like every existing enum in `models.py`. Flags that
are strictly 0/1 are `Boolean`. Every foreign key gets `index=True`: the lesson of
migration `a7c3e9f10b24` was that Postgres does not index them for you.

### 4.1 `fielding_event`

One row per entry in `Fielding Events`.

| Column | Type | JSON source |
|---|---|---|
| `id` | Integer PK | |
| `event_id` | FK `event.id`, not null | parent event |
| `fielder_character_game_summary_id` | FK, not null | `Fielder Roster Loc` |
| `sequence`, `parent_sequence`, `frame` | Integer | same names |
| `event_type` | Integer | `Event` (§3 point 4) |
| `position` | Integer | `Fielder Position` |
| `fielder_x_pos`, `fielder_y_pos`, `fielder_z_pos` | Float, nullable | `Position - X/Y/Z` |
| `catch_type`, `action`, `catch_height`, `catch_side`, `bobble`, `manual_select`, `on_base` | Integer, nullable | `Details` of Catch / Pickup / Receive / Bobble / Drop |
| `jump`, `running_catch`, `caught_in_air`, `swap` | Boolean, nullable | `Jump`, `Running Catch`, `Caught In Air`, `Swapped For Batter` |
| `thrown_to`, `receiver_position`, `frames_to_arrive`, `speed_type`, `chemistry` | Integer, nullable | `Details` of Throw |
| `receiver_character_game_summary_id` | FK, nullable | `Receiver Roster Loc` |
| `throw_distance` | Float, nullable | `Throw Distance` |
| `smash_throw`, `quick_throw`, `laser` | Boolean, nullable | same names |

Unique constraint on `(event_id, sequence)`, which also guards against a double ingest.
`Fielder Char Id` is not stored; it comes from the character summary, as it does for
`First Fielder` today.

The catch and throw details share one table with nullable columns instead of a child
table per event type. Counting dives or averaging throw distance per character is then
a single-table query, and unused nullable columns cost almost nothing in Postgres.

### 4.2 `fielding_out` and `fielding_assist`

One `fielding_out` row per entry in `Fielding Outs`.

| Column | Type | JSON source |
|---|---|---|
| `id` | Integer PK | |
| `event_id` | FK `event.id`, not null | parent event |
| `runner_id` | FK `runner.id`, nullable | the event's `runner_on_<Runner>` row |
| `putout_character_game_summary_id` | FK, not null | `Putout` |
| `putout_position` | Integer | `Putout Position` |
| `sequence`, `out_number`, `frame`, `base` | Integer | same names |
| `out_type` | Integer | `Out Type`, same codes as `runner.out_type` |
| `runner_slot` | Integer | `Runner` (0 batter, 1–3 bases) |
| `unassisted`, `infield_fly`, `double_play` | Boolean | same names |

`fielding_assist` has `id`, `fielding_out_id` (FK), `fielder_character_game_summary_id`
(FK), `position` and `assist_order`. A child table, not an array column, so "assists by
this fielder" is an indexed count.

### 4.3 `fielding_play_summary`

One row per event that carries a `Fielding Summary`. `event_id` is a unique FK.

| Column | Type | JSON source |
|---|---|---|
| `fielders_handling_ball`, `throws`, `bobbles` | Integer | same names |
| `credited_fielder_character_game_summary_id` | FK, nullable | `Credited Fielder` |
| `big_play_fielder_character_game_summary_id` | FK, nullable | `Big Play.Fielder` |
| `big_play_confirmed` | Boolean | `Big Play.Confirmed` |
| `error_fielder_character_game_summary_id` | FK, nullable | `Error.Charged To` |
| `error_type`, `error_attribution` | Integer, nullable | `Error.Type`, `Error.Attribution` |
| `truncated` | Boolean | `Truncated` (emulator step 3's 64-event cap) |

### 4.4 `character_fielding_summary`

The per-character aggregates from the emulator plan §5.2. `character_game_summary_id`
is a unique FK.

| Group | Columns (all Integer) |
|---|---|
| Counting stats | `putouts`, `assists`, `errors`, `chances`, `throws`, `pickups`, `big_plays_tracked` |
| Catches | `catches_routine`, `catches_running`, `catches_diving`, `catches_jumping`, `catches_wall_jump`, `catches_clamber`, `catches_thrown_ball` |
| Bobbles | `bobbles_knockback`, `bobbles_fumble`, `bobbles_drop`, `bobbles_star_hit` |
| Actions | `dives`, `wall_jumps`, `clambers`, `wall_splats`, `knockouts`, `body_checks` |

These are derivable from the event rows, but `/stats/` is built to sum per-character
summary rows and avoid scanning event-sized tables, and `fielding_event` will be the
largest table in the database (§4.6). They also give a free cross-check between the
tracker's running totals and its event list.

### 4.5 Why these choices

- **Parent is `event`, not `contact_summary`.** See §3 point 3. The contact is still one
  join away (`event → pitch_summary → contact_summary`).
- **The child holds the FK on the two one-to-one tables as well.**
  `character_position_summary` does it the other way round (the character row points at
  it). Copying that would mean adding a nullable column and an index to
  `character_game_summary` and `event`, and every pre-existing row would carry a NULL.
  With the child holding the FK, old games simply have no row.
- **`fielding_summary` stays.** The emulator keeps emitting `First Fielder`, so it keeps
  covering every game ever played. The new tables only cover games from the fielder
  stats release onward.

### 4.6 Size

The 2.1.1 sample in `app/tests/data` (6 innings) has 94 events, 59 contacts, 52 fielded
balls and 31 runner outs. At two to three fielding events per fielded ball, expect
roughly 100–150 `fielding_event` rows, about 30 `fielding_out` rows and 18
`character_fielding_summary` rows per game. That is an estimate: `fielding_event` will
have more rows than `event`, which is the largest table today. Measure it from the first
real stat files before deploying.

## 5. End-of-game ingest (`process_game`)

1. **Character aggregates.** In the character loop (`populate_db.py:445`), after the
   `CharacterGameSummary` is committed, create a `CharacterFieldingSummary` only when
   `'Putouts' in defensive_stats`. Read each counter with `.get(key, 0)`.
2. **Per-event blocks.** After the event row is committed (`populate_db.py:726`), look
   for the three blocks on the event first, then inside `Pitch.Contact`:

   ```python
   contact = event_data.get('Pitch', {}).get('Contact', {})
   src = event_data if 'Fielding Events' in event_data or 'Fielding Outs' in event_data else contact
   summary = src.get('Fielding Summary')
   f_events = src.get('Fielding Events', [])
   f_outs = src.get('Fielding Outs', src.get('Outs', []) if src is contact else [])
   ```

   Resolve every roster location with the same rule as `First Fielder`:
   `teams['Home']` when `Half Inning == 0`, else `teams['Away']`. Resolve
   `fielding_out.runner_id` from the event's `runner_on_<slot>`.
3. **Detect by key presence, not `Version`.** The contact-result handoff found that
   `"2.3.0"` was stamped on builds before and after a format change, and that version
   strings sort as text. A game has fielder stats if the keys are there.
4. **Isolate the new code.** `process_game` commits row by row inside one bare
   `except` that returns `'Unknown Error'` (`populate_db.py:800`). By the time events
   are processed, the `Game` row, the `GameHistory` row and the rating change are
   already committed, so an exception in new fielding code would leave a half-ingested
   game. Wrap each event's fielding rows in `db.session.begin_nested()`. On failure,
   roll back that savepoint, log the game id and event number, and carry on. Have
   `process_all_games` keep a copy of the file as `defect_fielding_<name>` (the
   `defect_` prefix is already skipped by the loop) so it can be replayed.
5. **Batch the inserts.** The existing code commits after every row. Add all of an
   event's fielding rows with `add_all` and commit once, or ingest time roughly doubles.
6. **Keep the `First Fielder` block as it is.**
7. **HUD-resumed games** (`Loaded from HUD == 1`) return before any character or event
   rows are written (`populate_db.py:424`), so they get no fielding rows either. No
   change needed.

`POST /populate_db/` itself needs no code change. It writes the body to disk untouched.
The body grows with the event list, so confirm the reverse proxy's request-size limit
against a real fielder-stats file (the sample files in the repo are 230–365 KB).

## 6. Ongoing-game ingest

### 6.1 What to send

The emulator plan's "end-of-game only" default is right for the event list, but live
fielding totals are small and worth having. Add three optional keys to both
`postOngoingGame` and `updateOngoingGame`:

```json
"Away Fielding Stats": { "Putouts": [0,1,4,2,0,3,1,2,1], "Assists": [..9..], "Errors": [..9..], "Big Plays": [..9..] },
"Home Fielding Stats": { "…": "same shape" },
"Last Play": { "Event Num": 41, "Result of AB": 15, "Fielders": [5, 3, 2], "Outs": 2, "Error Position": -1, "Big Play": 0 }
```

The arrays are indexed by roster location, matching `Away CharIDs` and
`Away Fielding Positions`. `Last Play.Fielders` is the positions that handled the ball,
in order, which is enough for a scoreboard to show "6-4-3".

Two properties of the client decide this shape:

- **Updates can be skipped.** The background submitter replaces an update that is still
  queued with the newer one (`MSB_StatTracker.cpp:2427`) and discards its queue on
  shutdown. So every payload must be a full snapshot. Cumulative per-roster totals
  survive a skipped update; per-play deltas would silently lose plays. `Last Play` is
  best-effort for the same reason.
- **Updates are frequent and the GET returns everything.** One update goes out per
  event, and `GET /populate_db/ongoing_game/` returns every live game's full row in one
  response. The full `Fielding Events` list does not belong here; it arrives in the
  final stat file a few minutes later.

On the emulator side, the totals come from the running aggregates of its step 6, so the
ongoing payload depends on that step. The update is built at the start of the next
event, so `Last Play` must be read from `previous_state`, not the current event.

### 6.2 Server changes

1. **Model.** Add nullable JSON columns `away_fielding_stats`, `home_fielding_stats`
   and `last_play` to `OngoingGame`, and the same three keys to `to_dict()`. Storing
   them verbatim means the emulator can add a counter later with no server migration.
2. **Handler.** Read the new keys with `request.json.get(...)` in both branches. In the
   update branch, assign only when the key is present, so a 2.2.0 or 2.3.0 client never
   blanks a column and never gets a 500 for a key it does not know about.
3. **Validate lightly, never reject.** Accept a value only if it is a dict (and, for
   the stats, its arrays have 9 integers). Otherwise drop that one field and still apply
   the rest of the update: a bad fielding block should not stop the score updating.
4. **No new version gate.** The existing `'Pitcher Roster Loc' not in request.json`
   check stays as the only gate. Missing fielding keys mean NULL columns.
5. **Consumers must treat NULL as "not reported"**, not zero. Older clients will be in
   the live list alongside new ones for a long time.

Unknown keys are already ignored by the handler, so the server can deploy before or
after the emulator without either side failing.

### 6.3 Existing weakness worth fixing in the same change

If the first post for a game fails, every later update for that `GameID` falls into the
create branch and raises `KeyError` on `'Home Player'`, so the game returns 500 for its
whole length and never appears live. Return a clear 404 for an update to an unknown
game. The emulator could then re-queue a full post.

## 7. Existing endpoints to adapt

| Endpoint | Change | Priority |
|---|---|---|
| `/populate_db/process/`, minute job | New ingest (§5) | Required |
| `/populate_db/ongoing_game/` POST and GET | Optional keys, three columns (§6) | Required for live totals |
| `/delete_game/` | Delete `fielding_assist`, `fielding_out`, `fielding_event`, `fielding_play_summary` before `Event`, and `character_fielding_summary` before `CharacterGameSummary`. Without this the delete hits a foreign-key violation, which the bare `except` turns into a 400 for every new game. | Required |
| `/init_db/` | Drops tables by name starting with `Event` (`db_setup.py:15`); add the new tables ahead of it. `/wipe_db/` uses `drop_all()` and needs nothing. | Required |
| `/stats/` | Third query in `query_detailed_fielding_stats`: `LEFT JOIN character_fielding_summary` and sum every column into the `Fielding` block. Also return `fielding_tracked_appearances` (count of character rows that have a summary), because rates over a date range that mixes old and new games are otherwise diluted. Sums come back `null` when no game in the sample has fielder stats. Keep the existing first-fielder counts (`jump_catches`, `diving_catches`, `wall_jumps`, `swap_successes`, `bobbles`): they cover all history. | High |
| `/events/` (and through it `/landing_data/`, `/star_chances/`) | New filters in `_build_event_filters`: `fielding_event_type`, `catch_type`, `handler_char`, `handler_pos`, `putout_char`, `putout_pos`, `error`, `big_play`. Each must be an `EXISTS` subquery. A plain join to `fielding_event` returns one row per fielding event and would duplicate event ids in every caller. `fielder_char` and `fielder_pos` keep meaning "first fielder". | Medium |
| `/landing_data/` | Opt-in `include_fielding_summary=1` that left-joins `fielding_play_summary` (one row per event, so the result stays flat): fielders handling, throws, error type, big play. Sequences go to the new endpoint. | Medium |
| `/recreate_stat_file/`, `/recreate_events/` | Emit the three blocks and the new `Defensive Stats` counters so a recreated file matches the original. | Medium |
| `/box_score/` | Add putouts, assists and errors per character. Still disabled in production. | Low |
| README | Document the new `/stats/` fields, filters and endpoint. | With the read changes |

`/games/`, `/star_chances/`, `/characters/`, `/ladder/games/` and the user summary need
no change.

A `by_position` grouping for `/stats/` fielding ("putouts at shortstop") is a natural
follow-up. It has to be computed from `fielding_out.putout_position` and
`fielding_event.position`, not from the character summary, so it is a separate piece of
work.

## 8. New endpoint: `GET /fielding_data/`

One new endpoint is appropriate. The defensive sequence is a list per event, and the
existing event endpoints return either ids (`/events/`) or one flat row per event
(`/landing_data/`). Leaderboard-style totals do not need an endpoint of their own:
`/stats/` with `by_user` and `by_char` already covers them once §7 is done.

- **Parameters.** All `/games/` and `/events/` parameters, including the new filters in
  §7, or explicit `events=` ids. Resolve them with `_resolve_event_ids(args,
  default_game_limit=50)`, the same 50-game default as `/landing_data/`.
- **Response.** Events with no fielding rows are left out.

  ```json
  { "Data": [ {
      "game_id": 123, "event_num": 41, "event_id": 99881,
      "inning": 5, "half_inning": 1, "fielding_username": "…",
      "summary": { "fielders_handling_ball": 3, "throws": 2, "bobbles": 0,
                   "credited_fielder_char_id": 6, "big_play_confirmed": false,
                   "error_type": null },
      "fielding_events": [ { "sequence": 1, "parent_sequence": 1, "frame": 34,
                             "event_type": 1, "char_id": 6, "position": 5, "…": "…" } ],
      "outs": [ { "out_number": 1, "out_type": 2, "base": 2,
                  "putout_char_id": 3, "putout_position": 3,
                  "assists": [ { "char_id": 6, "position": 5 } ] } ]
  } ] }
  ```
- **Implementation.** Four `IN (event ids)` queries (summary, events, outs, assists)
  grouped in Python, the way `build_linescore_and_scoring_plays` does it. No per-event
  queries. Use `select()` with `.in_()`, not string-built SQL.

## 9. Migration and rollout

One Alembic migration, chained from `a7c3e9f10b24`: create the five tables with their
indexes and unique constraints, and add the three `ongoing_game` columns with
`batch_alter_table` as `b2f4c8a1d3e7` did. There is no backfill. Older stat files do not
contain the data, and approximating it from `First Fielder` would mix exact and guessed
numbers in the same columns.

One deployment trap: the app calls `db.create_all()` at startup (`app/__init__.py:76`),
which creates any missing table but never adds a column. If the new code boots before
the migration runs, the five tables already exist and the migration's `create_table`
calls fail, while the `ongoing_game` columns are still missing. Run the migration before
the new code starts, or have the migration skip tables that already exist.

The contact-result handoff (`docs/contact-result-handoff.md`, Item 3) plans a data
migration on the same head. Whichever lands second must re-chain its `down_revision`.
Both also edit the event loop in `populate_db.py`. The repo's standing constraints
apply: branch from `develop`, generate the migration file without running it against a
database, keep a single head.

Order:

1. Settle §3 and update `MSB_FielderStats_Plan.md` §5 and step 7 to match.
2. **Server PR 1:** models, migration, constants in `consts.py`, `process_game` ingest,
   ongoing-game handler, `/delete_game/`, `/init_db/`, tests. Safe to deploy early: no
   client sends the new keys yet.
3. Emulator steps 1–6. Take a real stat file from its step 5 and add it to
   `app/tests/data` as the ingest fixture.
4. **Server PR 2:** `/stats/`, `/fielding_data/`, event filters, `/landing_data/`
   opt-in, recreate endpoints, README.
5. Emulator release.

## 10. Tests

The suite runs against a live server and Postgres and calls `/wipe_db/`, so local only.

- Ingest a fielder-stats file. Assert row counts per table, that a 6-4-3 double play
  produces two outs with the right putout and assist characters, and that fielders
  resolve to the fielding team in both half innings.
- Ingest the existing 2.1.1 and 1.9.5 fixtures. Assert the new tables stay empty and
  every existing assertion still passes.
- Ingest a file with one malformed fielding block. Assert the game, its events and its
  other fielding rows are all present.
- For each character in a new game, assert `character_fielding_summary.putouts` equals
  that character's `fielding_out` count, and the same for assists. This is the
  tracker-versus-event-list cross-check.
- `/delete_game/` on a new game succeeds and leaves no orphan rows.
- `/stats/` over a mix of old and new games: the new fields sum only the new games and
  `fielding_tracked_appearances` matches.
- `/events/` with a fielding filter returns each event id once.
- Ongoing game: a post and update with the new keys round-trip through the GET. A
  2.2.0-shaped post and update without them still return 200 and leave the columns
  NULL. An update without the keys does not clear values set by an earlier update.

## 11. Things that change meaning in the same emulator release

None of these need a schema change, and none can be backfilled. Document the first
version that carries them in the README. If a hard boundary is ever needed, "this game
has `character_fielding_summary` rows" marks the new semantics without relying on the
version string.

- `fielding_summary.fielder_y_pos` (exposed by `/landing_data/`): the action Y offset
  before the fix, the real Y position after it.
- `character_position_summary.outs_at_*`: force outs move from the first fielder to
  touch the ball to the fielder who made the putout (emulator step 4).
- Bobble labels: the numbers are unchanged, so stored rows are fine. But `/stats/`
  counts `bobbles` as `bobble != 0`, which includes value 1 (knocked back, ball still
  caught) and 16 (knockout). The new `bobbles_*` columns separate them.

## 12. Problems found on the way

Not caused by this work, but each affects fielding numbers.

- **`Fielder Swap` is unreliable, and so is `swap_successes`.** The 2.1.1 and 1.9.5
  sample files have values like 220 and 67 in half or more of their `First Fielder`
  blocks (26 of 52, 28 of 41). In the tracker, only `collect_fielder` is assigned
  (`MSB_StatTracker.cpp:886`), so `first_fielder`'s copy looks uninitialised. Ingest
  stores any non-zero value as `swap = True`. Fix the initialisation in the emulator,
  and make sure the new `Swapped For Batter` is a clean 0/1.
- **`/recreate_stat_file/` drops `First Fielder` when the pitcher fields the ball.**
  `recreate_stat_files.py:534` tests `if event.position:` and the pitcher is position 0.
  The samples have one to four such plays per game. Use `is not None`. This is the same
  bug class as the Sour-contact fix in the contact-result handoff.
- **`/recreate_stat_file/` position maps.** `"Batter Outs Per Position"` is filled from
  `outs_per_position`, and the first map is written as `"Pitches Per Position"` where
  ingest reads `"Batters Per Position"` (`recreate_stat_files.py:187-189`).

## 13. Decisions still open

- §3 points 1–7, with the emulator plan's owner.
- Whether strikeouts produce a `Fielding Outs` record (catcher putout). If they do,
  `fielding_out` covers every out in the game and "putouts" matches the baseball
  definition. If not, document that strikeouts are excluded.
- Whether pickoff and steal defense is in the first version (emulator plan §8). The
  schema handles either answer.
- The fielding `by_position` grouping for `/stats/` (§7): now or later.
