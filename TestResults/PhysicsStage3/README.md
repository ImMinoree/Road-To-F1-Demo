# Physics Stage 3 — standalone GJK/EPA geometry

## Scope / current state
Implemented convex detection only in D:/Boring/GitHub/Road-To-F1-Visual, branch visual-overhaul-first-person, starting HEAD 545ac69cd22517cd5b961152fe014e17be908179, origin https://github.com/ImMinoree/Road-To-F1.git. No commit/push/merge. Stage0–2 and visual work preserved. No existing Source file changed (source-before.json verification). BUILD_LOG.md is the only existing file edited by this phase. No assets edited.

Legacy remains the only playable backend; Custom remains rejected. No Tick, live registration, scene scan, body integration, impulses, transform writes, solver or Stage4 work. The world subsystem and Unreal geometric provider remain unchanged. The explicit dispatcher can consume candidates from the subsystem's experimental tree or a separate test tree.

## Files created
Under RoadToF1/Source/RoadToF1:
- KartPhysicsConvexShape.h/.cpp: validated value shapes, exact support and support-derived bounds.
- KartPhysicsNarrowphase.h: shared support/simplex/result/settings types and public geometry APIs; includes contact geometry rather than introducing a separate tiny header.
- KartPhysicsGJK.cpp: Minkowski support, closest-feature simplex reduction, bounded seed expansion, GJK and input/settings validation.
- KartPhysicsEPA.cpp: seed validation, oriented faces, closest-face expansion, horizon reconstruction and barycentric witnesses.
- KartPhysicsNarrowphase.cpp: query composition and explicit Stage2 pair dispatcher.
- KartNarrowphaseProbe.cpp: development-only temporary box/sphere visualization.
- Tests/KartPhysicsStage3Tests.cpp: meaningful fixtures, independent geometry oracles, numerical stress and isolation checks.

This report, logs, source hashes and test exports are new evidence under TestResults/PhysicsStage3. Existing Stage0–2 reports untouched.

## Reference study / adaptation
CrashCat Refrence/crashcat/src/collision/support.ts supplies shape-specific extreme points; gjk.ts reduces a closest simplex; penetration.ts expands deficient seeds and dispatches EPA; epa-convex-hull-builder.ts handles oriented faces and barycentric reconstruction. This implementation adapts these algorithmic responsibilities, not their source text or flat packed record layout. Our GJK enumerates up to15 simplex subsets, our EPA uses bounded inline arrays and a simple closest-face scan, and our shape support includes the actual sphere radius directly. No radius-shrunk core, speculative contact or persistent hull builder was imported. The car-physics-demo reference delegates detection to Bullet and does not provide the standalone narrowphase used here.

## Shape / support contract
Shapes are plain values: Box, Sphere, or the convex closure of up to128 local hull vertices. Position and normalized quaternion map local geometry to world; no scale/shear or mesh import. Box half-extents and sphere radius must be in [0.0001,100000]cm, positions within +/-100000000cm. Hull vertices are finite and bounded to +/-100000cm; a longest-baseline/maximum-area/plane-distance rank test rejects empty, duplicate-only, collinear and coplanar arrangements. Hull vertex ordering need not describe faces and interior vertices are harmless: support searches every supplied vertex, so the geometry is their convex hull. Very poorly conditioned geometry can be rejected by rank thresholds. Zero radius/zero box extent is unsupported rather than pretending it is a 3D solid.

Box support rotates direction to local space, chooses signed extents, then rotates back. Sphere uses normalized direction times radius. Hull uses maximum dot product, retaining first vertex on equal ties. Zero direction deterministically uses +X. Input validation wraps a documented internal prevalidated SupportOffset hot path. Bounds use six axial support queries, not an AABB approximation to shape support.

Minkowski support preserves PointOnA, PointOnB, Minkowski and query direction. It computes (positionA-positionB)+(offsetA-offsetB) to reduce large-world cancellation. EPA checks that each seed point matches a fresh support query against the supplied shape snapshot. Transforms may be copied from const Stage1 getters; no body mutation path exists.

## GJK
At most64 iterations by default (configurable0–128). A support plane strictly behind the origin certifies separation. Closest point to origin is computed over all nonempty simplex subsets: point, projected segment, triangle Gram solve, tetrahedral barycentrics. Only convex projections are accepted, with degeneracies falling back to lower-dimensional features. The winning feature reduces the simplex. Repeated support without a separation certificate reports Degenerate; exhaustion reports IterationLimit, neither silently converted into separation or intersection.

