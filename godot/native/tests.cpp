#include "combat_session.h"
#include <iostream>
#include <stdexcept>
using Json = CombatSession::Json;
static void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
int main(int argc, char** argv) {
    try {
        check(argc == 2, "Pass the data directory");
        const Json guards = {{"guards", {{{"id", "skeleton_warrior"}, {"count", 12}},
            {{"id", "sand_scorpion"}, {"count", 4}}}}, {"reward", "scarab_amulet"}};
        const Json army = {{{"id", "desert_archer"}, {"count", 10}}, {{"id", "mummy"}, {"count", 3}}};
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
        check(reply["state"]["survivors"][0]["count"] == 1 && reply["state"]["rewards"].empty(), "Retreat preserves survivors without loot");

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
