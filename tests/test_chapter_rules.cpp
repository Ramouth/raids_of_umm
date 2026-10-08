// test_chapter_rules.cpp — rules chapter 1's story data needs: an authored start
// cell, a per-map starting army, data-defined companions (Aldren), guards that
// vanish or withdraw, and choices that can be offered again.
#include "test_runner.h"
#include "adventure/AdventureSession.h"
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace {

using Json = nlohmann::json;

std::string tempPath(const std::string& name) {
    return (std::filesystem::temp_directory_path() / ("umm_chapter_rules_" + name)).string();
}

std::string writeTriggers(const std::string& name, const Json& j) {
    const std::string path = tempPath(name + ".triggers.json");
    std::ofstream(path) << j.dump();
    return path;
}

// A townless grass map: no town to spawn at, a pack of wolves and a shrine.
WorldMap fieldMap(std::optional<HexCoord> start = std::nullopt) {
    WorldMap map;
    map.clear(6);
    for (const auto& [c, t] : map) map.setTile(c, makeTile(Terrain::Grass));
    map.placeObject({{3, 0}, ObjType::Guard, "Hill Wolves", 0, ""});
    map.placeObject({{-3, 3}, ObjType::QuestGiver, "Shrine", 0, ""});
    map.setStart(start);
    return map;
}

int countOf(const std::vector<AdventureSession::Stack>& army, const std::string& id) {
    for (const auto& st : army) if (st.id == id) return st.count;
    return 0;
}

bool rides(const AdventureSession& s, const std::string& id) {
    return std::any_of(s.specials().begin(), s.specials().end(), [&](const auto& sc) { return sc.id == id; });
}

} // namespace

SUITE("WorldMap — the authored start cell survives save, load and clone") {
    WorldMap map = fieldMap(HexCoord{0, -4});
    const std::string path = tempPath("start_map.json");
    CHECK(!map.saveJson(path));
    WorldMap back;
    CHECK(!back.loadJson(path));
    CHECK(back.start() && *back.start() == HexCoord(0, -4));
    CHECK(back.clone().start() == back.start());

    WorldMap none = fieldMap();
    CHECK(!none.saveJson(path));
    CHECK(!back.loadJson(path));                 // a map without "start" clears the old one
    CHECK(!back.start());
}

SUITE("AdventureSession — the hero starts on the map's start cell before any town or first tile") {
    AdventureSession s;
    CHECK(!s.start(fieldMap(HexCoord{-2, 1}), "data", "", 1));
    CHECK(s.heroPos() == HexCoord(-2, 1));

    WorldMap withTown = fieldMap(HexCoord{1, 1});
    withTown.placeObject({{-5, 0}, ObjType::Town, "Home", 1, ""});
    CHECK(!s.start(std::move(withTown), "data", "", 1));
    CHECK(s.heroPos() == HexCoord(1, 1));       // start beats the player town

    WorldMap fallback = fieldMap();
    fallback.placeObject({{-5, 0}, ObjType::Town, "Home", 1, ""});
    CHECK(!s.start(std::move(fallback), "data", "", 1));
    CHECK(s.heroPos() == HexCoord(-5, 0));      // no start: the old rule
}

SUITE("AdventureSession — start_army arms a fresh start; carried state wins") {
    const std::string trig = writeTriggers("army", Json::parse(R"({"start_companions": [],
        "start_army": [{"id": "woad_runner", "count": 10}, {"id": "cruth_slinger", "count": 4}], "triggers": []})"));
    AdventureSession s;
    CHECK(!s.start(fieldMap(), "data", "", 1, trig));
    CHECK_EQ(countOf(s.army(), "woad_runner"), 10);
    CHECK_EQ(countOf(s.army(), "cruth_slinger"), 4);

    CampaignState fresh;                          // a new campaign: nothing carried yet
    CHECK(!s.start(fieldMap(), "data", "", 1, trig, fresh));
    CHECK_EQ(countOf(s.army(), "woad_runner"), 10);

    CampaignState carried;
    carried.carried = true;
    carried.army = {{"painted_blade", 3}};
    CHECK(!s.start(fieldMap(), "data", "", 1, trig, carried));
    CHECK_EQ(countOf(s.army(), "painted_blade"), 3);
    CHECK_EQ(countOf(s.army(), "woad_runner"), 0);
}

