# Road to F1 — visual foundation, milestone 1

## Open either version

- Preserved playable project: `D:/Boring/GitHub/Road-To-F1/RoadToF1/RoadToF1.uproject`
- Experimental project: `D:/Boring/GitHub/Road-To-F1-Visual/RoadToF1/RoadToF1.uproject`
- Engine: Unreal 5.8.3. Open only one project at a time to keep builds and memory use predictable.
- Preservation branch: `prototype-v1-preserved`, snapshot `545ac69cd22517cd5b961152fe014e17be908179`.
- Visual branch: `visual-overhaul-first-person`. No merge or push authorized.

## Controls and tuning

First person is the default. V toggles the retained third-person camera. F5 resets the race and motion state while retaining the chosen view. Existing driving controls, speed caps, HUD and F9 telemetry are unchanged.

`FKartViewSettings` exposes eye position/pitch, low/high FOV, response speed, motion strength and engine/road/kerb vibration. Defaults are 88–98 degrees FOV. Set MotionScale to zero to disable vibration and body response. FOV remains separately configurable; equal low/high values give a fixed FOV. All movement amplitudes and FOV are runtime-bounded.

The wheel uses the existing smoothed physical steering state. Its material pieces are separate, non-colliding meshes. Static player driver geometry is hidden only from its owning first-person camera to prevent helmet/arm clipping. Other drivers and the third-person driver remain visible. Proper rigged hands are intentionally deferred.

## Reproducible, original source assets

- `export_cockpit.mjs` derives a separated wheel and body from the project's own `Art/KartLab/scene.mjs` procedural kart. Run with Node from this directory or repository root.
- `make_surfaces.py` creates deterministic, tileable asphalt and grass color variations with Python/Pillow. These are newly authored textures, not third-party downloads.
- `exports/` contains editable OBJ intermediates and a material manifest.
- `Textures/` contains source PNGs.
- `Tools/build_visual_foundation.py` imports only into `/Game/RoadToF1/Art/VisualV01`, applies map material overrides, and configures lighting/post-process. It records a before/after gameplay geometry fingerprint. It is explicitly restricted to this visual worktree.
- Existing mesh geometry, collision profiles, gate order, route points and grid transforms must remain unchanged.

## Built-in tools inventory

No `.uproject` plugin changes are required for this milestone.

| Tool | Inspected availability | Use now |
|---|---|---|
| Modeling Tools Editor Mode | Installed; explicitly enabled for Editor | Available for later mesh authoring |
| Control Rig | Installed; engine default enabled | Future hands/arms rig |
| Niagara | Installed; engine default enabled | Existing/future effects; no new system |
| Enhanced Input | Installed; engine default enabled and existing module dependency | Preserve existing input bindings |
| Movie Render Queue | Installed; default disabled | Not required for gameplay screenshot captures |
| Geometry Script | Installed; not explicitly enabled | Not required |
| PCG | Installed; engine default enabled | Not used to scatter scenery yet |
| Python / Editor Scripting Utilities | Installed; default disabled | Command-line-only asset authoring, no shipping dependency |

Descriptor defaults are not a claim that every optional plugin is loaded in every runtime. Asset authoring enables Python/editor utilities only on the import command line. Existing Chaos plugin is retained; the custom kart physics is not replaced.

## Art direction and next assets

Original Road to F1 identity: clean silhouettes, blue/white circuit accents, warm daylight, cool shadows, restrained saturation, readable asphalt. This is an art-direction experiment, not a replica of another game's assets.

The current scenery is grouped low-poly placeholder geometry. Retain it in milestone 1. Replace in small reviewed batches, preserving all gameplay collision and track boundaries:

| Asset family | Current limitation | Next production approach |
|---|---|---|
| Trees | Repeated faceted canopy/cone silhouettes | 3–4 original broadleaf/cypress species with layered crowns, LODs and shared palette |
| Bushes | Sparse, little intermediate height | Small clustered shapes, varied scale; keep sightlines open |
| Grass | Flat ground surface | Shared material variation first; restrained instanced tufts later |
| Fencing | Simplified boundary detailing | Modular posts/rails, economical opaque or masked panels |
| Tire barriers | Repeated primitive shapes | Instanced tire stacks with readable rubber shading |
| Kart barriers | Basic blocks | Modular molded barrier kit with consistent blue/white panels |
| Paddock buildings | Simple box masses | Original modular facade, roof and garage doors |
| Tents | Repeated simple shelters | Two lightweight canopy types with fictional team colors |
| Spectators | Placeholder crowd presentation | Low-cost silhouette groups and distant impostors after profiling |
| Banners | Minimal branding | Original Road to F1 and fictional sponsor designs; Canva optional later |
| Lights | Simple poles | One modular pole/head family, shadows budgeted by distance |
| Track signs | Prototype markers | Clear fictional signage, marshal boards and directional cues |
| Distant scenery | Broad low-poly hills | Layered countryside silhouettes and coherent haze/color palette |

Next milestone should be an original cockpit and seated driver hand/arm rig in Blender, with editable `.blend` source here. Test reach, wheel rotation and camera clipping before environment expansion. Do not modify Blender installation/settings. No paid assets or external proprietary game assets are included.

## Performance and review

Camera math runs only for the locally controlled player in first person. The existing ground probes supply kerb identification; no new collision traces are introduced. Opponents update only their wheel transform, with both cameras and spring-arm ticks disabled. Materials use shared assets and simple color/roughness variation. Existing Lumen/virtual shadow configuration is retained; no new rendering plugin or dense scenery is added.

Rendered automated captures verify composition, not subjective motion comfort or a hardware performance target. Human review should drive a full lap, use V at rest and speed, steer both ways, cross a kerb, brake hard, restart in both views, and confirm readable HUD/track visibility. Compare sustained frame rate against the preserved project on the same machine and resolution.
