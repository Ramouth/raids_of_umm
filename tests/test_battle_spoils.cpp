// test_battle_spoils.cpp — what the result screens read: the combat engine's
// battle record (damage, kills, the biggest hit, losses), encounter spoils
// (resources and reward items), keepsakes, and the story's use of them
// (requires_item, take_item, a guard that stands aside instead of fighting).
#include "test_runner.h"
#include "adventure/AdventureSession.h"
#include "combat/CombatAI.h"
#include "combat/CombatEngine.h"
#include <algorithm>
#include <deque>
#include <filesystem>
#include <fstream>

namespace {

using Json = nlohmann::json;
namespace fs = std::filesystem;

std::deque<UnitType> s_types;   // CombatUnit keeps const UnitType*: they must outlive the engines

const UnitType* fighter(const std::string& id, int speed, int minDamage, int maxDamage, int hp = 10) {
    UnitType t;
    t.id = t.name = id;
    t.speed = speed;
    t.hitPoints = hp;
    t.attack = 5;
    t.defense = 3;
    t.minDamage = minDamage;
    t.maxDamage = maxDamage;
    s_types.push_back(std::move(t));
    return &s_types.back();
}

CombatArmy army(bool player, std::vector<std::pair<const UnitType*, int>> stacks) {
    CombatArmy a;
    a.isPlayer = player;
    a.ownerName = player ? "Player" : "Enemy";
    for (const auto& [type, count] : stacks) a.stacks.push_back(CombatUnit::make(type, count, player));
    return a;
}

int sideHp(const CombatArmy& a) {
    int hp = 0;
    for (const auto& u : a.stacks) hp += u.totalHp();
    return hp;
}

// The real data directory, plus a keepsake whose data carries a slot and an
// effect it must never get to use.
std::string keepsakeData() {
    const fs::path dir = fs::temp_directory_path() / "umm_keepsake_data";
    fs::create_directories(dir);
    for (const auto& entry : fs::directory_iterator("data"))
        if (entry.is_regular_file())
            fs::copy_file(entry.path(), dir / entry.path().filename(), fs::copy_options::overwrite_existing);
    Json items = Json::parse(std::ifstream("data/items.json"));
    items["items"].push_back({{"id", "test_dog_tag"}, {"name", "Dog Tag"}, {"kind", "keepsake"},
                              {"text", "Bran. A good boy."}, {"slot", "trinket"},
                              {"passiveEffects", {{{"stat", "attack"}, {"amount", 5}}}}});
    std::ofstream(dir / "items.json") << items.dump();
    return dir.string();
}

// Grass, a player town, two dog packs that each leave a tag, and a warrior on the road.
std::string spoilsMap() {
    WorldMap map;
    map.clear(6);
    for (const auto& [c, t] : map) map.setTile(c, makeTile(Terrain::Grass));
    map.placeObject({{-5, 0}, ObjType::Town, "Home", 1, ""});
    map.placeObject({{-2, 0}, ObjType::Guard, "Hill Dogs", 0, ""});
    map.placeObject({{-2, 3}, ObjType::Guard, "Ford Dogs", 0, ""});
    map.placeObject({{2, 0}, ObjType::Guard, "Northern Warrior", 0, ""});
    map.setStart(HexCoord{0, 0});
    const std::string path = (fs::temp_directory_path() / "umm_spoils_map.json").string();
    CHECK(!map.saveJson(path));
    return path;
}

std::string spoilsEncounters() {
    const std::string path = (fs::temp_directory_path() / "umm_spoils.encounters.json").string();
    std::ofstream(path) << R"({"objects": {
        "Hill Dogs": {"guards": [{"id": "grey_wolf", "count": 3}], "reward": {"Gold": 150, "items": ["test_dog_tag"]}},
        "Ford Dogs": {"guards": [{"id": "grey_wolf", "count": 3}], "reward": {"Gold": 150, "items": ["test_dog_tag"]}},
        "Northern Warrior": {"guards": [{"id": "grey_wolf", "count": 5}], "reward": {"Gold": 400}}}})";
    return path;
}

