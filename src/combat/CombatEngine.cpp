#include "CombatEngine.h"
#include <algorithm>
#include <iostream>

CombatEngine::CombatEngine(CombatArmy player, CombatArmy enemy)
    : m_player(std::move(player))
    , m_enemy(std::move(enemy))
    , m_rng(std::random_device{}())
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

// ── Actions ───────────────────────────────────────────────────────────────────

void CombatEngine::doAttack(int targetIndex) {
    if (isOver()) return;

    TurnSlot& slot = m_queue[m_turn];
    CombatUnit& attacker = slot.isPlayer ? m_player.stacks[slot.stackIndex]
                                         : m_enemy.stacks[slot.stackIndex];
    auto& enemyStacks = slot.isPlayer ? m_enemy.stacks : m_player.stacks;
    CombatUnit& target = enemyStacks[targetIndex];

    // Determine if ranged: unit has shots, has ammo, and target is not adjacent
    bool isRanged = attacker.type.isRanged()
                    && attacker.shotsLeft > 0
                    && attacker.pos.distanceTo(target.pos) > 1;
    if (isRanged) {
        --attacker.shotsLeft;
    }

    int damage = calcDamage(attacker, target, m_rng);
    applyDamage(target, damage);

    std::cout << "[CombatEngine] " << attacker.type.name
              << " attacks " << target.type.name
              << " for " << damage << " damage"
              << " (" << target.count << " survivors)\n";

    // Melee retaliation: target is alive, hasn't retaliated, attacker has no "no_retaliation"
    if (!isRanged && !target.isDead() && !target.hasRetaliated
        && !attacker.type.hasAbility("no_retaliation")) {
        int retDamage = calcDamage(target, attacker, m_rng);
        applyDamage(attacker, retDamage);
        target.hasRetaliated = true;
        std::cout << "[CombatEngine] " << target.type.name
                  << " retaliates for " << retDamage << " damage"
                  << " (" << attacker.count << " survivors)\n";
    }

    advance();
}

// static
int CombatEngine::calcDamage(const CombatUnit& attacker, const CombatUnit& defender,
                              std::mt19937& rng) {
    std::uniform_int_distribution<int> dist(attacker.type.minDamage, attacker.type.maxDamage);
    int baseDmg = 0;
    for (int i = 0; i < attacker.count; ++i)
        baseDmg += dist(rng);

    int effectiveDefense = defender.isDefending
        ? defender.type.defense + defender.type.defense / 4
        : defender.type.defense;

    int diff = attacker.type.attack - effectiveDefense;
    double mult;
    if (diff >= 0)
        mult = 1.0 + 0.05 * std::min(diff, 20);
    else
        mult = std::max(0.3, 1.0 + 0.025 * diff);

    return std::max(1, static_cast<int>(baseDmg * mult));
}

// static
void CombatEngine::applyDamage(CombatUnit& target, int damage) {
    while (damage > 0 && !target.isDead()) {
        if (damage >= target.hpLeft) {
            damage -= target.hpLeft;
            target.count--;
            target.hpLeft = (target.count > 0) ? target.type.hitPoints : 0;
        } else {
            target.hpLeft -= damage;
            break;
        }
    }
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

        for (auto& s : m_player.stacks) { s.isDefending = false; s.hasRetaliated = false; }
        for (auto& s : m_enemy.stacks)  { s.isDefending = false; s.hasRetaliated = false; }

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
