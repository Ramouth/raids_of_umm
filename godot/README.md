# Raids of Umm'Natur — Godot visual slice

The first migration milestone is a running Godot 4.7 project using the existing
desert map and sprites. The SDL/OpenGL game remains the gameplay reference.

## Run

From the repository root:

```sh
./scripts/run_godot.sh
```

The launcher finds `godot`, `godot4`, or the local, checksum-verified Godot 4.7.2
binary in `.tools/godot/`. On another machine, install the standard build from
[Godot's download page](https://godotengine.org/download/) or set `GODOT_BIN` to
the executable. The .NET build is not required.

To open the visual editor:

```sh
./scripts/run_godot.sh --editor
```

Alternatively, run `./scripts/run_godot.sh --prepare`, then import
`godot/project.godot` through Godot's project manager. Open `scenes/desert.tscn`
and press F6 to run the scene (F5 runs the project).

## What is working

- Imports all 169 cells and 11 landmarks from `data/maps/default.json`.
- Continuous sand, connected oasis shoreline and animated water, dune texture
  shared across neighboring cells, roads, and existing terrain/building sprites.
- An animated hero starting at Khemret, with foot-based Y sorting against buildings.
- Hex picking, weighted route previews, click-to-travel, and landmark descriptions.
- Pan, zoom, overview, hero focus, and an optional hex overlay.
- A native Godot UI and a map preview visible in the Godot editor.

Controls: left click to travel; wheel to zoom; right/middle drag or WASD to pan;
G toggles the grid; Space finds the hero; Home shows the entire map; Escape clears
the preview. Orders finish before another order is accepted. Close the window to exit.

Movement is free exploration in this visual slice: displayed costs describe the
route, but do not consume a turn budget. Visiting landmarks displays a description;
it does not trigger combat, recruitment, income, or loot.

## Working on visuals

Select `Map` in `desert.tscn`. The Inspector exposes the source map, sand color,
grain opacity, dune opacity, and landmark labels. Click **Rebuild map preview**
after changing them. Save the scene to retain those choices. The map's generated
children are a preview; make persistent changes through the source JSON or script.
The camera, HUD layout, reusable hero scene, and shaders are ordinary Godot assets.

The launcher stages copies of shared textures and the map under `godot/content/`.
That folder, `.godot/`, screenshots, and the local engine are ignored by Git. Edit
original assets under `assets/textures/` and `data/maps/default.json`, then rerun
the launcher. No source sprite is rewritten or recolored by the migration.

## Check

```sh
./scripts/run_godot.sh --headless --script res://tests/smoke.gd
./scripts/run_godot.sh --audio-driver Dummy --script res://tests/smoke.gd -- --capture
```

The second command needs a graphical display and writes `artifacts/overview.png`
and `artifacts/detail.png`. The smoke test checks map import, axial coordinates,
weighted road routing, blocked/unreachable destinations, actual mouse dispatch,
animated travel, and UI controls. Failures produce a nonzero exit status, including
a timeout if a script error prevents completion.

## Deliberate limits

This is the visual proof, not the completed game migration. Combat, economy,
recruitment, equipment, turn budgets, fog of war, saves, and the world builder have
not been connected. Existing C++ saves are not read or written.

The sawmill, quarry, and obsidian vent have named diamond markers because the
source art is missing. Other legacy sprites still vary in palette and scale.
The terrain shader blends their existing backgrounds; it does not replace the
need for a consistent final tileset. The current scene targets the canonical
desert map; other biomes, holes in maps, terrain variants, and rotations are not
fully represented. The ground uses the convex outline of the map, while movement
always respects the actual imported cells.

Nearest filtering preserves hard sprite edges. Overview and intermediate zoom
levels downsample the world and are not pixel-perfect integer scaling. Assess
the original pixels at the 1× zoom step. A fixed-resolution world viewport is a
later polish decision; the interface currently scales independently as canvas UI.

The project runs from source. Export presets/templates and inclusion of JSON data
in packaged builds are still to be configured when distribution becomes a milestone.

See [the migration plan](../docs/godot-migration.md) for the gameplay integration path.
