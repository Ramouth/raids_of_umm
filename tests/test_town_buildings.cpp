// test_town_buildings.cpp — HoMM3-style town buildings on AdventureSession:
// starting set, one build per day, prerequisites, dwellings gating recruits,
// hall income, fort growth, marketplace trading, the Drill Yard, save/load.
#include "test_runner.h"
#include "adventure/AdventureSession.h"

namespace {

// Player town at (-6,0), neutral town at (6,0).
AdventureSession townSession() {
    WorldMap map;
    map.clear(8);
    map.placeObject({{-6, 0}, ObjType::Town, "Home", 1});
    map.placeObject({{ 6, 0}, ObjType::Town, "Far Town", 0});
    AdventureSession s;
    CHECK(!s.start(std::move(map), "data"));
    return s;
}

void rich(AdventureSession& s) {
    ResourcePool plenty;
    for (int i = 0; i < RESOURCE_COUNT; ++i) plenty[static_cast<Resource>(i)] = i == 0 ? 100000 : 100;
    s.give(plenty);
}

const HexCoord HOME{-6, 0};

} // namespace

SUITE("Town buildings — a town starts with its hall, fort and first two dwellings") {
    auto s = townSession();
    const TownState* t = s.town(HOME);
    CHECK(t != nullptr);
    for (const char* id : {"village_hall", "fort", "muster_field", "archery_butts"}) CHECK(t->buildings.count(id));
    CHECK(!t->buildings.count("armoury"));
    CHECK(s.canRecruitHere(HOME, "levy_spearman"));
    CHECK(!s.canRecruitHere(HOME, "armoured_warrior"));
    CHECK(t->recruitPool.count("levy_spearman") && t->recruitPool.at("levy_spearman") > 0);
    CHECK(!t->recruitPool.count("armoured_warrior") || t->recruitPool.at("armoured_warrior") == 0);
}

SUITE("Town buildings — a missing dwelling blocks recruiting and says which") {
    auto s = townSession();
    rich(s);
    auto err = s.recruit(HOME, "armoured_warrior", 1);
    CHECK(err.has_value() && err->find("Armoury") != std::string::npos);
}

SUITE("Town buildings — one per day; prerequisites; cost is paid") {
    auto s = townSession();
    rich(s);
    CHECK(s.buildBlocker(HOME, "knights_hall").find("Needs") == 0);
    const int gold = s.treasury()[Resource::Gold];
    CHECK(!s.build(HOME, "armoury"));
    CHECK_EQ(s.treasury()[Resource::Gold], gold - 1000);
    CHECK(s.canRecruitHere(HOME, "armoured_warrior"));
    CHECK(s.town(HOME)->recruitPool.at("armoured_warrior") > 0);         // first week's recruits at once
    CHECK(!s.recruit(HOME, "armoured_warrior", 1));
    auto again = s.build(HOME, "marketplace");
    CHECK(again.has_value() && again->find("today") != std::string::npos);
    s.endDay();
    CHECK(!s.build(HOME, "marketplace"));
    CHECK(s.build(HOME, "marketplace").has_value());                     // already built
}

SUITE("Town buildings — the neutral town is not yours to build in") {
    auto s = townSession();
    rich(s);
    CHECK(s.build({6, 0}, "armoury").has_value());
}

SUITE("Town buildings — halls raise income, and the extra gold is really paid") {
    auto s = townSession();
    rich(s);
    const int before = s.dailyIncome()[Resource::Gold];
    CHECK(!s.build(HOME, "town_hall"));
    CHECK_EQ(s.dailyIncome()[Resource::Gold], before + 500);
    const int gold = s.treasury()[Resource::Gold];
    const int income = s.dailyIncome()[Resource::Gold];
    s.endDay();
    CHECK_EQ(s.treasury()[Resource::Gold], gold + income - s.upkeepPerDay());
}

SUITE("Town buildings — a citadel makes recruits grow 50% faster") {
    auto s = townSession();
    rich(s);
    CHECK(!s.build(HOME, "citadel"));
    CHECK_NEAR(s.growthBonus(HOME), 0.5, 1e-9);
    const int base = s.resources().unit("levy_spearman")->weeklyGrowth;
    const int before = s.town(HOME)->recruitPool.at("levy_spearman");
    while (s.dayOfWeek() != 7) s.endDay();
    s.endDay();                                                         // the week turns
    const int grown = s.town(HOME)->recruitPool.at("levy_spearman") - before;
    CHECK(grown >= base * 3 / 2 - 1 && grown <= base * 3 / 2 + 1);
}

SUITE("Town buildings — the marketplace trades at half / one-and-a-half value") {
    auto s = townSession();
    rich(s);
    auto none = s.trade(Resource::Wood, Resource::Gold, 10);
    CHECK(!none.error.empty());                                          // no market yet
    CHECK(!s.build(HOME, "marketplace"));
    CHECK(s.hasMarket());
    const int gold = s.treasury()[Resource::Gold];
    auto sold = s.trade(Resource::Wood, Resource::Gold, 10);
    CHECK(sold.error.empty());
    CHECK_EQ(sold.received, 750);                                        // 10 × 150 / 2
    CHECK_EQ(s.treasury()[Resource::Gold], gold + 750);
    CHECK_EQ(AdventureSession::tradeQuote(Resource::Gold, Resource::Obsidian, 1200), 2);   // 600 each
    CHECK_EQ(AdventureSession::tradeQuote(Resource::Wood, Resource::Obsidian, 8), 1);     // 600 / 600
}

SUITE("Town buildings — the Drill Yard gives the army +1 attack while the town is held") {
    auto s = townSession();
    rich(s);
    CHECK_EQ(s.armyAttackBonus(), 0);
    CHECK(!s.build(HOME, "armoury"));
    s.endDay();
    CHECK(!s.build(HOME, "drill_yard"));
    CHECK_EQ(s.armyAttackBonus(), 1);
}

SUITE("Town buildings — buildings and today's build survive a save") {
    AdventureSession a;
    WorldMap map;
    map.clear(8);
    map.placeObject({{-6, 0}, ObjType::Town, "Home", 1});
    const std::string path = "/tmp/raids_test_buildings_map.json";
    CHECK(!map.saveJson(path));
    CHECK(!a.start(path, "data"));
    rich(a);
    CHECK(!a.build(HOME, "armoury"));
    AdventureSession b;
    CHECK(!b.loadState(Scenario::Json::parse(a.saveState().dump())));
    CHECK(b.town(HOME)->buildings.count("armoury"));
    CHECK(b.buildBlocker(HOME, "marketplace").find("today") != std::string::npos);
}
