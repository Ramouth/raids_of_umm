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
    player.treasury[Resource::Gold]     = startingGold;
    player.treasury[Resource::Wood]     = 5;
    player.treasury[Resource::Stone]    = 5;
    player.treasury[Resource::Obsidian] = 2;
    player.treasury[Resource::Crystal]  = 2;
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
    m_lastEvent.clear();
    hero.resetMoves();
    applyMineIncome(control, rm);
    ++m_day;

    // End of week: unit pool growth.
    if (dayOfWeek() == 7) {
        tickWeeklyGrowth(towns, rm);

        // End of month: monthly event fires after weekly growth.
        if (weekOfMonth() == 4)
            tickMonthly(playerFaction());
    }

    return m_day;
}

void TurnManager::tickWeeklyGrowth(TownStateMap& towns, const ResourceManager& rm) {
    for (auto& [coord, town] : towns) {
        for (const UnitType* u : rm.unitsByTier()) {
            if (u->weeklyGrowth > 0)
                town.recruitPool[u->id] += u->weeklyGrowth;
        }
    }
    std::cout << "[TurnManager] Week " << week()
              << " growth ticked for " << towns.size() << " town(s).\n";
}

// Monthly event table — cycles by (month - 1) % count.
namespace {
struct MonthlyEvent { const char* name; int goldBonus; };
constexpr MonthlyEvent kMonthlyEvents[] = {
    { "Caravan Arrives",       500 },
    { "Festival of the Scarab",300 },
    { "Merchants from the East",400 },
    { "Month of Dust",           0 },  // quiet — no bonus
};
constexpr int kMonthlyEventCount =
    static_cast<int>(sizeof(kMonthlyEvents) / sizeof(kMonthlyEvents[0]));
} // namespace

void TurnManager::tickMonthly(Faction& player) {
    const MonthlyEvent& ev = kMonthlyEvents[(month() - 1) % kMonthlyEventCount];
    if (ev.goldBonus > 0)
        player.treasury[Resource::Gold] += ev.goldBonus;

    m_lastEvent = ev.name;
    if (ev.goldBonus > 0) {
        m_lastEvent += " (+";
        m_lastEvent += std::to_string(ev.goldBonus);
        m_lastEvent += " gold)";
    }
    std::cout << "[TurnManager] Month " << month()
              << " event: " << m_lastEvent << "\n";
}
