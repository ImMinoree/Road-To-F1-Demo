# Road to F1 — Codex continuation handoff

## Latest continuation override (read before historical status below)

The user subsequently authorized finishing implementation, then pushing/merging. Stage9 now PASSES: Stage9Acceptance1 full43/43,189.5648s, Editor18.64s and game115.89s Development builds pass, whitespace clean. Actual solo lap115.817s,48 forward/reverse actual rejoin cases, controlled2/5/10/20 and live20-kart player lap129.567s pass. All previous31 preserved. A local Stage9 checkpoint is being saved; read git HEAD rather than assuming the historical d953f2a is current. No push/merge yet. Stage10 is NEXT and must be completed before final merge/readiness claim.

Changes since this historical handoff: opt-in per-contact source/impulse diagnostics; actual solo/rejoin tests; KartPhysicsDrivingEnvelope.h physical braking-distance/selected-lane planning for Custom AI and shared scripted test driver; StaticBottomInsetCm (default0, live7.5) separates measured chassis underside from tire envelope for STATIC contacts only. Full kart-pair box98x63x20 unchanged. Static chassis lower face-12.5cm conservatively encloses authored fairing minimum-12.2652cm (9.7348cm fairing height minus22cm root/art offset), upper face20cm unchanged. Offset-aware triangle CCD and mass-origin anchors prevent double-counting tire low geometry as a rigid road plow. Braking-only SoloCircuit2 failed; geometry/control correction SoloCircuit3 passes. Legacy and caps unchanged. Both user Custom gates still false while Stage10 is pending. Fresh reports at TestResults/PhysicsStage9/Stage9Acceptance1; logs/build log contain current chronology.

The original CP2 stall and40/41 status described below are preserved historical evidence, not current readiness status. Proceed with Stage10 actual three-lap NPC/player integration, finish/reset safety and performance profiling, then explicit opt-in driving access and final regression/builds. Do not claim human testing; do not enable default Custom. User saved/closed Unreal. No original-worktree or Content asset edits in this continuation.

Updated 2026-10-19 after the latest full automation run. Read this before editing.

## 1. Most important current facts

- Active worktree: `D:\Boring\GitHub\Road-To-F1-Visual`.
- Unreal project: `D:\Boring\GitHub\Road-To-F1-Visual\RoadToF1\RoadToF1.uproject`.
- Map: `/Game/RoadToF1/SouthGarda_KartRace`.
- Engine: UE 5.8.3, installed at `D:\Boring\UE_5.8`.
- Branch: `visual-overhaul-first-person`.
- HEAD: `d953f2aff16f8f05b3b3883a9fadf2d5284ddcbf`.
- All Stage 9 continuation changes are UNCOMMITTED, including many new/untracked source files. Preserve them.
- Legacy remains the default. Normal Custom gameplay admission is CLOSED.
- Stage 9 is NOT complete. Stage 10 has NOT been started or passed.
- Latest full suite: **40 passed, 1 failed, 0 skipped, 0 warning suites**, 216.24244689941406 seconds.
- The one failing suite is `RoadToF1.Physics.Stage9LiveIntegration`: a genuine physically driven player lap stalls around checkpoint 2. Do not hide, remove, or relax this acceptance gate.
- Editor and standalone Win64 Development builds pass. No human Custom driving has been validated.
- No new commit, push, merge, reset, rebase, asset edits, or original-project changes were performed during this continuation.
- Latest validation has COMPLETED; no owned Unreal automation/build process remains pending as of handoff. Last automation session ID was 7421, now completed. Read exported results instead of rerunning blindly.

Create a new Codex chat/project entry pointing to THIS EXISTING directory. Do not create another Unreal project copy.

## 2. User's current authorization and objective

Complete Stage 9 and proceed automatically to Stage 10 only after required Stage 9 gates pass. Work continuously without asking for a new prompt after every success.

