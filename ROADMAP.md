# Raids of Umm'Natur — Roadmap

> **Guiding principle:** Always have a playable vertical slice.
> Each milestone produces something you can actually run and feel.
> Gameplay first — visual polish deliberately last.

---

## Completed

- [x] Hex grid, camera, world builder (save/load map, F1 alignment editor)
- [x] Adventure map — hero movement, pathfinding, object visits, turn system
- [x] Combat — initiative queue, BFS movement, click-move/attack, flanking/pinned
- [x] Combat AI — ranged priority, melee fallback, auto-battle toggle
- [x] Dungeon state — guard combat, SC recruitment, loot drop, result overlay
- [x] Town / Castle — unit recruitment, weekly growth, treasury
- [x] Resource tracking — gold + sand crystals, mine income
- [x] Special Character system — XP, levelling, stat growth
- [x] SC skill tree — branching level-up choices, branch-gated nodes, combat overlay
- [x] Party screen (F key) — view hero army and SC slots
- [x] Wondrous items — data layer, ResourceManager, hero inventory, equipment screen
- [x] Unique SC per dungeon (Ushari / Sekhara)
- [x] Session save / load — Ctrl+S / Ctrl+L, single JSON (hero, army, SCs, objectControl, treasury, day)
- [x] Main menu — New Game / Continue / World Builder / Quit; Continue greyed when no save exists
- [x] ESC exit prompt — Save & Exit / Exit / Cancel (optional save on leaving adventure)

---

## Next — Stable Test Environment

One item remains before the test environment is solid:

### Author the Canonical Test Map
- Design a fixed map in WorldBuilder, save as `data/maps/default.json`
- New Game will load it automatically; eliminates procedural-map instability
- Place at least: 1 town, 2 dungeons (Tomb of Kha'Set + Buried Sanctum), 1 gold mine

---

## Combat Depth (backlog, priority order)

These deepen the tactical system. Implement after the stable test environment
so every fight is reproducible.

| # | Feature | Notes |
|---|---------|-------|
| 4 | **Attack-type triangle** | Physical (vs defense) · Piercing (50% bypass) · Magical (ignores armor). One JSON field per unit. |
| 5 | **Commander aura** | Passive ring buff to adjacent friendly stacks |
| 6 | **AoE special attack** | Engine targeting + UI hex highlight |
| 7 | **Push / Telekinesis** | Move enemy unit on attack |
| 8 | **Zone of Control (ZoC)** | Heavy units "stick" — moving past costs an attack-of-opportunity |
| 9 | **Terrain in combat** | Rubble (+1 def, move cost 2) · High Ground (+2 def) · Pit (−1 def, move cost 2) |
| 10 | **SC death** | Permanent on loss (items drop) — raises stakes for cursed-item decisions |
| 11 | **Long Cast (telegraphed)** | Most powerful moves charge one round; affected hexes highlighted |
| 12 | **Glass Cannon channel** | SC channels nearby stack for a burst; stack skips its turn |

---

## Economy & Content

| # | Feature |
|---|---------|
| 13 | Unit & spell JSON → engine (attack-type, ability tags fully interpreted) |
| 14 | Castle building tree (Fort → Citadel → Castle, Mage Guild tiers) |
| 15 | Win condition (clear all dungeons) |
| 16 | Multiple dungeon types (vary guard composition per dungeon template) |
| 17 | Multiple heroes (hire at castle, independent armies) |

---

## Phase 2 — Strategy Depth *(after core loop is fun)*

- Fog of war: unexplored tiles hidden, vision radius per hero
- Scaling difficulty: dungeon encounters grow harder by map region
- Scenario scripting: win/lose conditions, intro text, timed events
- Diplomacy / faction system (Scarab Lords vs Sand Wraiths)
- Campaign: 3-map linear story arc

---

## Phase 3 — Visual Polish *(deliberately deferred)*

> Art that covers broken systems is wasted work.

**Non-negotiable technical baseline before any art work:**

1. **GL_NEAREST everywhere** — audit every `glTexParameteri` in `Texture.cpp`; no blur on pixel art
2. **Integer display scaling** — render to fixed logical res (e.g. 960×540), scale up by integer factor via offscreen FBO
3. **SpriteBatcher** — one draw call per texture instead of one per sprite; must land before Phase 3 art begins

**Phase 3 deliverables in order:**

| # | Task |
|---|------|
| 1 | Audit + fix all texture filters → GL_NEAREST |
| 2 | Integer-scale FBO pass |
| 3 | SpriteBatcher |
| 4 | Hex edge blending shader (Sand↔Dune soft transitions) |
| 5 | Sprite per ObjType on adventure map |
| 6 | Full unit roster sprites (all 7 unit types + SCs) |
| 7 | Unit hit-flash + death animation |
| 8 | Pixel-art HUD frame, portrait slots, minimap |

SC branch-choice overlay is an explicit placeholder — designed to be swapped wholesale here.

---

## Asset Queue (PixelLab)

| Asset | Size | Status |
|-------|------|--------|
| sand, dune, mountain, rock, oasis, ruins, obsidian, river terrain | 64×64 | ✅ |
| castle, dungeon, goldmine objects | 64×64–128×128 | ✅ |
| armoured_warrior, enemy_scout units | 128×128 | ✅ |
| rider variants (archer, banner, knight, lance) | 128×128 | ✅ |
| Ushari, Sekhara, Khet SC portraits | 128×128 | ⬜ |
| remaining unit types (djinn, ancient_guardian, pharaoh_lich, mummy) | 128×128 | ⬜ |
