# Raids of Umm'Natur — Roadmap

> **Guiding principle:** Always have a playable vertical slice.
> Each milestone produces something you can actually run and feel.
> Gameplay first — visual polish deliberately last.

---

## Vision

**Six factions race to claim the Crown of Pha'raxh** — an artifact that commands
all Djinns, sealed inside a fallen empire's capital. Two win conditions: military
elimination or assembling the Eye of Djangjix shards to unlock the vault.

See `Factions.md` for full faction design and lore.
See `economy.md` for economy decisions and implementation plan.

---

## Completed

- [x] Hex grid, camera, world builder (save/load map, F1 alignment editor)
- [x] Adventure map — hero movement, pathfinding, object visits, turn system
- [x] Combat — initiative queue, BFS movement, click-move/attack, flanking/pinned
- [x] Combat AI — ranged priority, melee fallback, auto-battle toggle
- [x] Attack-type triangle — Physical / Piercing (50% bypass) / Magical (full bypass); data-driven per unit
- [x] Dungeon state — guard combat, SC recruitment, loot drop, result overlay
- [x] Town / Castle — unit recruitment, weekly growth, treasury
- [x] Special Character system — XP, levelling, stat growth
- [x] SC skill tree — branching level-up choices, branch-gated nodes, combat overlay
- [x] Party screen (F key) — view hero army and SC slots
- [x] Wondrous items — data layer, ResourceManager, hero inventory, equipment screen
- [x] Unique SC per dungeon (Ushari / Sekhara)
- [x] Session save / load — Ctrl+S / Ctrl+L, single JSON (hero, army, SCs, objectControl, treasury, day)
- [x] Main menu — New Game / Continue / World Builder / Quit; Continue greyed when no save exists
- [x] ESC exit prompt — Save & Exit / Exit / Cancel (optional save on leaving adventure)
- [x] Animated sprites — AnimatedSprite + clip state machine across adventure, dungeon, combat
- [x] Canonical map — `data/maps/default.json` with 2 towns, 2 dungeons, 2 gold mines, 1 crystal mine, 1 artifact
- [x] Hero spawns at Khemret (west starting town), not map origin
- [x] Faction lore — 6 factions fully designed (Factions.md)
- [x] Economy design — resources, income, building tree approach, Djinn Manifest system (economy.md)
- [x] **Milestone 1 — Economy Foundation** — 5 resources (Gold/Wood/Stone/Obsidian/Crystal), mine income wired, town passive income +500g/day, Sawmill/Quarry/ObsidianVent on map, all unit costs remapped
- [x] **Road logic** — `MapTile.road` flag, moveCost halved (0.5), visual stone-block texture (PixelLab), 9-tile route Khemret → Tharakh
- [x] **Fog of War** — BFS radius-4 visibility, black void / dark shroud / full-visible render, progressive reveal per step, objects hidden in fog (owned assets always shown), persisted in session save

---

## Milestone 2 — Combat Depth

These must be validated with **heuristic calculations** before building on top of them.
Target: each match-up produces fights that last 8–15 rounds; no unit is obviously broken.

| # | Item | Status |
|---|------|--------|
| 1 | **Unit ability tags** | Wire `undead`, `poison_strike`, `curse_strike`, `raise_dead`, `flying`, `consumption` into engine | ⬜ |
| 2 | **Combat simulation harness** | Headless N-vs-M sim script — DPS/survivability tables per matchup | ⬜ |
| 3 | **Economy balance sheet** | Income curve vs unit cost; time-to-tier-5 per faction; validated against sim | ⬜ |
| 4 | **Commander aura** | Passive buff to adjacent stacks (Ushari / Kharim) | ⬜ |
| 5 | **ZoC (Zone of Control)** | Heavy units lock adjacent enemies — moving past triggers free attack | ⬜ |
| 6 | **Terrain in combat** | Rubble / High Ground / Pit tiles with move cost + def modifiers | ⬜ |
| 7 | **SC permadeath** | SC dies in battle → items drop, removed from hero roster permanently | ⬜ |

---

## Milestone 3 — Dungeon Narrative

Each dungeon tells a self-contained story with SC interaction.

| # | Item |
|---|------|
| 1 | Dungeon event sequence — enter → narration panel → optional SC dialogue → encounter |
| 2 | SC fear / refusal system — certain SCs refuse specific dungeons |
| 3 | Cutscene overlay — text + portrait, ESC to skip |
| 4 | Per-dungeon loot table and guard variant tied to dungeon template JSON |
| 5 | Eye shard as collectible dungeon reward (tracks toward Crown win) |

---

## Milestone 4 — Faction System & Win Conditions

| # | Item |
|---|------|
| 1 | Six factions defined in JSON with unit rosters, resource costs, building trees |
| 2 | Faction selection on New Game |
| 3 | Win condition: **Military** (destroy all enemy towns) |
| 4 | Win condition: **Crown of Pha'raxh** (assemble Eye shards → enter vault → claim Crown) |
| 5 | Faction-specific unit unlocks (building tree gating wired into CastleState) |
| 6 | Multiple heroes (hire second hero at castle) |
| 7 | **Djinn Manifest** — Manifest Fragments, Ritual Town, summoning action |
| 8 | **Sylvan Host corruption mechanic** — SC corruption arc, ability shift |

---

## Milestone 5 — Map & Exploration Polish

- Enemy hero AI — basic wander/capture/attack loop
- **Iron Conclave advantage** — partial fog-of-war reveal in ruins terrain
- More mine types on default map; second dungeon variant
- Fog-of-war sight radius tied to hero stats (upgradeable)

---

## Milestone 6 — Campaign

- Act I: one mission per faction (tutorial-pace, guided objectives)
  - Ivory Compact: march toward Pha'raxh, first contact with The Veined
  - Shariw: defend ancestral territory as outside factions pour in
  - Djinn Courts: race to the Crown before it falls into mortal hands
  - Sylvan Host: enter the ruins, first corruption beat
  - Iron Conclave: reach the city before anyone starts asking questions
  - The Veined: surface for the first time; reclaim what was always yours
- Act II: faction paths collide over dungeon control; SC fear events begin
- Act III: convergence — Pha'raxh, the vault, the Crown. Faction-driven finale.
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
| 6 | Full unit roster sprites (all 42 units across 6 factions) |
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
| road terrain tile | 128×128 | ✅ |
| Ushari, Sekhara SC portraits | 128×128 | ⬜ |
| new mine type objects (sawmill, quarry, obsidian vent) | 64×64 | ⬜ |
| unit sprites — all 6 faction rosters | 128×128 | ⬜ |
