#include "CombatEngine.h"
#include <algorithm>
#include <iostream>
#include <queue>
#include <unordered_set>

CombatEngine::CombatEngine(CombatArmy player, CombatArmy enemy)
    : m_player(std::move(player))
    , m_enemy(std::move(enemy))
    , m_rng(std::random_device{}())
{
    placeArmies();
    buildQueue();
    std::cout << "[CombatEngine] Round 1 start — "
              << m_queue.size() << " stacks in initiative order\n";
    // Catch degenerate cases (empty army passed in) before any action is taken.
    checkWinCondition();
    // Award per-turn XP to the first active unit (if SC).
    if (!isOver() && !m_queue.empty()) {
        TurnSlot& first = m_queue[m_turn];
        CombatUnit& firstUnit = first.isPlayer ? m_player.stacks[first.stackIndex]
                                               : m_enemy.stacks[first.stackIndex];
        awardScXp(firstUnit, first, firstUnit.perTurnXp);
    }
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

    // Build a set of all occupied hexes (any living stack, friend or foe).
    // A unit cannot move through or onto an occupied hex.
    std::unordered_set<HexCoord> occupied;
    for (const auto& s : m_player.stacks)
        if (!s.isDead()) occupied.insert(s.pos);
    for (const auto& s : m_enemy.stacks)
        if (!s.isDead()) occupied.insert(s.pos);

    // BFS flood-fill up to moveRange steps from the unit's current position.
    // State: (hex, steps_used).  We never step onto an occupied hex.
    struct Node { HexCoord hex; int steps; };
    std::queue<Node> frontier;
    std::unordered_set<HexCoord> visited;

    frontier.push({ unit.pos, 0 });
    visited.insert(unit.pos);

    std::vector<HexCoord> result;

    while (!frontier.empty()) {
        auto [hex, steps] = frontier.front();
        frontier.pop();

        if (steps >= unit.type->moveRange) continue;

        for (int dir = 0; dir < 6; ++dir) {
            HexCoord nb = hex.neighbor(dir);
            if (!CombatMap::inBounds(nb)) continue;
            if (visited.count(nb)) continue;
            visited.insert(nb);

            if (occupied.count(nb)) continue;  // blocked — cannot enter or pass through

            result.push_back(nb);
            frontier.push({ nb, steps + 1 });
        }
    }
    return result;
}

