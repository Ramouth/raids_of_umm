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

## Campaign arc — three stages, north to south

| Stage | Land | Status |
|-------|------|--------|
| **1 — The Northern Marches** | meadows, pine and oak forest, lakes, the Coldwater river, the Greyfang mountains | ◀ the demo (`data/maps/old_passage.*`, `scripts/gen_north_map.py`, `scripts/north_story.py`) |
| 2 — The Desert of Umm'Natur | sand, dunes, oases, ruins; the first desert draft | parked as `data/maps/desert_passage.*` |
| 3 — The Scorched Black Earth | burnt plains, obsidian, ash | ⬜ |

The old passage under the Greyfangs is the road from stage 1 to stage 2.
Map density for every stage follows HoMM3; see `docs/map_density.md`.

### Stage 1 — story (draft, see the story doc)
- **The families:** House Varen (the player) at Varenhold and House Hale (cousin Corvin) at Hallowmere hold the marches for the Ivory Compact.
- **The brother's fall:** Aldren rode east with Kharim's letter. His horse comes home on day 5, and his body is found on day 7, killed by crossbow bolts.
- **The ally:** Corvin sends spears and timber, lends Hallowmere, and gives the Hale signet.
- **The betrayal** (day 12+, after you have visited Hallowmere; day 16 at the latest): Corvin admits he killed Aldren and has sided with the Shariw. Hallowmere turns rival and the Hale household rides out.
- **The mythos, in layers:** four obelisks (inscription + Lore entry + rules out a false mine), relics (the Warm Stone, the Listening Shard), the Drowned King's barrow and crown. The deepest layer: the Veined, whose roads are the old passages. The journal (Q) has a **Lore** page.

### Stage 1 — HoMM3-style adventure objects (native, tested)
Resource piles, campfires, chests (gold **or** experience), windmills/watermills (weekly),
stables (+3 movement for the week), watchtowers (reveal radius 9), learning stones,
obelisks, creature dwellings (capture, recruit weekly), artifacts on the map, bridges,
lakes and swamps, and **guard zones of control** (walking next to a camp starts the fight;
routes go around camps when they can).

## Demo — "The Old Passage" (Godot) ◀ current focus

A 20–30 min handcrafted scenario that shows off the core loop:
explore → capture mines → recruit → grow SCs → find the passage.
Built in the Godot slice (`godot/`); turn/economy/fog logic moves into the native
GDExtension like CombatEngine did, so it stays covered by `raids_tests`.

### Locked decisions
- **Engine:** Godot. SDL build stays as reference/test host.
- **Player faction:** Ivory Compact (Gold + Wood emphasis).
- **Win:** find the old passage hidden in one of 3–4 **Old Mines**. The passage mine is
  picked at random from the candidates on each New Game. Finding it wins — no resource cost to open.
- **Lose:** hero's army destroyed, **or a Shariw hero finds the passage first**.
- **Rival:** one AI faction — the **Shariw** (locals who know the land; also hunting the passage).
- **Mine defence:** HoMM3-style garrisons — leave troops in a mine.
- **Mines = resources only (model A).** Units cost specialty resources, HoMM3-style;
  capturing a mine does not unlock units.
- **SC upkeep** is a deliberate exception to economy.md Q8 ("no unit upkeep").

### Map — three zones, gated by neutral guards
Radius ~13 (~547 cells, ~3× current). Linear west → east push.

| Zone | Contents | Guards |
|------|----------|--------|
| Home valley (west) | Start town Khemret, Gold Mine, Sawmill (oasis) | none — learn controls |
| **Ridge Pass** | mountain chokepoint | G1 — first real fight, ~day 3–4 |
| Contested middle | Quarry, Obsidian Vent, Tharakh (neutral town), Old Mines A + B, small dungeon (fetch item) | light/medium stacks |
| **Canyon Ford** | river/canyon chokepoint | G3 — hardest guard, needs T4–T5 |
| Far reach (east) | Crystal Cavern, Kharim's camp, Old Mines C + D, Shariw town | strong stacks |

Story regions (trigger targets): start, Ridge Pass sighted, first Old Mine sighted,
entering the far reach, wrong Old Mine cleared, passage found.

### Old Mines
Guarded; clearing one reveals **loot**, **a collapse/trap**, or **the passage**.
The quest giver sells clues that eliminate candidates — gold buys time.

### Army (Ivory Compact, T1–T5 for the demo — costs provisional)
| Tier | Unit | Cost driver |
|------|------|-------------|
| 1 | Levy Spearman | Gold |
| 2 | Desert Archer | Gold + Wood |
| 3 | Armoured Warrior | Gold + Stone |
| 4 | Rider Archer | Gold + Wood + Obsidian |
| 5 | Rider Knight | Gold + Stone + Obsidian |