Required work:
1. Accurate South Garda static collision coverage from actual Unreal collision geometry. No arbitrary mesh bounding boxes.
2. One shared world fixed-step Custom coordinator, deterministic ordering, each kart pair once per step, progressive 2/5/10/20 validation.
3. Real input/steering/braking/reverse/reset/camera/HUD/F9/checkpoint/lap integration.
4. Then Stage 10 representative race stability, ordered three-lap completion, existing AI compatibility, collision diagnostics and physics profiling; compare Legacy and Custom where possible.
5. Focused tests, full automation, Editor and standalone builds, `git diff --check`, updated reports.

Safety rules:
- Preserve all uncommitted work and user assets. Do not reset/discard anything.
- Do not modify `D:\Boring\GitHub\Road-To-F1`, the preserved original project.
- Do not push or merge. Local checkpoint commits are allowed ONLY AFTER successful complete stage validation. No Stage 9 checkpoint is appropriate yet.
- Do not enable Custom for normal play without approval. Explicit experimental development selection may be admitted only after its gates pass.
- Legacy stays fully functional and default. Keep player/NPC caps 57/55 km/h.
- Do not repeat or rewrite completed Stages 0–8.
- Do not weaken tests, widen checkpoints, disable collisions, teleport racers to pass gates, use hidden speed boosts, or relabel Legacy/scripted results as human Custom results.
- Report a genuine remaining blocker with exact failing test/evidence and smallest next action.

Read `AGENTS.md` first. Reference directory is actually misspelled **`Refrence`**, despite `AGENTS.md` calling it References. Study algorithms and adapt; do not copy reference source blindly. No subagent delegation was used for this continuation.

## 3. Broader game context

Road to F1 starts in karting, with future career progression through junior formulas toward F1. South Garda, Italy is the first prototype circuit. Genk, PFI and Bahrain were discussed as future venues; they are not this physics task.

Existing Legacy game includes the authored South Garda circuit, player kart, seated drivers with distinct suits and number badges, 19 varied NPC opponents, 20-racer standings, ordered checkpoints and three-lap timing, finish/reset behavior, collision incidents/fire/stun/recovery, sound/commentary, dynamic racing HUD, first-person/chase cameras and F9 local telemetry/training framework. These working systems must be retained. Current work is the visual/physics worktree, not the old `fix/packed-race-stability` branch discussed in historical chat.

Earlier AI pace/racecraft, audio, HUD, badges and training work is preserved. Do not infer new changes to those systems from this handoff: this continuation changed physics integration and its tests, not unrelated gameplay. Human footage/telemetry supplied earlier is evaluation data, not approved training data.

Preserved original worktree was verified at branch `prototype-v1-preserved`, HEAD `391839b`. Do not touch it.

## 4. Architecture and completed foundations (do not redo)

Collision ownership is Option B:
- Unreal supplies scene queries and cooked geometric collision data.
- Custom owns its rigid-body velocities, forces, impulses, poses and final published transform.
- A Custom kart never runs Unreal rigid-body simulation or Legacy swept movement in the same step.
- Gameplay incident consumers may count/display events but do not add a second physical impulse/rotation for Custom.
- Kart pairs are canonical and coordinated centrally.

Units: cm, seconds, kg; 1 N = 100 force units; 1 N m = 10,000 torque units.

Completed checkpoint history:
- `d92c539`: preserved visual work and physics Stages 0–4.
- `e7c78c2`: Stage 5 normal PGS solver.
- `6478f03`: Stage 6 friction/restitution.
- `89ab968`: Stage 7 isolated force-driven vehicle.
- `d953f2a`: Stage 8 physical view feedback (current HEAD).

