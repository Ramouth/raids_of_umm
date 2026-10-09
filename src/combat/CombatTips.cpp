#include "CombatTips.h"
#include <algorithm>
#include <cctype>

namespace {

std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
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
        if (m_shown.count(tip.id) || !holds.count(tip.when)) continue;
        if (std::find(tip.maps.begin(), tip.maps.end(), m_map) == tip.maps.end()) continue;
        if (!speakerFighting(engine, tip.speaker)) continue;
        m_shown.insert(tip.id);
        m_shownHere.push_back(tip.id);
        m_lastTurn = turn;
        return &tip;
    }
    return nullptr;
}