SUITE("AdventureSession — Aldren is a companion from data: starts, fights, leaves, rejoins, saves") {
    const std::string mapPath = tempPath("aldren_map.json");
    CHECK(!fieldMap(HexCoord{0, 0}).saveJson(mapPath));
    const std::string trig = writeTriggers("aldren", Json::parse(R"({"start_companions": ["aldren"], "triggers": [
        {"id": "go",   "when": {"event": "visit", "name": "Shrine"}, "do": [{"leave": "aldren"}]},
        {"id": "back", "when": {"event": "night", "after": "go"}, "do": [{"join": "aldren"}]}]})"));
    AdventureSession s;
    CHECK(!s.start(mapPath, "data", "", 1, trig));
    CHECK(rides(s, "aldren"));
    CHECK(s.specials().front().name == "Aldren");
    auto fighters = s.battleCompanions();
    CHECK(fighters.size() == 1 && fighters.front().id == "aldren");

    s.grantXp(120);
    const int xp = s.specials().front().xp;
    CHECK(xp > 0);
    const Json save = s.saveState();
    AdventureSession loaded;
    CHECK(!loaded.loadState(save));
    CHECK(rides(loaded, "aldren") && loaded.specials().front().xp == xp);

    s.travel({-3, 3});                           // the shrine: he leaves
    CHECK(!rides(s, "aldren"));
    CHECK(s.battleCompanions().empty());
    s.endDay();                                  // and rides back, his experience kept
    CHECK(rides(s, "aldren"));
    CHECK_EQ(s.specials().front().xp, xp);
}

SUITE("Scenario — vanish of a guard takes its hex, zone of control and counts it cleared") {
    const std::string trig = writeTriggers("vanish", Json::parse(R"({"start_companions": [], "triggers": [
        {"id": "gone",  "when": {"event": "visit", "name": "Shrine"}, "do": [{"vanish": "Hill Wolves"}]},
        {"id": "clear", "when": {"event": "cleared", "names": ["Hill Wolves"]}, "do": [{"flag": {"vale_clear": true}}]}]})"));
    AdventureSession s;
    CHECK(!s.start(fieldMap(HexCoord{0, 0}), "data", "", 1, trig));
    CHECK(s.isEncounter({3, 0}) && s.guardZoneAt({2, 0}));
    s.travel({-3, 3});
    CHECK(s.scenario().vanished().count("Hill Wolves"));
    CHECK(!s.isEncounter({3, 0}));
    CHECK(!s.guardZoneAt({2, 0}));
    CHECK(s.flags().value("vale_clear", false));
    s.endDay();
    s.travel({4, 0});                            // walks through where the pack was
    CHECK(s.heroPos() == HexCoord(4, 0) && !s.pendingEncounter());
}