std::string warriorTriggers() {
    const std::string path = (fs::temp_directory_path() / "umm_spoils_warrior.triggers.json").string();
    std::ofstream(path) << R"({"start_companions": [], "start_army": [{"id": "levy_spearman", "count": 20}],
        "triggers": [
        {"id": "warrior", "when": {"event": "engage", "name": "Northern Warrior"}, "do": [{"choice": {"id": "warrior",
            "presentation": "conversation", "options": [
            {"id": "fight", "label": "Fight", "then": []},
            {"id": "give_tag", "label": "Hand him the dog tag", "requires_item": "test_dog_tag",
             "then": [{"take_item": "test_dog_tag"}, {"say": [["Warrior", "He was my favourite."]]},
                      {"withdraw": "Northern Warrior"}]}]}}]},
        {"id": "fought", "when": {"event": "encounter_won", "name": "Northern Warrior"}, "do": [{"flag": {"fought": true}}]},
        {"id": "way_open", "when": {"event": "cleared", "names": ["Northern Warrior"]}, "do": [{"flag": {"way_open": true}}]}]})";
    return path;
}

int optionCount(const Json& choice) { return static_cast<int>(choice.value("options", Json::array()).size()); }

} // namespace

SUITE("Battle stats — one exchange: damage is HP lost, retaliation counts, the max hit is the bigger blow") {
    const UnitType* raider = fighter("raider", 8, 4, 4);
    const UnitType* guard  = fighter("guard", 4, 2, 2);
    CombatEngine eng(army(true, {{raider, 10}}), army(false, {{guard, 6}}), 0, 7u);
    eng.teleportUnit(true, 0, {0, 0});
    eng.teleportUnit(false, 0, {1, 0});
    CHECK(eng.currentTurn().isPlayer);
    eng.doAttack(0);
    const auto& st = eng.stats();
    const int guardLost  = 60 - eng.enemyArmy().stacks[0].totalHp();
    const int raiderLost = 100 - eng.playerArmy().stacks[0].totalHp();
    CHECK(guardLost > 0 && raiderLost > 0);               // a strike and its retaliation
    CHECK_EQ(st.playerDamage, guardLost);
    CHECK_EQ(st.enemyDamage, raiderLost);
    CHECK_EQ(st.player[0].damage, guardLost);
    CHECK_EQ(st.enemy[0].damage, raiderLost);
    CHECK_EQ(st.player[0].kills, 6 - eng.enemyArmy().stacks[0].count);
    CHECK_EQ(st.enemy[0].kills, 10 - eng.playerArmy().stacks[0].count);
    CHECK_EQ(st.maxHit.amount, std::max(guardLost, raiderLost));
    CHECK_EQ(st.maxHit.isPlayer, guardLost >= raiderLost);
    CHECK_EQ(st.maxHit.stackIndex, 0);
    auto lost = eng.losses(false);
    CHECK(lost.size() == 1 && lost[0].unitId == "guard" && lost[0].count == 6 - eng.enemyArmy().stacks[0].count);
}

SUITE("Battle stats — a whole seeded battle: totals match HP lost and the max hit is the true max") {
    const UnitType* a = fighter("pike", 6, 1, 6);
    const UnitType* b = fighter("axe", 5, 2, 5, 12);
    const UnitType* c = fighter("hound", 7, 1, 4, 8);
    const UnitType* d = fighter("brute", 3, 3, 8, 20);
    CombatEngine eng(army(true, {{a, 12}, {b, 8}}), army(false, {{c, 14}, {d, 5}}), 6, 99u);
    const int playerStart = sideHp(eng.playerArmy()), enemyStart = sideHp(eng.enemyArmy());
    // Watched from outside: without shooters, guardians or companions every
    // stack takes at most one blow per action, so its HP drop is that blow.
    int trueMax = 0;
    bool trueMaxPlayer = false;
    for (int turn = 0; turn < 400 && !eng.isOver(); ++turn) {
        std::vector<int> before;
        for (const auto* side : {&eng.playerArmy(), &eng.enemyArmy()})
            for (const auto& u : side->stacks) before.push_back(u.totalHp());
        CombatAI::takeTurn(eng);
        eng.drainEvents();
        int i = 0;
        for (const auto* side : {&eng.playerArmy(), &eng.enemyArmy()})
            for (const auto& u : side->stacks) {
                const int drop = before[i++] - u.totalHp();
                if (drop > trueMax) { trueMax = drop; trueMaxPlayer = !side->isPlayer; }
            }
    }
    CHECK(eng.isOver());
    const auto& st = eng.stats();
    CHECK_EQ(st.playerDamage, enemyStart - sideHp(eng.enemyArmy()));
    CHECK_EQ(st.enemyDamage, playerStart - sideHp(eng.playerArmy()));
    CHECK_EQ(st.player[0].damage + st.player[1].damage, st.playerDamage);
    CHECK_EQ(st.enemy[0].damage + st.enemy[1].damage, st.enemyDamage);
    CHECK_EQ(st.maxHit.amount, trueMax);
    CHECK_EQ(st.maxHit.isPlayer, trueMaxPlayer);
    const auto& rows = st.maxHit.isPlayer ? st.player : st.enemy;
    CHECK(st.maxHit.stackIndex >= 0 && rows[st.maxHit.stackIndex].biggestHit == trueMax);
    int enemyKilled = 0;
    for (const auto& l : eng.losses(false)) enemyKilled += l.count;
    CHECK_EQ(st.player[0].kills + st.player[1].kills, enemyKilled);
}

