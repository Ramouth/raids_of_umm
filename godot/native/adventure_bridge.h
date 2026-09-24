#pragma once
#include "adventure/AdventureSession.h"
#include <nlohmann/json.hpp>

// JSON facade over AdventureSession for the GDExtension (no Godot types here,
// so native tests can drive it too). Cells travel as [q, r] pairs.
class AdventureBridge {
public:
    using Json = nlohmann::json;
    Json start(const std::string& map_path, const std::string& data_dir,
               const std::string& encounters_path = "", uint32_t seed = 0,
               const std::string& triggers_path = "");
    Json snapshot() const;
    Json preview(int q, int r) const;
    Json travel(int q, int r);
    Json end_day();
    Json resolve_encounter(bool victory);
    Json set_army(const Json& stacks);
    Json recruit(int q, int r, const std::string& unit_id, int count);
    Json accept_offer(const std::string& id);
    Json transfer(int q, int r, const std::string& unit_id, int count, bool to_garrison);
    Json station(const std::string& id, bool stay);
    // Writes the session (+ caller extras, e.g. Godot inventory) to path.
    Json save(const std::string& path, const Json& extra);
    Json load(const std::string& path);   // snapshot + "extra"
    Json add_item(const std::string& id);
    Json claim_chest(bool gold);          // snapshot + "found"

private:
    Json with_lines(Json out);  // attaches dialogue produced by the last action
    AdventureSession session_;
};
