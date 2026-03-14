#pragma once
#include "factions/Faction.h"
#include "world/ObjectControl.h"
#include "world/TownState.h"
#include <string>
#include <vector>

class ResourceManager;
struct Hero;

class TurnManager {
public:
    // Create neutral + player factions; seed player treasury.
    void init(int startingGold = 2000);

    // Advance one day: reset hero moves, apply mine income, tick weekly/monthly events.
    // Returns new day number.
    int nextDay(Hero& hero, const ObjectControlMap& control,
                const ResourceManager& rm, TownStateMap& towns);

    // ── Calendar ─────────────────────────────────────────────────────────────
    int day()          const { return m_day; }
    int dayOfWeek()    const { return (m_day - 1) % 7 + 1; }        // 1–7
    int week()         const { return (m_day - 1) / 7 + 1; }        // 1-based absolute
    int weekOfMonth()  const { return (week() - 1) % 4 + 1; }       // 1–4
    int month()        const { return (m_day - 1) / 28 + 1; }       // 1-based

    // Set by nextDay(); describes what event (if any) fired this turn.
    // Empty string = no special event this turn.
    const std::string& lastEvent() const { return m_lastEvent; }

    // Session restore — called by AdventureState::loadSession()
    void setDay(int d) { m_day = d; }

    const std::vector<Faction>& factions()   const { return m_factions; }
    const Faction& playerFaction()           const { return m_factions[Faction::Player]; }
    Faction&       playerFaction()                 { return m_factions[Faction::Player]; }

private:
    int                  m_day = 1;
    std::vector<Faction> m_factions;  // index == faction id
    std::string          m_lastEvent;

    void applyMineIncome(const ObjectControlMap& control, const ResourceManager& rm);
    void tickWeeklyGrowth(TownStateMap& towns, const ResourceManager& rm);
    void tickMonthly(Faction& player);
};
