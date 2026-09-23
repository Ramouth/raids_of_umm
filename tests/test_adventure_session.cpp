// test_adventure_session.cpp — AdventureSession: calendar, movement points,
// fog of war, and capturing objects. Uses the real data/ registry.
#include "test_runner.h"
#include "adventure/AdventureSession.h"
#include <fstream>

namespace {

// Radius-8 sand map: player town at (-6,0), gold mine at (-3,0),
// old mine at (0,0), neutral town at (6,0).
WorldMap testMap() {
    WorldMap map;
    map.clear(8);
    map.placeObject({{-6, 0}, ObjType::Town,     "Home",     1});
    map.placeObject({{-3, 0}, ObjType::GoldMine, "Mine",     0});
    map.placeObject({{ 0, 0}, ObjType::OldMine,  "Old Mine", 0});
    map.placeObject({{ 6, 0}, ObjType::Town,     "Far Town", 0});
    return map;
}

AdventureSession started(WorldMap map = testMap()) {
    AdventureSession s;
    auto err = s.start(std::move(map), "data");
    CHECK(!err);
    return s;
}

} // namespace

SUITE("AdventureSession — starts at the player town on day 1") {
    auto s = started();
    CHECK(s.heroPos() == HexCoord(-6, 0));
    CHECK_EQ(s.day(), 1);
    CHECK_EQ(s.week(), 1);
    CHECK_EQ(s.dayOfWeek(), 1);
    CHECK(s.moves() == s.movesMax());
    CHECK(s.movesMax() == AdventureSession::DEFAULT_MOVES + 1.0f);     // Ushari's Drillmaster
    CHECK_EQ(s.treasury()[Resource::Gold], AdventureSession::STARTING_GOLD);
    CHECK_EQ(s.owner({-6, 0}), 1);
    CHECK_EQ(s.owner({6, 0}), 0);
}

SUITE("AdventureSession — travel spends weighted movement points") {
    auto s = started();
    auto steps = s.travel({-3, 0});  // three sand hexes
    CHECK_EQ((int)steps.size(), 3);
    CHECK(s.heroPos() == HexCoord(-3, 0));
    CHECK(s.moves() == s.movesMax() - 3.0f);
}

SUITE("AdventureSession — travel stops when points run out") {
    auto s = started();
    auto steps = s.travel({6, 0});  // 12+ hexes (detours round the old mine), only 11 points
    CHECK_EQ((int)steps.size(), 11);
    CHECK(s.heroPos() != HexCoord(6, 0));
    CHECK(s.moves() == 0.0f);
    CHECK(s.travel({6, 0}).empty());  // nothing left today
}

SUITE("AdventureSession — dunes cost more, roads cost less") {
    WorldMap map = testMap();
    MapTile dune = makeTile(Terrain::Dune);    // 1.5
    MapTile road = makeTile(Terrain::Sand);
    road.road = true;
    road.moveCost = 0.5f;
    map.setTile({-5, 0}, road);
    map.setTile({-4, 0}, dune);
    auto s = started(std::move(map));
    auto path = s.route({-4, 0});
    CHECK_EQ((int)path.size(), 2);
    CHECK(s.routeCost(path) == 2.0f);
}

SUITE("AdventureSession — entering a mine captures it; old mines are not captured") {
    auto s = started();
    auto steps = s.travel({-3, 0});
    CHECK(steps.back().capture == "Mine");
    CHECK_EQ(s.owner({-3, 0}), 1);
    s.endDay();
    s.travel({0, 0});
    CHECK_EQ(s.owner({0, 0}), 0);
}

SUITE("AdventureSession — end day pays income and resets movement") {
    auto s = started();
    s.travel({-3, 0});                                   // take the gold mine
    int gold  = s.treasury()[Resource::Gold];
    int daily = s.dailyIncome()[Resource::Gold];
    CHECK_EQ(daily, 1500);                               // town 500 + mine 1000
    s.endDay();
    CHECK_EQ(s.day(), 2);
    CHECK_EQ(s.treasury()[Resource::Gold], gold + daily - s.upkeepPerDay());
    CHECK(s.moves() == s.movesMax());
}

