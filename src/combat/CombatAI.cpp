#include "CombatAI.h"
#include "CombatEngine.h"
#include "CombatUnit.h"
#include <climits>
#include <iostream>

// ── Private helpers ───────────────────────────────────────────────────────────

int CombatAI::weakestTarget(const CombatEngine& engine, bool actorIsPlayer) {
    const CombatArmy& enemies = actorIsPlayer ? engine.enemyArmy()
                                              : engine.playerArmy();
    int bestIdx = -1;
    int lowestHp = INT_MAX;
    for (int i = 0; i < static_cast<int>(enemies.stacks.size()); ++i) {
        if (enemies.stacks[i].isDead()) continue;
        int hp = enemies.stacks[i].totalHp();
        if (hp < lowestHp) { lowestHp = hp; bestIdx = i; }
    }
    return bestIdx;
}

int CombatAI::nearestTarget(const CombatEngine& engine, bool actorIsPlayer,
                             HexCoord actorPos) {
    const CombatArmy& enemies = actorIsPlayer ? engine.enemyArmy()
                                              : engine.playerArmy();
    int bestIdx  = -1;
    int bestDist = INT_MAX;
    for (int i = 0; i < static_cast<int>(enemies.stacks.size()); ++i) {
        if (enemies.stacks[i].isDead()) continue;
        int d = actorPos.distanceTo(enemies.stacks[i].pos);
        if (d < bestDist) { bestDist = d; bestIdx = i; }
    }
    return bestIdx;
}

std::optional<HexCoord> CombatAI::bestMoveToward(const CombatEngine& engine,
                                                   HexCoord goalPos) {
    auto reachable = engine.reachableTiles();
    if (reachable.empty()) return std::nullopt;

    HexCoord best     = reachable[0];
    int      bestDist = best.distanceTo(goalPos);
    for (const auto& h : reachable) {
        int d = h.distanceTo(goalPos);
        if (d < bestDist) { bestDist = d; best = h; }
    }
    return best;
}

// ── takeTurn ─────────────────────────────────────────────────────────────────

void CombatAI::takeTurn(CombatEngine& engine) {
    if (engine.isOver()) return;

    const CombatUnit& actor      = engine.activeUnit();
    const bool        isPlayer   = actor.isPlayer;
    const HexCoord    actorPos   = actor.pos;

    // ── 1. Ranged: attack the weakest target ─────────────────────────────────
    if (actor.type->isRanged() && actor.shotsLeft > 0) {
        int idx = weakestTarget(engine, isPlayer);
        if (idx != -1) {
            engine.doAttack(idx);
            return;
        }
    }

    // ── 2. Find nearest enemy ─────────────────────────────────────────────────
    int nearIdx = nearestTarget(engine, isPlayer, actorPos);
    if (nearIdx == -1) { engine.doDefend(); return; }

    const CombatArmy& enemies = isPlayer ? engine.enemyArmy() : engine.playerArmy();
    HexCoord targetPos = enemies.stacks[nearIdx].pos;

    // ── 3. Adjacent: attack the weakest adjacent enemy ────────────────────────
    if (actorPos.distanceTo(targetPos) == 1) {
        // Among all adjacent enemies, pick the weakest.
        int attackIdx = -1;
        int lowestHp  = INT_MAX;
        for (int i = 0; i < static_cast<int>(enemies.stacks.size()); ++i) {
            if (enemies.stacks[i].isDead()) continue;
            if (actorPos.distanceTo(enemies.stacks[i].pos) != 1) continue;
            int hp = enemies.stacks[i].totalHp();
            if (hp < lowestHp) { lowestHp = hp; attackIdx = i; }
        }
        if (attackIdx != -1) { engine.doAttack(attackIdx); return; }
    }

    // ── 4. Move toward nearest enemy ─────────────────────────────────────────
    if (auto dest = bestMoveToward(engine, targetPos)) {
        engine.doMove(*dest);
        return;
    }

    // ── 5. Cornered / nothing useful ─────────────────────────────────────────
    engine.doDefend();
}
