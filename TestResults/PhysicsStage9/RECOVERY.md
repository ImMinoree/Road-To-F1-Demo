# Stage 9 recovery — numerical investigation

Worktree: D:/Boring/GitHub/Road-To-F1-Visual. Branch visual-overhaul-first-person; accepted HEAD d953f2aff16f8f05b3b3883a9fadf2d5284ddcbf. No commits, pushes or merges during recovery. Existing Stage 9 work preserved. Legacy remains default, normal Custom admission remains closed.

## Reproduced original failure
RoadToF1.Physics.Stage9SingleIntegration. Body position (0,0,22) cm, yaw 0.25 or 0.65 radians; velocity yaw-rotated (300,0,0) cm/s, angular velocity zero. Mass 160 kg, inertia diagonal (97333.3333,145333.3333,218666.6667) kg*cm^2, wheel speed 25 rad/s. Wall centre (300,0,50), half extents (1,500,50), identity rotation. Fixed dt 1/120 second.

RecoveryDiagnostic/index.json and recovery-diagnostic.log reproduce both failures:
- yaw .25: first rejected step187. Hit.Time=1.3749094, normal (-1,0,0), squared length1. Near-zero translation around (240.690,61.638,22). The hit lies beyond the requested cast interval. It is NOT a valid TOI and must not be clamped to 1.
- yaw .65: first rejected step190. Initial overlap0.000610351562 cm, position (234.988,179.302,22.010), velocity (.155,3.261,-.155), omega(-.011,-.008,-.001). Valid tiny overlap was rejected because no continuation manifold existed.

Neither failure entered the solver. No evidence supports an impulse-sign or inertia bug as the original cause. Normal approach at the latter state is small; exact contact-point relative velocity was not captured in that baseline and is not fabricated here.

Original bridge path: read-only Unreal ground rays -> vehicle force/pose prediction -> translation-only sweep at old orientation -> one transient contact -> custom solver. Stage2 tree, GJK/EPA and persistent geometric cache were not involved in those original failures. A sweep hit at one arbitrary corner also introduced artificial angular response in a head-on impact.

## Recovery changes
- Out-of-interval finite hits are not treated as TOIs. Actual endpoint overlaps are checked independently.
- Integrated rotation retained; exact UBoxComponent geometry reconstructed from component pose/scaled extents. No mesh-AABB substitution.
- Stage4 fresh manifolds resolve initial/rotational overlap with existing bounded velocity bias. All simultaneous static contacts enter one deterministic sorted solver batch. No actor sweep response or Chaos impulse.
- Valid TOI patches reconstructed across the existing0.05cm skin using a0.051cm geometry-only probe; body is never shifted by that probe. Zero-depth conservative contacts across the gap use reconstructed anchors.
- Optional caller-owned counters record contacts, iterations, maximum depth/residual, initial overlap and out-of-interval query hits. No Tick logging.
- Expanded18-case matrix plus read-only actual-map coverage audit.

RecoveryContact1 passed the original suite. RecoveryPatch2 reduced synthetic head-on peak angular speed18.095 ->1.317rad/s and fixed the two-wall continuation case. High-friction rest still failed at step114 with GJK Degenerate/EPA NotRun, prompting an exact OBB SAT fallback in Stage4 (no hull generalization or fabricated EPA success). Subsequent results are recorded separately.

## Authoritative track coverage blocker
RecoveryMatrix1/RecoveryPatch2 load the actual SouthGarda_KartRace map without saving. Audit found0 exact box components and12 query-enabled WorldStatic non-box components. These include road/environment static meshes; authoring script Tools/fix_track_surfaces.py explicitly uses complex-as-simple triangle collision. Rays can sample ground, but the new contact reconstruction cannot resolve these meshes. This is NOT full-track collision coverage.

Stage9 cannot be declared complete, and Stage10/full20-racer Custom admission cannot proceed safely until a validated mesh/convex collision adapter covers required track geometry. Original map assets remain untouched. The audit passing only means incomplete coverage is detected and runtime admission stays closed.
Final2026-10-19: false support, near-touch geometry, rotational TOI, split stabilization and post-impact sliding corrections are validated. RecoveryAcceptanceFinal31/31 passes; builds pass. Numerical blockers in this historical investigation are resolved. Overall Stage9 remains blocked by actual static-mesh coverage and multi-kart integration. See README.md for final numbers and manual test instructions.
