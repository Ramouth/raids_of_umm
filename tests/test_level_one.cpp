// test_level_one.cpp — the demo's opening level: no companion at the start,
// clear the vale's wolves (a hooded druid speaks before the last pack), and
// Ushari rides in once every pack is dead. Also the trigger tools it needs:
// "start_companions", the "cleared" and "engage" events, "wait" and "vanish".
#include "test_runner.h"
#include "adventure/AdventureSession.h"
#include <fstream>

namespace {

const char* kMap        = "data/maps/old_passage.json";
const char* kEncounters = "data/maps/old_passage.encounters.json";
const char* kTriggers   = "data/maps/old_passage.triggers.json";

HexCoord cellOf(const AdventureSession& s, const std::string& name) {
    for (const auto& o : s.map().objects()) if (o.name == name) return o.pos;
    return {99, 99};
}

bool spoke(const std::vector<Scenario::Line>& lines, const std::string& speaker) {
    for (const auto& l : lines) if (l.speaker == speaker) return true;
    return false;
}

bool hasQuest(const AdventureSession& s, const std::string& id, bool done) {
    for (const auto& q : s.scenario().quests()) if (q.id == id) return q.done == done;
    return false;
}

// March on a guard camp day after day and win every fight on the way.
// Returns the dialogue heard just before the fight at `name` began.
std::vector<Scenario::Line> beat(AdventureSession& s, const std::string& name) {
    HexCoord at = cellOf(s, name);
    std::vector<Scenario::Line> before;
    for (int day = 0; day < 12 && s.isEncounter(at); ++day) {
        s.scenario().drainLines();
        s.travel(at);
        while (auto pending = s.pendingEncounter()) {
            if (*pending == at) before = s.scenario().drainLines();
            s.resolveEncounter(true);
            if (!s.isEncounter(at)) break;
            s.travel(at);
        }
        if (s.isEncounter(at)) s.endDay();
    }
    CHECK(!s.isEncounter(at));
    return before;
}

} // namespace

SUITE("Level 1 — the expedition starts without Ushari, sent after the vale's wolves") {
    AdventureSession s;
    CHECK(!s.start(kMap, "data", kEncounters, 1, kTriggers));
    CHECK(s.specials().empty());
    CHECK(hasQuest(s, "wolves", false));
    CHECK(!hasQuest(s, "main", false));
    auto lines = s.scenario().drainLines();
    CHECK(spoke(lines, "Steward"));
    CHECK(!spoke(lines, "Ushari"));
    CHECK(s.map().objectAt(cellOf(s, "Hooded Stranger")) != nullptr);
}

SUITE("Level 1 — the druid speaks before his pack fights, then walks off; Ushari arrives") {
    AdventureSession s;
    CHECK(!s.start(kMap, "data", kEncounters, 1, kTriggers));
    s.setArmy({{"levy_spearman", 24}, {"desert_archer", 10}, {"armoured_warrior", 3}});
    beat(s, "Hill Wolves");
    beat(s, "Den Wolves");
    CHECK(s.specials().empty());                         // one pack still hunts the vale
    auto heard = beat(s, "Hermit's Wolves");
    CHECK(spoke(heard, "Hooded Druid"));
    CHECK(s.scenario().vanished().count("Hooded Stranger") == 1);
    auto after = s.scenario().drainLines();
    CHECK(spoke(after, "Ushari"));
    CHECK(!spoke(after, "Corvin"));                      // his letter comes the next morning
    CHECK_EQ((int)s.specials().size(), 1);
    if (!s.specials().empty()) CHECK(s.specials()[0].id == "ushari");
    CHECK(hasQuest(s, "wolves", true));
    CHECK(hasQuest(s, "main", false));
    s.endDay();
    CHECK(spoke(s.scenario().drainLines(), "Corvin"));
    CHECK(hasQuest(s, "bridge", false));
    // A save keeps the arrival, the beaten packs and the druid's absence.
    AdventureSession c;
    CHECK(!c.loadState(Scenario::Json::parse(s.saveState().dump())));
    CHECK_EQ((int)c.specials().size(), 1);
    CHECK(c.scenario().vanished().count("Hooded Stranger") == 1);
    CHECK(hasQuest(c, "wolves", true));
}

