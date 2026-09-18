# MSB Stat File — Full Defensive Tracking (design + address reference)

Branch `FielderStats`, based on upstream `master` (`24154dd7b3`). Written 2026-09-18.
**Nothing in this document has been implemented yet.** It is the plan and the engine
research another agent needs to build it.

Goal: the stat file currently describes a defensive play as "one fielder touched the
ball, and maybe bobbled it". Replace that with the full defensive sequence — every
pickup, every throw, every fielder action (dive, wall jump, clamber, wall splat,
running catch, jumping catch), every bobble and drop, plus who made each out, what
kind of out it was, and who assisted.

Companion doc: `docs/MSB_HazardEvents_Testing.md` on branch `hazardsInStatfiles`.
This plan deliberately reuses that doc's event-list shape (`Sequence` /
`Parent Sequence` / `Frame` / `Details`) so the two features read the same way.

---

## 1. What exists today, and what is wrong with it

`Source/Core/Core/MSB_StatTracker.cpp` tracks defense through two functions:

- `logFielderWithBall()` — scans the 9 fielders for `ControlStatus == 0xA` and returns
  the **first** one found, then stops.
- `logFielderBobble()` — scans for a non-zero bobble or knockout byte and returns the
  **first** one found, then stops.

These fill `Contact::first_fielder` and `Contact::collect_fielder`, and `getStatJSON`
emits only one of them as `"First Fielder"`. Five problems:

1. **Only the first touch is kept.** A 6-4-3 double play writes one fielder. Relays,
   cutoffs, bobble-then-recover and rundowns are invisible.
2. **No throws at all.** Nothing in the tracker reads the throw state, so assists
   cannot be derived from the stat file.
3. **Fielder actions are a single byte.** `fielder_action` is `0`/`2`/`3`, so dives and
   wall jumps are recorded but clambers, wall splats, running catches, jumping catches
   and knockouts are not distinguished. The `cFielderActions` map has no entry for `1`.
4. **Outs have no fielder attribution.** `Runner::out_type` says *what kind* of out it
   was; nothing says *who made it*. `FielderTracker::incrementOutForPosition` guesses
   ("first fielder to touch the ball") in `logFinalResults`, which is wrong on any play
   with a throw.
5. **A latent address bug.** `aFielder_Pos_Y = 0x8088F374` is `InMemFielder.actionYOffset`
   (struct offset `0x0C`), not `pos.y` (offset `0x04`, address `0x8088F36C`). Every
   `"Fielder Position - Y"` written to date is the action offset, which is 0 for a
   fielder standing on the ground. Fix this as part of step 1; it changes existing
   output, so call it out in the changelog.

Also worth correcting while in here: `cFielderBobbles` mislabels the values. The real
meaning, from `calculateBobble` and `catchAnimationProgression` in the decomp, is in
§4.3 below — `1` is *not* a failed play, the ball is still caught.

---

## 2. How to read memory (base addresses)

Every struct below lives in `game.rel`. The rel's `.bss` is loaded at
**`0x808610E0`**, derived and then cross-checked three ways: `g_Fielders`
(`.bss:0x0002E288`) lands on the existing `aFielder_Pos_X = 0x8088F368`; `g_Ball`
(`.bss:0x0002FA58`) lands on `aAB_BallPos_X = 0x80890B38`; and `g_Ball.framesSinceHit`
lands on `0x8089269E`, the frame counter the hazards work already uses. Ghidra's own
`lis`/`addi` pairs agree (`globalFielding` = `0x80892750`, `g_Ball` = `0x80890B38`).

| Symbol | rel `.bss` offset | Runtime address | Size / stride |
|---|---|---|---|
| `g_Runners[4]` | `0x2DD38` | `0x8088EE18` | `0x154` each |
| `g_Fielders[9]` | `0x2E288` | `0x8088F368` | `0x268` each |
| `g_Batter` | `0x2F830` | `0x80890910` | `0xB0` |
| `g_Pitcher` | `0x2F8E0` | `0x808909C0` | `0x178` |
| `g_Ball` | `0x2FA58` | `0x80890B38` | `0x1BF8` |
| `g_RunningLogic` | `0x31650` | `0x80892730` | `0x20` |
| `g_FieldingLogic` | `0x31670` | `0x80892750` | `0x150` |
| `g_Scores` | `0x317C0` | `0x808928A0` | `0xC8` |
| `g_Strikes` | `0x31888` | `0x80892968` | `0x24` |
| `g_GameLogic` | `0x318AC` | `0x8089298C` | `0x158` |
| `g_Controls[4]` | `0x32848` | `0x80893928` | `0x10` each |
| `storedInningInfo` | `0x32A94` | `0x80893B74` | `0x8C` |

Main-DOL tables (absolute, no rebasing):

| Table | Base | Stride |
|---|---|---|
| `PitcherStats[2][9]` | `0x803535C8` | `0x1E` player, `0x10E` team |
| `BatterStats[2][9]` | `0x803537E4` | `0x26` player, `0x156` team |
| `inMemRoster[2][9]` | `0x80353BE0` | `0xA0` player, `0x5A0` team |

`BatterStats` is the table the existing `aBatter_*` constants index into
(`aBatter_AtBats = 0x803537E8` is base + `0x04`). Despite the name it holds the
per-character **defensive** counters too — see §3.5.

**Fielder slot index is the position code.** `g_Fielders[0]` is always the pitcher,
`[1]` the catcher, `[2]` 1B, `[3]` 2B, `[4]` 3B, `[5]` SS, `[6]` LF, `[7]` CF, `[8]` RF.
This matches the tracker's existing `cPosition` map exactly, and is confirmed by the
zone/angle fallback in `postPlayTrackStats` (infield picks 2/3/5/4 by increasing angle;
outfield picks 8/7/6). So a slot index converts to a position with no lookup, and the
player in it comes from `rosterLocation` at slot + `0x178`.

---

## 3. Address reference

### 3.1 Per-fielder — `g_Fielders`, address = listed + `slot * 0x268`

Addresses shown are for slot 0 (the pitcher), matching the existing header's
convention.

