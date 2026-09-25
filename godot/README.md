# Raids of Umm'Natur — Godot playable slice

Godot 4.7 presents the existing desert map and sprites, with dungeon battles
powered by the original C++ CombatEngine and CombatAI through GDExtension.
The SDL/OpenGL game remains available alongside this migration.

## Run

From the repository root:

```sh
./scripts/run_godot.sh
```

The launcher finds `godot`, `godot4`, or the local, checksum-verified Godot 4.7.2
binary in `.tools/godot/`. On another machine, install the standard build from
[Godot's download page](https://godotengine.org/download/) or set `GODOT_BIN` to
the executable. The .NET build is not required.

The native bridge currently targets **Linux x86-64**. Install a C++17 compiler,
CMake 3.22+, and Python 3 (for binding generation). The first launch downloads
checksum-pinned godot-cpp and nlohmann/json sources and compiles them; subsequent
launches rebuild only changed code. `UMM_BUILD_JOBS=4` controls build parallelism.
Neither SDL nor OpenGL development libraries are needed for the bridge.

To open the visual editor:

```sh
./scripts/run_godot.sh --editor
```

Alternatively, run `./scripts/run_godot.sh --prepare`, then import
`godot/project.godot` through Godot's project manager. Open `scenes/desert.tscn`
and press F6 to run the scene (F5 runs the project).

## First chapter: the search for Aldren

The northern demo begins by clearing the road for Ushari, then follows Aldren's
trail through Corvin's country. Finding the untouched tunnel opens a permanent
choice: return to your father, or enter with Ushari. Each route ends in a short
night scene. On the home route she enters alone; her progression is retained for
a later reunion. The chapter outcome is saved to `user://chapter_one.json`.
Later chapters and the reunion are not yet playable.

Story source: `scripts/north_story.py`; regenerate with `python3 scripts/north_story.py`.
Map source: `scripts/gen_north_map.py`; hand-tuned playable map: `data/maps/old_passage.json`.
The vale now has a quarry, northern routes have stone and obsidian supplies, and
wood/stone resupply replaces redundant gold piles. Starting resources and prices
are unchanged.

Run the native story/save regressions with `./scripts/build_godot_combat.sh --test`
and both ending flows with `./scripts/run_godot.sh --headless --script res://tests/demo_smoke.gd`.

## Home and fieldworks

Varenhold opens on a Great Hall view with a dedicated castle illustration,
quest-aware household news, and an estate ledger. Recruitment, construction,
and trading remain available through the adjacent tabs.

The commander screen presents three progression paths. Siegemaster unlocks
reusable fieldworks: level 2 grants one wagon slot and barricades (250 gold,
3 wood); level 4 adds a slot and stakes (150 gold, 2 wood); level 5 adds a third
slot. Purchases and stock survive saves. Barricades block movement and arrows;
stakes block movement only. Place them in the highlighted deployment area,
right-click to recover them, then press Enter to begin. Construction during
combat is not implemented. Placement cannot seal off the battlefield.

The intro uses static paintings with fades, and cancels interrupted transitions
to prevent shaking or duplicate title cards when skipping quickly.

Run `res://tests/fieldworks_smoke.gd` for shop, deployment, combat, and intro
checks. Add `-- --capture` with a display to save screenshots to `godot/artifacts`.

## What is working

- Imports all 169 cells and 11 landmarks from `data/maps/default.json`.
- Continuous sand, connected oasis shoreline and animated water, dune texture
  shared across neighboring cells, roads, and existing terrain/building sprites.
- An animated hero starting at Khemret, with foot-based Y sorting against buildings.
- Hex picking, weighted route previews, click-to-travel, and landmark descriptions.
- Pan, zoom, overview, hero focus, and an optional hex overlay.
- A native Godot UI and a map preview visible in the Godot editor.
- Dungeon entry, an 11×5 battlefield, native initiative and enemy AI, legal-move
  and attack highlights, stack inspection, defend, retreat, and auto-battle.
- Ordered move, attack/projectile, damage, retaliation, defend, and death effects.
- Return to the same map with surviving stack counts; victory awards one Scarab
  Amulet and clears that dungeon for the current expedition.

Controls: left click to travel; wheel to zoom; right/middle drag or WASD to pan;
G toggles the grid; Space finds the hero; Home shows the entire map; Escape clears
the preview. Orders finish before another order is accepted. Close the window to exit.

Movement is free exploration in this visual slice: displayed costs describe the
route, but do not consume a turn budget. Walk onto a dungeon and click **Enter
dungeon** in the sidebar. Your starting army is 10 desert archers and 3 mummies;
guards are 12 skeleton warriors and 4 sand scorpions, matching the original game.
The encounter is configured in `content_source/dungeon_encounter.json`.

In battle, green hexes are legal moves and red hexes are legal attacks. Click a
stack to inspect it; only the active stack receives orders. Moving ends its turn.
Press D or click Defend to improve defense for the turn, or enable Auto-battle.
Orders are locked until all events from the previous action have animated. Retreat
requires confirmation and preserves surviving counts, but gives no reward. Return
from the result screen to resume exploration. After defeat, **New expedition**
explicitly resets your army, loot, and dungeon progress.

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
./scripts/build_godot_combat.sh --test
./scripts/run_godot.sh --headless --script res://tests/combat_smoke.gd
./scripts/run_godot.sh --audio-driver Dummy --script res://tests/smoke.gd -- --capture
./scripts/run_godot.sh --audio-driver Dummy --script res://tests/combat_smoke.gd -- --capture
```

The capture commands need a graphical display and write `artifacts/overview.png`
and `artifacts/detail.png`. The smoke test checks map import, axial coordinates,
weighted road routing, blocked/unreachable destinations, actual mouse dispatch,
animated travel, and UI controls. Failures produce a nonzero exit status, including
a timeout if a script error prevents completion.

The native tests cover bridge validation and presentation acknowledgements, plus
the existing combat regression suite. The combat integration test loads the real
extension in Godot, walks to a dungeon, clicks entry and attack controls, tests
defend and confirmed retreat, and runs victory/defeat through survivor and reward
handling. Outcome fixtures use deliberately unequal armies, without altering the
production combat RNG. Combat captures are `artifacts/combat.png` and
`artifacts/combat_victory.png`.

## Deliberate limits

This is a playable combat slice, not the completed game migration. Economy,
recruitment, equipment, turn budgets, fog of war, saves, and the world builder have
not been connected. Expedition state is **in memory only**: closing the game resets
it. Existing C++ saves are not read or written. Loot is collected but not equipped;
partial stack HP is not carried between battles, matching the original count-only
survivor handoff. Dungeon interiors, special-character recruitment/progression,
and spells are not exposed by this battle view.

Combat effects use the existing static sprites with movement tweens, melee lunges,
projectiles, damage flashes/numbers, and death fades. These are presentation
animations, not newly authored attack/death sprite sheets. Skeletons and scorpions
still use the original renderer's warrior/scout stand-ins. Asset consistency and
combat balance/play-feel tuning remain separate work.

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
