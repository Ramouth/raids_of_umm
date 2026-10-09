// test_party_limits.cpp — the hero's party (design rule 2026-10-09): at most five
// troop stacks and two riding companions; a third companion raises "Who stays
// behind?"; who companions are, what they cost and what they say comes from
// units.json; the story reaches the client as one ordered queue; story choice
// validation; an engage choice holds its fight; the battle's XP after the cap.
#include "test_runner.h"
#include "adventure/AdventureSession.h"
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace {

using Json = nlohmann::json;
namespace fs = std::filesystem;

std::string tempPath(const std::string& name) {
    return (fs::temp_directory_path() / ("umm_party_" + name)).string();
}

std::string writeTriggers(const std::string& name, const Json& j) {
    const std::string path = tempPath(name + ".triggers.json");
    std::ofstream(path) << j.dump();
    return path;
}

// The real data, with the companion fields the story data is to carry
// (lines, upkeep) patched in, so these rules do not wait on that data.
std::string companionData() {
    const fs::path dir = fs::temp_directory_path() / "umm_party_data";
    fs::create_directories(dir);
    for (const auto& entry : fs::directory_iterator("data"))
        if (entry.is_regular_file())
            fs::copy_file(entry.path(), dir / entry.path().filename(), fs::copy_options::overwrite_existing);
    Json units = Json::parse(std::ifstream("data/units.json"));
    for (auto& u : units["units"]) {
        const std::string id = u.value("id", "");
        if (id == "drest" || id == "aldren") u["upkeep"] = 0;
        if (id == "kharim") {
            u["upkeep"] = 100;
            u["lines"] = {{"join", "Test: Kharim joins."}, {"unpaid", "Test: Kharim unpaid."},
                          {"leaves", "Test: Kharim leaves."}};
        }
    }
    std::ofstream(dir / "units.json") << units.dump();
    return dir.string();
}

// Grass; a player town at (-5, 0) unless townless; wolves at (3, 0).
std::string partyMap(bool town, const std::string& name) {
    WorldMap map;
    map.clear(6);
    for (const auto& [c, t] : map) map.setTile(c, makeTile(Terrain::Grass));
    if (town) map.placeObject({{-5, 0}, ObjType::Town, "Home", 1, ""});
    map.placeObject({{3, 0}, ObjType::Guard, "Hill Wolves", 0, ""});
    map.setStart(HexCoord{0, 0});
    const std::string path = tempPath(name + ".json");
    CHECK(!map.saveJson(path));
    return path;
}

bool rides(const AdventureSession& s, const std::string& id) {
    return std::any_of(s.specials().begin(), s.specials().end(),
                       [&](const auto& sc) { return sc.id == id && !sc.stationed; });
}

std::vector<std::string> optionIds(const Json& choice) {
    std::vector<std::string> out;
    for (const auto& o : choice.value("options", Json::array())) out.push_back(o.value("id", ""));
    return out;
}

} // namespace

SUITE("Party — five stacks at most: a sixth kind is left out (logged), the same kind merges") {
    const std::string mapPath = partyMap(true, "five");
    AdventureSession s;
    CHECK(!s.start(mapPath, "data"));
    auto left = s.setArmy({{"woad_runner", 5}, {"cruth_slinger", 4}, {"levy_spearman", 3}, {"brigand", 2},
                           {"feral_dog", 2}, {"grey_wolf", 7}, {"woad_runner", 5}});
    CHECK_EQ((int)s.army().size(), 5);
    CHECK(left.size() == 1 && left[0].id == "grey_wolf" && left[0].count == 7);
    CHECK_EQ(s.army()[0].count, 10);                       // the second woad stack merged into the first
    CHECK(s.setArmy(s.army()).empty());
}

SUITE("Party — recruiting a sixth kind of soldier is refused; more of a riding kind is not") {
    WorldMap map;
    map.clear(8);
    map.placeObject({{-6, 0}, ObjType::Town, "Home", 1});
    AdventureSession s;
    CHECK(!s.start(std::move(map), "data"));
    ResourcePool plenty;
    plenty[Resource::Gold] = 100000;
    s.give(plenty);
    s.setArmy({{"cruth_slinger", 4}, {"levy_spearman", 3}, {"brigand", 2}, {"feral_dog", 2}, {"grey_wolf", 7}});
    auto err = s.recruit({-6, 0}, "woad_runner", 1);
    CHECK(err.has_value() && err->find("free slot") != std::string::npos);
    s.setArmy({{"woad_runner", 1}, {"levy_spearman", 3}, {"brigand", 2}, {"feral_dog", 2}, {"grey_wolf", 7}});
    CHECK(!s.recruit({-6, 0}, "woad_runner", 1));
    CHECK_EQ(s.army()[0].count, 2);
}

