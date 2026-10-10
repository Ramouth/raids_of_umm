#pragma once
#include "hex/HexCoord.h"
#include <optional>
#include <string>
#include <vector>

class CombatEngine;

/*
 * CombatAI — scored-heuristic driver for any combat stack.
 *
 * Works for enemy turns, friendly auto-battle (TAB / Godot "Auto-battle") and
 * headless auto-resolve (AdventureSession::autoBattle, battle_sim, demo_bot).
 * Depends only on CombatEngine's public interface — no SDL, no GL.
 *
 * Rules (HoMM3): a melee stack may walk and strike in one turn; shooters with
 * ammo shoot or move.  Every candidate ends the turn:
 *
 *   Attack  — one candidate per (standing hex, target): shoot from here,
 *             strike from here, or walk to any legal hex next to the target
 *             and strike.  value = threat removed from the target (its
 *             damage-per-turn × fraction of its HP destroyed, expected value
 *             of the real damage formula, exact pinning for that hex) + bonus
 *             for wiping the stack − threat lost to retaliation, plus the
 *             prospects of the hex we end on minus our exposure there.
 *   Move    — walk without striking: best (discounted) attack set up for next
 *             turn — flanking hexes score higher, targets already claimed by
 *             allies are devalued (spreading), crowded hexes cost a little —
 *             minus exposure.
 *   Defend  — hold position (defence bonus lowers exposure); never preferred
 *             over an available strike.
 *
 * Exposure = expected strikes from melee foes that act before our next turn
 * and can reach the hex (move + attack), net of our retaliation.  Its weight
 * fades out by round 4 (and is off when only the other side has shooters), so
 * the weaker side may hold for a round or two but never stalls a battle.
 *
 * Companions (single figures with an aura) are valued kCompanionValue× their
 * damage: the AI hunts the other side's and shields its own.  Shots are
 * valued through line of sight (a blocked shot does half damage).  Every walk
 * is charged for the reaction shots it would draw (CombatEngine::reactionsTo).
 *
 * Breaking through: a stack that cannot reach any enemy at all, because
 * obstacles wall it in (or wall the enemy in), attacks the first obstacle on
 * the cheapest way to the nearest enemy (each obstacle costs the turns needed
 * to break it), walking up to it first, instead of idling (breachTarget).
 *
 * Reckless (a unit with the "reckless" ability, UnitType::isReckless): no
 * scoring. It strikes the nearest enemy it can reach this turn; otherwise it
 * follows the cheapest way to the nearest enemy (the same costs as breaking
 * through), hacking at the first obstacle on it when that is the cheaper way,
 * else walking as far along it as it can. CombatSession plays such a stack's
 * turns itself even in the player's army: the player cannot hold it back.
 *
 * Randomness: the best action is picked by a softmax over candidates within a
 * few percent of the top score, using engine.aiRng() — deterministic for a
 * given CombatEngine::setSeed(), different between battles otherwise.
 */
class CombatAI {
public:
    // Compute and execute the best single action for the current actor.
    // Caller is responsible for ensuring it is not the human player's turn
    // (or that auto-battle mode is active). A mad stack (rabies) is always
    // played here, whichever side it is on: CombatEngine::doMadTurn.
    static void takeTurn(CombatEngine& engine);

    // One scored option, exposed for tests and debugging.
    struct Candidate {
        enum class Kind { Attack, Move, Defend };
        Kind     kind   = Kind::Defend;
        int      target = -1;   // enemy stack index (Attack)
        HexCoord hex;           // destination (Move)
        double   score  = 0.0;
    };

    // All candidates for the current actor with their scores (unsorted).
    static std::vector<Candidate> scoreActions(const CombatEngine& engine);

    // The candidate takeTurn() would choose (uses and advances engine.aiRng()).
    static Candidate chooseAction(CombatEngine& engine);

    // The obstacle the active stack should break, or nullopt while some enemy
    // can be attacked or walked to around the obstacles (stacks are ignored:
    // they move). See "Breaking through" above.
    static std::optional<HexCoord> breachTarget(const CombatEngine& engine);
};
