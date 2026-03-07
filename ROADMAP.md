# Raids of Umm'Natur — Development Roadmap

## Current State (March 2026)

469/469 logic tests pass.

**What works end-to-end:**
- World builder → save/load map → test-play adventure
- Hero pathfinds, spends move points, visits town → CastleState recruit screen
- Hero visits dungeon → combat screen with full initiative queue, click-move/attack, CombatAI
- Win → hero army rebuilt from survivors, loot item added to inventory; Lose → defeated overlay
- End turn → day advances, mine income credited, weekly unit growth, move points reset
- Wondrous items: data layer, ResourceManager loader, Hero inventory, dungeon loot pipeline

---

## Guiding principle

**Always have a playable vertical slice.** Each milestone produces something you can
actually run and feel. Gameplay first — visual polish deliberately last.

---

## MVP — The Core Loop

> Hero goes to city → buys troops → goes to dungeon → finds Special Character
> → finds Wondrous Item → equips it → goes to battle.
> All under correct movement and turn logic.

This is the first version of the game that feels like *Raids of Umm'Natur*,
not just a tech demo. Every item below is a hard dependency.

### MVP checklist

| # | What | Status | Strand |
|---|------|--------|--------|
| 1 | Hero visits city, buys troops | ✅ done | — |
| 2 | Turn logic: move points, day/week cycle | ✅ done | — |
| 3 | Wondrous item data + Hero inventory | ✅ done | A, B |
| 4 | Dungeon drops item on victory | ✅ done | C |
| 5 | Combat attack-range highlights (red/blue) | ✅ done | D0 |
| 6 | Flanking / Pinned mechanic | ✅ done | D1–D2 |
| 7 | `SpecialCharacter` struct + spawns at dungeon | ✅ done | F1 |
| 8 | Clearing dungeon recruits SC into hero party | ✅ done | F2 |
| 9 | `EquipmentState` — equip items onto SCs | ✅ done | G1–G2 |
| 10 | Cursed item locks in slot (no unequip) | ✅ done | G3 |
| 11 | Equipped item passive effects fire in combat | ✅ done | E1 |
| 12 | Loot table wired to hardcoded dungeon guard | ✅ done | C fixup |

**Done = MVP shipped.**

---

## Sequence to MVP

Work in this order — each step is additive and leaves tests green.

### D0 — Combat visual feedback *(do first — gates all combat testing)*

When a unit is selected: reachable hexes tint **blue**, enemy hexes in attack
range tint **red**. Renderer-only change; no engine logic.

### D1–D2 — Flanking / Pinned

If two units occupy opposite hex sides of an enemy: that enemy loses
Counter-Attack and takes 150% damage. New branch in `doAttack()` + tests.

### F1 — `SpecialCharacter` struct

```cpp
struct SpecialCharacter {
    std::string      id;
    std::string      name;
    std::string      archetype;   // "tank", "healer", "glass_cannon"
    // Equipment slots — filled by EquipmentState
    std::array<const WondrousItem*, 4> equipped{};
};
```

Lives in `src/entities/SpecialCharacter.h`. Pure data, no GL.
Hero gains `std::vector<SpecialCharacter> specials` (max 3 slots).

### F2 — SC spawns at dungeon, joins on clear

One hardcoded SC ("Kha'Rim the Wanderer", archetype tank) placed at the test
dungeon. On `PlayerWon`, if dungeon had an SC and hero has a free SC slot, SC
joins. Same `onResume()` pattern as item drops.

### C fixup — Wire loot table to the existing dungeon guard

`buildEnemyArmy()` currently ignores `EncounterDef`. Pass a loot table
(`{"scarab_amulet"}`) into `CombatState` when launching from the dungeon so
the pipeline built in Strand C is actually reachable in-game.

### G1–G3 — `EquipmentState`

Pushed from AdventureState (H key or hero portrait). Shows hero SC slots on
the left, item bag on the right. Click item → highlight valid slots. Click
slot → equip. Cursed items: red slot border, no unequip button.

### E1 — Passive item effects in combat

`calcDamage()` in `CombatEngine` checks `hero.specials[i].equipped[j]->passiveEffects`
and applies stat bonuses to the stacks they control. Additive to existing
damage formula.

---

## Post-MVP backlog *(in priority order)*

| # | Feature | Unlocks |
|---|---------|---------|
| 1 | Zone of Control (ZoC) — heavy units | Tactical depth |
| 2 | Telegraphed moves (Long Cast) | Risk/reward tension |
| 3 | Geometric AoE items (E2) | Item variety |
| 4 | Cursed item adverse effects (E3) | Risk/reward items |
| 5 | Win condition (clear all dungeons) | Complete game loop |
| 6 | Multiple dungeons on map | Progression arc |
| 7 | Save / Load session | Replayability |
| 8 | MainMenuState | Shippable entry point |
| 9 | Glass Cannon channel mechanic (F3) | High-skill play |
| 10 | Buildings in CastleState | Economy depth |
| 11 | Multiple heroes | Strategy layer |
| 12 | **SpriteBatcher** — before Phase 3 animations | Performance |
| 13 | **Unit sprites in army slots** — small per-unit icon in each slot so unit type is visually unambiguous (no more reading truncated names) | Clarity |

