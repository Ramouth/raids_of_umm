// test_conditions.cpp — the story's condition trees (Condition.h): every leaf,
// the combinators, validation at load, and the places a story decides with
// them (trigger "if", option "if" / "requires", the if/then/else action), plus
// the legacy fields translated into the same trees.
#include "test_runner.h"
#include "adventure/AdventureSession.h"
#include "adventure/Condition.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace {

using Json = nlohmann::json;

std::string tempPath(const std::string& name) {
    return (std::filesystem::temp_directory_path() / ("umm_conditions_" + name)).string();
}

std::string writeTriggers(const std::string& name, const Json& j) {
    const std::string path = tempPath(name + ".triggers.json");
    std::ofstream(path) << j.dump();
    return path;
}

// A grass map with a pack of wolves east of the start and a shrine to the south-west.
std::string fieldMap() {
    WorldMap map;
    map.clear(6);
    for (const auto& [c, t] : map) map.setTile(c, makeTile(Terrain::Grass));
    map.placeObject({{3, 0}, ObjType::Guard, "Hill Wolves", 0, ""});
    map.placeObject({{-3, 3}, ObjType::QuestGiver, "Shrine", 0, ""});
    map.setStart(HexCoord{0, 0});
    const std::string path = tempPath("field.json");
    map.saveJson(path);
    return path;
}

bool holds(const Json& c, const AdventureSession& s) { return Condition::eval(c, s); }

bool throwsInvalid(const std::function<void()>& f) {
    try { f(); } catch (const std::invalid_argument&) { return true; }
    return false;
}

// A session on the field map; 'triggers' is the triggers file's content.
void begin(AdventureSession& s, const std::string& name, const Json& triggers) {
    CHECK(!s.start(fieldMap(), "data", "", 1, writeTriggers(name, triggers)));
}

std::vector<std::string> optionIds(const Json& choice) {
    std::vector<std::string> out;
    for (const auto& o : choice.value("options", Json::array())) out.push_back(o.value("id", ""));
    return out;
}

} // namespace

SUITE("Condition — flag leaves: set and truthy, or equal to a value") {
    const Json flags = {{"spared", true}, {"zero", 0}, {"empty", ""}, {"choice.warrior", "give_tag"}};
    CHECK(Condition::evalFlags(Json{{"flag", "spared"}}, flags));
    CHECK(!Condition::evalFlags(Json{{"flag", "zero"}}, flags));            // 0 is not truthy
    CHECK(!Condition::evalFlags(Json{{"flag", "empty"}}, flags));
    CHECK(!Condition::evalFlags(Json{{"flag", "missing"}}, flags));
    CHECK(Condition::evalFlags(Json{{"flag", "zero"}, {"is", 0}}, flags));   // but it can be matched
    CHECK(Condition::evalFlags(Json{{"flag", "choice.warrior"}, {"is", "give_tag"}}, flags));
    CHECK(!Condition::evalFlags(Json{{"flag", "choice.warrior"}, {"is", "fight"}}, flags));
    CHECK(!Condition::evalFlags(Json{{"flag", "missing"}, {"is", nullptr}}, flags));   // absent is not null
    CHECK(throwsInvalid([&] { Condition::evalFlags(Json{{"item", "x"}}, flags); }));   // flags alone cannot answer
}

SUITE("Condition — combinators, nested, and the constants") {
    const Json flags = {{"a", true}, {"b", false}};
    const Json a = {{"flag", "a"}}, b = {{"flag", "b"}};
    CHECK(Condition::evalFlags(true, flags) && !Condition::evalFlags(false, flags));
    CHECK(Condition::evalFlags(Json{{"all", Json::array()}}, flags));           // empty all holds
    CHECK(!Condition::evalFlags(Json{{"any", Json::array()}}, flags));          // empty any does not
    CHECK(!Condition::evalFlags(Json{{"all", {a, b}}}, flags));
    CHECK(Condition::evalFlags(Json{{"any", {a, b}}}, flags));
    CHECK(Condition::evalFlags(Json{{"not", b}}, flags));
    CHECK(Condition::evalFlags(Json{{"all", {a, {{"not", b}}, {{"any", {b, {{"not", {{"not", a}}}}}}}}}}, flags));
    CHECK(!Condition::evalFlags(Json{{"any", {b, {{"all", {a, b}}}}}}, flags));
}

