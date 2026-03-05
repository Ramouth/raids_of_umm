# Raids of Umm'Natur — Development Roadmap

## Current State (March 2026)

Combat loop is fully closed. Initiative queue, click-move/attack, CombatAI, event-driven
animation, CombatOutcome propagated back to AdventureState. Hero army syncs after battle.
TurnManager drives day/week cycle; mine income flows to faction treasury; gold shown in HUD.
432/432 logic tests pass.

**What works end-to-end:**
- World builder → save/load map → test-play adventure
- Hero pathfinds, spends move points, visits dungeon → combat screen
- Combat: player clicks to move/attack, enemy AI acts, log shows every event
- Win → hero army rebuilt from survivors; Lose → army cleared, dungeon guard restored
- End turn → day advances, mine income credited, moves reset

---

## Guiding principle

**Always have a playable vertical slice.** Each milestone produces something you can
actually run and feel. No phase is "done" before the next begins — features at different
phases run in parallel when they don't create downstream dependencies.

The one exception: **don't build CastleState before TownState**, because the week cycle
directly determines what units are available to recruit. Getting that relationship wrong
means ripping up CastleState later.

---

## Next milestone — CastleState (recruit units)

This is the highest-value missing piece. Without it the economy has no sink and gold
is meaningless. The week cycle already runs; TownState plugs into it naturally.

### A. `TownState` struct  *(new: `src/world/TownState.h`)*

Persistent per-town data. Lives in `AdventureState::m_townStates` (same pattern as
`m_objectControl`). Updated weekly by TurnManager; read by CastleState on each visit.

```cpp
struct TownState {
    std::unordered_map<std::string, int> recruitPool;  // unitId → count available
    std::set<std::string>                buildings;    // buildingId (empty for now)
};
using TownStateMap = std::unordered_map<HexCoord, TownState>;
```

### B. TurnManager ticks weekly growth

`TurnManager::nextDay()` already takes `ObjectControlMap`. Extend signature to also
take `TownStateMap&`. At day % 7 == 0, for each town in the map, add `weeklyGrowth`
for each known unit type. Buildings later multiply this.

### C. CastleState gets a real interface

Replace the current `CastleState(string name)` stub with:

```cpp
CastleState(std::string name, TownState& town, ResourcePool& treasury);
```

Renders a list of recruitable unit types with available count and gold cost.
Click to buy: deduct count from `town.recruitPool`, deduct gold from `treasury`,
call `AdventureState::onResume()` to sync hero army (same mechanism as combat).

CastleState writes recruited units into a `std::vector<SurvivorStack>` shared result
(same `shared_ptr` pattern used for `CombatOutcome`), and AdventureState's `onResume()`
adds them to the hero's army slots.

### D. Lose state

If hero army is empty after defeat (EnemyWon), show a simple "Defeated — press any key"
overlay in AdventureState and block all movement. Offer restart via R key (already wired).

---

## Near-term backlog  *(order matters — do in sequence)*

| # | Feature | Depends on | Unlocks |
|---|---------|-----------|---------|
| 1 | TownState + weekly growth tick | TurnManager | CastleState |
| 2 | CastleState recruit screen | TownState | Gold sink, army building |
| 3 | Lose state | Combat outcome (done) | Meaningful defeat |
| 4 | Multiple dungeon encounters on map | — | Progression arc |
| 5 | Win condition (clear all dungeons) | Dungeon encounters | A complete game loop |
| 6 | Save / Load session | All of above stable | Replayability |
| 7 | MainMenuState | Save/Load | Shippable entry point |

Save/Load and MainMenu are last because they don't change whether the game is *fun* —
they change whether it's *shippable*. Build fun first.

---

## Phase 2 — Strategy Depth  *(after core loop is satisfying)*

- **Buildings in CastleState**: building tree unlocks higher-tier units and weekly
  growth bonuses. `TownState::buildings` is already the hook.
- **Multiple heroes**: hire at castle, independent armies and move budgets
- **Fog of war**: unexplored tiles hidden, vision radius per hero
- **Scaling difficulty**: dungeon encounters grow harder by map region
- **Scenario scripting**: win/lose conditions, intro text, timed events

---

## Phase 3 — Visual Polish  *(deliberately deferred)*

> Do this *after* gameplay is fun. Art that covers broken systems is wasted work.

### Terrain
- Hex edge blending shader (soft transitions between Sand↔Dune, etc.)
- Animated tiles: ripple on River, shimmer on Obsidian

### Map Objects
- Sprite per ObjType: Town (pyramid silhouette), Mine (tent + pickaxe), Dungeon (ruin entrance)
- Visited-dimming already works; add "cleared" visual state for defeated dungeons

### Units / Characters — Animation System

**Architecture (5 steps):**

| # | What | Files touched | Status |
|---|------|---------------|--------|
| 1 | Shader UV-frame selection | `sprite.vert`, `SpriteRenderer.h/cpp` | ✅ done |
| 2 | `AnimatedSprite` class | `src/render/AnimatedSprite.h/cpp` | ✅ done |
| 3 | PixelLab client fix (add `--ref` param to animate) | `scripts/pixellab_client.py` | ⬜ |
| 4 | Generate idle/walk strips for warrior + scout | `assets/textures/units/` | ⬜ |
| 5 | `AdventureState` integration | `src/adventure/AdventureState.h/cpp` | ⬜ |

**Spritesheet contract:**
- Horizontal strip, N frames each 64×64 px → image width = N×64, height = 64
- UV per frame: `u ∈ [frame/N, (frame+1)/N]`

**AnimClip struct:**
```cpp
struct AnimClip { int firstFrame; int numFrames; float fps; bool loop; };
```
Planned clips: `idle` (4 frames, 6 fps), `walk` (4 frames, 8 fps), `attack` (4 frames, 12 fps, no loop)

### Combat Screen
- Unit sprites with hit-flash and death animation tied to CombatEvent types
- Spell VFX (particle system or sprite sheet)
- Full pixel-art battle backdrop (desert/ruins/dungeon based on tile type)

### UI
- Pixel-art HUD frames (status bars, button panels)
- Minimap in corner

---

## Phase 4 — Content & Juice  *(polish pass)*
- Sound effects + desert-themed ambient music
- Campaign: 3-map linear story arc set in Umm'Natur
- Artifact effects (passive bonuses on hero)
- Diplomacy / faction system (two factions: Scarab Lords vs Sand Wraiths)
- Achievements / high score

---

## Deferred / Won't Do Soon
- Multiplayer
- 3D rendering (keep the flat-top hex + billboard sprite aesthetic)
- Procedural campaign (hand-crafted scenarios first)

---

## Asset Generation Queue (PixelLab)
Track what still needs to be generated. Use `scripts/pixellab_client.py`.

| Asset | Type | Size | Status |
|-------|------|------|--------|
| sand.png | terrain | 64×64 | ✅ done |
| dune.png | terrain | 64×64 | ✅ done |
| mountain.png | terrain | 64×64 | ✅ done |
| rock.png | terrain | 64×64 | ✅ done |
| oasis.png | terrain | 64×64 | ✅ done |
| ruins.png | terrain | 64×64 | ✅ done |
| obsidian.png | terrain | 64×64 | ✅ done |
| river.png | terrain | 64×64 | ✅ done |
| town.png | object | 64×64 | ✅ done |
| dungeon.png | object | 64×64 | ✅ done |
| goldmine.png | object | 64×64 | ✅ done |
| armoured_warrior.png | unit | 128×128 | ✅ done |
| enemy_scout.png | unit | 128×128 | ✅ done |
