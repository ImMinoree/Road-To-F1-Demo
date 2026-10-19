# Physics Stage 4 — persistent geometric contact manifolds

## Overview / preservation
Worktree D:/Boring/GitHub/Road-To-F1-Visual; branch visual-overhaul-first-person; HEAD 545ac69cd22517cd5b961152fe014e17be908179; origin https://github.com/ImMinoree/Road-To-F1.git. Unreal was closed at preflight. Existing uncommitted visual and Stage0–3 work preserved. No project copy, asset edits, commit, push or merge. Source-before hashes are retained for preservation verification.

Implemented a manually invoked manifold builder and pair cache. Nothing is attached to live karts or the world Tick. Only the cache's own value data changes. No impulse storage/calculation/application, solver, transform ownership, player/AI/race/camera/HUD/audio/training changes or Stage5 implementation. Legacy remains the only selectable backend.

## Files
Created under RoadToF1/Source/RoadToF1:
- KartPhysicsContactManifold.h: contact/manifold value structs, explicit statuses/methods and threshold settings.
- KartPhysicsManifoldBuilder.h/.cpp: analytic sphere paths, box clipping, EPA witness fallback, local anchors, reduction and validity checks.
- KartPhysicsManifoldCache.h/.cpp: explicit complete-snapshot update, matching, IDs/age, invalidation and diagnostics.
- KartManifoldProbe.cpp: development-only temporary three-refresh demonstration.
- Tests/KartPhysicsStage4Tests.cpp: independent fixtures, invariants, cache lifecycle, deterministic stress and gameplay isolation.

Existing source modified: none. Existing non-source file modified: BUILD_LOG.md. New reports/evidence reside only under TestResults/PhysicsStage4; Stage0–3 evidence remains intact.

## Reference study / Stage3 quality gate
Reviewed CrashCat Refrence/crashcat/src/manifold/clip.ts (Sutherland–Hodgman half-space clipping), manifold/manifold.ts (bounded patch reduction and paired witnesses) and contacts.ts (pair-scoped previous/current cached contacts). Adapted these responsibilities into independent Unreal C++ value types; no source translation, impulse history or solver integration copied. car-physics-demo continues to delegate contact processing to Bullet.

Stage3 has read-only support/GJK/EPA results with explicit convergence flags, stable Stage2 pair IDs and a validated shape map contract. General hulls contain vertices only, not faces. Stage3 coincident spheres can exhaust EPA while safely returning invalid geometry. Stage4 deliberately supplies a separate sphere–sphere analytic path; raw Stage3 GJK/EPA and its tests are UNCHANGED. Arbitrary EPA failure never becomes a valid manifold. General hulls and box edge cases still require valid converged EPA geometry. This is a Stage4 quality-gate addition, not an unreported rewrite of Stage3.

## Contact data / conventions
FKartContactPoint contains world witnesses A/B, local anchors A/B, nonnegative penetration depth in cm, a topological Feature hint, cache-assigned PersistentId and Age (matched update count). No impulse fields exist. FKartContactManifold contains canonical Pair, A-to-B Normal, at most4 points, status, method, original GJK/EPA statuses, bNarrowphaseRun, bAmbiguousNormal, Age and pre-reduction CandidateCount.

WorldA-WorldB approximately equals Normal*Depth. Moving A hypothetically by -Normal*Depth would separate it; no movement is performed. Local anchors use inverse quaternion rotation of World-Position. Shape origin must equal body origin, no scale/COM offset supported. Stage1 pose getters can supply these transforms without body mutation. Touching analytical spheres can return zero-depth contact; generic Stage3 Touching currently returns NoContact because it supplies no reliable penetration normal. This distinction is explicit, not speculative collision response.

For analytical paths bNarrowphaseRun=false: GJK/EPA fields are not a claim that those algorithms ran. Method and manifold status are authoritative. For generic convex paths bNarrowphaseRun=true and original convergence statuses remain available. Invalid pair/shape/settings returns InvalidInput; unresolved geometry returns GeometryFailure. NoContact and Valid are distinct. Unsupported is reserved for future shape extensions; current validated shape kinds have defined paths.

## Sphere contacts
Sphere–sphere uses exact center distance and radius sum. Separated pairs have no contact; overlap depth is rA+rB-distance. For distinct centers the normal is normalized B-A and witnesses are centerA+N*rA and centerB-N*rB. Equal centers use deterministic world +X with bAmbiguousNormal=true; no claim of uniqueness or swap reversal for that symmetric case. The path is independent of EPA iteration budgets. Near-but-distinct centers retain their unique normal.

Sphere–box uses the sphere center in box-local coordinates and clamps it to box extents. Outside: normal follows box closest point to sphere center. Inside/on boundary: select the nearest box face (axis order breaks ties), and depth is sphere radius plus center-to-face distance. Corresponding witnesses lie on the box surface and sphere surface. Shape order reverses normal and swaps witnesses. Centered inside-box cases flag ambiguity; other equidistant face ties use deterministic axis order. These named analytic methods are only for sphere pairs, never general convex bypasses.