When closest distance is within tolerance, deterministic seed expansion tests center direction, local basis axes, current triangle normals and26 integer grid directions. A support plane at/below tolerance reports Touching. Otherwise a bounded search for a strictly enclosing nondegenerate tetrahedron establishes Intersecting. If it cannot establish one, returns Degenerate. There is no unbounded random perturbation. This bounded search is correctness-oriented and can be relatively expensive in difficult shapes.

Touching means boundary proximity within tolerance, including sufficiently shallow positive penetration or gap. Depth stays0, EPA is NotRun and bGeometryValid is false: no unique reliable normal/witness is invented. Consumers must inspect statuses, not just depth.

## EPA / geometry convention
Only a validated enclosing4-point seed can enter EPA. Invalid/repeated/wrong-snapshot seeds report InvalidSeed. Faces orient outward with nonnegative origin distance. Closest face with a valid perpendicular barycentric projection is selected; new support along its normal either converges or expands the polytope. Visible faces are removed, reversed interior edges cancel, and the horizon must form one closed directed loop. Degenerate triangles/horizons, duplicate nonconverged support and negative advancement fail explicitly. Memory is bounded to520 support vertices/1040 faces; default256 iterations, hard configurable maximum512. CapacityLimit and IterationLimit are distinct. Geometry remains invalid on failure, even when GJK already established intersection.

On convergence Normal is outward from A-B, conventionally A toward B; PointOnA-PointOnB approximately equals Normal*Depth. A hypothetical displacement of A by -Normal*Depth would separate it; no displacement occurs. Depth is nonnegative cm and estimates minimum translational penetration. In containment/symmetric cases “A toward B” means this separating-feature convention, not necessarily the center-to-center vector. Non-unique normals use deterministic iteration/tie ordering, not a claim of unique physical direction.

Witnesses barycentrically interpolate original A/B support points at the closest face projection. Face-face regions yield one representative pair, not a manifold. Smooth sphere witnesses lie on an inscribed approximation and may sit slightly inside the true surface. GeometryValid is required before consuming normal/depth/witnesses; GJK status alone is insufficient.

## Numerical policy
GJK linear tolerance =1e-7cm +1e-9*max(shape scale). EPA advancement tolerance =1e-7cm +1e-5*max(shape scale). Scale is box extent length, sphere radius or maximum hull local radius, independent of world translation. Settings validate finite positive bounded values; zero iteration budgets intentionally test exhaustion. Named conditioning guards in KartNarrowphaseNumerics document direction squared floor1e-28, triangle Gram relative floor1e-14, tetra determinant relative floor1e-12, strict seed weight floor1e-10, barycentric slack1e-8 and world witness roundoff1e-7cm. Visible-face tolerance is0.1*GJK linear tolerance. Hull rank uses max(1e-7cm,scale*1e-9). These are detection tolerances, not physical collision margins.

## Stage2 dispatcher
Dispatch takes a const broadphase, candidate ID array and const ID-to-shape map. It sorts/deduplicates canonical pairs, skips self/invalid/removed/disabled/mask-rejected/static-static pairs, and resolves both shapes. Missing/invalid shapes or bounds differing from registered tight bounds by more than1e-7cm emit InvalidInput for that pair. This rejects stale mappings instead of silently testing unrelated geometry. Caller must maintain synchronized experimental proxy/shape snapshots and a world-local ID source. Results include canonical IDs and are sorted deterministically. Tree, shapes and bodies are not modified; no persistent contact state exists.

## Tests / independent references
RoadToF1.Physics.Stage3Narrowphase checks support mappings/transforms, Minkowski correspondence, invalid shape data, separation/touching/overlap, deep/tiny/coincident cases, nearly parallel faces, explicit iteration limits, repeated seed vertices, wrong-snapshot seeds, finite/unit geometry, swapped normals/depth, witnesses and repeat determinism. Degenerate hull arrangements are rejected explicitly.

Analytical box intervals and exact sphere geometry verify known depths; sphere-box fixtures use closest-face distances. Test-only15-axis OBB SAT verifies independently rotated overlap and depth; it is not used in production. Fixed seed381028:180 oriented-box/box-derived-hull cases (30 vertex hulls), including small scales and large translations;120 additional sphere/sphere-box cases, including shallow overlaps. Logs report intersections, failures, iteration counts and maximum errors. These do not substitute arbitrary human driving for a physics test.

Non-mutation checks snapshot Stage1 pose/motion/forces/torque/mass, proxy bounds/tree diagnostics, and a live test Pawn transform/incidents. Dispatcher tests cover sorting, duplicates, stale/missing shape data, removal and disabled proxies. Existing suites retain ownership/backend rejection and gameplay coverage.

