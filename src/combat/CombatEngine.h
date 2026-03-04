#pragma once
#include "CombatArmy.h"
#include "CombatMap.h"
#include <vector>

/*
 * CombatResult — outcome of a finished battle.
 */
enum class CombatResult { Ongoing, PlayerWon, EnemyWon, Retreated };

/*
 * TurnSlot — identifies which stack in which army is currently acting.
 */
struct TurnSlot {
    bool isPlayer;
    int  stackIndex;   // index into CombatArmy::stacks
};

/*
 * CombatEngine — pure combat logic.
 *
 * Owns two armies and a CombatMap.  Drives turn order via an initiative
 * queue sorted by UnitType::speed.  Places units on spawn hexes at startup.
 * Provides reachable- and attackable-hex queries for highlighting.
 *
 * Action methods are stubs for this pass; damage resolution and ability
 * tags will be added in the next iteration.
 *
 * No GL, no SDL, no render types — fully unit-testable via raids_tests.
 */
class CombatEngine {
public:
    CombatEngine(CombatArmy player, CombatArmy enemy);

    // ── Queries ──────────────────────────────────────────────────────────────

    CombatResult      result()      const { return m_result; }
    bool              isOver()      const { return m_result != CombatResult::Ongoing; }
    const TurnSlot&   currentTurn() const { return m_queue[m_turn]; }
    const CombatArmy& playerArmy()  const { return m_player; }
    const CombatArmy& enemyArmy()   const { return m_enemy; }
    const CombatMap&  map()         const { return m_map; }
    int               roundNumber() const { return m_round; }

    // Returns the unit currently acting.
    const CombatUnit& activeUnit() const;

    // Hexes the active unit can move to this turn (within moveRange, not occupied by friendlies).
    std::vector<HexCoord> reachableTiles()  const;

    // Hexes that contain an enemy adjacent to the active unit (melee),
    // or all living enemy hexes (ranged).
    std::vector<HexCoord> attackableTiles() const;

    // ── Movement ─────────────────────────────────────────────────────────────

    // True if dest is within the active unit's moveRange, in bounds, and not
    // blocked by a friendly stack.
    bool canMoveTo(HexCoord dest) const;

    // Move the active unit to dest and advance the turn.
    // Only call after canMoveTo returns true.
    void doMove(HexCoord dest);

    // ── Actions (stubs — damage resolution added in next pass) ───────────────

    // Attack target stack in the opposing army by index.
    void doAttack(int targetIndex);

    // Find the enemy stack standing on targetHex and attack it.
    // Returns true if an enemy was found and attacked.
    bool doAttackAt(HexCoord targetHex);

    // Take a defensive stance (sets isDefending; damage bonus applied during resolve).
    void doDefend();

    // Immediately end combat with the player retreating.
    void doRetreat();

    // Advance to the next actor; rebuilds the queue when a round ends.
    void advance();

private:
    // Assign each stack its spawn position on the CombatMap.
    void placeArmies();

    // Build m_queue sorted by speed desc; player wins speed ties.
    void buildQueue();

    // Check if either side is fully dead and update m_result accordingly.
    void checkWinCondition();

    CombatArmy            m_player;
    CombatArmy            m_enemy;
    CombatMap             m_map;
    std::vector<TurnSlot> m_queue;
    int                   m_turn  = 0;
    int                   m_round = 1;
    CombatResult          m_result = CombatResult::Ongoing;
};