| Field | Struct off | Slot-0 address | Type | Notes |
|---|---|---|---|---|
| `pos.x` | `0x000` | `0x8088F368` | f32 | existing `aFielder_Pos_X` |
| `pos.y` | `0x004` | **`0x8088F36C`** | f32 | **correct Y; existing constant is wrong** |
| `pos.z` | `0x008` | `0x8088F370` | f32 | existing `aFielder_Pos_Z` |
| `actionYOffset` | `0x00C` | `0x8088F374` | f32 | what `aFielder_Pos_Y` actually reads |
| `velocityX` / `velocityZ` | `0x030` / `0x034` | `0x8088F398` / `0x8088F39C` | f32 | |
| `currentVelocity` | `0x050` | `0x8088F3B8` | f32 | |
| `distanceFromBall` | `0x074` | `0x8088F3DC` | f32 | |
| `distanceFromLandingSpot` | `0x080` | `0x8088F3E8` | f32 | |
| `distanceToBases[4]` | `0x0A8` | `0x8088F410` | f32[4] | index = base code, §3.4 |
| `rosterLocation` | `0x178` | `0x8088F4E0` | s16 | existing reads low byte `…E1` |
| `CharID` | `0x17A` | `0x8088F4E2` | s16 | existing reads low byte `…E3` |
| `framesToGetToBallLandingSpot` | `0x186` | `0x8088F4EE` | s16 | |
| `locationResponsibleForCovering` | `0x18C` | `0x8088F4F4` | s16 | base this fielder covers |
| `framesSinceThrowWasMade` | `0x194` | `0x8088F4FC` | s16 | |
| `timeSinceThrowWasCaught` | `0x1AC` | `0x8088F514` | s16 | |
| `jumpCountDown` | `0x1AE` | `0x8088F516` | s16 | |
| `specialActionCountdown` | `0x1B2` | `0x8088F51A` | s16 | clamber timer |
| `wallSplatStageCountDown` | `0x1B4` | `0x8088F51C` | s16 | |
| `onFireCountdown` | `0x1BA` | `0x8088F522` | s16 | |
| `knockOutCountDown` | `0x1C0` | `0x8088F528` | s16 | |
| `AI_Ind` | `0x1C5` | `0x8088F52D` | u8 | CPU-controlled |
| `autoFielderInd` | `0x1C6` | `0x8088F52E` | u8 | auto-fielding on |
| `wallActionAbility` | `0x1CB` | `0x8088F533` | u8 | 1 splat, 2 wall jump, 3 clamber |
| `hasSuperJump` | `0x1CC` | `0x8088F534` | u8 | |
| `throwingArm` | `0x1CE` | `0x8088F536` | u8 | |
| `lockoutDuration` | `0x1D2` | `0x8088F53A` | u8 | |
| `autoMovementFunctionIndex` | `0x1D3` | `0x8088F53B` | u8 | existing `aFielder_ControlStatus`; `0xA` = holding ball |
| `catchStrategy` | `0x1DD` | `0x8088F545` | u8 | |
| `fielderMadeThrow` | `0x1DF` | **`0x8088F547`** | u8 | set for 60 frames after a throw |
| `hitKnockbackCountdown` | `0x1EE` | `0x8088F556` | u8 | ball knocked him back |
| `stunFramesOnFireBall` | `0x1EF` | `0x8088F557` | u8 | |
| `baseCurrentlyOn` | `0x1F5` | **`0x8088F55D`** | s8 | `-1` = not on a base |
| `outFieldZoneCode` | `0x1F8` | `0x8088F560` | u8 | |
| `isJump` | `0x203` | `0x8088F56B` | u8 | existing `aFielder_AnyJump` |
| `clamberStatus` | `0x205` | **`0x8088F56D`** | u8 | 0 none, 1 climbing, 2 on wall, 3–5 jumping off |
| `wallSplatStatus` | `0x207` | **`0x8088F56F`** | u8 | 0 none, 1–4 stages |
| `onFire` | `0x20F` | `0x8088F577` | u8 | already used by the hazards work |
| `knockoutStatus` | `0x210` | `0x8088F578` | u8 | existing `aFielder_Knockout`; 1 flying, 2 down |
| `bodyCheckResult` | `0x211` | **`0x8088F579`** | u8 | 1 = check landed, 2 = runner stopped |
| `bodyCheckStatus` | `0x212` | `0x8088F57A` | u8 | |
| `bodyCheckRunnerNumber` | `0x213` | `0x8088F57B` | u8 | |
| `bodyCheckBase` | `0x214` | `0x8088F57C` | u8 | |
| `throwWindUpFrames` | `0x215` | `0x8088F57D` | u8 | |
| `catchAnimationFramesCountDown` | `0x24C` | `0x8088F5B4` | s16 | frames until the catch resolves |
| `catchAnimation` | `0x252` | **`0x8088F5BA`** | u8 | **the catch-type byte, §4.1** |
| `catchVerticalZone` | `0x253` | `0x8088F5BB` | u8 | 0 low, 1 mid, 2 high |
| `catchCentreRightLeftOfBody` | `0x254` | `0x8088F5BC` | u8 | 0 centre, 1 left, 2 right |
| `catchFastBattedBallInd` | `0x255` | `0x8088F5BD` | u8 | |
| `bobble` | `0x258` | `0x8088F5C0` | u8 | existing `aFielder_Bobble`, §4.3 |
| `action` | `0x259` | `0x8088F5C1` | u8 | existing `aFielder_Action`; 0 none, 2 dive, 3 wall jump |
| `autoCatch0_noCatchAnimationOnly1` | `0x25B` | `0x8088F5C3` | u8 | |
| `wallJumpStatus` | `0x25E` | **`0x8088F5C6`** | u8 | |
| `runningCatchInd` | `0x260` | **`0x8088F5C8`** | u8 | |
| `caughtBallInAir` | `0x264` | **`0x8088F5CC`** | u8 | catch will be an out, not a trap |
| `suctionCatchInd` | `0x265` | `0x8088F5CD` | u8 | |

