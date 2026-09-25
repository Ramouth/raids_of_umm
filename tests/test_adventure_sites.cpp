// test_adventure_sites.cpp — HoMM3-style adventure sites on AdventureSession:
// pickups and chests, mills, stables, watchtowers, learning stones, obelisks,
// creature dwellings, guard zones of control, and the scenario's betrayal.
#include "test_runner.h"
#include "adventure/AdventureSession.h"
#include <fstream>
#include <iostream>

namespace {

MapObjectDef site(HexCoord at, ObjType type, const char* name, const char* kind = "", int faction = 0) {
    MapObjectDef o{at, type, name, faction};
    o.kind = kind;
    return o;
}

// Radius-8 grass map, player town at (-6,0); sites laid out along r = 0 and r = 2.
WorldMap sitesMap() {
    WorldMap map;
    map.clear(8);
    for (const auto& [c, t] : map) map.setTile(c, makeTile(Terrain::Grass));
    map.placeObject(site({-6, 0}, ObjType::Town,          "Home", "", 1));
    map.placeObject(site({-5, 0}, ObjType::Pickup,        "Log Pile", "wood"));
    map.placeObject(site({-4, 0}, ObjType::Pickup,        "Strongbox", "chest"));
    map.placeObject(site({-3, 0}, ObjType::Mill,          "Windmill", "windmill"));
    map.placeObject(site({-2, 0}, ObjType::Stables,       "Stables"));
    map.placeObject(site({-1, 0}, ObjType::LearningStone, "Stone"));
    map.placeObject(site({ 0, 0}, ObjType::Watchtower,    "Tower"));
    map.placeObject(site({-6, 2}, ObjType::Dwelling,      "Wolf Den", "grey_wolf"));
    map.placeObject(site({-4, 2}, ObjType::Obelisk,       "Obelisk"));
    map.placeObject(site({ 4, -4}, ObjType::OldMine,      "Mine A"));
    map.placeObject(site({ 4, 2}, ObjType::OldMine,       "Mine B"));
    map.placeObject(site({ 6, 0}, ObjType::Town,          "Keep", "", 1));
    return map;
}

AdventureSession started(WorldMap map = sitesMap(), const std::string& triggers = "") {
    AdventureSession s;
    CHECK(!s.start(std::move(map), "data", "", 0, triggers));
    return s;
}

} // namespace

SUITE("Sites — a resource pile is collected once, on the way past") {
    auto s = started();
    int wood = s.treasury()[Resource::Wood];
    auto steps = s.travel({-5, 0});
    CHECK_EQ((int)steps.size(), 1);
    CHECK(steps[0].found.find("+6 Wood") != std::string::npos);
    CHECK_EQ(s.treasury()[Resource::Wood], wood + 6);
    CHECK(s.pickupAt({-5, 0}) == nullptr);
    CHECK(s.siteUsed({-5, 0}));
}

SUITE("Sites — a chest holds gold, not experience (unlike HoMM3)") {
    auto s = started();
    int gold = s.treasury()[Resource::Gold];
    int xp = s.heroProgress().xp;
    s.travel({-3, 0});                                   // route passes the chest
    CHECK(!s.pendingChest().has_value());                // no choice to make
    CHECK_EQ(s.treasury()[Resource::Gold], gold + 1000);
    CHECK_EQ(s.heroProgress().xp, xp);
}

SUITE("Sites — a war journal (tome) teaches once") {
    WorldMap map = sitesMap();
    map.placeObject(site({-5, 1}, ObjType::Pickup, "Field Book", "tome"));
    auto s = started(std::move(map));
    int xp = s.heroProgress().xp;
    auto steps = s.travel({-5, 1});
    CHECK(!steps.empty() && steps.back().found.find("experience") != std::string::npos);
    CHECK_EQ(s.heroProgress().xp, xp + AdventureSession::TOME_XP);
    CHECK(s.pickupAt({-5, 1}) == nullptr);
}

