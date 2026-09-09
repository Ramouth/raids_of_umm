#include "combat_session.h"
#include "combat/CombatAI.h"
#include <algorithm>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

using Json = CombatSession::Json;
namespace {
Json hex(HexCoord cell) { return {cell.q, cell.r}; }
std::string key(bool player, int index) { return std::string(player ? "p" : "e") + std::to_string(index); }
const char* result_name(CombatResult result) {
    switch (result) {
    case CombatResult::PlayerWon: return "victory";
    case CombatResult::EnemyWon: return "defeat";
    case CombatResult::Retreated: return "retreat";
    default: return "ongoing";
    }
}
}

CombatArmy CombatSession::make_army(const Json& input, bool player) const {
    if (!input.is_array() || input.empty() || input.size() > CombatMap::GRID_H)
        throw std::runtime_error("An army must contain between one and five stacks.");
    CombatArmy army;
    army.isPlayer = player;
    army.ownerName = player ? "Expedition" : "Dungeon guards";
    for (const auto& entry : input) {
        const auto id = entry.at("id").get<std::string>();
        const int count = entry.at("count").get<int>();
        const UnitType* type = resources_->unit(id);
        if (!type || count <= 0 || count > 10000)
            throw std::runtime_error("Unknown unit or invalid stack size: " + id);
        army.stacks.push_back(CombatUnit::make(type, count, player));
    }
    return army;
}

Json CombatSession::start(const std::string& data_dir, const Json& army, const Json& encounter) {
    if (engine_ && (!engine_->isOver() || awaiting_animation_))
        return failure("Finish the current battle first.");
    engine_.reset();
    resources_ = std::make_unique<ResourceManager>();
    try {
        if (auto error = resources_->load(data_dir)) return failure(*error);
        reward_ = encounter.value("reward", std::string{});
        if (!reward_.empty() && !resources_->item(reward_))
            return failure("Unknown encounter reward: " + reward_);
        auto player = make_army(army, true);
        auto enemy = make_army(encounter.at("guards"), false);
        engine_ = std::make_unique<CombatEngine>(std::move(player), std::move(enemy));
        return response();
    } catch (const std::exception& error) {
        engine_.reset();
        return failure(error.what());
    }
}

std::vector<HexCoord> CombatSession::legal_targets() const {
    if (!engine_ || engine_->isOver()) return {};
    const auto& actor = engine_->activeUnit();
    const auto& enemies = actor.isPlayer ? engine_->enemyArmy() : engine_->playerArmy();
    std::vector<HexCoord> result;
    for (const auto& unit : enemies.stacks) {
        if (!unit.isDead() && (actor.pos.distanceTo(unit.pos) == 1 ||
            (actor.type->isRanged() && actor.shotsLeft > 0))) result.push_back(unit.pos);
    }
    return result;
}

Json CombatSession::failure(const std::string& error) const {
    return {{"ok", false}, {"error", error}};
}

Json CombatSession::command(const std::string& action, int q, int r) {
    if (!engine_) return failure("No battle has started.");
    if (awaiting_animation_) return failure("Wait for the current action to finish.");
    if (engine_->isOver()) return failure("The battle has already ended.");
    if (engine_->hasPendingChoice()) return failure("Resolve the pending character choice first.");
    const HexCoord cell{q, r};
    if (action == "retreat") {
        engine_->doRetreat();
    } else if (action == "ai") {
        CombatAI::takeTurn(*engine_);
    } else {
        if (!engine_->currentTurn().isPlayer) return failure("It is the enemy's turn.");
        if (action == "move") {
            if (!engine_->canMoveTo(cell)) return failure("That hex is not reachable.");
            engine_->doMove(cell);
        } else if (action == "attack") {
            const auto targets = legal_targets();
            if (std::find(targets.begin(), targets.end(), cell) == targets.end())
                return failure("That enemy is out of range.");
            engine_->doAttackAt(cell);
        } else if (action == "defend") {
            engine_->doDefend();
        } else return failure("Unknown combat action.");
    }
    return response();
}

bool CombatSession::acknowledge(int64_t ticket) {
    if (!awaiting_animation_ || ticket != ticket_) return false;
    awaiting_animation_ = false;
    return true;
}

