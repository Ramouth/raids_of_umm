#pragma once
#include "Scenario.h"
#include "core/ResourceManager.h"
#include "core/TurnManager.h"
#include "hero/Hero.h"
#include "world/ObjectControl.h"
#include "world/TownState.h"
#include "world/WorldMap.h"
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <map>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

/*
 * AdventureSession — adventure-map rules with no rendering or input.
 *
 * Owns the calendar, hero movement points, fog of war, and object ownership.
 * The Godot slice drives it through the GDExtension bridge; raids_tests
 * exercises it directly. AdventureState (SDL) still has its own copy of
 * these rules and can migrate onto this class later.
 *
 * Movement uses weighted terrain cost (road 0.5, sand 1.0, dune 1.5, ...).
 * travel() walks as far along the route as the day's points allow and
 * stops; the rest of the route is left for the next day (HoMM3-style).
 *
 * Encounters (guards, old mines) block their hex until beaten. Travelling
 * into one stops the hero next to it and reports `encounter`; the caller
 * runs the battle and calls resolveEncounter(). One old mine, picked at
 * random per game, hides the passage — clearing it wins the scenario.
 */
// Bonuses every troop stack of the hero's army fights with (commander's
// items, town buildings such as the Drill Yard).
struct ArmyBonus { int attack = 0, defense = 0, speed = 0; bool readiedShot = false; };

class AdventureSession {
public:
    static constexpr int   SIGHT_RADIUS       = 4;   // hero
    static constexpr int   OWNED_SIGHT_RADIUS = 2;   // owned towns and mines
    static constexpr float DEFAULT_MOVES      = 10.0f;
    static constexpr int   STARTING_GOLD      = 2000;

    struct Encounter {
        std::string guardsJson = "[]";  // combat army, opaque to the session
        ResourcePool reward;            // paid on victory
        std::string item;               // item id granted on victory ("" = none)
    };

    // Outcome of clearing an old mine.
    enum class MineFind { None, Passage, Loot, Collapse };

    struct Step {
        HexCoord    cell;
        std::string capture;  // non-empty when this step took an object ("Desert Quarry")
        std::vector<HexCoord> revealed;  // cells explored for the first time on this step
        std::string found;    // what a visited site gave ("Wood Pile: +5 Wood"), "" if nothing
    };

    // One-time pickups: resource piles, campfires, artifacts and chests.
    // A chest offers a HoMM3 choice — gold, or experience for the companions.
    struct Pickup {
        ResourcePool reward;
        std::string  item;       // artifact item id ("" = none)
        int          xp = 0;     // chest: the experience alternative
        bool         chest = false;
    };
    static constexpr int   WATCHTOWER_RADIUS = 9;
    static constexpr float STABLES_BONUS     = 3.0f;  // extra moves per day until week's end
    static constexpr int   LEARNING_XP       = 400;

    // Loads the map + data registry and seeds ownership from map factionIds.
    // Hero starts at the first player-owned town (else first town, else first passable tile).
    // encountersPath: optional JSON of guard armies + rewards (see data/maps/*.encounters.json).
    // seed picks which old mine hides the passage.
    // triggersPath: optional Scenario script (quests, dialogue, clues).
    std::optional<std::string> start(const std::string& mapPath, const std::string& dataDir,
                                     const std::string& encountersPath = "", uint32_t seed = 0,
                                     const std::string& triggersPath = "");
    std::optional<std::string> start(WorldMap map, const std::string& dataDir,
                                     const std::string& encountersPath = "", uint32_t seed = 0,
                                     const std::string& triggersPath = "");

    // ── Movement ─────────────────────────────────────────────────────────────
    // Route from the hero to 'to', excluding the hero's own cell. Empty = no route.
    std::vector<HexCoord> route(const HexCoord& to) const;
    float stepCost(const HexCoord& cell) const;
    float routeCost(const std::vector<HexCoord>& path) const;
    // How many leading steps of 'path' today's points cover.
    int   affordableSteps(const std::vector<HexCoord>& path) const;
    // Moves the hero along the route as far as points allow. Stops beside an
    // uncleared encounter on the route; pendingEncounter() then names it.
    std::vector<Step> travel(const HexCoord& to);

    // ── Encounters ───────────────────────────────────────────────────────────
    bool isEncounter(const HexCoord& c) const;       // uncleared guard / old mine
    // HoMM3 zone of control: the neutral guard camp next to 'c' that attacks a
    // hero stepping there (old mines and war-bands do not). nullopt = none.
    std::optional<HexCoord> guardZoneAt(const HexCoord& c) const;
    const Encounter* encounterAt(const HexCoord& c) const;
    std::optional<HexCoord> pendingEncounter() const { return m_pending; }
    // Called after the battle. Victory clears the hex, pays the reward and moves the
    // hero onto it (spending its step cost). Returns what an old mine held.
    MineFind resolveEncounter(bool victory);
    bool won() const { return m_won; }
    std::optional<HexCoord> passageMine() const { return m_passage; }

