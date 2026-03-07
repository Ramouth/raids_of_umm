# Raids of Umm'Natur — Project Overview

## Vision

A turn-based strategy game combining two pillars:

- **Heroes of Might & Magic III** — hex-grid adventure map, army stacks, town building, resource economy, tactical combat, AI opponents
- **Final Fantasy (I–VI era)** — named heroes with job classes and skill trees, story-driven scenarios, dialogue/quests, RPG character progression layered onto the strategy skeleton

The world is **Umm'Natur**: a dying desert empire, pyramid ruins half-buried under dunes, undead pharaoh legions, bound djinn, and nomadic tribes fighting over the last oases. Aesthetic is ancient Egyptian / Arabian Nights.

---

## Tech Stack

| Layer | Library | Notes |
|---|---|---|
| Language | C++17 | standard library only beyond deps |
| Window/Input | SDL2 2.0 | system shared lib |
| Rendering | OpenGL 3.3 Core | GLAD loader (bundled) |
| Math | GLM | header-only |
| Data | nlohmann/json | header-only |
| Build | CMake 3.16+ | local cmake binary in `local_deps/` |

---

## What Is Currently Working

| System | Status |
|---|---|
| SDL2 window + OpenGL 4.6 context | Working |
| GLAD loader | Working |
| Shader compilation and uniform helpers | Working |
| Flat-top hex tile VAO + outline shader | Working |
| Sprite renderer (animated atlas) | Working |
| Adventure map (469-tile procedural) | Working |
| Orthographic camera — pan (WASD) + zoom (scroll) | Working |
| Hover highlight + A* path preview | Working |
| Hero smooth movement animation | Working |
| Turn system (day counter, move point reset) | Skeleton only |
| Terrain types (6 kinds) | Color-coded placeholders |
| Map objects (Town, Dungeon, Mines, Artifact) | Placeholders |
| Resource types (7, Egyptian-themed) | Defined |
| Unit types (7, tiers 1–7) | JSON data only |
| Spells (5) | JSON data only |
| EventBus (typed pub/sub) | Built, not wired |

---

## Source Layout (current)

```
src/
  main.cpp
  core/
    Application.h/cpp     — SDL2 window, GL context, main loop
    StateMachine.h/cpp    — stack-based FSM with deferred ops
    EventBus.h            — typed pub/sub singleton
  hex/
    HexCoord.h            — axial coords, world↔hex conversion, hash
    HexGrid.h             — generic unordered tile map + A* pathfinding
    HexGrid.cpp           — (stub)
  render/
    Shader.h/cpp          — GLSL compile/link, uniform helpers
    HexRenderer.h/cpp     — per-tile draw (shared VAO, per-call matrix+color)
    SpriteRenderer.h/cpp  — sprite atlas + animation
    Texture.h/cpp         — SDL_Surface → GL texture
  adventure/
    AdventureState.h/cpp  — main playable state (also contains Terrain, MapTile,
                            MapObject, Hero structs — see Architecture Issues)
  entities/
    UnitType.h            — immutable unit data (loaded from JSON)
  world/
    Resources.h           — Resource enum + ResourcePool
```

---

## Architecture — What Is Good

- **Stack-based FSM with deferred ops** is the right model for a game with MainMenu → Adventure → Combat → Town sub-screens. States don't modify the stack while being iterated.
- **HexGrid<T>** is cleanly generic; works for both the adventure map and the tactical combat grid without any coupling.
- **HexCoord** is a pure value type with compile-time arithmetic — correct.
- **A* in HexGrid** with a pluggable cost/passability function is flexible.
- **EventBus** decouples systems that need to react to game events (combat result, resource changed, hero levels up) without circular includes.
- **nlohmann/json data files** for units/spells/artifacts are the right call — designers can iterate without recompiling.
- **ResourcePool** with operator overloads is clean economy math.
- **SpriteRenderer** already handles atlas animations — useful for unit portraits and map sprites.

---

## Architecture Issues (to fix before going deeper)

### 1. AdventureState.h carries world-model types

`Terrain`, `MapTile`, `MapObject`, and the `Hero` struct all live in `AdventureState.h`. This pollutes the rendering state with domain model, makes it impossible to reference these types from other states (Combat needs MapTile passability), and conflates presentation logic (`terrainColor()`) with data.

**Fix:** Create `src/world/MapTile.h` (Terrain, MapTile, terrainColor), `src/world/MapObject.h` (ObjType, MapObject), and `src/hero/Hero.h`.

### 2. Hero is a thin struct, not an RPG character

The current `Hero` only has `pos`, `movesMax`, `movesLeft`. A HoMM3+FF hybrid needs full character data.

**Fix:** Expand Hero (see Hero System section below).

### 3. No ResourceManager

UnitType references `meshId`/`textureId` strings but nothing loads them. JSON data files exist but are not parsed at runtime.

**Fix:** Add `src/data/ResourceManager.h/cpp` — loads and caches `UnitType[]`, `SpellDef[]`, `ArtifactDef[]` from JSON at startup; caches GL textures by string key.

### 4. Camera is private to AdventureState

Camera math (`viewMatrix`, `projMatrix`, `screenToWorld`, pan/zoom state) is entangled with game logic. Combat will need its own camera.

**Fix:** Extract `src/render/Camera2D.h` (orthographic pan/zoom with world↔screen helpers).

### 5. A* open set is a linear scan

`findPath` scans the entire open list for the minimum each step — O(n²). Fine for the 469-tile map right now, but combat grids and large maps will feel it.

**Fix (later):** Replace with `std::priority_queue` or a min-heap. Not urgent yet.

### 6. EventBus is a global singleton

