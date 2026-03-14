# Raids of Umm'Natur — Roadmap

## Completed

- [x] Hex grid, camera, world builder
- [x] Adventure map — hero movement, pathfinding, object visits
- [x] Combat system — initiative queue, BFS movement, attack/defend/retreat
- [x] Combat AI — ranged priority, melee fallback, auto-battle toggle
- [x] Dungeon state — guard combat, SC recruitment, loot drop
- [x] Turn manager — day/week cycle, movement reset, income
- [x] Resource tracking — gold + sand crystals in treasury
- [x] Special Character system — XP, levelling, stat growth
- [x] SC skill tree — branching level-up choices, branch-gated nodes
- [x] Combat result overlay (dungeon + direct combat)
- [x] Party screen (F key) — view hero army and SC slots
- [x] Unique SC per dungeon (Ushari at Tomb of Kha'Set, Sekhara at Buried Sanctum)

---

## In Progress

*(nothing currently in flight)*

---

## Next — Stable Test Environment

The current test path (WorldBuilder → P → random procedural map) generates a
different world each run, causing ghost objects and non-reproducible bugs.
Fix: author a canonical map, load it the same way every time.

### 1. Session Save / Load
- Save format: single JSON file containing map path + session state
  - Hero: pos, army slots, SC slots, move points
  - Object control: guardDefeated flags, ownership per hex
  - Turn manager: day number, treasury
- Triggered by Ctrl+S / Ctrl+L in AdventureState
- WorldMap serialisation already exists; session layer wraps it

### 2. Main Menu (MainMenuState)
- Replaces WorldBuilder as the default entry point
- Four actions: **New Game**, **Continue**, **World Builder**, **Quit**
- New Game: picks a map file (start with hardcoded default map path)
- Continue: loads last session save if one exists
- Same HUD-drawn button style as rest of UI (no art needed yet)

### 3. Author the Canonical Test Map
- Design a fixed map in WorldBuilder, save it as `data/maps/default.json`
- All future playtesting uses this map via the menu
- Eliminates random-map ghost behaviour

---

## Backlog (priority order)

4. **Commander aura mechanic** — passive ring buff to adjacent friendly stacks
5. **AoE special attack** — engine targeting + UI highlight
6. **Push / Telekinesis** — move enemy unit on attack
7. **Unit & spell JSON data** — move hardcoded stats out of C++ into `data/`
8. **Castle / Town building tree** — recruit higher-tier units, build structures
9. **Win condition** — clear all dungeons to win
10. **Multiple dungeon types** — vary guard composition and SC per dungeon template
11. **Visual / UI overhaul** — integer scaling FBO, SpriteBatcher, pixel-art sprites
    - SC branch-choice overlay is a thin placeholder designed for this pass
12. **Save/Load slot UI** — multiple named saves, timestamp display

---

## Deferred / Nice-to-Have

- Fog of war
- Hero experience and levelling (separate from SC system)
- Multiplayer / hotseat
- Sound effects and music
- Procedural map generator with authored constraints (not raw random)
