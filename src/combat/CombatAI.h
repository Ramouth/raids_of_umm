#pragma once
#include "hex/HexCoord.h"
#include <optional>

class CombatEngine;

/*
 * CombatAI — pure logic AI driver for any combat unit.
 *
 * Works for both enemy and friendly (auto-battle) turns.
 * Depends only on CombatEngine's public interface — no SDL, no GL.
 *
 * Decision tree per turn:
 *   1. Ranged unit with ammo  → attack weakest living enemy
 *   2. Melee unit, adjacent   → attack weakest adjacent enemy
 *   3. Melee unit, not adjacent → move to reachable hex closest to nearest enemy
 *   4. Nothing useful to do   → defend
 *
 * "Nearest" is used for move targeting so the unit advances naturally.
 * "Weakest" (lowest totalHp) is used for attack selection to maximize kills.
 */
class CombatAI {
public:
    // Compute and execute the best single action for the current actor.
    // Caller is responsible for ensuring it is not the human player's turn
    // (or that auto-battle mode is active).
    static void takeTurn(CombatEngine& engine);

private:
    // Index of the living enemy with the lowest totalHp.  -1 if none.
    static int weakestTarget(const CombatEngine& engine, bool actorIsPlayer);

    // Index of the living enemy closest to actorPos.  -1 if none.
    static int nearestTarget(const CombatEngine& engine, bool actorIsPlayer,
                             HexCoord actorPos);

    // Among reachableTiles(), the hex that minimises distance to goalPos.
    // Returns nullopt if there are no reachable tiles.
    static std::optional<HexCoord> bestMoveToward(const CombatEngine& engine,
                                                   HexCoord goalPos);
};
