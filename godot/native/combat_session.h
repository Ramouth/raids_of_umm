#pragma once
#include "combat/CombatEngine.h"
#include "core/ResourceManager.h"
#include <nlohmann/json.hpp>
#include <memory>

// Owns the registry for longer than the engine's non-owning UnitType pointers.
// No Godot, SDL, or rendering dependency: also exercised by native tests.
class CombatSession {
public:
    using Json = nlohmann::json;
    Json start(const std::string& data_dir, const Json& army, const Json& encounter);
    // action: move | attack | strike | defend | retreat | ai.  "strike" attacks
    // the enemy on (q, r) after walking to the standing hex (fq, fr).
    Json command(const std::string& action, int q = 0, int r = 0, int fq = 0, int fr = 0);
    // Player-chosen route: walk `route` ([[q, r], ...], ending on the
    // destination), then for "strike" attack the enemy on (q, r).
    // "opening" (Tactics): `route` lists the hexes of the player's stacks that
    // act first, in order (empty = no orders); only before anyone has acted.
    Json command_route(const std::string& action, const Json& route, int q = 0, int r = 0);
    Json snapshot() const;
    bool acknowledge(int64_t ticket);

private:
    CombatArmy make_army(const Json& input, bool player) const;
    std::vector<HexCoord> legal_targets() const;
    Json next_round() const;
    Json previews() const;
    Json reactions(HexCoord to) const;   // enemy shooters that would fire as the active stack walks to `to`
    Json movement_path(HexCoord from, HexCoord to, bool player, int index) const;
    Json response();
    Json failure(const std::string& error) const;
    std::unique_ptr<ResourceManager> resources_;
    std::unique_ptr<CombatEngine> engine_;
    std::string reward_;
    bool awaiting_animation_ = false;
    int opening_ = 0;   // Tactics rank: stacks the player may order first, until the orders are given
    int64_t ticket_ = 0;
};
