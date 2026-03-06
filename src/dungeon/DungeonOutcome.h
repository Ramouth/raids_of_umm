#pragma once
#include "combat/CombatEvent.h"   // SurvivorStack
#include "entities/SpecialCharacter.h"
#include <string>
#include <vector>

/*
 * DungeonOutcome — result of a DungeonState session.
 *
 * Written in DungeonState::onExit() and read by AdventureState::onResume()
 * via the same shared_ptr handshake used by CombatOutcome.
 *
 * completed   : true iff the hero defeated the dungeon guard
 * survivors   : hero army state when the dungeon was exited (empty = defeated)
 * scRecruited : SCs who joined during the dungeon
 * itemsFound  : item IDs collected during the dungeon
 */
struct DungeonOutcome {
    bool                          completed   = false;
    std::vector<SurvivorStack>    survivors;
    std::vector<SpecialCharacter> scRecruited;
    std::vector<std::string>      itemsFound;
};
