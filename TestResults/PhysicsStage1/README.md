# Physics Stage 1 — isolated rigid-body foundation

## Scope and ownership
Worktree: D:/Boring/GitHub/Road-To-F1-Visual. Branch: visual-overhaul-first-person. Starting HEAD: 545ac69cd22517cd5b961152fe014e17be908179. Origin: https://github.com/ImMinoree/Road-To-F1.git. UnrealEditor and UnrealEditor-Cmd were not running at inspection.

The current request's Stage 1 includes state, mass/inertia and direct-call integration (some of these appeared as separate stages in the earlier planning document). It does not include a world scheduler. The types are standalone C++ values, not components and not attached to AKartPawn. No registry, actor transform write, world Tick or gameplay selection change exists. All pre-existing Source files are checked against source-before.json. Stage 0 files/evidence are preserved. No commit/push/merge.

## Units and conventions
Native Unreal-compatible centimetres, seconds and kilograms, using double-precision FVector/FQuat math:
- Position and full box dimensions: cm. Position is the world centre of mass.
- Linear velocity: cm/s; acceleration: cm/s^2.
- Angular velocity: world-axis rad/s; angular acceleration: rad/s^2.
- Mass: kg; inverse mass: 1/kg.
- Force: kg*cm/s^2; 1 newton = 100 force units.
- Torque: kg*cm^2/s^2; 1 newton-metre = 10000 torque units.
- Inertia: kg*cm^2; inverse inertia: 1/(kg*cm^2).
- Orientation maps local principal/body axes to world axes. Torque uses FVector::CrossProduct(r,F); quaternion increments use the same vector-axis convention. No FRotator degree convention is used by integration.

## Created source files
All beneath RoadToF1/Source/RoadToF1:
- KartRigidBodyState.h/.cpp: guarded value type; mass/box tensors, pose/motion, accumulation APIs.
- KartPhysicsIntegrator.h/.cpp: transactional direct-call semi-implicit integration.
- KartRigidBodyProbe.cpp: non-Shipping numeric console demonstration, without actor/world access.
- Tests/KartPhysicsStage1Tests.cpp: RoadToF1.Physics.Stage1RigidBody automation suite.

Existing modified file: BUILD_LOG.md only. No existing source, asset, map, configuration, Stage 0 provider, pawn, subsystem or backend selector modifications. Evidence files are stored only in TestResults/PhysicsStage1.

## State and mass
Private position, normalized quaternion, linear/angular velocity, positive mass, inverse mass, local 3x3 inertia/inverse tensors, force/torque sums and explicit immovable flag. Read access uses getters; mutations validate inputs. World inverse inertia is derived on demand, avoiding a stale orientation cache.

Default: finite origin/identity/zero motion and accumulators, 1kg uniform 1cm cube. This default is a valid mathematical fixture, not kart calibration. ConfigureBox accepts positive finite mass and positive finite full dimensions; reciprocals/inertias must also be finite and positive. Invalid, underflowing or overflowing configurations return false without changing anything. Static representation is explicit SetImmovable(true), retaining positive mass/inertia while zeroing effective inverse mass/inertia, velocities and accumulators. Nonzero velocity assignment is rejected while immovable. Returning to dynamic restores inverse properties; it does not restore previous motion.

## Inertia
Uniform box about its centred COM: Ixx=m*(y^2+z^2)/12, Iyy=m*(x^2+z^2)/12, Izz=m*(x^2+y^2)/12. Local tensor is diagonal in principal axes; this is three independent moments, not a scalar approximation. FKartTensor3 stores a column-major 3x3 map. World inverse is R*Ilocal^-1*R^T and can have off-diagonal elements. Test box m=12kg, size=(2,4,6)cm produces diagonal (52,40,20)kg*cm^2. Rotation by 45 degrees about Z verifies off-diagonal terms.

## Force APIs and rejection policy
AddForce and AddTorque add world-space values without moving the body. AddForceAtPoint adds F and (point-COM) cross F atomically. ClearAccumulators clears both. All fallible APIs return bool; callers must handle false. Invalid values and arithmetic overflow leave the entire prior state intact. Rejected integration preserves queued force/torque for explicit caller handling, not silent consumption. Successful steps clear accumulators, including immovable steps.

## Integrator
Explicit finite 0 < dt <= 0.1 seconds; no clamping, automatic subdivision or hidden gravity.
1. Validate dt and current state; operate on a temporary copy.
2. vNext = v + (F*inverseMass)*dt.
3. positionNext = position + vNext*dt (semi-implicit, not position-first Euler).
4. omegaNext = omega + (Iworld^-1*torque)*dt.
5. Build axis-angle delta quaternion from omegaNext*dt; left-multiply local-to-world orientation because omega is world-space; normalize.
6. Clear accumulators, validate result and commit only on success.
Immovable steps preserve pose and zero motion, clearing accumulators. Gravity must be supplied explicitly by a caller as mass*g. Damping is not implemented.

