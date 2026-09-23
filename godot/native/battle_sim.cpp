// battle_sim — balance tool: auto-battles a player army against guards.
// Usage: battle_sim <data_dir> '<army json>' '<guards json>' [runs]
// Prints win rate and average surviving stacks. Both sides use CombatAI.
#include "combat_session.h"
#include <iostream>

using Json = CombatSession::Json;

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "usage: battle_sim <data_dir> '<army>' '<guards>' [runs]\n";
        return 2;
    }
    const Json army   = Json::parse(argv[2]);
    const Json guards = {{"guards", Json::parse(argv[3])}};
    const int  runs   = argc > 4 ? std::stoi(argv[4]) : 20;
    int wins = 0;
    Json lastSurvivors;
    for (int i = 0; i < runs; ++i) {
        CombatSession session;
        auto reply = session.start(argv[1], army, guards);
        if (!reply.value("ok", false)) { std::cerr << reply.dump() << "\n"; return 1; }
        session.acknowledge(reply["ticket"]);
        for (int n = 0; n < 2000 && session.snapshot()["result"] == "ongoing"; ++n) {
            reply = session.command("ai");
            if (!reply.value("ok", false)) break;
            session.acknowledge(reply["ticket"]);
        }
        auto state = session.snapshot();
        if (state["result"] == "victory") { ++wins; lastSurvivors = state["survivors"]; }
    }
    std::cout << "win " << wins << "/" << runs << "  survivors(last win): " << lastSurvivors.dump() << "\n";
}