Existing foundation source under `RoadToF1/Source/RoadToF1`:
- Backend/provider/world: `KartPhysicsBackend.h`, `KartCollisionProvider.*`, `KartPhysicsWorldSubsystem.*`.
- Body/integration: `KartRigidBodyState.*`, `KartPhysicsIntegrator.*`, probe code.
- Broadphase: AABB/proxy/body ID/pair types, `KartPhysicsBroadphase.*`, dynamic AABB tree.
- Narrowphase: convex shape, GJK, EPA, narrowphase files.
- Manifolds: builder/cache/contact types.
- Solver/materials: `KartPhysicsSolver.*`, `KartContactMaterial.h`.
- Vehicle/view: `KartExperimentalVehicle.*`, `KartPhysicsViewFeedback.*`.
- Existing Stage 0–8 tests in `Tests/KartPhysicsStage[0-8]Tests.cpp`.

Stage 7 uses a prototype four-support-point vehicle with load-dependent combined tire forces, wheel angular velocity, spring/damping and torque governor. This is not a calibrated production tire model. No solver warm starting is implemented: every physical batch starts from zero impulse. Stage 4 geometric persistent IDs/cache are used by the fleet; do not claim this is impulse warm starting.

Useful references already studied:
- `Refrence/crashcat/src/collision/triangle.ts`.
- `Refrence/crashcat/src/shapes/triangle-mesh.ts`.
- `Refrence/crashcat/src/shapes/utils/triangle-mesh-bvh.ts`.
- `Refrence/crashcat/src/constraints/contact-constraints.ts`.
- `Refrence/car-physics-demo/src/contact_constraint.cpp` and `wheel_contact_constraint.cpp`.

## 5. Stage 9 numerical recovery that was already passing before this continuation

Read `TestResults/PhysicsStage9/RECOVERY.md` for historical investigation. Before adding mesh/fleet work, `RecoveryAcceptanceFinal/index.json` had 31/31 passing full suites (153.4196s), Editor PASS14.21s and game PASS39.98s.

Preserved repairs:
- Reject inside-wall, zero-time, above-wheel suspension support hits; retry below rejected components, max4.
- Unreal sweep Time outside [0,1] is not a valid TOI; independently check endpoint geometry.
- Reconstruct exact box contact patches, not one arbitrary sweep corner.
- Independently validated exact OBB SAT fallback for GJK Degenerate/IterationLimit near touch. Deliberate EPA failures remain failures.
- Bracket newly intersecting rotating box endpoints with32 bisections.
- Separate physical velocities from bounded split pseudo-velocity penetration correction.
- Preserve remaining post-impact motion without applying tire/engine forces twice;16 event budget, fail closed.
- Thin-wall max centre X248.999791742cm against unchanged <=249.001cm assertion.
- Tangential sliding13.9188m in5s; resting0.01cm overlap stable; initial0.1cm overlap corrects to0.02cm slop without a velocity kick.

The previous report's mesh/fleet blockers were historical. Accurate geometry import and shared fleet now exist, but live race acceptance still fails. Do not mistake an old report saying 'no fleet implementation' for current source state.

## 6. New accurate static geometry implementation

New `KartStaticMeshGeometry.h/.cpp`:
- Immutable snapshot of actual `UBodySetup::TriMeshGeometries` cooked Chaos triangles, transformed into world space. Not visual render bounds.
- Actual South Garda audit corrected the old count: **11 physical meshes + one nonphysical editor builder brush**, not12 physical meshes.
- All11 use complex-as-simple collision and have no simple aggregate shapes.
- **35,724 physical triangles**, median-split static triangle BVH, height14.
- Mesh counts: Grass7212, Asphalt2212, Kerb_blue1344, White2088, Concrete108, Roof416, Glass264, Aluminium320, Fairing700, Bark10880, Foliage10180.
- Exact UBoxComponent sources can also be tessellated to12 real box triangles. Unsupported physical geometry fails closed; simple mesh aggregate shapes currently require another explicit adapter.
- Only default editor builder brush and `UDebugDrawComponent` presentation primitives are explicitly excluded. PIE's GameplayDebugger rendering component inherited misleading WorldStatic/query flags but has no geometry; this caused LiveIntegration1 and was corrected.
- Complete finite triangle/OBB SAT and clipped1–4 point witnesses. TriangleClip method appended to manifold enum.
- Snapshot source transforms/BodySetup identity/revision checked for staleness. New static sources are not dynamically discovered every step; selecting/resetting rebuilds as needed. Dynamic scene editing/streaming remains a limitation.