SUITE("Battle stats — a retreat still reports what each side lost") {
    const UnitType* raider = fighter("raider2", 8, 4, 4);
    const UnitType* guard  = fighter("guard2", 4, 9, 9);
    CombatEngine eng(army(true, {{raider, 10}}), army(false, {{guard, 6}}), 0, 3u);
    eng.teleportUnit(true, 0, {0, 0});
    eng.teleportUnit(false, 0, {1, 0});
    eng.doAttack(0);
    eng.doRetreat();
    CHECK(eng.result() == CombatResult::Retreated);
    const auto mine = eng.losses(true);
    CHECK(!mine.empty() && mine[0].unitId == "raider2" && mine[0].count == 10 - eng.playerArmy().stacks[0].count);
    CHECK(!eng.losses(false).empty());
}

SUITE("Battle stats — a mad stack's blow on its own side (and the retaliation) credits no one") {
    // The striker is found by address on the other side's list; before, a pointer
    // subtracted across the two armies' vectors could land on an enemy row (UB).
    const UnitType* runner = fighter("mad_runner", 9, 3, 3);
    const UnitType* friendType = fighter("mad_friend", 2, 2, 2);
    const UnitType* foe = fighter("far_foe", 1, 1, 1);
    int checked = 0;
    for (uint32_t seed = 1; seed <= 300 && checked < 3; ++seed) {
        auto mine = army(true, {{runner, 5}, {friendType, 5}});
        mine.stacks[0].bitten = true;
        CombatEngine eng(std::move(mine), army(false, {{foe, 5}}), 0, seed);
        if (!eng.activeUnit().mad) continue;
        eng.teleportUnit(true, 0, CombatMap::toHex(1, 2));
        eng.teleportUnit(true, 1, CombatMap::toHex(1, 2).neighbor(0));
        eng.teleportUnit(false, 0, CombatMap::toHex(9, 2));
        CHECK(eng.doMadTurn());
        CHECK(eng.playerArmy().stacks[1].totalHp() < 50);            // the friend was struck
        const auto& st = eng.stats();
        CHECK_EQ(st.playerDamage, 0);
        CHECK_EQ(st.enemyDamage, 0);
        for (const auto& row : st.player) CHECK(row.damage == 0 && row.kills == 0);
        for (const auto& row : st.enemy)  CHECK(row.damage == 0 && row.kills == 0);
        CHECK_EQ(st.maxHit.stackIndex, -1);
        ++checked;
    }
    CHECK(checked >= 3);
}

SUITE("Spoils — a won encounter pays its gold and its keepsake, once; a defeat pays nothing") {
    const std::string data = keepsakeData();
    AdventureSession s;
    CHECK(!s.start(spoilsMap(), data, spoilsEncounters(), 1, warriorTriggers()));
    const int gold = s.treasury()[Resource::Gold];
    s.travel({-2, 0});
    CHECK(s.pendingEncounter().has_value());
    s.resolveEncounter(true);
    CHECK_EQ(s.treasury()[Resource::Gold], gold + 150);
    CHECK_EQ(s.lastSpoils().resources[Resource::Gold], 150);
    CHECK(s.lastSpoils().items == std::vector<std::string>{"test_dog_tag"});
    CHECK(s.hasItem("test_dog_tag"));
    CHECK(s.keepsakes().size() == 1 && s.keepsakes()[0]->text == "Bran. A good boy.");

    s.travel({-2, 3});                                    // a second pack with the same tag
    CHECK(s.pendingEncounter().has_value());
    s.resolveEncounter(true);
    CHECK_EQ(s.lastSpoils().resources[Resource::Gold], 150);
    CHECK(s.lastSpoils().items.empty());                  // already carried: not found twice
    CHECK_EQ((int)s.keepsakes().size(), 1);


    AdventureSession lost;                                // the warrior pays 400 gold, but only if beaten
    CHECK(!lost.start(spoilsMap(), data, spoilsEncounters(), 1, warriorTriggers()));
    lost.travel({1, 0});
    CHECK(lost.pendingEncounter().has_value());
    CHECK(!lost.scenario().choose(lost, "fight"));
    const int before = lost.treasury()[Resource::Gold];
    lost.resolveEncounter(false);
    CHECK(lost.lastSpoils().empty());
    CHECK_EQ(lost.treasury()[Resource::Gold], before);
}

