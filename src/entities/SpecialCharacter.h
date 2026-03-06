#pragma once
#include "entities/UnitType.h"
#include "world/WondrousItem.h"
#include <string>
#include <array>

/*
 * SpecialCharacter — a named individual who can join the hero's party.
 *
 * SCs fight as solo units in combat (count=1 stack) using combatStats.
 * Each SC carries up to 4 equipment slots filled by EquipmentState (G1–G3).
 * Slot layout matches WondrousItem::slot values:
 *   [0] weapon   [1] armor   [2] amulet   [3] trinket
 *
 * Pure data — no GL, no SDL, no game logic.
 * Pointers in equipped[] point into ResourceManager's item registry.
 */
struct SpecialCharacter {
    std::string id;
    std::string name;
    std::string archetype;   // "tank", "healer", "glass_cannon"

    // Combat stats — used when the SC fights as a solo unit.
    UnitType combatStats;

    // Equipment slots — filled by EquipmentState
    std::array<const WondrousItem*, 4> equipped{};

    bool isEmpty() const { return id.empty(); }

    // Factory: create an SC with archetype-appropriate base stats.
    static SpecialCharacter make(const std::string& id_,
                                 const std::string& name_,
                                 const std::string& archetype_) {
        SpecialCharacter sc;
        sc.id        = id_;
        sc.name      = name_;
        sc.archetype = archetype_;

        sc.combatStats.id   = id_;
        sc.combatStats.name = name_;

        if (archetype_ == "tank") {
            sc.combatStats.hitPoints  = 40;
            sc.combatStats.attack     = 8;
            sc.combatStats.defense    = 14;
            sc.combatStats.minDamage  = 3;
            sc.combatStats.maxDamage  = 6;
            sc.combatStats.speed      = 3;
            sc.combatStats.moveRange  = 2;
        } else if (archetype_ == "healer") {
            sc.combatStats.hitPoints  = 25;
            sc.combatStats.attack     = 6;
            sc.combatStats.defense    = 8;
            sc.combatStats.minDamage  = 2;
            sc.combatStats.maxDamage  = 5;
            sc.combatStats.speed      = 5;
            sc.combatStats.moveRange  = 3;
        } else {  // glass_cannon (default)
            sc.combatStats.hitPoints  = 14;
            sc.combatStats.attack     = 16;
            sc.combatStats.defense    = 4;
            sc.combatStats.minDamage  = 6;
            sc.combatStats.maxDamage  = 14;
            sc.combatStats.speed      = 7;
            sc.combatStats.moveRange  = 4;
        }
        return sc;
    }
};