Angular simplification: gyroscopic omega cross (I*omega) is omitted. This is not a complete Euler rigid-body solver and does not promise correct asymmetric torque-free tumbling/angular-momentum conservation. Zero torque preserves orientation only when angular velocity is also zero; nonzero angular velocity still rotates.

## Determinism and numerical validation
No frame Tick, randomness, wall clock, actor state or mutable global body storage. Two independently advanced bodies receive identical forces/torques and 1/120 steps for 10000 iterations; the test checks exact final field equality on the same build plus finite state and quaternion normalization throughout. This is not a cross-platform bitwise determinism guarantee.

Analytic fixture: m=2kg, F=10N=1000 native force units, dt=.1s -> a=500cm/s^2, v1=50cm/s, x1=5cm. The next unforced step retains v and reaches x=10cm, proving accumulator clearing/no automatic gravity. One-second constant-force runs at 60/120 fixed steps compare to discrete semi-implicit x=250*(1+dt)cm: 254.166666667cm and 252.083333333cm, both v=500cm/s. Continuous x=250cm; the first-order position error is documented rather than hidden.

## Tests
The new suite covers default state, valid/invalid mass and dimensions, reciprocals, box tensors, rotated inverse tensor, force/torque summation, force-at-COM and off-centre torque, no immediate motion, accumulator reset, constant-velocity and constant-force integration, explicit static handling, known torque/rotation direction, zero-omega orientation preservation, nonzero torque-free rotation, long normalized runs, deterministic repeatability, invalid dt/pose/motion/forces and arithmetic-overflow rejection with no partial writes.

Existing Stage0Ownership continues testing Custom rejection, Legacy default, no engine simulation, query hits/misses/initial overlaps and no transform/speed/incident mutation. Full-suite execution covers existing gameplay regressions.

## Debug/manual checks
1. Open this worktree's project; run RoadToF1.Physics.Stage1RigidBody in Test Automation. Expected: success and analytical measurements in test output.
2. In the Output Log execute RoadToF1.Physics.BodyProbe. Expected: valid=1, mass=2kg, Ixx=3333.333333kg*cm^2, finalX=252.083333333cm and finalVx=500cm/s. This command only logs a temporary value calculation; it draws/moves/spawns nothing.
3. Existing Stage0 probe and gameplay remain available. Custom selection still returns false. Drive/F5/V/F9 can be manually checked as before.

## Limitations and exact Stage 2 recommendation
No collisions, contact solver, impulses, friction/restitution, suspension, tires, load transfer, drivetrain or aero. No gravity/damping by default. No body-to-actor ownership or COM offset from a visual origin. Only uniform box mass properties; no arbitrary tensor setter, composite driver model or moving kinematic-body mode. Large finite inputs that overflow arithmetic fail instead of being silently clamped. No packaging/Shipping executable or manual rendered test claim.

Stage 2 recommendation (NOT implemented): a fixed-step accumulator/scheduler for isolated experimental bodies only, with explicit catch-up/backlog policy, previous/current snapshots and render interpolation tests. Feed identical input timelines through 30/60/120Hz frame schedules and injected hitches; verify same simulated fixed steps/results. Do not activate Custom, connect live pawn transforms or add collision response at that stage without separate authorization.

## Validation results
- Editor Win64 Development: PASS (final corrected build 6.40s).
- Standalone RoadToF1 Win64 Development: PASS (118.33s).
- Full automation: 20/20 success, 0 failed, 0 skipped, 148.91s. Stage0Ownership and Stage1RigidBody have zero warnings/errors. One existing PrototypePIE test carries an HTTP generate_204 connectivity timeout warning. Startup also logged editor-layout migration and slow smoke-test warnings; no new body-test warnings.
- BodyProbe executed in Unreal: valid=1, Ixx=3333.333333kg*cm^2, finalX=252.083333333cm, finalVx=500cm/s.
- Initial compilation caught C4668: UE_BUILD_SHIPPING was checked before including CoreMinimal.h in the new probe. Fixed by adding the include; final builds have no errors. Full recompilation also surfaced existing HUD InputKey deprecation warnings; those files were not changed.
- Builds used the previously successful one-off -NoUBA -NoPCH -NoSharedPCH -MaxParallelActions=4 flags. No persistent engine/build configuration change.
- git diff --check and no-index checks of new source files show no whitespace errors; Git prints LF/CRLF conversion notices. No-index exit 1 signifies new-file differences, not a whitespace failure.
- All pre-existing Source files match their before hashes. New source hashes and summary are in VALIDATION.json. Stage0 reports were not overwritten.
- Headless Unreal automation and numeric debug execution verified; no new manual rendered driving test or packaged/Shipping build performed.
- No Stage 2 scheduler, collision response or Custom gameplay activation implemented. No commit, push or merge.
