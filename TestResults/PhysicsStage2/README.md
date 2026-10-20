# Physics Stage 2 — dormant custom broadphase

## Scope and architecture
This request explicitly defines Stage 2 as broadphase/pair generation, superseding the earlier suggested scheduler stage. No scheduler or Stage 3 narrowphase was implemented.

Worktree: D:/Boring/GitHub/Road-To-F1-Visual; branch visual-overhaul-first-person; starting HEAD 545ac69cd22517cd5b961152fe014e17be908179; origin https://github.com/ImMinoree/Road-To-F1.git. No UnrealEditor/UnrealEditor-Cmd process was running before editing. Existing uncommitted work is preserved. No commit/push/merge.

Chosen architecture: flat-node dynamic AABB tree with parent/child indices, a node free list, greedy surface-area-increase insertion, upward bounds/height refit, removal and reinsertion when a tight box escapes its fat leaf. No per-proxy UObject. Query stacks and result scratch reuse array capacity. Only exact current AABB overlap is emitted after tree pruning; this is still conservative relative to actual oriented shapes, NOT exact shape collision.

CrashCat's Refrence/crashcat/src/broadphase/dbvt.ts was studied for flat storage, fat leaves, filtering and tree traversal. Its deferred rebuild strategy is not copied: this initial tree uses simpler explicit reinsertion/refit. car-physics-demo/src/game_world.cpp delegates broadphase to Bullet btAxisSweep3. Existing Unreal geometric queries remain separate and unchanged. No source was translated line-for-line.

## Files
Created under RoadToF1/Source/RoadToF1:
- KartPhysicsBodyId.h: session-scoped IDs and monotonic source.
- KartPhysicsAABB.h/.cpp: validated cm-space bounds and oriented-body conversion.
- KartPhysicsProxy.h: value metadata, mobility, categories/masks, enabled flag.
- KartPhysicsPair.h: normalized comparable/hashable pair IDs.
- KartPhysicsBroadphase.h/.cpp: tree, lifecycle/query/filter APIs, diagnostics and explicit invariant checking.
- KartBroadphaseProbe.cpp: non-Shipping temporary fixture visualization.
- Tests/KartPhysicsStage2Tests.cpp: RoadToF1.Physics.Stage2Broadphase including brute-force/stress/isolation checks.
Modified existing source: KartPhysicsWorldSubsystem.h only, to own dormant tree/ID source and expose explicitly invoked APIs; ownership comments updated. Existing provider, subsystem implementation, Pawn, backend gate, Stage1 and gameplay source untouched. BUILD_LOG.md updated. This report and evidence live in TestResults/PhysicsStage2; Stage0/Stage1 evidence preserved.

## ID lifetime
FKartPhysicsBodyId is a uint64 value, zero invalid; no pointer order or node-slot IDs. Use one FKartPhysicsBodyIdSource per experimental world/session; it allocates increasing IDs and fails with invalid on exhaustion. The subsystem owns its source. Tests can supply predetermined IDs to reproduce a scene in independent trees. Caller must not import IDs from another world/source into the same registry. Duplicate active IDs are rejected. Removed IDs are retired for that tree's lifetime and cannot identify a replacement proxy. Free node indices may be reused without reusing body IDs. Retired-ID bookkeeping grows with lifetime churn; ID sources must not be reset/copied to create competing allocators within one session.

## AABB and oriented bodies
Min/Max in centimetres, inclusive overlap/containment. Touching and point/zero-size boxes are valid. Inverted, nonfinite, or out-of-domain bounds are rejected. Supported coordinates are +/-1e9cm to bound surface-area arithmetic; expansion must remain inside that domain. Invalid mutation leaves existing tree unchanged. Invalid QueryAABB clears output and returns false. Combine returns an invalid box if an input is invalid.

FromOrientedBox computes world extent = abs(Rx)*ex + abs(Ry)*ey + abs(Rz)*ez, then position +/- extent. Quaternion must be finite and normalized; half-extents finite and nonnegative. FromBody reads Stage1 pose by const reference, requires valid state and writes only the output AABB. Dimensions are supplied separately; no modification to Stage1 body shape/mass state. Tests compare arbitrary rotation against an independent eight-corner min/max calculation.