SUITE("AdventureSession — seven days make a week") {
    auto s = started();
    std::string event;
    for (int i = 0; i < 6; ++i) event = s.endDay();
    CHECK_EQ(s.day(), 7);
    CHECK(!event.empty());                               // weekly growth day
    event = s.endDay();
    CHECK_EQ(s.day(), 8);
    CHECK_EQ(s.week(), 2);
    CHECK_EQ(s.dayOfWeek(), 1);
}

SUITE("AdventureSession — fog: sight radius, exploration memory") {
    auto s = started();
    CHECK(s.isVisible({-6, 0}));
    CHECK(s.isVisible({-2, 0}));                          // 4 away
    CHECK(!s.isExplored({-1, 0}));                        // 5 away
    auto steps = s.travel({-3, 0});
    CHECK(s.isVisible({0, 0}));
    int revealed = 0;
    for (const auto& st : steps) revealed += (int)st.revealed.size();
    CHECK(revealed > 0);
    s.endDay();
    s.travel({2, 0});
    CHECK(s.isExplored({-7, 0}));                         // remembered
    CHECK(s.isVisible({-3, 0}));                          // owned mine keeps watch
}

SUITE("AdventureSession — mountains block sight") {
    WorldMap map = testMap();
    MapTile wall = makeTile(Terrain::Mountain);
    wall.passable = false;
    for (int r = -8; r <= 8; ++r)
        if (map.hasTile({-4, r})) map.setTile({-4, r}, wall);
    auto s = started(std::move(map));
    CHECK(s.isVisible({-4, 0}));                          // the mountain itself is seen
    CHECK(!s.isExplored({-3, 0}));                        // but not what lies behind
}

// ── Encounters ────────────────────────────────────────────────────────────────

namespace {

// Mountain wall at q=-2 with one gap at (-2,0) held by a guard.
WorldMap guardedMap() {
    WorldMap map = testMap();
    MapTile wall = makeTile(Terrain::Mountain);
    wall.passable = false;
    for (int r = -8; r <= 8; ++r)
        if (r != 0 && map.hasTile({-2, r})) map.setTile({-2, r}, wall);
    map.placeObject({{-2, 0}, ObjType::Guard, "Pass Guard", 0});
    return map;
}

std::string writeEncounters() {
    const std::string path = "/tmp/raids_test_encounters.json";
    std::ofstream f(path);
    f << R"({"defaults":{"old_mine":{"guards":[{"id":"mummy","count":2}]}},
             "objects":{"Pass Guard":{"guards":[{"id":"skeleton_warrior","count":9}],
                                      "reward":{"Gold":500},"item":"scarab_amulet"}}})";
    return path;
}

} // namespace

SUITE("AdventureSession — a guard blocks the only pass") {
    AdventureSession s;
    CHECK(!s.start(guardedMap(), "data"));
    CHECK(s.isEncounter({-2, 0}));
    CHECK(s.route({0, 0}).empty());                // cannot path through the guard
    CHECK(!s.route({-2, 0}).empty());              // but can target it
}

SUITE("AdventureSession — travelling into a guard stops beside it") {
    AdventureSession s;
    CHECK(!s.start(guardedMap(), "data", writeEncounters()));
    auto steps = s.travel({-2, 0});
    CHECK(s.heroPos() == HexCoord(-3, 0));
    CHECK(s.pendingEncounter().has_value());
    const auto* enc = s.encounterAt({-2, 0});
    CHECK(enc != nullptr);
    if (enc) {
        CHECK(enc->guardsJson.find("skeleton_warrior") != std::string::npos);
        CHECK_EQ(enc->reward[Resource::Gold], 500);
        CHECK(enc->item == "scarab_amulet");
    }
}