SUITE("Party — the troops action cannot add a sixth stack; the narrator says who stays behind") {
    const std::string mapPath = partyMap(true, "troops");
    const std::string trig = writeTriggers("troops", Json::parse(R"({"start_companions": [],
        "start_army": [{"id": "woad_runner", "count": 5}, {"id": "cruth_slinger", "count": 4},
                       {"id": "levy_spearman", "count": 3}, {"id": "brigand", "count": 2}, {"id": "feral_dog", "count": 2}],
        "triggers": [{"id": "help", "when": {"event": "start"},
                      "do": [{"troops": [{"id": "grey_wolf", "count": 6}, {"id": "brigand", "count": 1}]}]}]})"));
    AdventureSession s;
    CHECK(!s.start(mapPath, "data", "", 1, trig));
    CHECK_EQ((int)s.army().size(), 5);
    int brigands = 0;
    for (const auto& st : s.army()) if (st.id == "brigand") brigands = st.count;
    CHECK_EQ(brigands, 3);
    bool told = false;
    for (const auto& l : s.scenario().drainLines())
        told |= l.speaker.empty() && l.text.find("no room in the column for 6") != std::string::npos;
    CHECK(told);
}

SUITE("Party — a third companion: who stays behind? Essential ones are not offered; the one left governs the nearest town") {
    const std::string data = companionData();
    const std::string mapPath = partyMap(true, "third");
    const std::string trig = writeTriggers("third", Json::parse(R"({"start_companions": ["drest", "aldren"],
        "essential_companions": ["drest"], "triggers": []})"));
    AdventureSession s;
    CHECK(!s.start(mapPath, data, "", 1, trig));
    CHECK(!s.scenario().awaitingChoice());
    s.scenario().drainStory();
    s.joinSpecial("kharim");
    const Json choice = s.scenario().offeredChoice(s);
    CHECK(choice.value("id", "") == "stay_behind" && choice.value("kind", "") == "party");
    CHECK(choice.value("title", "") == "Who stays behind?" && choice.value("newcomer", "") == "kharim");
    CHECK(optionIds(choice) == (std::vector<std::string>{"aldren", "kharim"}));   // Drest is essential
    CHECK(s.scenario().blocksAdventure());
    CHECK_EQ(s.ridingCount(), 3);
    CHECK_EQ((int)s.battleCompanions().size(), 2);        // battles field two even before the answer
    CHECK(!s.scenario().choose(s, "aldren"));
    CHECK_EQ(s.ridingCount(), 2);
    CHECK(rides(s, "drest") && rides(s, "kharim") && !rides(s, "aldren"));
    const auto* aldren = &*std::find_if(s.specials().begin(), s.specials().end(), [](const auto& sc) { return sc.id == "aldren"; });
    CHECK(aldren->stationed && *aldren->stationed == HexCoord(-5, 0));
    Json party;
    for (const auto& e : s.scenario().drainStory()) if (e.value("kind", "") == "party") party = e;
    CHECK(party.value("id", "") == "aldren" && party.value("where", "") == "Home"
          && party.value("text", "") == "Aldren stays behind to govern Home.");
    CHECK(!s.scenario().awaitingChoice() && !s.scenario().blocksAdventure());

    // Recalled only when there is room: two already ride.
    s.travel({-5, 0});
    CHECK(s.heroPos() == HexCoord(-5, 0));
    auto refused = s.station("aldren", false);
    CHECK(refused.has_value() && refused->find("Two companions") != std::string::npos);
    CHECK(!s.station("kharim", true));                    // leave Kharim here instead
    CHECK(!s.station("aldren", false));
    CHECK(rides(s, "aldren") && !rides(s, "kharim"));
}

