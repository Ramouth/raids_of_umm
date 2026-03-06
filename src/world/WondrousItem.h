#pragma once
#include <string>
#include <vector>

/*
 * ItemEffect — one passive bonus/penalty entry on a WondrousItem.
 *
 * Mirrors SpellEffect in spirit: pure data, no logic.
 * Interpretation lives in CombatEngine when Phase E lands.
 *
 * stat examples: "attack", "defense", "damage", "speed", "move"
 */
struct ItemEffect {
    std::string stat;
    int         amount = 0;   // positive = bonus, negative = penalty
};

/*
 * WondrousItem — immutable data describing an item, loaded from items.json.
 *
 * Heroes carry const WondrousItem* in their inventory.
 * Loaded once at startup by ResourceManager; never duplicated.
 *
 * Phase E will add:
 *   std::vector<HexCoord> pattern;   // relative hex offsets for geometric AoE
 *   std::string           useAction; // "damage_aoe", "heal_aoe", "buff_aoe"
 *   int                   usePower;  // magnitude of the use action
 */
struct WondrousItem {
    std::string id;
    std::string name;
    std::string description;
    std::string slot;          // "amulet", "weapon", "armor", "trinket"
    bool        cursed = false;

    std::vector<ItemEffect> passiveEffects;
};
