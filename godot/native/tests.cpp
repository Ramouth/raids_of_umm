#include "combat_session.h"
#include "adventure_bridge.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using Json = CombatSession::Json;
static void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
int main(int argc, char** argv) {
    try {
        check(argc == 2, "Pass the data directory");
        AdventureBridge adventure;
        const std::string data = argv[1];
        check(adventure.start(data + "/maps/old_passage.json", data, data + "/maps/old_passage.encounters.json", 1,
                              data + "/maps/old_passage.triggers.json").at("ok"), "Production adventure starts");
        {   // Campaign flow: load, start a map, decide, end it, advance, carry, save the campaign.
            const std::string dir = (std::filesystem::temp_directory_path() / "umm_bridge_campaign").string();
            std::filesystem::create_directories(dir);
            WorldMap fixture;
            fixture.clear(4);
            fixture.placeObject({{-2, 0}, ObjType::Town, "Home", 1});
            check(!fixture.saveJson(dir + "/a.json"), "Campaign fixture map written");
            std::ofstream(dir + "/a.triggers.json") << R"({"triggers": [
                {"id": "oath", "when": {"event": "start"}, "do": [{"choice": {"id": "oath", "title": "Oath", "options": [
                    {"id": "spare", "label": "Spare", "then": [{"flag": {"mercy": true}}, {"ending": {"id": "e", "next": "2.0"}}]},
                    {"id": "burn", "label": "Burn", "requires_flag": "torch", "then": []}]}}]}]})";
            std::ofstream(dir + "/b.triggers.json") << R"({"triggers": [
                {"id": "remember", "when": {"event": "start", "flag": {"choice.oath": "spare"}},
                 "do": [{"say": [["Ushari", "You spared them."]]}]}]})";
            std::ofstream(dir + "/campaign.json") << Json{{"chapters", {
                {{"id", "one"}, {"title", "One"}, {"maps", {
                    {{"id", "1.0"}, {"title", "A"}, {"map", dir + "/a.json"}, {"triggers", dir + "/a.triggers.json"}, {"max_level", 2}},
                    {{"id", "1.1"}, {"title", "skipped"}, {"map", dir + "/a.json"}}}}},
                {{"id", "two"}, {"title", "Two"}, {"maps", {
                    {{"id", "2.0"}, {"title", "B"}, {"map", dir + "/a.json"}, {"triggers", dir + "/b.triggers.json"}}}}}}}}.dump();
            AdventureBridge camp;
            check(!camp.campaign_start_map(1).at("ok"), "No campaign map before a campaign is loaded");
            auto loaded = camp.campaign_load(dir + "/campaign.json", data);
            check(loaded.at("ok") && loaded["campaign"]["map"]["id"] == "1.0", "Campaign loads at its first map");
            check(loaded["chapters"].size() == 2, "Campaign lists its chapters");
            auto map = camp.campaign_start_map(1);
            check(map.at("ok") && map["level_cap"] == 2 && map["hero_progress"]["cap"] == 2, "Map starts with its level cap");
            check(map["story_choice"]["options"][1]["locked"] == true, "requires_flag option is shown locked");
            check(!camp.campaign_advance().at("ok"), "Cannot advance before the map ends");
            check(!camp.choose_story("burn").at("ok"), "Locked option refused");
            auto chosen = camp.choose_story("spare");
            check(chosen["flags"]["choice.oath"] == "spare" && chosen["flags"]["mercy"] == true, "Choice and flag action recorded");
            check(chosen["won"] == true && chosen["next_map"] == "2.0", "Ending names the next map");
            auto step = camp.campaign_advance();
            check(step.at("ok") && step["new_chapter"] == true && step["campaign"]["map"]["id"] == "2.0",
                  "Advance follows the ending's next map into the next chapter");
            check(step["campaign"]["state"]["history"].size() == 1, "Finished map's stats are recorded");
            check(camp.campaign_save(dir + "/campaign.save.json").at("ok"), "Campaign saves");
            AdventureBridge resumed;
            auto back = resumed.campaign_load_save(dir + "/campaign.save.json");
            check(back.at("ok") && back["campaign"]["map"]["id"] == "2.0", "Campaign save reloads its position");
            auto second = resumed.campaign_start_map(1);
            bool remembered = false;
            for (const auto& line : second["lines"]) remembered |= line["text"] == "You spared them.";
            check(second.at("ok") && remembered, "Flag from map 1.0 gates a trigger on map 2.0");
            check(second["level_cap"] == 10, "No max_level: the global cap");
            check(second["flags"]["choice.oath"] == "spare", "Flags carried into the next map");
            auto last = resumed.campaign_current();
            check(last["campaign"]["chapter"]["id"] == "two", "Current chapter reported");
        }
        check(adventure.resolve_encounter(false, Json::parse(R"([{"id":"grey_wolf","count":3.0}])")).at("ok"),
              "Godot integral JSON floats are accepted for enemy survivors");
        for (const char* invalid : {R"([{"id":"grey_wolf","count":1.5}])", R"([{"id":"grey_wolf","count":-1}])",
                                   R"([{"id":"grey_wolf","count":1e20}])", R"([{"id":"grey_wolf","count":"3"}])"})
            check(!adventure.resolve_encounter(false, Json::parse(invalid)).at("ok"), "Malformed survivor counts are rejected");
        const Json guards = {{"guards", {{{"id", "skeleton_warrior"}, {"count", 12}},
            {{"id", "sand_scorpion"}, {"count", 4}}}}, {"reward", "scarab_amulet"}};
        const Json army = {{{"id", "desert_archer"}, {"count", 10}}, {{"id", "mummy"}, {"count", 3}}};
        // The Godot bridge must pass the seed before rolling initial initiative.
        bool runnerFirst = false, wolfFirst = false;
        for (unsigned seed = 0; seed < 32; ++seed) {
            CombatSession rolled, replay;
            Json troops = {{{"id", "woad_runner"}, {"count", 10}}};
            Json foes = {{"guards", {{{"id", "grey_wolf"}, {"count", 10}}}}, {"seed", seed}};
            auto a = rolled.start(argv[1], troops, foes);
            auto b = replay.start(argv[1], troops, foes);
            check(a.at("ok") && b.at("ok"), "Seeded initiative battles start");
            check(a["state"] == b["state"], "The same seed reproduces initial rolls and queue");
            const auto& state = a["state"];
            runnerFirst |= state["active"] == "p0";
            wolfFirst |= state["active"] == "e0";
            for (const auto& unit : state["units"]) {
                const int roll = unit["initiative_roll"];
                check(roll >= 1 && roll <= 6, "Every stack rolls d6");
                check(unit["initiative_score"].get<int>() == unit["speed"].get<int>() + roll,
                      "Displayed initiative equals speed plus roll");
            }
            check(state["next_round"][0] == state["active"], "Round preview uses the same battle rolls");
        }
        check(runnerFirst && wolfFirst, "Wolves do not always start ahead of runners");
        CombatSession steady, shaken;
        Json withCompanion = army;
        withCompanion.push_back({{"id", "ushari"}, {"level", 3}, {"companion", true}});
        auto normal = steady.start(argv[1], withCompanion, guards);
        withCompanion.back()["morale"] = -2;
        auto troubled = shaken.start(argv[1], withCompanion, guards);
        check(normal.at("ok") && troubled.at("ok"), "Companion morale battles start");
        check(normal["state"]["units"][2]["attack"].get<int>() - 2 ==
              troubled["state"]["units"][2]["attack"].get<int>(), "Shaken Ushari actually loses two attack");
        check(normal["state"]["units"][0]["attack"] == troubled["state"]["units"][0]["attack"],
              "Ushari's morale does not penalize troops");
        CombatSession session;
        auto reply = session.start(argv[1], army, guards);
        check(reply.at("ok"), "Registry and original armies load");
        check(!session.command("defend").at("ok"), "Commands blocked before presentation acknowledgement");
        check(!session.acknowledge(reply.at("ticket").get<int>() + 1), "Wrong acknowledgement rejected");
        check(session.acknowledge(reply.at("ticket")), "Correct acknowledgement accepted");
        check(!session.acknowledge(reply.at("ticket")), "Duplicate acknowledgement rejected");
        check(!session.command("move", 100, 100).at("ok"), "Out-of-bounds move rejected");
        check(!session.command("attack", 0, 0).at("ok"), "Empty target rejected");
        const auto friendly_cell = reply["state"]["units"][1]["cell"];
        check(!session.command("move", friendly_cell[0], friendly_cell[1]).at("ok"), "Occupied friendly hex rejected");
        check(!session.command("attack", friendly_cell[0], friendly_cell[1]).at("ok"), "Friendly fire rejected");
        check(!session.start(argv[1], army, guards).at("ok"), "Cannot reset a running battle");
        for (int actions = 0; actions < 500 && session.snapshot()["result"] == "ongoing"; ++actions) {
            reply = session.command("ai");
            check(reply.at("ok"), "AI action accepted");
            check(!reply["events"].empty(), "Actions produce animation events");
            for (const auto& event : reply["events"]) {
                if (event["type"] != "move") continue;
                HexCoord previous{event["from"][0], event["from"][1]};
                check(!event["path"].empty(), "Movement has animation steps");
                for (const auto& step : event["path"]) {
                    HexCoord cell{step[0], step[1]};
                    check(CombatMap::inBounds(cell) && previous.distanceTo(cell) == 1,
                        "Animation path stays on adjacent board hexes");
                    for (const auto& unit : reply["state"]["units"])
                        if (unit["key"] != event["unit"] && unit["count"].get<int>() > 0)
                            check(unit["cell"] != step, "Animation never crosses an occupied hex");
                    previous = cell;
                }
                check(event["path"].back() == event["to"], "Animation ends on the native destination");
            }
            if (!reply["state"]["player_turn"].get<bool>() && reply["state"]["result"] == "ongoing") {
                check(session.acknowledge(reply["ticket"]), "AI acknowledgement accepted");
                check(!session.command("defend").at("ok"), "Player cannot act during enemy turn");
            } else check(session.acknowledge(reply["ticket"]), "Action acknowledged");
        }
        check(session.snapshot()["result"] != "ongoing", "Auto-battle terminates");
        check(!session.command("defend").at("ok"), "Finished combat rejects orders");
        auto state = session.snapshot();
        check((state["result"] == "victory") == !state["rewards"].empty(), "Only victory awards loot");
        if (state["result"] == "defeat") check(state["survivors"].empty(), "Defeat has no survivors");

        CombatSession melee;
        reply = melee.start(argv[1], {{{"id", "djinn"}, {"count", 1}}}, guards);
        check(melee.acknowledge(reply["ticket"]), "Melee setup acknowledged");
        const auto enemy_cell = reply["state"]["units"][1]["cell"];
        check(!melee.command("attack", enemy_cell[0], enemy_cell[1]).at("ok"), "Distant melee attack rejected");
        reply = melee.command("retreat");
        check(reply["state"]["result"] == "retreat", "Retreat ends combat");
        check(!reply["state"]["enemy_survivors"].empty(), "Retreat exports the surviving enemy army for adventure persistence");
        check(reply["state"]["survivors"][0]["count"] == 1 && reply["state"]["rewards"].empty(), "Retreat preserves survivors without loot");

        // Tactics: opening orders come before any other command, once.
        CombatSession tactics;
        Json planned = guards;
        planned["tactics"] = 1;
        reply = tactics.start(argv[1], army, planned);
        check(reply["state"]["opening"] == 1, "Tactics rank is reported as opening orders");
        check(tactics.acknowledge(reply["ticket"]), "Tactics setup acknowledged");
        check(!tactics.command("ai").at("ok"), "No action before the opening orders");
        const auto mummies = reply["state"]["units"][1]["cell"];
        const auto archers = reply["state"]["units"][0]["cell"];
        check(!tactics.command_route("opening", Json::array({archers, mummies})).at("ok"), "Rank 1 orders one stack");
        check(!tactics.command_route("opening", Json::array({reply["state"]["units"][2]["cell"]})).at("ok"),
              "Opening orders name your own stacks");
        reply = tactics.command_route("opening", Json::array({mummies}));
        check(reply.at("ok") && reply["state"]["opening"] == 0, "Opening orders accepted");
        check(reply["state"]["active"] == "p1", "The chosen stack acts first");
        check(tactics.acknowledge(reply["ticket"]), "Opening acknowledged");
        check(!tactics.command_route("opening", Json::array()).at("ok"), "Orders are given once");

        CombatSession fieldworks;
        Json prepared = guards;
        prepared["fieldworks"] = {{"barricade", 1}, {"stakes", 1}};
        prepared["tactics"] = 1;
        reply = fieldworks.start(argv[1], army, prepared);
        check(reply["state"]["deploying"], "Purchased fieldworks open deployment");
        fieldworks.acknowledge(reply["ticket"]);
        check(!fieldworks.command("ai")["ok"], "AI cannot act during deployment");
        check(!fieldworks.command_route("opening", Json::array())["ok"], "Tactics waits until fieldworks are placed");
        check(!fieldworks.command("place_barricade", 9, -2)["ok"], "Cannot build in enemy deployment zone");
        reply = fieldworks.command("place_barricade", 2, 1);
        check(reply["ok"] && reply["state"]["fieldworks"].size() == 1, "Barricade placed");
        fieldworks.acknowledge(reply["ticket"]);
        check(!fieldworks.command("place_barricade", 3, 0)["ok"], "Cannot place more equipment than owned");
        check(!fieldworks.command("place_stakes", 2, 1)["ok"], "Cannot overlap fieldworks");
        reply = fieldworks.command("remove_fieldwork", 2, 1);
        fieldworks.acknowledge(reply["ticket"]);
        reply = fieldworks.command("place_barricade", 2, 1);
        check(reply["ok"], "Recovered equipment can be repositioned");
        fieldworks.acknowledge(reply["ticket"]);
        reply = fieldworks.command("deploy_done");
        check(reply["ok"] && !reply["state"]["deploying"].get<bool>() && reply["state"]["opening"] == 1,
              "Deployment ends before opening orders");
        fieldworks.acknowledge(reply["ticket"]);
        reply = fieldworks.command_route("opening", Json::array());
        fieldworks.acknowledge(reply["ticket"]);
        check(!fieldworks.command("place_stakes", 3, 0)["ok"], "No construction once battle starts");
        check(!fieldworks.command("remove_fieldwork", 2, 1)["ok"], "No removal once battle starts");

        CombatSession invalid;
        check(!invalid.start(argv[1], {{{"id", "not_a_unit"}, {"count", 1}}}, guards).at("ok"), "Invalid registry references rejected");
        check(!invalid.start(argv[1], Json::array(), guards).at("ok"), "Empty army rejected");
        std::cout << "Combat bridge tests: PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Combat bridge tests: FAIL: " << error.what() << '\n';
        return 1;
    }
}