## Box faces / edges / hulls
Box–box first requires converged Stage3 geometry. If the normal aligns with a box face (absolute axis dot >=1-1e-6), select the better-aligned reference box; ties prefer A. Select the incident face opposing its normal. Construct its genuine four vertices and clip against the four reference side half-spaces using Sutherland–Hodgman. Retain points behind the reference face plane; project corresponding witnesses onto that face. Local anchors are reconstructed independently for both shapes. Clipped polygons can produce more than4 candidates (the square/45-degree-square fixture produces8).

Non-face box normals use ONE converged EPA witness pair representing an edge contact region. No unsupported multi-point edge patch is invented. Tests verify these witnesses lie on the actual box surfaces. General hull interactions likewise use one valid EPA witness; arbitrary vertex clouds are never assigned invented face topology. General hull clipping is deferred until validated face/edge data exists.

## Deduplication / reduction / feature identity
Candidates are sorted by Feature, then local coordinates. Remove near-duplicates using BOTH witness distances. Always retain a deepest point, then greedily choose points maximally distant from the selected set, up to MaxContacts1–4. Valid previous contacts break coverage ties. This preserves depth and spread without four copies of a single witness. Sorting again gives deterministic public order.

Clipped feature hints encode reference side/face, incident face, incident source-vertex bitset and clipping-plane bitset. Intersections union source provenance; IDs do not use pointer addresses or polygon array indices. Analytic sphere types have stable hints, while EPA witness fallback uses Feature0 (geometric matching only). Hints are not globally unique contact keys and can change when reference/clip topology changes. Cache PersistentId is the actual session identity, allocated monotonically in sorted pair/contact order. New points start Age0; matched points increment, saturating at uint32 maximum. Session reset restarts IDs; they are not cross-session identities.

## Persistence / validity / invalidation
Each explicit Update receives a COMPLETE current candidate snapshot, a const Stage2 tree and a const shape map. Sort/deduplicate IDs; reject self-pairs; re-check current registration, enabled/mobility/masks. Resolve shapes and require support-derived bounds to match registered tight bounds within1e-7cm. Stale/missing data emits InvalidInput and expires prior cache data.

Fresh valid geometry is rebuilt each update. Old local anchors are reconstructed with current poses and tested for normal separation and tangent drift; transported prior normal must remain aligned. Stable feature hints are preferred, otherwise local-anchor proximity matches. Matching is one-to-one and deterministic. Fresh witness coordinates remain authoritative; only identity/age transfers. Old inaccurate coordinates are not retained merely to fill a patch. The old manifold is supplied to reduction only as a tie preference; every output point comes from freshly validated geometry.

Named settings: DedupCm=.001; AnchorDriftCm=.5 for tangent drift/proximity fallback; FeatureMatchCm=1 for same-feature local-anchor matching; SeparationCm=.02 for refreshed old-anchor separation; NormalMatchCos=.95 dimensionless. Settings are validated and configurable within bounded ranges. Invalid settings return false WITHOUT updating cache; callers must handle failure. Fresh separated pairs expire immediately even if old anchors fall within SeparationCm. Thus the separation threshold validates matching, not permission to keep separated manifolds alive.

Removal/retirement, disabling, any filter/mobility change, loss from the full snapshot, separation, invalid shape, missing mapping, changed shape parameters/vertices and unresolved geometry all invalidate old contact history. Optional per-body GeometryRevisions invalidate identical replacements; caller increments revisions for such replacements. Cache stores geometry snapshots for comparison. Stage2 retired IDs cannot be reused in a tree. A cache belongs to one world/session; caller MUST Reset when replacing that world/tree, before IDs from a new allocator are used. Reset drops all results/history. No hidden automatic world lifecycle hook or live scheduler was added.

## Determinism / performance
Hash maps provide lookup/storage, never public processing order. Pair list and contacts are sorted; tie rules explicit. Two independent caches receiving identical sequences must match witnesses, features, persistent IDs and ages exactly on the same build. No cross-platform bitwise guarantee. No time integration or timestep-dependent physics exists; Age counts calls rather than wall time.

Contacts use inline4 storage, clip scratch inline16, no per-contact UObject. Output capacity is reused; cache entries include shape snapshots (hulls may allocate) and the current implementation rebuilds the next entry map each update. Correctness-first: not a production benchmark or allocation-free guarantee. No threading/GPU work. The maximum-ID wrap after2^64 assignments is not a practical supported lifetime; reset the session before identity exhaustion.

## Independent tests / stress
Known box-face geometry verifies four distinct20cm-spanning corner contacts at5cm depth, shallow/deep overlaps, swapped witnesses and transformed anchor round trips. Sphere fixtures verify exact5cm and coincident20cm depth, inside-box sphere depth11cm, near-coincident unique normals and swapped sphere–box orientation. The rotated square clipping fixture proves >4 candidates reduced to4. Random slender boxes exercise edge contacts and verify actual box surfaces independently from the builder's anchor round-trip predicate.