SUITE("Sites — a mill pays once per week") {
    auto s = started();
    s.travel({-4, 0});
    s.claimChest(true);
    int stone = s.treasury()[Resource::Stone];
    s.travel({-3, 0});
    CHECK_EQ(s.treasury()[Resource::Stone], stone + 2);
    CHECK(s.siteUsed({-3, 0}));
    s.travel({-4, 0});
    s.travel({-3, 0});                                   // same week: nothing
    CHECK_EQ(s.treasury()[Resource::Stone], stone + 2);
    for (int i = 0; i < 7; ++i) s.endDay();              // next week
    CHECK(!s.siteUsed({-3, 0}));
}

SUITE("Sites — stables add movement until the week ends") {
    auto s = started();
    float base = s.movesMax();
    s.travel({-4, 0});
    s.claimChest(true);
    s.travel({-2, 0});
    CHECK(s.movesMax() == base + AdventureSession::STABLES_BONUS);
    s.endDay();
    CHECK(s.movesMax() == base + AdventureSession::STABLES_BONUS);
    while (s.dayOfWeek() != 1) s.endDay();
    CHECK(s.movesMax() == base);
}

SUITE("Sites — the watchtower reveals far and the learning stone teaches once") {
    auto s = started();
    CHECK(!s.isExplored({7, -3}));
    s.travel({-4, 0});
    s.claimChest(true);
    s.travel({0, 0});
    CHECK(s.isExplored({7, -3}));                        // 7 hexes from the tower
    int xp = s.specials()[0].xp;
    s.travel({-1, 0});
    CHECK_EQ(s.specials()[0].xp, xp);                    // already read on the way
    CHECK(s.siteUsed({-1, 0}));
}

SUITE("Sites — an obelisk rules out a false old mine") {
    auto s = started();
    CHECK(s.ruledOut().empty());
    auto steps = s.travel({-4, 2});
    if (s.pendingChest()) { s.claimChest(true); steps = s.travel({-4, 2}); }
    CHECK(s.heroPos() == HexCoord(-4, 2));
    CHECK(!steps.empty());
    CHECK_EQ((int)s.ruledOut().size(), 1);
    CHECK(!s.ruledOut().count(*s.passageMine()));
    CHECK(steps.back().found.find("leads nowhere") != std::string::npos);
}

SUITE("Sites — a dwelling is captured and recruits its creature weekly") {
    auto s = started();
    CHECK(s.town({-6, 2}) != nullptr);
    CHECK(s.recruit({-6, 2}, "grey_wolf", 1).has_value());   // not ours yet
    s.travel({-6, 2});
    CHECK_EQ(s.owner({-6, 2}), (int)Faction::Player);
    CHECK_EQ(s.minesHeld(), 0);                              // dwellings are not mines
    int pool = s.town({-6, 2})->recruitPool.at("grey_wolf");
    CHECK(pool > 0);
    CHECK(!s.recruit({-6, 2}, "grey_wolf", 2));
    CHECK(s.recruit({-6, 2}, "levy_spearman", 1).has_value());
}

SUITE("Sites — a guard's zone of control stops a hero walking past") {
    WorldMap map = sitesMap();
    map.placeObject(site({-4, -1}, ObjType::Guard, "Wolves"));
    auto s = started(std::move(map));
    // The only cells west→east along r=0 at q=-5..-3 touch the guard; the route skirts it if it can.
    auto path = s.route({-2, 0});
    for (const auto& c : path) CHECK(!s.guardZoneAt(c) || c == HexCoord(-2, 0));
    // Target a cell inside the zone: the guard attacks, the hero holds its ground after winning.
    s.travel({-5, 0});
    CHECK(s.pendingEncounter() && *s.pendingEncounter() == HexCoord(-4, -1));
    s.resolveEncounter(true);
    CHECK(s.heroPos() == HexCoord(-5, 0));
    CHECK(!s.isEncounter({-4, -1}));
}

