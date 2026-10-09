# Abilities, skills and effects: what exists (2026-10-08)

Gathered before designing a skills system, so new abilities stop being ad hoc. Facts from the
code at commit 1f1ce78; line numbers drift.

## Four separate mechanisms today

1. **Unit tags** (`data/units.json` `abilities`): free strings. The battle engine reads some of them, hard-coded by name.
2. **Companion map abilities**: a C++ list keyed by companion id (`Specials.cpp:22` `abilitiesOf`).
3. **Hero path tree** (`data/hero_tree.json`): three paths of nine nodes each, with data-driven `effect` keys.
4. **Buildings and items**: stat bonuses summed into one army-wide bonus (`AdventureSession.cpp` `armyBonus`).

Not wired up: `data/spells.json` (5 spells, nothing casts them, no mana), `data/artifacts.json`
(no loader), and the legacy `SCDef` tree (old SDL path only).

## 1. Unit tags

**Read by the code (8):**

| Tag | Units | Effect |
|---|---|---|
| lone_wolf | woad_runner, painted_blade | +25% damage with no friendly stack adjacent |
| strike_and_return | woad_runner, gododdin_rider | after walk-and-strike, returns to its start hex |
| renown / great_renown | painted_blade, gododdin_rider / teulu | +2 / +4 attack per enemy stack destroyed, this battle |
| binding_thread | ushari | curse: 2 turns weakened (x0.75 damage); from level 3 also bound (-2 speed, -2 move) |
| witch_knife | ushari | her melee knife |
| opportunity_strike | maerwen | strikes an enemy leaving her reach, once per round |
| rabid | rabid_hound | 10% bite per landed hit on player troops (new) |

**In the engine but no unit has them:** no_retaliation, readied_shot, no_melee_penalty.

**In the data but the code never reads them (13):** undead, flying, curse_strike,
poison_strike, magic_resistance, large_creature, raise_dead, aura_of_decay, incorporeal,
chain_lightning, companion. The unit descriptions promise their effects; in play they do
nothing.
- **Bug:** ancient_guardian has `no_retaliation_cap`, but the code checks `no_retaliation`, so the guardian does retaliate.

**Unit fields that act like abilities:** attack type and armour bypass, aura (Ushari: +3 defence
to adjacent stacks), companion level growth.

## 2. Hero paths

- **Marshal, Siegemaster, Quartermaster:** 9 nodes each, levels 2 to 10. The effect keys are fieldworks, tactics, attack, defense, moves, sight, gold, growth and readied_shot.
- **Veined wildcard path:** 4 nodes, still without effects.

## 3. Companions

| Companion | Map abilities (code) | Battle |
|---|---|---|
| Ushari | L1 Drillmaster +1 move, L3 Eagle Eye +2 sight, L5 Warlord +50% guard gold, L8 Legend of the Sands +2 move | Binding Thread, knife, +3 defence aura, bodyguard |
| Kharim | L1 Old Maps +1 sight, L3 Surveyor +1 move, L5 Deep Reading +2 sight | none |
| Maerwen | L1 Heron's Patience (text only; the effect is her battle tag), L3 Lowland Roads +1 move | opportunity_strike |
| Aldren | none | none |

- Companion map abilities lapse when the companion is wounded (3 days), unpaid for 3 days, or stationed in a town.
- There is no active map ability anywhere: no "use" command, no cooldown, no mana.

## 4. Items

- 16 items. Their effects are attack, defense and speed only, and they apply to troops, not companions.
- 3 are cursed and cannot be removed: Cursed Eye of Set, Drowned Crown, Warm Stone.
- A Warm Seed has no effect.
- No item can grant an ability.

## 5. Statuses

**Battle:** weakened, bound, defending, flanked (from position), engaged shooter, renown, aura,
bitten, mad.

**Map:**
- **Bitten:** cured by ending a day in a town you hold. This is hard-coded in `endDay`.
- **Wounded companion:** lasts 3 days.
- **Unpaid companion:** after 3 days they sulk; after 7 they leave.
- These three statuses have three separate cure rules, and none of them is in the data.

## Overlaps

- **Army stat buffs:** come from path nodes, buildings, items and morale, all summed together.
- **Readied shot:** has three sources: the Siegemaster path, the Marksmen's Tower and the unit tag.
- **Moves and sight:** come from path nodes, companions, stables and items. Each is listed by name in C++.
- **"Curse" means four things:** Binding Thread, cursed items, the `curse_strike` tag and the spell `curse_of_khepri`.
- **Poison and curse:** both exist as tags that do nothing, next to Ushari's weaken, which works.

## Where "Ushari cures rabies" could live

- **Today:** the only hook is the companion map-ability list (`abilitiesOf` / `hasAbility`). It could make the town cure also work wherever Ushari is. That is one more hard-coded line, which is the ad hoc pattern to avoid.
- **Missing for a real system:**
  - a data record for a status: its name, what it does and what cures it;
  - an active ability with a cost or cooldown, on the map or in battle;
  - a single place that defines an ability as data, shared by units, companions, path nodes and items.

## Added since (2026-10-09)

- Drest (companion, `drest`): defence aura +2, radius 1 (same mechanic as Ushari's). Battle tips in `data/combat_tips.json`.
- `feral_dog` (1.0, no abilities: the sickness is text only) and `northern_warrior` (1.2, no abilities).
- Keepsake items (`kind: keepsake`): readable, no effects; first is the Dog Tag.
- Story conditions are one system now (`docs/story_conditions.md`); ability conditions should reuse it.
