#pragma once
#include "CombatUnit.h"
#include <vector>
#include <string>

/*
 * CombatArmy — a named collection of creature stacks entering a battle.
 *
 * Pure data.  CombatEngine owns two of these and never exposes mutable
 * references outside itself.
 */
struct CombatArmy {
    std::string             ownerName;
    std::vector<CombatUnit> stacks;
    bool                    isPlayer = false;

    bool allDead() const {
        for (const auto& s : stacks)
            if (!s.isDead()) return false;
        return true;
    }
};