Tests cover geometry revisions, parameter replacement, allowed/rejecting mask changes, disabled/removed/retired IDs, omitted candidates, anchor-only drift, separation, atomic invalid-settings rejection, deduplication, repeated deterministic caches and forced EPA exhaustion (not promoted). Stage1 body pose/motion/force/torque and a live test Pawn transform/incidents are snapshotted; Custom remains rejected.

Stress:200 experimental bodies (100 isolated pairs) across12 explicit updates, mixed static/dynamic boxes and spheres, box rotations, small translations, contact break/reentry and one removal. Two caches compare exact ordered output each step. Check finite anchors, unique sorted pairs, max4, no removed-body references, no tree diagnostic mutation and zero geometry failures. Numeric results are recorded below after final validation. No machine-time pass threshold.

## Debug command / manual Editor checklist
1. Open this worktree project and SouthGarda_KartRace. Start PIE and enter RoadToF1.Physics.ManifoldProbe.
2. Expect cyan/yellow temporary boxes near the player, four distinct colored witness pairs/connecting lines, red normal arrows, local-anchor labels, contact feature/identity and persistence age. The command performs3 small temporary geometry refreshes and draws only the final snapshot for8seconds; it is not an animated scheduler.
3. Expected log: active1, reused4, new0, invalidated0, max_points4, failures0. Final contact ages should be2. Repeating the command creates a fresh temporary cache and repeats the same local IDs/ages; old drawings expire.
4. Drive, V toggle, F5 reset and F9 recording should behave as before. Probe reads player location only to position the drawing; no racer/body writes, incidents or telemetry events.
5. Run RoadToF1.Physics.Stage4Manifolds in Test Automation, then the full RoadToF1 suite. Custom must still fail selection.

Headless log verification is AUTOMATED, not rendered/manual verification. Manual visual appearance and subjective driving remain pending.

## Remaining limitations / Stage5 recommendation
No general hull multi-face clipping, CCD, compound shapes, body-local shape offsets, dynamic world import or live contact scheduler. Box edges/hulls use one EPA witness and inherit numerical limitations; unresolved general intersections cannot be solved by this layer. Analytic sphere paths intentionally resolve sphere geometry without claiming EPA improved. The cache transfers identity and age only, not forces or impulses.

Stage5 recommendation (NOT implemented): an isolated explicit fixed-step sequential-impulse/PGS contact solver over these manifolds and Stage1 mass/inertia. First validate normal convention, Jacobians/effective mass, static/dynamic ownership and convergence on analytical fixtures. Add bounded accumulated normal impulses and a defined tangent basis, then separately validated friction/restitution and actual warm-start impulse transfer. No playable Custom activation until solver, reset/lifecycle, nonconvergence policy and single-response ownership are tested. No silent position correction or competing Unreal response.

## Validation
IMPLEMENTED: standalone builder, analytic sphere paths, box clipping/reduction, single-witness edge/hull fallback, explicit persistent cache and development probe. No pre-existing Source files changed.

AUTOMATED TESTED:
- Full23/23 suites PASS,0 failures/skips/warning suites,150.759s (AutomationFinal/index.json; automation-final.log). Stage0–3 and all gameplay suites pass unchanged.
- Stage4 deterministic stress:200 bodies,12 steps,99 active manifolds at end,1364 reused contacts,636 new,438 invalidated,0 failures, maximum4 points. Two caches match exact ordered outputs.55 random edge fixtures exercised single-point EPA geometry and actual surface checks.
- Focused1 also passed before the final additional invalidation fixtures. Probe headless output: active1/reused4/new0/invalidated0/max_points4/failures0.
- Editor Win64 Development PASS (editor-final-2.log,17.66s). Standalone Win64 Development PASS (game-final.log,26.83s). No new compiler errors/warnings. First cached discovery omitted new files, so source discovery was forced; subsequent build explicitly compiled all new sources. No persistent build-configuration changes.
- git diff --check PASS; new-source no-index whitespace checks PASS (exit1 means new-file differences). Line-ending conversion notices only.
- Before-source SHA256 verification: zero pre-existing source files changed. Existing assets and earlier evidence preserved.

MANUALLY VERIFIED: no rendered/manual test in this phase. Exact Editor checklist above remains pending; headless probe output is not a screenshot or subjective driving test.

Unrelated startup notices: editor-layout compatibility migration and unavailable non-Windows SDKs; Win64 SDK valid. Final automation reports no warning suites.

NOT IMPLEMENTED: Stage5, impulses, friction/restitution, warm starting, live custom scheduler/response, hull face topology/clipping or gameplay changes. Raw Stage3 coincident-sphere EPA limitation remains; Stage4 analytic sphere geometry deliberately addresses it. No commit/push/merge. Await user review.
