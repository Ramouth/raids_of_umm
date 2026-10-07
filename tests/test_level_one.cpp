// test_level_one.cpp — the demo's opening level: no companion at the start,
// clear the vale's wolves, then search for Aldren with Ushari.
// Ushari rides in at the next night after every pack is dead. Also the trigger tools it needs:
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

// Corridors can have a guard on the route itself. Clear an approachable
// blocking camp before resuming the destination order, as the player would.
HexCoord nextStop(AdventureSession& s, HexCoord target) {
    if (s.scenario().choice().value("id", "") == "rook_passage")
        CHECK(!s.scenario().choose(s, "decline_rook_passage"));
    if (!s.route(target).empty()) return target;
    const auto path = s.map().findPathWeighted(s.heroPos(), target, [](const HexCoord&) { return false; });
    for (auto at : path) if (at != s.heroPos() && s.isEncounter(at)) return at;
    return target;
}

// Walk to a place day after day, winning every fight on the way.
void reach(AdventureSession& s, HexCoord at) {
    if (hasQuest(s, "wolves", true) && s.specials().empty()) s.endDay();
    if (s.scenario().choice().value("id", "") == "ushari_at_night")
        CHECK(!s.scenario().choose(s, "stand_by_aldren"));
    for (int day = 0; day < 40 && s.heroPos() != at; ++day) {
        s.travel(nextStop(s, at));
        while (s.pendingEncounter()) { s.resolveEncounter(true); s.travel(nextStop(s, at)); }
        if (s.heroPos() != at) s.endDay();
        while (s.pendingEncounter()) s.resolveEncounter(true);   // a night ambush
    }
}

