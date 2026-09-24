// battle_sim — balance tool: auto-battles a player army against guards.
// Usage: battle_sim <data_dir> '<army json>' '<guards json>' [runs]
// Prints win rate, average surviving stacks, and how often companions fell
// (army entries may include {"id": "ushari", "level": 3, "companion": true}).
// Both sides use CombatAI.  With HOLD=1 in the environment, player companions
// only defend (a player who keeps them behind the line).
#include "combat_session.h"
#include <cstdlib>
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
    int wins = 0, fell = 0, fellRounds = 0;
    Json lastSurvivors;
    for (int i = 0; i < runs; ++i) {
        CombatSession session;
        auto reply = session.start(argv[1], army, guards);
        if (!reply.value("ok", false)) { std::cerr << reply.dump() << "\n"; return 1; }
        session.acknowledge(reply["ticket"]);
        for (int n = 0; n < 2000 && session.snapshot()["result"] == "ongoing"; ++n) {
            const auto now = session.snapshot();
            bool hold = std::getenv("HOLD") && now["player_turn"].get<bool>();
            if (hold) {
                hold = false;
                for (const auto& u : now["units"])
                    if (u["key"] == now["active"] && u["companion"].get<bool>()) hold = true;
            }
            reply = session.command(hold ? "defend" : "ai");
            if (!reply.value("ok", false)) break;
            session.acknowledge(reply["ticket"]);
        }
        auto state = session.snapshot();
        if (state["result"] == "victory") { ++wins; lastSurvivors = state["survivors"]; }
        if (!state["fallen"].empty()) { ++fell; fellRounds += state["round"].get<int>(); }
    }
    std::cout << "win " << wins << "/" << runs << "  survivors(last win): " << lastSurvivors.dump();
    if (fell) std::cout << "  companion fell " << fell << "/" << runs;
    std::cout << "\n";
}
