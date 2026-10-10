#pragma once
#include "combat/CombatEngine.h"
#include "combat/CombatTips.h"
#include "hero/Hero.h"
#include "core/ResourceManager.h"
#include <nlohmann/json.hpp>
#include <memory>

// Owns the registry for longer than the engine's non-owning UnitType pointers.
// No Godot, SDL, or rendering dependency: also exercised by native tests.
class CombatSession {
public:
    using Json = nlohmann::json;
    // army: up to five troop stacks and two companions. encounter: {guards, seed?,
    // reward?, fieldworks?, tactics?, tips?, layout?}; layout = a combat_layouts.json
    // id (default "field"). Every battle opens deploying ("deploying": true): the
    // player may "place_unit" in "deploy_zone" and build fieldworks, then
    // "deploy_done". With no fieldworks to build, any battle order (or "ai")
    // accepts the placement as it stands. tips = the adventure snapshot's
    // "tip_context" {map, shown}. The snapshot's "tip" is {id, speaker, text} in the
    // reply of the action that made a companion tip due (null otherwise; decided by
    // CombatTipWatch, never twice), and "battle"."tips" lists the ids shown so far.
    // "exchanges" lists every tip due in this reply, in order (usually 0 or 1; "tip"
    // is the first): {id, speaker, text, reply: [{speaker, text}], lines: the whole
    // exchange in order [{speaker, text}], after_event: how many of the reply's
    // events come before it}. A reply line whose speaker is not standing is dropped.
    // Reckless stacks (units.json ability "reckless"): on such a player stack's turn
    // the session plays it itself (CombatAI) right after the action before it, in
    // the same reply (its events carry "auto": true); orders for it are refused.
    // Units carry "reckless"; "auto_turn" is true (and "player_turn" false) only
    // while one waits, which only a pending character choice can cause.
    Json start(const std::string& data_dir, const Json& army, const Json& encounter);
    // action: move | attack | strike | defend | retreat | ai.  "strike" attacks
    // the enemy on (q, r) after walking to the standing hex (fq, fr).
    // "attack" / "strike" on an obstacle's hex attack the obstacle (melee from
    // next to it, or a shot); the snapshot lists those hexes in "attackable"
    // and "attackable_obstacles", and every obstacle in "obstacles".
    // Deployment: deploy_done | place_unit (your stack on (fq, fr) to (q, r);
    // swaps with your own stack there) | place_barricade | place_stakes |
    // remove_fieldwork (on (q, r)).
    // A mad stack (rabies) refuses orders: the snapshot says "mad_turn" and
    // "player_turn": false, and the caller plays it with "ai".
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
    // {result, losses: {player, enemy}, total_damage: {player, enemy}, max_hit, per_stack}
    Json battle() const;
    Json previews() const;
    Json obstacle_preview(HexCoord cell) const;   // {cell, ranged, damage_min/max, best, options[{from, path, reactions}]}
    Json reactions(HexCoord to) const;   // enemy shooters that would fire as the active stack walks to `to`
    Json movement_path(HexCoord from, HexCoord to, bool player, int index) const;
    Json response();
    bool auto_turn() const;   // the active stack is the player's and reckless, and the battle is under way
    Json failure(const std::string& error) const;
    int fieldwork_stock() const { return barricades_ + stakes_; }
    std::unique_ptr<ResourceManager> resources_;
    std::unique_ptr<CombatEngine> engine_;
    std::string reward_;
    bool awaiting_animation_ = false;
    bool deploying_ = false;
    int barricades_ = 0, stakes_ = 0;
    int opening_ = 0;   // Tactics rank: stacks the player may order first, until the orders are given
    int64_t ticket_ = 0;
    CombatTipWatch tips_;
    Json tip_ = nullptr;   // the tip due after the last action
    Json exchanges_ = Json::array();   // every tip due in the last reply, in order
};