// March on a guard camp day after day and win every fight on the way.
// Returns the dialogue heard just before the fight at `name` began.
std::vector<Scenario::Line> beat(AdventureSession& s, const std::string& name) {
    HexCoord at = cellOf(s, name);
    std::vector<Scenario::Line> before;
    for (int day = 0; day < 40 && s.isEncounter(at); ++day) {
        s.scenario().drainLines();
        s.travel(nextStop(s, at));
        while (auto pending = s.pendingEncounter()) {
            if (*pending == at) before = s.scenario().drainLines();
            s.resolveEncounter(true);
            if (!s.isEncounter(at)) break;
            s.travel(nextStop(s, at));
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
    CHECK(s.map().objectAt(cellOf(s, "Kharim's Camp")) == nullptr);
}

SUITE("Level 1 — clearing all three packs brings Ushari and the search for Aldren") {
    AdventureSession s;
    CHECK(!s.start(kMap, "data", kEncounters, 1, kTriggers));
    s.setArmy({{"levy_spearman", 24}, {"desert_archer", 10}, {"armoured_warrior", 3}});
    beat(s, "Hill Wolves");
    beat(s, "Den Wolves");
    CHECK(s.specials().empty());                         // one pack still hunts the vale
    s.endDay();
    CHECK(s.specials().empty());                         // night alone is not enough
    CHECK(!s.scenario().awaitingChoice());
    auto heard = beat(s, "Hermit's Wolves");
    CHECK(spoke(heard, "Hooded Druid"));
    CHECK(s.scenario().vanished().count("Hooded Stranger") == 1);
    CHECK(s.specials().empty());
    CHECK(!s.scenario().awaitingChoice());
    CHECK(!hasQuest(s, "aldren", false));
    // Saving between the last kill and nightfall retains the scheduled arrival.
    AdventureSession waiting;
    CHECK(!waiting.loadState(s.saveState()));
    CHECK(waiting.specials().empty());
    waiting.endDay();
    CHECK_EQ((int)waiting.specials().size(), 1);
    CHECK(waiting.scenario().choice().at("id") == "ushari_at_night");
    s.endDay();
    auto after = s.scenario().drainLines();
    CHECK(s.scenario().choice().at("id") == "ushari_at_night");
    CHECK(!s.scenario().choose(s, "stand_by_aldren"));
    CHECK_EQ(s.scenario().morale("ushari"), 0);
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
    c.endDay();
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
    reach(s, cellOf(s, "Aldren's Camp"));
    for (const char* stone : {"Varenhold Wall Rune Stone", "Coldwater Bridge Rune Stone", "Assembly Rune Stone"})
        { reach(s, cellOf(s, stone)); CHECK(s.heroPos() == cellOf(s, stone)); }
    beat(s, "Troll Gate");
    reach(s, cellOf(s, "Troll Gate"));
    CHECK(!s.departureRoute().empty());
    CHECK(s.passageMine().has_value());
    auto passage = *s.passageMine();
    beat(s, s.map().objectAt(passage)->name);
    CHECK(!s.won());
    CHECK(s.scenario().awaitingChoice());
    if (!s.scenario().awaitingChoice()) return;
    const int night = s.day();
    CHECK(!s.scenario().blocksAdventure());
    CHECK(s.scenario().choose(s, "invalid").has_value());
    const auto pending = Scenario::Json::parse(s.saveState().dump());
    AdventureSession exploring;
    CHECK(!exploring.loadState(pending));
    exploring.endDay();
    CHECK_EQ(exploring.day(), night + 1);
    const auto gate = exploring.heroPos();
    bool left = false;
    for (int direction = 0; direction < 6; ++direction) {
        auto neighbor = gate.neighbor(direction);
        if (exploring.isEncounter(neighbor)) continue;
        auto path = exploring.route(neighbor);
        if (path.size() != 1) continue;
        exploring.travel(neighbor);
        if (exploring.heroPos() != gate) { left = true; break; }
    }
    CHECK(left);
    CHECK(exploring.scenario().choose(exploring, "esh_varen_tal_eren_orun_ishara").has_value());
    AdventureSession resumed;
    CHECK(!resumed.loadState(Scenario::Json::parse(exploring.saveState().dump())));
    CHECK(resumed.scenario().choice().at("at") == s.scenario().choice().at("at"));
    resumed.travel(gate);
    CHECK(resumed.heroPos() == gate);
    CHECK(!resumed.scenario().choose(resumed, "esh_varen_tal_eren_orun_ishara"));
    const auto ushariXp = s.specials()[0].xp;
    for (const std::string branch : {"return_to_father", "follow_ushari"}) {
        AdventureSession route;
        CHECK(!route.loadState(pending));
        CHECK(route.scenario().awaitingChoice());
        CHECK(route.scenario().choice().at("id") == "passage_gate");
        CHECK(route.scenario().choose(route, branch).has_value());
        CHECK(!route.scenario().choose(route, "esh_varen_tal_eren_orun_ishara"));
        CHECK(route.scenario().choice().at("id") == "at_the_passage");
        CHECK(route.scenario().blocksAdventure());
        route.endDay();
        CHECK_EQ(route.day(), night);
        CHECK(route.travel(cellOf(route, "Varenhold")).empty());
        CHECK(!route.scenario().choose(route, branch));
        CHECK(route.won());
        CHECK(!route.scenario().awaitingChoice());
        CHECK_EQ(route.scenario().outcome().at("night").get<int>(), night);
        CHECK(route.scenario().choose(route, "follow_ushari").has_value());
        CHECK(!hasQuest(route, "aldren", true)); // we have not found our brother
        const bool home = branch == "return_to_father";
        CHECK_EQ((int)route.specials().size(), 1);
        if (home) CHECK(route.heroPos() == cellOf(route, "Varenhold"));
        CHECK(route.scenario().outcome().at("ushari") == "with_player");
        CHECK(route.scenario().outcome().at("route") == "greyfen_road");
        CHECK(route.scenario().outcome().at("aldren_trace") == "annotated_survey");
        CHECK(route.scenario().outcome().at("passage_entered") == false);
        CHECK(route.scenario().outcome().at("next_chapter") == "greyfen_road");
        for (const auto& frame : route.scenario().outcome().at("frames")) {
            CHECK(frame.at("scene") != "tunnel");
            CHECK(frame.at("scene") != "alone");
            CHECK(frame.at("scene") != "together");
            CHECK(frame.at("text").get<std::string>().find("zombie") == std::string::npos);
        }
        AdventureSession loaded;
        CHECK(!loaded.loadState(Scenario::Json::parse(route.saveState().dump())));
        CHECK(loaded.won());
        CHECK(loaded.scenario().outcome() == route.scenario().outcome());
        CHECK_EQ((int)loaded.specials().size(), 1);
        loaded.joinSpecial("ushari"); // future reunion retains her progression
        CHECK_EQ((int)loaded.specials().size(), 1);
        CHECK_EQ(loaded.specials()[0].xp, ushariXp);
    }
}

SUITE("Level 1 — an early survey discovery waits for Aldren’s camp") {
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
    loaded.endDay();
    CHECK(loaded.scenario().choice().at("id") == "ushari_at_night");
    CHECK(!loaded.scenario().choose(loaded, "stand_by_aldren"));
    CHECK(!loaded.scenario().awaitingChoice());
    loaded.scenario().fire(loaded, "visit", {{"name", "Hallowmere"}});
    CHECK(!loaded.scenario().awaitingChoice());
    loaded.scenario().fire(loaded, "visit", {{"name", "Aldren's Camp"}});
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

SUITE("Level 1 — arrival response persists and cannot be changed") {
    AdventureSession s;
    CHECK(!s.start(kMap, "data", kEncounters, 1, kTriggers));
    for (const char* pack : {"Hill Wolves", "Den Wolves", "Hermit's Wolves"})
        s.scenario().fire(s, "encounter_won", {{"name", pack}});
    s.scenario().fire(s, "cleared");
    CHECK(!s.scenario().awaitingChoice());
    s.endDay();
    CHECK(s.scenario().awaitingChoice());
    AdventureSession loaded;
    CHECK(!loaded.loadState(s.saveState()));
    CHECK(loaded.scenario().choice().at("id") == "ushari_at_night");
    CHECK(loaded.scenario().choose(loaded, "invalid").has_value());
    CHECK(!loaded.scenario().choose(loaded, "dismiss_aldren"));
    CHECK_EQ(loaded.scenario().morale("ushari"), -2);
    CHECK(loaded.scenario().choose(loaded, "stand_by_aldren").has_value());
    AdventureSession restored;
    CHECK(!restored.loadState(loaded.saveState()));
    CHECK_EQ(restored.scenario().morale("ushari"), -2);
    restored.endDay();
    CHECK_EQ(restored.scenario().morale("ushari"), -2);
}

SUITE("Gate runes — Ushari translates remembered stones and readings survive saves") {
    AdventureSession s;
    CHECK(!s.start(kMap, "data", kEncounters, 1, kTriggers));
    const auto loreCount = s.scenario().lore().size();
    for (const char* stone : {"Varenhold Wall Rune Stone", "Coldwater Bridge Rune Stone", "Assembly Rune Stone"})
        s.scenario().fire(s, "visit", {{"name", stone}});
    CHECK_EQ(s.scenario().lore().size(), loreCount);
    AdventureSession loaded;
    CHECK(!loaded.loadState(Scenario::Json::parse(s.saveState().dump())));
    for (const char* pack : {"Hill Wolves", "Den Wolves", "Hermit's Wolves"})
        loaded.scenario().fire(loaded, "encounter_won", {{"name", pack}});
    loaded.scenario().fire(loaded, "cleared");
    loaded.endDay();
    int readings = 0;
    for (const auto& entry : loaded.scenario().lore())
        if (entry.title.find("Gate rune") == 0) ++readings;
    CHECK_EQ(readings, 3);
    CHECK(spoke(loaded.scenario().drainLines(), "Ushari"));
    AdventureSession restored;
    CHECK(!restored.loadState(Scenario::Json::parse(loaded.saveState().dump())));
    CHECK_EQ(restored.scenario().lore().size(), loaded.scenario().lore().size());
    restored.scenario().fire(restored, "visit", {{"name", "Varenhold Wall Rune Stone"}});
    CHECK_EQ(restored.scenario().lore().size(), loaded.scenario().lore().size());
}


SUITE("Rookwatch — northern castle, finite economy, construction and saved state") {
    AdventureSession s;
    CHECK(!s.start(kMap, "data", kEncounters, 1, kTriggers));
    const auto castle = cellOf(s, "Rookwatch");
    CHECK_EQ(s.owner(castle), 2);
    CHECK_EQ(s.rivals().size(), 1u);
    CHECK(s.rivals()[0].name == "Lord Edric Rook");
    CHECK(s.townRoster(castle) == "cruths");
    CHECK(s.town(castle)->buildings.count("castle"));
    CHECK(s.garrison(castle));
    CHECK(s.isEncounter(castle));
    for (const auto& st : s.rivals()[0].army) CHECK(s.resources().unit(st.id)->faction == "cruths");
    const auto initial = s.saveState();
    const auto buildings = s.town(castle)->buildings.size();
    const auto initialPool = s.town(castle)->recruitPool;
    s.endDay();
    CHECK_EQ(s.town(castle)->buildings.size(), buildings);
    CHECK(s.saveState()["rival_treasury"] == initial["rival_treasury"]);
    for (int n = 0; n < 10; ++n) s.endDay();
    CHECK_EQ(s.rivals().size(), 1u); // no free day-ten army
    CHECK(s.rivals()[0].pos.q >= 5); // opening stays in its own marches
    CHECK(!s.lost());
    CHECK(s.town(castle)->recruitPool == initialPool);
    AdventureSession loaded;
    CHECK(!loaded.loadState(s.saveState()));
    CHECK(loaded.saveState()["rival_treasury"] == s.saveState()["rival_treasury"]);
    CHECK(loaded.rivals()[0].pos == s.rivals()[0].pos);
    CHECK(loaded.town(castle)->buildings == s.town(castle)->buildings);
    CHECK(loaded.town(castle)->recruitPool == s.town(castle)->recruitPool);

    // With no money and no field army, a mobilized lord earns exactly owned income.
    auto poor = initial;
    poor["rival_treasury"] = Scenario::Json::array({0,0,0,0,0});
    poor["rivals"][0]["alive"] = false;
    CHECK(!loaded.loadState(poor));
    loaded.scenario().fire(loaded, "encounter_won", {{"name", "Hill Wolves"}});
    auto mobilized = loaded.saveState();
    mobilized["scenario"]["fired"].push_back("ushari_arrives");
    CHECK(!loaded.loadState(mobilized));
    loaded.scenario().fire(loaded, "visit", {{"name", "Hallowmere"}});
    CHECK(loaded.rivalMobilized());
    const auto income = loaded.dailyIncome(2);
    loaded.endDay();
    const auto after = loaded.saveState();
    for (int i = 0; i < RESOURCE_COUNT; ++i) CHECK_EQ(after["rival_treasury"][i].get<int>(), income.amounts[i]);
}

SUITE("Rookwatch — capture defeats the garrison and keeps a functioning northern town") {
    AdventureSession s;
    CHECK(!s.start(kMap, "data", kEncounters, 1, kTriggers));
    auto fixture = s.saveState();
    const auto castle = cellOf(s, "Rookwatch");
    fixture["rivals"][0]["alive"] = false;
    // Stage an approach on a real adjacent road, with no neutral guard interference.
    HexCoord approach = castle;
    for (int d = 0; d < 6; ++d) {
        const auto c = castle.neighbor(d);
        if (s.map().tileAt(c) && s.map().tileAt(c)->passable) { approach = c; break; }
    }
    fixture["hero"] = {approach.q, approach.r};
    fixture["encounters"] = Scenario::Json::array();
    CHECK(!s.loadState(fixture));
    const auto stock = s.town(castle)->recruitPool;
    s.travel(castle);
    CHECK(s.pendingEncounter().has_value());
    CHECK(!s.pendingIsAmbush());
    s.resolveEncounter(true);
    CHECK_EQ(s.owner(castle), 1);
    CHECK(!s.isEncounter(castle));
    CHECK(s.town(castle)->buildings.count("castle"));
    CHECK(s.town(castle)->recruitPool == stock); // no free stock on conquest
    CHECK(hasQuest(s, "rookwatch", true));
    CHECK(!s.recruit(castle, "woad_runner", 1));
    CHECK(s.scenario().outcome().empty()); // conquest is not the chapter ending
    AdventureSession loaded;
    CHECK(!loaded.loadState(s.saveState()));
    CHECK_EQ(loaded.owner(castle), 1);
    CHECK(!loaded.isEncounter(castle));
    CHECK(hasQuest(loaded, "rookwatch", true));
    loaded.scenario().fire(loaded, "visit", {{"name", "Hallowmere"}});
    bool steward = false;
    for (const auto& line : loaded.scenario().drainLines()) steward |= line.text.find("steward") != std::string::npos;
    CHECK(steward);
}

SUITE("Rookwatch — passage request persists then launches a finite, one-shot ambush") {
    AdventureSession s;
    CHECK(!s.start(kMap, "data", kEncounters, 1, kTriggers));
    auto fixture = s.saveState();
    fixture["scenario"]["fired"].push_back("ushari_arrives");
    CHECK(!s.loadState(fixture));
    s.scenario().fire(s, "visit", {{"name", "Hallowmere"}});
    CHECK(hasQuest(s, "rookwatch", false));
    CHECK(s.scenario().offer("rookwatch_supplies"));
    s.scenario().fire(s, "enter_q", {{"q", 5}});
    CHECK(s.scenario().choice().value("id", "") == "rook_passage");
    AdventureSession loaded;
    CHECK(!loaded.loadState(s.saveState()));
    const auto castle = cellOf(loaded, "Rookwatch");
    const auto held = *loaded.garrison(castle);
    CHECK(!loaded.scenario().choose(loaded, "ask_rook_passage"));
    CHECK(!loaded.pendingEncounter());
    CHECK_EQ(loaded.rivals().size(), 1u);
    auto meeting = loaded.saveState();
    const auto watch = cellOf(loaded, "Greyfang Watch");
    meeting["hero"] = {watch.q, watch.r};
    CHECK(!loaded.loadState(meeting));
    loaded.scenario().fire(loaded, "visit", {{"name", "Greyfang Watch"}});
    CHECK(loaded.pendingIsAmbush());
    CHECK(loaded.pendingEncounter().has_value());
    CHECK_EQ(loaded.rivals().size(), 2u);
    CHECK(loaded.rivals().back().name == "Rook's escort");
    for (size_t i = 0; i < held.size(); ++i)
        CHECK_EQ((*loaded.garrison(castle))[i].count, held[i].count);
    CHECK(loaded.scenario().choose(loaded, "ask_rook_passage").has_value());
    loaded.resolveEncounter(true);
    CHECK(!loaded.rivals().back().alive);
    CHECK(loaded.departureRoute() == "patrol_cut");
    CHECK(!loaded.ambushFrom("Greyfang Watch", "Duplicate escort"));
    CHECK(!loaded.won() && !loaded.lost());
    CHECK(!loaded.pendingEncounter());
    AdventureSession declined;
    CHECK(!declined.loadState(s.saveState()));
    CHECK(!declined.scenario().choose(declined, "decline_rook_passage"));
    CHECK(!declined.pendingEncounter());
    CHECK_EQ(declined.rivals().size(), 1u);
}

SUITE("Rookwatch — mobilized lord captures player towns without conjuring recruits") {
    AdventureSession s;
    CHECK(!s.start(kMap, "data", kEncounters, 1, kTriggers));
    auto fixture = s.saveState();
    fixture["encounters"] = Scenario::Json::array(); // isolate strategic movement from neutral battles
    fixture["scenario"]["fired"].push_back("ushari_arrives");
    CHECK(!s.loadState(fixture));
    s.setArmy({{"rider_knight", 1000}}); // keeps the test focused on territory rather than ambushes
    s.scenario().fire(s, "visit", {{"name", "Hallowmere"}});
    bool crossed = false;
    bool captured = false;
    for (int n = 0; n < 25; ++n) {
        s.endDay();
        crossed |= s.rivals()[0].pos.q < 5;
        captured |= s.owner(cellOf(s, "Hallowmere")) == 2;
    }
    CHECK(crossed);
    CHECK(captured);
    CHECK(s.townRoster(cellOf(s, "Hallowmere")) == "cruths");
    CHECK(!s.lost()); // no hidden race to seal the passage
}

SUITE("Demo retreat — neutral and combined castle casualties survive saves without awarding victory") {
    for (const std::string name : {"Hill Wolves", "Rookwatch"}) {
        AdventureSession s;
        CHECK(!s.start(kMap, "data", kEncounters, 1, kTriggers));
        const auto at = cellOf(s, name);
        auto fixture = s.saveState();
        HexCoord approach = at;
        for (int d = 0; d < 6; ++d) {
            auto c = at.neighbor(d);
            if (s.map().tileAt(c) && s.map().tileAt(c)->passable && !s.isEncounter(c)) { approach = c; break; }
        }
        fixture["hero"] = {approach.q, approach.r};
        fixture["encounters"] = name == "Rookwatch" ? Scenario::Json::array() : Scenario::Json::array({{at.q, at.r}});
        CHECK(!s.loadState(fixture));
        s.travel(at);
        CHECK(s.pendingEncounter() == at);
        const auto gold = s.treasury()[Resource::Gold];
        const auto xp = s.heroProgress().xp;
        auto original = Scenario::Json::parse(s.encounterAt(at)->guardsJson);
        std::vector<AdventureSession::Stack> survivors;
        int expected = 0;
        for (const auto& st : original) {
            const int count = st.at("count").get<int>() / 2;
            survivors.push_back({st.at("id"), count}); expected += count;
        }
        survivors.push_back({"ancient_guardian", 999}); // cannot introduce a new stack
        s.resolveEncounter(false, survivors);
        CHECK(!s.pendingEncounter());
        CHECK_EQ(s.treasury()[Resource::Gold], gold);
        CHECK_EQ(s.heroProgress().xp, xp);
        CHECK(s.isEncounter(at));
        AdventureSession loaded;
        CHECK(!loaded.loadState(s.saveState()));
        int actual = 0;
        for (const auto& st : Scenario::Json::parse(loaded.encounterAt(at)->guardsJson)) {
            CHECK(st.at("id") != "ancient_guardian");
            actual += st.at("count").get<int>();
        }
        CHECK_EQ(actual, expected);
        if (name == "Rookwatch") CHECK_EQ(loaded.owner(at), 2);
    }
}

SUITE("Survey — required readings and a secured approach cannot be bypassed by knowing the answer") {
    AdventureSession s;
    CHECK(!s.start(kMap, "data", kEncounters, 99, kTriggers));
    CHECK(s.passageMine() == cellOf(s, "Old Mine of Kaldur"));
    auto fixture = s.saveState();
    fixture["scenario"]["fired"].push_back("ushari_arrives");
    const auto mine = *s.passageMine();
    fixture["hero"] = {mine.q, mine.r};
    CHECK(!s.loadState(fixture));
    s.scenario().fire(s, "visit", {{"name", "Hallowmere"}});
    s.scenario().fire(s, "visit", {{"name", "Aldren's Camp"}});
    s.scenario().fire(s, "passage_found");
    CHECK(s.scenario().choose(s, "esh_varen_tal_eren_orun_ishara").has_value());
    for (const char* stone : {"Varenhold Wall Rune Stone", "Coldwater Bridge Rune Stone", "Assembly Rune Stone"})
        s.scenario().fire(s, "visit", {{"name", stone}});
    CHECK(s.scenario().choose(s, "esh_varen_tal_eren_orun_ishara").has_value());
    s.scenario().fire(s, "encounter_won", {{"name", "Troll Gate"}});
    CHECK(!s.scenario().choose(s, "esh_varen_tal_eren_orun_ishara"));
    CHECK(!s.scenario().choose(s, "follow_ushari"));
    CHECK(s.scenario().outcome().at("approach") == "southern_track");
    CHECK(s.scenario().outcome().at("supply_base") == "");
}
