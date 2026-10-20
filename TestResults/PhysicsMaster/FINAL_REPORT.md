# Road to F1 — current physics status

Stage9 and Stage10 automated acceptance PASS. Experimental Custom driving is explicitly selectable; Legacy remains default. Final admission verification is tracked in ../PhysicsStage10/README.md. Earlier blocked reports are historical, not current status.

Project: D:\Boring\GitHub\Road-To-F1-Visual\RoadToF1\RoadToF1.uproject
Branch: visual-overhaul-first-person. Original Road-To-F1 worktree preserved unchanged.

Stage9: 43/43 suites, actual South Garda cooked collision adapter (11 physical meshes / 35,724 triangles), shared 120 Hz multi-kart coordinator, actual chassis geometry, physical controls, telemetry, checkpoint/lap/reset integration. Local checkpoint15e27b4.

Stage10: AcceptanceCandidate2 51/51 suites passed. Both 30/60 FPS native races complete three ordered laps for all19 NPCs and scripted player; peak overlap below1mm, zero duplicate response/dropped time, cooldown retirement and F5 restoration pass. Shared adaptive integration resolves the earlier15.096cm overlap; exact finite-edge witnesses repair a captured narrowphase failure; tangent reverse and behind-target heading recovery repair observed stalls. Full metrics, limitations, failed baselines and source details: ../PhysicsStage10/README.md.

## Play the new backend
Open the Visual project and SouthGarda_KartRace. Play and click viewport. Press F5 then F10 while stationary; verify mph / CUSTOM. Normal Play starts mph / LEGACY. F5 resets the selected backend; F5 then F10 returns to Legacy. Moving switches are rejected. W/S/A/D, Space, V and F9 remain available.

The screenshot looked unchanged because normal gameplay still used Legacy. F10 now explicitly activates the integrated force-driven backend; it does not replace the art or silently change defaults.

## Limits
Automated scripted driving is not human playtesting. Braking/handling and cockpit comfort require human evaluation. 30/60 FPS race results differ, contacts remain, and competitive AI tuning is unfinished. NullRHI profiling is CPU-only. No Shipping/cooked release, streaming-geometry discovery, multiplayer or cross-platform determinism certification. See the Stage10 report before extending this work.