SUITE("AdventureSession — defeat leaves the guard; victory clears and pays") {
    AdventureSession s;
    CHECK(!s.start(guardedMap(), "data", writeEncounters()));
    s.travel({-2, 0});
    s.resolveEncounter(false);
    CHECK(s.isEncounter({-2, 0}));
    CHECK(s.heroPos() == HexCoord(-3, 0));

    s.travel({-2, 0});
    int gold = s.treasury()[Resource::Gold];
    s.resolveEncounter(true);
    CHECK(!s.isEncounter({-2, 0}));
    CHECK(s.heroPos() == HexCoord(-2, 0));
    CHECK_EQ(s.treasury()[Resource::Gold], gold + 500);
    CHECK(!s.route({0, 0}).empty());               // the pass is open
}

SUITE("AdventureSession — one old mine hides the passage and wins") {
    WorldMap map = testMap();
    map.placeObject({{0, 3}, ObjType::OldMine, "Second Old Mine", 0});
    AdventureSession s;
    CHECK(!s.start(std::move(map), "data", writeEncounters(), 7));
    CHECK(s.passageMine().has_value());
    HexCoord passage = *s.passageMine();
    HexCoord other   = passage == HexCoord(0, 0) ? HexCoord(0, 3) : HexCoord(0, 0);

    int finds = 0;
    for (HexCoord target : {other, passage}) {
        for (int day = 0; day < 3 && !s.pendingEncounter(); ++day) {
            s.travel(target);
            if (!s.pendingEncounter()) s.endDay();
        }
        CHECK(s.pendingEncounter().has_value());
        auto find = s.resolveEncounter(true);
        if (target == passage) CHECK(find == AdventureSession::MineFind::Passage);
        else                   CHECK(find != AdventureSession::MineFind::Passage);
        ++finds;
        s.endDay();
    }
    CHECK_EQ(finds, 2);
    CHECK(s.won());
}

SUITE("AdventureSession — the passage mine depends on the seed") {
    WorldMap base = testMap();
    for (int i = 1; i <= 3; ++i)
        base.placeObject({{i * 2 - 4, 4}, ObjType::OldMine, "Mine " + std::to_string(i), 0});
    std::unordered_set<HexCoord> picks;
    for (uint32_t seed = 0; seed < 20; ++seed) {
        AdventureSession s;
        CHECK(!s.start(base.clone(), "data", "", seed));
        picks.insert(*s.passageMine());
    }
    CHECK(picks.size() > 1);
}

// ── Towns and recruiting ──────────────────────────────────────────────────────

SUITE("AdventureSession — player towns start with a week of their roster") {
    auto s = started();
    const TownState* home = s.town({-6, 0});
    CHECK(home != nullptr);
    if (home) {
        CHECK(home->recruitPool.count("levy_spearman") == 1);
        CHECK(home->recruitPool.count("skeleton_warrior") == 0);     // neutral creature
        CHECK(home->recruitPool.count("shariw_scout") == 0);         // other roster
        CHECK_EQ(home->recruitPool.at("levy_spearman"), s.resources().unit("levy_spearman")->weeklyGrowth);
    }
    const TownState* far = s.town({6, 0});
    CHECK(far != nullptr && far->recruitPool.empty());                // neutral town recruits nothing
}

SUITE("AdventureSession — recruiting spends gold and fills the army") {
    auto s = started();
    int gold = s.treasury()[Resource::Gold];
    CHECK(!s.recruit({-6, 0}, "levy_spearman", 10));
    CHECK_EQ(s.treasury()[Resource::Gold], gold - 10 * s.resources().unit("levy_spearman")->cost[Resource::Gold]);
    auto army = s.army();
    CHECK_EQ((int)army.size(), 1);
    if (!army.empty()) CHECK(army[0].id == "levy_spearman" && army[0].count == 10);
    CHECK(s.recruit({-6, 0}, "levy_spearman", 999).has_value());      // pool limit
    CHECK(s.recruit({-6, 0}, "rider_knight", 2).has_value());         // cannot afford obsidian
}