## Debug tooling / exact manual checks
1. Open this worktree's RoadToF1.uproject and SouthGarda_KartRace, then PIE. Run RoadToF1.Physics.NarrowphaseProbe in the console.
2. For8seconds expect cyan box, yellow sphere, silver AABBs, green witness points and red normal. The fixed fixture overlaps by20cm; expected GJK=3(Intersecting), EPA=1(Converged), valid=1, depth near20. The magenta tetrahedron ABOVE the fixture is translated Minkowski space, not world contact geometry.
3. Repeat: identical statuses/depth; live racers continue normally. The probe reads player location solely for drawing placement, does not register/spawn actors or trigger incidents. Old drawings expire after8seconds.
4. Run RoadToF1.Physics.Stage3Narrowphase in Test Automation. Expected pass. Custom selection must remain false/Legacy.
5. Drive, toggle V, use F5 and F9 as before. These manual visual/subjective checks are NOT claimed as performed. Automated headless execution verifies command output, not rendered appearance.

## Performance / limitations
No UObject per shape/query. Simplex fixed4; seed and EPA scratch use bounded inline arrays on the stack, no global mutable scratch. Dispatcher copies/sorts candidates and uses caller-owned output capacity; hull storage allocates at construction. EPA linearly scans faces and validates seed supports; seed expansion may enumerate many tetrahedra. No timing requirement or production performance claim; benchmark before live use. Same-build deterministic results only, not cross-platform bitwise guarantees.

Coincident spheres currently hit default EPA iteration limit while uniformly refining a symmetric polytope; GJK establishes intersection but penetration geometry is unavailable. Other untested poorly conditioned or extreme aspect-ratio cases may also return explicit failure. No CCD, distance query guarantee for separated shapes, triangle meshes, compound shapes, world-static import, multiple contact points, clipping or contact persistence. No live custom backend, physics response, friction/restitution, suspension/tires or integration scheduler. EPA outputs must not be used as a production solver without later validation.

## Exact Stage4 recommendation (NOT implemented)
Add an isolated contact-manifold layer: shape feature identification, reference/incident face clipping for polyhedra, analytic sphere feature handling, bounded multi-point reduction, persistent pair/feature keys, cache refresh/invalidation and deterministic geometric tests. First improve seed/curved-symmetry convergence or define a validated fallback policy; do not persist unconverged geometry. Continue explicit dormant tests. No impulses, warm-start impulses, solver or playable Custom activation until separately authorized.

## Validation
IMPLEMENTED: standalone convex support, GJK/EPA, explicit filtered dispatcher and development probe as described above. No existing source files modified.

AUTOMATED TESTED:
- Full22/22 suites PASS, zero failures/skips/warning suites,159.633s. AutomationFinal/index.json and automation-final.log. Includes all earlier physics and gameplay suites.
-180 SAT cases:119 intersections,0 failures,0 nonfinite;656 GJK iterations,795 EPA iterations in aggregate; maximum independent depth error8.8817842e-15cm. Includes30 equivalent vertex-hull representations.
-120 curved cases:92 intersections,0 failures; maximum analytical depth error4.99825764e-05cm. Shallow, small-scale and oblique cases included. Coincident spheres separately report EPA IterationLimit(4), not a valid contact, as documented.
- Headless NarrowphaseProbe: GJK3/EPA1, depth20.00000cm, valid1. No rendered inspection claimed.
- Editor Win64 Development PASS (editor-final.log,18.12s); standalone Win64 Development PASS (game-final.log,21.53s). Final source differs from those builds only by removal of redundant EOF blank lines in two new files; no executable code changed after validation.
- git diff --check PASS. New-source no-index whitespace checks PASS; exit1 indicates new-file differences, line-ending conversion notices only. Before-source hashes confirm all pre-existing files byte-identical.

MANUALLY VERIFIED: none in this phase; follow the Editor checklist above.

Build/test issues retained transparently: initial cached source discovery skipped new files, so forced -gather. User opened Unreal after initial preflight; Live Coding blocked the build, and compilation resumed only after the user saved/closed it. Fixed new probe missing Engine/World.h (C2664). First focused test crashed on fixture Pairs.Add(Pairs[0]); fixed by copying the pair before append. Focused2 and full final suite pass. Existing RaceHUDTests InputKey deprecation warnings were not changed; startup non-Windows SDK notices do not invalidate Win64 build. No new outstanding compiler errors.

NOT IMPLEMENTED: Stage4, response/impulses, manifold persistence, live custom simulation or gameplay changes. No commit/push/merge. Awaiting user review.
