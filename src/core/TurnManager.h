#pragma once
#include "factions/Faction.h"
#include "world/ObjectControl.h"
#include "world/TownState.h"
#include <vector>

class ResourceManager;
struct Hero;

class TurnManager {
public:
    // Create neutral + player factions; seed player treasury.
    void init(int startingGold = 2000);

    // Advance one day: reset hero moves, apply mine income, tick weekly growth.
    // Returns new day number.
    int nextDay(Hero& hero, const ObjectControlMap& control,
                const ResourceManager& rm, TownStateMap& towns);

    int day() const { return m_day; }

    const std::vector<Faction>& factions()   const { return m_factions; }
    const Faction& playerFaction()           const { return m_factions[Faction::Player]; }
    Faction&       playerFaction()                 { return m_factions[Faction::Player]; }

private:
    int                  m_day = 1;
    std::vector<Faction> m_factions;  // index == faction id

    void applyMineIncome(const ObjectControlMap& control, const ResourceManager& rm);
    void tickWeeklyGrowth(TownStateMap& towns, const ResourceManager& rm);
};
