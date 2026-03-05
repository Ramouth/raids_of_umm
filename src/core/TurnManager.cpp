#include "TurnManager.h"
#include "core/ResourceManager.h"
#include "hero/Hero.h"
#include <iostream>

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
                          const ResourceManager& rm, TownStateMap& towns) {
    hero.resetMoves();
    applyMineIncome(control, rm);
    ++m_day;
    if (m_day % 7 == 0)
        tickWeeklyGrowth(towns, rm);
    return m_day;
}

void TurnManager::tickWeeklyGrowth(TownStateMap& towns, const ResourceManager& rm) {
    for (auto& [coord, town] : towns) {
        for (const UnitType* u : rm.unitsByTier()) {
            if (u->weeklyGrowth > 0)
                town.recruitPool[u->id] += u->weeklyGrowth;
        }
    }
    std::cout << "[TurnManager] Week growth ticked for " << towns.size() << " town(s).\n";
}