Crystal is unused by the Compact's roster — it feeds high-level SC upkeep instead.

### Special Characters — "more and more special"
Levels add *rule-breaking* abilities, not just stats:

| Stage | Lv | Gains |
|-------|----|-------|
| Recruit | 1–2 | stat bumps, one basic action |
| Veteran | 3–4 | first branch choice + an adventure-map ability (e.g. +2 sight radius) |
| Champion | 5–7 | signature combat ability (double strike, revive a stack) |
| Legend | 8+ | changes how the game plays (cross mountains, double adjacent mine output) |

- **Stationing:** an SC can stay in a town — Governor (+income / cheaper recruits),
  Warden (defends the town), Mentor (recruits there gain XP). Role follows branch.
- **Upkeep:** gold/day rising with level (~100 → 300 → 600); Legend stage also costs
  1 Crystal/week. Same upkeep in town or in the field.
- **Unpaid:** loyalty drops — 3 days unpaid = abilities off, 7 days = SC leaves.
- Demo cast: Ushari (starts with hero), Kharim (quest giver — see below), + one found in a dungeon.

### Quest giver (proposal)
**Kharim** is out in the far reach studying the old maps. He's data-driven via
`data/quests.json` (condition + reward):
- **Tribute** — pay N gold → a clue (rules out one Old Mine)
- **Fetch** — bring back an item from the middle dungeon → a clue
- **Control** — hold 3 mines at once → Kharim joins as an SC

### Storytelling — Warcraft 3 style
WC3 is the reference for narrative delivery and hero-centric play.
- **In-mission transmissions** — portrait + dialogue line pops up while the player keeps
  control. Carries most of the story; long cutscenes are rare.
- **Trigger system** — per-map `triggers.json`: *event → condition → action*.
  Events: enter region, day N, capture object, combat won, item acquired, quest completed.
  Actions: show dialogue, reveal fog, add/complete quest, spawn stack, give item/resource, win/lose.
  Quests, clues and the win condition all run through it — new missions are mostly data.
