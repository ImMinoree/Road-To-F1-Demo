# Stage9 current status — 2026-10-19 continuation

## Latest: Stage9 automated acceptance PASSED

Stage9Acceptance1/index.json:43/43 pass,0 failures/skips/warning suites,189.5648s. Editor18.64s and standalone115.89s Development builds pass; diff/new-source whitespace checks pass. ChassisLive1 now completes an actual ordered player lap with20 active karts; SoloCircuit3 completes the actual circuit; ActualRejoins1 passes48 forward/reverse road-edge crossings. Controlled2/5/10/20 and all previous31 regressions remain passing.

Confirmed integration correction: the Legacy envelope includes low tire geometry and was incorrectly also used as a solid chassis against the road, alongside separate wheel support. Actual authored fairing minimum relative to22cm root/art offset is-12.2652cm. The static chassis conservatively uses lower face-12.5cm and unchanged upper20cm, with offset-aware geometric sweeps and mass-origin-correct anchors. Kart-pair98x63x20 envelope, scene triangles, caps and Legacy behaviour are unchanged. A Custom-only braking-distance/selected-lane envelope adapts AI commands to force-driven grip; no hidden acceleration/speed boost. Braking-only SoloCircuit2 failed; combined corrected geometry/control SoloCircuit3 passes.

Stage10 is next, not yet accepted. Human driving/comfort and full Custom three-lap completion remain pending. Both ordinary Custom gates remain closed until final experimental readiness validation. The failed statuses below are retained as historical baselines and superseded by this section.

**Incomplete: actual live Custom lap stalls atCP2. Stage10 not started.** Latest full run FleetFull1:40 pass,1 fail,0 skipped,0 warning suites;216.2424s. All previous31 regressions still pass. Editor/standalone Development builds pass. Actual cooked11-mesh/35,724-triangle adapter, world coordinator and progressive2/5/10/20 controlled tests now pass. The historical missing-coverage/fleet blocker below is superseded.

Stage9LiveIntegration physically starts timing and crossesCP1/CP2, then stalls below1km/h for30s. Full throttle/no brake, driven-wheel support loss in some NPCs and persistent contact impulses are measured; exact common cause remains unresolved. Live max penetration4.59771cm and normal residual22.64134cm/s need investigation. Normal Custom admission stays false. No human Custom readiness or full-race claim.

Current files, exact evidence, next steps and commands: repository-root CODEX_HANDOFF_STAGE9_2026-10-19.md. Concise current report: ../PhysicsMaster/FINAL_REPORT.md. Historical numerical recovery below is retained intact.

## Historical numerical recovery status (pre-fleet)

Numerical collision recovery PASSED. Overall Stage9 gameplay acceptance remains BLOCKED by actual-track collision coverage and unfinished shared multi-kart integration. Stage10 was not entered. Legacy remains default; ordinary Custom selection remains disabled.

## Confirmed causes and corrections
- Near-stationary Unreal sweeps returned an out-of-interval Time1.3749094; a second fixture had0.000610351562cm initial overlap. The old bridge rejected both before the solver. Finite out-of-interval hits are not used as TOIs; exact endpoint geometry is independently checked.
- A wheel ray starting inside the wall was accepted as upward-facing ground. At step0 this produced vertical velocity12.25cm/s and an artificial suspension kick. Reject interior/zero-time/above-mount support hits and retry below rejected components, bounded to4 queries. Real floor support remains tested.
- A single cast corner introduced artificial angular response. Reconstruct a multi-point box contact patch, with bounded geometry-only contact margin and fresh anchors.
- Near-touch GJK Degenerate/IterationLimit cases use an independent exact OBB SAT and face clipping. EPA failures are still rejected; no false EPA-convergence claim. Hull/mesh failures remain unsupported.
- Translation-only sweeps missed new overlap from rotation. Bracket newly intersecting exact box poses with32 bisections and advance consistently in translation/orientation to the conservative TOI.
- Penetration velocity bias left initial-overlap bodies coasting. Separate physical impulses from bounded split pseudo-velocity stabilization, preserving real velocity. No global velocity clamp or arbitrary relocation.
- Discarding post-impact time suppressed sliding despite sideways velocity. Integrate remaining motion without reapplying engine/tire forces, with a16-contact-event budget and fail-closed exhaustion. Separating/tangential touches do not trigger repeated closing response. Sub-micron remaining travel may be discarded as numerical termination.