    // ── Adventure sites (mills, stables, watchtowers, pickups, dwellings) ────
    const Pickup* pickupAt(const HexCoord& c) const;
    // Travel stops on a chest; the choice is made here (gold=false takes the xp).
    std::optional<HexCoord> pendingChest() const { return m_pendingChest; }
    std::string claimChest(bool gold);
    // True when a weekly site (mill, stables) was already used this week, or a
    // one-time site (watchtower, learning stone) was already visited.
    bool siteUsed(const HexCoord& c) const;

    // ── Army ─────────────────────────────────────────────────────────────────
    struct Stack { std::string id; int count = 0; };
    std::vector<Stack> army() const;
    // Replaces the hero's army (e.g. with combat survivors). Unknown ids are dropped.
    void setArmy(const std::vector<Stack>& stacks);

    // ── Garrisons (HoMM3-style: troops left at an owned mine or town) ────────
    const std::vector<Stack>* garrison(const HexCoord& c) const;
    // Moves 'count' of a unit between the hero (standing there) and the site's garrison.
    // toGarrison=false moves them back. Returns an error, or nullopt.
    std::optional<std::string> transfer(const HexCoord& site, const std::string& unitId,
                                        int count, bool toGarrison);

    // ── Towns ────────────────────────────────────────────────────────────────
    // Faction roster a town recruits from, by owner (1 = Ivory Compact, 2 = Shariw).
    static std::string rosterFor(int owner);
    const TownState* town(const HexCoord& c) const;
    // Recruit into the hero's army. Hero must stand in a town the player owns.
    // Returns an error message, or nullopt on success.
    std::optional<std::string> recruit(const HexCoord& town, const std::string& unitId, int count);

    // ── Town buildings (data/buildings.json) ─────────────────────────────────
    // One building per town per day, in any town the player holds (no hero
    // needed, as in HoMM3). A new dwelling brings its first week of recruits.
    std::optional<std::string> build(const HexCoord& town, const std::string& buildingId);
    // Why `buildingId` cannot be built in `town` right now ("" = it can).
    std::string buildBlocker(const HexCoord& town, const std::string& buildingId) const;
    bool canRecruitHere(const HexCoord& town, const std::string& unitId) const;   // dwelling built?
    double growthBonus(const HexCoord& town) const;      // best fort: 0, 0.5, 1.0
    int    townGold(const HexCoord& town) const;         // best hall's income (else the flat town income)
    bool   hasMarket() const;                            // a marketplace in any town the player holds
    int    armyAttackBonus() const { return armyBonus().attack; }

    // ── Commander's equipment (HoMM3 paper doll) ─────────────────────────────
    // Items the commander wears help every troop stack: +attack, +defence,
    // +speed. Unworn items sit in the backpack. A cursed item cannot be taken
    // off once worn. Slots: helm, amulet, armor, weapon, boots, trinket1, trinket2.
    static const std::vector<std::string>& equipSlots();
    std::optional<std::string> equip(const std::string& itemId);
    std::optional<std::string> unequip(const std::string& slot);
    const std::map<std::string, std::string>& equipped() const { return m_equipped; }
    std::vector<std::string> backpack() const;   // owned, not worn (sorted)
    using ArmyBonus = ::ArmyBonus;
    ArmyBonus armyBonus() const;                  // worn items + buildings (e.g. Drill Yard)
    // Marketplace: sell `amount` of `give` for as much `get` as it buys.
    // Returns the amount received, or an error.
    struct TradeResult { int received = 0; std::string error; };
    TradeResult trade(Resource give, Resource get, int amount);
    static int tradeQuote(Resource give, Resource get, int amount);
    const ResourceManager& resources() const { return *m_resources; }

