// test_hero_equipment.cpp — the commander's paper doll: slots, trinket pair,
// swapping, cursed items, the army-wide bonus, and save/load.
#include "test_runner.h"
#include "adventure/AdventureSession.h"

namespace {
AdventureSession gearSession() {
    WorldMap map;
    map.clear(6);
    map.placeObject({{-4, 0}, ObjType::Town, "Home", 1});
    AdventureSession s;
    CHECK(!s.start(std::move(map), "data"));
    return s;
}
}

SUITE("Equipment — an item goes to its slot and helps the whole army") {
    auto s = gearSession();
    CHECK(s.equip("greyfang_helm").has_value());                 // not carried yet
    s.addItem("greyfang_helm");
    CHECK_EQ((int)s.backpack().size(), 1);
    CHECK(!s.equip("greyfang_helm"));
    CHECK(s.equipped().at("helm") == "greyfang_helm");
    CHECK(s.backpack().empty());
    CHECK_EQ(s.armyBonus().defense, 2);
    CHECK(!s.unequip("helm"));
    CHECK_EQ(s.armyBonus().defense, 0);
    CHECK_EQ((int)s.backpack().size(), 1);
}

SUITE("Equipment — two trinket slots; a third trinket swaps out the first") {
    auto s = gearSession();
    for (const char* id : {"hale_signet", "listening_shard", "cursed_eye_of_set"}) s.addItem(id);
    CHECK(!s.equip("hale_signet"));
    CHECK(!s.equip("listening_shard"));
    CHECK(s.equipped().at("trinket1") == "hale_signet" && s.equipped().at("trinket2") == "listening_shard");
    CHECK_EQ(s.armyBonus().speed, 1);
    CHECK(!s.equip("cursed_eye_of_set"));                         // both full: replaces trinket1
    CHECK(s.equipped().at("trinket1") == "cursed_eye_of_set");
    CHECK_EQ(s.armyBonus().attack, 4);
    CHECK_EQ(s.armyBonus().defense, 1 - 2);
}

SUITE("Equipment — a cursed item will not come off") {
    auto s = gearSession();
    s.addItem("drowned_crown");
    s.addItem("hale_signet");
    CHECK(!s.equip("drowned_crown"));
    auto err = s.unequip("trinket1");
    CHECK(err.has_value() && err->find("cursed") != std::string::npos);
    CHECK(!s.equip("hale_signet"));                               // the free trinket slot is fine
    s.addItem("warm_stone");
    auto blocked = s.equip("warm_stone");                          // both trinket slots taken; the crown is first
    CHECK(blocked.has_value() && blocked->find("cursed") != std::string::npos);
    CHECK(s.equipped().at("trinket1") == "drowned_crown");        // the crown stays on
}

SUITE("Equipment — worn items and buildings add up; saved and loaded") {
    AdventureSession a;
    WorldMap map;
    map.clear(6);
    map.placeObject({{-4, 0}, ObjType::Town, "Home", 1});
    const std::string path = "/tmp/raids_test_gear_map.json";
    CHECK(!map.saveJson(path));
    CHECK(!a.start(path, "data"));
    a.addItem("barrow_blade");
    a.addItem("bearhide_cloak");
    CHECK(!a.equip("barrow_blade"));
    CHECK(!a.equip("bearhide_cloak"));
    CHECK_EQ(a.armyBonus().attack, 2);
    CHECK_EQ(a.armyBonus().defense, 2);
    AdventureSession b;
    CHECK(!b.loadState(Scenario::Json::parse(a.saveState().dump())));
    CHECK(b.equipped().at("weapon") == "barrow_blade");
    CHECK_EQ(b.armyBonus().defense, 2);
}

SUITE("Equipment — auto-battles fight with the army bonus") {
    auto s = gearSession();
    // Same fight, same engine: +10 attack for every stack must not do worse.
    int plain = 0, boosted = 0;
    for (int i = 0; i < 6; ++i) {
        plain   += s.autoBattle({{"levy_spearman", 30}}, {{"grey_wolf", 14}}).attackerWon;
        boosted += s.autoBattle({{"levy_spearman", 30}}, {{"grey_wolf", 14}}, {}, ArmyBonus{10, 10, 0}).attackerWon;
    }
    CHECK(boosted >= plain);
    CHECK_EQ(boosted, 6);
}
