#include "CombatLayout.h"
#include "CombatMap.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <queue>
#include <unordered_set>

CombatLayout CombatLayout::field() {
    CombatLayout out;
    // Back column centre-out (row 2, 1, 3, 0, 4), then the column in front of it.
    for (int col : {0, 1})        for (int row : {2, 1, 3, 0, 4}) out.playerZone.push_back(CombatMap::toHex(col, row));
    for (int col : {10, 9})       for (int row : {2, 1, 3, 0, 4}) out.enemyZone.push_back(CombatMap::toHex(col, row));
    for (int col : {2, 3, 4})     for (int row = 0; row < 5; ++row) out.fieldworkZone.push_back(CombatMap::toHex(col, row));
    return out;
}

const char* CombatLayout::firstMoveName(FirstMove f) {
    switch (f) {
    case FirstMove::EnemyFirst:  return "enemy_first";
    case FirstMove::PlayerFirst: return "player_first";
    default:                     return "normal";
    }
}

const CombatLayout* CombatLayouts::find(const std::string& id) const {
    for (const auto& l : layouts) if (l.id == id) return &l;
    return nullptr;
}

const ObstacleKind* CombatLayouts::kind(const std::string& id) const {
    auto it = kinds.find(id);
    return it != kinds.end() ? &it->second : nullptr;
}

std::optional<std::string> validateCombatLayout(const CombatLayout& layout,
                                                const std::map<std::string, ObstacleKind>& kinds) {
    const std::string where = "combat layout '" + layout.id + "': ";
    if (layout.id.empty()) return std::string("a combat layout has no id");
    if (layout.width < 2 || layout.height < 2 || layout.width > 40 || layout.height > 40)
        return where + "size must be between 2 and 40 hexes each way";
    const CombatMap map(layout.width, layout.height);
    std::unordered_set<HexCoord> blocked;
    for (const auto& o : layout.obstacles) {
        if (!map.inBounds(o.cell)) return where + "an obstacle lies outside the grid";
        if (!kinds.count(o.kind)) return where + "unknown obstacle kind '" + o.kind + "'";
        if (!blocked.insert(o.cell).second) return where + "two obstacles share a hex";
    }
    auto checkZone = [&](const std::vector<HexCoord>& zone, const char* name, size_t least) -> std::optional<std::string> {
        std::unordered_set<HexCoord> seen;
        for (const auto& c : zone) {
            if (!map.inBounds(c)) return where + name + " has a hex outside the grid";
            if (blocked.count(c)) return where + name + " has a hex under an obstacle";
            if (!seen.insert(c).second) return where + name + " lists a hex twice";
        }
        if (zone.size() < least)
            return where + name + " needs at least " + std::to_string(least) + " hexes";
        return std::nullopt;
    };
    if (auto e = checkZone(layout.playerZone, "player_zone", CombatLayout::MAX_STACKS)) return e;
    if (auto e = checkZone(layout.enemyZone, "enemy_zone", CombatLayout::MAX_STACKS)) return e;
    if (auto e = checkZone(layout.fieldworkZone, "fieldwork_zone", 0)) return e;
    for (const auto& c : layout.enemyZone) {
        if (std::find(layout.playerZone.begin(), layout.playerZone.end(), c) != layout.playerZone.end())
            return where + "player_zone and enemy_zone overlap";
        if (std::find(layout.fieldworkZone.begin(), layout.fieldworkZone.end(), c) != layout.fieldworkZone.end())
            return where + "fieldwork_zone overlaps enemy_zone";
    }
    // Every open hex reachable from every other: no pocket a stack can never leave.
    const auto all = map.allHexes();
    auto start = std::find_if(all.begin(), all.end(), [&](HexCoord c) { return !blocked.count(c); });
    if (start == all.end()) return where + "has no open hex";
    std::unordered_set<HexCoord> reached{*start};
    std::queue<HexCoord> frontier;
    frontier.push(*start);
    while (!frontier.empty()) {
        const HexCoord at = frontier.front(); frontier.pop();
        for (int d = 0; d < 6; ++d) {
            const HexCoord next = at.neighbor(d);
            if (!map.inBounds(next) || blocked.count(next) || reached.count(next)) continue;
            reached.insert(next); frontier.push(next);
        }
    }
    if (reached.size() + blocked.size() != all.size()) return where + "obstacles cut the open hexes apart";
    return std::nullopt;
}

std::optional<std::string> parseCombatLayouts(const std::string& text, CombatLayouts& out) {
    using json = nlohmann::json;
    out = {};
    try {
        const json root = json::parse(text);
        const json kinds = root.value("obstacle_kinds", json::object());
        for (const auto& [id, k] : kinds.items()) {
            ObstacleKind kind{id, k.value("hp", 0), k.value("opaque", true)};
            if (kind.hp <= 0) return "obstacle kind '" + id + "' needs hit points above 0";
            out.kinds[id] = kind;
        }
        auto cell = [](const json& c) { return CombatMap::toHex(c.at(0).get<int>(), c.at(1).get<int>()); };
        auto cells = [&](const json& list) {
            std::vector<HexCoord> v;
            for (const auto& c : list) v.push_back(cell(c));
            return v;
        };
        for (const auto& j : root.value("layouts", json::array())) {
            CombatLayout l;
            l.id = j.value("id", "");
            l.width = j.value("width", 0);
            l.height = j.value("height", 0);
            for (const auto& o : j.value("obstacles", json::array())) {
                CombatLayout::Obstacle ob{cell(o.at("cell")), o.at("kind").get<std::string>()};
                if (const ObstacleKind* k = out.kind(ob.kind)) { ob.hp = k->hp; ob.opaque = k->opaque; }
                l.obstacles.push_back(std::move(ob));
            }
            l.playerZone = cells(j.value("player_zone", json::array()));
            l.enemyZone = cells(j.value("enemy_zone", json::array()));
            l.fieldworkZone = j.contains("fieldwork_zone") ? cells(j.at("fieldwork_zone")) : l.playerZone;
            const std::string first = j.value("first_move", "normal");
            if (first == "normal")            l.firstMove = CombatLayout::FirstMove::Normal;
            else if (first == "enemy_first")  l.firstMove = CombatLayout::FirstMove::EnemyFirst;
            else if (first == "player_first") l.firstMove = CombatLayout::FirstMove::PlayerFirst;
            else return "combat layout '" + l.id + "': first_move must be normal, enemy_first or player_first";
            if (auto e = validateCombatLayout(l, out.kinds)) return e;
            if (out.find(l.id)) return "combat layout '" + l.id + "' is defined twice";
            out.layouts.push_back(std::move(l));
        }
    } catch (const json::exception& e) {
        return std::string("combat layouts: ") + e.what();
    }
    return std::nullopt;
}
