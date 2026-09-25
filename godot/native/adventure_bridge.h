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
    Json build(int q, int r, const std::string& id);
    Json equip(const std::string& id);       // commander's paper doll
    Json unequip(const std::string& slot);                              // one per town per day
    Json trade(const std::string& give, const std::string& get, int amount);     // marketplace
    Json trade_quote(const std::string& give, const std::string& get, int amount) const;
    // Companions who fell in the last battle: wounded, or gone if it was lost.
    Json companions_fell(const Json& fallen, bool lost);
    Json recruit(int q, int r, const std::string& unit_id, int count);
    Json accept_offer(const std::string& id);
    Json choose_story(const std::string& id);
    Json transfer(int q, int r, const std::string& unit_id, int count, bool to_garrison);
    Json station(const std::string& id, bool stay);
    // Writes the session (+ caller extras, e.g. Godot inventory) to path.
    Json save(const std::string& path, const Json& extra);
    Json load(const std::string& path);   // snapshot + "extra"
    Json add_item(const std::string& id);
    Json buy_fieldwork(const std::string& kind);
    Json choose_path(const std::string& id);    // the commander's path, chosen once at level 2
    Json join_special(const std::string& id);   // a companion joins (tests, story tools)
    Json claim_chest(bool gold);          // snapshot + "found"

private:
    Json with_lines(Json out);  // attaches dialogue produced by the last action
    AdventureSession session_;
};
