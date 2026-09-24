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
    Json command(const std::string& action, int q = 0, int r = 0);
    Json snapshot() const;
    bool acknowledge(int64_t ticket);

private:
    CombatArmy make_army(const Json& input, bool player) const;
    std::vector<HexCoord> legal_targets() const;
    Json next_round() const;
    Json previews() const;
    Json movement_path(HexCoord from, HexCoord to, bool player, int index) const;
    Json response();
    Json failure(const std::string& error) const;
    std::unique_ptr<ResourceManager> resources_;
    std::unique_ptr<CombatEngine> engine_;
    std::string reward_;
    bool awaiting_animation_ = false;
    int64_t ticket_ = 0;
};