SUITE("Party — with no town held, the one who stays behind waits; saved, carried and brought back with her progress") {
    const std::string data = companionData();
    const std::string mapPath = partyMap(false, "waiting");
    const std::string trig = writeTriggers("waiting", Json::parse(R"({"start_companions": ["drest", "aldren"],
        "triggers": []})"));
    AdventureSession s;
    CHECK(!s.start(mapPath, data, "", 1, trig));
    s.joinSpecial("kharim");
    CHECK(optionIds(s.scenario().offeredChoice(s)) == (std::vector<std::string>{"drest", "aldren", "kharim"}));
    s.grantXp(120);
    CHECK(!s.scenario().choose(s, "kharim"));
    CHECK(!rides(s, "kharim") && s.ridingCount() == 2 && s.specials().size() == 2);
    CHECK(s.departedSpecials().size() == 1 && s.departedSpecials()[0].waiting && s.departedSpecials()[0].xp == 120);
    bool told = false;
    for (const auto& e : s.scenario().drainStory())
        told |= e.value("kind", "") == "party" && e.value("where", "x").empty()
                && e.value("text", "") == "Kharim stays behind and will wait for word from you.";
    CHECK(told);

    AdventureSession loaded;
    CHECK(!loaded.loadState(Json::parse(s.saveState().dump())));
    CHECK(loaded.departedSpecials().size() == 1 && loaded.departedSpecials()[0].waiting);
    CHECK_EQ(loaded.ridingCount(), 2);

    const CampaignState out = s.carryOut();
    CHECK(out.companions.size() == 2 && out.departed.size() == 1 && out.departed[0].waiting);
    CHECK(CampaignState::fromJson(out.toJson()).departed[0].waiting);

    s.leaveSpecial("aldren");                              // room again: the story sends for him
    s.joinSpecial("kharim");
    CHECK(rides(s, "kharim") && !s.scenario().awaitingChoice());
    CHECK_EQ(std::find_if(s.specials().begin(), s.specials().end(), [](const auto& sc) { return sc.id == "kharim"; })->xp, 120);
}

SUITE("Party — a carried company of three: the third waits; governors ride on only while there is room") {
    const std::string mapPath = partyMap(true, "carry");
    CampaignState carry;
    carry.carried = true;
    carry.army = {{"woad_runner", 5}};
    carry.companions = {{"drest", "Drest", "", 2, 150, 0}, {"aldren", "Aldren", "", 1, 0, 0}, {"kharim", "Kharim", "", 1, 0, 0}};
    AdventureSession s;
    CHECK(!s.start(mapPath, "data", "", 1, writeTriggers("carry", Json::parse(R"({"triggers": []})")), carry));
    CHECK_EQ(s.ridingCount(), 2);
    CHECK(s.departedSpecials().size() == 1 && s.departedSpecials()[0].id == "kharim" && s.departedSpecials()[0].waiting);

    s.travel({-5, 0});
    CHECK(!s.station("drest", true));
    s.joinSpecial("kharim");                               // drest governs: two ride, no choice
    CHECK(!s.scenario().awaitingChoice() && s.ridingCount() == 2);
    const CampaignState out = s.carryOut();
    CHECK(out.companions.size() == 2 && out.departed.size() == 1 && out.departed[0].id == "drest" && out.departed[0].waiting);
}

SUITE("Party — the party choice waits behind an open story choice, and opens after it") {
    const std::string data = companionData();
    const std::string mapPath = partyMap(true, "queue");
    const std::string trig = writeTriggers("queue", Json::parse(R"({"start_companions": ["drest", "aldren"],
        "triggers": [{"id": "meet", "when": {"event": "start"}, "do": [
            {"choice": {"id": "oath", "options": [{"id": "swear", "then": []}]}},
            {"join": "kharim"}]}]})"));
    AdventureSession s;
    CHECK(!s.start(mapPath, data, "", 1, trig));
    CHECK(s.scenario().choice().value("id", "") == "oath");
    AdventureSession loaded;                               // the queued choice survives a save
    CHECK(!loaded.loadState(Json::parse(s.saveState().dump())));
    for (AdventureSession* x : {&s, &loaded}) {
        CHECK(!x->scenario().choose(*x, "swear"));
        CHECK(x->scenario().choice().value("id", "") == "stay_behind");
    }
}

