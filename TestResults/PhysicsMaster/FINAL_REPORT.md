# Road to F1 — current physics readiness, 2026-10-19

## Latest: Stage9 PASS; Stage10 next

Stage9Acceptance1:43/43 full suites pass (all previous31 preserved),189.5648s. Editor18.64s/game115.89s Development builds and whitespace checks pass. Actual solo circuit,48 forward/reverse rejoin cases, controlled2/5/10/20 fleets and live20-kart controls/physical ordered lap now pass. Corrected separate tire/chassis static envelope from measured authored geometry; retained full kart-pair hull and57/55caps. Added physical braking-distance planning only for Custom AI; Legacy unchanged. No human driving or Stage10 completion claim. Experimental user gates remain closed pending final validation. User has now explicitly authorized eventual push/merge AFTER implementation/testing; no remote changes yet. Earlier incomplete reports below are historical, superseded by this status.

**Latest continuation: Stage9 INCOMPLETE; Custom is NOT ready for experimental human South Garda driving. Stage10 NOT STARTED/PASSED. Legacy remains default.**

Detailed current source state, every file, evidence, commands and next steps: [Codex handoff](../../CODEX_HANDOFF_STAGE9_2026-10-19.md).

- Latest full suite: ../PhysicsStage9/FleetFull1/index.json — **40 passed,1 failed,0 skipped,0 warning suites**,216.24244689941406s. All previous31 regressions still pass.
- Failure: RoadToF1.Physics.Stage9LiveIntegration — actual physical lap stalls below1km/h for30s atCP2. Earlier240simulated-second run also failed to complete a lap.
- Actual cooked coverage now implemented:11 physical meshes,35,724 triangles,BVHheight14. Old count12 included the nonphysical builder brush. PIE debug-draw primitives are also explicitly excluded; unknown physical shapes fail closed. No mesh-bounds substitution.
- Shared world120Hz coordinator, unique canonical pairs, coupled static/dynamic constraints and progressive2/5/10/20 controlled value/actor lifecycle tests pass. Mesh/CCD/contact regressions pass. Coverage and fleet are no longer missing implementations.
- Captured first-bend CCD convergence failure repaired with directional support bounds and bounded interval certificates; rotated/fast/parallel sweep tests pass.
- Editor Development PASS46.25s (live-profile-build.log), standalone Development PASS137.39s (integration-game-build.log). Later edits are editor-only diagnostics/tests. Diff check passes;16 untracked source files have no trailing whitespace.
- Live observations: full throttle/no brake can coexist with a stationary body and persistent contact impulses. Some NPCs lose both driven-wheel loads; others retain loaded driven wheels. Common stall root cause is NOT yet confirmed. Live max penetration4.59771cm and residual22.64134cm/s require investigation.
- Live timing at9542steps:mean2.9582ms,max8.9609ms. This is a failing Stage9 diagnostic, NOT Stage10 full-race profiling.
- Smallest next action: capture opposing contact source/pair IDs, normals, depth/impulses and wheel support hits atCP2; isolate actual road/grass/kerb versus chassis/support and speculative-pair constraints. Do not shrink hulls, disable collisions or weaken the physical-lap gate to pass.
- Both Custom user admission gates remain false. No human Custom driving, completed Custom three-lap race, Shipping/cooked or camera comfort acceptance.
- Branch visual-overhaul-first-person;HEADd953f2aff16f8f05b3b3883a9fadf2d5284ddcbf. Uncommitted work preserved; no checkpoint/push/merge/reset/assets/original-project edits. Preserved original remains prototype-v1-preserved at391839b.

## Historical pre-fleet report (retained; superseded by status above)

The following describes the earlier numerical recovery, before mesh/fleet continuation. PRE_FLEET_REPORT.md also preserves that earlier snapshot.

The original Stage9 angled/resting-wall numerical failures are repaired. Final full regression:31 passed,0 failed,0 skipped,0 warning suites (153.42s). Editor and standalone Win64 Development builds pass. Overall Stage9 remains incomplete because actual-track collision coverage and multi-kart integration are missing. Stage10 was not started. Custom is not admitted for normal driving; Legacy remains default.

## What changed
The scene bridge now rejects false suspension support from rays starting inside barriers, reconstructs exact-box contact patches, handles near-touch boxes using independently validated SAT geometry, brackets rotational contact, separates stabilization from physical velocity, and preserves post-impact sliding time without applying forces twice. Existing0–8 systems were preserved; only box manifold fallback was extended, with all foundation tests passing. See ../PhysicsStage9/README.md for algorithms, measurements, source files and exact test instructions. Earlier history remains in PRE_RECOVERY_REPORT.md and ../PhysicsStage9/RECOVERY.md.

