#pragma once
#include "hex/HexCoord.h"
#include "entities/UnitType.h"
#include <string>
#include <vector>
#include <memory>

/*
 * CombatResult — outcome of a finished battle.
 * Defined here (rather than CombatEngine.h) so CombatEvent can reference it
 * without a circular include.
 */
enum class CombatResult { Ongoing, PlayerWon, EnemyWon, Retreated };

/*
 * CombatOutcome — written by CombatState::onExit(), read by the caller via
 * a shared_ptr passed into CombatState at construction.
 *
 * survivors: one entry per living player stack (type pointer + count remaining).
 * The caller uses this to sync the hero's army after battle.
 */
struct SurvivorStack {
    const UnitType* type  = nullptr;
    int             count = 0;
};

struct CombatOutcome {
    CombatResult               result    = CombatResult::Ongoing;
    std::vector<SurvivorStack> survivors;  // living player stacks after battle
    std::vector<std::string>   itemsFound; // item IDs awarded on PlayerWon (C3 fills this)
};

/*
 * CombatEvent — a single observable state change produced by CombatEngine.
 *
 * CombatEngine appends events to an internal queue during every action.
 * CombatState drains the queue each update() and plays one animation per
 * event before advancing to the next.  This decouples game-logic timing
 * from render timing and makes every new action type animatable for free.
 *
 * Field usage by Type:
 *   UnitMoved    — isPlayer, stackIndex, from, to
 *   UnitAttacked — isPlayer, stackIndex, targetIsPlayer, targetIndex
 *   UnitDamaged  — isPlayer, stackIndex, damage
 *   UnitDied     — isPlayer, stackIndex
 *   UnitDefended — isPlayer, stackIndex
 *   BattleEnded  — result
 */
struct CombatEvent {
    enum class Type {
        UnitMoved,
        UnitAttacked,
        UnitDamaged,
        UnitDied,
        UnitDefended,
        BattleEnded,
    };

    Type         type           = Type::BattleEnded;

    // Subject (the unit performing the action)
    bool         isPlayer       = false;
    int          stackIndex     = -1;

    // Target (UnitAttacked only)
    bool         targetIsPlayer = false;
    int          targetIndex    = -1;

    // Damage amount (UnitDamaged only)
    int          damage         = 0;

    // Movement endpoints (UnitMoved only)
    HexCoord     from;
    HexCoord     to;

    // Battle outcome (BattleEnded only)
    CombatResult result         = CombatResult::Ongoing;
};
