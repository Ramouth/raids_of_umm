#pragma once
#include "CampaignState.h"
#include "Scenario.h"
#include "core/ResourceManager.h"
#include "core/TurnManager.h"
#include "hero/Hero.h"
#include "world/ObjectControl.h"
#include "world/TownState.h"
#include "world/WorldMap.h"
#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <map>
#include <optional>
#include <string>
#include <set>
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
 * runs the battle and calls resolveEncounter(). A scenario may name its survey mine; legacy maps choose one at random.
 * Clearing it either wins or starts a scripted discovery.
 */
// Bonuses every troop stack of the hero's army fights with (commander's
// items, town buildings such as the Drill Yard).
struct ArmyBonus { int attack = 0, defense = 0, speed = 0; bool readiedShot = false; };
class CombatEngine;

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
        std::vector<std::string> items; // reward "items": more ids granted on victory (keepsakes)
        std::string layout = "field";   // battlefield (combat_layouts.json id); from the file, never saved
    };

    // What the last resolveEncounter() handed over: the reward (Warlord bonus
    // included), an old mine's loot, and items newly gained (one already
    // carried is not listed). Empty after a defeat, a retreat or a war-band.
    // xp: the experience the commander actually gained for the victory, after
    // the level cap (a war-band's too); 0 after a defeat. Not part of empty().
    struct Spoils {
        ResourcePool resources;
        std::vector<std::string> items;
        int xp = 0;
        bool empty() const {
            return items.empty() && std::all_of(resources.amounts.begin(), resources.amounts.end(),
                                                [](int n) { return n == 0; });
        }
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
        int          xp = 0;     // tome: experience; chest: the experience alternative
        bool         chest = false;   // a choice of gold or experience (only if data gives it xp)
    };
    static constexpr int   WATCHTOWER_RADIUS = 9;
    static constexpr float STABLES_BONUS     = 3.0f;  // extra moves per day until week's end
    static constexpr int   LEARNING_XP       = 400;
    static constexpr int   TOME_XP           = 150;   // a war journal, read once

    // Loads the map + data registry and seeds ownership from map factionIds.
    // Hero starts at the map's "start" cell; without one, at the first player-owned
    // town (else first town, else first passable tile). With no carried state the
    // army is the triggers' start_army and the treasury its start_resources (if any).
    // encountersPath: optional JSON of guard armies + rewards (see data/maps/*.encounters.json).
    // seed picks the discovery mine unless the scenario names a survey_site.
    // triggersPath: optional Scenario script (quests, dialogue, clues).
    std::optional<std::string> start(const std::string& mapPath, const std::string& dataDir,
                                     const std::string& encountersPath = "", uint32_t seed = 0,
                                     const std::string& triggersPath = "");
    std::optional<std::string> start(WorldMap map, const std::string& dataDir,
                                     const std::string& encountersPath = "", uint32_t seed = 0,
                                     const std::string& triggersPath = "");
    // A campaign map: as above, then the carried state (flags, level cap and,
    // once carried, commander, army, items, companions, treasury) is applied
    // after the per-map reset and before the "start" triggers fire.
    std::optional<std::string> start(const std::string& mapPath, const std::string& dataDir,
                                     const std::string& encountersPath, uint32_t seed,
                                     const std::string& triggersPath, const CampaignState& carry);
    std::optional<std::string> start(WorldMap map, const std::string& dataDir,
                                     const std::string& encountersPath, uint32_t seed,
                                     const std::string& triggersPath, const CampaignState& carry);
    // What this map hands to the next: carried parts, flags, and this map's
    // stats in lastMap. The campaign position is the Campaign's to fill.
    CampaignState carryOut() const;

    // ── Story flags (the campaign's memory; carried and saved) ───────────────
    const Scenario::Json& flags() const { return m_flags; }
    void setFlag(const std::string& name, const Scenario::Json& value) { m_flags[name] = value; }
    // The map the ending names ("ending": {"next": id}); "" = next in order.
    std::string nextMap() const { return m_scenario.nextMap(); }

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
    // The fight about to start is not fought; the hero stays where travel stopped.
    void cancelEncounter() { m_pending.reset(); m_pendingAmbush = false; m_pendingZone = false; }
    // The story choice raised at this fight's "engage" is still open: the fight
    // waits for the answer (an option may withdraw the camp). resolveEncounter()
    // refuses while it holds (nothing changes, the fight stays pending).
    bool encounterAwaitsChoice() const;
    // Story: the named guard camp or old mine is gone (vanish / withdraw). Its hex
    // and zone of control clear and a fight pending with it is called off.
    // False = no standing encounter by that name.
    bool removeEncounter(const std::string& objectName);
    // Called after the battle. Victory clears the hex, pays the reward and moves the
    // hero onto it (spending its step cost). Returns what an old mine held.
    // bitten: rabies (see CombatEngine::kBiteChance); kept per unit id in the hero's army.
    struct Stack { std::string id; int count = 0; bool bitten = false; };
    MineFind resolveEncounter(bool victory, const std::optional<std::vector<Stack>>& enemySurvivors = std::nullopt);
    const Spoils& lastSpoils() const { return m_spoils; }
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

    std::vector<Stack> army() const;
    // Replaces the hero's army (e.g. with combat survivors). Unknown ids are dropped.
    // Stacks of one id merge; the merged stack is bitten if any of them was.
    // At most Hero::ARMY_SLOTS (5) stacks ride: stacks of a sixth (or later) unit
    // are left out, logged to stderr, and returned (empty = everyone fit).
    std::vector<Stack> setArmy(const std::vector<Stack>& stacks);
    std::string unitName(const std::string& unitId) const;   // display name (the id if unknown)
    // Rabies: unit ids in the hero's army that carry a bite. A stack that dies
    // out (or leaves the army) loses its mark. Ending a day in a player-owned
    // town tends them all; endDay() then says so and tendedIn() names the town.
    bool isBitten(const std::string& unitId) const { return m_bitten.count(unitId) > 0; }
    const std::string& tendedIn() const { return m_tendedIn; }   // "" = no one tended last endDay()

    // ── Garrisons (HoMM3-style: troops left at an owned mine or town) ────────
    const std::vector<Stack>* garrison(const HexCoord& c) const;
    // Moves 'count' of a unit between the hero (standing there) and the site's garrison.
    // toGarrison=false moves them back. Returns an error, or nullopt.
    std::optional<std::string> transfer(const HexCoord& site, const std::string& unitId,
                                        int count, bool toGarrison);

    // ── Towns ────────────────────────────────────────────────────────────────
    // Faction roster a town recruits from, by owner (1 = Ivory Compact, 2 = Shariw).
    static std::string rosterFor(int owner);
    std::string townRoster(const HexCoord& c) const;
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
    std::vector<std::string> backpack() const;   // owned, not worn, not keepsakes (sorted)
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
    // hunts: the band rides for the hero and attacks whenever it reaches him,
    // whatever the odds (Rival::hunts); otherwise it picks goals as any war-band.
    bool betray(const std::string& objectName, const std::string& bandName, const std::vector<Stack>& army,
                bool hunts = false);
    // Every soldier of `unitId` in the hero's army and garrisons turns on the
    // player: they leave, form a war-band beside the hero and attack at once.
    // Returns how many turned (0 = none were hired, nothing happens).
    int turncoats(const std::string& unitId, const std::string& bandName);
    bool ambushFrom(const std::string& townName, const std::string& bandName, bool hunts = false);
    // Story "disband": every living war-band of that name leaves the map. No
    // battle, no experience, no "rival_beaten"; sites it took stay as they are.
    // A fight pending with it is called off. False if no such band is alive.
    bool disband(const std::string& bandName);
    // Rules out one wrong old mine not yet searched or ruled out; returns its name.
    std::optional<std::string> giveClue();
    const std::unordered_set<HexCoord>& ruledOut() const { return m_ruledOut; }
    void addItem(const std::string& id);
    bool hasItem(const std::string& id) const { return m_items.count(id) > 0; }
    // Story "take_item": the hero hands it over (taken off if worn). False = not carried.
    bool takeItem(const std::string& id);
    // Keepsakes carried (items.json "kind": "keepsake"), sorted by id: read, never worn.
    std::vector<const WondrousItem*> keepsakes() const;
    // Story "open": [q, r] — a closed tile (map "passable": false, e.g. a
    // gatekeeper's bridge) becomes passable for the rest of this map. Saved,
    // not carried. False if the map has no such tile.
    bool openTile(const HexCoord& c);
    const std::unordered_set<HexCoord>& opened() const { return m_opened; }
    // The map's passability with the opened tiles applied (false off the map).
    bool passable(const HexCoord& c) const;
    const std::unordered_set<std::string>& items() const { return m_items; }
    // Accept a scenario offer at the hero's current object. Error or nullopt.
    std::optional<std::string> acceptOffer(const std::string& id);
    int minesHeld(int faction = Faction::Player) const;

    // ── Hero growth (Specials.cpp) ───────────────────────────────────────────
    // The commander levels from the same experience as the companions, up to
    // MAX_HERO_LEVEL (then the campaign hands you a new hero). Experience is
    // rare off the battlefield: fights, quests and the odd war journal.
    static constexpr int MAX_HERO_LEVEL = 10;
    struct HeroProgress { int level = 1; int xp = 0; };
    const HeroProgress& heroProgress() const { return m_heroProgress; }
    // Each map caps the commander's level (campaign max_level, else MAX_HERO_LEVEL).
    // At the cap, experience past xpForLevel(cap) is lost, not banked; it is counted.
    int levelCap() const { return m_levelCap; }
    int capReachedDay() const { return m_capReachedDay; }   // first day at the cap on this map, -1 = not yet
    int xpLostToCap() const { return m_xpLostToCap; }
    // Experience a victory at 'c' would give (guards, old mines, war-bands); 0 = none.
    int encounterXp(const HexCoord& c) const;
    // Experience for the commander and every travelling companion (victories, story rewards).
    void grantXp(int xp);

    // The faction's paths (data/hero_tree.json). At level 2 the player chooses
    // one for good; every level then grants that path's skill for the level.
    // The Veined is the wildcard: shown, offered by the story, never chosen here.
    struct HeroEffects {
        int fieldworks = 0;
        int tactics = 0, attack = 0, defense = 0, moves = 0, sight = 0, gold = 0;
        double growth = 0;
        bool readiedShot = false;
    };
    struct TreeNode {
        int level = 0;
        std::string id, name, text;
        bool live = false;          // has an effect in the demo
        HeroEffects effect;
    };
    struct TreePath { std::string id, name, text; std::vector<TreeNode> nodes; };
    const std::vector<TreePath>& heroPaths() const { return m_paths; }
    const TreePath& wildcardPath() const { return m_wildcard; }
    const std::string& heroPath() const { return m_path; }
    bool needsPath() const { return m_path.empty() && m_heroProgress.level >= 2; }
    std::optional<std::string> choosePath(const std::string& pathId);
    bool learned(const std::string& nodeId) const;
    HeroEffects heroEffects() const;          // everything the commander's skills add up to
    int fieldworkCapacity() const { return std::min(3, heroEffects().fieldworks); }
    const std::map<std::string, int>& fieldworkStock() const { return m_fieldworkStock; }
    std::optional<std::string> buyFieldwork(const std::string& kind);
    int tacticsRank() const { return heroEffects().tactics; }   // stacks ordered first (0–3)
    // Level-ups since the last drain, for the UI's pop-up. skill "" = choose a path.
    struct LevelUp { int level = 0; std::string skill, text; };
    std::vector<LevelUp> drainLevelUps();

    // ── Special characters (see Specials.cpp) ────────────────────────────────
    // Companions are units of faction "companion" in units.json (name, title,
    // upkeep, lines). At most MAX_RIDING ride with the hero; others govern a
    // town (stationed) or wait (departed with waiting = true).
    struct Ability { int level; std::string name, text; };
    struct Special {
        std::string             id, name, title;
        int                     level = 1;
        int                     xp = 0;
        std::optional<HexCoord> stationed;   // governing an owned town
        int                     unpaidDays = 0;
        int                     woundedUntil = 0;   // wounded (no battles, no abilities) while day() < this
        bool                    waiting = false;    // departed: stayed behind for want of room (not left)
    };
    static constexpr int MAX_RIDING = Hero::SC_SLOTS;   // 2 companions ride with the hero
    static constexpr int WOUND_DAYS = 3;   // a companion who falls in battle is out this long
    static constexpr int SULK_DAYS  = 3;   // unpaid this long: abilities stop
    static constexpr int LEAVE_DAYS = 7;   // unpaid this long: the SC leaves
    const std::vector<Special>& specials() const { return m_specials; }
    // Gone for now with their progression: left (story "leave", unpaid) or waiting.
    const std::vector<Special>& departedSpecials() const { return m_departedSpecials; }
    static std::vector<Ability> abilitiesOf(const std::string& id);
    static int xpForLevel(int level);      // total xp needed to reach 'level'
    static int companionXpForLevel(const std::string& id, int level);
    static int upkeepFor(int level);       // the standard wage table, gold per day
    // This companion's wage: the table scaled by its units.json "upkeep" (0 = none).
    int upkeepOf(const Special& sc) const;
    // What a companion says at a moment ("join", "unpaid", "paid", "leaves",
    // "wounded"): units.json "lines", else a neutral default ("join": none).
    std::string companionLine(const std::string& id, const std::string& moment) const;
    bool hasAbility(const std::string& abilityName) const;  // active on an SC travelling with the hero
    int  ridingCount() const;              // companions travelling with the hero (not stationed)
    // A companion joins (story "join"; a departed or waiting one comes back with
    // her progression). If that makes three riding, the story choice "Who stays
    // behind?" opens ({"id": "stay_behind", "kind": "party", "newcomer": id,
    // options: one per riding companion and the newcomer, essential companions
    // left out, each {"id": companion id, "then": [{"stay_behind": id}]}}).
    // Adventure waits for the answer; until then three ride, and battles field two.
    void joinSpecial(const std::string& id);
    void leaveSpecial(const std::string& id);  // retain progression for a later reunion
    // The companion stops riding: she governs the nearest town the player holds
    // (hex distance from the hero), else waits off the map (departed, waiting) until
    // the story brings her back. Pushes a "party" story event. False = no such rider.
    bool stayBehind(const std::string& id);
    void completeChapter(const std::string& returnTo = "");
    std::string departureRoute() const;
    bool rivalMobilized() const;
    bool isWounded(const Special& sc) const { return day() < sc.woundedUntil; }
    // Companions who ride into the hero's next battle: travelling, unwounded, paid;
    // at most MAX_RIDING of them (the earliest to join).
    struct Companion { std::string id; int level = 1; };
    std::vector<Companion> battleCompanions() const;
    // After a battle: each fallen companion is wounded for WOUND_DAYS, or lost
    // for good if the battle was lost as well.
    void companionsFell(const std::vector<std::string>& fallen, bool battleLost);
    // stay=false recalls (refused while MAX_RIDING already ride).
    std::optional<std::string> station(const std::string& id, bool stay);
    int upkeepPerDay() const;

    // ── Combat tips (data/combat_tips.json, see combat/CombatTips.h) ────────
    // Tips already shown, once per campaign: saved, and carried map to map.
    const std::set<std::string>& shownTips() const { return m_shownTips; }
    void markTipShown(const std::string& tipId) { if (!tipId.empty()) m_shownTips.insert(tipId); }
    // The campaign map this session plays ("1.0"; "" = a map started on its own).
    const std::string& campaignMap() const { return m_campaignMap; }

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
        bool               hunts = false;  // rides for the hero, whatever the odds (saved)
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
    // engine; `companions` fight on the attacking side (at most MAX_RIDING, and
    // at most five troop stacks a side). `watch`, if set, sees the engine at the
    // start and after every turn (demo_bot's combat tips). `layout` is a
    // combat_layouts.json id (unknown or "field": the open field).
    AutoResult autoBattle(const std::vector<Stack>& attacker, const std::vector<Stack>& defender,
                          const std::vector<Companion>& companions = {}, ArmyBonus attackerBonus = {},
                          const std::function<void(const CombatEngine&)>& watch = nullptr,
                          const std::string& layout = "field") const;
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
    std::optional<std::string> startMap(WorldMap map, const std::string& dataDir, const std::string& encountersPath,
                                        uint32_t seed, const std::string& triggersPath, const CampaignState* carry);
    void applyCarry(const CampaignState& carry);
    void capXp();                                // enforce the level cap on the commander's experience
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
    void rivalBuild();
    std::vector<HexCoord> rivalRoute(const Rival& r, const HexCoord& to) const;
    float rivalStepCost(const HexCoord& c) const;
    std::vector<Stack> guardsOf(const HexCoord& c) const;
    void report(const std::string& speaker, const std::string& text);
    std::string adviser() const;   // who reports news: Ushari once she rides with you
    void loadHeroTree(const std::string& path);
    // Puts the companion with the hero (from the departed/waiting list, else from
    // units.json): no line, no limit. Null if unknown or already riding.
    Special* addSpecial(const std::string& id);
    void askWhoStaysBehind(const std::string& newcomer);
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
    std::unordered_set<std::string>  m_items;      // commander items and keepsakes
    Spoils                           m_spoils;     // from the last resolveEncounter()
    std::set<std::string>            m_bitten;     // rabies, by unit id in the hero's army
    std::string                      m_tendedIn;   // town where the bitten were tended last endDay()
    std::unordered_set<HexCoord>     m_opened;     // tiles the story opened ("open")
    std::map<std::string, std::string> m_equipped;   // slot → item id
    std::unordered_map<HexCoord, std::vector<Stack>> m_garrisons;
    std::map<std::string, int>        m_fieldworkStock;
    std::vector<Special>             m_specials;
    std::vector<Special>             m_departedSpecials;
    std::set<std::string>            m_shownTips;    // combat tips shown this campaign
    std::string                      m_campaignMap;  // campaign map id ("" = none)
    HeroProgress                     m_heroProgress;
    int                              m_levelCap = MAX_HERO_LEVEL;
    int                              m_capReachedDay = -1;
    int                              m_xpLostToCap = 0;
    Scenario::Json                   m_flags = Scenario::Json::object();
    std::vector<TreePath>            m_paths;
    TreePath                         m_wildcard;
    std::string                      m_path;      // chosen at level 2 ("" = not yet)
    std::vector<std::string>         m_learned;   // path skills granted, in order
    std::vector<LevelUp>             m_levelUps;  // for the pop-up
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
