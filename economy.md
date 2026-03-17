# Raids of Umm'Natur — Economy Design

> Status: **Decisions locked below** — ready for implementation planning.
> Purpose: define the economy system before any code changes.

---

## Locked Decisions

### Q1 — Resources: 5 common, natural names ✅

Five resources. Names are geological/natural — valid for any faction on any biome.

| Resource  | Thematic role                    | Produced by       |
|-----------|----------------------------------|-------------------|
| **Gold**  | Universal currency               | Gold Mine         |
| **Wood**  | Construction, siege engines      | Sawmill / Forest  |
| **Stone** | Fortification, heavy units       | Quarry            |
| **Obsidian** | Refined weapons, desert glass | Obsidian Vent     |
| **Crystal** | Magical, arcane components     | Crystal Cavern    |

Old names (BoneDust, DjinnEssence, AncientRelic, MercuryTears, Amber) are retired.
Faction flavour comes from *which* resources their units/buildings cost, not from resource names.

**Faction–resource identity:**

| Faction         | Primary   | Secondary |
|-----------------|-----------|-----------|
| Scarab Lords    | Wood      | Obsidian  |
| Tomb Dynasties  | Stone     | Crystal   |
| Djinn Courts    | Crystal   | Obsidian  |

Djinn Courts and Scarab Lords both want Obsidian → designed map conflict.
Tomb Dynasties and Djinn Courts both want Crystal → designed map conflict.
Gold is universal; every faction needs it.

---

### Q2 — Income curve target ✅

| Milestone          | Target turn |
|--------------------|-------------|
| Full T1 + T2 stack | Turn 7 (end week 1) |
| First T4/5 unit    | Turn 21 (end week 3) — requires holding a specialty mine |
| T7 unit available  | Turn 35–42 — requires dominant map control |
| Eye assembly       | Turn 35–45 (per Factions.md) |
| Military win       | Turn 30–50 (per Factions.md) |

---

### Q3 — Town passive income: flat +500 gold/day ✅

Every owned town generates +500 gold/day from day 1, no buildings required.
This is the economic backbone of the early game — identical to HoMM3's Village Hall.
Upgradeable when the building tree lands (Milestone 3).

---

### Q4 — Specialty resource acquisition ✅

**Primary source: mines on the map.**
Each specialty resource has a dedicated capturable mine type.
The canonical map needs 1–2 of each mine type in contested zones.

| Mine Type       | Daily Output | Resource  |
|-----------------|-------------|-----------|
| Gold Mine       | +1,000/day  | Gold      |
| Sawmill         | +2/day      | Wood      |
| Quarry          | +2/day      | Stone     |
| Obsidian Vent   | +1/day      | Obsidian  |
| Crystal Cavern  | +1/day      | Crystal   |

**Secondary source: dungeon loot.**
Small amounts of specialty resources as dungeon rewards (1–3 units).
Supplements mines; cannot replace them as a build-order backbone.

---

### Q5 — Building tree: data-only now, wired in Milestone 3 ✅

Author full `buildings.json` with costs, prerequisites, unit unlocks.
CastleState does not enforce unlock gates until Milestone 3 (factions land).
All units remain recruitable in the interim — no throwaway work.

---

### Q6 — Sand Wraith T1 Crystal cost: intentional ✅

Sand Wraith is a Djinn Courts faction unit. Its Crystal cost is the design signal
that Djinn Courts players must prioritise a Crystal Cavern early.
*(Note: DjinnEssence renamed to Crystal — update units.json accordingly.)*

---

### Q7 — Starting resources ✅

| Resource  | Starting amount |
|-----------|-----------------|
| Gold      | 10,000          |
| Wood      | 5               |
| Stone     | 5               |
| Obsidian  | 2               |
| Crystal   | 2               |

Gold mine rate stays at 1,000/day — matches HoMM3, no change needed.

---

### Q8 — No unit upkeep ✅

Aligned with HoMM3. Gold sinks: building construction, unit recruitment, hero hiring.
No per-turn army cost.