New `KartMeshSceneMotion.h/.cpp`:
- Existing 0.02cm contact margin, exact current/speculative triangle contacts, custom physical PGS and separate stabilization.
- Conservative mesh CCD with finite geometry, remaining-time advancement and16 event cap; no mesh AABB physical response.
- Near-parallel clipped-coordinate roundoff handled before dividing by tiny normal components.
- Live first-bend CCD initially failed96-iteration convergence due overly loose radius/angular motion bounds near tangency.
- Added a captured-state regression, spatial termination1e-7cm inside the contact margin, projected angular support-motion bounds and bounded left-first interval certificates (4096 node budget, depth30). Failure remains fail-closed. Not an unlimited/universal rotational CCD claim.
- Captured case passes: cast t0.824638160386, positive gap0.0100000873812cm; independent dense sampled first skin contact t0.8247.

Bridge additions in `KartCustomSceneBridge.*`:
- Shared robust ground sampling, actual mesh-name material classification for Grass/Kerb.
- `Prepare`: integrates wheel/force/velocity state once, restores pose for shared solve.
- `Move`: advances already-prepared velocities with static contact handling; no duplicate engine force.
- `HasGroundSupport`: all four wheel-ray validity preflight.
- Optional scene pointer selects cooked mesh path. Old null-scene exact-box regression path retained.

`RoadToF1.Build.cs` adds private Chaos module dependency to READ cooked geometry. This is not Chaos Vehicles simulation, a new plugin or a backend switch.

## 7. New shared fleet value simulation

New `KartCustomFleet.h/.cpp`:
- `FKartFleetBody`: stable ID, vehicle state, normal input, synchronous actor-ignore pointer and net impact impulse.
- `FKartCustomFleet`: transactional body batch, Stage2 broadphase and Stage4 geometric manifold cache.
- Sort by body ID for deterministic processing.
- Prepare forces exactly once per body, build swept/interaction-envelope bounds, generate each canonical kart pair once.
- Current or speculative geometric constraints, plus static triangle constraints in the SAME shared velocity batch.
- New `FKartContactPoint::AllowedClosingSpeed` defaults0. Gap/dt bound allows safe approaching motion instead of stopping every near pair immediately. Solver validates it and suppresses premature restitution for speculative rows.
- Physical bias0; separate pseudo-velocity stabilization. No hidden velocity cap change.
- Coupled-chain failures were diagnosed, not hidden: newly accelerated middle kart could hit a pair absent from free prediction; add reachable interaction envelope and finite face patch.
- Residual-aware adaptive solve: initial24 iterations, restore untouched pre-solve velocities and retry128 if residual>.05cm/s and multiple constraints. Commit only the accepted solve; no double application.
- Direct/side/glance/rear/three-kart contact cases preserve post-step overlap assertion <0.2cm.
- Chain fixture final peak overlap0.003870413cm. Earlier failed drafts retained as FleetContacts1–4; successful FleetContacts5.
- Diagnostics: steps, canonical pair counts, static pairs, rows, geometric reuse, residuals, penetration, iteration retries, total/max CPU step time.

`KartExperimentalVehicle.*` adds validated `CollisionHalfExtent` setting (default50,40,10) used by scene/fleet collision shapes. Live actors currently supply authored Legacy collision box **98,63,20**. The existing central mass/inertia model still uses wheelbase100, track80 and30cm height. This hull/mass/support compatibility needs careful diagnosis; do not silently shrink the hull to make tests pass.

## 8. New live world coordinator and controls

`KartPhysicsWorldSubsystem.h` now derives from `UTickableWorldSubsystem`; new implementation in `KartCustomWorld.cpp`.