Character fielding abilities (to explain *why* a fielder could clamber, dive, laser…)
are a `u32` bitfield in `inMemRoster` at `0x80353C00 + team*0x5A0 + roster*0xA0`.
Bits: `0x1` wall splat, `0x2` wall jump, `0x4` clamber, `0x8` sliding catch, `0x10`
laser, `0x20` quick throw, `0x40` super jump, `0x80` magical catch, `0x100` tongue
catch, `0x200` suction, `0x400` super catch, `0x800` ball dash, `0x1000` body check,
`0x2000` super curve.

### 3.2 Ball — `g_Ball = 0x80890B38`

| Field | Struct off | Address | Type | Notes |
|---|---|---|---|---|
| `maxYOfHit` | `0x19D4` | `0x8089250C` | f32 | existing `ball_max_height` |
| `throwDestination` x/y/z | `0x19EC` | `0x80892524`/`28`/`2C` | f32 | |
| `throwTarget` x/z | `0x19F8` | `0x80892530`/`34` | f32 | |
| `ballVelocity` | `0x1A14` | `0x8089254C` | f32 | scalar speed |
| `landingSpotLocation` x/z | `0x1A2C` | `0x80892564`/`68` | f32 | |
| `throwStartingLocation` x/z | `0x1A34` | `0x8089256C`/`70` | f32 | |
| `throwDistance` | `0x1A4C` | **`0x80892584`** | f32 | |
| `ballPickedUpCaught` x/z | `0x1A50` | `0x80892588`/`8C` | f32 | where the ball was collected |
| `ballEnergy` | `0x1A58` | `0x80892590` | f32 | |
| `hangtimeOfHit` | `0x1B5E` | `0x80892696` | s16 | existing `ball_hang_time` |
| `framesUntilBallHitsGround` | `0x1B60` | `0x80892698` | s16 | |
| `framesUntilThrowReachesDest` | `0x1B62` | **`0x8089269A`** | s16 | |
| `framesSinceHit` | `0x1B66` | **`0x8089269E`** | s16 | **play clock; use for `Frame`** (+100 during a pickoff) |
| `framesSinceThrowStarted` | `0x1B6C` | **`0x808926A4`** | s16 | |
| `timeSinceBallPickedUp` | `0x1B6E` | `0x808926A6` | s16 | `-1` while not held |
| `framesSinceBallHitGroundOrWasCaught` | `0x1B72` | `0x808926AA` | s16 | |
| `fielderWBallIndex` | `0x1B78` | **`0x808926B0`** | s16 | who holds the ball, `-1` = nobody |
| `AtBat_ContactResult` | `0x1B7A` | `0x808926B2` | s16 | existing reads low byte; `-1` foul, 0 in air, 1 landed, 2 fielded, 3 caught |
| `fielderAboutToGetBall_hasBall` | `0x1B7E` | `0x808926B6` | s16 | |
| `ballAngleFromHome` | `0x1B80` | `0x808926B8` | s16 | |
| `ballTravelAngle` | `0x1B82` | `0x808926BA` | s16 | |
| `baseBallAndFielderAreOn` | `0x1B86` | **`0x808926BE`** | s16 | base the ball is being held on; drives force outs |
| `fielderBeingThrownTo` | `0x1B8A` | **`0x808926C2`** | s16 | |
| `fielderWithBallIndexStored` | `0x1B8E` | **`0x808926C6`** | s16 | first fielder to handle the ball this play |
| `fielderWithBallIndexStored2` | `0x1B90` | **`0x808926C8`** | s16 | same, but back-filled by zone if nobody touched it |
| `fielderWhoGotLastOut` | `0x1B92` | **`0x808926CA`** | s16 | **the game's own out credit, §4.4** |
| `throwingFielder` | `0x1B94` | **`0x808926CC`** | s16 | last fielder to release a throw |
| `ballZoneAwayFromHome` | `0x1BBE` | `0x808926F6` | u8 | distance band; `< 2` = infield |
| `landingSpotZoneAwayFromHome` | `0x1BBF` | `0x808926F7` | u8 | |
| `ballState` | `0x1BC9` | **`0x80892701`** | u8 | **0 hit, 1 held, 2 thrown, 3 loose** |
| `IsAntichemistryThrow` | `0x1BCB` | **`0x80892703`** | u8 | |
| `hitWallInd` | `0x1BCC` | `0x80892704` | u8 | |
| `lineDriveThroughPitcherInd` | `0x1BCE` | `0x80892706` | u8 | |
| `deadBallReason` | `0x1BD1` | `0x80892709` | u8 | existing |
| `framesOnGroundUntilPickedUp` | `0x1BD6` | `0x8089270E` | u8 | |
| `numberOfThrowsDuringPlay` | `0x1BD7` | `0x8089270F` | u8 | misnamed: `++` per **catch** |
| `numFieldersWhoHandledBallDuringPlay` | `0x1BD8` | **`0x80892710`** | u8 | |
| `numThrowsDuringPlay` | `0x1BD9` | **`0x80892711`** | u8 | `++` per actual throw |
| `bobbleLocation_1fair_2foul` | `0x1BDA` | `0x80892712` | u8 | |
| `ballZoneWhenCaught` | `0x1BDD` | `0x80892715` | u8 | |
| `looseBall_codeForHowLongUntilSomeoneWillGetIt` | `0x1BE1` | `0x80892719` | u8 | |
| `hardHitIndicator` | `0x1BE6` | `0x8089271E` | u8 | |
| `catchAnimationTotalFrames` | `0x1BF0` | `0x80892728` | u8 | non-zero while a catch is animating |

### 3.3 Fielding logic — `g_FieldingLogic = 0x80892750`