SUITE("AdventureSession — must stand in an owned town to recruit") {
    auto s = started();
    s.travel({-5, 0});
    CHECK(s.recruit({-6, 0}, "levy_spearman", 1).has_value());
    CHECK(s.recruit({6, 0}, "levy_spearman", 1).has_value());
}

SUITE("AdventureSession — weekly growth refills pools; capture swaps roster") {
    auto s = started();
    int start = s.town({-6, 0})->recruitPool.at("levy_spearman");
    for (int i = 0; i < 6; ++i) s.endDay();                            // day 7
    CHECK_EQ(s.town({-6, 0})->recruitPool.at("levy_spearman"), start * 2);
    CHECK(s.town({6, 0})->recruitPool.empty());
    for (int day = 0; day < 3 && s.owner({6, 0}) != 1; ++day) { s.travel({6, 0}); s.endDay(); }
    CHECK_EQ(s.owner({6, 0}), 1);
    CHECK(s.town({6, 0})->recruitPool.count("levy_spearman") == 1);
}

SUITE("AdventureSession — setArmy keeps known stacks") {
    auto s = started();
    s.setArmy({{"levy_spearman", 5}, {"not_a_unit", 3}, {"desert_archer", 0}});
    auto army = s.army();
    CHECK_EQ((int)army.size(), 1);
}

// ── Scenario: triggers, quests, offers, clues ────────────────────────────────

namespace {

std::string writeTriggers() {
    const std::string path = "/tmp/raids_test_triggers.json";
    std::ofstream f(path);
    f << R"({
      "quests": [{"id": "main", "title": "Find the passage", "text": "Search.", "main": true},
                 {"id": "tribute", "title": "Tribute", "text": "Pay."}],
      "offers": [{"id": "pay", "label": "Pay 500 gold", "at": "Mine", "cost": {"Gold": 500},
                  "quest": "tribute", "then": [{"clue": true}]}],
      "triggers": [
        {"id": "intro", "when": {"event": "start"}, "do": [{"say": [["Ushari", "We march."]]}, {"quest": "main"}]},
        {"id": "seen", "when": {"event": "see_type", "type": "old_mine"}, "do": [{"say": [["Ushari", "An old mine!"]]}]},
        {"id": "visit", "when": {"event": "visit", "name": "Mine"}, "do": [{"quest": "tribute"}, {"offer": "pay"}]},
        {"id": "day3", "when": {"event": "day", "day": 3}, "do": [{"give": {"Wood": 7}}, {"reveal": [6, 0, 1]}]}
      ]})";
    return path;
}

AdventureSession storySession() {
    WorldMap map = testMap();
    map.placeObject({{0, 3}, ObjType::OldMine, "Second Old Mine", 0});
    map.placeObject({{0, -3}, ObjType::OldMine, "Third Old Mine", 0});
    AdventureSession s;
    CHECK(!s.start(std::move(map), "data", "", 3, writeTriggers()));
    return s;
}

} // namespace

SUITE("Scenario — start trigger speaks and adds the main quest") {
    auto s = storySession();
    auto lines = s.scenario().drainLines();
    CHECK(!lines.empty());
    if (!lines.empty()) CHECK(lines[0].speaker == "Ushari");
    CHECK_EQ((int)s.scenario().quests().size(), 1);
    CHECK(s.scenario().drainLines().empty());                    // drained
}

SUITE("Scenario — sighting and visiting fire once") {
    auto s = storySession();
    s.scenario().drainLines();
    s.travel({-3, 0});                                           // old mine (0,0) comes into view
    auto lines = s.scenario().drainLines();
    bool sawMine = false;
    for (const auto& l : lines) sawMine = sawMine || l.text == "An old mine!";
    CHECK(sawMine);
    CHECK_EQ((int)s.scenario().quests().size(), 2);              // visit added the tribute quest
    CHECK_EQ((int)s.scenario().offersAt("Mine").size(), 1);
}