- `SelectFleet`: atomic all-kart admission,1–20 unique kart numbers, stationary Legacy actors, no Unreal simulation, four valid ground samples, complete static scene, static spawn overlap >2cm rejected. All current AKartPawn actors must be included; mixed Legacy/Custom racing forbidden.
- World owns ONE `FKartFixedStepClock`,120Hz with max8steps/frame, discarded backlog counted. Individual fleet pawns no longer independently advance.
- `AKartPawn::Tick` Custom branch skips Legacy and uses old isolated path only when not a fleet member.
- `ReleaseFleet` only when all bodies stationary; restores Legacy collision profile.
- Resets clear body velocities, view history, contacts/broadphase, shared clock and faults. Finished hidden racers retire collision bodies and can be recreated on F5 reset.
- Actor publication is unswept position/orientation interpolation, not Unreal collision response.
- Existing throttle/brake/stun/finish inputs and AI director remain active. NPC steering input is adapted to preserve the director's requested bicycle curvature under the Custom rack model; no AI decision/style rewrite or speed boost.
- Real acceleration/angular velocity/net contact impulse feed existing Stage8 camera feedback. Existing wheel presentation/audio/number update still runs.
- Fault freezes fleet publication and logs one actionable error. Reset required.
- Newly appearing unregistered Legacy kart causes a fault instead of silently mixing ownership.
- No initial kart-pair overlap preflight has yet been added; actual authored launch fixture passes, but arbitrary overlapping spawn admission remains a limitation.

Gates/commands:
- `FKartCustomSceneBridge::IsGameplayReady()` remains false (old restricted single-kart normal admission).
- `UKartPhysicsWorldSubsystem::IsExperimentalRaceReady()` remains **false**.
- New non-Shipping console `RoadToF1.Physics.CustomRace 1` collects all karts but ordinary PIE MUST REJECT until that experimental readiness gate is validated. `0` requests stationary release.
- Automation uses `GIsAutomationTesting` to exercise integration behind the closed user gate.
- Do not remove the gate merely to let a human try the failing build.
- Some header comments still describe the older single-kart-only architecture; update them when returning to implementation, without changing behavior accidentally.

## 9. Tests and latest exact results

Latest full acceptance evidence:
`TestResults/PhysicsStage9/FleetFull1/index.json`
`TestResults/PhysicsStage9/fleet-full1.log`

Result: **40 success /1 fail /0 skipped /0 warning suites**,216.24244689941406s.
All previous31 suites still pass in this full run. New passing suites:
- Stage9TriangleGeometry.
- Stage9CookedMeshCoverage.
- Stage9ActualMeshMotion (real map, independent Unreal ray agreement for all11 sources, actual finite wall impacts and grid acceleration/brake/reverse).
- Stage9TriangleCastConvergence (captured first-bend case).
- Stage9TriangleSweepMatrix (27 rotated/fast/spinning crossings plus finite-miss and parallel-floor tests).
- Stage9SpeculativePair (analytical momentum/gap allowance).
- Stage9FleetContacts (five contact configurations).
- Stage9FleetProgression (2,5,10,20 progressively, reverse insertion replay, no launch constraints, lifecycle/reset).
- Stage9ActualWorldFleet (actual map and actors2,5,10,20: movement, camera mode, reset, fallback, one shared clock).

Controlled progression timing from FleetProgression1, not full racing:
2 mean0.0396ms max0.1892;5 mean0.08546 max0.1483;10 mean0.1673 max0.2629;20 mean0.3443 max0.5340. Do not present these as full-race performance.

### Failing live acceptance

`Tests/KartPhysicsLiveTests.cpp` runs actual PIE frames with FApp fixed1/60 clock (restored after test), existing19 NPC controls, real physics and a clearly scripted player evaluation driver. It never teleports to gates or manually awards progress.