SUITE("Condition — session leaves: item, companion, quest, cleared, lore, level, day, resource, route") {
    AdventureSession s;
    begin(s, "leaves", Json::parse(R"({"start_companions": [], "quests": [
        {"id": "hunt", "title": "The hunt", "text": "Drive off the wolves."},
        {"id": "pray", "title": "Pray", "text": "At the shrine."}],
        "triggers": [{"id": "intro", "when": {"event": "start"}, "do": [{"quest": "hunt"}, {"quest": "pray"}, {"quest_done": "pray"}]}]})"));

    CHECK(!holds({{"item", "scarab_amulet"}}, s));
    s.addItem("scarab_amulet");
    CHECK(holds({{"item", "scarab_amulet"}}, s));

    CHECK(!holds({{"companion", "ushari"}}, s));
    s.joinSpecial("ushari");
    CHECK(holds({{"companion", "ushari"}}, s));
    CHECK(holds({{"companion", "ushari"}, {"present", true}}, s));
    s.companionsFell({"ushari"}, false);                 // wounded: with you, but not present
    CHECK(holds({{"companion", "ushari"}}, s));
    CHECK(!holds({{"companion", "ushari"}, {"present", true}}, s));

    CHECK(holds({{"quest", "hunt"}, {"state", "active"}}, s));
    CHECK(!holds({{"quest", "hunt"}, {"state", "done"}}, s));
    CHECK(holds({{"quest", "pray"}, {"state", "done"}}, s));
    CHECK(!holds({{"quest", "pray"}, {"state", "active"}}, s));
    CHECK(!holds({{"quest", "never"}, {"state", "active"}}, s));   // not given: neither

    CHECK(!holds({{"cleared", "Hill Wolves"}}, s));
    s.scenario().run(s, Json::parse(R"([{"withdraw": "Hill Wolves"}])"));
    CHECK(holds({{"cleared", "Hill Wolves"}}, s));

    CHECK(!holds({{"lore", "The Ford"}}, s));
    s.scenario().run(s, Json::parse(R"([{"lore": ["The Ford", "Cold water."]}])"));
    CHECK(holds({{"lore", "The Ford"}}, s));

    CHECK(holds({{"level", {{"==", 1}}}}, s) && !holds({{"level", {{">=", 2}}}}, s));
    s.grantXp(AdventureSession::xpForLevel(2));
    CHECK(holds({{"level", {{">=", 2}}}}, s));

    const int day = s.day();
    CHECK(holds({{"day", {{"==", day}}}}, s) && !holds({{"day", {{">", day}}}}, s));
    s.endDay();
    CHECK(holds({{"day", {{">", day}}}}, s) && holds({{"day", {{"<=", day + 1}}}}, s));

    const int gold = s.treasury()[Resource::Gold];
    CHECK(holds({{"resource", "Gold"}, {">=", gold}}, s));
    CHECK(!holds({{"resource", "Gold"}, {">", gold}}, s));
    CHECK(holds({{"resource", "Gold"}, {"<", gold + 1}}, s));

    CHECK(!holds({{"route", true}}, s));                 // no way onward secured on this map
}

SUITE("Condition — trigger_fired, and how many days ago") {
    AdventureSession s;
    begin(s, "fired", Json::parse(R"({"start_companions": [], "triggers": [
        {"id": "intro", "when": {"event": "start"}, "do": []},
        {"id": "later", "when": {"event": "visit", "name": "Shrine"}, "do": []}]})"));
    CHECK(holds({{"trigger_fired", "intro"}}, s));
    CHECK(!holds({{"trigger_fired", "later"}}, s));
    CHECK(!holds({{"trigger_fired", "intro"}, {"days", {{">=", 2}}}}, s));
    s.endDay();
    s.endDay();
    CHECK(holds({{"trigger_fired", "intro"}, {"days", {{">=", 2}}}}, s));
}

SUITE("Condition — malformed trees are named, with their path") {
    auto bad = [](const Json& c) { return Condition::validate(c).has_value(); };
    CHECK(!bad(Json{{"all", {{{"flag", "a"}}, {{"not", {{"item", "x"}}}}}}}));
    CHECK(bad(Json{{"flg", "a"}}));                                  // unknown leaf
    CHECK(bad(Json{{"flag", "a"}, {"was", 1}}));                     // unknown key beside a leaf
    CHECK(bad(Json{{"flag", "a"}, {"item", "x"}}));                  // two leaves in one object
    CHECK(bad(Json{{"all", {{"flag", "a"}}}}));                     // all wants a list
    CHECK(bad(Json{{"all", Json::array()}, {"flag", "a"}}));         // combinators stand alone
    CHECK(bad(Json::object()));
    CHECK(bad(Json("a")));
    CHECK(bad(Json{{"flag", ""}}));
    CHECK(bad(Json{{"level", 3}}));                                  // a comparison is an object
    CHECK(bad(Json{{"level", {{"~", 3}}}}));
    CHECK(bad(Json{{"level", {{">=", 3}, {"<", 5}}}}));
    CHECK(bad(Json{{"day", {{">=", "5"}}}}));
    CHECK(bad(Json{{"quest", "hunt"}}));                             // state is required
    CHECK(bad(Json{{"quest", "hunt"}, {"state", "open"}}));
    CHECK(bad(Json{{"resource", "Mana"}, {">=", 1}}));
    CHECK(bad(Json{{"resource", "Gold"}}));
    CHECK(bad(Json{{"companion", "ushari"}, {"present", "yes"}}));
    CHECK(bad(Json{{"route", false}}));
    const auto err = Condition::validate(Json{{"any", {{{"flag", "a"}}, {{"flg", "b"}}}}}, "here");
    CHECK(err && err->find("here.any[1]") != std::string::npos && err->find("flg") != std::string::npos);
}