SUITE("Scenario — accepting an offer pays, completes the quest and rules out a mine") {
    auto s = storySession();
    s.travel({-3, 0});
    int gold = s.treasury()[Resource::Gold];
    CHECK(!s.acceptOffer("pay"));
    CHECK_EQ(s.treasury()[Resource::Gold], gold - 500);
    CHECK_EQ((int)s.ruledOut().size(), 1);
    CHECK(!s.ruledOut().count(*s.passageMine()));                // never the real passage
    CHECK(s.acceptOffer("pay").has_value());                     // one time only
    bool done = false;
    for (const auto& q : s.scenario().quests()) if (q.id == "tribute") done = q.done;
    CHECK(done);
}

SUITE("Scenario — day trigger gives resources and reveals") {
    auto s = storySession();
    CHECK(!s.isExplored({6, 0}));
    s.endDay();
    s.endDay();                                                  // day 3
    CHECK_EQ(s.treasury()[Resource::Wood], 5 + 7);
    CHECK(s.isExplored({6, 0}));
}

SUITE("Scenario — clues run out after every false mine") {
    auto s = storySession();
    CHECK(s.giveClue().has_value());
    CHECK(s.giveClue().has_value());
    CHECK(!s.giveClue().has_value());                            // 3 mines, 1 is the passage
}

// ── Rivals (Shariw AI) ────────────────────────────────────────────────────────

namespace {

// Player town west, a player-owned mine in the middle, Shariw town east.
WorldMap rivalMap() {
    WorldMap map;
    map.clear(8);
    map.placeObject({{-6, 0}, ObjType::Town,     "Home",     1});
    map.placeObject({{ 1, 0}, ObjType::GoldMine, "Mine",     1});
    map.placeObject({{ 6, 0}, ObjType::Town,     "Ain Sharu", 2});
    return map;
}

} // namespace

SUITE("Rivals — a war-band spawns at the Shariw town and waits until day 3") {
    AdventureSession s;
    CHECK(!s.start(rivalMap(), "data"));
    CHECK_EQ((int)s.rivals().size(), 1);
    CHECK(s.rivals()[0].pos == HexCoord(6, 0));
    CHECK(s.isEncounter({6, 0}));                    // the war-band blocks its hex
    s.endDay();                                       // evening of day 1
    CHECK(s.rivals()[0].pos == HexCoord(6, 0));
}

SUITE("Rivals — the war-band rides out and steals the player's mine") {
    AdventureSession s;
    CHECK(!s.start(rivalMap(), "data"));
    for (int i = 0; i < 4 && s.owner({1, 0}) != 2; ++i) s.endDay();
    CHECK_EQ(s.owner({1, 0}), 2);
    CHECK(!s.lastRivalMoves().empty() || s.rivals()[0].pos == HexCoord(1, 0));
    bool reported = false;
    for (const auto& l : s.scenario().drainLines()) reported = reported || l.text.find("taken the Mine") != std::string::npos;
    CHECK(reported);
}

SUITE("Rivals — auto-battle picks the stronger side") {
    AdventureSession s;
    CHECK(!s.start(rivalMap(), "data"));
    auto big = s.autoBattle({{"rider_knight", 20}}, {{"skeleton_warrior", 5}});
    CHECK(big.attackerWon);
    CHECK(!big.attacker.empty() && big.defender.empty());
    auto small = s.autoBattle({{"levy_spearman", 2}}, {{"ancient_guardian", 3}});
    CHECK(!small.attackerWon);
}

SUITE("Rivals — a strong war-band ambushes a weak hero") {
    AdventureSession s;
    CHECK(!s.start(rivalMap(), "data"));
    s.setArmy({{"levy_spearman", 1}});
    s.travel({2, 0});
    for (int i = 0; i < 3 && !s.pendingIsAmbush(); ++i) s.endDay();
    CHECK(s.pendingIsAmbush());
    CHECK(s.pendingEncounter().has_value());
    s.resolveEncounter(true);                        // player wins the defence
    bool anyAlive = false;
    for (const auto& r : s.rivals()) anyAlive = anyAlive || r.alive;
    CHECK(!anyAlive);
}

