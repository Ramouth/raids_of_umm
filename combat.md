# Raids of Umm'Natur — Combat System Design

## Core philosophy

Every action in combat answers four questions:

```
WHO acts?        → Initiative  (speed stat, modified by equipment)
WHERE can it go? → Movement    (moveRange minus terrain cost)
WHAT can it hit? → Reach       (attackRange, attack type)
HOW MUCH?        → Damage      (formula driven by atk/def/type triangle)
```

Everything else — flanking, terrain, items — is a modifier to one of these four.
The goal is a system where a single JSON edit changes how combat feels, with no recompile.

---

## Attack type triangle

Three attack types. Each has a specific counter and a specific bypass.

| Type         | Countered by              | Bypasses          | Example unit         |
|--------------|---------------------------|-------------------|----------------------|
| **Physical** | Armor (defense stat)      | —                 | Warrior, Mummy       |
| **Piercing** | Partial armor only        | 50% of defense    | Archer, Scorpion     |
| **Magical**  | Resistance (separate stat)| All armor         | Spell-caster SC      |

Implication: stacking heavy armor is a strong counter to warriors but largely wasted against archers.
Equipment choices become meaningful because the enemy roster matters.

---

## Ranged combat rules

- Ranged units attack from a distance defined by `attackRange` (e.g. 3–4 hexes).
- **No counter-attack** if the attacker is outside the defender's own attack range.
- A melee unit reaching a ranged unit in close combat forces melee exchange (normal counter rules apply).
- Optional knob: `rangedPenaltyPerHex` — damage fall-off per extra hex beyond range 1.
  Set to 0.0 in the base config; raise it to reward close positioning.

---

## Terrain

Two effects per tile: how hard it is to enter, and how safe it is to stand on.

```
moveCost     → movement points spent to enter the tile
defenseBonus → flat defense bonus while a unit occupies the tile
```

### Dungeon tile set

| Terrain         | Move cost | Defense bonus | Design intent                              |
|-----------------|-----------|---------------|--------------------------------------------|
| Sand (open)     | 1         | 0             | Default floor — no friction                |
| Rubble          | 2         | +1            | Slows advance; mild cover                  |
| Wall fragment   | impassable| —             | Forces pathing around; creates chokepoints |
| **High Ground** | 1         | +2            | Strong sniper perch (see below)            |
| Pit             | 2         | -1            | Dangerous to hold; punishes static play    |

### High Ground — a real trade-off

High Ground gives the best defense bonus (+2) and costs nothing to enter.
That makes it a prime position — but there are natural limiters:

- **Scarcity**: only 1–3 high ground hexes per dungeon layout.
  Both sides race to claim them.
- **Exposure**: a unit on high ground is visible and predictable.
  The enemy AI will prioritise eliminating it.
- **Ranged synergy**: a ranged SC on high ground fires without counter-attack
  AND gets +2 defense. This is intentional — it rewards planning your SC loadout
  around the dungeon layout.
- **Melee counter**: a melee unit that reaches high ground also gets +2 defense,
  so an enemy who charged up to dislodge your archer now has cover too.
  Reclaiming high ground is a multi-turn commitment.

Possible future trade-off to prototype: **High Ground costs 2 to enter** (climbing),
but only 1 to leave. Creates a "last one up loses the race" dynamic.
Leave this as a tuning knob: `highGroundEnterCost` (default 1, prototype with 2).

---

## SC equipment — what each slot does

Equipment is **SC-only**. Regular unit stacks use their base stats.

```
Weapon  → sets attackRange and attackType, overrides the SC's archetype defaults
           Unarmed SC uses archetype base stats.
           Example: "Longbow"       → range 4, piercing
           Example: "Obsidian Blade"→ range 1, physical, +3 damage

Armor   → modifies defense; may also modify moveCost
           Example: "Scale Mail"    → +3 defense, -1 move
           Example: "Sand Cloak"    → +1 defense, ignore Rubble move penalty

Amulet  → passive stat bonus (existing system)
           Stats: attack, defense, damage, speed, resistance

Trinket → passive OR active (one use per combat)
           Passive example: "Serpent Band" → +1 speed always
           Active example:  "War Horn"     → once per combat, all friendlies +2 speed this round
```

The weapon slot replaces the SC's attack profile rather than stacking on top.
This makes weapon choice a meaningful decision rather than always "equip the best number."

---

## CombatConfig — the tuning knobs

All constants live in `data/combat_config.json`. No recompile to change combat feel.

```json
{
  "atkDefSlope":              0.05,
  "flankDamageMultiplier":    1.5,
  "flankPinsCounter":         true,
  "rangedCounterAttack":      false,
  "rangedPenaltyPerHex":      0.0,
  "piercingBypassFraction":   0.5,
  "highGroundEnterCost":      1,
  "terrainMoveCosts":         { "sand": 1, "rubble": 2, "pit": 2 },
  "terrainDefenseBonuses":    { "sand": 0, "rubble": 1, "high_ground": 2, "pit": -1 }
}
```

---

## Beta-test configuration (first locked integration)

A specific set of content to play-test the system against before expanding it.

| Element         | Beta config                                                              |
|-----------------|--------------------------------------------------------------------------|
| Unit roster     | Warrior (melee/physical), Archer (ranged/piercing), Mummy (melee/physical)|
| SC archetypes   | Tank (high defense), Glass Cannon (high attack, low defense)             |
| Item set        | Longbow (weapon), Scale Mail (armor), Scarab Amulet (amulet), War Horn (trinket active) |
| Dungeon layout  | Sand floor + Rubble clusters + 2 High Ground hexes + 1 Wall fragment     |
| Starting config | atkDefSlope=0.05, piercingBypass=0.5, rangedPenalty=0, highGroundEnterCost=1 |

Ten fights with this config. Questions to answer:
- Does positioning matter? (Would you rather be on high ground than attack this turn?)
- Does the item choice feel meaningful? (Is Longbow obviously better than Obsidian Blade, or does it depend?)
- Does the type triangle show up? (Is Scale Mail useless when facing archers?)

---

## Open questions

### 1. Resistance stat
Magical defense is separate from armor defense. Add it now so SC spell-caster archetypes
have a full stat set from day one, or defer until a magic SC actually exists?

**Risk of deferring:** adding a new defensive stat later requires touching the damage formula,
all unit data files, and the equipment system at once.

### 2. Active trinket
A "use item" action needs a slot in the combat turn menu (alongside Move / Attack / Defend / Retreat).
Is this in the first implementation pass or the second?

### 3. Line of sight for ranged
Can an archer shoot through allied or enemy units?

- **No LOS (simplest):** shoot anything in range, regardless of what's between.
- **Blocked by walls only:** HoMM3 rule. Walls and impassable tiles block; units don't.
- **Fully blocked by units:** adds real blocking/shielding play but complicates the AI.

### 4. SC death
If a SC dies in combat, is it:

- **Permanent** — high stakes, equipment lost too. Makes cursed items genuinely scary.
- **Revives at 1 HP after combat** — forgiving; lets the player keep progressing.
- **Revives only if the battle is won** — middle ground.

This decision affects how much risk the player takes when equipping a SC with a cursed item
and charging them into high-ground fire. Lean toward permanent death for the beta test —
it makes every equipment decision carry real weight.

### 5. High Ground enter cost
Default: 1 (free to climb). Prototype value: 2 (costs a move point to ascend).
Test both in the beta. The "race to high ground" dynamic only appears if entering costs something.
