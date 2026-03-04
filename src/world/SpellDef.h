#pragma once
#include <string>
#include <vector>

/*
 * SpellEffect — one effect entry from a spell's "effects" array.
 *
 * Kept intentionally minimal: only the fields the current JSON uses.
 * When a SpellEngine is added, richer interpretation lives there —
 * not here. This struct is pure data, no logic.
 */
struct SpellEffect {
    std::string type;      // "damage", "heal", "buff", "debuff", "summon"
    std::string stat;      // for buff/debuff: "attack", "defense", "damage_roll"
    int         amount   = 0;    // for numeric buff/debuff
    int         duration = 0;    // turns, 0 = instant
    std::string unitId;          // for summon
    std::string formula;         // formula string, e.g. "spellpower * 5"
    std::string mode;            // for buff mode, e.g. "maximize"
};

/*
 * SpellDef — immutable data describing a spell loaded from spells.json.
 *
 * All Hero spellbooks and spell UI reference const SpellDef*.
 * Loaded once at startup by ResourceManager; never duplicated.
 */
struct SpellDef {
    std::string id;
    std::string name;
    std::string school;      // "earth", "air", "necromancy", …
    int         level    = 1;
    int         manaCost = 0;
    std::string targeting;   // "enemy_unit", "friendly_unit", "area", "corpse"
    int         radius   = 0;
    std::string description;

    std::vector<SpellEffect> effects;
};