SUITE("Keepsakes — never worn, never a bonus, and kept through save/load") {
    const std::string data = keepsakeData();
    AdventureSession s;
    CHECK(!s.start(spoilsMap(), data, spoilsEncounters(), 1, warriorTriggers()));
    s.scenario().run(s, Json::parse(R"([{"item": "test_dog_tag"}])"));   // the story's "item" action
    CHECK(s.hasItem("test_dog_tag"));
    CHECK(s.backpack().empty());                          // not a commander item
    CHECK(s.equip("test_dog_tag").has_value());
    CHECK(s.equipped().empty());
    CHECK_EQ(s.armyBonus().attack, 0);
    AdventureSession b;
    CHECK(!b.loadState(Json::parse(s.saveState().dump())));
    CHECK(b.keepsakes().size() == 1 && b.keepsakes()[0]->id == "test_dog_tag");
    CHECK_EQ(b.armyBonus().attack, 0);
}

SUITE("Story — requires_item hides an option; take_item hands it over once, and stays handed over") {
    const std::string data = keepsakeData();
    AdventureSession without;
    CHECK(!without.start(spoilsMap(), data, spoilsEncounters(), 1, warriorTriggers()));
    without.travel({1, 0});
    CHECK(without.scenario().awaitingChoice());
    CHECK_EQ(optionCount(without.scenario().offeredChoice(without)), 1);   // only "fight"
    CHECK_EQ(optionCount(without.scenario().choice()), 2);
    CHECK(without.scenario().choose(without, "give_tag").has_value());   // refused
    CHECK(without.pendingEncounter().has_value());

    AdventureSession s;
    CHECK(!s.start(spoilsMap(), data, spoilsEncounters(), 1, warriorTriggers()));
    s.addItem("test_dog_tag");
    s.scenario().run(s, Json::parse(R"([{"take_item": "test_dog_tag"}])"));
    CHECK(!s.hasItem("test_dog_tag"));
    s.scenario().run(s, Json::parse(R"([{"take_item": "test_dog_tag"}])"));   // not carried: nothing happens
    CHECK(!s.hasItem("test_dog_tag") && s.keepsakes().empty());
    AdventureSession b;
    CHECK(!b.loadState(Json::parse(s.saveState().dump())));
    CHECK(!b.hasItem("test_dog_tag"));
}

SUITE("Story — handing over the tag at engage: no battle, no XP, no spoils, the guard is gone") {
    const std::string data = keepsakeData();
    AdventureSession s;
    CHECK(!s.start(spoilsMap(), data, spoilsEncounters(), 1, warriorTriggers()));
    s.addItem("test_dog_tag");
    const int xp = s.heroProgress().xp;
    const int gold = s.treasury()[Resource::Gold];
    s.travel({1, 0});
    CHECK(s.pendingEncounter() && *s.pendingEncounter() == HexCoord(2, 0));
    CHECK_EQ(optionCount(s.scenario().offeredChoice(s)), 2);
    CHECK(!s.scenario().choose(s, "give_tag"));
    CHECK(!s.pendingEncounter());                        // no fight to start
    CHECK(!s.isEncounter({2, 0}) && !s.guardZoneAt({1, 0}));
    CHECK(!s.hasItem("test_dog_tag"));
    CHECK_EQ(s.heroProgress().xp, xp);
    CHECK_EQ(s.treasury()[Resource::Gold], gold);
    CHECK(s.lastSpoils().empty());
    CHECK(!s.flags().contains("fought"));                // not a battle won
    CHECK(s.flags().value("way_open", false));           // but the way is open
    s.travel({3, 0});                                    // and the map goes on past his hex
    CHECK(s.heroPos() == HexCoord(3, 0));
    AdventureSession b;
    CHECK(!b.loadState(Json::parse(s.saveState().dump())));
    CHECK(!b.isEncounter({2, 0}) && !b.hasItem("test_dog_tag"));
}