    // ── Story (Scenario hooks and effects) ───────────────────────────────────
    Scenario&       scenario()       { return m_scenario; }
    const Scenario& scenario() const { return m_scenario; }
    void revealArea(const HexCoord& center, int radius);
    void give(const ResourcePool& resources);
    // A house turns on the player: the named town/site flies the rival banner
    // and a war-band with 'army' rides out of it. Returns false if no such object.
    bool betray(const std::string& objectName, const std::string& bandName, const std::vector<Stack>& army);
    // Rules out one wrong old mine not yet searched or ruled out; returns its name.
    std::optional<std::string> giveClue();
    const std::unordered_set<HexCoord>& ruledOut() const { return m_ruledOut; }
    void addItem(const std::string& id);
    bool hasItem(const std::string& id) const { return m_items.count(id) > 0; }
    const std::unordered_set<std::string>& items() const { return m_items; }
    // Accept a scenario offer at the hero's current object. Error or nullopt.
    std::optional<std::string> acceptOffer(const std::string& id);
    int minesHeld(int faction = Faction::Player) const;

    // ── Hero growth (Specials.cpp) ───────────────────────────────────────────
    // The commander levels from the same experience as the companions; each
    // level after the first is one point to spend in the spell tree.
    struct HeroProgress { int level = 1; int xp = 0; int points = 0; };
    const HeroProgress& heroProgress() const { return m_heroProgress; }
    // Experience a victory at 'c' would give (guards, old mines, war-bands); 0 = none.
    int encounterXp(const HexCoord& c) const;

    // ── Special characters (see Specials.cpp) ────────────────────────────────
    struct Ability { int level; std::string name, text; };
    struct Special {
        std::string             id, name, title;
        int                     level = 1;
        int                     xp = 0;
        std::optional<HexCoord> stationed;   // governing an owned town
        int                     unpaidDays = 0;
        int                     woundedUntil = 0;   // wounded (no battles, no abilities) while day() < this
    };
    static constexpr int WOUND_DAYS = 3;   // a companion who falls in battle is out this long
    static constexpr int SULK_DAYS  = 3;   // unpaid this long: abilities stop
    static constexpr int LEAVE_DAYS = 7;   // unpaid this long: the SC leaves
    const std::vector<Special>& specials() const { return m_specials; }
    static std::vector<Ability> abilitiesOf(const std::string& id);
    static int xpForLevel(int level);      // total xp needed to reach 'level'
    static int upkeepFor(int level);       // gold per day
    bool hasAbility(const std::string& abilityName) const;  // active on an SC travelling with the hero
    void joinSpecial(const std::string& id);
    bool isWounded(const Special& sc) const { return day() < sc.woundedUntil; }
    // Companions who ride into the hero's next battle: travelling, unwounded, paid.
    struct Companion { std::string id; int level = 1; };
    std::vector<Companion> battleCompanions() const;
    // After a battle: each fallen companion is wounded for WOUND_DAYS, or lost
    // for good if the battle was lost as well.
    void companionsFell(const std::vector<std::string>& fallen, bool battleLost);
    std::optional<std::string> station(const std::string& id, bool stay);  // stay=false recalls
    int upkeepPerDay() const;

    // ── Rivals (Shariw AI, see Rivals.cpp) ───────────────────────────────────
    struct Rival {
        int                id = 0;
        std::string        name;
        HexCoord           pos;
        HexCoord           home;
        std::vector<Stack> army;
        double             startPower = 0;
        bool               alive = true;
        std::string        goal;      // what it is doing, for debugging/tests
    };
    struct RivalMove { int id; std::vector<HexCoord> path; };
    static constexpr int RIVAL_START_DAY = 3;    // war-bands ride from this day
    static constexpr int SECOND_BAND_DAY = 10;
    const std::vector<Rival>&     rivals() const { return m_rivals; }
    const std::vector<RivalMove>& lastRivalMoves() const { return m_rivalMoves; }
    const Rival* rivalAt(const HexCoord& c) const;
    // True when the pending encounter is a war-band attacking the hero (hero defends).
    bool pendingIsAmbush() const { return m_pendingAmbush; }
    bool lost() const { return m_lost; }
    const std::string& lostReason() const { return m_lostReason; }
    // Battle strength estimate used by the AI to pick fights.
    double power(const std::vector<Stack>& army) const;
    struct AutoResult {
        bool attackerWon = false;
        std::vector<Stack> attacker, defender;
        std::vector<std::string> fallen;   // attacker companions who fell
    };
    // Resolves a battle with no player involvement (AI vs guards) using the real
    // engine; `companions` fight on the attacking side.
    AutoResult autoBattle(const std::vector<Stack>& attacker, const std::vector<Stack>& defender,
                          const std::vector<Companion>& companions = {}, ArmyBonus attackerBonus = {}) const;
    // Runs the Shariw turn now (endDay calls this; exposed for tests).
    void runRivals();

    // ── Save / load (SessionSave.cpp) ────────────────────────────────────────
    // Only sessions started from files can be saved (the save names them).
    Scenario::Json saveState() const;
    std::optional<std::string> loadState(const Scenario::Json& save);

