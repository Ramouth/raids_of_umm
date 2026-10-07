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
trail through Corvin's country. His note and annotated survey establish that he
was there and reveal a definite surface route across Greyfen toward the old
passage. The chapter ends with a choice: report to your father, or follow that
road with Ushari. On the home route she returns with the expedition to report and provision the next march. Both branches retain her progression. Neither ending reaches or enters
the old passage. A second above-ground level, **The Greyfen Road**, is planned
before the underground journey. Greyfen is House Rook's domain; taking
Rookwatch secures its western approach, not the lands beyond it. The chapter outcome is saved to `user://chapter_one.json`.
Later chapters are not yet playable. Start a new expedition for the revised fixed survey and escort flow; old in-progress choices are not migrated.

Ending a day briefly dims the map and announces the day; week boundaries have a longer week card. The hooded figure stands beside the Hermit’s Wolves, warns you before their battle, and disappears after their defeat. After all five wolf packs fall, the next **End Day** brings nightfall and Ushari arrives in a player-paced night conversation with a portrait and speaker nameplates. She asks where Aldren went; the commander can point her east, answer bluntly, or welcome her. The `[name]` marker is reserved for future commander-name substitution. Choices and journal entries describe her reaction without exposing numerical consequences; the relationship state survives saves.

Run `res://tests/story_effects_smoke.gd` for calendar and conversation presentation checks.

Story source: `scripts/north_story.py`; regenerate with `python3 scripts/north_story.py`.
Map source: `scripts/gen_north_map.py`; hand-tuned playable map: `data/maps/old_passage.json`.
The vale now has a quarry, northern routes have stone and obsidian supplies, and
wood/stone resupply replaces redundant gold piles. Starting resources and prices
are unchanged.

Run the native story/save regressions with `./scripts/build_godot_combat.sh --test`
and both ending flows with `./scripts/run_godot.sh --headless --script res://tests/demo_smoke.gd`.

## House Rook: the computer opponent

Start a **New expedition** for this revision. Lord Edric Rook rules Rookwatch
beyond Greyfang, with an existing castle, gold mine, sawmill, quarry and northern
army. End Day advances his turn: he develops towns, pays for recruits from weekly
pools, gathers supplies, captures holdings and returns home to replenish or defend.
His economy, growth, construction and movement wait until the meeting with Corvin; two days after that warning he can march west. He receives ordinary town/mine income, with no daily subsidy or
free second army, and does not seal the passage.

Corvin offers the optional **Corvin's Old Rival** quest and campaign supplies.
At the northern approach, **Ask House Rook for passage** leads to a deceptive
escort meeting at **Greyfang Watch**, followed by an actual ambush battle; **We will find our own road** avoids it.
Rookwatch can be captured and used for construction and recruitment. Defeating
Edric's army and capturing his remaining local towns ends resistance on this map;
House Rook's wider domain in Greyfen remains ahead, and
the passage search and both chapter endings remain available without conquest.
The escort is a separate finite patrol; requesting passage does not weaken the castle. Victory yields 300 gold and patrol orders opening an alternative approach. Clearing Troll Gate provides the southern track. Capturing Rookwatch adds a town supply base and 750 gold/5 wood from its stores. The ending records these differences for the planned next chapter.
Return to Hallowmere after taking the castle for Corvin's revealing offer.
Enemy losses persist after retreat, including combined castle defenders, and survive saves.

Run `res://tests/rookwatch_smoke.gd` for the passage request, live castle
defenders and dialogue-to-combat handoff; native regressions cover the economy,
territorial expansion, capture and save/load.

Town defenses use the existing tactical battlefield; dedicated siege walls and
siege engines are not part of this demo. The plot is recorded in
[the roadmap](../ROADMAP.md#stage-1--story) and [faction notes](../Factions.md).

## Expanded northern exploration

The Old Passage now spans 1,027 hexes with 89 sites. The original opening vale
leads into winding woodland and mountain routes, guarded supplies and a northern
detour. Arrow keys or WASD pan the camera. Guard badges show total creature counts;
hover a camp to inspect its individual stacks.

Three surviving inscriptions reveal the conquered land beneath the player's
inherited map: Esh Varen beneath Varenhold, Tal Eren at the Coldwater crossing,
and Orun at the ruined assembly court. Ushari translates them into the journal;
inscriptions found before she arrives are remembered as rubbings. The survey is always at **Old Mine of Kaldur**, identified by the required camp visit. At the survey lodge,
she supplies Ishara, the house of names, and explains her own purpose: recovering
her people's household records. The player's family belongs to the colonizers.

Match the survey diagram's marks to those original names to read Aldren's route. **Ask Ushari for help** recalls discovered readings. **Leave the survey**
(or Escape) returns to exploration; movement, days and saves remain available.
Return to the same mine and use **Read the survey with Ushari** to reopen it.
Wrong attempts have no penalty. Decoding it gives the Greyfen itinerary and the chapter ending choice.
All three translated inscriptions and one secured approach (castle, Troll Gate or patrol orders) are required before completing the survey and departure choice. The location, unresolved choice and translations survive saves; unfinished
wheel positions reset. Start a new expedition for the revised map and story.

Map generation: `python3 scripts/gen_north_map.py` (the authored opening terrain is
kept in `scripts/north_vale.json`). Story generation: `python3 scripts/north_story.py`.
Run `res://tests/expanded_demo_smoke.gd` for arrow keys, guard counts, rune stones,
and puzzle controls, and `res://tests/demo_smoke.gd` for the full story flows.

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

Controls: left click to travel; wheel to zoom; right/middle drag, arrows or WASD to pan;
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

See [the demo flow audit](../docs/demo_flow.md) for player context at each beat and automated route verification.