Json CombatSession::snapshot() const {
    if (!engine_) return Json::object();
    Json state = {{"result", result_name(engine_->result())}, {"round", engine_->roundNumber()},
        {"units", Json::array()}, {"initiative", Json::array()}, {"reachable", Json::array()},
        {"attackable", Json::array()}, {"survivors", Json::array()}, {"rewards", Json::array()},
        {"active", ""}, {"player_turn", false}};
    for (bool player : {true, false}) {
        const auto& army = player ? engine_->playerArmy() : engine_->enemyArmy();
        for (int index = 0; index < static_cast<int>(army.stacks.size()); ++index) {
            const auto& unit = army.stacks[index];
            state["units"].push_back({{"key", key(player, index)}, {"id", unit.type->id},
                {"name", unit.type->name}, {"player", player}, {"cell", hex(unit.pos)},
                {"count", unit.count}, {"hp", unit.totalHp()}, {"unit_hp", unit.type->hitPoints},
                {"shots", unit.shotsLeft}, {"ranged", unit.type->isRanged()},
                {"attack", unit.effectiveAttack()}, {"defense", unit.effectiveDefense()},
                {"speed", unit.effectiveSpeed()}, {"defending", unit.isDefending}});
            if (player && !unit.isDead()) state["survivors"].push_back({{"id", unit.type->id}, {"count", unit.count}});
        }
    }
    if (!engine_->isOver()) {
        const auto& turn = engine_->currentTurn();
        state["active"] = key(turn.isPlayer, turn.stackIndex);
        state["player_turn"] = turn.isPlayer;
        for (auto cell : engine_->reachableTiles()) state["reachable"].push_back(hex(cell));
        for (auto cell : legal_targets()) state["attackable"].push_back(hex(cell));
        int index = 0;
        for (const auto& slot : engine_->turnOrder()) {
            const auto& unit = (slot.isPlayer ? engine_->playerArmy() : engine_->enemyArmy()).stacks[slot.stackIndex];
            if (!unit.isDead()) state["initiative"].push_back({{"key", key(slot.isPlayer, slot.stackIndex)},
                {"acted", index < engine_->turnIndex()}});
            ++index;
        }
    }
    if (engine_->result() == CombatResult::PlayerWon && !reward_.empty()) {
        const auto* item = resources_->item(reward_);
        state["rewards"].push_back({{"id", item->id}, {"name", item->name}, {"description", item->description}});
    }
    return state;
}

Json CombatSession::movement_path(HexCoord from, HexCoord to, bool player, int index) const {
    std::unordered_set<HexCoord> occupied;
    for (bool side : {true, false}) {
        const auto& army = side ? engine_->playerArmy() : engine_->enemyArmy();
        for (int i = 0; i < static_cast<int>(army.stacks.size()); ++i)
            if (!(side == player && i == index) && !army.stacks[i].isDead()) occupied.insert(army.stacks[i].pos);
    }
    std::queue<HexCoord> queue;
    std::unordered_map<HexCoord, HexCoord> previous;
    queue.push(from);
    previous[from] = from;
    while (!queue.empty()) {
        const auto current = queue.front(); queue.pop();
        if (current == to) break;
        for (int dir = 0; dir < 6; ++dir) {
            const auto next = current.neighbor(dir);
            if (!CombatMap::inBounds(next) || occupied.count(next) || previous.count(next)) continue;
            previous[next] = current;
            queue.push(next);
        }
    }
    if (!previous.count(to)) throw std::runtime_error("Combat movement has no traversable animation path.");
    std::vector<HexCoord> reversed;
    for (auto cell = to; cell != from; cell = previous.at(cell)) reversed.push_back(cell);
    Json path = Json::array();
    for (auto it = reversed.rbegin(); it != reversed.rend(); ++it) path.push_back(hex(*it));
    return path;
}

Json CombatSession::response() {
    static const char* names[] = {"move", "attack", "damage", "death", "defend", "end", "xp", "level", "choice"};
    Json events = Json::array();
    for (const auto& event : engine_->drainEvents()) {
        Json item = {{"type", names[static_cast<int>(event.type)]}, {"unit", key(event.isPlayer, event.stackIndex)},
            {"target", key(event.targetIsPlayer, event.targetIndex)}, {"damage", event.damage},
            {"retaliation", event.isRetaliation}, {"flanked", event.wasFlanked},
            {"from", hex(event.from)}, {"to", hex(event.to)}};
        if (event.type == CombatEvent::Type::UnitMoved)
            item["path"] = movement_path(event.from, event.to, event.isPlayer, event.stackIndex);
        events.push_back(std::move(item));
    }
    awaiting_animation_ = true;
    return {{"ok", true}, {"ticket", ++ticket_}, {"state", snapshot()}, {"events", events}};
}
