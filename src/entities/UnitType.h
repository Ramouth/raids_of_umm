#pragma once
#include "world/Resources.h"
#include <string>
#include <vector>

/*
 * UnitType — immutable data describing a unit species loaded from units.json.
 *
 * All CombatUnit and Army slots point to a const UnitType*.
 * Never duplicated — loaded once at startup by ResourceManager.
 */
struct UnitType {
    // ── Identity ─────────────────────────────────────────────────────────────
    std::string id;          // JSON key, e.g. "mummy"
    std::string name;        // Display name, e.g. "Mummy"
    std::string description;
    int         tier = 1;   // 1–7 (HoMM3-style)

    // ── Combat stats ─────────────────────────────────────────────────────────
    int attack       = 1;   // Offensive power
    int defense      = 1;   // Damage reduction factor
    int minDamage    = 1;   // Damage range per unit in stack
    int maxDamage    = 1;
    int hitPoints    = 1;   // HP per individual creature
    int speed        = 4;   // Initiative/movement (higher = acts sooner/more)
    int moveRange    = 3;   // Hex tiles per combat turn
    int shots        = 0;   // 0 = melee only; >0 = ranged unit

    // ── Growth & recruitment ─────────────────────────────────────────────────
    int weeklyGrowth = 0;   // Units added to town dwelling per week
    ResourcePool cost;      // Per-unit recruitment cost

    // ── Abilities (string tags, resolved by combat system) ───────────────────
    // Examples: "undead", "flying", "poison_strike", "no_retaliation"
    std::vector<std::string> abilities;

    // ── Visual ───────────────────────────────────────────────────────────────
    std::string meshId;     // ResourceManager key for the 3D mesh
    std::string textureId;  // ResourceManager key for albedo texture

    // Helpers
    bool hasAbility(const std::string& tag) const {
        for (const auto& a : abilities)
            if (a == tag) return true;
        return false;
    }
    bool isRanged()   const { return shots > 0; }
    bool isUndead()   const { return hasAbility("undead"); }
    bool isFlying()   const { return hasAbility("flying"); }

    int avgDamage() const { return (minDamage + maxDamage) / 2; }
};
