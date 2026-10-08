# The magic system must be built (2026-10-08)

Decided by the user after rabies went in (commit 1f1ce78): **we need the magic system to be
built.** No new abilities, spells or cures are added ad hoc until it exists.

## Why now

- Rabies added disease as a concept. Its only cure is hard-coded in `endDay`: rest a day in a
  town you hold.
- The user wants Ushari to cure rabies. There is no place to put that except another
  hard-coded line, which is the pattern the user asked us to stop.
- The inventory (`docs/abilities_inventory.md`) shows the abilities spread over four separate
  places. 13 unit abilities are described but do nothing. Spells exist in `data/spells.json`
  with mana costs, but nothing casts them.

## What the system has to cover (from the inventory; the design is the user's call)

1. **Statuses as data:** what each one does, how long it lasts and what cures it. Rabies (bitten and mad), weakened, bound, wounded and the poison/curse tags all go here.
2. **One ability definition** shared by units, companions, hero path nodes and items, instead of unit tags, the C++ companion list and per-name bonus code.
3. **Active abilities and spells** with a cost (mana, cooldown or both), on the map and in battle. Today the only active action is Ushari's Binding Thread, wired up by its name.
4. **Fixes found along the way:**
   - "curse" means four different things;
   - Ancient Guardian has `no_retaliation_cap`, but the code checks `no_retaliation`;
   - Maerwen's Heron's Patience is defined twice, once as text and once as a battle tag.

## First use

Ushari cures rabies: the first ability built on the system, not before it.

## Status

Not started. The next step is a design proposal for the user to decide on. It comes after the
user's Chapter 1 smoke test.