## Fat bounds and updates
Default margin: 5cm each side. SetFatMargin accepts finite 0..100cm only while empty. No velocity prediction or swept bounds; this is discrete broadphase, not CCD. Small moves update tight bounds without reinserting while the existing fat leaf contains them. Escapes replace the fat box with tight+margin and reinsert; ancestors are refit, not allowed to grow forever. Tight boxes shrinking inside a retained fat leaf can leave conservative traversal overhead until escape. Reinsertion count is exposed.

## Mobility, filtering and pairs
Dynamic-dynamic and dynamic-static are allowed; static-static excluded. Both enabled flags must be true. Category/mask agreement is bilateral: (A.Category & B.Mask)!=0 AND (B.Category & A.Mask)!=0. Named bits Kart, TrackStatic, Barrier and Prop are conveniences; arbitrary uint32 categories/masks remain possible. SetFilter changes metadata without touching body state; disabled leaves remain stored but are omitted from queries and pairs.

Pairs are canonical minID/maxID. Only the smaller-ID body emits the pair, each tree leaf is visited once per query, and output is sorted lexicographically. Thus no self/duplicate pairs and no hash/pointer iteration ordering. Sorted IDs drive traversal requests; the hash map is used for lookup only. Fat traversal plus tight leaf filtering ensures exact agreement with filtered brute-force AABB pairs independent of fat history/insertion order. This does not claim collision between actual convex shapes.

## Lifecycle, query and diagnostics
RegisterProxy validates input and rejects duplicates/retired handles. RemoveProxy detaches/frees nodes and removes lookup/sorted ID entries; invalid/double removal returns false. UpdateProxy replaces tight bounds and conditionally reinserts. Bounds/filter rejection has no partial write. GetProxy/GetFatBounds copy values out, so callers cannot mutate stored nodes directly.

QueryAABB returns sorted enabled IDs intersecting current tight bounds by default; optional bFatLeaves returns conservative fat-leaf intersections for inspection. It does not apply pair mobility rules because a region query has no second body. GeneratePairs applies bilateral filters. Outputs reset each call; no stale pair cache is retained.

GetDiagnostics reports registered proxies, active nodes, height, last generated pair count, cumulative reinserts and last pair traversal visits. LastPairCount resets on mutation; last traversal visits describe the previous generation until regenerated. GetDebugNodes exposes copied bounds/IDs. Validate checks parent links, reachability, free-list ownership, current leaf containment, map/ID correspondence, heights and exact internal unions. It is an explicit test/development check, not automatic frame work. Queries reuse member scratch and therefore require single-threaded/external serialization.

## Subsystem and live ownership
UKartPhysicsWorldSubsystem owns an initially empty experimental tree and ID source. Nothing populates or updates it automatically. There is no Tick, actor/body registry, actor transform write, AI hook, response or connection to the Stage1 integrator. Stage0 query provider is unchanged and remains separately accessible. Legacy remains the sole executable backend; TrySelectPhysicsBackend(Custom) remains false. Experimental test/debug proxies are not imported from the real racing field or track.

## Tests and oracle
RoadToF1.Physics.Stage2Broadphase checks empty/single/separated/touching/overlap cases, canonical pairs/hashes, static/dynamic and bilateral masks, disabled/self/duplicate exclusion, register/remove/double-remove/retired IDs, move into/out of overlap, tiny movement/reinsert counts, rotating bounds, query output/reset and invalid/NaN/Inf bounds. It also verifies dormant world storage and no live pawn transform/incident mutation.

Deterministic seed 240217 generates 500 proxies. Two trees insert identical IDs in opposite orders, then receive 12 rounds of 500 movements (6000 updates each), plus four remove/replace cycles with fresh IDs. All 13 snapshots compare BOTH trees to an independently written O(n^2) scalar AABB/filter oracle (26 exact set/order checks). Repeated pair generation and strict sorted/non-self uniqueness are checked at every snapshot; both trees' structural invariants are validated. Timing is diagnostic only, not a pass threshold. No nondeterministic random source.