| Field | Struct off | Address | Type | Notes |
|---|---|---|---|---|
| `selectedFielder` | `0xB0` | **`0x80892800`** | s16 | fielder under player control |
| `secondaryFielder` / `tertiaryFielder` | `0xB2` / `0xB4` | `0x80892802` / `0x80892804` | s16 | |
| `cutoffFielderIndex` | `0xBE` | `0x8089280E` | s16 | |
| `locationThrownTo` | `0xC4` | **`0x80892814`** | s16 | `-1` none, 0 home, 1 1B, 2 2B, 3 3B, 5 pitcher cutoff, 6 OF cutoff |
| `fielderAssignedLocationIndex[7]` | `0xD0` | **`0x80892820`** | s16[7] | who is covering each base; index = base code |
| `runnerChasingAfter` | `0xDE` | `0x8089282E` | s16 | |
| `throwWindUpFrameCounter` | `0xE6` | `0x80892836` | s16 | |
| `runnerBeingTargettedForOut` | `0xE8` | **`0x80892838`** | s16 | runner index the defense is playing on |
| `lastThrowingFielder` | `0xEA` | **`0x8089283A`** | s16 | **the fielder the game charges an error to** |
| `framesRunnerIsOutBy` | `0xEC` | `0x8089283C` | s16 | |
| `fielderAutoMovementCode[9]` | `0xF8` | `0x80892848` | u8[9] | |
| `baseCoveredInd[4]` | `0x101` | `0x80892851` | u8[4] | |
| `throwSpeedType` | `0x106` | **`0x80892856`** | u8 | see §4.2 |
| `liveBallBcOfPickoffOrStealCd` | `0x107` | `0x80892857` | u8 | existing `aAB_PickoffAttempt`; 1 pickoff, 2 steal |
| `infieldFlyIndicator` | `0x108` | **`0x80892858`** | u8 | 0 none, 1 declared, 2 declared + resolved |
| `tagAnimationType` | `0x111` | **`0x80892861`** | u8 | 0 none, 1 regular tag, 2–5 tag-up variants, 6/7 body check |
| `tagResult_1out_2safe` | `0x112` | **`0x80892862`** | u8 | |
| `processErrorCode` | `0x113` | **`0x80892863`** | u8 | error state machine, §4.5 |
| `fielderActionBeingProcessed` | `0x114` | `0x80892864` | u8 | mirror of the catcher's `action` |
| `bodyCheckResult` | `0x116` | `0x80892866` | u8 | |
| `quickThrowInd` | `0x11F` | `0x8089286F` | u8 | |
| `baseFielderIsOn` | `0x125` | `0x80892875` | u8 | |
| `runnerTargetedOnThrowDuringSteal` | `0x126` | **`0x80892876`** | s8 | |
| `pickoffStealResultCode` | `0x127` | **`0x80892877`** | u8 | 0 ongoing, `0xFF` no target, 1 pickoff advanced, 2 steal advanced |
| `smash0_normalThrow1` | `0x12D` | `0x8089287D` | u8 | |
| `errorTypeCd` | `0x132` | **`0x80892882`** | u8 | 1 dropped fly, 2 ground ball, 3 failed rundown |
| `bigPlayPotential` | `0x133` | **`0x80892883`** | u8 | 0 none, 1 potential, 2 confirmed |
| `bigPlayFielderIndex` | `0x134` | **`0x80892884`** | u8 | `-1` when unset |
| `knockoutFinished` | `0x13B` | `0x8089288B` | u8 | |
| `throwInterceptionTriggered` | `0x13C` | `0x8089288C` | u8 | |
| `smashThrowInd` | `0x13D` | `0x8089288D` | u8 | |
| `IsChemistryThrow` | `0x13E` | **`0x8089288E`** | u8 | 0 none, 1 chem, 2 chem capped |
| `laser_1` / `laser_2` | `0x140` / `0x141` | `0x80892890` / **`0x80892891`** | u8 | laser throw fired |
| `fielderInputs` | `0x148` | `0x80892898` | u16 | fielding-team button mask |

### 3.4 Outs, runners, play result

`g_Strikes = 0x80892968`:

| Field | Off | Address | Type | Notes |
|---|---|---|---|---|
| `strikes` | `0x00` | `0x80892968` | int | existing reads low byte `…6B` |
| `balls` | `0x04` | `0x8089296C` | int | |
| `outs` | `0x08` | `0x80892970` | int | |
| `storedOuts` | `0x0C` | **`0x80892974`** | int | outs at the start of the play |
| `forcedOutToEndInningInd` | `0x10` | `0x80892978` | int | |
| `howRunnerReachedBase` | `0x14` | **`0x8089297C`** | int | 0 TBD, 2 reached on error, 3 fielder's choice |
| `runnerIndexForEachOutThisPitch[3]` | `0x18` | **`0x80892980`** | s16[3] | **outs in order, `-1` = unused** |

`runnerIndexForEachOutThisPitch` is the authoritative out list — `runnerOut()` appends
to it, `newPitch` clears it. Prefer `outs - storedOuts` over the existing
`aAB_NumOutsDuringPlay` for the count; it is what `postPlayTrackStats` itself uses.

`g_Runners = 0x8088EE18`, stride `0x154`, index 0 = batter-runner:

| Field | Off | Slot-0 address | Notes |
|---|---|---|---|
| `forceOutCd` | `0x0EE` | `0x8088EF06` | s16; 0 not forced, 1 forced, 2 out on force |
| `framesSinceOut` | `0x0F4` | `0x8088EF0C` | s16 |
| `baseReachedAtTimeOfThrow` | `0x0FE` | `0x8088EF16` | s16 |
| `runnerStatus` | `0x123` | `0x8088EF3B` | 0 none, 1 on field, 2 out, 3 scored, 4 scored dead-ball, 5 walked while stealing |
| `currentBase` | `0x125` | `0x8088EF3D` | existing |
| `baseRunningTowards` | `0x127` | `0x8088EF3F` | existing `aRunner_OutLoc` |
| `tagUpInd` | `0x128` | `0x8088EF40` | 0 none, 1 in air, 2 tagged |
| `outType` | `0x12C` | `0x8088EF44` | existing |
| `tagType` | `0x130` | `0x8088EF48` | 1 running tag, 2 sliding tag |
| `stealingStatus` | `0x14E` | `0x8088EF66` | existing |