SUITE("Scenario — a malformed condition anywhere in the file fails the load") {
    auto loads = [](const std::string& name, const char* text) {
        AdventureSession s;
        return !s.start(fieldMap(), "data", "", 1, writeTriggers(name, Json::parse(text))).has_value();
    };
    CHECK(loads("ok", R"({"triggers": [{"id": "t", "when": {"event": "start"}, "if": {"flag": "a"}, "do": [
        {"if": {"item": "x"}, "then": [{"choice": {"id": "c", "options": [{"id": "o", "if": {"lore": "L"}, "requires": {"day": {">=": 2}}},
                                                                          {"id": "plain"}]}}]}]}]})"));   // a blocking choice keeps one unconditional option
    CHECK(!loads("trigger_if", R"({"triggers": [{"id": "t", "when": {"event": "start"}, "if": {"flagg": "a"}, "do": []}]})"));
    CHECK(!loads("option_if", R"({"triggers": [{"id": "t", "when": {"event": "start"}, "do": [
        {"choice": {"id": "c", "options": [{"id": "o", "if": {"item": "x", "count": 2}}]}}]}]})"));
    CHECK(!loads("option_requires", R"({"triggers": [{"id": "t", "when": {"event": "start"}, "do": [
        {"choice": {"id": "c", "options": [{"id": "o", "requires": {"not": [{"flag": "a"}]}}]}}]}]})"));
    CHECK(!loads("nested_action", R"({"triggers": [{"id": "t", "when": {"event": "start"}, "do": [
        {"if": true, "then": [{"if": {"day": 3}, "then": []}]}]}]})"));
    CHECK(!loads("if_with_say", R"({"triggers": [{"id": "t", "when": {"event": "start"}, "do": [
        {"if": true, "then": [], "say": [["A", "b"]]}]}]})"));
    CHECK(!loads("legacy_flag", R"({"triggers": [{"id": "t", "when": {"event": "start", "flag": 3}, "do": []}]})"));
    CHECK(!loads("offer", R"({"offers": [{"id": "o", "at": "Shrine", "then": [{"if": {"gold": 3}, "then": []}]}], "triggers": []})"));

    AdventureSession s;
    const auto err = s.start(fieldMap(), "data", "", 1, writeTriggers("named", Json::parse(
        R"({"triggers": [{"id": "meet", "when": {"event": "start"}, "if": {"all": [{"flag": "a"}, {"flg": "b"}]}, "do": []}]})")));
    CHECK(err && err->find("'meet'") != std::string::npos && err->find("all[1]") != std::string::npos);
}

SUITE("Scenario — an option's if hides it; requires shows it locked") {
    AdventureSession s;
    begin(s, "options", Json::parse(R"({"start_companions": [], "triggers": [
        {"id": "warrior", "when": {"event": "engage", "name": "Hill Wolves"}, "do": [{"choice": {"id": "warrior", "options": [
            {"id": "fight", "label": "Fight", "then": []},
            {"id": "give_tag", "label": "Hand him the tag", "if": {"item": "scarab_amulet"}, "then": [{"withdraw": "Hill Wolves"}]},
            {"id": "with_ushari", "if": {"all": [{"companion": "ushari"}, {"flag": "spared", "is": true}]}, "then": []},
            {"id": "pay", "requires": {"resource": "Gold", ">=": 999999}, "locked_text": "Not enough gold.", "then": []}]}}]}]})"));
    s.travel({3, 0});
    CHECK(s.scenario().awaitingChoice());
    CHECK(optionIds(s.scenario().offeredChoice(s)) == std::vector<std::string>({"fight", "pay"}));
    CHECK(s.scenario().choose(s, "give_tag").has_value());         // hidden: refused
    CHECK(s.scenario().choose(s, "with_ushari").has_value());
    const auto why = s.scenario().choose(s, "pay");
    CHECK(why && *why == "Not enough gold.");                       // shown, but locked
    const Json options = s.scenario().choice()["options"];
    CHECK(Scenario::locked(s, options[3]).has_value() && !Scenario::locked(s, options[0]));

    s.addItem("scarab_amulet");
    s.joinSpecial("ushari");
    s.setFlag("spared", true);
    CHECK(optionIds(s.scenario().offeredChoice(s)) == std::vector<std::string>({"fight", "give_tag", "with_ushari", "pay"}));
    CHECK(!s.scenario().choose(s, "give_tag"));
    CHECK(!s.pendingEncounter() && s.scenario().cleared("Hill Wolves"));
}

