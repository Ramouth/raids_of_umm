# Raids of Umm'Natur — Roadmap

> **Guiding principle:** Always have a playable vertical slice.
> Each milestone produces something you can actually run and feel.
> Gameplay first — visual polish deliberately last.

---

## Vision

**Warcraft 3 meets HoMM3 in the Egyptian desert.**
Solid multiplayer (2–3 factions, two win conditions) built on top of a hand-crafted campaign.
See `Factions.md` for full faction design and `Factions.md#open-questions` for decisions still needed.

---

## Completed

- [x] Hex grid, camera, world builder (save/load map, F1 alignment editor)
- [x] Adventure map — hero movement, pathfinding, object visits, turn system
- [x] Combat — initiative queue, BFS movement, click-move/attack, flanking/pinned
- [x] Combat AI — ranged priority, melee fallback, auto-battle toggle
- [x] Attack-type triangle — Physical / Piercing (50% bypass) / Magical (full bypass); data-driven per unit
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
- [x] Animated sprites — AnimatedSprite + clip state machine across adventure, dungeon, combat

---

## Milestone 0 — Stable Test Environment *(do first)*

### Author the Canonical Map
- Design a fixed map in WorldBuilder, save as `data/maps/default.json`
- New Game loads it automatically; kills procedural-map instability
- Must include: 2 towns (one per starting zone), 2 dungeons (Tomb of Kha'Set + Buried Sanctum),
  2 gold mines, 1 crystal mine, 1 Eye shard artifact

---

## Milestone 1 — Combat & Economy Foundation

These must be validated with **heuristic calculations** before building on top of them.
Target: each match-up produces fights that last 8–15 rounds; no unit is obviously broken.

| # | Item | Status |
|---|------|--------|
| 1 | **Unit JSON → engine ability tags** | Full interpretation of `undead`, `poison_strike`, `curse_strike`, `raise_dead`, `flying`, `aura_of_decay` | ⬜ |
| 2 | **Combat simulation harness** | Headless N-vs-M sim script to generate DPS/survivability tables per unit matchup | ⬜ |
| 3 | **Economy balance sheet** | Gold income curve vs unit cost; time-to-tier-5 per faction; validated against sim | ⬜ |
| 4 | **Commander aura** | Passive buff to adjacent stacks (Ushari / Kharim) | ⬜ |
| 5 | **ZoC (Zone of Control)** | Heavy units lock adjacent enemies — moving past triggers free attack | ⬜ |
| 6 | **Terrain in combat** | Rubble / High Ground / Pit tiles with move cost + def modifiers | ⬜ |
| 7 | **SC permadeath** | SC dies in battle → items drop, removed from hero roster permanently | ⬜ |

---

## Milestone 2 — Dungeon Narrative

Each dungeon tells a self-contained story with SC interaction.

| # | Item |
|---|------|
| 1 | Dungeon event sequence — enter → narration panel → optional SC dialogue → encounter |
| 2 | SC fear / refusal system — certain SCs refuse specific dungeons; affects army composition |
| 3 | Cutscene overlay — text + portrait, ESC to skip |
| 4 | Per-dungeon loot table and Guard variant tied to dungeon template JSON |
| 5 | Eye shard as collectible dungeon reward (tracks toward Pact win) |

---

## Milestone 3 — Faction System & Win Conditions

| # | Item |
|---|------|
| 1 | Three factions defined in JSON (Scarab Lords, Tomb Dynasties, Djinn Courts) |
| 2 | Faction selection on New Game / lobby |
| 3 | Win condition: **Military** (destroy all enemy towns) |
| 4 | Win condition: **Pact of Djangjix** (collect all Eye shards + deliver to Tomb) |
| 5 | Faction-specific unit unlocks in Castle (unit availability gated by faction) |
| 6 | Multiple heroes (hire second hero at castle) |

---

## Milestone 4 — Fog of War & Map Exploration

*(after canonical map and unit JSON are locked)*

- Unexplored tiles fully hidden, vision radius per hero (3 hex default)
- Mines / dungeons only visible once explored
- Enemy hero positions visible only within vision range
- Fog data stored in session save

---

## Milestone 5 — Campaign

- Act I: one mission per faction (tutorial-pace, guided objectives)
- Act II: three missions; faction paths diverge at end
- Act III: convergence mission — Tomb of Djangjix, SC-driven narrative finale
- Branching: linear within acts (WC3-style), choice at act boundary
- Campaign save separate from skirmish save

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
