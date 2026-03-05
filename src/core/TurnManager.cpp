#include "TurnManager.h"
#include "core/ResourceManager.h"
#include "hero/Hero.h"

void TurnManager::init(int startingGold) {
    m_day = 1;
    m_factions.clear();

    Faction neutral;
    neutral.id   = Faction::Neutral;
    neutral.name = "Neutral";
    m_factions.push_back(std::move(neutral));

    Faction player;
    player.id   = Faction::Player;
    player.name = "Player";
    player.treasury[Resource::Gold] = startingGold;
    m_factions.push_back(std::move(player));
}

void TurnManager::applyMineIncome(const ObjectControlMap& control,
                                   const ResourceManager& rm) {
    for (const auto& [coord, ctrl] : control) {
        if (ctrl.ownerFaction < 1) continue;
        if (ctrl.ownerFaction >= static_cast<int>(m_factions.size())) continue;
        ResourcePool income = rm.mineIncome(ctrl.objType);
        m_factions[ctrl.ownerFaction].treasury += income;
    }
}

int TurnManager::nextDay(Hero& hero, const ObjectControlMap& control,
                          const ResourceManager& rm) {
    hero.resetMoves();
    applyMineIncome(control, rm);
    return ++m_day;
}