SUITE("Rivals — clearing the passage mine first loses the game") {
    WorldMap map = rivalMap();
    map.placeObject({{4, -2}, ObjType::OldMine, "Near Mine", 0});
    AdventureSession s;
    CHECK(!s.start(std::move(map), "data"));      // no encounters file: the old mine has no guards
    s.setArmy({{"rider_knight", 50}});               // too strong to ambush
    s.travel({-5, 0});
    for (int i = 0; i < 8 && !s.lost(); ++i) s.endDay();
    CHECK(s.lost());
    CHECK(!s.lostReason().empty());
}

// ── Garrisons ─────────────────────────────────────────────────────────────────

SUITE("Garrisons — troops move between hero and an owned mine") {
    AdventureSession s;
    CHECK(!s.start(rivalMap(), "data"));
    s.setArmy({{"levy_spearman", 20}, {"desert_archer", 5}});
    CHECK(s.transfer({1, 0}, "levy_spearman", 5, true).has_value());   // hero not there
    s.travel({1, 0});
    CHECK(!s.transfer({1, 0}, "levy_spearman", 15, true));
    CHECK(s.garrison({1, 0}) != nullptr);
    CHECK_EQ(s.army()[0].count, 5);
    CHECK(!s.transfer({1, 0}, "levy_spearman", 5, false));
    CHECK_EQ(s.army()[0].count, 10);
    CHECK(s.transfer({1, 0}, "desert_archer", 5, true).has_value() == false);
    CHECK(s.transfer({1, 0}, "levy_spearman", 10, true).has_value());  // cannot strip the hero bare
}

SUITE("Garrisons — a strong garrison holds a mine against the war-band") {
    AdventureSession s;
    CHECK(!s.start(rivalMap(), "data"));
    s.setArmy({{"rider_knight", 60}, {"levy_spearman", 1}});
    s.travel({1, 0});
    CHECK(!s.transfer({1, 0}, "rider_knight", 60, true));
    s.travel({-5, 0});
    for (int i = 0; i < 6; ++i) s.endDay();
    CHECK_EQ(s.owner({1, 0}), 1);                                      // never taken
}

SUITE("Garrisons — a weak garrison falls and the mine is lost") {
    AdventureSession s;
    CHECK(!s.start(rivalMap(), "data"));
    s.setArmy({{"rider_knight", 60}, {"levy_spearman", 2}});
    s.travel({1, 0});
    CHECK(!s.transfer({1, 0}, "levy_spearman", 2, true));
    s.travel({-5, 0});
    for (int i = 0; i < 6 && s.owner({1, 0}) != 2; ++i) s.endDay();
    CHECK_EQ(s.owner({1, 0}), 2);
    CHECK(s.garrison({1, 0}) == nullptr);
}

// ── Special characters ────────────────────────────────────────────────────────

SUITE("Specials — Ushari joins at the start with Drillmaster") {
    auto s = started();
    CHECK_EQ((int)s.specials().size(), 1);
    CHECK(s.specials()[0].id == "ushari" && s.specials()[0].level == 1);
    CHECK(s.hasAbility("Drillmaster"));
    CHECK(!s.hasAbility("Eagle Eye"));
    CHECK_EQ(s.upkeepPerDay(), 100);
}

SUITE("Specials — a victory gives the travelling SC experience") {
    AdventureSession s;
    CHECK(!s.start(guardedMap(), "data", writeEncounters()));
    s.setArmy({{"rider_knight", 40}});
    s.travel({-2, 0});
    CHECK(s.pendingEncounter().has_value());
    s.resolveEncounter(true);
    CHECK(s.specials()[0].xp >= 20);
    CHECK_EQ(AdventureSession::xpForLevel(3), 250);
    CHECK_EQ(AdventureSession::upkeepFor(5), 300);
}

