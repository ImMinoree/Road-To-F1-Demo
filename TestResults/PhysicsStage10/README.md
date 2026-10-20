# Stage 10 — experimental Custom physics validation

Stage9 checkpoint: 15e27b48744a075395f75214d8f76732ab036b4c. Stage9 passed 43/43 suites; Stage10 AcceptanceCandidate2 passed 51/51 suites, zero failures/skips. FinalAdmission passes 1/1 with the automation bypass disabled during selection. Final Editor Development builds pass (79.39 s; test update18.31 s), standalone Development62.41 s passes, git diff --check passes. Existing five HUD InputKey deprecation warnings remain; no new compiler errors. Legacy remains the default; Custom requires F10 while stationary.

## What changed
- Real South Garda cooked collision geometry: 11 physical meshes, 35,724 triangles, BVH queries, finite triangle contacts. No complex-mesh bounding-box substitution.
- One 120 Hz Custom world coordinator, coupled kart contacts, transactional whole-batch refinement. Each accepted slice owns response once; rejected trials cannot publish impulses or poses.
- Exact finite-edge witnesses repair a captured degenerate GJK box contact. Forced EPA failure remains rejected.
- Real chassis clearance, safe tangent/separating reverse queries and yaw-only behind-target steering recovery prevent measured road-edge/recovery stalls. Legacy controls and 57/55 km/h propulsion caps remain unchanged.
- Development F10 selects/releases the entire stationary fleet. Permanent speed label identifies LEGACY or CUSTOM. F5 retains the selected backend and resets the race.

## Automated evidence
AcceptanceCandidate2/index.json: all 51 suites pass. Includes all 31 earlier collision/gameplay regressions, progressive 2/5/10/20 fleets, actual road edges/barriers, keyboard/HUD selection, ordered three-lap races, frozen results, cooldown retirement, F5 reset and physical heading recovery.

| Metric | Custom 30 FPS | Custom 60 FPS |
|---|---:|---:|
| NPC finishers (3 laps) | 19/19 | 19/19 |
| Scripted player finishes | yes | yes |
| Mean NPC official finish | 516.56 s | 455.64 s |
| Last NPC finish | 781.87 s | 662.00 s |
| Gameplay contact events including player | 38 | 32 |
| Maximum actual overlap | 0.099959 cm | 0.099628 cm |
| Duplicate pair responses / dropped seconds | 0 / 0 | 0 / 0 |
| Mean physics world step including retries | 0.4973 ms | 0.5183 ms |
| Maximum physics world step including retries | 8.187 ms | 10.408 ms |
| Refined steps | 205 | 140 |

Detailed per-NPC metrics: Runs/Custom30_41A4FE9F400C7F1083398BA3DD6241D4.json and Runs/Custom60_B390B79E4D090E67D7192E8A1C4E58EA.json. These are NullRHI CPU measurements, not whole-game rendering benchmarks. Launch checks passed. Post-finish NPCs coast then retire; F5 restores all 20 bodies and presentation.

## Baseline and failed iterations
Earlier full-race baseline completed but had 15.096 cm actual overlap and was rejected. Refined collision reduced overlap below 1 mm. AcceptanceCandidate1 passed 48 and failed two full races: one off-map force guard and one recovery stall. A physical 180-degree heading fixture reproduces and validates behind-target reacquisition using ordinary controls. Candidate2 passes both races. Failed outputs remain separate; no tests or safety guards were weakened to obtain acceptance.

## Exact human test
1. Open D:\Boring\GitHub\Road-To-F1-Visual\RoadToF1\RoadToF1.uproject and /Game/RoadToF1/SouthGarda_KartRace.
2. Play, click the viewport. Confirm mph / LEGACY initially.
3. Press F5, then F10 before accelerating. Confirm mph / CUSTOM. All 20 karts transfer atomically.
4. W accelerate, S brake/reverse, A/D steer, Space brake; V camera; F9 telemetry. Check braking, kerbs, light contact, reverse, checkpoint/lap progress and cockpit comfort.
5. Finish three laps; verify frozen results and no visible finisher stack. F5 resets all racers.
6. F5 then F10 returns to Legacy; confirm label. Moving backend switches must be rejected.

## Limitations
Human driving/comfort remains untested. AI finish distributions differ at 30/60 FPS; fixed-step physics does not make frame-driven AI decisions identical. Contacts still occur; this is not contact-free racing or finished competitive tuning. Custom braking differs from Legacy (measured full-throttle straight test: approximately 18.82 m versus 8.29 m stop distance). Physics uses the project's experimental tire/support model, not a validated commercial simulator. Static collision snapshots do not discover newly streamed geometry automatically. Shipping/cooked packaging, multiplayer, cross-platform determinism and GPU performance are not certified. Visual art/lighting warnings were outside this physics change.