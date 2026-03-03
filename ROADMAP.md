# Raids of Umm'Natur — Development Roadmap

## Current State (March 2026)
World builder works. Adventure map loads. Hero moves, pathfinds, visits objects.
Pixel-art terrain tiles render (sand, dune, mountain). Combat modal exists as a stub.

---

## Phase 1 — Playable Core  *(next 4–6 milestones)*
> Goal: one complete game loop you can actually *play*, ugly or not.

### 1.1  Combat System (priority #1)
- Turn order based on unit speed / initiative queue
- Actions: Attack · Defend · Spell · Retreat
- Damage formula (ATK - DEF with dice variance)
- Win/lose condition → pop state, apply result (loot, XP, loss)
- Units survive between combats (HP carried over, die at 0)

### 1.2  TurnManager — Day/Week Cycle
- Day counter (already has `m_day`)
- Weekly income phase: owned mines pay out gold/crystals
- Hero move-point reset each new day
- Week-end events (creature replenishment, random events)

### 1.3  Resource Tracking
- Gold + Sand Crystal counters in AdventureState
- Income applied by TurnManager each week
- HUD shows resources (extend existing HUDRenderer)

### 1.4  Save / Load Full Session
- Serialise: WorldMap (already works) + hero state + resources + day + visited flags
- Load brings you back exactly where you left off

### 1.5  MainMenuState
- New Game / Load / Quit buttons
- Entry point instead of WorldBuilder

### 1.6  Unit & Spell Data (JSON)
- `data/units.json`: name, hp, atk, def, speed, move, cost
- `data/spells.json`: name, damage/effect, mana cost
- ResourceManager loads at startup; combat references these

---

## Phase 2 — World & Strategy Depth  *(after Phase 1)*
> Goal: meaningful decisions, replayability.

- **Castle / Town screen**: building tree, recruit units, buy spells
- **Multiple heroes**: hire at castle, give them armies
- **Fog of war**: unexplored tiles hidden, vision radius per hero
- **Neutral armies**: guard mines/dungeons, fight back
- **Scenario scripting**: win/lose conditions, intro text, timed events
- **Difficulty settings**: starting resources, AI aggression

---

## Phase 3 — Visual Polish  *(deliberately deferred)*
> Do this *after* gameplay is fun. Art that covers broken systems is wasted work.

### Terrain
- Generate remaining 7 terrain textures via PixelLab (rock, oasis, ruins, obsidian, river, wall, battle)
- Hex edge blending shader (soft transitions between Sand↔Dune, etc.)
- Animated tiles: ripple on River, shimmer on Obsidian

### Map Objects
- Sprite per ObjType: Town (pyramid silhouette), Mine (tent + pickaxe), Dungeon (ruin entrance), Artifact (glowing relic)
- Visited-dimming already works; add "cleared" visual state

### Units / Characters
- Animated sprite sheets (idle 4-frame, walk 4-frame, attack 4-frame)
- Hero facing direction using current `m_heroFacingAngle`
- Enemy unit sprites rendered on adventure map

### Combat Screen
- Full pixel-art battle backdrop (desert/ruins/dungeon based on tile type)
- Unit sprites with hit-flash and death animation
- Spell VFX (particle system or sprite sheet)

### UI
- Pixel-art HUD frames (status bars, button panels)
- Font rendering (bitmap font via texture atlas)
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
- Procedural campaign (nice to have but hand-crafted scenarios first)

---

## Asset Generation Queue (PixelLab)
Track what still needs to be generated. Use `scripts/pixellab_client.py`.

| Asset | Type | Size | Status |
|-------|------|------|--------|
| sand.png | terrain | 64×64 | ✅ done |
| dune.png | terrain | 64×64 | ✅ done |
| mountain.png | terrain | 64×64 | ✅ done |
| armoured_warrior.png | unit | 128×128 | ✅ done |
| rock.png | terrain | 64×64 | pending |
| oasis.png | terrain | 64×64 | pending |
| ruins.png | terrain | 64×64 | pending |
| obsidian.png | terrain | 64×64 | pending |
| river.png | terrain | 64×64 | pending |
| town.png | object | 64×64 | pending |
| dungeon.png | object | 64×64 | pending |
| goldmine.png | object | 64×64 | pending |
| enemy_scout.png | unit | 128×128 | ✅ done |
