#include "CombatTips.h"
#include <algorithm>
#include <cctype>

namespace {

std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// The unit is `who` (its id, companion id or name, any case).
bool isUnit(const CombatUnit& u, const std::string& who) {
    const std::string w = lower(who);
    return lower(u.type->id) == w || lower(u.type->name) == w || (!u.scId.empty() && lower(u.scId) == w);
}

// The player stack that played the turn just ended (null: an enemy's, or none yet).
const CombatUnit* lastPlayerActor(const CombatEngine& engine) {
    const auto& t = engine.lastTurn();
    const auto& own = engine.playerArmy().stacks;
    if (t.serial == 0 || !t.isPlayer || t.stackIndex < 0 || t.stackIndex >= static_cast<int>(own.size())) return nullptr;
    return &own[t.stackIndex];
}

} // namespace

CombatTipWatch::CombatTipWatch(const std::vector<CombatTip>* tips, std::string map,
                               std::set<std::string> shown, const CombatEngine& engine)
    : m_tips(tips), m_map(std::move(map)), m_shown(std::move(shown)) {
    for (const auto& u : engine.playerArmy().stacks)
        m_companionHp.push_back(u.isSpecialCharacter ? u.totalHp() : 0);
}

bool CombatTipWatch::speakerFighting(const CombatEngine& engine, const std::string& speaker) {
    const std::string who = lower(speaker);
    for (const auto& u : engine.playerArmy().stacks)
        if (u.isSpecialCharacter && !u.isDead() && (lower(u.scId) == who || lower(u.type->name) == who))
            return true;
    return false;
}

bool CombatTipWatch::rushed(const CombatEngine& engine) {
    const CombatUnit* u = lastPlayerActor(engine);
    if (!u || u->isDead() || !engine.lastTurn().moved || engine.lastTurn().mad) return false;
    bool engaged = false;
    for (const auto& f : engine.enemyArmy().stacks)
        engaged |= !f.isDead() && f.pos.distanceTo(u->pos) == 1;
    if (!engaged) return false;
    for (const auto& other : engine.playerArmy().stacks)
        if (&other != u && !other.isDead() && other.pos.distanceTo(u->pos) <= 2) return false;
    return true;
}

bool CombatTipWatch::actorMatches(const CombatEngine& engine, const CombatTip& tip) {
    const CombatUnit* who = nullptr;
    if (tip.when == "rushes" || tip.when == "obstacle_broken") {
        who = lastPlayerActor(engine);
        if (tip.when == "obstacle_broken" && !tip.kind.empty()) {
            bool kind = false;
            for (const auto& o : engine.lastTurn().broken) kind |= lower(o.kind) == lower(tip.kind);
            if (!kind) return false;
        }
    } else if (tip.when == "first_turn_shooter" || tip.when == "defend_available") {
        if (!engine.isOver()) who = &engine.activeUnit();
    } else return true;   // the other keys are about no one stack
    return tip.actor.empty() || (who && isUnit(*who, tip.actor));
}

std::set<std::string> CombatTipWatch::conditions(const CombatEngine& engine) const {
    std::set<std::string> holds;
    const auto& own = engine.playerArmy().stacks;
    for (size_t i = 0; i < own.size() && i < m_companionHp.size(); ++i)
        if (own[i].isSpecialCharacter && own[i].totalHp() < m_companionHp[i]) holds.insert("companion_hit");
    if (engine.isOver()) {
        if (engine.result() == CombatResult::PlayerWon) holds.insert("victory_first");
        return holds;
    }
    if (engine.roundNumber() == 1) holds.insert("battle_start");
    if (rushed(engine)) holds.insert("rushes");
    if (lastPlayerActor(engine) && !engine.lastTurn().broken.empty()) holds.insert("obstacle_broken");
    const auto& turn = engine.currentTurn();
    const CombatUnit& active = engine.activeUnit();
    if (turn.isPlayer && !active.mad) {
        holds.insert("defend_available");
        if (active.type->isRanged() && active.shotsLeft > 0 && !engine.isEngaged(active))
            holds.insert("first_turn_shooter");
    }
    const auto& foes = engine.enemyArmy().stacks;
    const auto& stats = engine.stats().player;
    for (size_t i = 0; i < own.size(); ++i) {
        const CombatUnit& u = own[i];
        if (u.isDead()) continue;
        for (const auto& f : foes)
            if (!f.isDead() && f.pos.distanceTo(u.pos) == 1) holds.insert("enemy_adjacent");
        if (CombatEngine::isFlanked(u, foes)) holds.insert("flanked");
        const int started = i < stats.size() ? stats[i].startCount : 0;
        if (!u.isSpecialCharacter && started > 0 && u.count * 4 < started) holds.insert("low_stack");
    }
    return holds;
}

const CombatTip* CombatTipWatch::check(const CombatEngine& engine) {
    if (!m_tips || m_map.empty()) return nullptr;
    const std::string turn = engine.isOver() ? "result"
        : std::to_string(engine.roundNumber()) + ":" + (engine.currentTurn().isPlayer ? "p" : "e")
          + std::to_string(engine.currentTurn().stackIndex);
    if (turn == m_lastTurn) return nullptr;
    const auto holds = conditions(engine);
    for (const auto& tip : *m_tips) {
        const bool shownHere = std::find(m_shownHere.begin(), m_shownHere.end(), tip.id) != m_shownHere.end();
        if (shownHere || (!tip.perBattle && m_shown.count(tip.id)) || !holds.count(tip.when)) continue;
        if (std::find(tip.maps.begin(), tip.maps.end(), m_map) == tip.maps.end()) continue;
        if (!speakerFighting(engine, tip.speaker) || !actorMatches(engine, tip)) continue;
        m_shown.insert(tip.id);
        m_shownHere.push_back(tip.id);
        m_lastTurn = turn;
        m_lines = {{tip.speaker, tip.text}};
        for (const auto& line : tip.reply)
            if (speakerFighting(engine, line.speaker)) m_lines.push_back(line);
        return &tip;
    }
    return nullptr;
}