It exercises W, Space, S, V, F9, F5 through PlayerController input, HUD existence, then a real ordered lap with existing route. HUD visual rendering/comfort is not established by NullRHI. F9 schema remains unchanged; short test recordings may be in Saved/Training; no candidate was deliberately promoted or human telemetry trained.

Earlier LiveIntegration4 ran240s of simulation, accepted CP0/1/2, then timed out with player around `(12565.493,-3932.144,23.560)` and zero speed. Most NPCs stalled off-line; a few got farther (one CP8), no full lap accepted. It completed naturally; an attempted targeted stop came after the process had already exited, so no interruption report was created.

Latest FleetFull1 live test adds10-second wall snapshots and an EARLIER failure for30 continuous simulated seconds below1km/h, while retaining the240s lap deadline and600s wall watchdog. This strengthens failure detection; it does not turn the failure into a pass.

Exact latest error:
`Custom physical lap failed: player stalled below1km/h for30s; see body/contact snapshots`.

Measured snapshots (live test, not Stage10 full race):
- sim26.683s:2482steps,mean0.9132ms,max2.5868ms,playerCP2,speed1.440km/h.
- sim85.517s:9542steps,mean2.9582ms,max8.9609ms;61,498 cumulative dynamic pair solves,149,688 static pairs,603,836 rows.
- Max recorded current penetration4.59771cm; maxnormal residual22.64134cm/s. These live values are NOT equivalent to the much cleaner controlled contact fixture. Investigate them.
- Player at sim85.517 has throttle1,brake0,speed0, position about `(12565.496,-3932.148,23.553)`; near-level but tilted orientation; wheel loads `[0,54240.924,61273.072,16662.698]`,rear tire forces `[25600,12497.024]`; net contact impulse about `(317.365,-8.511,205.183)`;collision counter1.
- NPC5 has throttle0.984,brake0,zero chassis speed;front loads `[28229.610,47927.322]`, BOTH REAR LOADS0,spinning rear wheels~136rad/s. This is measured loss of driven-wheel support, not proof all stalls share one cause.
- Other stationary NPCs still have four loaded wheels and nonzero rear tire forces while contact response balances them. Do not assume every stall is just rear-wheel support.

What IS confirmed: actual live integration is not race-ready despite isolated success; full throttle/no brake can coexist with a stalled chassis, wheelspin or unsupported driven wheels and persistent contacts. It is not merely an absent throttle command or top-speed cap issue. The initial CCD convergence failure was repaired and regression-tested.

What is NOT confirmed: exact common cause of the pack stalls. Candidate investigations: authored full kart box vs smaller Stage7 support/mass model; asphalt/grass/kerb clearance transitions; chassis support unloading driven wheels; finite-triangle internal-edge/seam contact normals; excessive static body friction; speculative kart constraints; coupled residual penetration and initial corner understeer. Do not announce a guessed root cause as established.

### Build evidence
- `world-fleet-build.log`: Editor PASS218.48s.
- `live-profile-build.log`: latest Editor PASS46.25s, includes latest diagnostic/test source.
- `integration-game-build.log`: standalone RoadToF1 Win64 Development PASS137.39s. Latest changes after this build are editor-only test diagnostics; runtime code is the same.
- Initial missing FWeakObjectPtr include and PIE test FInputKeyParams include errors were fixed. New test switched to current FInputKeyEventArgs API. Historical existing HUD InputKey deprecation warnings remain in full recompiles.
- `git diff --check` was passing (line-ending notices only); rerun at next milestone and scan untracked source separately.
- No Shipping/package/cooked execution, rendered screenshot acceptance, manual Custom driving or cross-platform determinism claim.

Failed/intermediate evidence is intentionally separate. Do not delete it or replace it with a new run in the same folder. Useful folders: RecoveryAcceptanceFinal, MeshMotion2, FleetContacts5, FleetProgression1, ActualWorldFleet1, CastConvergence1/2/3, LiveIntegration1/2/3/4, LiveDiagnostic1, FleetFull1.

## 10. Files created/modified in current uncommitted work

