// test_story_objectives.cpp — story objectives read from triggers and quests
// (what demo_bot's campaign mode pursues instead of a hand-written route).
#include "test_runner.h"
#include "adventure/AdventureSession.h"
#include "adventure/StoryObjectives.h"
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace {

using Json = nlohmann::json;

WorldMap objectiveMap() {
    WorldMap map;
    map.clear(6);
    for (const auto& [c, t] : map) map.setTile(c, makeTile(Terrain::Grass));
    map.placeObject({{-5, 0}, ObjType::Town, "Home", 1, ""});
    map.placeObject({{-3, 1}, ObjType::GoldMine, "Varen Mine", 0, ""});
    map.placeObject({{3, 0}, ObjType::Guard, "Hill Wolves", 0, ""});
    map.placeObject({{0, -5}, ObjType::Guard, "Den Wolves", 0, ""});
    map.placeObject({{-2, 4}, ObjType::QuestGiver, "Shrine", 0, ""});
    map.placeObject({{2, 2}, ObjType::Watchtower, "Old Tower", 0, ""});
    return map;
}

const Json kTriggers = Json::parse(R"({
  "start_companions": [],
  "quests": [{"id": "climb", "title": "The tower", "text": "Climb Old Tower before the snow."}],
  "triggers": [
    {"id": "intro",   "when": {"event": "start"}, "do": [{"quest": "climb"}]},
    {"id": "mine",    "when": {"event": "capture", "name": "Varen Mine"}, "do": []},
    {"id": "wolves",  "when": {"event": "cleared", "names": ["Hill Wolves", "Den Wolves"]}, "do": []},
    {"id": "shrine",  "when": {"event": "visit", "name": "Shrine", "after": "mine"}, "do": []},
    {"id": "mercy",   "when": {"event": "visit", "name": "Shrine", "flag": "mercy"}, "do": []},
    {"id": "tower",   "when": {"event": "visit", "name": "Old Tower", "unless": "mine"}, "do": []},
    {"id": "nowhere", "when": {"event": "visit", "name": "Nowhere"}, "do": []},
    {"id": "east",    "when": {"event": "enter_q", "q_min": 5, "after": "wolves"}, "do": []}
  ]})");

const StoryObjective* find(const std::vector<StoryObjective>& list, const std::string& name,
                           const std::string& trigger) {
    for (const auto& o : list)
        if (o.name == name && o.trigger == trigger) return &o;
    return nullptr;
}

} // namespace

SUITE("StoryObjectives — places come from the triggers and quests, armed by their story conditions") {
    const std::string path = (std::filesystem::temp_directory_path() / "umm_story_objectives.triggers.json").string();
    std::ofstream(path) << kTriggers.dump();
    AdventureSession s;
    CHECK(!s.start(objectiveMap(), "data", "", 1, path));

    auto list = storyObjectives(s);
    const auto* mine = find(list, "Varen Mine", "mine");
    CHECK(mine && mine->armed && mine->event == "capture" && mine->at == HexCoord(-3, 1));
    CHECK(find(list, "Hill Wolves", "wolves") && find(list, "Hill Wolves", "wolves")->armed);
    CHECK(find(list, "Den Wolves", "wolves") && find(list, "Den Wolves", "wolves")->event == "cleared");
    CHECK(find(list, "Shrine", "shrine") && !find(list, "Shrine", "shrine")->armed);   // after: mine
    CHECK(find(list, "Shrine", "mercy") && !find(list, "Shrine", "mercy")->armed);     // flag: mercy
    CHECK(find(list, "Old Tower", "tower") && find(list, "Old Tower", "tower")->armed);
    const auto* quest = find(list, "Old Tower", "");                                  // named in the quest text
    CHECK(quest && quest->event == "quest" && quest->armed);
    CHECK(!std::any_of(list.begin(), list.end(), [](const StoryObjective& o) { return o.name == "Nowhere"; }));
    CHECK(!find(list, "Home", "intro"));                                               // fired, and no place
    const auto* east = find(list, "the road east (5,0)", "east");                     // column q_min, open tiles
    CHECK(east && east->event == "enter_q" && !east->armed && east->at == HexCoord(5, 0));
    CHECK(std::all_of(list.begin(), list.end(), [](const StoryObjective& o) { return o.event != "enter_q" || o.at.q == 5; }));

    s.setFlag("mercy", true);
    list = storyObjectives(s);
    CHECK(find(list, "Shrine", "mercy") && find(list, "Shrine", "mercy")->armed);

    s.travel({-3, 1});                       // capture the mine: its trigger fires
    CHECK(s.owner({-3, 1}) == Faction::Player);
    list = storyObjectives(s);
    CHECK(!find(list, "Varen Mine", "mine"));
    CHECK(find(list, "Shrine", "shrine") && find(list, "Shrine", "shrine")->armed);    // its "after" fired
    CHECK(!find(list, "Old Tower", "tower"));                                          // overtaken (unless)
    CHECK(find(list, "Old Tower", ""));                                                // the quest is still open

    s.travel({3, 0});                        // beat one of the two packs
    CHECK(s.pendingEncounter() && *s.pendingEncounter() == HexCoord(3, 0));
    s.resolveEncounter(true);
    list = storyObjectives(s);
    CHECK(!find(list, "Hill Wolves", "wolves"));
    CHECK(find(list, "Den Wolves", "wolves"));                                         // still unbeaten

    for (int day = 0; day < 3 && !s.pendingEncounter(); ++day) { s.travel({0, -5}); if (!s.pendingEncounter()) s.endDay(); }
    CHECK(s.pendingEncounter() && *s.pendingEncounter() == HexCoord(0, -5));
    s.resolveEncounter(true);                // beat the other: the way east is armed
    list = storyObjectives(s);
    CHECK(find(list, "the road east (5,0)", "east") && find(list, "the road east (5,0)", "east")->armed);
}