## Final validation
- RecoveryAcceptanceFinal/index.json:31 succeeded,0 failed,0 skipped,0 warning suites;153.419631958s. Includes13 physics suites and18 existing gameplay suites.
- Focused ResumeSlide2:4/4 Stage9 suites passed before final diagnostic-counter-only update; final full run repeats all4 successfully.
- Editor Win64 Development PASS14.21s: recovery-final-editor.log.
- Standalone Win64 Development PASS39.98s: recovery-final-game.log.
- git diff --check and new-source trailing-whitespace scan PASS. Line-ending notices only. No Content assets modified.
- Earlier failed runs retained separately, including RecoveryDiagnostic, RecoveryFull1, ResumeFocused, ResumeSplit, ResumePhysicsFinal and ResumeSlide. ResumeFullFinal30/30 is a pre-sliding-change result and is not substituted for the final31/31 result.

## Measured final fixtures
- Thin wall atX300,half-width1cm: max chassis centreX248.999791742cm, unchanged assertion<=249.001. No tunneling in that fixture.
- Tangential case:1391.883cm Y travel in5s; assertion requires>200cm. Peak speed299.960cm/s; final tangential speed267.681cm/s.
- Head-on peak angular speed0.08636rad/s (earlier single-corner draft18.095rad/s).
- Initial overlap0.1cm: final residual about0.02cm slop, movement0.08cm, peak physical speed1.44e-6cm/s.
- Resting0.01cm overlap: zero recorded speed/drift across600steps.
- Reverse-away: finalX-907.128cm; clear of barrier. Rotated30/45/60degree barriers, alternate axis, low/high friction, repeated small impacts, shallow/medium angles and two-wall fixture pass.
- Maximum reported pre-solve residual closing speed in high-friction fixture0.13884cm/s; no nonfinite/fault in final matrix. These are synthetic fixed120Hz tests, not a human-driving or full-track performance claim.

## Acceptance blocker and coverage
Actual /Game/RoadToF1/SouthGarda_KartRace audit:0 supported UBoxComponents,12 query-enabled WorldStatic non-box components. Track authoring uses complex-as-simple static-mesh collision. Ground rays can hit those meshes, but the response adapter does not reconstruct their triangle/convex contacts. Unsupported contact fails closed; no bounding-box replacement or omitted barrier is accepted.

The coverage-audit test passes because it detects this deficiency and confirms runtime admission is closed. It does not certify the track. A shared Custom kart-pair coordinator, multi-kart scenarios, AI compatibility, live Custom F5/F9, camera driving, finish/reset flow and three-lap Custom race are not accepted. Per the requested stop conditions, do not start Stage10/full20-racer Custom tests until these prerequisites pass.

## Files
Recovery edits: KartCustomSceneBridge.cpp/.h, KartPhysicsManifoldBuilder.cpp, KartPhysicsContactManifold.h, Tests/KartPhysicsStage9Tests.cpp under RoadToF1/Source/RoadToF1. Existing uncommitted Stage9 KartCustomGameplay.cpp, KartFixedStepClock.h, KartPawn.cpp/.h and KartPhysicsWorldSubsystem.h preserved. No unrelated AI/HUD/audio/visual edits.

## Exact Editor testing
1. Open D:/Boring/GitHub/Road-To-F1-Visual/RoadToF1/RoadToF1.uproject with UE5.8.3.
2. Normal driving: open /Game/RoadToF1/SouthGarda_KartRace and Play. This uses Legacy;57/55km/h caps remain.
3. Save work before automation because fixtures create/load temporary worlds. In editor console run `Automation RunTests RoadToF1.Physics.Stage9` (or Session Frontend > Automation, filter RoadToF1.Physics.Stage9). Expect4 passing suites: SingleIntegration, StaticMatrix, BoxNearTouch, CoverageAudit.
4. To repeat everything, run `Automation RunTests RoadToF1.`. Expect31 suites on this source.
5. `RoadToF1.Physics.CustomTest 1` in ordinary PIE must reject. Custom is not safe/admitted for user driving yet. Do not remove the gate to try SouthGarda.
6. Stop PIE and restart the race map to return to the default Legacy world. The isolated automation verifies stationary Custom-to-Legacy fallback/reset; manual F5/F9 Custom driving is pending.

No rendered/manual driving occurred. No new checkpoint: full Stage9 gates remain unmet. Branch visual-overhaul-first-person, HEADd953f2a. No push/merge or original-project modification. See RECOVERY.md for earlier numerical evidence and ../PhysicsMaster/FINAL_REPORT.md for overall readiness.
