# Stage 0 — Option B ownership foundation

This is scaffolding and read-only query validation, not custom physics.

## Selection and ownership
- AKartPawn.GetPhysicsBackend() always returns Legacy in Stage 0.
- TrySelectPhysicsBackend(Legacy) succeeds; Custom or an invalid enum fails without changing anything.
- There is deliberately no editable/serialized backend switch and no custom simulation tick.
- The existing pawn movement, collision response, game mode, race director, finish filtering, reset, AI, camera, audio and telemetry remain unchanged.
- Future Custom activation must be an atomic coordinator-owned transition: disable engine simulation and legacy movement/impact response before permitting custom response. Custom owns transforms, velocities, forces and impulses; Unreal supplies geometric queries only. Each kart pair must be solved once per shared step. Incident consumers must not add a second physical response.
- Those future transitions and solver guarantees are not implemented or validated by Stage 0. Custom cannot run now.

## Provider scope
The world subsystem owns a stateless collision provider; it does not tick or register bodies. SweepBox queries simple collision for WorldStatic and Pawn objects, ignoring the querying actor. Dynamic props are intentionally excluded. Results are FHitResult observations, not manifolds. Initial-overlap depth/normal quality needs further validation before solver use. Debug component IDs are session-local UObject IDs, not persistent physics handles. Pair labels are canonical debug labels, not a solver deduplication system.

## Exact Editor checks
1. Open RoadToF1.uproject and /Game/RoadToF1/SouthGarda_KartRace. Play. Drive, brake, reverse, toggle V and restart with F5. Expected: existing behavior and speed caps unchanged.
2. While stationary and facing a barrier or kart, open the console and enter `RoadToF1.Physics.Probe 600`. Expected for 5 seconds: cyan start box/path, yellow end box, Legacy/body/pair label; a red normal arrow if geometry is hit. Probe itself does not move either object or produce an incident. Live gameplay continues normally.
3. Try `RoadToF1.Physics.Probe 100` in clear space. Expected: boxes/path with no hit normal. Initial overlap with ground may be reported depending on position; this command is a geometry probe, not a driving prediction.
4. In a temporary Blueprint graph with a KartPawn reference, call Try Select Physics Backend with Custom, inspect its boolean and Get Physics Backend. Expected: false and Legacy. Legacy returns true. Do not save a map change for this test.
5. Stop PIE. Open Tools > Test Automation (Session Frontend Automation tab), filter RoadToF1.Physics.Stage0Ownership and run. Expected: passing static box, pawn-object query, miss/reset and initial overlap assertions; unchanged transform/speed/incident count and rejected Custom selection.
6. Restart PIE and repeat probe/F5. Expected: no stale body registry or contact state (none is stored).

The development console probe is compiled out of Shipping. No per-frame log/debug overhead was added. Visual checks remain manual unless separately documented.

## Validation — 2026-10-16
- Editor Win64 Development: PASS. Standalone Win64 Development: PASS.
- Full RoadToF1 automation: 19/19 pass, 0 failures, 0 skipped, 144.38 seconds. 12 passed suites contain the audio-device warning AUDCLNT_E_RAW_MODE_UNSUPPORTED. Stage0Ownership passes.
- git diff --check: PASS; Git emits line-ending conversion notices.
- Initial sandbox build exited before useful diagnostics. Standard/PCH compiler attempts stalled and were cancelled. Successful one-off build flags: -NoUBA -NoPCH -NoSharedPCH -MaxParallelActions=4. UE 5.8 still calls this executor UBA but runs actions without detouring.
- Fixed new-provider missing FHitResult include (C2061/C2511 and follow-on C4390). Final build has no remaining source errors. Existing RaceHUDTests.cpp InputKey deprecation warnings appeared during full recompilation; unrelated code untouched.
- Headless automation verifies geometry and non-mutation, not rendered debug visibility or subjective handling. Manual PIE checks above remain outstanding.
- Stage 1 NOT implemented: no rigid-body state, integration, forces, inertia, wheel model, solver or contact persistence.
- No commit, push or merge.
