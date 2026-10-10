#include "combat_session.h"
#include "adventure_bridge.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sys/wait.h>
using Json = CombatSession::Json;
static void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
int main(int argc, char** argv) {
    try {
        check(argc == 2 || argc == 3, "Pass the data directory (and demo_bot)");
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
                    {"id": "burn", "label": "Burn", "requires_flag": "torch", "then": []},
                    {"id": "secret", "label": "Secret", "if": {"flag": "never"}, "then": []},
                    {"id": "pay", "label": "Pay", "requires": {"resource": "Gold", ">=": 999999}, "then": []}]}}]}]})";
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
            check(map["map_start_army"] == false && map["treasury"]["Gold"] == 2000 && map["treasury"]["Wood"] == 5,
                  "A map without start_army / start_resources: the client's army, the default purse");
            check(map["story_choice"]["options"][1]["locked"] == true, "requires_flag option is shown locked");
            check(map["story_choice"]["options"].size() == 3 && map["story_choice"]["options"][2]["id"] == "pay"
                  && map["story_choice"]["options"][2]["locked"] == true && map["story_choice"]["options"][0]["locked"] == false,
                  "An option whose if fails is left out; one whose requires fails is shown locked");
            check(!camp.choose_story("secret").at("ok") && !camp.choose_story("pay").at("ok"), "Hidden and locked options refused");
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
        {   // A campaign's first map: its start cell, start_army and Aldren reach the snapshot;
            // a choice at engage can withdraw the pack.
            const std::string dir = (std::filesystem::temp_directory_path() / "umm_bridge_chapter").string();
            std::filesystem::create_directories(dir);
            WorldMap field;
            field.clear(4);
            field.placeObject({{-3, 0}, ObjType::Town, "Home", 1});
            field.placeObject({{2, 0}, ObjType::Guard, "Ford Wolves", 0});
            field.setStart(HexCoord{0, 0});
            check(!field.saveJson(dir + "/field.json"), "Chapter fixture map written");
            std::ofstream(dir + "/field.triggers.json") << R"({"start_companions": ["aldren"],
                "start_army": [{"id": "woad_runner", "count": 10}, {"id": "cruth_slinger", "count": 4}],
                "start_resources": {"Gold": 300, "Wood": 2},
                "triggers": [{"id": "meet", "when": {"event": "engage", "name": "Ford Wolves"},
                  "do": [{"choice": {"id": "pack", "options": [{"id": "spare", "then": [{"withdraw": "Ford Wolves"}]}]}}]}]})";
            std::ofstream(dir + "/campaign.json") << Json{{"chapters", {{{"id", "one"}, {"title", "One"}, {"maps", {
                {{"id", "1.0"}, {"title", "A"}, {"map", dir + "/field.json"}, {"triggers", dir + "/field.triggers.json"}}}}}}}}.dump();
            AdventureBridge camp;
            check(camp.campaign_load(dir + "/campaign.json", data).at("ok"), "Chapter fixture campaign loads");
            auto map = camp.campaign_start_map(1);
            check(map.at("ok") && map["hero"] == Json::array({0, 0}), "Hero starts on the map's start cell, not the town");
            check(map["army"].size() == 2 && map["army"][0]["id"] == "woad_runner" && map["army"][0]["count"] == 10,
                  "A fresh campaign start fields the map's start_army");
            check(map["map_start_army"] == true, "The client is told the map names its own army");
            check(map["treasury"]["Gold"] == 300 && map["treasury"]["Wood"] == 2 && map["treasury"]["Stone"] == 0,
                  "A fresh campaign start holds exactly the map's start_resources");
            check(map["specials"].size() == 1 && map["specials"][0]["id"] == "aldren"
                  && map["battle_companions"].size() == 1, "Aldren rides and fights as a companion");
            auto met = camp.travel(2, 0);
            check(met["encounter"].is_object() && met["story_choice"]["id"] == "pack", "Engage raises the choice with the fight pending");
            auto spared = camp.choose_story("spare");
            check(spared.at("ok") && spared["encounter"].is_null(), "Withdraw calls the pending fight off");
        }
        {   // A scene action mid-map: the reply that clears the pack carries its frames once; the map goes on.
            const std::string dir = (std::filesystem::temp_directory_path() / "umm_bridge_scene").string();
            std::filesystem::create_directories(dir);
            WorldMap field;
            field.clear(4);
            field.placeObject({{-3, 0}, ObjType::Town, "Home", 1});
            field.placeObject({{2, 0}, ObjType::Guard, "Rabid Dogs", 0});
            field.setStart(HexCoord{0, 0});
            check(!field.saveJson(dir + "/field.json"), "Scene fixture map written");
            std::ofstream(dir + "/field.triggers.json") << R"({"start_companions": [],
                "start_army": [{"id": "woad_runner", "count": 10}],
                "triggers": [{"id": "tag", "when": {"event": "cleared", "names": ["Rabid Dogs"]},
                  "do": [{"scene": {"frames": [{"scene": "dog_tag", "text": "A tag in a northern script."}]}}]}]})";
            AdventureBridge adv;
            auto first = adv.start(dir + "/field.json", data, "", 1, dir + "/field.triggers.json");
            check(first.at("ok"), "Scene fixture starts");
            check(first["map_start_army"] == true && first["treasury"]["Gold"] == 2000,
                  "start_army without start_resources: the default purse");
            auto met = adv.travel(2, 0);
            check(met["scenes"].is_array() && met["scenes"].empty(), "No scene before the pack is beaten");
            auto won = adv.resolve_encounter(true);
            check(won["scenes"].size() == 1 && won["scenes"][0]["frames"][0]["scene"] == "dog_tag"
                  && won["scenes"][0]["frames"][0]["text"] == "A tag in a northern script.", "The scene's frames reach the client");
            check(won["won"] == false && won["lost"] == false, "A scene does not end the map");
            auto next = adv.end_day();
            check(next["scenes"].empty(), "The scene is delivered once");
        }
        {   // Result screens: the combat's battle record and the encounter's spoils reach the
            // client; keepsakes are listed and never worn; a requires_item option appears only
            // while the item is carried, and handing it over ends the encounter without a fight.
            namespace fs = std::filesystem;
            const fs::path dir = fs::temp_directory_path() / "umm_bridge_spoils";
            fs::create_directories(dir / "data");
            for (const auto& entry : fs::directory_iterator(data))
                if (entry.is_regular_file())
                    fs::copy_file(entry.path(), dir / "data" / entry.path().filename(), fs::copy_options::overwrite_existing);
            Json items = Json::parse(std::ifstream(data + "/items.json"));
            items["items"].push_back({{"id", "test_dog_tag"}, {"name", "Dog Tag"}, {"kind", "keepsake"},
                                      {"text", "Bran. A good boy."}});
            std::ofstream(dir / "data" / "items.json") << items.dump();
            const std::string fixtureData = (dir / "data").string();
            WorldMap field;
            field.clear(4);
            field.placeObject({{-3, 0}, ObjType::Town, "Home", 1});
            field.placeObject({{2, 0}, ObjType::Guard, "Hill Dogs", 0});
            field.placeObject({{0, 3}, ObjType::Guard, "Northern Warrior", 0});
            field.setStart(HexCoord{0, 0});
            check(!field.saveJson((dir / "field.json").string()), "Spoils fixture map written");
            std::ofstream(dir / "field.encounters.json") << R"({"objects": {
                "Hill Dogs": {"guards": [{"id": "grey_wolf", "count": 2}], "reward": {"Gold": 150, "items": ["test_dog_tag"]}},
                "Northern Warrior": {"guards": [{"id": "grey_wolf", "count": 6}], "reward": {"Gold": 400}}}})";
            std::ofstream(dir / "field.triggers.json") << R"({"start_companions": [],
                "start_army": [{"id": "woad_runner", "count": 12}],
                "triggers": [{"id": "warrior", "when": {"event": "engage", "name": "Northern Warrior"},
                  "do": [{"choice": {"id": "warrior", "presentation": "conversation", "options": [
                    {"id": "fight", "label": "Fight", "then": []},
                    {"id": "give_tag", "label": "Hand him the dog tag", "requires_item": "test_dog_tag",
                     "then": [{"take_item": "test_dog_tag"}, {"withdraw": "Northern Warrior"}]}]}}]}]})";
            auto begin = [&](AdventureBridge& adv) {
                return adv.start((dir / "field.json").string(), fixtureData, (dir / "field.encounters.json").string(), 1,
                                 (dir / "field.triggers.json").string());
            };
            // Plays the pending fight with the AI on both sides ("retreat": the player runs at once).
            auto fight = [&](const Json& snap, bool retreat) {
                CombatSession combat;
                auto reply = combat.start(fixtureData, snap["army"], {{"guards", snap["encounter"]["guards"]}, {"seed", 5}});
                check(reply.at("ok"), "Spoils fixture battle starts");
                int hp[2] = {0, 0};
                for (const auto& u : reply["state"]["units"]) hp[u["player"].get<bool>() ? 0 : 1] += u["hp"].get<int>();
                for (int turn = 0; turn < 300 && reply["state"]["result"] == "ongoing"; ++turn) {
                    combat.acknowledge(reply["ticket"]);
                    reply = combat.command(retreat ? "retreat" : "ai");
                    check(reply.at("ok"), "Spoils fixture battle command accepted");
                }
                const Json state = reply["state"];
                int left[2] = {0, 0};
                for (const auto& u : state["units"]) left[u["player"].get<bool>() ? 0 : 1] += u["hp"].get<int>();
                const Json& b = state["battle"];
                check(b["total_damage"]["player"] == hp[1] - left[1] && b["total_damage"]["enemy"] == hp[0] - left[0],
                      "Battle totals match the hit points each side lost");
                int biggest = 0;
                for (const auto& row : b["per_stack"]) biggest = std::max(biggest, row["biggest_hit"].get<int>());
                check(b["max_hit"]["amount"] == biggest, "The max hit is the biggest blow any stack landed");
                return state;
            };

            AdventureBridge adv;
            check(begin(adv).at("ok"), "Spoils fixture starts");
            auto met = adv.travel(2, 0);
            check(met["encounter"].is_object() && met["encounter"]["items"] == Json::array({"test_dog_tag"}),
                  "The pending encounter lists its reward items");
            const int xp = met["encounter"]["xp"];
            const Json state = fight(met, false);
            check(state["result"] == "victory" && state["battle"]["max_hit"]["side"] == "player"
                  && state["battle"]["max_hit"]["unit"] == "woad_runner", "The expedition wins and lands the biggest blow");
            auto won = adv.resolve_encounter(true, state["enemy_survivors"], state["battle"]);
            check(won["battle"]["result"] == "victory" && won["battle"]["xp"] == xp
                  && won["battle"]["total_damage"] == state["battle"]["total_damage"]
                  && won["battle"]["losses"]["enemy"][0]["id"] == "grey_wolf" && won["battle"]["losses"]["enemy"][0]["count"] == 2,
                  "The reply carries the battle record, its XP and the losses");
            check(won["spoils"]["resources"] == Json{{"Gold", 150}} && won["spoils"]["items"].size() == 1
                  && won["spoils"]["items"][0] == Json{{"id", "test_dog_tag"}, {"name", "Dog Tag"}, {"kind", "keepsake"},
                                                       {"text", "Bran. A good boy."}}, "Spoils list the gold and the keepsake");
            check(won["keepsakes"] == Json::array({Json{{"id", "test_dog_tag"}, {"name", "Dog Tag"}, {"text", "Bran. A good boy."}}})
                  && won["backpack"].empty() && !adv.equip("test_dog_tag").at("ok"), "A keepsake is listed, never worn");
            auto warrior = adv.travel(1, 2);
            check(warrior["story_choice"]["options"].size() == 2, "Carrying the tag offers it");
            auto spared = adv.choose_story("give_tag");
            check(spared.at("ok") && spared["encounter"].is_null() && spared["keepsakes"].empty()
                  && spared["hero_progress"]["xp"] == won["hero_progress"]["xp"], "Handing it over: no fight, no XP, the tag is gone");

            AdventureBridge ran;
            check(begin(ran).at("ok"), "Spoils fixture restarts");
            auto toWarrior = ran.travel(1, 2);
            check(toWarrior["story_choice"]["options"].size() == 1, "Without the tag the option is not offered");
            check(!ran.choose_story("give_tag").at("ok"), "The hidden option is refused");
            auto fought = ran.choose_story("fight");
            check(fought["encounter"].is_object(), "Fight keeps the battle pending");
            const Json fled = fight(fought, true);
            check(fled["result"] == "retreat", "The expedition retreats");
            auto back = ran.resolve_encounter(false, fled["enemy_survivors"], fled["battle"]);
            check(back["battle"]["result"] == "retreat" && back["battle"]["xp"] == 0
                  && back["battle"]["losses"] == fled["battle"]["losses"]
                  && back["spoils"]["resources"].empty() && back["spoils"]["items"].empty(),
                  "A retreat carries its losses and no spoils");
            // A real defeat: the hero walks back to the warrior (a fight pending, its choice
            // answered), loses with no record passed; nothing is paid and the camp stands.
            auto again = ran.travel(0, 3);
            if (again["story_choice"].is_object() && !again["story_choice"].empty()) again = ran.choose_story("fight");
            check(again["encounter"].is_object() && again["encounter"]["name"] == "Northern Warrior"
                  && again["encounter_awaits_choice"] == false, "The warrior's fight is pending again");
            const Json goldBefore = again["treasury"]["Gold"];
            auto defeat = ran.resolve_encounter(false);   // no record passed: a bare defeat
            check(defeat.at("ok") && defeat["battle"]["result"] == "defeat" && defeat["battle"]["xp"] == 0
                  && defeat["spoils"]["resources"].empty() && defeat["spoils"]["items"].empty()
                  && defeat["treasury"]["Gold"] == goldBefore, "A defeat pays nothing");
            check(defeat["encounter"].is_null() && defeat["guarded"].size() == again["guarded"].size(),
                  "After a defeat the camp still stands");

            // The engage choice holds its fight: the bridge refuses to resolve it until answered.
            AdventureBridge held;
            check(begin(held).at("ok"), "Spoils fixture restarts for the held fight");
            auto asked = held.travel(1, 2);
            check(asked["encounter"].is_object() && asked["encounter_awaits_choice"] == true,
                  "The snapshot says the pending fight awaits the story choice");
            auto early = held.resolve_encounter(true);
            check(!early.at("ok") && early["encounter"].is_object() && early["guarded"].size() == asked["guarded"].size(),
                  "resolve_encounter refuses while the engage choice is open");
            auto answered = held.choose_story("fight");
            check(answered["encounter_awaits_choice"] == false, "Answered: the fight may be fought");
        }
        {   // take_item on a commander item: the snapshot's items, backpack and paper doll all lose it.
            const std::string dir = (std::filesystem::temp_directory_path() / "umm_bridge_take").string();
            std::filesystem::create_directories(dir);
            WorldMap field;
            field.clear(4);
            field.placeObject({{-3, 0}, ObjType::Town, "Home", 1});
            field.placeObject({{2, 0}, ObjType::QuestGiver, "Smith", 0});
            field.setStart(HexCoord{0, 0});
            check(!field.saveJson(dir + "/field.json"), "Take fixture map written");
            std::ofstream(dir + "/field.triggers.json") << R"({"start_companions": [],
                "start_army": [{"id": "woad_runner", "count": 10}],
                "triggers": [{"id": "smith", "when": {"event": "visit", "name": "Smith"},
                  "do": [{"take_item": "red_hand_sword"}, {"take_item": "hermits_seed"}]}]})";
            AdventureBridge adv;
            check(adv.start(dir + "/field.json", data, "", 1, dir + "/field.triggers.json").at("ok"), "Take fixture starts");
            adv.add_item("red_hand_sword");
            auto gained = adv.add_item("hermits_seed");
            bool listed = false;
            for (const auto& e : gained["story"]) listed |= e["kind"] == "item" && e["id"] == "hermits_seed";
            check(listed, "An item gained is a story event");
            auto worn = adv.equip("red_hand_sword");
            check(worn.at("ok"), "The sword is worn");
            auto taken = adv.travel(2, 0);
            bool slotEmpty = true;
            for (const auto& [slot, id] : taken["equipped"].items()) slotEmpty &= id != "red_hand_sword";
            bool inItems = false;
            for (const auto& id : taken["items"]) inItems |= id == "red_hand_sword" || id == "hermits_seed";
            bool inPack = false;
            for (const auto& id : taken["backpack"]) inPack |= id == "red_hand_sword" || id == "hermits_seed";
            int lost = 0;
            for (const auto& e : taken["story"]) lost += e["kind"] == "item_lost";
            check(slotEmpty && !inItems && !inPack && lost == 2, "Handed over: gone from items, backpack and the paper doll");
            check(!adv.equip("red_hand_sword").at("ok"), "A handed-over item cannot be worn again");
        }
        {   // One ordered story queue: a trigger that says, then shows a scene, then says again
            // reaches the client in that order; lines and scenes stay for older callers; a
            // pending queue survives a save.
            const std::string dir = (std::filesystem::temp_directory_path() / "umm_bridge_story").string();
            std::filesystem::create_directories(dir);
            WorldMap field;
            field.clear(4);
            field.placeObject({{-3, 0}, ObjType::Town, "Home", 1});
            field.placeObject({{2, 0}, ObjType::QuestGiver, "Cairn", 0});
            field.setStart(HexCoord{0, 0});
            check(!field.saveJson(dir + "/field.json"), "Story fixture map written");
            std::ofstream(dir + "/field.triggers.json") << R"({"start_companions": [],
                "start_army": [{"id": "woad_runner", "count": 10}],
                "triggers": [{"id": "cairn", "when": {"event": "visit", "name": "Cairn"}, "do": [
                  {"say": [["Drest", "Read it."]]},
                  {"scene": {"frames": [{"scene": "cairn", "text": "Names cut in the stone."}]}},
                  {"say": [["Drest", "Your grandfather's."]]},
                  {"lore": ["The cairn", "Names of the fallen."]}]}]})";
            AdventureBridge adv;
            check(adv.start(dir + "/field.json", data, "", 1, dir + "/field.triggers.json").at("ok"), "Story fixture starts");
            auto at = adv.travel(2, 0);
            std::vector<std::string> kinds;
            for (const auto& e : at["story"]) kinds.push_back(e["kind"]);
            check(kinds == std::vector<std::string>{"line", "scene", "line", "lore"}, "Story events arrive in action order");
            check(at["story"][0]["text"] == "Read it." && at["story"][1]["frames"][0]["text"] == "Names cut in the stone."
                  && at["story"][2]["speaker"] == "Drest", "Each story event carries its own words");
            check(at["lines"].size() == 2 && at["scenes"].size() == 1, "lines and scenes are the same events, filtered");
            check(adv.end_day()["story"].empty(), "The queue is delivered once");

            AdventureSession raw;                 // a save taken before the client drained it keeps it
            check(!raw.start(dir + "/field.json", data, "", 1, dir + "/field.triggers.json"), "Raw story session starts");
            raw.travel({2, 0});
            const std::string save = dir + "/pending.json";
            std::ofstream(save) << raw.saveState().dump();
            AdventureBridge resumed;
            auto back = resumed.load(save);
            check(back.at("ok") && back["story"].size() == 4 && back["story"][1]["kind"] == "scene",
                  "A pending story queue survives save and load, in order");
        }
        {   // Party limits through the bridge: two ride, a third raises "Who stays behind?".
            const std::string dir = (std::filesystem::temp_directory_path() / "umm_bridge_party").string();
            std::filesystem::create_directories(dir);
            WorldMap field;
            field.clear(4);
            field.placeObject({{-3, 0}, ObjType::Town, "Home", 1});
            field.setStart(HexCoord{0, 0});
            check(!field.saveJson(dir + "/field.json"), "Party fixture map written");
            std::ofstream(dir + "/field.triggers.json") << R"({"start_companions": ["drest", "aldren"],
                "essential_companions": ["drest"], "start_army": [{"id": "woad_runner", "count": 10}], "triggers": []})";
            AdventureBridge adv;
            auto first = adv.start(dir + "/field.json", data, "", 1, dir + "/field.triggers.json");
            check(first.at("ok") && first["max_riding"] == 2 && first["max_stacks"] == 5, "The snapshot names the limits");
            auto joined = adv.join_special("kharim");
            const Json& choice = joined["story_choice"];
            check(choice["id"] == "stay_behind" && choice["kind"] == "party" && choice["options"].size() == 2
                  && choice["options"][0]["id"] == "aldren" && choice["options"][1]["id"] == "kharim",
                  "A third companion raises Who stays behind? without the essential one");
            check(joined["battle_companions"].size() == 2, "Battles field two companions");
            auto stayed = adv.choose_story("aldren");
            bool told = false;
            for (const auto& e : stayed["story"])
                told |= e["kind"] == "party" && e["id"] == "aldren" && e["where"] == "Home";
            check(stayed.at("ok") && told && stayed["story_choice"].empty(), "The one who stays behind is a party story event");
            CombatSession combat;
            Json three = Json::array({{{"id", "woad_runner"}, {"count", 5}},
                                      {{"id", "drest"}, {"level", 1}, {"companion", true}},
                                      {{"id", "aldren"}, {"level", 1}, {"companion", true}},
                                      {{"id", "kharim"}, {"level", 1}, {"companion", true}}});
            check(!combat.start(data, three, {{"guards", {{{"id", "grey_wolf"}, {"count", 2}}}}}).at("ok"),
                  "A battle refuses a third companion");
        }
        {   // Combat tips are decided in C++: the snapshot carries the due tip once; a battle
            // without the speaker (wounded, absent) says nothing; resolve_encounter remembers
            // the shown ids for the campaign (tip_context.shown).
            Json army = Json::array({{{"id", "woad_runner"}, {"count", 30}}, {{"id", "drest"}, {"level", 1}, {"companion", true}}});
            Json guards = Json::array({{{"id", "grey_wolf"}, {"count", 2}}});
            auto play = [&](const Json& side, const Json& tips) {
                CombatSession combat;
                auto reply = combat.start(data, side, {{"guards", guards}, {"seed", 4}, {"tips", tips}});
                check(reply.at("ok"), "Tip battle starts");
                std::vector<std::string> seen;
                for (int turn = 0; turn < 300; ++turn) {
                    if (reply["state"]["tip"].is_object()) seen.push_back(reply["state"]["tip"]["id"]);
                    if (reply["state"]["result"] != "ongoing") break;
                    combat.acknowledge(reply["ticket"]);
                    reply = combat.command("ai");
                }
                return std::make_pair(seen, reply["state"]);
            };
            auto [heard, end] = play(army, {{"map", "1.0"}, {"shown", Json::array()}});
            check(!heard.empty() && heard[0] == "drest_battle_start", "Drest's first tip opens the battle");
            check(end["battle"]["tips"] == Json(heard), "The battle record lists the tips shown");
            std::set<std::string> unique(heard.begin(), heard.end());
            check(unique.size() == heard.size(), "No tip twice");
            Json alone = Json::array({{{"id", "woad_runner"}, {"count", 30}}});
            check(play(alone, {{"map", "1.0"}, {"shown", Json::array()}}).first.empty(),
                  "A companion who is not in the battle (wounded, absent) never speaks");
            check(play(army, {{"map", "1.0"}, {"shown", Json::array({"drest_battle_start"})}}).first.front() != "drest_battle_start",
                  "A tip shown before in the campaign is not shown again");
            check(play(army, Json::object()).first.empty(), "No tip_context: no tips");

            const std::string dir = (std::filesystem::temp_directory_path() / "umm_bridge_tips").string();
            std::filesystem::create_directories(dir);
            WorldMap field;
            field.clear(4);
            field.placeObject({{-3, 0}, ObjType::Town, "Home", 1});
            field.placeObject({{2, 0}, ObjType::Guard, "Ford Dogs", 0});
            field.setStart(HexCoord{0, 0});
            check(!field.saveJson(dir + "/field.json"), "Tips fixture map written");
            std::ofstream(dir + "/field.encounters.json") << R"({"objects": {"Ford Dogs": {"guards": [{"id": "grey_wolf", "count": 2}]}}})";
            std::ofstream(dir + "/field.triggers.json") << R"({"start_companions": ["drest"],
                "start_army": [{"id": "woad_runner", "count": 30}], "triggers": []})";
            std::ofstream(dir + "/campaign.json") << Json{{"chapters", {{{"id", "one"}, {"title", "One"}, {"maps", {
                {{"id", "1.0"}, {"title", "A"}, {"map", dir + "/field.json"}, {"encounters", dir + "/field.encounters.json"},
                 {"triggers", dir + "/field.triggers.json"}}}}}}}}.dump();
            AdventureBridge camp;
            check(camp.campaign_load(dir + "/campaign.json", data).at("ok"), "Tips campaign loads");
            auto map = camp.campaign_start_map(1);
            check(map["tip_context"]["map"] == "1.0" && map["tip_context"]["shown"].empty(), "The snapshot gives the tip context");
            auto met = camp.travel(2, 0);
            CombatSession combat;
            auto reply = combat.start(data, Json::array({{{"id", "woad_runner"}, {"count", 30}},
                                                         {{"id", "drest"}, {"level", 1}, {"companion", true}}}),
                                      {{"guards", met["encounter"]["guards"]}, {"seed", 4}, {"tips", met["tip_context"]}});
            for (int turn = 0; turn < 300 && reply["state"]["result"] == "ongoing"; ++turn) {
                combat.acknowledge(reply["ticket"]);
                reply = combat.command("ai");
            }
            auto won = camp.resolve_encounter(reply["state"]["result"] == "victory", reply["state"]["enemy_survivors"],
                                              reply["state"]["battle"]);
            check(!reply["state"]["battle"]["tips"].empty() && won["tip_context"]["shown"] == reply["state"]["battle"]["tips"],
                  "resolve_encounter remembers the battle's tips");
            bool marked = false;
            const Json afterMark = camp.tip_shown("test_tip");
            for (const auto& id : afterMark["tip_context"]["shown"]) marked |= id == "test_tip";
            check(marked, "tip_shown marks a tip shown");
            const std::string save = dir + "/tips.save.json";
            check(camp.save(save, Json::object()).at("ok"), "Tips session saves");
            AdventureBridge reloaded;
            check(reloaded.load(save)["tip_context"] == camp.snapshot()["tip_context"], "Shown tips and the map survive a save");
        }
        {   // Banter and reckless companions: a units.json "reckless" stack (Aldren) is played by the
            // session itself on its turns, in the reply of the action before (events "auto": true),
            // while the player still places him; "exchanges" carries the tip and its reply in order.
            const std::filesystem::path dir = std::filesystem::temp_directory_path() / "umm_bridge_reckless";
            std::filesystem::create_directories(dir);
            for (const auto& entry : std::filesystem::directory_iterator(data))
                if (entry.is_regular_file())
                    std::filesystem::copy_file(entry.path(), dir / entry.path().filename(), std::filesystem::copy_options::overwrite_existing);
            Json units = Json::parse(std::ifstream(dir / "units.json"));
            for (auto& u : units["units"]) if (u["id"] == "aldren") u["abilities"].push_back("reckless");
            std::ofstream(dir / "units.json") << units.dump();
            std::ofstream(dir / "combat_tips.json") << R"({"tips": [
                {"id": "b_rush", "when": "rushes", "actor": "aldren", "repeat": "battle", "speaker": "Drest",
                 "text": "Stay back behind the lines!", "maps": ["1.0"],
                 "reply": [{"speaker": "Aldren", "text": "Then keep up, old man."}, {"speaker": "Kharim", "text": "Absent."}]}]})";
            CombatSession combat;
            auto reply = combat.start(dir.string(), Json::array({{{"id", "woad_runner"}, {"count", 20}},
                                          {{"id", "drest"}, {"level", 1}, {"companion", true}},
                                          {{"id", "aldren"}, {"level", 1}, {"companion", true}}}),
                                      {{"guards", {{{"id", "levy_spearman"}, {"count", 6}}}}, {"seed", 4},
                                       {"tips", {{"map", "1.0"}, {"shown", Json::array()}}}});
            check(reply.at("ok"), "Reckless battle starts");
            std::string aldren;
            Json at;
            for (const auto& u : reply["state"]["units"])
                if (u["id"] == "aldren") { aldren = u["key"]; at = u["cell"]; check(u["reckless"] == true, "Aldren is reckless"); }
                else check(u["reckless"] == false, "Other stacks are not reckless");
            // Deployment: the player still places him.
            Json free = nullptr;
            for (const auto& cell : reply["state"]["deploy_zone"]) {
                bool taken = false;
                for (const auto& u : reply["state"]["units"]) taken |= u["cell"] == cell;
                if (!taken) { free = cell; break; }
            }
            check(!free.is_null(), "A free deployment hex");
            combat.acknowledge(reply["ticket"]);
            reply = combat.command("place_unit", free[0], free[1], at[0], at[1]);
            bool placed = false;
            for (const auto& u : reply["state"]["units"]) placed |= u["key"] == aldren && u["cell"] == free;
            check(reply.at("ok") && placed, "A reckless companion is placed like any stack");
            combat.acknowledge(reply["ticket"]);
            reply = combat.command("deploy_done");
            int charges = 0, rushes = 0;
            for (int turn = 0; turn < 300; ++turn) {
                check(reply.at("ok"), "Every order of the reckless battle is accepted");
                const Json& state = reply["state"];
                for (const auto& e : reply["events"])
                    if (e["unit"] == aldren && (e["type"] == "move" || e["type"] == "obstacle_attack"
                        || (e["type"] == "attack" && !e["retaliation"] && !e["reaction"] && !e["opportunity"]))) {
                        check(e["auto"] == true, "Aldren's own moves and blows are the session's, never the player's");
                        ++charges;
                    }
                for (const auto& x : state["exchanges"])
                    if (x["id"] == "b_rush") {
                        ++rushes;
                        check(x["lines"].size() == 2 && x["lines"][0]["speaker"] == "Drest"
                              && x["lines"][1] == Json({{"speaker", "Aldren"}, {"text", "Then keep up, old man."}})
                              && x["reply"].size() == 1 && x["after_event"].get<size_t>() <= reply["events"].size(),
                              "The exchange: Drest, then Aldren; the absent Kharim's reply is dropped");
                    }
                if (state["result"] != "ongoing") break;
                check(!(state["active"] == aldren && state["player_turn"] == true), "Aldren never waits for the player's orders");
                combat.acknowledge(reply["ticket"]);
                reply = combat.command(state["player_turn"] == true ? "defend" : "ai");   // the player holds everyone back
            }
            check(charges > 0, "Aldren charged on his own while the player held the line");
            check(rushes == 1, "Drest shouts once in the battle when Aldren rushes ahead (repeat: battle)");
            check(reply["state"]["tip"].is_null() || reply["state"]["tip"].is_object(), "tip stays the first exchange");
        }
        if (argc == 3) {   // demo_bot's campaign mode plays a fixture campaign from its story data alone
            const std::string dir = (std::filesystem::temp_directory_path() / "umm_bot_campaign").string();
            std::filesystem::create_directories(dir);
            WorldMap hunt;
            hunt.clear(6);
            hunt.placeObject({{-5, 0}, ObjType::Town, "Home", 1, ""});
            hunt.placeObject({{2, 0}, ObjType::Guard, "Ford Wolves", 0, ""});
            hunt.placeObject({{-4, 4}, ObjType::Pickup, "Old Journal", 0, "tome"});
            check(!hunt.saveJson(dir + "/hunt.json"), "Bot fixture map 1 written");
            WorldMap shrine;
            shrine.clear(6);
            shrine.placeObject({{-5, 0}, ObjType::Town, "Home", 1, ""});
            shrine.placeObject({{3, -1}, ObjType::QuestGiver, "Shrine", 0, ""});
            shrine.placeObject({{-1, 4}, ObjType::QuestGiver, "Altar", 0, ""});
            check(!shrine.saveJson(dir + "/shrine.json"), "Bot fixture map 2 written");
            std::ofstream(dir + "/hunt.encounters.json") << R"({"objects": {"Ford Wolves": {"guards": [{"id": "grey_wolf", "count": 4}]}}})";
            // The first option is locked, so the default policy takes "spare"; the wolves
            // are named only by the encounter_won trigger, never by the bot.
            std::ofstream(dir + "/hunt.triggers.json") << R"({"start_companions": [],
                "quests": [{"id": "hunt", "title": "The hunt", "text": "Drive off the wolves.", "main": true}],
                "triggers": [
                {"id": "intro", "when": {"event": "start"}, "do": [{"quest": "hunt"},
                    {"troops": [{"id": "woad_runner", "count": 30}]},
                    {"choice": {"id": "oath", "title": "Oath", "options": [
                        {"id": "burn", "label": "Burn", "requires_flag": "torch", "then": []},
                        {"id": "spare", "label": "Spare", "then": [{"flag": {"mercy": true}}]},
                        {"id": "hunt", "label": "Hunt", "then": []}]}}]},
                {"id": "won", "when": {"event": "encounter_won", "name": "Ford Wolves", "after": "intro"},
                 "do": [{"quest_done": "hunt"}, {"xp": 300}, {"ending": {"id": "ford", "heading": "The ford is clear"}}]}]})";
            // Only the flag set by map 1's choice arms one of the two endings.
            std::ofstream(dir + "/shrine.triggers.json") << R"({"start_companions": [], "triggers": [
                {"id": "shrine", "when": {"event": "visit", "name": "Shrine", "flag": {"choice.oath": "spare"}},
                 "do": [{"say": [["Priest", "The shrine remembers mercy."]]}, {"ending": {"id": "s", "heading": "Mercy"}}]},
                {"id": "altar", "when": {"event": "visit", "name": "Altar", "flag": {"choice.oath": "hunt"}},
                 "do": [{"say": [["Hunter", "Blood for the altar."]]}, {"ending": {"id": "a", "heading": "Blood"}}]}]})";
            std::ofstream(dir + "/campaign.json") << Json{{"chapters", {{{"id", "one"}, {"title", "One"}, {"maps", {
                {{"id", "1.0"}, {"title", "The hunt"}, {"map", dir + "/hunt.json"}, {"encounters", dir + "/hunt.encounters.json"},
                 {"triggers", dir + "/hunt.triggers.json"}, {"max_level", 2}},
                {{"id", "1.1"}, {"title", "The shrine"}, {"map", dir + "/shrine.json"}, {"triggers", dir + "/shrine.triggers.json"}}}}}}}}.dump();
            auto bot = [&](const std::string& args, std::string& out) {
                const std::string cmd = "\"" + std::string(argv[2]) + "\" \"" + data + "/..\" campaign \"" + dir + "/campaign.json\" " + args + " 2>/dev/null";
                FILE* pipe = popen(cmd.c_str(), "r");
                check(pipe, "demo_bot runs");
                char buf[512];
                while (fgets(buf, sizeof buf, pipe)) out += buf;
                const int status = pclose(pipe);
                return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            };
            auto has = [](const std::string& out, const std::string& text) { return out.find(text) != std::string::npos; };
            std::string hasty;
            check(bot("hasty 1", hasty) == 0, "Hasty campaign route exits 0");
            check(has(hasty, "MAP 1.0 The hunt\n") && has(hasty, "MAP 1.1 The shrine\n"), "Each map announced");
            check(has(hasty, "CHOICE oath=spare"), "Default policy takes the first unlocked option");
            check(has(hasty, "CAP map=1.0 cap=2 reached_day=1 map_days=1 xp_lost="), "Map 1's cap reported");
            check(has(hasty, "trivial=yes\nMAP 1.1"), "Reaching the cap on day 1 is trivial");
            check(has(hasty, "CAP map=1.1 cap=10 reached_day=never map_days="), "No cap reached on map 2");
            check(has(hasty, "CAMPAIGN result=FINISHED map=1.1 day="), "Campaign finished");
            check(!has(hasty, "Old Journal"), "Hasty skips side pickups");
            std::string thorough;
            check(bot("thorough 1 --transcript --choose oath=hunt", thorough) == 0, "Thorough campaign route exits 0");
            check(has(thorough, "MAP 1.0 The hunt\n") && has(thorough, "MAP 1.1 The shrine\n"), "Transcript carries the title cards");
            check(has(thorough, "VISIT Old Journal"), "Thorough takes the pickup");
            check(has(thorough, "CHOSE Hunt"), "--choose overrides the default");
            check(has(thorough, "SAY Hunter: Blood for the altar.") && !has(thorough, "Priest"),
                  "Map 2's flag-gated trigger follows map 1's choice");
            check(!has(thorough, "CAP ") && !has(thorough, "GOAL "), "Transcript holds only player-visible lines");

            // A conversation at the engage: the bot answers before it fights. The first
            // option sends the warrior away (no battle); a forced "fight" fights after it.
            WorldMap ridge;
            ridge.clear(6);
            ridge.placeObject({{-5, 0}, ObjType::Town, "Home", 1, ""});
            ridge.placeObject({{2, 0}, ObjType::Guard, "Northern Warrior", 0, ""});
            check(!ridge.saveJson(dir + "/ridge.json"), "Bot engage fixture map written");
            std::ofstream(dir + "/ridge.encounters.json") << R"({"objects": {"Northern Warrior": {"guards": [{"id": "grey_wolf", "count": 2}]}}})";
            std::ofstream(dir + "/ridge.triggers.json") << R"({"start_companions": [],
                "start_army": [{"id": "woad_runner", "count": 30}],
                "triggers": [
                {"id": "warrior", "when": {"event": "engage", "name": "Northern Warrior"},
                 "do": [{"choice": {"id": "warrior", "presentation": "conversation", "options": [
                    {"id": "give_tag", "label": "Hand him the tag", "then": [{"withdraw": "Northern Warrior"}]},
                    {"id": "fight", "label": "Fight", "then": []}]}}]},
                {"id": "past", "when": {"event": "cleared", "names": ["Northern Warrior"]},
                 "do": [{"ending": {"id": "ridge", "heading": "The way north"}}]}]})";
            std::ofstream(dir + "/ridge_campaign.json") << Json{{"chapters", {{{"id", "one"}, {"title", "One"}, {"maps", {
                {{"id", "1.2"}, {"title", "The ridge"}, {"map", dir + "/ridge.json"}, {"encounters", dir + "/ridge.encounters.json"},
                 {"triggers", dir + "/ridge.triggers.json"}}}}}}}}.dump();
            auto ridgeBot = [&](const std::string& args, std::string& out) {
                const std::string cmd = "\"" + std::string(argv[2]) + "\" \"" + data + "/..\" campaign \"" + dir + "/ridge_campaign.json\" " + args + " 2>/dev/null";
                FILE* pipe = popen(cmd.c_str(), "r");
                check(pipe, "demo_bot runs");
                char buf[512];
                while (fgets(buf, sizeof buf, pipe)) out += buf;
                const int status = pclose(pipe);
                return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            };
            std::string spared;
            check(ridgeBot("hasty 1 --transcript", spared) == 0, "Engage fixture campaign exits 0");
            check(has(spared, "CHOSE Hand him the tag"), "The default answer at the engage is the first option");
            check(!has(spared, "BATTLE"), "Answered first, the warrior leaves without a fight");
            std::string fought;
            check(ridgeBot("hasty 1 --transcript --choose warrior=fight", fought) == 0, "Forced-fight campaign exits 0");
            const auto chose = fought.find("CHOSE Fight"), battle = fought.find("BATTLE Northern Warrior -> victory");
            check(chose != std::string::npos && battle != std::string::npos && chose < battle,
                  "With 'fight' chosen the bot answers, then fights");
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
                    check(CombatMap().inBounds(cell) && previous.distanceTo(cell) == 1,
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

        {   // Rabies: a bitten fighter shows its mark, may go mad (refuses orders; "ai" plays it)
            // and leaves the battle still bitten.
            bool sawMad = false, sawSurvivor = false;
            for (unsigned seed = 0; seed < 300 && !(sawMad && sawSurvivor); ++seed) {
                CombatSession rabid;
                const Json fighters = {{{"id", "woad_runner"}, {"count", 30}, {"bitten", true}},
                                       {{"id", "cruth_slinger"}, {"count", 5}}};
                auto turn = rabid.start(argv[1], fighters, {{"guards", {{{"id", "grey_wolf"}, {"count", 4}}}}, {"seed", seed}});
                check(turn.at("ok"), "A bitten army starts a battle");
                check(turn["state"]["units"][0]["bitten"] == true && turn["state"]["units"][1]["bitten"] == false
                      && turn["state"]["units"][0]["mad"].is_boolean(), "Units report bitten and mad");
                for (int n = 0; n < 300 && turn["state"]["result"] == "ongoing"; ++n) {
                    rabid.acknowledge(turn["ticket"]);
                    const auto& state = turn["state"];
                    if (state["mad_turn"] == true) {
                        sawMad = true;
                        check(state["active"] == "p0" && state["player_turn"] == false, "A mad turn is not the player's");
                        bool announced = false;
                        for (const auto& event : turn["events"]) announced |= event["type"] == "madness" && event["unit"] == "p0";
                        check(announced, "The madness event names the stack");
                        for (const auto& unit : state["units"]) if (unit["key"] == "p0") check(unit["mad"] == true, "The mad unit is marked");
                        check(!rabid.command("defend").at("ok"), "A mad stack refuses orders");
                        check(!rabid.command_route("move", Json::array({Json::array({1, 1})})).at("ok"), "A mad stack refuses a route");
                    }
                    turn = rabid.command("ai");
                    check(turn.at("ok"), "The AI plays every turn, mad or not");
                }
                if (turn["state"]["result"] == "victory")
                    for (const auto& s : turn["state"]["survivors"])
                        if (s["id"] == "woad_runner") { sawSurvivor = true; check(s["bitten"] == true, "Survivors keep the bite"); }
                        else check(s["bitten"] == false, "Clean survivors stay clean");
            }
            check(sawMad && sawSurvivor, "Madness seen and a bitten stack survived");

            const std::string dir = (std::filesystem::temp_directory_path() / "umm_bridge_rabies").string();
            std::filesystem::create_directories(dir);
            WorldMap field;
            field.clear(3);
            field.placeObject({{-2, 0}, ObjType::Town, "Home", 1, ""});
            check(!field.saveJson(dir + "/home.json"), "Rabies fixture map written");
            AdventureBridge home;
            check(home.start(dir + "/home.json", data).at("ok"), "Rabies fixture starts in the player's town");
            auto snap = home.set_army(Json::parse(R"([{"id": "woad_runner", "count": 5, "bitten": true}])"));
            check(snap["army"][0]["bitten"] == true && snap["opened"].is_array(), "set_army takes the bite; the snapshot shows it");
            auto night = home.end_day();
            check(night["tended"] == "Home" && night["army"][0]["bitten"] == false
                  && night["event"].get<std::string>().find("The bitten are tended in Home.") != std::string::npos,
                  "A night in your own town tends the bitten");
        }

        {   // Deployment is always open at the start: units move inside the zone, then any order begins the battle.
            CombatSession deploy;
            const Json party = {{{"id", "woad_runner"}, {"count", 10}}, {{"id", "painted_blade"}, {"count", 4}},
                                {{"id", "ushari"}, {"level", 1}, {"companion", true}}};
            reply = deploy.start(argv[1], party, guards);
            check(reply.at("ok"), "A plain battle starts");
            auto state = reply["state"];
            check(state["deploying"] == true && state["unit_placement"] == true, "Every battle opens with deployment");
            check(state["layout"] == "field" && state["width"] == 11 && state["height"] == 5, "Default layout is the 11 x 5 field");
            check(state["deploy_zone"].size() == 10 && state["fieldwork_zone"].size() == 15, "Field zones: cols 0-1, fieldworks cols 2-4");
            check(state["obstacles"].empty() && state["first_move"] == "normal" && state["enemy_first"] == false, "The field is open; initiative decides");
            check(state["units"][2]["cell"] == Json::array({0, 2}), "Ushari still starts at the back, centre");
            deploy.acknowledge(reply["ticket"]);
            const auto runner = state["units"][0]["cell"];
            check(!deploy.command("place_unit", 2, 2, runner[0], runner[1]).at("ok"), "Cannot place a unit outside the deployment zone");
            check(!deploy.command("place_unit", 0, 0, 5, 5).at("ok"), "place_unit names one of your stacks");
            reply = deploy.command("place_unit", 0, 0, runner[0], runner[1]);
            check(reply.at("ok") && reply["state"]["units"][0]["cell"] == Json::array({0, 0}), "A unit moves inside the zone");
            deploy.acknowledge(reply["ticket"]);
            const auto blade = reply["state"]["units"][1]["cell"];
            reply = deploy.command("place_unit", blade[0], blade[1], 0, 0);
            check(reply.at("ok") && reply["state"]["units"][0]["cell"] == blade
                  && reply["state"]["units"][1]["cell"] == Json::array({0, 0}), "Placing onto your own stack swaps the two");
            deploy.acknowledge(reply["ticket"]);
            check(!deploy.command("move", 100, 100).at("ok") && deploy.snapshot()["deploying"] == true,
                  "A refused order leaves the deployment open");
            reply = deploy.command("ai");
            check(reply.at("ok") && reply["state"]["deploying"] == false && reply["state"]["unit_placement"] == false,
                  "With nothing to build, any order accepts the placement");
            deploy.acknowledge(reply["ticket"]);
            check(!deploy.command("place_unit", 0, 1, 0, 0).at("ok"), "No placement once the battle has begun");
        }
        {   // Layouts from data: the corridor ambush (the proposed tests/fixtures file in a copy of the data dir).
            const auto dir = std::filesystem::temp_directory_path() / "umm_bridge_layouts";
            std::filesystem::remove_all(dir);
            std::filesystem::create_directories(dir);
            for (const auto& entry : std::filesystem::directory_iterator(argv[1]))
                if (entry.is_regular_file() && entry.path().extension() == ".json")
                    std::filesystem::copy_file(entry.path(), dir / entry.path().filename());
            std::filesystem::copy_file(std::filesystem::path(argv[1]) / ".." / "tests" / "fixtures" / "combat_layouts.json",
                                       dir / "combat_layouts.json", std::filesystem::copy_options::overwrite_existing);
            CombatSession unknown;
            Json nowhere = guards;
            nowhere["layout"] = "nowhere";
            check(!unknown.start(dir.string(), army, nowhere).at("ok"), "An unknown layout is refused");
            CombatSession ambush;
            Json thicket = {{"guards", {{{"id", "feral_dog"}, {"count", 3}}, {{"id", "feral_dog"}, {"count", 3}}}},
                            {"layout", "corridor_ambush"}, {"seed", 4}};
            const Json party = {{{"id", "painted_blade"}, {"count", 12}}, {{"id", "woad_runner"}, {"count", 10}},
                                {{"id", "drest"}, {"level", 1}, {"companion", true}}};
            reply = ambush.start(dir.string(), party, thicket);
            check(reply.at("ok"), "The corridor ambush starts");
            auto state = reply["state"];
            check(state["layout"] == "corridor_ambush" && state["width"] == 13 && state["height"] == 9, "Corridor size");
            check(state["enemy_first"] == true && state["first_move"] == "enemy_first", "The dogs act first");
            check(state["active"].get<std::string>()[0] == 'e', "An enemy stack opens the battle");
            check(!state["obstacles"].empty(), "Trees stand in the corridor");
            for (const auto& o : state["obstacles"])
                check(o["kind"] == "tree" && o["hp"] == o["max_hp"] && o["hp"].get<int>() > 0 && o["cell"].is_array(), "Obstacles report kind and hit points");
            for (const auto& u : state["units"])
                if (u["player"] == true) {
                    bool inZone = false;
                    for (const auto& c : state["deploy_zone"]) inZone |= c == u["cell"];
                    check(inZone, "The party deploys in the lane");
                }
            ambush.acknowledge(reply["ticket"]);
            // Play the dogs' opening, then break a tree with our first stack that can.
            bool broke = false;
            for (int n = 0; n < 60 && !broke && ambush.snapshot()["result"] == "ongoing"; ++n) {
                const auto now = ambush.snapshot();
                if (now["player_turn"] == true && !now["attackable_obstacles"].empty()) {
                    const auto tree = now["attackable_obstacles"][0];
                    bool listed = false;
                    for (const auto& c : now["attackable"]) listed |= c == tree;
                    check(listed, "Breakable obstacles are listed in attackable");
                    const auto& preview = now["obstacle_previews"][0];
                    check(preview["cell"] == tree && !preview["options"].empty() && preview["best"].is_array()
                          && preview["damage_max"].get<int>() >= preview["damage_min"].get<int>(), "Obstacle previews list standing hexes and damage");
                    int before = 0;
                    for (const auto& o : now["obstacles"]) if (o["cell"] == tree) before = o["hp"];
                    reply = ambush.command("attack", tree[0], tree[1]);
                    check(reply.at("ok"), "Attack on an obstacle accepted");
                    bool struck = false, hurt = false;
                    for (const auto& event : reply["events"]) {
                        struck |= event["type"] == "obstacle_attack" && event["cell"] == tree;
                        hurt |= event["type"] == "obstacle_damage" && event["cell"] == tree && event["damage"].get<int>() > 0;
                    }
                    check(struck && hurt, "Obstacle attack events name the hex");
                    int after = 0;
                    for (const auto& o : reply["state"]["obstacles"]) if (o["cell"] == tree) after = o["hp"];
                    check(after < before, "The obstacle loses hit points (0 = gone)");
                    broke = true;
                } else reply = ambush.command(now["player_turn"] == true ? "defend" : "ai");
                check(reply.at("ok"), "Ambush actions accepted");
                ambush.acknowledge(reply["ticket"]);
            }
            check(broke, "A player stack could break a tree in the ambush");
        }

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