SUITE("Level 1 — Ushari's lines wait for her; other beats do not") {
    AdventureSession s;
    CHECK(!s.start(kMap, "data", kEncounters, 1, kTriggers));
    s.setArmy({{"levy_spearman", 24}, {"desert_archer", 10}, {"armoured_warrior", 3}});
    for (int day = 0; day < 4 && s.heroPos() != cellOf(s, "Tarn Obelisk"); ++day) {
        s.travel(cellOf(s, "Tarn Obelisk"));
        while (s.pendingEncounter()) { s.resolveEncounter(true); s.travel(cellOf(s, "Tarn Obelisk")); }
        if (s.heroPos() != cellOf(s, "Tarn Obelisk")) s.endDay();
    }
    CHECK(s.heroPos() == cellOf(s, "Tarn Obelisk"));
    auto early = s.scenario().drainLines();
    CHECK(spoke(early, "Inscription"));                  // the stone speaks at once
    CHECK(!spoke(early, "Ushari"));                      // her remark waits
    // Her held remark survives a save and plays when she rides in.
    AdventureSession c;
    CHECK(!c.loadState(Scenario::Json::parse(s.saveState().dump())));
    for (const char* pack : {"Hill Wolves", "Den Wolves", "Hermit's Wolves"}) beat(c, pack);
    CHECK_EQ((int)c.specials().size(), 1);
    bool remark = false;
    for (const auto& l : c.scenario().drainLines())
        remark = remark || (l.speaker == "Ushari" && l.text.find("It is not carved") != std::string::npos);
    CHECK(remark);
}

SUITE("Scenario — wait holds a beat until its cue; cleared needs every camp; vanish") {
    WorldMap map;
    map.clear(6);
    for (const auto& [c, t] : map) map.setTile(c, makeTile(Terrain::Grass));
    map.placeObject({{-4, 0}, ObjType::Town, "Home", 1});
    map.placeObject({{-1, 0}, ObjType::Guard, "Pack A", 0});
    map.placeObject({{ 1, -3}, ObjType::Guard, "Pack B", 0});
    map.placeObject({{ 2, 0}, ObjType::QuestGiver, "Stranger", 0});
    const std::string path = "/tmp/raids_test_level_one.json";
    { std::ofstream f(path); f << R"({"start_companions":[],"triggers":[
        {"id":"early","when":{"event":"encounter_won","name":"Pack A","wait":"arrive"},"do":[{"say":[["Ushari","Late news."]]}]},
        {"id":"talk","when":{"event":"engage","name":"Pack A"},"do":[{"say":[["Druid","Teeth, then."]]}]},
        {"id":"arrive","when":{"event":"cleared","names":["Pack A","Pack B"]},
         "do":[{"join":"ushari"},{"vanish":"Stranger"},{"say":[["Ushari","Here."]]}]}]})"; }
    AdventureSession s;
    CHECK(!s.start(std::move(map), "data", "", 0, path));
    CHECK(s.specials().empty());
    s.travel({-1, 0});
    CHECK(s.pendingEncounter().has_value());
    auto talk = s.scenario().drainLines();
    CHECK(spoke(talk, "Druid"));
    s.resolveEncounter(true);
    CHECK(s.scenario().drainLines().empty());            // "early" is held, "arrive" not yet
    CHECK(s.specials().empty());
    s.travel({1, -3});
    CHECK(s.pendingEncounter().has_value());
    s.resolveEncounter(true);
    auto lines = s.scenario().drainLines();
    CHECK_EQ((int)lines.size(), 2);
    if (lines.size() == 2) {
        CHECK(lines[0].text == "Here.");                 // the cue first, then what waited for it
        CHECK(lines[1].text == "Late news.");
    }
    CHECK_EQ((int)s.specials().size(), 1);
    CHECK(s.scenario().vanished().count("Stranger") == 1);
}