    // ── Calendar ─────────────────────────────────────────────────────────────
    // Applies income, weekly growth, resets movement. Returns the event text ("" if none).
    std::string endDay();
    int day()       const { return m_turns.day(); }
    int week()      const { return m_turns.week(); }
    int dayOfWeek() const { return m_turns.dayOfWeek(); }
    int month()     const { return m_turns.month(); }

    // ── State ────────────────────────────────────────────────────────────────
    const WorldMap&     map()       const { return m_map; }
    const HexCoord&     heroPos()   const { return m_hero.pos; }
    float               moves()     const { return m_moves; }
    float               movesMax()  const { return m_movesMax; }
    const ResourcePool& treasury()  const { return m_turns.playerFaction().treasury; }
    ResourcePool        dailyIncome(int faction = Faction::Player) const;
    int                 owner(const HexCoord& c) const;
    const ObjectControlMap& control() const { return m_control; }

    // ── Fog of war ───────────────────────────────────────────────────────────
    bool isVisible (const HexCoord& c) const { return m_visible.count(c)  > 0; }
    bool isExplored(const HexCoord& c) const { return m_explored.count(c) > 0; }
    const std::unordered_set<HexCoord>& visible()  const { return m_visible; }
    const std::unordered_set<HexCoord>& explored() const { return m_explored; }

private:
    std::optional<std::string> loadEncounters(const std::string& path);
    void growTown(const HexCoord& c);
    void ensureBuildings(const HexCoord& c);   // a town the player gains starts with the faction's starting set
    ResourcePool extraIncome(int faction) const; // beyond TurnManager's flat mine/town income
    void fireSightings(const std::vector<HexCoord>& revealed);
    void spawnRival(const HexCoord& town, int band);
    std::string visitSite(const HexCoord& cell);
    std::string collect(const HexCoord& cell, bool gold);
    void rivalTurn(Rival& r);
    void rivalRecruit(Rival& r);
    std::vector<HexCoord> rivalRoute(const Rival& r, const HexCoord& to) const;
    float rivalStepCost(const HexCoord& c) const;
    std::vector<Stack> guardsOf(const HexCoord& c) const;
    void report(const std::string& speaker, const std::string& text);
    std::string adviser() const;   // who reports news: Ushari once she rides with you
    void grantXp(int xp);
    void payUpkeep();
    float movesBonus() const;
    int   sightBonus() const;
    void recomputeVisibility();
    void revealFrom(const HexCoord& origin, int radius);
    std::string enter(const HexCoord& cell);

    WorldMap                         m_map;
    std::unique_ptr<ResourceManager> m_resources;
    TurnManager                      m_turns;
    Hero                             m_hero;
    float                            m_moves    = DEFAULT_MOVES;
    float                            m_movesMax = DEFAULT_MOVES;
    ObjectControlMap                 m_control;
    TownStateMap                     m_towns;
    std::unordered_set<HexCoord>     m_visible;
    std::unordered_set<HexCoord>     m_explored;
    std::unordered_map<HexCoord, Encounter> m_encounters;  // uncleared only
    std::unordered_map<HexCoord, MineFind>  m_mineFinds;   // old mine contents
    std::optional<HexCoord>          m_pending;
    std::optional<HexCoord>          m_passage;
    bool                             m_won = false;
    Scenario                         m_scenario;
    std::unordered_set<HexCoord>     m_ruledOut;
    std::unordered_set<std::string>  m_items;
    std::map<std::string, std::string> m_equipped;   // slot → item id
    std::unordered_map<HexCoord, std::vector<Stack>> m_garrisons;
    std::vector<Special>             m_specials;
    HeroProgress                     m_heroProgress;
    struct StartArgs { std::string map, data, encounters, triggers; uint32_t seed = 0; };
    std::optional<StartArgs>         m_startArgs;   // set when started from files
    std::vector<Rival>               m_rivals;
    std::vector<RivalMove>           m_rivalMoves;
    bool                             m_pendingAmbush = false;
    bool                             m_pendingZone = false;  // guard attacked from its zone of control
    bool                             m_lost = false;
    std::string                      m_lostReason;
    mutable Encounter                m_rivalEncounter;  // synthesized for encounterAt()
    std::unordered_map<HexCoord, Pickup> m_pickups;        // uncollected only
    std::unordered_map<HexCoord, int>    m_siteWeek;       // weekly sites: week last used
    std::unordered_set<HexCoord>         m_visitedOnce;    // one-time sites already used
    std::optional<HexCoord>              m_pendingChest;
    int                                  m_stablesWeek = 0;
};
