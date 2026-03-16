#pragma once
#include "world/UnitType.h"
#include "hex/HexCoord.h"
#include <vector>

/*
 * RecruitResult — units purchased during a single CastleState visit.
 *
 * Written by CastleState, read by AdventureState::onResume().
 * Shared via shared_ptr using the same pattern as CombatOutcome.
 *
 * townCoord: identifies the source town so AdventureState can refund the
 * recruit pool (and treasury) if addUnit() fails because the army is full.
 */
struct RecruitResult {
    struct Stack {
        const UnitType* type  = nullptr;
        int             count = 0;
    };
    HexCoord           townCoord;  // set by AdventureState before pushing CastleState
    std::vector<Stack> hired;
};