SUITE("Specials — unpaid upkeep: sulk after 3 days, leave after 7") {
    auto s = started();
    ResourcePool drain;
    drain[Resource::Gold] = -s.treasury()[Resource::Gold];
    s.give(drain);                                             // empty treasury
    for (int i = 0; i < 3; ++i) { ResourcePool d; d[Resource::Gold] = -s.treasury()[Resource::Gold]; s.give(d); s.endDay(); }
    CHECK(!s.specials().empty() && s.specials()[0].unpaidDays == 3);
    CHECK(!s.hasAbility("Drillmaster"));                       // sulking
    for (int i = 0; i < 4; ++i) { ResourcePool d; d[Resource::Gold] = -s.treasury()[Resource::Gold]; s.give(d); s.endDay(); }
    CHECK(s.specials().empty());                               // gone
}

SUITE("Specials — a governor earns gold in town but no XP") {
    auto s = started();
    int before = s.dailyIncome()[Resource::Gold];
    CHECK(!s.station("ushari", true));
    CHECK_EQ(s.dailyIncome()[Resource::Gold], before + 100);
    CHECK(!s.hasAbility("Drillmaster"));                       // not travelling
    s.travel({-5, 0});
    CHECK(s.station("ushari", false).has_value());             // must return to recall
    s.travel({-6, 0});
    CHECK(!s.station("ushari", false));
    CHECK(s.hasAbility("Drillmaster"));
}

// ── Save / load ───────────────────────────────────────────────────────────────

SUITE("Save — the demo map round-trips mid-game") {
    AdventureSession a;
    CHECK(!a.start("data/maps/old_passage.json", "data", "data/maps/old_passage.encounters.json", 5,
                   "data/maps/old_passage.triggers.json"));
    a.setArmy({{"rider_knight", 40}, {"levy_spearman", 10}});
    a.travel({-6, 3});                               // to the Ridge Pass guards
    if (a.pendingEncounter()) a.resolveEncounter(true);
    a.endDay();
    a.endDay();
    a.endDay();
    auto save = a.saveState();
    CHECK(!save.is_null());

    AdventureSession b;
    CHECK(!b.loadState(Scenario::Json::parse(save.dump())));
    CHECK_EQ(b.day(), a.day());
    CHECK(b.heroPos() == a.heroPos());
    CHECK_EQ(b.treasury()[Resource::Gold], a.treasury()[Resource::Gold]);
    CHECK_EQ(b.isEncounter({-6, 3}), a.isEncounter({-6, 3}));
    CHECK_EQ((int)b.explored().size(), (int)a.explored().size());
    CHECK(b.passageMine() == a.passageMine());
    CHECK_EQ((int)b.army().size(), (int)a.army().size());
    CHECK_EQ(b.specials()[0].xp, a.specials()[0].xp);
    CHECK_EQ((int)b.scenario().quests().size(), (int)a.scenario().quests().size());
    CHECK_EQ((int)b.rivals().size(), (int)a.rivals().size());
    if (!a.rivals().empty()) CHECK(b.rivals()[0].pos == a.rivals()[0].pos);
    CHECK(b.moves() == a.moves());
}

SUITE("Save — a map-object session cannot be saved") {
    auto s = started();
    CHECK(s.saveState().is_null());
}

SUITE("Scenario — 'after' gates a trigger until another has fired") {
    const std::string path = "/tmp/raids_test_after.json";
    { std::ofstream f(path); f << R"({"triggers":[
        {"id":"late","when":{"event":"day","day":2,"after":"early"},"do":[{"say":[["A","late"]]}]},
        {"id":"early","when":{"event":"day","day":3},"do":[{"say":[["A","early"]]}]}]})"; }
    AdventureSession s;
    CHECK(!s.start(testMap(), "data", "", 0, path));
    s.endDay();                                          // day 2: "late" is gated
    CHECK(s.scenario().drainLines().empty());
    s.endDay();                                          // day 3: early fires, then late may
    auto lines = s.scenario().drainLines();
    CHECK_EQ((int)lines.size(), 1);
    s.endDay();
    lines = s.scenario().drainLines();
    CHECK_EQ((int)lines.size(), 1);
    if (!lines.empty()) CHECK(lines[0].text == "late");
}