All source paths below are relative to `RoadToF1/Source/RoadToF1/`.

New files (some began in earlier interrupted Stage9 work; preserve all):
- `KartCustomGameplay.cpp` — gated backend selection, isolated integration/reset.
- `KartCustomSceneBridge.h/.cpp` — read-only queries and custom scene motion response.
- `KartFixedStepClock.h` — bounded120Hz accumulator.
- `KartStaticMeshGeometry.h/.cpp` — actual cooked triangle snapshot/BVH/contact generation.
- `KartMeshSceneMotion.h/.cpp` — finite triangle CCD/custom static response.
- `KartCustomFleet.h/.cpp` — deterministic shared value simulation.
- `KartCustomWorld.cpp` — world tick, atomic live fleet ownership, publication and console command.
- `Tests/KartPhysicsStage9Tests.cpp` — earlier numerical scene recovery tests.
- `Tests/KartPhysicsMeshTests.cpp` — actual geometry, mesh motion and CCD tests.
- `Tests/KartPhysicsFleetTests.cpp` — speculative/contacts/progressive fleet tests.
- `Tests/KartPhysicsWorldFleetTests.cpp` — real map/actor fleet lifecycle tests.
- `Tests/KartPhysicsLiveTests.cpp` — actual PIE controls and failing physical lap acceptance.

Modified:
- `KartPawn.cpp/.h` — Custom ownership dispatch/velocity/public helper/incident guard.
- `KartPhysicsWorldSubsystem.h` — coordinator state/API and Tickable base; old subsystem cpp preserved.
- `KartExperimentalVehicle.cpp/.h` — explicit validated collision extent.
- `KartPhysicsContactManifold.h` — TriangleClip method, speculative gap allowance.
- `KartPhysicsManifoldBuilder.cpp` — earlier exact-box near-touch fallback.
- `KartPhysicsSolver.cpp` — validated speculative closing-speed bias and restitution policy.
- `RoadToF1.Build.cs` — private Chaos geometry dependency.
- `BUILD_LOG.md`, PhysicsMaster reports/progress, Stage9/10 reports and test outputs.

No Content/map/.uproject/plugin assets were edited in this continuation. Many older untracked reports/assets existed before this task. Do not bulk-add, delete, clean or overwrite them.

## 11. Exact next work (do not skip the failure)

1. Read this handoff, AGENTS.md, latest Stage9 report and FleetFull1 result/log. Check git status/branch/HEAD and Unreal processes before edits.
2. Preserve the40 passing suites. Legacy remains default; both Custom user gates false.
3. Diagnose the live stall at South Garda CP2 using measured body/contact snapshots. Add opt-in/test-only per-contact source IDs, normal/depth, impulse and wheel-support hit data; identify which constraints oppose forward movement. Distinguish dynamic kart jams from real mesh curb/body grounding and artificial seam normals.
4. Reproduce the cause in the smallest fixture using the ACTUAL South Garda geometry and body/support configuration; include stationary grass-to-asphalt rejoin, driven-wheel contact loss and first-bend path following as appropriate. No bounding-box replacement for meshes.
5. Verify/repair model compatibility or contact logic based on measurements. Do not shrink the chassis, disable friction/collisions, flatten the world, widen gates, teleport or boost engine speed merely to pass.
6. Retest relevant focused geometry/contact fixtures and2/5/10/20 progression after material runtime changes.
7. Make Stage9LiveIntegration pass a genuine physical ordered lap and reliable normal controls/reset/telemetry/UI integration. Add finish-state/reset evidence at the proper point. Existing controlled lifecycle checks are not full-race acceptance.
8. Full RoadToF1 suite and Editor/standalone builds, fresh output directories; check tracked diff and untracked source whitespace. Complete Stage9 report and only then consider a local checkpoint.
9. ONLY after all Stage9 gates pass, automatically implement/run Stage10: representative20-racer stability, all19 NPCs ordered3-lap completion, player scripted evaluation clearly labeled, AI style/launch/recovery/finish/reset compatibility, contact/depth/residual diagnostics, step-time distributions/budgets/dropped time, and honest Legacy comparison. No fabricated human performance.
10. Open explicit DEVELOPMENT experimental South Garda admission only if validated, keeping normal Legacy default. Provide exact driving commands/manual checklist. Stage10 passing does not equal human comfort validation.

