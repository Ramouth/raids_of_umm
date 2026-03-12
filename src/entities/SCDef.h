#pragma once
#include <string>
#include <vector>

/*
 * SCDef — static definition for a Special Character's progression.
 *
 * Pure data — no methods, no GL, no SDL.
 * Instances are hardcoded here and looked up by id via findSCDef().
 * Will move to data/sc_defs.json + ResourceManager in a future pass.
 *
 * xpThresholds: cumulative XP required to reach each level beyond 1.
 *   len = maxLevel - 1.  e.g. {15, 35} means level 2 at 15 XP, level 3 at 35.
 *
 * Stat growth is applied once per level-up to the SC's CombatUnit bonuses
 * (during combat) and to SpecialCharacter::combatStats (after combat, via
 * outcome sync in AdventureState/DungeonState::onResume).
 */
struct SCAbilityDef {
    int         level;        // unlocks when reaching this level
    std::string key;          // matches arena action_key
    std::string name;
    std::string description;
};

struct SCDef {
    std::string id;
    std::string name;
    std::string flavour;

    int maxLevel;
    std::vector<int> xpThresholds;  // len = maxLevel - 1; cumulative

    int perTurnXp;     // XP at the start of this SC's every turn
    int killBonusXp;   // bonus XP per enemy stack killed
    int levelUpHeal;   // flat HP restored on level-up (capped at current max)

    // Per-level-up stat deltas applied to CombatUnit bonuses (during combat)
    // and to combatStats (after combat).
    int attackGrowth  = 0;
    int defenseGrowth = 0;
    int maxHpGrowth   = 0;
    int speedGrowth   = 0;

    std::vector<SCAbilityDef> abilities;
};

// ── Hardcoded SC definitions ───────────────────────────────────────────────────

inline const SCDef& ushariDef() {
    static const SCDef d = {
        "ushari",
        "Ushari",
        "Blade Dancer of the Crimson Dunes. She does not wait for enemies to come to her.",
        3,
        { 15, 35 },
        2, 6, 15,
        /*attackGrowth=*/1, 0, 8, 0,
        {
            { 2, "double_strike", "Double Strike",
              "Attack your target and one adjacent enemy." },
            { 3, "sweeping_arc",  "Sweeping Arc",
              "Strike every enemy in the ring around you." },
        }
    };
    return d;
}

inline const SCDef& khetDef() {
    static const SCDef d = {
        "khet",
        "Khet",
        "Sand Warden. She does not fight the desert — she IS the desert.",
        4,
        { 12, 28, 50 },
        2, 6, 12,
        0, /*defenseGrowth=*/1, 6, 0,
        {
            { 2, "mend",      "Mend",      "Restore HP to an adjacent friendly unit." },
            { 2, "quicksand", "Quicksand", "Mark a hex as difficult terrain." },
            { 4, "ward",      "Ward",      "Seal a hex — no unit may enter it." },
        }
    };
    return d;
}

inline const SCDef& sekharaDef() {
    static const SCDef d = {
        "sekhara",
        "Sekhara",
        "Flame Architect. Every wall she raises is a question: how will you answer it?",
        5,
        { 12, 26, 46, 72 },
        2, 6, 10,
        0, 0, 5, 0,
        {
            { 2, "displace",   "Displace",   "Push or pull an enemy up to 2 hexes." },
            { 3, "firewall",   "Firewall",   "Lay a 3-hex line of fire." },
            { 4, "stone_wall", "Stone Wall", "Raise a 2-hex impassable wall." },
        }
    };
    return d;
}

// Returns a pointer to the matching SCDef, or nullptr if unknown.
inline const SCDef* findSCDef(const std::string& id) {
    if (id == "ushari")  return &ushariDef();
    if (id == "khet")    return &khetDef();
    if (id == "sekhara") return &sekharaDef();
    return nullptr;
}
