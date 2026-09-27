# Northern Marches art and map controls

The North is a single reviewable package: [manifest](../../godot/themes/north.json), scenery, landmark overrides and HUD palette. The [desert package](../../godot/themes/desert.json) retains its existing artwork independently. Grass maps select North automatically.

## Package contents

| Element | Treatment |
| --- | --- |
| Quarry | Mossy limestone hillside, timber crane and green irregular perimeter |
| Mountains | Connected slate ridges spanning up to three adjacent blocked cells |
| Undergrowth | Ferns, brambles and mossy stones with varied placement and size |
| Meadow | Continuous world-space grass shading without repeated hex-shaped bases |
| Trees | Existing pine and oak clusters, mixed with varied offsets and sizes |
| Landmarks | Northern buildings grouped into the manifest |
| HUD | Shared moss-green panels, frames and buttons |

New transparent PNG originals live in `assets/textures/themes/north/`. They were generated using the built-in image-generation tool; exact prompts and generation sources are recorded in [north-prompts.json](north-prompts.json). The launcher stages assets into `godot/content/textures/`. Theme textures receive cached mipmaps at runtime, so zoom quality does not depend on local editor import settings.

Mountain grouping only combines connected, impassable mountain cells without map objects. Rendering changes do not modify terrain, travel costs or routes. Footprints remain irregular rather than tile-shaped. Future biome packages should provide their palette, object overrides, scenery and review map together.

## Map controls

- Wheel or **+ / −**: smooth zoom from 10% to 250%; wheel zoom preserves the world point beneath the pointer.
- **Fit map / Home**: fit the map into the available viewing area.
- **Find hero / Space**: center the expedition in the map area.
- **Hex grid / G**: toggle readable outlines above scenery and beneath fog; both grid switches stay synchronized.
- The expedition sidebar scrolls when its content exceeds the window height.

## Review and verification

Stage assets with `./scripts/run_godot.sh --prepare`, or launch the game normally once. Run:

```sh
.tools/godot/Godot_v4.7.2-stable_linux.x86_64 --headless --path godot --script res://tests/north_theme_smoke.gd
.tools/godot/Godot_v4.7.2-stable_linux.x86_64 --path godot --script res://tests/north_theme_smoke.gd -- --capture /tmp/umm-north-review
```

The package test checks imported alpha, northern/desert separation, connected ridge footprints, unchanged navigation, grid layering and synchronized switches, zoom limits and anchor preservation. The graphical run also compares actual pixels before and after enabling the grid.

Review images: [overview](review/north_overview.jpg), [quarry](review/north_quarry.jpg), [quarry with grid](review/north_quarry_grid.jpg), [ridges](review/north_ridges.jpg).