SUITE("Sites — save/load keeps collected pickups and used sites") {
    AdventureSession s;
    const std::string path = "/tmp/raids_test_sites_map.json";
    CHECK(!sitesMap().saveJson(path));
    CHECK(!s.start(path, "data"));
    s.travel({-4, 0});
    s.claimChest(true);
    s.travel({-3, 0});
    AdventureSession b;
    CHECK(!b.loadState(Scenario::Json::parse(s.saveState().dump())));
    CHECK(b.pickupAt({-5, 0}) == nullptr);
    CHECK(b.pickupAt({-4, 0}) == nullptr);
    CHECK(b.siteUsed({-3, 0}));
    CHECK(!b.siteUsed({-2, 0}));
}

SUITE("Scenario — allies send troops and gifts, then a cousin betrays") {
    const std::string path = "/tmp/raids_test_betray.json";
    { std::ofstream f(path); f << R"({"triggers":[
        {"id":"gift","when":{"event":"start"},"do":[{"troops":[{"id":"levy_spearman","count":12}]},
                                                    {"item":"hale_signet"},{"lore":["The Families","Old houses."]}]},
        {"id":"turn","when":{"event":"day","day":3},"do":[{"betray":{"at":"Keep","band":"Hale household",
                                                    "army":[{"id":"levy_spearman","count":20}]}}]},
        {"id":"end","when":{"event":"rival_beaten","name":"Hale household"},"do":[{"say":[["Corvin","..."]]}]}]})"; }
    auto s = started(sitesMap(), path);
    s.setArmy({});
    auto s2 = started(sitesMap(), path);
    int levies = 0;
    for (const auto& st : s2.army()) if (st.id == "levy_spearman") levies = st.count;
    CHECK(levies >= 12);
    CHECK(s2.hasItem("hale_signet"));
    CHECK_EQ((int)s2.scenario().lore().size(), 1);
    CHECK_EQ(s2.owner({6, 0}), (int)Faction::Player);
    s2.endDay();
    s2.endDay();
    CHECK_EQ(s2.owner({6, 0}), (int)Faction::AI);
    bool band = false;
    for (const auto& r : s2.rivals()) if (r.name == "Hale household" && r.alive) band = true;
    CHECK(band);
}

SUITE("Demo map — the northern stage loads with all its sites") {
    AdventureSession s;
    CHECK(!s.start("data/maps/old_passage.json", "data", "data/maps/old_passage.encounters.json", 3,
                   "data/maps/old_passage.triggers.json"));
    int sites = 0, obelisks = 0, dwellings = 0;
    for (const auto& o : s.map().objects()) {
        sites     += o.type >= ObjType::Pickup ? 1 : 0;
        obelisks  += o.type == ObjType::Obelisk ? 1 : 0;
        dwellings += o.type == ObjType::Dwelling ? 1 : 0;
    }
    CHECK(sites >= 30);
    CHECK_EQ(obelisks, 4);
    CHECK_EQ(dwellings, 2);
    CHECK(s.passageMine().has_value());
    CHECK(!s.scenario().quests().empty());
    CHECK(s.hasItem("hale_signet") == false);        // the signet comes at Hallowmere
    CHECK(s.army().empty() || s.army()[0].count >= 12);  // Corvin's twelve spears
}

SUITE("Demo map — day one: the hero can march on the Bridge Wardens") {
    AdventureSession s;
    CHECK(!s.start("data/maps/old_passage.json", "data", "data/maps/old_passage.encounters.json", 1,
                   "data/maps/old_passage.triggers.json"));
    CHECK(s.heroPos() == HexCoord(-11, 6));                 // Varenhold
    auto path = s.route({-4, 2});
    CHECK(!path.empty());
    CHECK_EQ(s.affordableSteps(path), (int)path.size());   // reachable on day one
    s.travel({-4, 2});
    CHECK(s.pendingEncounter().has_value());
    if (s.pendingEncounter()) CHECK(*s.pendingEncounter() == HexCoord(-4, 2));
    std::cout << "    hero at (" << s.heroPos().q << "," << s.heroPos().r << "), route " << path.size()
              << ", cost " << s.routeCost(path) << ", moves left " << s.moves() << "\n";
}