Fine for a small game, but game-state events (unit damaged, hero moved) and application events (window resized) share the same bus. There's no lifetime management beyond manual `unsubscribe`.

**Fix (later):** Scope buses per-state or add RAII subscription handles. Low priority until many subscribers exist.

---

## Proposed Source Layout (target)

```
src/
  core/            — Application, StateMachine, EventBus        [done]
  render/          — Shader, HexRenderer, SpriteRenderer,
                     Texture, Camera2D                          [Camera2D: new]
  hex/             — HexCoord, HexGrid                         [done]
  world/           — Resources, MapTile, MapObject, TileMap     [split + new]
  data/            — ResourceManager (JSON loaders, tex cache)  [new]
  hero/            — Hero, HeroClass, HeroSkills, Spellbook,
                     Army, ArmySlot                            [new]
  combat/          — CombatState, CombatGrid, CombatUnit,
                     InitiativeQueue, CombatResolver,
                     StatusEffect                              [new]
  town/            — TownState, Building, Dwelling             [new]
  adventure/       — AdventureState, AdventureMap              [refactor]
  campaign/        — Scenario, Quest, Dialogue, StoryEvent     [new — FF layer]
  ui/              — UIManager, Panel, Button, HUD,
                     DialogueBox, MiniMap                      [new]
  ai/              — AdventureAI, CombatAI                     [new]
  save/            — SaveManager (JSON serialize/deserialize)  [new]
```

---

## System Design

### Hero System (HoMM3 + FF hybrid)

```
Hero
  ├── Identity: id, name, class (HeroClass enum), portrait
  ├── RPG stats (FF layer)
  │     attack, defense, spellpower, knowledge     ← primary (grow per level)
  │     xp, level (1–50), mana, manaMax
  ├── Secondary skills (HoMM3 layer)
  │     up to 8 skills, each Basic/Advanced/Expert
  │     e.g. Archery, Logistics, Necromancy, Diplomacy, Leadership
  ├── Spellbook: vector<SpellId>
  ├── Equipment: 7 artifact slots (helm, armor, weapon, boots, cloak, ring×2)
  ├── Army: 7 ArmySlot { const UnitType*, int count }
  └── Adventure: pos, movesMax, movesLeft, visitedObjects
```

**HeroClass** determines starting skills, spell school affinity, and stat growth rates.
Planned classes: Warlord, Arcanist, Shadowblade, Hierophant, Nomad.

### Army & Combat (HoMM3 core)

- Hero carries up to 7 unit stacks.
- Combat is hex-tactical: 11×9 grid, alternating turns by initiative order.
- Stacks act as one unit; damage kills whole creatures from the stack.
- Spells cast from hero's spellbook during their turn.
- Morale and Luck rolls (HoMM3) modify outcomes.

### Town System

- Towns are the economic engine: buildings unlock units and spells.
- Building tree: Fort → Citadel → Castle; Mage Guild (levels 1–5); Dwellings per tier.
- Each turn: dwellings accumulate weekly growth; player visits town to recruit.

### Quest / Dialogue System (FF layer)

- Scenarios have named NPCs at map locations.
- Dialogue trees trigger on hero visit.
- Quests: deliver artifact, defeat specific army, reach location.
- Story events (scripted) fire on day N or after objective completion.
- This is what separates the game from pure HoMM3 — a narrative layer.

### Resource Economy

Seven resources (all Egyptian-themed replacements for HoMM3 originals):
Gold, SandCrystal, BoneDust, DjinnEssence, AncientRelic, MercuryTears, Amber.

Mines on the adventure map produce 1 resource/day when controlled.
Towns have a marketplace for resource conversion at an exchange rate.

### Save / Load

Full game state serialized to JSON via nlohmann. One file per slot.
Includes map state, hero state, town state, turn number, random seed.

---

## Immediate Next Steps (recommended order)

1. **Split AdventureState.h** — extract `MapTile`/`MapObject` → `world/`, `Hero` → `hero/`
2. **ResourceManager** — load `units.json`, `spells.json`, `artifacts.json` at startup; texture cache
3. **Camera2D** — extract camera from AdventureState into a reusable component
4. **Hero model expansion** — add XP, level, primary stats, secondary skills list, spellbook, army slots
5. **Army/Stack types** — `ArmySlot { const UnitType* type; int count; }`, `Army { std::array<ArmySlot,7> }` in `hero/`
6. **TurnManager** — week/day cycle, income phase, growth phase, hero move-point reset
7. **UI HUD skeleton** — resource bar, day counter, end-turn button (SDL2 immediate-mode or simple retained panel)
8. **Town state** — visit screen, building tree, unit recruitment
9. **CombatState** — tactical grid, initiative queue, attack/spell resolution
10. **MainMenuState** — scenario select, new game, load game
11. **Campaign/Quest system** — scripted events, dialogue boxes, objectives
12. **AI** — adventure pathfinding for enemy heroes; combat heuristics
13. **Save/Load** — JSON serialization of full world state

---

## Data Files

```
data/
  units.json       — 7 unit types, tiers 1–7 (done)
  spells.json      — 5 spells (done)
  artifacts.json   — artifact definitions (stub)
  maps/
    tutorial.json  — first scenario definition (stub)
```

Future: `heroes.json` (hero templates), `towns.json` (building trees), `events.json` (scripted story events).

---

## Controls (current)

| Input | Action |
|---|---|
| WASD / Arrow keys | Pan camera |
| Mouse scroll | Zoom in/out |
| Left click (tile) | Move hero (if selected) |
| Left click (hero) | Select hero |
| T | End turn |
| ESC | Quit |
