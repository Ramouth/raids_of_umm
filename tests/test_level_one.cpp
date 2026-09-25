// test_level_one.cpp — the demo's opening level: no companion at the start,
// clear the vale's wolves, then search for Aldren with Ushari.
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

// Walk to a place day after day, winning every fight on the way.
void reach(AdventureSession& s, HexCoord at) {
    for (int day = 0; day < 14 && s.heroPos() != at; ++day) {
        s.travel(at);
        while (s.pendingEncounter()) { s.resolveEncounter(true); s.travel(at); }
        if (s.heroPos() != at) s.endDay();
        while (s.pendingEncounter()) s.resolveEncounter(true);   // a night ambush
    }
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
    CHECK(s.map().objectAt(cellOf(s, "Hooded Stranger")) == nullptr);
    CHECK(s.map().objectAt(cellOf(s, "Kharim's Camp")) == nullptr);
}

SUITE("Level 1 — clearing all three packs brings Ushari and the search for Aldren") {
    AdventureSession s;
    CHECK(!s.start(kMap, "data", kEncounters, 1, kTriggers));
    s.setArmy({{"levy_spearman", 24}, {"desert_archer", 10}, {"armoured_warrior", 3}});
    beat(s, "Hill Wolves");
    beat(s, "Den Wolves");
    CHECK(s.specials().empty());                         // one pack still hunts the vale
    auto heard = beat(s, "Hermit's Wolves");
    CHECK(!spoke(heard, "Hooded Druid"));
    CHECK(s.scenario().vanished().empty());
    auto after = s.scenario().drainLines();
    CHECK(spoke(after, "Ushari"));
    CHECK(!spoke(after, "Corvin"));                      // his letter comes the next morning
    CHECK_EQ((int)s.specials().size(), 1);
    if (!s.specials().empty()) CHECK(s.specials()[0].id == "ushari");
    CHECK(hasQuest(s, "wolves", true));
    CHECK(hasQuest(s, "aldren", false));
    CHECK_EQ(s.heroProgress().level, 2);                 // the first area is worth a level
    CHECK(s.scenario().quests().size() > 0 && s.needsPath());   // level 2: time to choose a path
    CHECK_EQ(s.specials()[0].xp, 0);                     // she arrives after the reward
    CHECK(!hasQuest(s, "bridge", false));                // the politics wait for the morning
    s.endDay();
    CHECK(spoke(s.scenario().drainLines(), "Ushari"));
    CHECK(hasQuest(s, "bridge", false));
    // A save keeps the arrival, the beaten packs and the druid's absence.
    AdventureSession c;
    CHECK(!c.loadState(Scenario::Json::parse(s.saveState().dump())));
    CHECK_EQ((int)c.specials().size(), 1);
    CHECK(c.scenario().vanished().empty());
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
        remark = remark || (l.speaker == "Ushari" && l.text.find("My mother") != std::string::npos);
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

SUITE("Level 1 — Corvin remains a useful ally, even after the old betrayal deadline") {
    AdventureSession s;
    CHECK(!s.start(kMap, "data", kEncounters, 1, kTriggers));
    s.setArmy({{"rider_knight", 30}, {"levy_spearman", 40}, {"desert_archer", 20}});
    for (const char* pack : {"Hill Wolves", "Den Wolves", "Hermit's Wolves"}) beat(s, pack);
    reach(s, cellOf(s, "Hallowmere"));
    CHECK(s.hasItem("hale_signet"));
    CHECK(hasQuest(s, "bridge", true));
    CHECK(!s.acceptOffer("hire_hale"));
    for (int i = 0; i < 20; ++i) {
        s.endDay();
        while (s.pendingEncounter()) s.resolveEncounter(true);
    }
    bool stillHired = false;
    for (const auto& st : s.army()) if (st.id == "hale_man_at_arms") stillHired = true;
    CHECK(stillHired);
    CHECK_EQ(s.owner(cellOf(s, "Hallowmere")), 1);
    for (const auto& r : s.rivals()) CHECK(r.name != "Hale household" && r.name != "Hale men-at-arms");
    for (const auto& line : s.scenario().drainLines())
        CHECK(line.text.find("He is dead") == std::string::npos);
}

SUITE("Level 1 — both branches persist, take the same night, and cannot be combined") {
    AdventureSession s;
    CHECK(!s.start(kMap, "data", kEncounters, 1, kTriggers));
    s.setArmy({{"rider_knight", 60}});
    for (const char* pack : {"Hill Wolves", "Den Wolves", "Hermit's Wolves"}) beat(s, pack);
    reach(s, cellOf(s, "Hallowmere"));
    CHECK(s.passageMine().has_value());
    auto passage = *s.passageMine();
    beat(s, s.map().objectAt(passage)->name);
    CHECK(!s.won());
    CHECK(s.scenario().awaitingChoice());
    if (!s.scenario().awaitingChoice()) return;
    const int night = s.day();
    const auto here = s.heroPos();
    s.endDay();
    CHECK_EQ(s.day(), night);
    CHECK(s.travel(cellOf(s, "Varenhold")).empty());
    CHECK(s.heroPos() == here);
    CHECK(s.scenario().choose(s, "invalid").has_value());
    CHECK(s.scenario().awaitingChoice());
    const auto pending = Scenario::Json::parse(s.saveState().dump());
    const auto ushariXp = s.specials()[0].xp;
    for (const std::string branch : {"return_to_father", "follow_ushari"}) {
        AdventureSession route;
        CHECK(!route.loadState(pending));
        CHECK(route.scenario().awaitingChoice());
        CHECK(!route.scenario().choose(route, branch));
        CHECK(route.won());
        CHECK(!route.scenario().awaitingChoice());
        CHECK_EQ(route.scenario().outcome().at("night").get<int>(), night);
        CHECK(route.scenario().choose(route, "follow_ushari").has_value());
        CHECK(!hasQuest(route, "aldren", true)); // we have not found our brother
        const bool home = branch == "return_to_father";
        CHECK_EQ((int)route.specials().size(), home ? 0 : 1);
        if (home) CHECK(route.heroPos() == cellOf(route, "Varenhold"));
        CHECK(route.scenario().outcome().at("ushari") == (home ? "entered_alone" : "with_player"));
        AdventureSession loaded;
        CHECK(!loaded.loadState(Scenario::Json::parse(route.saveState().dump())));
        CHECK(loaded.won());
        CHECK(loaded.scenario().outcome() == route.scenario().outcome());
        CHECK_EQ((int)loaded.specials().size(), home ? 0 : 1);
        loaded.joinSpecial("ushari"); // future reunion retains her progression
        CHECK_EQ((int)loaded.specials().size(), 1);
        CHECK_EQ(loaded.specials()[0].xp, ushariXp);
    }
}

SUITE("Level 1 — an early passage discovery waits for the family visit") {
    AdventureSession s;
    CHECK(!s.start(kMap, "data", kEncounters, 2, kTriggers));
    s.scenario().fire(s, "passage_found");
    CHECK(!s.scenario().awaitingChoice());
    AdventureSession loaded;
    CHECK(!loaded.loadState(Scenario::Json::parse(s.saveState().dump())));
    for (const auto* name : {"Hill Wolves", "Den Wolves", "Hermit's Wolves"})
        loaded.scenario().fire(loaded, "encounter_won", {{"name", name}});
    loaded.scenario().fire(loaded, "cleared");
    CHECK(!loaded.scenario().awaitingChoice());
    loaded.scenario().fire(loaded, "visit", {{"name", "Hallowmere"}});
    CHECK(loaded.scenario().awaitingChoice());
}

SUITE("Level 1 — Ushari stays on the search despite wounds or unpaid upkeep") {
    AdventureSession s;
    CHECK(!s.start(kMap, "data", kEncounters, 1, kTriggers));
    s.joinSpecial("ushari");
    CHECK(s.station("ushari", true).has_value());
    s.companionsFell({"ushari"}, true);
    CHECK_EQ((int)s.specials().size(), 1);
    CHECK(s.isWounded(s.specials()[0]));
    for (int day = 0; day < 8; ++day) { ResourcePool drain; drain[Resource::Gold] = -s.treasury()[Resource::Gold]; s.give(drain); s.endDay(); }
    CHECK_EQ((int)s.specials().size(), 1);
}

SUITE("Scenario — turncoats: nobody hired, nothing happens; hired men leave army and garrisons") {
    WorldMap map;
    map.clear(6);
    for (const auto& [c, t] : map) map.setTile(c, makeTile(Terrain::Grass));
    map.placeObject({{-4, 0}, ObjType::Town, "Home", 1});
    map.placeObject({{ 3, 0}, ObjType::Town, "Keep", 1});
    AdventureSession s;
    CHECK(!s.start(std::move(map), "data"));
    s.setArmy({{"levy_spearman", 10}});
    CHECK_EQ(s.turncoats("hale_man_at_arms", "Turncoats"), 0);
    CHECK(s.rivals().empty());
    CHECK(!s.pendingEncounter().has_value());
    s.setArmy({{"levy_spearman", 10}, {"hale_man_at_arms", 12}});
    CHECK(!s.transfer({-4, 0}, "hale_man_at_arms", 5, true));   // leave five in the garrison
    CHECK_EQ(s.turncoats("hale_man_at_arms", "Turncoats"), 12);
    CHECK_EQ((int)s.army().size(), 1);
    if (const auto* g = s.garrison({-4, 0})) for (const auto& st : *g) CHECK(st.id != "hale_man_at_arms");
    CHECK_EQ((int)s.rivals().size(), 1);
    CHECK(s.pendingIsAmbush());
}
