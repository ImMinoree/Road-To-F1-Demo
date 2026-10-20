# Stage6 — friction and restitution

Implemented: reusable material parameters/presets; geometric-mean friction and maximum restitution pair combination; two coupled tangent rows projected onto a Coulomb disk; static region and dynamic slip; impact bounce target from pre-solve velocity with100cm/s low-speed threshold. Defaults remain frictionless/inelastic unless materials supplied. Solver still isolated, transactional, deterministic and velocity-only. No tire model or live gameplay changes.

Files: KartContactMaterial.h; KartPhysicsSolver.h/.cpp; Tests/KartPhysicsStage6Tests.cpp.

Automated: all7 physics suites PASS (Physics1/index.json). No failed tests. Elastic/inelastic analytical speeds and bounded energy; low-speed bounce suppression; dynamic sliding; subthreshold static load; Coulomb bounds; repeat determinism; two-box resting stack; oblique dissipative impact; material bounds. Sliding v0=200cm/s stopped after35.459594cm. Static100cm/s^2 load drift rounded0.000000cm. Stack600steps max penetration0.1cm, final speed0.000061390cm/s.

Editor Development PASS36.61s after source-discovery refresh. Existing HUD InputKey deprecations only. Standalone result appended on completion. First editor build succeeded but omitted the new test file; it is not counted as test validation. Second compiled it explicitly.

No manual/rendered verification. Unreal Editor Session Frontend: run RoadToF1.Physics.Stage6Materials; expect pass. Normal game remains Legacy.

Limitations: cold-start constraints (optional warm starting deferred); no rolling/spin friction; coefficients are prototypes, not measured surface data; restitution uses pre-solve velocity including forces already applied by caller (threshold suppresses gravity micro-bounce); velocity bias may inject bounded separation energy. No shipping/packaged validation. No live integration. Proceed to Stage7 only after builds pass.

Final standalone Development PASS35.43s. Whitespace check PASS. Stage6 complete.