- **Quest log** — main quest ("Find the old passage") + optional quests (Kharim's tasks),
  each announced with portrait + sound.
- **Guard stacks drop items** — neutral guards are WC3 creep camps: clearing one rewards, not just unblocks.
- **Short intro + outro** — camera pan + one dialogue exchange before control;
  a closing scene when the passage is found.
- **Map regions** — the map marks named regions for story beats
  ("entering the far reach", "first sight of an Old Mine").

### Rival — the Shariw (AI)
- **Town** in the far reach; capturing it is a major optional objective.
- **1–2 AI heroes**, priority list: defend town → grab weakly-held mines (incl. yours)
  → search unsearched Old Mines (the race) → hunt the player if clearly stronger → retreat to recruit.
- **Same rules as the player** — town recruits weekly, mines pay income. Difficulty via
  starting bonuses, not cheats.
- **Enemy turn** after End Day: visible moves animated, fogged moves instant.
- **Stolen mines** flip ownership + income next day, with a transmission
  ("Scouts report the Quarry has fallen to the Shariw").
- Fog needs 3 states (unseen / remembered / visible) — enemy heroes only shown while visible.
- Scripted escalation via triggers: day 3 scouts sighted, day 5 raid on the Sawmill,
  day 7 a Shariw hero enters the middle.
- *Optional:* The Veined as a scripted surprise in a far-reach Old Mine (triggers only, no second AI).

### Screens (Godot screen stack)
- **Root `Game` scene** owns session state (native `UmmAdventure`, army, inventory).
  Screens are pushed on top and pop with a result — the SDL `StateMachine` model.
- Screens: **Adventure** (stays loaded underneath), **Combat**, **Town** (HoMM3-style:
  buildings, recruit panel, garrison), later **Hero/Party**, **Main menu**.
- **Underground (after the demo):** maps gain levels (surface / underground), portals link
  hexes between levels, fog per level, adventure view shows one level at a time.
  Story hook: *the old passage is the way down.*

### Build order (self-verified by Claude in large batches; user beta-tests)

Story beats live in the Claude Doc "The Old Passage — Story Script" (symbol legend: 💬 dialogue, ⚔️ battle,
👁️ discovery, 📜 quest, 🔍 clue, 💰 reward, 🚩 capture, 🐍 rival, 🏆 victory, ☠️ defeat).
Balance tool: `godot/native/build/battle_sim data '<army>' '<guards>' [runs]`.
| # | Step | Status |
|---|------|--------|
| 1 | Map layout on paper (zones, objects, story regions) → author in editor → import to Godot | 🟡 draft generated (`scripts/gen_old_passage_map.py`), needs hand-tuning |
| 2 | Turns + movement points in Godot (native `AdventureSession` → `UmmAdventure`) | ✅ |
| 2b | Screen stack: root Game scene; combat moved onto it | ✅ |
| 3 | Trigger system (`triggers.json`) + dialogue/portrait panel | ✅ |
| 4 | Port fog of war to Godot | ✅ |
| 5 | Mine capture + daily income + neutral guard stacks with item drops | ✅ |
| 6 | Mine garrisons (HoMM3-style) | ✅ |
| 7 | Town recruitment with resource costs (Ivory Compact T1–T5 in units.json) | ✅ |
| 8 | SC stationing + upkeep + loyalty | 🟡 native done; Godot party screen next |
| 9 | Shariw rival: town, AI heroes, enemy turn, hero-vs-hero combat | ✅ |
| 10 | Old Mines + random passage + race-loss + win/lose + intro/outro scenes | ✅ (outro illustrations in progress) |
| 11 | Kharim + `quests.json` + quest log | ✅ (quests live in triggers.json) |
| 12 | SC stage abilities (Veteran map ability, Champion signature) | 🟡 adventure abilities done; SCs in combat ⬜ |
| 13 | PixelLab art: Old Mine, Kharim + Ushari portraits, Compact + Shariw units/heroes, new mine objects | 🟡 objects + units done (PixelLab + OpenAI) |
| 14 | **Stage 1 north:** northern map at HoMM3 density, sites, dwellings, zone of control, obelisks + lore codex, families/betrayal story, northern art | ✅ first pass (demo_bot 6/12 won, average day 40, so the race needs tuning) |
| 15 | **Combat AI + clarity:** scored AI (spreads out, flanks, seeded randomness), turn-order bar, hover damage/kill forecast, combat log, stack inspector | ✅ |
| 16 | **Move-and-attack in one turn** (HoMM3): melee walks up and strikes; cursor side picks the standing hex; decided 2026-09-24 | ✅ (demo_bot 6/12 won, 4 sealed, 2 timeout) |
| 17 | **Hero growth**: XP visible (✅ 7d69c03); spell tree, 1 point per level, no mana: **Command** (Banner aura, Wider Banner, Rally, Oathbound) · **Sand** (Displace, Quicksand, Stone Wall, Ward) · **Veined**, after 2 obelisks (Vein Sight, Grave Harvest, Listen Below, The Door). Designed 2026-09-24 | 🟡 designed |
| 17b | **Companions in battle: powerful but vulnerable**: single figure, high damage, low HP (Ushari 75 +10/level); troops deploy walling them in, front first, the source of an aura (Ushari +3 defence, radius 1); a stack beside them takes half of every melee blow; the enemy AI hunts them; red warning when they can be reached. Falls = wounded 3 days, lost if the battle is lost too | ✅ (demo_bot 6/12 won, same as before; ~2 wounds a game) |
| 17e | **Line of sight**: any stack between a shooter and its target halves the shot; the forecast and a drawn line of fire show it. Moving units (Displace, walls) will open or close lines | ✅ |
| 17g | **Reaction fire**: a shooter fires once per round at a stack that moves closer, before its strike lands; the AI picks volley-free approaches but never stalls | ✅ |
| 19 | **Town buildings** (HoMM3): one per town per day; Hall line (500/1000/2000 gold), Fort line (growth +50%/+100%), dwellings gate tiers 3–5, Marketplace, Drill Yard (+1 attack); Build and Market tabs | ✅ first pass (demo_bot builds nearly everything by day 40, so costs may need raising) |
| 17f | **Sand spells**: Displace, Quicksand, Stone Wall, Ward in the combat engine (hero tree + Sekhara/Khet) | ⬜ next |
| 17c | **Grave Harvest (necromancy, HoMM3 caveats)**: raise skeletons from living enemies only; cap 3 × hero level per battle; dead stack ≤ largest living stack; decay 10%/week unless paid Crystal; Ushari and northern towns object | ⬜ |
| 17d | **Baldur's Gate companions**: approval per companion (wages fold in), interjections at choice points, "X disapproves", warning scene, leaving on crossed lines, personal quests at high approval, camp banter | ⬜ |
| 18 | **Atmosphere beyond text**: obelisk sound sting + violet tint, ambient audio per region, slower "strange" transmissions, a visual for the Veined | ⬜ next |

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