## Validation evidence
| Check | Final result | Evidence |
|---|---|---|
| Full RoadToF1 suite |31 pass /0 fail /0 skipped;153.4196s|PhysicsStage9/RecoveryAcceptanceFinal/index.json|
| Physics foundation and Stage9 |13/13 pass within full suite|same report|
| Stage9 focused |4/4 pass, including18-case collision matrix and sliding-distance assertion|same report; ResumeSlide2/index.json|
| Editor Development |PASS14.21s|PhysicsStage9/recovery-final-editor.log|
| Standalone Development |PASS39.98s|PhysicsStage9/recovery-final-game.log|
| Whitespace |PASS, including untracked source scan|VALIDATION.json|

No new compiler errors. Existing HUD InputKey deprecations appeared in earlier recompilations; non-Windows SDK notices do not invalidate Win64 builds. No manual driving, Shipping/cooked build, human comfort, race performance or cross-platform determinism claim.

Thin-wall maximum centreX248.999791742cm against unchanged249.001cm assertion. Tangential travel13.9188m in5s; no false stationary sliding. Initial overlap0.1cm corrects to0.02cm slop without a velocity kick (peak1.44e-6cm/s). Resting0.01cm overlap remains motionless. Reverse, rotated walls, friction and corner fixtures pass. All failed intermediate evidence remains separate.

## Remaining acceptance gates
- Static coverage: actual SouthGarda map has12 query-enabled WorldStatic non-box components and0 supported UBoxComponents. These meshes cannot be replaced with bounds. Rays provide ground samples but do not establish barrier/contact coverage. Unsupported contacts fail closed.
- Shared multi-kart response is not implemented. Existing single-kart gate rejects race directors, SouthGarda and additional karts. No two/five/ten/twenty-kart Custom test or Custom three-lap race was executed.
- Actual Custom input/HUD/F9 schema compatibility, F5 keyboard reset, finish flow, V toggle and first-person comfort need integrated on-track validation after geometry coverage is complete. Isolated pawn/reset/view tests do not substitute for human driving.
- Fresh box manifolds are used; live persistent cache/warm starting is not integrated. Rotational endpoint bracketing is bounded mitigation, not universal convex rotational CCD.16-event exhaustion fails closed.
- No Custom race profiling, broadphase-tree performance claim or balancing change. Test durations are not physics frame-time benchmarks.

Per the requested stop conditions, Stage10 cannot safely run. The smallest next integration task is a validated collision adapter for the actual required static meshes/convex geometry, followed by single-kart track tests and a shared exactly-once kart-pair coordinator. Preserve the passing collision regressions during that work.

## Manual instructions / backend ownership
Open the visual project's .uproject withUE5.8.3, open /Game/RoadToF1/SouthGarda_KartRace, and Play for Legacy. For isolated Custom verification save editor work, then console `Automation RunTests RoadToF1.Physics.Stage9`; fixtures create/load test worlds and exercise the gated Custom path. Ordinary `RoadToF1.Physics.CustomTest 1` must reject. There is currently no supported command to personally drive Custom on SouthGarda. Stop PIE/reopen the race map for Legacy; no source reversion is needed. Do not enable Custom for normal play before explicit user approval.

Custom transform/velocity response remains exclusive: Legacy movement is skipped, root rigid-body simulation is forbidden, scene queries do not move actors, and incident callbacks cannot apply a second physical speed/rotation response. The remaining-time substeps integrate already-updated velocities only, avoiding duplicated tire/engine forces.

## Git / preservation
Worktree D:/Boring/GitHub/Road-To-F1-Visual; branch visual-overhaul-first-person; HEADd953f2aff16f8f05b3b3883a9fadf2d5284ddcbf. Stage9 source/report changes remain uncommitted because all gameplay acceptance gates have not passed. No push, merge, rebase or original worktree changes. No map/content assets changed in this recovery.

Existing local checkpoints remain: d92c539 (0–4/visual preservation), e7c78c2 (5),6478f03 (6),89ab968 (7),d953f2a (8). Original worktree remains prototype-v1-preserved at391839b. No new checkpoint was created.

Recovery source changes are KartCustomSceneBridge.cpp/.h, Tests/KartPhysicsStage9Tests.cpp, KartPhysicsManifoldBuilder.cpp and KartPhysicsContactManifold.h. Prior Stage9 KartCustomGameplay.cpp, KartFixedStepClock.h, KartPawn.cpp/.h and KartPhysicsWorldSubsystem.h remain preserved. Reports updated: BUILD_LOG.md, PhysicsStage9/README.md, PhysicsMaster/PROGRESS.md, this report and validation/status evidence; PhysicsStage10/README.md records its blocked status.

Recommendation: retain Legacy for human racing. Numerical recovery is validated; full Stage9 and Stage10 readiness is not yet achieved.