SUITE("Companions from data — wage 0 never sulks or leaves; lines come from units.json; level-ups are narrated") {
    const std::string data = companionData();
    const std::string mapPath = partyMap(false, "wages");
    const std::string trig = writeTriggers("wages", Json::parse(R"({"start_companions": ["drest"], "triggers": []})"));
    AdventureSession s;
    CHECK(!s.start(mapPath, data, "", 1, trig));
    s.joinSpecial("kharim");
    bool joined = false;
    for (const auto& l : s.scenario().drainLines()) joined |= l.speaker == "Kharim" && l.text == "Test: Kharim joins.";
    CHECK(joined);
    const auto& drest = s.specials()[0];
    CHECK_EQ(s.upkeepOf(drest), 0);
    CHECK_EQ(s.upkeepPerDay(), AdventureSession::upkeepFor(1));   // Kharim's wage only
    ResourcePool gold;
    gold[Resource::Gold] = -s.treasury()[Resource::Gold];
    s.give(gold);                                          // an empty treasury
    std::vector<std::string> said;
    for (int d = 0; d < 8; ++d) {
        s.endDay();
        for (const auto& l : s.scenario().drainLines()) said.push_back(l.speaker + "|" + l.text);
    }
    CHECK(std::find(said.begin(), said.end(), "Kharim|Test: Kharim unpaid.") != said.end());
    CHECK(std::find(said.begin(), said.end(), "Kharim|Test: Kharim leaves.") != said.end());
    CHECK(std::none_of(said.begin(), said.end(), [](const std::string& l) { return l.rfind("Drest|", 0) == 0; }));
    CHECK(s.specials().size() == 1 && s.specials()[0].id == "drest" && s.specials()[0].unpaidDays == 0);

    s.scenario().drainStory();
    s.grantXp(AdventureSession::companionXpForLevel("drest", 2));
    bool narrated = false;
    for (const auto& l : s.scenario().drainLines())
        narrated |= l.speaker.empty() && l.text.rfind("Drest reaches level 2.", 0) == 0;
    CHECK(narrated);
}

SUITE("Companions from data — without lines in units.json: silent join, neutral wage lines") {
    AdventureSession s;
    CHECK(!s.start(partyMap(false, "defaults"), "data", "", 1,
                   writeTriggers("defaults", Json::parse(R"({"start_companions": ["drest"], "triggers": []})"))));
    CHECK_EQ(s.companionLine("drest", "join"), std::string{});
    CHECK(!s.companionLine("drest", "unpaid").empty() && !s.companionLine("drest", "leaves").empty());
    for (const auto& l : s.scenario().drainLines()) CHECK(l.text.find("Compact") == std::string::npos);
}

SUITE("Story queue — say, scene, lore, item in the order the actions ran; pending events survive a save") {
    const std::string mapPath = partyMap(true, "story");
    const std::string trig = writeTriggers("story", Json::parse(R"({"start_companions": [], "triggers": [
        {"id": "beat", "when": {"event": "day", "day": 2}, "do": [
            {"say": [["Drest", "Look there."]]},
            {"scene": {"frames": [{"scene": "dog_tag", "text": "A tag in the snow."}]}},
            {"say": [["Drest", "Keep it."]]},
            {"lore": ["Tags", "Northern dogs wear them."]},
            {"item": "hermits_seed"}]}]})"));
    AdventureSession s;
    CHECK(!s.start(mapPath, "data", "", 1, trig));
    s.scenario().drainStory();
    s.endDay();
    AdventureSession loaded;
    CHECK(!loaded.loadState(Json::parse(s.saveState().dump())));
    for (AdventureSession* x : {&s, &loaded}) {
        std::vector<std::string> kinds;
        auto story = x->scenario().drainStory();
        for (const auto& e : story) kinds.push_back(e.value("kind", ""));
        CHECK(kinds == (std::vector<std::string>{"line", "scene", "line", "lore", "item"}));
        if (story.size() == 5) {
            CHECK(story[1]["frames"][0]["text"] == "A tag in the snow." && story[2]["text"] == "Keep it.");
            CHECK(story[4]["id"] == "hermits_seed" && !story[4].value("name", "").empty());
        }
        CHECK(x->scenario().drainStory().empty());
    }
    // drainLines / drainScenes take only their own kind and leave the rest in order.
    s.scenario().say("A", "one");
    s.scenario().pushStory({{"kind", "scene"}, {"frames", Json::array({{{"text", "x"}}})}});
    s.scenario().say("B", "two");
    CHECK_EQ((int)s.scenario().drainLines().size(), 2);
    CHECK_EQ((int)s.scenario().drainScenes().size(), 1);
    CHECK(s.scenario().drainStory().empty());
}