---

## The Djinn Manifest System

### Concept

The Djinn is not recruited — it is *summoned*. It functions as a legendary Special
Character available to any faction, not a unit in the army roster.
Unit rework (Sand Wraith, Djinn, Storm Caller as recruitable units) is deferred
until factions are decided.

### Manifest Fragments

A small number of **Manifest Fragments** are placed on the map — in high-tier
dungeons, guarded by the strongest enemies in the game. Fragments have no other
use; picking one up signals your intent to everyone.

**Suggested count:** 3 fragments per standard map (requires clearing 3 dungeons).

### The Ritual Town

One town on the map is designated the **Ritual Site** — a neutral or contested
special location. To summon the Djinn, a player must:

1. Hold all 3 Manifest Fragments (carried by their hero)
2. Own or occupy the Ritual Town
3. Perform the ritual (a town action, not an instant)

The Djinn SC manifests **at the Ritual Town**, bound to the hero who performed
the ritual. It cannot be transferred.

### Strategic tensions

- **Last-mile exposure:** you are most vulnerable at the moment you are most
  powerful — your hero must stand in one town with all fragments to complete
  the ritual.
- **Ritual Town as a fourth objective:** players can hold/siege the Ritual Town
  specifically to deny the summon, regardless of the Eye race or military position.
- **Cross-faction power:** any faction can summon the Djinn. A Tomb Dynasties
  player fielding a Djinn SC is a late-game swing no one can ignore.
- **One per game:** fragments are scarce and the ritual requires all of them.
  Only one player completes the summon per match.

### What the Djinn SC looks like (placeholder — full design deferred)

Extremely powerful, one-of-a-kind. Not a stack — a single entity with SC-style
abilities (levelled, with a branch skill tree). Probably the strongest individual
unit in the game. Acts as a force multiplier for whichever army it joins.

---

## What Needs Updating in Code / Data

| Item | File | What to change |
|------|------|---------------|
| Rename resources | `src/world/Resources.h` | Gold, Wood, Stone, Obsidian, Crystal + update enum |
| Update unit costs | `data/units.json` | Replace DjinnEssence→Crystal, BoneDust→Stone, AncientRelic→Obsidian, Amber→Obsidian or Crystal |
| Mine income | `data/buildings.json` | Add Sawmill, Quarry, Obsidian Vent, Crystal Cavern income |
| Starting gold | `src/core/TurnManager.cpp` | `init()` default from 2,000 → 10,000 |
| Starting resources | `src/core/TurnManager.cpp` | Seed Wood=5, Stone=5, Obsidian=2, Crystal=2 |
| Town income | `src/core/TurnManager.cpp` | `applyMineIncome` or new `applyTownIncome` — +500/day per owned town |
| Map objects | `src/world/MapObject.h` | Add Sawmill, Quarry, ObsidianVent, CrystalCavern to ObjType enum |
| Canonical map | `data/maps/default.json` | Place new mine types in contested zone |
| Manifest Fragment | `src/world/MapObject.h` | New ObjType::ManifestFragment |
| Ritual Town | `data/maps/default.json` | Designate one town as ritual site |

---

## HoMM3 Reference (for calibration)

| Metric | HoMM3 Normal | Raids of Umm (target) |
|--------|--------------|-----------------------|
| Starting gold | 20,000 | 10,000 (smaller map) |
| Starting specialty | 4× each (6 types) | 2–5× each (4 types) |
| Mine daily output (basic) | +2 wood / +2 ore | +2 wood / +2 stone |
| Mine daily output (rare) | +1 each magical | +1 obsidian / +1 crystal |
| Town income floor | +500g/day (Village Hall) | +500g/day (flat) |
| Town income ceiling | +4,000g/day (Capitol) | TBD — Milestone 3 |
| Time to T7 (contested) | Week 4–6 | Week 5–6 |
| Unit upkeep | None | None |
| Building tree depth | 10–15 per town | Full data, gating in M3 |