SUITE("Scenario — a trigger's if gates it, and it fires once the condition holds") {
    AdventureSession s;
    begin(s, "trigger_if", Json::parse(R"({"start_companions": [], "triggers": [
        {"id": "blessing", "when": {"event": "visit", "name": "Shrine"}, "if": {"item": "scarab_amulet"},
         "do": [{"say": [["Priest", "The scarab is welcome."]]}]}]})"));
    s.travel({-3, 3});
    CHECK(!s.scenario().fired("blessing"));                          // not without the amulet
    s.scenario().drainLines();
    s.addItem("scarab_amulet");
    s.endDay();
    s.travel({-2, 3});
    s.travel({-3, 3});                                               // back again, carrying it
    CHECK(s.scenario().fired("blessing"));
    bool heard = false;
    for (const auto& l : s.scenario().drainLines()) heard = heard || l.speaker == "Priest";
    CHECK(heard);
}

SUITE("Scenario — the if/then/else action runs one branch, nested") {
    AdventureSession s;
    begin(s, "branch", Json::parse(R"({"start_companions": [], "triggers": []})"));
    const Json branch = Json::parse(R"([{"if": {"flag": "spared"},
        "then": [{"say": [["Warrior", "You again."]]}, {"if": {"item": "scarab_amulet"}, "then": [{"flag": {"amulet_seen": true}}]}],
        "else": [{"say": [["Warrior", "Who are you?"]]}]}])");
    s.scenario().run(s, branch);
    auto lines = s.scenario().drainLines();
    CHECK(lines.size() == 1 && lines[0].text == "Who are you?");
    s.setFlag("spared", true);
    s.addItem("scarab_amulet");
    s.scenario().run(s, branch);
    lines = s.scenario().drainLines();
    CHECK(lines.size() == 1 && lines[0].text == "You again.");
    CHECK(s.flags().value("amulet_seen", false));
    s.scenario().run(s, Json::parse(R"([{"if": false, "then": [{"flag": {"x": true}}]}])"));   // no else: nothing
    CHECK(!s.flags().contains("x"));
}

SUITE("Scenario — legacy fields become the same trees and behave as before") {
    CHECK(Condition::fromFlags("a") == Json({{"flag", "a"}}));
    CHECK(Condition::fromFlags(Json::array({"a", "b"})) == Json({{"all", {{{"flag", "a"}}, {{"flag", "b"}}}}}));
    CHECK(Condition::fromFlags(Json{{"a", 1}}) == Json({{"all", {{{"flag", "a"}, {"is", 1}}}}}));
    CHECK(throwsInvalid([] { Condition::fromFlags(3); }));
    CHECK(Scenario::flagsMatch(Json{{"a", true}, {"b", 2}}, Json::array({"a", "b"})));
    CHECK(!Scenario::flagsMatch(Json{{"a", true}}, Json::array({"a", 7})));   // malformed: never holds

    const Json option = Json::parse(R"({"id": "o", "requires_item": "x", "if": {"flag": "f"}, "requires_flag": {"g": 1}})");
    CHECK(Scenario::shownWhen(option) == Json({{"all", {{{"item", "x"}}, {{"flag", "f"}}}}}));
    CHECK(Scenario::lockedUnless(option) == Json({{"all", {{{"flag", "g"}, {"is", 1}}}}}));

    // after + delay, unless, flag, unless_flag: the same story order as before.
    AdventureSession s;
    begin(s, "legacy", Json::parse(R"({"start_companions": [], "triggers": [
        {"id": "intro", "when": {"event": "start"}, "do": []},
        {"id": "dawn", "when": {"event": "night", "after": "intro", "delay": 2, "flag": "awake", "unless_flag": "asleep"},
         "do": [{"flag": {"dawn": true}}]},
        {"id": "too_late", "when": {"event": "night", "after": "intro", "unless": "dawn"}, "do": []},
        {"id": "never", "when": {"event": "night", "delay": 1}, "do": []}]})"));
    s.endDay();
    CHECK(s.scenario().fired("too_late"));                           // night 1: fired before dawn could
    s.setFlag("awake", true);
    s.setFlag("asleep", true);
    s.endDay();
    s.endDay();
    CHECK(!s.scenario().fired("dawn"));                              // unless_flag holds
    s.setFlag("asleep", false);
    s.endDay();
    CHECK(s.scenario().fired("dawn") && s.flags().value("dawn", false));
    CHECK(!s.scenario().fired("never"));                             // a delay without after never comes
}
