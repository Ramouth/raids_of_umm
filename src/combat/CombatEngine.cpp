#include "CombatEngine.h"
#include <algorithm>
#include <iostream>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <climits>

CombatEngine::CombatEngine(CombatArmy player, CombatArmy enemy)
    : m_player(std::move(player))
    , m_enemy(std::move(enemy))
    , m_rng(std::random_device{}())
    , m_aiRng(std::random_device{}())
{
    placeArmies();
    refreshAuras();
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

void CombatEngine::setSeed(uint32_t seed) {
    m_rng.seed(seed);
    m_aiRng.seed(seed ^ 0x9E3779B9u);
}

// ── Queries ───────────────────────────────────────────────────────────────────

const CombatUnit& CombatEngine::activeUnit() const {
    const TurnSlot& slot = m_queue[m_turn];
    return slot.isPlayer ? m_player.stacks[slot.stackIndex]
                         : m_enemy.stacks[slot.stackIndex];
}

std::vector<HexCoord> CombatEngine::reachableTiles() const {
    if (isOver()) return {};
    return reachableFor(activeUnit());
}

std::vector<HexCoord> CombatEngine::reachableFor(const CombatUnit& unit) const {

    // Build a set of all occupied hexes (any living stack, friend or foe).
    // A unit cannot move through or onto an occupied hex.
    std::unordered_set<HexCoord> occupied;
    for (const auto& s : m_player.stacks)
        if (!s.isDead()) occupied.insert(s.pos);
    for (const auto& s : m_enemy.stacks)
        if (!s.isDead()) occupied.insert(s.pos);
    occupied.erase(unit.pos);

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
    for (int i = 0; i < static_cast<int>(enemies.size()); ++i)
        if (canAttack(i)) result.push_back(enemies[i].pos);
    return result;
}

namespace {
bool shootsNow(const CombatUnit& u) { return u.type->isRanged() && u.shotsLeft > 0; }
}

std::vector<HexCoord> CombatEngine::attackHexesFor(int targetIndex) const {
    std::vector<HexCoord> out;
    if (isOver()) return out;
    const CombatUnit& actor = activeUnit();
    const auto& enemies = actor.isPlayer ? m_enemy.stacks : m_player.stacks;
    if (targetIndex < 0 || targetIndex >= static_cast<int>(enemies.size())) return out;
    const CombatUnit& target = enemies[targetIndex];
    if (target.isDead()) return out;
    if (actor.pos.distanceTo(target.pos) == 1) out.push_back(actor.pos);
    if (shootsNow(actor)) return out;
    for (const HexCoord& h : reachableTiles())
        if (h.distanceTo(target.pos) == 1) out.push_back(h);
    return out;
}

bool CombatEngine::canAttackFrom(HexCoord from, int targetIndex) const {
    if (isOver()) return false;
    const CombatUnit& actor = activeUnit();
    const auto& enemies = actor.isPlayer ? m_enemy.stacks : m_player.stacks;
    if (targetIndex < 0 || targetIndex >= static_cast<int>(enemies.size())) return false;
    if (enemies[targetIndex].isDead()) return false;
    if (from == actor.pos && shootsNow(actor)) return true;
    for (const HexCoord& h : attackHexesFor(targetIndex))
        if (h == from) return true;
    return false;
}

HexCoord CombatEngine::bestAttackHex(int targetIndex) const {
    const CombatUnit& actor = activeUnit();
    if (shootsNow(actor)) return actor.pos;
    const auto hexes = attackHexesFor(targetIndex);
    if (hexes.empty()) return actor.pos;
    if (hexes.front() == actor.pos) return actor.pos;   // already adjacent: stay
    const auto& enemies = actor.isPlayer ? m_enemy.stacks : m_player.stacks;
    std::vector<CombatUnit> friends = actor.isPlayer ? m_player.stacks : m_enemy.stacks;
    const int self = currentTurn().stackIndex;
    // Walking distance from the actor over free hexes (BFS, same rules as reachableTiles).
    std::unordered_set<HexCoord> occupied;
    for (const auto* army : {&m_player, &m_enemy})
        for (const auto& s : army->stacks) if (!s.isDead()) occupied.insert(s.pos);
    std::unordered_map<HexCoord, int> dist{{actor.pos, 0}};
    std::queue<HexCoord> frontier;
    frontier.push(actor.pos);
    while (!frontier.empty()) {
        HexCoord h = frontier.front(); frontier.pop();
        for (int dir = 0; dir < 6; ++dir) {
            HexCoord n = h.neighbor(dir);
            if (!CombatMap::inBounds(n) || occupied.count(n) || dist.count(n)) continue;
            dist[n] = dist[h] + 1;
            frontier.push(n);
        }
    }
    HexCoord best = hexes.front();
    int bestKey = INT_MAX;
    for (const HexCoord& h : hexes) {
        friends[self].pos = h;
        const bool pin = isFlanked(enemies[targetIndex], friends);
        const int key = (pin ? 0 : 1000) + (dist.count(h) ? dist[h] : 999);
        if (key < bestKey) { bestKey = key; best = h; }
    }
    return best;
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
    refreshAuras();
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
    if (canAttack(targetIndex)) {
        doAttackFrom(bestAttackHex(targetIndex), targetIndex);
        return;
    }
    resolveAttack(targetIndex);   // unreachable: legacy strike in place
}

bool CombatEngine::doAttackFrom(HexCoord from, int targetIndex) {
    if (!canAttackFrom(from, targetIndex)) return false;
    TurnSlot& slot = m_queue[m_turn];
    CombatUnit& actor = slot.isPlayer ? m_player.stacks[slot.stackIndex]
                                      : m_enemy.stacks[slot.stackIndex];
    if (from != actor.pos) {
        CombatEvent ev;
        ev.type       = CombatEvent::Type::UnitMoved;
        ev.isPlayer   = slot.isPlayer;
        ev.stackIndex = slot.stackIndex;
        ev.from       = actor.pos;
        ev.to         = from;
        m_events.push_back(ev);
        actor.pos = from;
        refreshAuras();
    }
    resolveAttack(targetIndex);
    return true;
}

void CombatEngine::resolveAttack(int targetIndex) {
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

    // Line of sight: a shot through another stack loses half its force.
    const bool blocked = isRanged && !hasLineOfSight(attacker.pos, target.pos);

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
        ev.blockedShot    = blocked;
        m_events.push_back(ev);
    }

    int damage = calcDamage(attacker, target, m_rng);
    if (pinned) damage = damage * 3 / 2;
    if (blocked) damage = std::max(1, damage / 2);

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

    if (hitStack(!slot.isPlayer, targetIndex, damage, !isRanged))
        awardScXp(attacker, slot, attacker.killXp);   // kill XP if the attacker is an SC

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
        target.hasRetaliated = true;
        hitStack(slot.isPlayer, slot.stackIndex, retDamage, true);

        std::cout << "[CombatEngine] " << target.type->name
                  << " retaliates for " << retDamage << " damage"
                  << " (" << attacker.type->name << ": " << attacker.count << " left)\n";
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
double CombatEngine::damageMultiplier(const CombatUnit& attacker, const CombatUnit& defender) {
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
    if (diff >= 0)
        return 1.0 + 0.05 * std::min(diff, 20);
    return std::max(0.3, 1.0 + 0.025 * diff);
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

    return std::max(1, static_cast<int>(baseDmg * damageMultiplier(attacker, defender)));
}

// static
DamageRange CombatEngine::damageRange(const CombatUnit& attacker, const CombatUnit& defender,
                                      bool pinned) {
    DamageRange out;
    if (attacker.isDead()) return out;
    const double mult = damageMultiplier(attacker, defender);
    const int n = attacker.count;
    auto finish = [&](double base) {
        int dmg = std::max(1, static_cast<int>(base * mult));
        return pinned ? dmg * 3 / 2 : dmg;
    };
    out.min = finish(double(n) * (attacker.type->minDamage + attacker.damageBonus));
    out.max = finish(double(n) * (attacker.type->maxDamage + attacker.damageBonus));
    const double avgBase = double(n) * (0.5 * (attacker.type->minDamage + attacker.type->maxDamage)
                                        + attacker.damageBonus);
    out.avg = std::max(1.0, avgBase * mult) * (pinned ? 1.5 : 1.0);
    return out;
}

// static
int CombatEngine::killsFor(const CombatUnit& target, int damage) {
    CombatUnit copy = target;
    applyDamage(copy, damage);
    return target.count - copy.count;
}

bool CombatEngine::canAttack(int targetIndex) const {
    if (isOver()) return false;
    const CombatUnit& actor = activeUnit();
    const auto& enemies = actor.isPlayer ? m_enemy.stacks : m_player.stacks;
    if (targetIndex < 0 || targetIndex >= static_cast<int>(enemies.size())) return false;
    if (enemies[targetIndex].isDead()) return false;
    if (shootsNow(actor)) return true;
    return !attackHexesFor(targetIndex).empty();
}

AttackPreview CombatEngine::previewAttack(int targetIndex) const {
    if (!canAttack(targetIndex)) return {};
    return previewAttack(targetIndex, bestAttackHex(targetIndex));
}

AttackPreview CombatEngine::previewAttack(int targetIndex, HexCoord from) const {
    if (!canAttackFrom(from, targetIndex)) return {};
    return previewAttackUnchecked(targetIndex, from);
}

AttackPreview CombatEngine::previewAttackUnchecked(int targetIndex, HexCoord from) const {
    AttackPreview p;
    if (isOver()) return p;
    CombatUnit actor = activeUnit();
    actor.pos = from;
    const auto& enemies = actor.isPlayer ? m_enemy.stacks : m_player.stacks;
    if (targetIndex < 0 || targetIndex >= static_cast<int>(enemies.size())
        || enemies[targetIndex].isDead()) return p;
    std::vector<CombatUnit> friends = actor.isPlayer ? m_player.stacks : m_enemy.stacks;
    const int self = currentTurn().stackIndex;
    friends[self].pos = from;
    actor.auraBonus = auraAt(friends, self, from);
    const CombatUnit& target = enemies[targetIndex];

    // A companion's bodyguard takes half of a melee blow (rounded down).
    auto split = [](DamageRange& d, DamageRange& guard) {
        guard = {d.min / 2, d.max / 2, d.avg / 2};
        d = {d.min - guard.min, d.max - guard.max, d.avg - guard.avg};
    };

    p.valid  = true;
    p.ranged = actor.type->isRanged() && actor.shotsLeft > 0
               && actor.pos.distanceTo(target.pos) > 1;
    p.pinned = !p.ranged && isFlanked(target, friends);
    p.damage = damageRange(actor, target, p.pinned);
    p.blocked = p.ranged && !hasLineOfSight(from, target.pos);
    if (p.blocked)
        p.damage = {std::max(1, p.damage.min / 2), std::max(1, p.damage.max / 2), std::max(1.0, p.damage.avg / 2)};
    p.guarded = !p.ranged && bodyguardFor(enemies, targetIndex) >= 0;
    if (p.guarded) split(p.damage, p.guardDamage);
    p.killsMin = killsFor(target, p.damage.min);
    p.killsMax = killsFor(target, p.damage.max);

    const bool canRetaliate = !p.ranged && !p.pinned && !target.hasRetaliated
                              && !actor.type->hasAbility("no_retaliation");
    // Retaliation only happens if the target survives; the low roll leaves the most alive.
    if (canRetaliate && p.killsMin < target.count) {
        p.retaliation = true;
        CombatUnit strongest = target;   // survivors after the low roll
        applyDamage(strongest, p.damage.min);
        CombatUnit weakest = target;     // survivors after the high roll
        applyDamage(weakest, p.damage.max);
        CombatUnit expected = target;
        applyDamage(expected, static_cast<int>(p.damage.avg));
        DamageRange hi = damageRange(strongest, actor);
        DamageRange lo = weakest.isDead() ? DamageRange{} : damageRange(weakest, actor);
        DamageRange mid = expected.isDead() ? DamageRange{} : damageRange(expected, actor);
        p.retaliationDamage.min = lo.min;
        p.retaliationDamage.max = hi.max;
        p.retaliationDamage.avg = mid.avg;
        p.retaliationGuarded = bodyguardFor(friends, self) >= 0;
        if (p.retaliationGuarded) {
            DamageRange ignored;
            split(p.retaliationDamage, ignored);
        }
        p.retKillsMin = killsFor(actor, lo.min);
        p.retKillsMax = killsFor(actor, hi.max);
    }
    return p;
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
    refreshAuras();
}

// static
void CombatEngine::applyDamage(CombatUnit& target, int damage) {
    while (damage > 0 && !target.isDead()) {
        if (damage >= target.hpLeft) {
            damage -= target.hpLeft;
            target.count--;
            target.hpLeft = (target.count > 0) ? target.maxHp() : 0;
        } else {
            target.hpLeft -= damage;
            break;
        }
    }
}

bool CombatEngine::hitStack(bool targetIsPlayer, int targetIndex, int damage, bool melee) {
    auto& stacks = targetIsPlayer ? m_player.stacks : m_enemy.stacks;
    CombatUnit& target = stacks[targetIndex];
    auto emitDamage = [&](int index, int amount, int before, bool guard) {
        CombatEvent ev;
        ev.type       = CombatEvent::Type::UnitDamaged;
        ev.isPlayer   = targetIsPlayer;
        ev.stackIndex = index;
        ev.damage     = amount;
        ev.kills      = before - stacks[index].count;
        ev.remaining  = stacks[index].count;
        ev.bodyguard  = guard;
        m_events.push_back(ev);
        if (stacks[index].isDead()) {
            CombatEvent died;
            died.type       = CombatEvent::Type::UnitDied;
            died.isPlayer   = targetIsPlayer;
            died.stackIndex = index;
            m_events.push_back(died);
        }
    };
    // A companion's bodyguard steps into half of any melee blow.
    const int guard = melee ? bodyguardFor(stacks, targetIndex) : -1;
    if (guard >= 0) {
        const int share = damage / 2;
        damage -= share;
        const int before = stacks[guard].count;
        applyDamage(stacks[guard], share);
        emitDamage(guard, share, before, true);
    }
    const int before = target.count;
    applyDamage(target, damage);
    emitDamage(targetIndex, damage, before, false);
    if (guard >= 0 && stacks[guard].isDead()) refreshAuras();
    if (target.isDead()) refreshAuras();
    return target.isDead();
}

bool CombatEngine::hasLineOfSight(HexCoord from, HexCoord to) const {
    std::unordered_set<HexCoord> occupied;
    for (const auto* army : {&m_player, &m_enemy})
        for (const auto& s : army->stacks) if (!s.isDead()) occupied.insert(s.pos);
    for (int nudge : {1, -1}) {
        const auto line = from.lineTo(to, nudge);
        bool clear = true;
        for (size_t i = 1; i + 1 < line.size() && clear; ++i)
            clear = !occupied.count(line[i]);
        if (clear) return true;
    }
    return false;
}

int CombatEngine::auraAt(const std::vector<CombatUnit>& friends, int self, HexCoord pos) {
    int best = 0;
    for (int j = 0; j < static_cast<int>(friends.size()); ++j) {
        const CombatUnit& s = friends[j];
        if (j == self || s.isDead() || s.type->auraRadius <= 0) continue;
        if (s.pos.distanceTo(pos) <= s.type->auraRadius) best = std::max(best, s.type->auraDefense);
    }
    return best;
}

int CombatEngine::bodyguardFor(const std::vector<CombatUnit>& friends, int self) {
    if (self < 0 || self >= static_cast<int>(friends.size())) return -1;
    const CombatUnit& ward = friends[self];
    if (!ward.isSpecialCharacter || ward.isDead()) return -1;
    int best = -1, bestHp = 0;
    for (int j = 0; j < static_cast<int>(friends.size()); ++j) {
        const CombatUnit& s = friends[j];
        if (j == self || s.isDead() || s.isSpecialCharacter || s.pos.distanceTo(ward.pos) != 1) continue;
        if (s.totalHp() > bestHp) { bestHp = s.totalHp(); best = j; }
    }
    return best;
}

void CombatEngine::refreshAuras() {
    for (auto* army : {&m_player, &m_enemy})
        for (int i = 0; i < static_cast<int>(army->stacks.size()); ++i)
            army->stacks[i].auraBonus = auraAt(army->stacks, i, army->stacks[i].pos);
}

std::vector<int> CombatEngine::threatsTo(bool isPlayer, int index) const {
    std::vector<int> out;
    const auto& own = isPlayer ? m_player.stacks : m_enemy.stacks;
    const auto& foes = isPlayer ? m_enemy.stacks : m_player.stacks;
    if (index < 0 || index >= static_cast<int>(own.size()) || own[index].isDead()) return out;
    const HexCoord at = own[index].pos;
    for (int i = 0; i < static_cast<int>(foes.size()); ++i) {
        const CombatUnit& f = foes[i];
        if (f.isDead()) continue;
        // A shooter counts only with a clear line: a blocked shot is half a threat.
        bool reaches = (shootsNow(f) && hasLineOfSight(f.pos, at)) || f.pos.distanceTo(at) == 1;
        if (!reaches)
            for (const HexCoord& h : reachableFor(f))
                if (h.distanceTo(at) == 1) { reaches = true; break; }
        if (reaches) out.push_back(i);
    }
    return out;
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

    // Companions take the back-line hexes first (centre outwards).  Troops
    // then wall them in — the free hexes around a companion, front ones first —
    // and fill the rest of the back line, then the column in front of it.
    auto place = [](CombatArmy& army, std::vector<HexCoord> spawns, int frontCol) {
        for (int row : {2, 1, 3, 0, 4}) spawns.push_back(CombatMap::toHex(frontCol, row));
        std::vector<HexCoord> taken;
        size_t next = 0;
        for (auto& s : army.stacks)
            if (s.isSpecialCharacter && next < spawns.size()) taken.push_back(s.pos = spawns[next++]);
        std::vector<HexCoord> order;
        auto add = [&](HexCoord h) {
            if (std::find(spawns.begin(), spawns.end(), h) == spawns.end()) return;   // not a deployment hex
            if (std::find(taken.begin(), taken.end(), h) != taken.end()) return;
            if (std::find(order.begin(), order.end(), h) != order.end()) return;
            order.push_back(h);
        };
        std::vector<HexCoord> around;
        for (const HexCoord& c : taken)
            for (int dir = 0; dir < 6; ++dir) around.push_back(c.neighbor(dir));
        std::stable_sort(around.begin(), around.end(), [frontCol](HexCoord a, HexCoord b) {
            return std::abs(a.q - frontCol) < std::abs(b.q - frontCol);   // front first
        });
        for (const HexCoord& h : around) add(h);
        for (const HexCoord& h : spawns) add(h);
        size_t k = 0;
        for (auto& s : army.stacks)
            if (!s.isSpecialCharacter && k < order.size()) s.pos = order[k++];
    };
    place(m_player, pSpawns, 1);
    place(m_enemy, eSpawns, CombatMap::GRID_W - 2);
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