Current deliverable answer is: **Custom is NOT yet ready for experimental human South Garda driving, Stage9 incomplete, Stage10 not passed.** Continue diagnosing; do not claim the stalled live race is complete.

## 12. Commands and environment notes

PowerShell, cwd `D:\Boring\GitHub\Road-To-F1-Visual`. Source reads/edits are allowed under D:\Boring. Unreal build/test caches outside the workspace required `sandbox_permissions=require_escalated` in this Codex environment; use routine authorized escalation rather than asking the user again unnecessarily.

Build (use a NEW log name):
```powershell
& 'D:\Boring\UE_5.8\Engine\Build\BatchFiles\Build.bat' RoadToF1Editor Win64 Development '-Project=D:\Boring\GitHub\Road-To-F1-Visual\RoadToF1\RoadToF1.uproject' -WaitMutex -gather -ForceRulesCompile -NoUBTMakefiles -NoUBA -NoPCH -NoSharedPCH -MaxParallelActions=4 '-Log=D:\Boring\GitHub\Road-To-F1-Visual\TestResults\PhysicsStage9\NEW-editor.log'
```
Standalone: same command, target `RoadToF1`, maxparallel2 if memory is tight. Machine32GB; commits can exceed physical memory. Avoid heavy parallel runs when profiling.

Automation (fresh output):
```powershell
& 'D:\Boring\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\Boring\GitHub\Road-To-F1-Visual\RoadToF1\RoadToF1.uproject' -unattended -nop4 -nosplash -nullrhi '-ExecCmds=Automation RunTests RoadToF1.' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=D:\Boring\GitHub\Road-To-F1-Visual\TestResults\PhysicsStage9\NEW-Full' '-abslog=D:\Boring\GitHub\Road-To-F1-Visual\TestResults\PhysicsStage9\NEW-full.log'
```
Replace filter with `RoadToF1.Physics.Stage9LiveIntegration` for the focused failing gate. `+` joins multiple test filters in ONE RunTests command. Separate repeated RunTests commands did not reliably execute both.

IMPORTANT: process exit code0 does NOT mean all Unreal tests passed. Parse index.json `succeeded`, `failed`, `notRun`, warning suites and per-test `state`.

Do not edit headers while UHT/build is running; changing generated-line offsets during a build previously caused avoidable compilation failures. Wait for build completion. Do not terminate unknown/user editor processes; verify any owned test PID/command before cleanup.

For live tests use actual normal engine ticks/latent commands. Manually ticking all actors repeatedly within one engine frame can skip world/tickable updates and is not equivalent. FApp fixed step globals must be restored even on failure. Existing older Legacy race fixtures sometimes manually cross gates/park the player; do not reuse them as proof of a physical Custom lap.

## 13. Suggested first prompt for the new Codex chat

> Read AGENTS.md and CODEX_HANDOFF_STAGE9_2026-10-19.md in D:\Boring\GitHub\Road-To-F1-Visual. Continue the existing uncommitted Stage9 work from HEADd953f2a on visual-overhaul-first-person. Latest full run FleetFull1 is40/41 with only Stage9LiveIntegration failing from a physical CP2 stall; diagnose the measured wheel/contact state rather than repeating completed stages or weakening tests. Keep Legacy default, player/NPC57/55, Custom user gates closed until validation, preserve all current work and the original worktree, no push/merge, checkpoint only after successful stage validation. Complete Stage9, then automatically Stage10 if prerequisites pass, with fresh evidence and accurate automated-versus-human reporting.