Base codes are consistent across `locationThrownTo`, `baseBallAndFielderAreOn`,
`distanceToBases[]` and `fielderAssignedLocationIndex[]`: **0 home, 1 first, 2 second,
3 third**. The runner forced at base *B* is runner index `(B + 3) & 3`.

`storedInningInfo = 0x80893B74`:

| Field | Off | Address | Notes |
|---|---|---|---|
| `throw_RunnersPotentiallyTargeted` | `0x20` | `0x80893B94` | s16 |
| `batterResultBase` | `0x22` | `0x80893B96` | s8 |
| `someRunnerResultCode` | `0x24` | `0x80893B98` | sac fly / bunt classification |
| `nRunnersForcedOut` | `0x25` | `0x80893B99` | 2 = force double play |
| `rbisWaitingToBeAddedToScore` | `0x26` | `0x80893B9A` | existing `aAB_RBI` |
| `atBatResult` | `0x36` | `0x80893BAA` | existing `aAB_FinalResult` |
| `atBatResult` history | `0x37`–`0x3A` | `0x80893BAB`+ | previous four results |
| **`creditedFielderIndex`** | `0x3B` | **`0x80893BAF`** | snapshot of `fielderWithBallIndexStored2` at play end |
| `rbisOnPlay` | `0x3C` | `0x80893BB0` | |
| `storedOutsAtPlayEnd` | `0x3D` | `0x80893BB1` | |

### 3.5 Per-character defensive counters the game already keeps

`BatterStats[team][roster] = 0x803537E4 + team*0x156 + roster*0x26`. Three fields the
tracker does not read yet:

| Off | Address (team 0, roster 0) | Meaning |
|---|---|---|
| `+0x15` | `0x803537F9` | runners advanced against / allowed to advance on error |
| `+0x16` | `0x803537FA` | **defensive chances** — plays this fielder was involved in |
| `+0x1A`–`+0x21` | `0x803537FE`–`0x80353805` | `positionsPlayed[8]` flags: P, C, 1B, 2B, 3B, SS, OF, none |
| `+0x23` | `0x80353807` | Big Plays (already read as `aBatter_BigPlays`) |

`+0x16` is incremented in `updateStatsBasedOnABResult` for
`fielderWithBallIndexStored`, and again for `fielderWithBallIndexStored2` when
`bigPlayPotential == 2`. It is a useful cross-check against the tracker's own chance
count, not a replacement for it.

---

## 4. Engine semantics you must match

### 4.1 Catches — `catchAnimation` (`0x8088F5BA + slot*0x268`)

Set by whichever routine claims the ball; it is the single best "what kind of play was
that" byte in the game. Zero when no catch is in progress.

| Value | Set by | Meaning |
|---|---|---|
| 1 | `checkIfCatchOccurs` | routine catch / pickup |
| 2 | `catchThrownBallFun` | caught a thrown ball (sets `quickThrowInd`) |
| 3 | `divingCatch` | diving catch (also sets `action = 2`) |
| 4 | `wallJumpInitialization` | wall-jump catch (also sets `action = 3`) |
| 5 | `clamberCheckCatch` | catch made while clambering the wall |
| 6 | `checkForCatchBallAction` jump branch | jumping catch |
| 7 | `checkIfRunningCatchOccurs`, `divingCatch` backward path | running catch (also sets `runningCatchInd = 1`) |
| 8 | `ballThrownToEmptyBaseCatchAttempt` | moving to cover a base for a throw |

The catch is only *resolved* when `catchAnimationFramesCountDown` reaches 0 and
`catchAnimationProgression` calls `updateVariablesPostCatch`, which is where
`ballState` becomes `1` (held) and `fielderWBallIndex` is set. So: record the
`catchAnimation` value on its rising edge, then confirm the outcome when
`fielderWBallIndex` goes non-negative (kept) or `bobble` lands on 2/3 (dropped).

`caughtBallInAir` (`0x8088F5CC`) distinguishes a catch for an out from a trap.
`wallSplatStatus`, `clamberStatus` and `wallJumpStatus` are independent of
`catchAnimation` — a fielder can splat into the wall without any catch.

### 4.2 Throws

A throw starts in `makeThrowVariables`, which sets `ballState = 2`,
`framesSinceThrowStarted = 0`, `fielderWBallIndex = -1`,
`throwingFielder = <slot>` and `fielderBeingThrownTo`. So the detection edge is
**`ballState` 1 → 2**, and everything worth recording is valid on that frame:

- thrower `throwingFielder` `0x808926CC`, receiver `fielderBeingThrownTo` `0x808926C2`
- target `locationThrownTo` `0x80892814` (cutoffs use 5/6 and have no base)
- `throwDistance` `0x80892584`, `framesUntilThrowReachesDest` `0x8089269A`
- `throwSpeedType` `0x80892856`, `smashThrowInd` `0x8089288D`
- `IsChemistryThrow` `0x8089288E`, `IsAntichemistryThrow` `0x80892703`
- `laser_2` `0x80892891`, `quickThrowInd` `0x8089286F`

The throw ends when `ballState` leaves 2: → 1 means it was caught
(`ballState_thrown_to_holding`), → 3 means it got away
(`checkForAndHandleLooseBalls` sets `looseBall_codeForHowLongUntilSomeoneWillGetIt`).
That 2 → 3 transition is the overthrow signal; pair it with `processErrorCode` to
decide whether the game charged an error.

Do **not** trust `numberOfThrowsDuringPlay` (`0x8089270F`) — despite the name it is
incremented once per *catch* in `updateVariablesPostCatch`. The real throw counter is
`numThrowsDuringPlay` (`0x80892711`), incremented in `makeThrowVariables`.

### 4.3 Bobbles — `bobble` (`0x8088F5C0 + slot*0x268`)

Rolled in `calculateBobble` against `BobbleArray` using the fielder's character class,
whether he is diving (`action != 0`), running (`runningCatchInd`), or reaching
off-centre (`catchCentreRightLeftOfBody`), and whether the ball is still in the air.
Correct meanings:

| Value | Meaning | Ball outcome |
|---|---|---|
| 0 | clean | caught |
| 1 | knocked back — sets `hitKnockbackCountdown` | **still caught** |
| 2 | fumble on a grounder / non-airborne ball | ball goes loose (`bobbleDirection`) |
| 3 | drop on a fly ball or liner | ball goes loose |
| 4 | forced drop by a star hit (Wario/Waluigi garlic, fireball) | ball goes loose, fielder catches fire |

So only 2, 3 and 4 are failed plays. The existing `cFielderBobbles` map calls 1
"Slide/stun lock" and 4 "Fireball", and adds a synthetic `0x10` for knockouts —
keep `0x10` (the tracker invents it) but relabel 1–4 per this table.

`calculateBobble` is skipped entirely when the fielder has Super Catch, and when
`numFieldersWhoHandledBallDuringPlay != 0 || hitWallInd != 0 ||
framesOnGroundUntilPickedUp >= 5` — i.e. relay throws and slow rollers cannot be
bobbled.

### 4.4 Outs — who gets credit

Three code paths produce an out, and each sets `outType` and *may* set
`fielderWhoGotLastOut` (`0x808926CA`) if it is still `-1`:

| Path | Function | `outType` | `fielderWhoGotLastOut` becomes |
|---|---|---|---|
| Fly/line out, incl. infield fly | `running_checkForOuts` | 1 caught | `fielderWBallIndex` (the catcher of the ball) |
| Force out | `running_CheckForForceOuts_…` | 2 force | `throwingFielder` if ≥ 0, else `fielderWBallIndex` |
| Tag out, body check | `running_checkForOuts` | 3 tag | `fielderWithBallIndexStored2` |
| Out while tagging up | `running_checkForOuts` | 4 | unchanged |

Note the asymmetry: on a **force out the game stores the thrower**, not the fielder on
the bag. So `fielderWhoGotLastOut` is a *credit* field, not a putout field. Derive
putout and assist yourself:

- **Putout** = the fielder in possession when the out fires. For a catch that is
  `fielderWBallIndex`; for a force that is `fielderAssignedLocationIndex[base]` where
  `base = baseBallAndFielderAreOn` (`0x808926BE`), which is also the fielder whose
  `baseCurrentlyOn` equals that base; for a tag it is `fielderWBallIndex`.
- **Assists** = every distinct fielder in the handling chain before the putout fielder,
  since the last putout. Build the chain from `fielderWBallIndex` rising edges plus
  deflections (a `bobble` 2/3 edge on a fielder who does not end up with the ball).
- **Unassisted** when the chain has one entry.
- **Strikeout**: the game does not touch these fields. Credit the putout to the catcher
  (`g_Fielders[1].rosterLocation`) as the tracker already records in
  `Event::catcher_roster_loc`, and the assist to the pitcher.

`runnerIndexForEachOutThisPitch` (`0x80892980`) gives the runners put out in order, so
each out can be matched to its runner without inferring from `outType` edges.
`infieldFlyIndicator == 2` (`0x80892858`) marks an infield fly that resolved.
`nRunnersForcedOut == 2` (`0x80893B99`) marks a force double play.

### 4.5 Errors

`processErrorCode` (`0x80892863`) is a state machine, and `errorTypeCd`
(`0x80892882`) is the reason:

| `processErrorCode` | Meaning |
|---|---|
| 0 | no error |
| 1 | pending — dropped fly |
| 2 | pending — ground ball |
| 3–7 | pending — late throw, `3 + runnerIndex` (set by `pickoff_infieldThrow_related`) |
| 9 | **error confirmed** |

| `errorTypeCd` | Meaning |
|---|---|
| 1 | dropped fly |
| 2 | ground ball |
| 3 | failed rundown |

`monitorForErrors` promotes pending → 9 once the batter-runner reaches base, and
cancels it if the batter is forced out or the ball is caught.
`updateStatsBasedOnABResult` charges the error to
`g_Fielders[lastThrowingFielder].rosterLocation`. For a dropped fly that attribution
is unreliable (`lastThrowingFielder` can be stale from an earlier throw) — prefer
`fielderWithBallIndexStored` for `errorTypeCd == 1`, and say so in the JSON so the
consumer knows which rule produced the name.

`g_Strikes.howRunnerReachedBase` (`0x8089297C`) records the outcome at the runner
level: 2 = reached on error, 3 = fielder's choice.

### 4.6 Big plays

`bigPlayPotential` (`0x80892883`) is set to 1 in `updateVariablesPostCatch` when the
fielder covered ground quickly, and promoted to 2 for a wall-jump/clamber catch, a
catch while jumping above the hitbox, or any catch with `action != 0`.
`bigPlayFielderIndex` (`0x80892884`) names the fielder. This is the same signal behind
the existing `aBatter_BigPlays` counter, so recording the per-play version lets the
stat file explain the season total.

---

## 5. Proposed stat-file schema

Additive only. `"First Fielder"` stays exactly as it is so existing consumers keep
working; everything new sits beside it and is omitted when empty, so a play with no
defensive activity produces a byte-identical file.

### 5.1 Per-contact event list