SUITE("Scenario — withdraw in a choice at engage calls the fight off and the pack leaves") {
    const std::string mapPath = tempPath("withdraw_map.json");
    CHECK(!fieldMap(HexCoord{0, 0}).saveJson(mapPath));
    const std::string trig = writeTriggers("withdraw", Json::parse(R"({"start_companions": [], "triggers": [
        {"id": "meet", "when": {"event": "engage", "name": "Hill Wolves"}, "do": [{"choice": {"id": "pack", "options": [
            {"id": "fight", "then": []},
            {"id": "spare", "then": [{"withdraw": "Hill Wolves"}]}]}}]}]})"));
    AdventureSession fight;
    CHECK(!fight.start(mapPath, "data", "", 1, trig));
    fight.travel({3, 0});
    CHECK(fight.pendingEncounter() && *fight.pendingEncounter() == HexCoord(3, 0));
    CHECK(fight.scenario().awaitingChoice());
    CHECK(!fight.scenario().choose(fight, "fight"));
    CHECK(fight.pendingEncounter().has_value());  // the other option still fights

    AdventureSession s;
    CHECK(!s.start(mapPath, "data", "", 1, trig));
    s.travel({3, 0});
    CHECK(s.pendingEncounter().has_value());
    CHECK(!s.scenario().choose(s, "spare"));
    CHECK(!s.pendingEncounter());
    CHECK(!s.isEncounter({3, 0}) && !s.guardZoneAt({2, 0}));
    CHECK(s.scenario().vanished().count("Hill Wolves"));
    CHECK(s.resolveEncounter(true) == AdventureSession::MineFind::None);   // nothing left to fight

    AdventureSession loaded;                     // the pack stays gone after a load
    CHECK(!loaded.loadState(s.saveState()));
    CHECK(!loaded.isEncounter({3, 0}) && loaded.scenario().vanished().count("Hill Wolves"));
}

SUITE("Scenario — an option with again closes the choice and leaves its trigger live") {
    const std::string mapPath = tempPath("again_map.json");
    CHECK(!fieldMap(HexCoord{0, 0}).saveJson(mapPath));
    const std::string trig = writeTriggers("again", Json::parse(R"({"start_companions": [], "triggers": [
        {"id": "roots", "when": {"event": "visit", "name": "Shrine"}, "do": [{"choice": {"id": "roots", "options": [
            {"id": "turn_back", "again": true, "then": [{"say": [["Aldren", "Not yet."]]}]},
            {"id": "go", "then": [{"flag": {"went": true}}]}]}}]},
        {"id": "wolves", "when": {"event": "engage", "name": "Hill Wolves"}, "do": [{"choice": {"id": "wolves", "options": [
            {"id": "back_off", "again": true, "then": []},
            {"id": "attack", "then": []}]}}]}]})"));
    AdventureSession s;
    CHECK(!s.start(mapPath, "data", "", 1, trig));
    s.travel({-3, 3});
    CHECK(s.scenario().awaitingChoice());
    CHECK(s.scenario().choiceTrigger() == "roots");
    CHECK(!s.scenario().choose(s, "turn_back"));
    CHECK(!s.scenario().awaitingChoice());
    CHECK(!s.scenario().fired("roots"));
    CHECK(s.flags().value("choice.roots", "") == "turn_back");

    s.travel({-2, 3});
    s.travel({-3, 3});                           // back again: offered again
    CHECK(s.scenario().awaitingChoice());
    AdventureSession loaded;                     // the open choice still knows its trigger after a load
    CHECK(!loaded.loadState(s.saveState()));
    CHECK(loaded.scenario().choiceTrigger() == "roots");
    CHECK(!loaded.scenario().choose(loaded, "turn_back"));
    CHECK(!loaded.scenario().fired("roots"));

    CHECK(!s.scenario().choose(s, "go"));        // an ordinary option uses the trigger up
    CHECK(s.scenario().fired("roots"));
    s.travel({-2, 3});
    s.travel({-3, 3});
    CHECK(!s.scenario().awaitingChoice());

    s.endDay();                                  // at an engage, again also calls the fight off
    s.travel({3, 0});
    CHECK(s.pendingEncounter() && s.scenario().awaitingChoice());
    CHECK(!s.scenario().choose(s, "back_off"));
    CHECK(!s.pendingEncounter());
    CHECK(s.isEncounter({3, 0}));               // the pack stays
    s.endDay();
    s.travel({3, 0});
    CHECK(s.pendingEncounter() && s.scenario().awaitingChoice());
    CHECK(!s.scenario().choose(s, "attack"));
    CHECK(s.pendingEncounter().has_value());
}