SUITE("Story choice validation — a blocking choice whose every option is conditional fails the load") {
    Scenario sc;
    auto err = sc.load(writeTriggers("allif", Json::parse(R"({"triggers": [{"id": "t", "when": {"event": "start"},
        "do": [{"choice": {"id": "c", "options": [
            {"id": "a", "if": {"flag": "x"}, "then": []},
            {"id": "b", "requires_item": "dog_tag", "then": []}]}}]}]})")));
    CHECK(err.has_value() && err->find("every option is conditional") != std::string::npos);
    Scenario nested;                                       // in an if-branch too
    CHECK(nested.load(writeTriggers("allif_nested", Json::parse(R"({"triggers": [{"id": "t", "when": {"event": "start"},
        "do": [{"if": {"flag": "y"}, "then": [{"choice": {"id": "c", "options": [
            {"id": "a", "if": {"flag": "x"}, "then": []}]}}]}]}]})"))).has_value());
    Scenario fallback;                                     // one unconditional option is enough
    CHECK(!fallback.load(writeTriggers("fallback", Json::parse(R"({"triggers": [{"id": "t", "when": {"event": "start"},
        "do": [{"choice": {"id": "c", "options": [
            {"id": "a", "if": {"flag": "x"}, "then": []}, {"id": "b", "then": []}]}}]}]})"))));
    Scenario roaming;                                      // a choice that lets the map go on may hide all
    CHECK(!roaming.load(writeTriggers("roaming", Json::parse(R"({"triggers": [{"id": "t", "when": {"event": "start"},
        "do": [{"choice": {"id": "c", "allow_exploration": true, "options": [
            {"id": "a", "if": {"flag": "x"}, "then": []}]}}]}]})"))));
}

SUITE("Encounters — resolveEncounter refuses while the engage choice is open") {
    const std::string mapPath = partyMap(true, "engage");
    const std::string trig = writeTriggers("engage", Json::parse(R"({"start_companions": [],
        "start_army": [{"id": "woad_runner", "count": 30}], "triggers": [
        {"id": "meet", "when": {"event": "engage", "name": "Hill Wolves"}, "do": [{"choice": {"id": "pack", "options": [
            {"id": "fight", "then": []}, {"id": "spare", "then": [{"withdraw": "Hill Wolves"}]}]}}]}]})"));
    AdventureSession s;
    CHECK(!s.start(mapPath, "data", "", 1, trig));
    s.travel({3, 0});
    CHECK(s.encounterAwaitsChoice());
    const int gold = s.treasury()[Resource::Gold];
    s.resolveEncounter(true);
    CHECK(s.pendingEncounter().has_value() && s.isEncounter({3, 0}));   // refused: nothing changed
    CHECK_EQ(s.treasury()[Resource::Gold], gold);
    CHECK(!s.scenario().choose(s, "fight"));
    s.resolveEncounter(true);
    CHECK(!s.pendingEncounter() && !s.isEncounter({3, 0}));
}

SUITE("Spoils — xp is what the commander actually gained, after the level cap") {
    const std::string mapPath = partyMap(true, "cap");
    const std::string trig = writeTriggers("cap", Json::parse(R"({"start_companions": [],
        "start_army": [{"id": "woad_runner", "count": 30}], "triggers": []})"));
    CampaignState carry;
    carry.levelCap = 2;
    AdventureSession s;
    CHECK(!s.start(mapPath, "data", "", 1, trig, carry));
    s.grantXp(AdventureSession::xpForLevel(2) - 5);        // five short of the cap
    s.travel({3, 0});
    CHECK(s.pendingEncounter().has_value());
    const int estimate = s.encounterXp(*s.pendingEncounter());
    CHECK(estimate > 5);
    s.resolveEncounter(true);
    CHECK_EQ(s.lastSpoils().xp, 5);
    CHECK_EQ(s.heroProgress().xp, AdventureSession::xpForLevel(2));
    s.resolveEncounter(false);
    CHECK_EQ(s.lastSpoils().xp, 0);
}
