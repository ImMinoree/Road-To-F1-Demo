# Stage8 — isolated physical cockpit feedback

Implemented pure FKartPhysicsViewFeedback value model, FKartPhysicsViewInput and comfort settings. Inputs: local acceleration (gravity removed by producer), angular velocity, mass-normalized incremental impact impulse, speed, engine/road/kerb activity. Outputs: bounded translation/rotation/FOV. Analytic exponential response/decay and phase vibration; no per-frame random noise. No actor, physics-body, existing camera or asset changes.

Files KartPhysicsViewFeedback.h/.cpp; Tests/KartPhysicsStage8Tests.cpp. Defaults maximum1.5cm offset/2degrees, amount0.65; input saturation and finite validation. Impact must be supplied once per event, not repeatedly while contact persists. Reset is required on race/camera switch. Dynamic varying input remains sampled, so exact cross-frame identity only applies to equivalent constant signals/impulse timelines. Comfort is unverified by humans.

Tests cover neutral signal, bounded acceleration/large inputs, small-vs-large impact, monotone decay,30/60/120Hz constant-signal equivalence, reset, existing camera toggle and absence of gameplay mutation. Full project/physics regression is running as pre-integration baseline. Editor Development PASS53.62s; standalone PASS76.85s. Existing HUD InputKey warnings unchanged. No manual or rendered validation yet.

No motion blur added. Effects are experimental output only, not bound to Legacy cameras. Existing first/third-person/V/wheel visuals remain intact. In Editor run Session Frontend filter RoadToF1.Physics.Stage8View. Normal play continues to show its existing camera behavior.

Final: full27/27 project suites PASS,0 failed/skipped,159.33s. Includes all9 physics suites. Both Development builds passed. No failed Stage8 tests. Stage8 complete; this is the pre-Stage9 baseline.
