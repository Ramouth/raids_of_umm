#include "CombatEngine.h"
#include <algorithm>
#include <iostream>

CombatEngine::CombatEngine(CombatArmy player, CombatArmy enemy)
    : m_player(std::move(player))
    , m_enemy(std::move(enemy))
{
    placeArmies();
    buildQueue();
    std::cout << "[CombatEngine] Round 1 start — "
              << m_queue.size() << " stacks in initiative order\n";
}

// ── Queries ───────────────────────────────────────────────────────────────────

const CombatUnit& CombatEngine::activeUnit() const {
    const TurnSlot& slot = m_queue[m_turn];
    return slot.isPlayer ? m_player.stacks[slot.stackIndex]
                         : m_enemy.stacks[slot.stackIndex];
}

std::vector<HexCoord> CombatEngine::reachableTiles() const {
    if (isOver()) return {};

    const CombatUnit& unit = activeUnit();
    std::vector<HexCoord> result;

    for (HexCoord c : CombatMap::allHexes()) {
        if (c == unit.pos) continue;
        if (unit.pos.distanceTo(c) > unit.type.moveRange) continue;

        // Not occupied by a friendly stack
        const auto& allies = unit.isPlayer ? m_player.stacks : m_enemy.stacks;
        bool blocked = false;
        for (const auto& ally : allies)
            if (!ally.isDead() && ally.pos == c) { blocked = true; break; }

        if (!blocked) result.push_back(c);
    }
    return result;
}

std::vector<HexCoord> CombatEngine::attackableTiles() const {
    if (isOver()) return {};

    const CombatUnit& unit = activeUnit();
    const auto& enemies = unit.isPlayer ? m_enemy.stacks : m_player.stacks;

    std::vector<HexCoord> result;

    if (unit.type.isRanged()) {
        // Ranged: can target any living enemy on the field
        for (const auto& e : enemies)
            if (!e.isDead()) result.push_back(e.pos);
    } else {
        // Melee: only enemies adjacent to the active unit
        for (const auto& e : enemies)
            if (!e.isDead() && unit.pos.distanceTo(e.pos) == 1)
                result.push_back(e.pos);
    }
    return result;
}

// ── Movement ──────────────────────────────────────────────────────────────────

bool CombatEngine::canMoveTo(HexCoord dest) const {
    if (isOver()) return false;
    const auto reachable = reachableTiles();
    for (const auto& h : reachable)
        if (h == dest) return true;
    return false;
}

void CombatEngine::doMove(HexCoord dest) {
    TurnSlot& slot = m_queue[m_turn];
    CombatUnit& actor = slot.isPlayer ? m_player.stacks[slot.stackIndex]
                                      : m_enemy.stacks[slot.stackIndex];
    std::cout << "[CombatEngine] " << actor.type.name
              << " moves " << actor.pos.q << "," << actor.pos.r
              << " → " << dest.q << "," << dest.r << "\n";
    actor.pos = dest;
    advance();
}

bool CombatEngine::doAttackAt(HexCoord targetHex) {
    if (isOver()) return false;

    const CombatUnit& actor = activeUnit();
    auto& enemies = actor.isPlayer ? m_enemy.stacks : m_player.stacks;

    for (int i = 0; i < static_cast<int>(enemies.size()); ++i) {
        if (!enemies[i].isDead() && enemies[i].pos == targetHex) {
            doAttack(i);
            return true;
        }
    }
    return false;
}

// ── Actions (stubs) ───────────────────────────────────────────────────────────

void CombatEngine::doAttack(int targetIndex) {
    (void)targetIndex;
    std::cout << "[CombatEngine] " << activeUnit().type.name
              << " attacks (stub — damage resolution not yet implemented)\n";
    advance();
}

void CombatEngine::doDefend() {
    TurnSlot& slot = m_queue[m_turn];
    CombatUnit& actor = slot.isPlayer ? m_player.stacks[slot.stackIndex]
                                      : m_enemy.stacks[slot.stackIndex];
    actor.isDefending = true;
    std::cout << "[CombatEngine] " << actor.type.name << " defends\n";
    advance();
}

void CombatEngine::doRetreat() {
    std::cout << "[CombatEngine] Player retreats\n";
    m_result = CombatResult::Retreated;
}

// ── Turn advancement ──────────────────────────────────────────────────────────

void CombatEngine::advance() {
    if (isOver()) return;

    checkWinCondition();
    if (isOver()) return;

    ++m_turn;

    if (m_turn >= static_cast<int>(m_queue.size())) {
        // All living stacks have acted — start next round
        ++m_round;
        m_turn = 0;

        for (auto& s : m_player.stacks) s.isDefending = false;
        for (auto& s : m_enemy.stacks)  s.isDefending = false;

        buildQueue();
        std::cout << "[CombatEngine] --- Round " << m_round << " ---\n";
    }
}

// ── Private ───────────────────────────────────────────────────────────────────

void CombatEngine::placeArmies() {
    auto pSpawns = CombatMap::playerSpawns();
    auto eSpawns = CombatMap::enemySpawns();

    for (size_t i = 0; i < m_player.stacks.size() && i < pSpawns.size(); ++i)
        m_player.stacks[i].pos = pSpawns[i];

    for (size_t i = 0; i < m_enemy.stacks.size() && i < eSpawns.size(); ++i)
        m_enemy.stacks[i].pos = eSpawns[i];
}

void CombatEngine::buildQueue() {
    m_queue.clear();

    for (int i = 0; i < static_cast<int>(m_player.stacks.size()); ++i)
        if (!m_player.stacks[i].isDead())
            m_queue.push_back({ true, i });

    for (int i = 0; i < static_cast<int>(m_enemy.stacks.size()); ++i)
        if (!m_enemy.stacks[i].isDead())
            m_queue.push_back({ false, i });

    // Higher speed acts first; player wins ties
    std::stable_sort(m_queue.begin(), m_queue.end(),
        [&](const TurnSlot& a, const TurnSlot& b) {
            const CombatUnit& ua = a.isPlayer ? m_player.stacks[a.stackIndex]
                                              : m_enemy.stacks[a.stackIndex];
            const CombatUnit& ub = b.isPlayer ? m_player.stacks[b.stackIndex]
                                              : m_enemy.stacks[b.stackIndex];
            if (ua.type.speed != ub.type.speed)
                return ua.type.speed > ub.type.speed;
            return a.isPlayer && !b.isPlayer;
        });
}

void CombatEngine::checkWinCondition() {
    if (m_enemy.allDead()) {
        m_result = CombatResult::PlayerWon;
        std::cout << "[CombatEngine] " << m_player.ownerName << " wins!\n";
    } else if (m_player.allDead()) {
        m_result = CombatResult::EnemyWon;
        std::cout << "[CombatEngine] " << m_enemy.ownerName << " wins!\n";
    }
}