### SpriteBatcher — when and why

Current `SpriteRenderer` does one GL draw call per sprite. Fine for MVP
(~20 sprites max). Starts to hurt when Phase 3 lands: per-unit hit-flash,
death animations, and adventure map object icons push toward 50+ sprites/frame
with frequent texture state changes.

**Do this before Phase 3, not during.** The interface is simple:

```cpp
// Submit phase — no GL, just record
batcher.submit(texId, worldPos, size);

// Flush — one draw call per unique texture (or one with atlas)
batcher.flush(view, proj);
```

Current `SpriteRenderer::draw()` call sites swap to `submit()` one-for-one.
Fits cleanly into the `beginFrame` / `endFrame` pattern already in
`CombatRenderer` and `AdventureState`. Migration is a find-and-replace,
not a rewrite.

---

## Phase 2 — Strategy Depth *(after MVP)*

- Building tree in CastleState (`TownState::buildings` is already the hook)
- Multiple heroes: hire at castle, independent armies and move budgets
- Fog of war: unexplored tiles hidden, vision radius per hero
- Scaling difficulty: dungeon encounters grow harder by map region
- Scenario scripting: win/lose conditions, intro text, timed events

---

## Phase 3 — Visual Polish *(deliberately deferred)*

> Art that covers broken systems is wasted work. Do this after MVP is fun.

### Graphics direction: quality pixel art, not modern 3D

The target look is **HoMM3-era 2D pixel art** — rich, hand-crafted sprites on
a hex grid. Not a modern PBR pipeline. This is achievable within the current
OpenGL 3.3 Core renderer with focused effort.

**Non-negotiable technical baseline (do before any art work):**

1. **GL_NEAREST everywhere** — all textures must use `GL_NEAREST` mag/min
   filter. Any `GL_LINEAR` interpolation on pixel art produces blur. Audit
   every `glTexParameteri` call in `Texture.cpp`.

2. **Integer display scaling** — render to a fixed logical resolution
   (e.g. 960×540) then scale up to the window by an integer factor.
   Prevents sub-pixel bleed at non-integer window sizes. Needs an offscreen
   FBO + a simple blit pass.

3. **SpriteBatcher** (already in post-MVP backlog) — must land before Phase 3
   art work begins. One draw call per texture vs one per sprite.

**Art pipeline — PixelLab + manual touch-up:**

- All sprites generated at 128×128, transparent background (flood-fill removal
  script already exists in this codebase).
- Style: limited palette (~16–24 colours), no anti-aliasing, 1px black or dark
  outline on all characters.
- Terrain tiles: 128×128, follow hex tile guide in `assets/pixellab`.
- Background removal: corner flood-fill script (tolerance 30) works for clean
  studio backgrounds; complex backgrounds need manual masking in Aseprite.

**Phase 3 deliverables in order:**

| # | Task |
|---|------|
| 1 | Audit + fix all texture filters → GL_NEAREST |
| 2 | Integer-scale FBO pass |
| 3 | SpriteBatcher (prerequisite) |
| 4 | Hex edge blending shader (Sand↔Dune soft transitions) |
| 5 | Sprite per ObjType: Town (pyramid), Mine (tent), Dungeon (ruin entrance) |
| 6 | Full unit roster sprites (all 7 unit types + SCs) |
| 7 | Unit hit-flash + death animation (AnimatedSprite system already wired) |
| 8 | Pixel-art HUD frame, portrait slots, minimap |
| 9 | Optional: scanline / CRT overlay shader (toggle in settings) |

---

## Phase 4 — Content & Juice

- Sound effects + desert-themed ambient music
- Campaign: 3-map linear story arc set in Umm'Natur
- Diplomacy / faction system (Scarab Lords vs Sand Wraiths)
- Achievements / high score

---

## Asset Generation Queue (PixelLab)

| Asset | Type | Size | Status |
|-------|------|------|--------|
| sand.png | terrain | 64×64 | ✅ |
| dune.png | terrain | 64×64 | ✅ |
| mountain.png | terrain | 64×64 | ✅ |
| rock.png | terrain | 64×64 | ✅ |
| oasis.png | terrain | 64×64 | ✅ |
| ruins.png | terrain | 64×64 | ✅ |
| obsidian.png | terrain | 64×64 | ✅ |
| river.png | terrain | 64×64 | ✅ |
| town.png | object | 64×64 | ✅ |
| dungeon.png | object | 64×64 | ✅ |
| goldmine.png | object | 64×64 | ✅ |
| armoured_warrior.png | unit | 128×128 | ✅ |
| enemy_scout.png | unit | 128×128 | ✅ |
| special_character_tank.png | unit | 128×128 | ⬜ |
| special_character_healer.png | unit | 128×128 | ⬜ |
