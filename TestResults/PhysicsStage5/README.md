# Stage 5 — isolated normal-contact solver

Implemented: deterministic pair/contact PGS, world inverse inertia/effective mass, nonnegative accumulated normal impulses, equal/opposite impulses, explicit immovable handling, bounded penetration velocity bias, transactional failure. No gameplay integration or actor dependencies. Added KartPhysicsSolver.h/.cpp and Tests/KartPhysicsStage5Tests.cpp.

Units: kg, cm, seconds. Impulses kg*cm/s. Default24 iterations, configurable1-128. Penetration slop0.02cm, bias fraction0.15, maximum correction speed20cm/s. Bias may inject small bounded separation energy; it never teleports. Bias can be disabled. Geometry failures, stale anchors, missing bodies, duplicate manifolds, near-zero effective mass and excessive impulses reject the complete batch.

Actual isolated caller order: integrate forces into velocity, build manifolds at current poses, solve velocity constraints, integrate corrected velocity into poses. Solver itself changes velocities only. Production integrator and Legacy code unchanged.

Tests: all six RoadToF1.Physics suites passed (Physics1/index.json). Analytical equal/unequal mass inelastic momentum; wall; angular off-center impact; static-static; separation/no attraction; stale geometry atomic rejection; duplicate pairs; 1200-step gravity/resting four-point patch; exact repeat; Legacy selection/nonmutation. Rest max penetration0.100000000cm (initial overlap), max speed1.44cm/s (bounded correction), closing residual rounded0.000000000cm/s.

Editor Development build passed,65.41s. Existing RaceHUDTests InputKey deprecation warnings only. Standalone build pending at report creation; see appended result. No failed tests. No rendered/manual validation. Run Session Frontend Automation filter RoadToF1.Physics.Stage5Solver with Unreal open on this project; expect pass. Normal play remains Legacy and Custom selection still rejects.

Limitations: velocity-bias stabilization rather than split impulse; cold starting only (warm starting optional/deferred); no friction/restitution until Stage6, CCD, joints, gyroscopic torque or live scheduler. Transactional body-map copies prioritize atomicity, not final performance. Same-platform deterministic ordering; cross-platform bitwise identity not promised. Stage4 hull single-contact and unresolved EPA limitations retained.

Final: standalone Win64 Development PASS (51.38s). git diff --check PASS. Stage5 complete; proceeding to6. No live gameplay or manual validation claim.