## Debug tooling / manual Editor checks
1. Open this worktree project and SouthGarda_KartRace; PIE.
2. Enter RoadToF1.Physics.BroadphaseProbe in console. For eight seconds: cyan dynamic tight boxes, blue static tight box, yellow fat boxes, silver internal bounds, white TEST proxy IDs and green candidate connections. A temporary three-box fixture should log 3 proxies, 5 nodes, 2 candidate pairs and valid=1. It reads the player's location only to place the overlay; it never registers/moves the player or any actors, emits incidents, or changes the subsystem's experimental registry.
3. Repeat the command: same local fixture IDs and pair count; previous drawing may overlap until its eight-second lifetime ends. These fixture IDs are local to its temporary tree, not the subsystem's IDs.
4. Test Automation: run RoadToF1.Physics.Stage2Broadphase, then existing Stage0Ownership and Stage1RigidBody. Expected success.
5. Drive and use V/F5/F9 as before; Custom remains unavailable. Rendering/subjective play remains a manual check unless separately recorded.

## Limitations and Stage 3 recommendation
No balancing rotations/periodic rebuild yet: adversarial insertion can produce a deep tree and worst-case quadratic traversal. Dense all-overlap output inherently has quadratic pair count. Diagnostics expose height/visits; optimize only after profiling. No multithread/reentrant query contract, CCD/velocity prediction, automatic world geometry import, per-body multiple proxies, exact convex tests, contacts, manifolds, impulses or solver. Only registered experimental proxies are covered; Unreal scene queries remain a separate service.

Stage 3 recommendation (NOT implemented): standalone convex support-shape abstraction and GJK distance/overlap tests for known convex shapes, consuming Stage2 candidate IDs. Add EPA penetration only as a separately validated extension if approved, with degeneracy/nonconvergence handling. Keep all results observational; no manifolds/impulses, live Custom activation or replacement of Unreal track detection. The standalone fixed-step scheduler previously suggested remains deferred.

## Validation
- Final full automation: 21/21 success, zero failed/skipped/warning suites, 137.019 seconds. Evidence: AutomationFinal/index.json and automation-final.log. Includes Stage0Ownership, Stage1RigidBody and all existing gameplay suites.
- Stage2 stress PASS: 500 proxies, 13 snapshots, 6000 updates per tree, four removal/replacement cycles, 26 exact independent oracle comparisons; opposite insertion order and repeated generation match. Final first-tree height15, nodes999, reinserts5935, total pair traversal visits196362, final pairs120. No timing-based threshold.
- Probe PASS: proxies3/nodes5/height2/pairs2/visits15/valid1. Headless execution validates output only; drawing appearance and subjective driving still require the manual Editor checklist above.
- Editor Win64 Development PASS (editor-final.log); standalone Win64 Development PASS (game-final.log). Both prior full compiles also passed. Forced -NoUBTMakefiles ensured new cpp files were included; one-off no-PCH/no-UBA flags, no persistent build settings changed.
- Initial automation retained in Automation/ and automation.log: 20 passes, one new analytical quarter-turn assertion failed because PI supplied float precision to a 1e-9 double assertion. Corrected the angle to a double-precision constant, retaining the strict tolerance; arbitrary rotated eight-corner oracle and stress checks already passed. Entire suite rerun successfully.
- No new compiler errors. Initial full Editor build had existing RaceHUDTests InputKey C4996 deprecations; final incremental builds clean. Startup emitted unrelated missing non-Windows SDK and editor-layout compatibility notices; Windows SDK valid and final automation suites report no warnings.
- git diff --check PASS (exit0), line-ending notices only. Existing-source SHA256 comparison shows only KartPhysicsWorldSubsystem.h changed this phase; existing Pawn/gameplay/Stage0/Stage1 code preserved. New source whitespace checks also recorded separately.
- BUILD_LOG updated. Branch/HEAD unchanged; no commit, push or merge. Stage3, narrowphase and collision response NOT implemented; Legacy remains the only playable backend.