```json
"Contact": {
  "…": "unchanged fields",
  "Contact Result - Secondary": "Out-force",

  "Fielding Summary": {
    "Fielders Handling Ball": 2,
    "Throws": 1,
    "Bobbles": 0,
    "Credited Fielder": 5,
    "Big Play": { "Fielder": 5, "Confirmed": 1 },
    "Error": { "Charged To": 3, "Type": "Dropped Fly", "Attribution": "First Handler" }
  },

  "Fielding Events": [
    { "Sequence": 1, "Parent Sequence": 1, "Frame": 34,
      "Event": "Catch",
      "Fielder Roster Loc": 6, "Fielder Position": "SS", "Fielder Char Id": "Yoshi",
      "Position - X": 12.3, "Position - Y": 0.0, "Position - Z": -41.2,
      "Details": {
        "Catch Type": "Diving",
        "Action": "Dive", "Jump": 0, "Running Catch": 0,
        "Caught In Air": 1, "Catch Height": "Mid", "Catch Side": "Left",
        "Bobble": "None", "Manual Select": "This player selected",
        "Swapped For Batter": 0
      } },
    { "Sequence": 2, "Parent Sequence": 1, "Frame": 51,
      "Event": "Throw",
      "Fielder Roster Loc": 6, "Fielder Position": "SS",
      "Position - X": 12.3, "Position - Y": 0.0, "Position - Z": -41.2,
      "Details": {
        "Thrown To": "1B", "Receiver Position": "1B", "Receiver Roster Loc": 2,
        "Throw Distance": 27.4, "Frames To Arrive": 38,
        "Speed Type": 3, "Smash Throw": 0, "Quick Throw": 0,
        "Chemistry": "None", "Laser": 0
      } },
    { "Sequence": 3, "Parent Sequence": 1, "Frame": 89,
      "Event": "Receive",
      "Fielder Roster Loc": 2, "Fielder Position": "1B",
      "Details": { "Catch Type": "Thrown Ball", "On Base": 1, "Bobble": "None" } }
  ],

  "Outs": [
    { "Sequence": 1, "Out Number": 1, "Frame": 89,
      "Out Type": "Force", "Runner": 0, "Base": 1,
      "Putout": 2, "Assists": [6],
      "Unassisted": 0, "Infield Fly": 0, "Double Play": 0 }
  ],

  "First Fielder": { "…": "unchanged" }
}
```

`Event` values: `Catch`, `Pickup`, `Receive`, `Throw`, `Bobble`, `Drop`, `Knockout`,
`Wall Splat`, `Clamber`, `Wall Jump`, `Body Check`, `Tag`.
`Parent Sequence` groups one continuous possession (catch → throw, or bobble →
recovery by the same fielder), mirroring the hazard-event convention.
`Frame` is `g_Ball.framesSinceHit` (`0x8089269E`), the same clock the hazard events
use, so the two lists interleave correctly.

The same list is mirrored into the HUD JSON under the contact, as the hazard list is.

### 5.2 Per-character aggregates

Extend `Character Game Stats → <team> Roster N → Defensive Stats` with counters the
tracker accumulates itself across the game:

```json
"Defensive Stats": {
  "…": "existing pitcher stats and per-position maps",
  "Putouts": 3,
  "Assists": 2,
  "Errors": 0,
  "Chances": 5,
  "Throws": 2,
  "Pickups": 4,
  "Catches": { "Routine": 2, "Running": 1, "Diving": 1, "Jumping": 0,
               "Wall Jump": 0, "Clamber": 0, "Thrown Ball": 2 },
  "Bobbles": { "Knockback": 1, "Fumble": 0, "Drop": 0, "Star Hit": 0 },
  "Actions": { "Dives": 1, "Wall Jumps": 0, "Clambers": 0, "Wall Splats": 1,
               "Knockouts": 0, "Body Checks": 0 },
  "Big Plays (Tracked)": 1
}
```

Keep the existing `"Big Plays"` (read from `aBatter_BigPlays`) untouched and add
`"Big Plays (Tracked)"` beside it so the two can be compared during validation.

---

## 6. Implementation steps

Each step builds and is verifiable on its own. Stop and check in after each.

**Step 1 — header constants and the Y fix.**
Add the addresses from §3 to `MSB_StatTracker.h` using the existing naming style
(`aFielder_*`, `aBall_*`, `aFielding_*`, `aOuts_*`). Fix `aFielder_Pos_Y` to
`0x8088F36C` and add `aFielder_ActionYOffset = 0x8088F374` so nothing is lost. Correct
`cFielderBobbles` per §4.3 and add `cCatchAnimation`, `cThrowTarget`, `cErrorType`,
`cBallState`, `cRunnerStatus` decode maps. No behaviour change beyond the Y fix.

**Step 2 — per-frame field snapshot.**
Add `FielderFrameState` (one per slot: `catchAnimation`, `bobble`, `action`, `isJump`,
`clamberStatus`, `wallSplatStatus`, `wallJumpStatus`, `runningCatchInd`,
`knockoutStatus`, `onFire`, `bodyCheckResult`, `baseCurrentlyOn`, `fielderMadeThrow`,
position) plus a `BallFrameState` (`ballState`, `fielderWBallIndex`,
`fielderBeingThrownTo`, `throwingFielder`, `baseBallAndFielderAreOn`,
`framesSinceHit`). Add `sampleDefense(guard)` that fills a `current` snapshot and
retains `previous`, called once per frame. Log edges to stdout only — no JSON yet.
This is the step that verifies the addresses: play a game with the console open and
confirm every dive, wall jump and throw prints exactly once.

**Step 3 — event list.**
Add `FieldingEvent` and `Contact::fielding_events`, and turn the step-2 edges into
appended events. Call `sampleDefense` from `PITCH_RESULT` (for steal and pickoff
throws), `CONTACT_RESULT`, `MONITOR_RUNNERS`, and once on entry to `PLAY_OVER`.
Reset the list in `logContact` the way `resetHazardTracking` does. Cap the list at 64
events per contact and set a `"Truncated": 1` flag if it overflows, so a rundown cannot
blow up the file.

**Step 4 — possession chain and outs.**
Maintain `std::vector<u8> m_handling_chain` for the play. On each out edge (a runner's
`outType` going non-zero, matched against `runnerIndexForEachOutThisPitch`), emit an
`Out` record with putout and assists derived per §4.4. Replace the guess in
`logFinalResults` — the existing `incrementOutForPosition` call for `out_type == 2`
should now use the real putout fielder.

**Step 5 — JSON output.**
`getStatJSON`, `getEventJSON` and `getHUDJSON` gain the `Fielding Events`, `Outs` and
`Fielding Summary` blocks. Follow the hazard-event precedent: omit entirely when empty,
decode enums when `inDecode` is true, raw numbers otherwise.

**Step 6 — per-character aggregates.**
Accumulate the §5.2 counters in `FielderTracker` alongside the existing per-position
maps, keyed the same way (away/home index, **not** the port-remapped team id — see the
comment block in `lookForTriggerEvents`; the batting-order table is away/home indexed
and this new state must follow the same convention).

