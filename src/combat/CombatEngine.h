#pragma once
#include "CombatArmy.h"
#include "CombatEvent.h"
#include "CombatMap.h"
#include <random>
#include <vector>

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

    // True if target has friendly attackers on two opposite hex sides.
    // Pinned units take 150% damage and cannot retaliate.
    static bool isFlanked(const CombatUnit& target,
                          const std::vector<CombatUnit>& attackers);

    // ── Event queue ──────────────────────────────────────────────────────────
    // Returns all events produced since the last drain, then clears the queue.
    // Call once per update() tick; consume each event to drive animations.
    std::vector<CombatEvent> drainEvents();

    // ── Setup helpers (tests / scenario editor) ───────────────────────────────

    // Forcibly place a stack at a position after construction.
    // Useful in tests where placeArmies() needs to be overridden.
    void teleportUnit(bool isPlayer, int stackIdx, HexCoord pos);

    // ── SC branch choice ──────────────────────────────────────────────────────

    // True while waiting for the player to resolve a level-up branch choice.
    // Combat can finish, but XP awards and AI turns are paused until resolved.
    bool hasPendingChoice() const { return m_pendingChoice; }

    // Resolve the pending branch choice for the given SC stack.
    // Applies the chosen BranchOption's effects, records the choice on the unit,
    // then resumes any remaining level-up processing.
    // No-op if no choice is pending or the indices don't match.
    void resolveScChoice(bool isPlayer, int stackIdx, const std::string& branchId);

private:
    // Award XP to a SC stack; handles level-up logic and emits ScXpGained /
    // ScLevelUp events.  No-ops if the unit has no SC def or amount <= 0.
    void awardScXp(CombatUnit& unit, const TurnSlot& slot, int amount);

    // Process any pending level-ups for `unit`, driving the skill tree.
    // Emits ScLevelUp for each level gained; emits ScChoicePending and pauses
    // if a choice node is reached.
    void processLevelUps(CombatUnit& unit, const TurnSlot& slot);

    // Apply a list of NodeEffects directly to a CombatUnit.
    // stat_mod → bonus fields / scExtraStats;  unlock → scUnlocked;
    // unknown types → logged and skipped (open extension point).
    void applyNodeEffects(CombatUnit& unit, const std::vector<NodeEffect>& effects);

    // Returns true if `node` passes its requiresBranch gate for `unit`.
    bool nodePassesBranchGate(const SCLevelNode& node, const CombatUnit& unit) const;

    // Assign each stack its spawn position on the CombatMap.
    void placeArmies();

    // Build m_queue sorted by speed desc; player wins speed ties.
    void buildQueue();

    // Check if either side is fully dead and update m_result accordingly.
    void checkWinCondition();

    // HoMM3-style damage roll: sum rand(min,max) over each creature in attacker.
    static int calcDamage(const CombatUnit& attacker, const CombatUnit& defender,
                          std::mt19937& rng);

    // Cascade damage through the stack, decrementing count as creatures die.
    static void applyDamage(CombatUnit& target, int damage);


    CombatArmy            m_player;
    CombatArmy            m_enemy;
    CombatMap             m_map;
    std::vector<TurnSlot> m_queue;
    int                   m_turn  = 0;
    int                   m_round = 1;
    CombatResult          m_result = CombatResult::Ongoing;
    std::mt19937          m_rng;

    std::vector<CombatEvent> m_events;   // output queue; drained by CombatState

    // Pending branch choice state.
    bool m_pendingChoice         = false;
    bool m_pendingChoiceIsPlayer = false;
    int  m_pendingChoiceStackIdx = -1;
};