std::vector<HexCoord> CombatEngine::attackableTiles() const {
    if (isOver()) return {};

    const CombatUnit& unit = activeUnit();
    const auto& enemies = unit.isPlayer ? m_enemy.stacks : m_player.stacks;

    std::vector<HexCoord> result;

    if (unit.type->isRanged()) {
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
    std::cout << "[CombatEngine] " << actor.type->name
              << " moves " << actor.pos.q << "," << actor.pos.r
              << " → " << dest.q << "," << dest.r << "\n";

    CombatEvent ev;
    ev.type       = CombatEvent::Type::UnitMoved;
    ev.isPlayer   = slot.isPlayer;
    ev.stackIndex = slot.stackIndex;
    ev.from       = actor.pos;   // captured before update
    ev.to         = dest;
    m_events.push_back(ev);

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

    // Bounds and liveness guard — silently ignore invalid or dead targets.
    if (targetIndex < 0 || targetIndex >= static_cast<int>(enemyStacks.size())) return;
    CombatUnit& target = enemyStacks[targetIndex];
    if (target.isDead()) return;

    // Determine if ranged: unit has shots, has ammo, and target is not adjacent
    bool isRanged = attacker.type->isRanged()
                    && attacker.shotsLeft > 0
                    && attacker.pos.distanceTo(target.pos) > 1;
    if (isRanged) {
        --attacker.shotsLeft;
    }

    // Flanking / Pinned: target has attackers on two opposite hex sides.
    // Pinned units take 150% damage and lose their retaliation.
    const auto& friendlyStacks = slot.isPlayer ? m_player.stacks : m_enemy.stacks;
    bool pinned = !isRanged && isFlanked(target, friendlyStacks);
    if (pinned) {
        target.hasRetaliated = true;  // suppress retaliation before it fires
        std::cout << "[CombatEngine] " << target.type->name << " is PINNED — no retaliation, +50% damage\n";
    }

    // ── Attack event ─────────────────────────────────────────────────────────
    {
        CombatEvent ev;
        ev.type           = CombatEvent::Type::UnitAttacked;
        ev.isPlayer       = slot.isPlayer;
        ev.stackIndex     = slot.stackIndex;
        ev.targetIsPlayer = !slot.isPlayer;
        ev.targetIndex    = targetIndex;
        ev.wasFlanked     = pinned;
        m_events.push_back(ev);
    }

    int damage = calcDamage(attacker, target, m_rng);
    if (pinned) damage = damage * 3 / 2;
    applyDamage(target, damage);

    // Build attack-type annotation for the log.
    static const char* kAttackTypeNames[] = { "physical", "piercing", "magical" };
    const char* atName = kAttackTypeNames[static_cast<int>(attacker.type->attackType)];
    int rawDef    = target.effectiveDefense();
    int reducedDef = static_cast<int>(rawDef * (1.0f - attacker.type->defBypassRatio));

    std::cout << "[CombatEngine] " << attacker.type->name
              << " attacks " << target.type->name
              << " for " << damage << " damage"
              << " [" << atName << " | effDef " << rawDef << "→" << reducedDef << "]"
              << " (" << target.count << " survivors)\n";

    // ── Damage event ─────────────────────────────────────────────────────────
    {
        CombatEvent ev;
        ev.type       = CombatEvent::Type::UnitDamaged;
        ev.isPlayer   = !slot.isPlayer;   // target's army
        ev.stackIndex = targetIndex;
        ev.damage     = damage;
        m_events.push_back(ev);
    }
    if (target.isDead()) {
        CombatEvent ev;
        ev.type       = CombatEvent::Type::UnitDied;
        ev.isPlayer   = !slot.isPlayer;
        ev.stackIndex = targetIndex;
        m_events.push_back(ev);

        // Award kill XP to the attacker if it is an SC.
        awardScXp(attacker, slot, attacker.killXp);
    }

    // ── Melee retaliation ────────────────────────────────────────────────────
    if (!isRanged && !target.isDead() && !target.hasRetaliated
        && !attacker.type->hasAbility("no_retaliation")) {

        {
            CombatEvent ev;
            ev.type           = CombatEvent::Type::UnitAttacked;
            ev.isPlayer       = !slot.isPlayer;   // retaliator
            ev.stackIndex     = targetIndex;
            ev.targetIsPlayer = slot.isPlayer;
            ev.targetIndex    = slot.stackIndex;
            ev.isRetaliation  = true;
            m_events.push_back(ev);
        }

        int retDamage = calcDamage(target, attacker, m_rng);
        applyDamage(attacker, retDamage);
        target.hasRetaliated = true;

        std::cout << "[CombatEngine] " << target.type->name
                  << " retaliates for " << retDamage << " damage"
                  << " (" << attacker.type->name << ": " << attacker.count << " left)\n";

        {
            CombatEvent ev;
            ev.type       = CombatEvent::Type::UnitDamaged;
            ev.isPlayer   = slot.isPlayer;   // attacker took retaliation
            ev.stackIndex = slot.stackIndex;
            ev.damage     = retDamage;
            m_events.push_back(ev);
        }
        if (attacker.isDead()) {
            CombatEvent ev;
            ev.type       = CombatEvent::Type::UnitDied;
            ev.isPlayer   = slot.isPlayer;
            ev.stackIndex = slot.stackIndex;
            m_events.push_back(ev);
        }
    }

    advance();
}

// static
bool CombatEngine::isFlanked(const CombatUnit& target,
                               const std::vector<CombatUnit>& attackers) {
    // Opposite-direction pairs on a hex grid: (0,3), (1,4), (2,5).
    // Pinned = at least one pair where both opposite neighbours are occupied
    // by a living attacker.
    for (int dir = 0; dir < 3; ++dir) {
        HexCoord sideA = target.pos.neighbor(dir);
        HexCoord sideB = target.pos.neighbor(dir + 3);
        bool hasA = false, hasB = false;
        for (const auto& s : attackers) {
            if (!s.isDead()) {
                if (s.pos == sideA) hasA = true;
                if (s.pos == sideB) hasB = true;
            }
        }
        if (hasA && hasB) return true;
    }
    return false;
}

// static
int CombatEngine::calcDamage(const CombatUnit& attacker, const CombatUnit& defender,
                              std::mt19937& rng) {
    // Base damage roll: each creature in the stack rolls [minDmg, maxDmg],
    // then adds a flat per-creature bonus from equipped items.
    std::uniform_int_distribution<int> dist(attacker.type->minDamage, attacker.type->maxDamage);
    int baseDmg = 0;
    for (int i = 0; i < attacker.count; ++i)
        baseDmg += dist(rng) + attacker.damageBonus;

    // Effective attack and defense incorporate item bonuses.
    int effAtk = attacker.effectiveAttack();
    int effDef = defender.effectiveDefense();
    if (defender.isDefending)
        effDef += effDef / 4;

    // Attack-type armor bypass: Physical=0%, Piercing=50% (default), Magical=100%.
    // defBypassRatio is data-driven per unit so it can be tuned in units.json.
    int effDefReduced = effDef;
    if (attacker.type->defBypassRatio > 0.0f)
        effDefReduced = static_cast<int>(effDef * (1.0f - attacker.type->defBypassRatio));

    int diff = effAtk - effDefReduced;
    double mult;
    if (diff >= 0)
        mult = 1.0 + 0.05 * std::min(diff, 20);
    else
        mult = std::max(0.3, 1.0 + 0.025 * diff);

    return std::max(1, static_cast<int>(baseDmg * mult));
}

// ── SC XP ─────────────────────────────────────────────────────────────────────

void CombatEngine::awardScXp(CombatUnit& unit, const TurnSlot& slot, int amount) {
    if (!unit.isSpecialCharacter || !unit.scDef || amount <= 0) return;
    if (m_pendingChoice) return;  // pause XP while waiting for a branch choice

    unit.scXp += amount;

    CombatEvent xpEv;
    xpEv.type       = CombatEvent::Type::ScXpGained;
    xpEv.isPlayer   = slot.isPlayer;
    xpEv.stackIndex = slot.stackIndex;
    xpEv.xpAmount   = amount;
    m_events.push_back(xpEv);

    processLevelUps(unit, slot);
}

void CombatEngine::processLevelUps(CombatUnit& unit, const TurnSlot& slot) {
    while (unit.scLevel < unit.scDef->maxLevel
           && unit.scXp >= unit.scDef->xpThresholds[unit.scLevel - 1]) {
        ++unit.scLevel;

        // Always: heal on level-up (SCDef-level, branch-independent).
        unit.hpLeft = std::min(unit.hpLeft + unit.scDef->levelUpHeal,
                               unit.type->hitPoints);

        // Walk tree nodes for this level; apply auto effects or emit a choice.
        bool hasChoice = false;
        for (const auto& node : unit.scDef->tree) {
            if (node.level != unit.scLevel) continue;
            if (!nodePassesBranchGate(node, unit)) continue;

            if (!node.choices.empty()) {
                // Choice point — emit event and pause further level-up.
                CombatEvent choiceEv;
                choiceEv.type          = CombatEvent::Type::ScChoicePending;
                choiceEv.isPlayer      = slot.isPlayer;
                choiceEv.stackIndex    = slot.stackIndex;
                choiceEv.choiceLevel   = unit.scLevel;
                choiceEv.branchOptions = node.choices;
                m_events.push_back(choiceEv);

                m_pendingChoice         = true;
                m_pendingChoiceIsPlayer = slot.isPlayer;
                m_pendingChoiceStackIdx = slot.stackIndex;
                hasChoice = true;
                break;
            }

            applyNodeEffects(unit, node.autoEffects);
        }

        // Emit ScLevelUp after applying effects (or before choice UI).
        CombatEvent lvEv;
        lvEv.type       = CombatEvent::Type::ScLevelUp;
        lvEv.isPlayer   = slot.isPlayer;
        lvEv.stackIndex = slot.stackIndex;
        lvEv.newLevel   = unit.scLevel;
        m_events.push_back(lvEv);

        std::cout << "[CombatEngine] " << unit.type->name
                  << " reached level " << unit.scLevel << "!\n";

        if (hasChoice) break;  // resume after resolveScChoice()
    }
}

void CombatEngine::resolveScChoice(bool isPlayer, int stackIdx, const std::string& branchId) {
    if (!m_pendingChoice
        || isPlayer != m_pendingChoiceIsPlayer
        || stackIdx != m_pendingChoiceStackIdx) return;

    auto& stacks = isPlayer ? m_player.stacks : m_enemy.stacks;
    if (stackIdx < 0 || stackIdx >= static_cast<int>(stacks.size())) return;
    CombatUnit& unit = stacks[stackIdx];
    if (!unit.scDef) return;

    for (const auto& node : unit.scDef->tree) {
        if (node.level != unit.scLevel || node.choices.empty()) continue;
        if (!nodePassesBranchGate(node, unit)) continue;

        for (const auto& opt : node.choices) {
            if (opt.id != branchId) continue;
            unit.scChosenBranches[unit.scLevel] = branchId;
            applyNodeEffects(unit, opt.effects);
            std::cout << "[CombatEngine] " << unit.type->name
                      << " chose \"" << opt.name
                      << "\" at level " << unit.scLevel << "\n";
            break;
        }
        break;
    }

    m_pendingChoice = false;

    // Resume level-up loop in case XP crossed more than one threshold.
    TurnSlot slot { isPlayer, stackIdx };
    processLevelUps(unit, slot);
}

void CombatEngine::applyNodeEffects(CombatUnit& unit,
                                    const std::vector<NodeEffect>& effects) {
    for (const auto& ef : effects) {
        if (ef.type == "stat_mod") {
            if      (ef.key == "attack")  unit.attackBonus  += ef.value;
            else if (ef.key == "defense") unit.defenseBonus += ef.value;
            else if (ef.key == "hp")      unit.hpLeft = std::min(
                                              unit.hpLeft + ef.value, unit.type->hitPoints);
            else if (ef.key == "speed")   unit.speedBonus   += ef.value;
            else                          unit.scExtraStats[ef.key] += ef.value;
        } else if (ef.type == "unlock") {
            auto& ul = unit.scUnlocked;
            if (std::find(ul.begin(), ul.end(), ef.key) == ul.end())
                ul.push_back(ef.key);
        } else if (ef.type == "passive") {
            std::string tag = "passive:" + ef.key + ":" + std::to_string(ef.value);
            auto& ul = unit.scUnlocked;
            if (std::find(ul.begin(), ul.end(), tag) == ul.end())
                ul.push_back(tag);
        } else if (ef.type == "aura_mod") {
            unit.scExtraStats[ef.key] += ef.value;
        } else {
            std::cout << "[CombatEngine] Unknown NodeEffect type: \"" << ef.type << "\"\n";
        }
    }
}

bool CombatEngine::nodePassesBranchGate(const SCLevelNode& node,
                                         const CombatUnit& unit) const {
    if (node.requiresBranch.empty()) return true;
    for (const auto& [level, branchId] : unit.scChosenBranches)
        if (branchId == node.requiresBranch) return true;
    return false;
}

void CombatEngine::teleportUnit(bool isPlayer, int stackIdx, HexCoord pos) {
    auto& stacks = isPlayer ? m_player.stacks : m_enemy.stacks;
    if (stackIdx >= 0 && stackIdx < static_cast<int>(stacks.size()))
        stacks[stackIdx].pos = pos;
}

// static
void CombatEngine::applyDamage(CombatUnit& target, int damage) {
    while (damage > 0 && !target.isDead()) {
        if (damage >= target.hpLeft) {
            damage -= target.hpLeft;
            target.count--;
            target.hpLeft = (target.count > 0) ? target.type->hitPoints : 0;
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
    std::cout << "[CombatEngine] " << actor.type->name << " defends\n";

    CombatEvent ev;
    ev.type       = CombatEvent::Type::UnitDefended;
    ev.isPlayer   = slot.isPlayer;
    ev.stackIndex = slot.stackIndex;
    m_events.push_back(ev);

    advance();
}

void CombatEngine::doRetreat() {
    if (isOver()) return;
    std::cout << "[CombatEngine] Player retreats\n";
    m_result = CombatResult::Retreated;

    CombatEvent ev;
    ev.type   = CombatEvent::Type::BattleEnded;
    ev.result = CombatResult::Retreated;
    m_events.push_back(ev);
}

std::vector<CombatEvent> CombatEngine::drainEvents() {
    std::vector<CombatEvent> out;
    out.swap(m_events);
    return out;
}

// ── Turn advancement ──────────────────────────────────────────────────────────

void CombatEngine::advance() {
    if (isOver()) return;

    checkWinCondition();
    if (isOver()) return;

    ++m_turn;

    // Skip any stacks that died mid-round (retaliation, splash, etc.)
    while (m_turn < static_cast<int>(m_queue.size())) {
        const TurnSlot& s = m_queue[m_turn];
        const CombatUnit& u = s.isPlayer ? m_player.stacks[s.stackIndex]
                                         : m_enemy.stacks[s.stackIndex];
        if (!u.isDead()) break;
        ++m_turn;
    }

    if (m_turn >= static_cast<int>(m_queue.size())) {
        // All living stacks have acted — start next round
        ++m_round;
        m_turn = 0;

        for (auto& s : m_player.stacks) { s.isDefending = false; s.hasRetaliated = false; }
        for (auto& s : m_enemy.stacks)  { s.isDefending = false; s.hasRetaliated = false; }

        buildQueue();
        std::cout << "[CombatEngine] --- Round " << m_round << " ---\n";
    }

    // Per-turn XP for the newly active SC (fires each time a unit becomes active,
    // including the first turn of each round after queue rebuild).
    if (!isOver()) {
        TurnSlot& newSlot = m_queue[m_turn];
        CombatUnit& newActive = newSlot.isPlayer ? m_player.stacks[newSlot.stackIndex]
                                                 : m_enemy.stacks[newSlot.stackIndex];
        awardScXp(newActive, newSlot, newActive.perTurnXp);
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

    // Higher effective speed acts first (base speed + item bonuses); player wins ties.
    std::stable_sort(m_queue.begin(), m_queue.end(),
        [&](const TurnSlot& a, const TurnSlot& b) {
            const CombatUnit& ua = a.isPlayer ? m_player.stacks[a.stackIndex]
                                              : m_enemy.stacks[a.stackIndex];
            const CombatUnit& ub = b.isPlayer ? m_player.stacks[b.stackIndex]
                                              : m_enemy.stacks[b.stackIndex];
            int sa = ua.effectiveSpeed();
            int sb = ub.effectiveSpeed();
            if (sa != sb) return sa > sb;
            return a.isPlayer && !b.isPlayer;
        });
}

void CombatEngine::checkWinCondition() {
    if (m_enemy.allDead()) {
        m_result = CombatResult::PlayerWon;
        std::cout << "[CombatEngine] " << m_player.ownerName << " wins!\n";
        CombatEvent ev;
        ev.type   = CombatEvent::Type::BattleEnded;
        ev.result = CombatResult::PlayerWon;
        m_events.push_back(ev);
    } else if (m_player.allDead()) {
        m_result = CombatResult::EnemyWon;
        std::cout << "[CombatEngine] " << m_enemy.ownerName << " wins!\n";
        CombatEvent ev;
        ev.type   = CombatEvent::Type::BattleEnded;
        ev.result = CombatResult::EnemyWon;
        m_events.push_back(ev);
    }
}
