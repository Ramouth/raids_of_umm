#pragma once
#include "hex/HexCoord.h"
#include "entities/UnitType.h"
#include "entities/SpecialCharacter.h"
#include <string>
#include <vector>
#include <memory>

/*
 * SCProgressUpdate — SC state snapshot written into CombatOutcome and
 * DungeonOutcome so callers can sync the hero's SpecialCharacter after combat.
 *
 * level/xp/unlockedActions reflect the SC's state at end-of-battle.
 * Callers compute the level delta and apply stat growth to combatStats
 * using the SCDef growth tables.
 */
struct SCProgressUpdate {
    std::string              scId;
    int                      level = 1;
    int                      xp    = 0;
    std::vector<std::string> unlockedActions;
};

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
    std::vector<SurvivorStack> survivors;          // living player stacks after battle
    std::vector<std::string>   itemsFound;         // item IDs awarded on PlayerWon
    std::vector<SpecialCharacter> scFound;         // SCs who join hero on PlayerWon
    std::vector<SCProgressUpdate> scUpdates;       // SC progression earned this battle
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
 *   UnitAttacked — isPlayer, stackIndex, targetIsPlayer, targetIndex,
 *                  isRetaliation, wasFlanked
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
        ScXpGained,      // isPlayer, stackIndex, xpAmount
        ScLevelUp,       // isPlayer, stackIndex, newLevel
        ScChoicePending, // isPlayer, stackIndex, choiceLevel, branchOptions
    };

    Type         type           = Type::BattleEnded;

    // Subject (the unit performing the action)
    bool         isPlayer       = false;
    int          stackIndex     = -1;

    // Target (UnitAttacked only)
    bool         targetIsPlayer  = false;
    int          targetIndex     = -1;
    bool         isRetaliation   = false;  // true when this attack is a counter-attack
    bool         wasFlanked      = false;  // true when target was pinned (flanking bonus applied)

    // Damage amount (UnitDamaged only)
    int          damage         = 0;

    // Movement endpoints (UnitMoved only)
    HexCoord     from;
    HexCoord     to;

    // Battle outcome (BattleEnded only)
    CombatResult result         = CombatResult::Ongoing;

    // SC progression (ScXpGained / ScLevelUp only)
    int          xpAmount       = 0;   // XP awarded this event
    int          newLevel       = 0;   // level reached (ScLevelUp)

    // Branch choice (ScChoicePending only)
    int                      choiceLevel  = 0;  // level at which the choice occurs
    std::vector<BranchOption> branchOptions;    // options for the player to choose from
};
