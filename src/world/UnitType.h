#pragma once
#include "world/Resources.h"
#include <string>
#include <unordered_map>
#include <vector>

/*
 * AttackType — determines how the attacker's damage interacts with defender DEF.
 *
 *   Physical : normal ATK vs DEF formula  (defBypassRatio = 0.0)
 *   Piercing : arrows / barbs bypass some armor  (defBypassRatio typically 0.5)
 *   Magical  : ignores armor entirely  (defBypassRatio = 1.0)
 *
 * defBypassRatio (0.0–1.0) is loaded from JSON so it can be tuned per unit
 * without recompiling.  The engine computes:
 *   effectiveDef = effDef * (1.0 - defBypassRatio)
 */
enum class AttackType { Physical, Piercing, Magical };

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
    std::string faction;    // roster owner: "cruths" (the player), "shariw", "symmachy", "" = neutral

    // ── Combat stats ─────────────────────────────────────────────────────────
    int attack       = 1;   // Offensive power
    int defense      = 1;   // Damage reduction factor
    int minDamage    = 1;   // Damage range per unit in stack
    int maxDamage    = 1;
    int hitPoints    = 1;   // HP per individual creature
    int speed        = 4;   // Initiative/movement (higher = acts sooner/more)
    int moveRange    = 3;   // Hex tiles per combat turn
    int shots        = 0;   // 0 = melee only; >0 = ranged unit

    // ── Attack type ──────────────────────────────────────────────────────────
    AttackType attackType     = AttackType::Physical;
    float      defBypassRatio = 0.0f;  // 0.0 = none, 0.5 = half, 1.0 = full bypass

    // ── Growth & recruitment ─────────────────────────────────────────────────
    int weeklyGrowth = 0;   // Units added to town dwelling per week
    ResourcePool cost;      // Per-unit recruitment cost

    // ── Abilities (string tags, resolved by combat system) ───────────────────
    // Examples: "undead", "flying", "poison_strike", "no_retaliation"
    std::vector<std::string> abilities;

    // ── Companions (faction "companion") ────────────────────────────────────
    // Aura: living friendly stacks within auraRadius hexes (not the source)
    // gain auraDefense.  levelGrowth: gained per companion level above 1.
    // title: the epithet shown under the name ("Keeper of the old ways").
    // upkeep: the wage at level 1 in gold per day (100 = the standard table,
    // AdventureSession::upkeepFor; it scales with level the same way). 0 = family
    // or household: no wage, never sulks, never leaves for want of pay.
    // lines: what the companion says, by moment: "join" (riding in beside
    // others; none = silent), "unpaid", "paid", "leaves" (unpaid a week),
    // "wounded". A missing moment gets AdventureSession's neutral default.
    std::string title;
    int upkeep = 100;
    std::unordered_map<std::string, std::string> lines;
    int auraRadius  = 0;
    int auraDefense = 0;
    struct LevelGrowth { int hitPoints = 0, attack = 0, defense = 0, damage = 0; };
    LevelGrowth levelGrowth;

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
    bool isCompanion() const { return faction == "companion"; }

    int avgDamage() const { return (minDamage + maxDamage) / 2; }
};