SUITE("Hero XP — the preview matches what a victory pays; levels survive a save") {
    WorldMap map = sitesMap();
    map.placeObject(site({-4, -1}, ObjType::Guard, "Wolves"));
    auto s = started(std::move(map));
    CHECK_EQ(s.heroProgress().level, 1);
    int preview = s.encounterXp({-4, -1});
    CHECK(preview >= 20);
    CHECK_EQ(s.encounterXp({-5, 0}), 0);                 // a pile is not a fight
    s.travel({-5, 0});
    CHECK(s.pendingEncounter().has_value());
    int before = s.heroProgress().xp;
    s.resolveEncounter(true);
    CHECK_EQ(s.heroProgress().xp, before + preview);
    s.grantXp(500);                                        // e.g. a quest reward
    CHECK(s.heroProgress().level >= 3);                  // 250 xp reaches level 3
    AdventureSession b;
    const std::string path = "/tmp/raids_test_hero_xp_map.json";
    WorldMap again = sitesMap();
    CHECK(!again.saveJson(path));
    CHECK(!b.start(path, "data"));
    b.grantXp(300);
    AdventureSession c;
    CHECK(!c.loadState(Scenario::Json::parse(b.saveState().dump())));
    CHECK_EQ(c.heroProgress().xp, b.heroProgress().xp);
    CHECK_EQ(c.heroProgress().level, b.heroProgress().level);
}

SUITE("Companions — a fallen companion is wounded for three days, lost if the battle was") {
    auto s = started();
    CHECK_EQ((int)s.battleCompanions().size(), 1);
    CHECK(s.battleCompanions()[0].id == "ushari");
    CHECK(s.hasAbility("Drillmaster"));
    s.companionsFell({"ushari"}, false);
    CHECK_EQ((int)s.specials().size(), 1);
    CHECK(s.isWounded(s.specials()[0]));
    CHECK(s.battleCompanions().empty());                 // sits out the next fights
    CHECK(!s.hasAbility("Drillmaster"));                 // and her map ability rests
    AdventureSession b;
    const std::string path = "/tmp/raids_test_wounds_map.json";
    WorldMap again = sitesMap();
    CHECK(!again.saveJson(path));
    CHECK(!b.start(path, "data"));
    b.companionsFell({"ushari"}, false);
    AdventureSession c;
    CHECK(!c.loadState(Scenario::Json::parse(b.saveState().dump())));
    CHECK(c.isWounded(c.specials()[0]));                 // wounds survive a save
    for (int d = 0; d < AdventureSession::WOUND_DAYS; ++d) s.endDay();
    CHECK(!s.isWounded(s.specials()[0]));
    CHECK_EQ((int)s.battleCompanions().size(), 1);
    s.companionsFell({"ushari"}, true);
    CHECK(s.specials().empty());
}

SUITE("Companions — auto-resolved battles field them and report who fell") {
    auto s = started();
    auto lost = s.autoBattle({{"levy_spearman", 1}}, {{"forest_troll", 40}}, s.battleCompanions());
    CHECK(!lost.attackerWon);
    CHECK_EQ((int)lost.fallen.size(), 1);
    CHECK(lost.attacker.empty());                        // companions never appear as troops
    auto won = s.autoBattle({{"armoured_warrior", 60}}, {{"grey_wolf", 2}}, s.battleCompanions());
    CHECK(won.attackerWon);
    for (const auto& st : won.attacker) CHECK(st.id != "ushari");
}
