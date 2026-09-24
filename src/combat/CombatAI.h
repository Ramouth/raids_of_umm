#pragma once
#include "hex/HexCoord.h"
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
 * The engine's turn is move OR attack, so every candidate action ends the turn:
 *
 *   Attack  — strike an adjacent enemy, or shoot any non-adjacent one.
 *             value = threat removed from the target (its damage-per-turn ×
 *                     fraction of its HP destroyed, expected value of the real
 *                     damage formula) + bonus for wiping the stack out
 *                     − threat we lose to its retaliation.
 *   Move    — step to a reachable hex to set up next turn's attack.
 *             value = best (discounted) attack available from that hex,
 *                     where flanking positions (pinned: ×1.5, no retaliation)
 *                     score higher, targets already claimed by allies are
 *                     devalued (spreading), and hexes crowded by allies cost
 *                     a little
 *                     − first-strike danger: melee enemies next to the hex that
 *                       will strike before us (net of our retaliation).
 *   Defend  — hold position (same position value as staying, slightly less
 *             incoming damage).
 *
 * Danger weighting fades out over the rounds (and is off when only the other
 * side has shooters), so two cautious AIs can never stall a battle forever.
 *
 * Randomness: the best action is picked by a softmax over candidates within a
 * few percent of the top score, using engine.aiRng() — deterministic for a
 * given CombatEngine::setSeed(), different between battles otherwise.
 */
class CombatAI {
public:
    // Compute and execute the best single action for the current actor.
    // Caller is responsible for ensuring it is not the human player's turn
    // (or that auto-battle mode is active).
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
};