**Step 7 — server payload.**
Decide with the Rio server owners whether `postOngoingGame` / `updateOngoingGame` and
the end-of-game POST carry the new blocks. The ongoing-game payloads are size-sensitive
and go through the background submitter; the safe default is end-of-game only.

### Notes for whoever implements this

- All reads happen on the CPU thread inside the existing `CPUThreadGuard`, so there is
  no netplay desync risk — this is read-only observation. Both clients run the same
  code on the same RAM, so both should produce identical lists; that is a validation
  check, not an assumption.
- Budget: a full 9-fielder snapshot is roughly 200 `HostRead_U8` calls per frame. The
  existing `MONITOR_RUNNERS` path already does about half that. Sample once per frame
  into the snapshot and derive everything from it rather than re-reading per event.
- Replays re-run live-ball code. The hazard work guards with
  `GameStatus == LiveBall` (`0x80892AAA == 2`); do the same here, and if duplicate
  events still appear after a home run, add the `aAB_IsReplay` (`0x80872540`) guard.
- Pickoffs and steals reach `MONITOR_RUNNERS` with no `pitch`/`contact` object. The
  existing code already guards for this; the new event list needs somewhere to live for
  those plays — either hang it off the `Event` rather than the `Contact`, or skip
  pickoff defense in v1 and say so.

---

## 7. Validation plan

Play local games with the console visible and check each row logs once:

1. **Catch types** — a routine grounder (1), a quick throw catch (2), a dive (3), a wall
   jump catch (4), a clamber catch (5), a jumping catch (6), a running catch (7), and a
   fielder moving to cover a base for a throw (8). Wall-jump and clamber require a
   character with the ability (`wallActionAbility` 2 and 3).
2. **Throw chain** — a 6-4-3 double play must produce Catch(SS) → Throw(SS→2B) →
   Receive(2B) → Throw(2B→1B) → Receive(1B) with two `Out` records, the first
   putout 2B / assist SS and the second putout 1B / assist 2B.
3. **Out types** — fly out (putout = catcher of the ball, no assist), force out at first
   (putout 1B, assist the fielder), tag out (putout = tagger), infield fly
   (`Infield Fly: 1`), force double play (`Double Play: 1`, `nRunnersForcedOut == 2`).
4. **Errors** — drop a fly ball and confirm `processErrorCode` reaches 9,
   `errorTypeCd == 1`, and the charged fielder is the one who dropped it. Overthrow a
   base and confirm `errorTypeCd == 2` or the late-throw pending states 3–7.
5. **Bobbles** — confirm a value-1 bobble still results in a catch and is *not* counted
   as a drop, and that 2/3 put the ball on the ground.
6. **Knockouts and hazards** — a fielder knocked out by a chain chomp should produce a
   `Knockout` event whose frame interleaves correctly with the hazard-event list.
7. **Netplay** — both clients produce identical `Fielding Events` and `Outs` arrays.
8. **Regression** — a game with the new code must produce the same `First Fielder`,
   `Outs Per Position` and `Batter Outs Per Position` values as the old code, except
   where step 4 deliberately corrects the force-out attribution.

---

## 8. Open questions

- `ballZoneAwayFromHome` band values are inferred from comparisons
  (`< OFShallow` for infield, `>= 3` for deep). Read the actual values off the console
  during step 2 before decoding them in JSON.
- `throwSpeedType` is an enum in Ghidra (`x1.3`, `x1.2`, `x1.1`, `x1`, `x0.9`, `x0.8`,
  `x0.7`, `setTo0.3/0.325/0.35`, `setTo0.25`, `setTo0.55`, `9`) but the numeric order is
  not confirmed. Emit the raw byte until it is.
- `tagAnimationType` values 2–5 are all tag-up variants in Ghidra
  (`tagOnRunnerTaggingUp?`, `ragOnRunnerTaggingUpAtDifferentBase?`); 6 and 7 are the two
  body-check outcomes. Confirm before decoding.
- `outType == 4`: the existing `cOutType` map calls it "Force Back", but Ghidra names
  the code path `whileTaggingUp` — a runner doubled off after a catch. Confirm and
  relabel.
- `aAB_NumOutsDuringPlay = 0x808938AD` does not land inside any `game.rel` symbol this
  research could identify. It appears to work today, but prefer
  `g_Strikes.outs - g_Strikes.storedOuts` for the new code and consider retiring the
  constant once the two are confirmed to agree.
- Whether pickoff and steal defense should share the contact event list or get its own
  (§6, last note).

Ghidra function names used for this research, for re-checking:
`updateVariablesPostCatch`, `catchAnimationProgression`, `calculateBobble`,
`bobbleDirection`, `checkIfCatchOccurs`, `checkIfRunningCatchOccurs`, `divingCatch`,
`clamberCheckCatch`, `clamberInitialization`, `wallJumpInitialization`,
`wallJumpSOmething3`, `wallSplat_setPosAndVelo`, `checkForCatchBallAction`,
`checkForAndSetFielderWallActionsOrDives`, `catchThrownBallFun`,
`ballThrownToEmptyBaseCatchAttempt`, `checkForAndHandleLooseBalls`, `knockBallLoose`,
`processFielderKnockout`, `handleBodyCheck2`, `autoMovement10_HasBall`,
`fielderHasBall`, `makeThrowVariables`, `initializeThrowAngle_Speed_Length`,
`tagOutValues`, `tagRelated`, `running_checkForOuts`, `runnerOut`,
`running_CheckForForceOuts_UpdateStamina_UpdateTagOutVars`, `postPlayTrackStats`,
`midPlay_trackStats`, `monitorForErrors`, `pickoff_infieldThrow_related`,
`update_runnersBeingTargetedWhileBatterCanBeForcedOut`, `updateStatsBasedOnABResult`,
`steal_pickoff_incrementSteal_runsStats`, `setDefaultInMemFielder`,
`fielderResetAndStoreValuesEachFrame`, `fielder_endOfInning_deadball_updateCounters`.
