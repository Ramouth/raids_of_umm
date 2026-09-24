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
    // Up to five troop stacks, plus companions ({"id", "level", "companion": true}).
    if (!input.is_array() || input.empty())
        throw std::runtime_error("An army must contain between one and five stacks.");
    CombatArmy army;
    army.isPlayer = player;
    army.ownerName = player ? "Expedition" : "Dungeon guards";
    int troops = 0, companions = 0;
    for (const auto& entry : input) {
        const auto id = entry.at("id").get<std::string>();
        const UnitType* type = resources_->unit(id);
        if (entry.value("companion", false)) {
            const int level = entry.value("level", 1);
            if (!type || !type->isCompanion() || level < 1 || level > 30 || ++companions > 3)
                throw std::runtime_error("Unknown companion or too many companions: " + id);
            army.stacks.push_back(CombatUnit::companion(type, level, player));
            continue;
        }
        const int count = entry.at("count").get<int>();
        if (!type || count <= 0 || count > 10000)
            throw std::runtime_error("Unknown unit or invalid stack size: " + id);
        if (++troops > CombatMap::GRID_H)
            throw std::runtime_error("An army must contain between one and five stacks.");
        army.stacks.push_back(CombatUnit::make(type, count, player));
        army.stacks.back().attackBonus = std::clamp(entry.value("attack_bonus", 0), 0, 20);   // e.g. Drill Yard
    }
    if (troops == 0) throw std::runtime_error("An army must contain between one and five stacks.");
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
        // Tests and replays may fix the dice and the AI's choices.
        if (encounter.contains("seed")) engine_->setSeed(encounter.at("seed").get<uint32_t>());
        return response();
    } catch (const std::exception& error) {
        engine_.reset();
        return failure(error.what());
    }
}

std::vector<HexCoord> CombatSession::legal_targets() const {
    // Shots, adjacent strikes and (HoMM3) walk-then-strike targets.
    if (!engine_ || engine_->isOver()) return {};
    return engine_->attackableTiles();
}

Json CombatSession::failure(const std::string& error) const {
    return {{"ok", false}, {"error", error}};
}

Json CombatSession::command(const std::string& action, int q, int r, int fq, int fr) {
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
        } else if (action == "strike") {
            // Move-and-attack from an explicit standing hex (fq, fr).
            const auto& foes = engine_->currentTurn().isPlayer ? engine_->enemyArmy() : engine_->playerArmy();
            int target = -1;
            for (int i = 0; i < static_cast<int>(foes.stacks.size()); ++i)
                if (!foes.stacks[i].isDead() && foes.stacks[i].pos == cell) target = i;
            if (target < 0) return failure("There is no enemy there.");
            if (!engine_->doAttackFrom(HexCoord{fq, fr}, target))
                return failure("You cannot strike that enemy from there.");
        } else if (action == "defend") {
            engine_->doDefend();
        } else return failure("Unknown combat action.");
    }
    return response();
}

Json CombatSession::command_route(const std::string& action, const Json& route, int q, int r) {
    if (!engine_) return failure("No battle has started.");
    if (awaiting_animation_) return failure("Wait for the current action to finish.");
    if (engine_->isOver()) return failure("The battle has already ended.");
    if (!engine_->currentTurn().isPlayer) return failure("It is the enemy's turn.");
    std::vector<HexCoord> path;
    try {
        for (const auto& cell : route) path.push_back({cell.at(0).get<int>(), cell.at(1).get<int>()});
    } catch (const std::exception&) { return failure("A route is a list of [q, r] hexes."); }
    if (action == "move") {
        if (!engine_->doMoveAlong(path)) return failure("That route is blocked or too long.");
    } else if (action == "strike") {
        const auto& foes = engine_->enemyArmy();
        int target = -1;
        for (int i = 0; i < static_cast<int>(foes.stacks.size()); ++i)
            if (!foes.stacks[i].isDead() && foes.stacks[i].pos == HexCoord{q, r}) target = i;
        if (target < 0) return failure("There is no enemy there.");
        if (!engine_->doAttackAlong(path, target)) return failure("You cannot strike that enemy along that route.");
    } else return failure("Unknown route action.");
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
        {"fallen", Json::array()},
        {"active", ""}, {"player_turn", false}};
    for (bool player : {true, false}) {
        const auto& army = player ? engine_->playerArmy() : engine_->enemyArmy();
        for (int index = 0; index < static_cast<int>(army.stacks.size()); ++index) {
            const auto& unit = army.stacks[index];
            state["units"].push_back({{"key", key(player, index)}, {"id", unit.type->id},
                {"name", unit.type->name}, {"player", player}, {"cell", hex(unit.pos)},
                {"count", unit.count}, {"hp", unit.totalHp()}, {"unit_hp", unit.maxHp()},
                {"companion", unit.isSpecialCharacter}, {"level", unit.scLevel},
                {"aura_radius", unit.type->auraRadius}, {"aura_defense", unit.type->auraDefense},
                {"aura_bonus", unit.auraBonus},
                {"bodyguard", [&] {
                    const int guard = CombatEngine::bodyguardFor(army.stacks, index);
                    return guard >= 0 ? key(player, guard) : std::string{};
                }()},
                {"hp_left", unit.hpLeft}, {"shots", unit.shotsLeft}, {"shots_max", unit.type->shots},
                {"ranged", unit.type->isRanged()},
                {"attack", unit.effectiveAttack()}, {"defense", unit.effectiveDefense()},
                {"min_damage", unit.type->minDamage + unit.damageBonus},
                {"max_damage", unit.type->maxDamage + unit.damageBonus},
                {"move", unit.type->moveRange}, {"abilities", unit.type->abilities},
                {"retaliated", unit.hasRetaliated},
                {"speed", unit.effectiveSpeed()}, {"defending", unit.isDefending}});
            if (player && unit.isSpecialCharacter) {
                if (unit.isDead()) state["fallen"].push_back(unit.scId);
                else {
                    // Enemies that can reach this companion on their next turn.
                    Json threats = Json::array();
                    if (!engine_->isOver())
                        for (int foe : engine_->threatsTo(true, index)) threats.push_back(key(false, foe));
                    state["units"].back()["threats"] = threats;
                }
            } else if (player && !unit.isDead()) {
                state["survivors"].push_back({{"id", unit.type->id}, {"count", unit.count}});
            }
        }
    }
    if (!engine_->isOver()) {
        const auto& turn = engine_->currentTurn();
        state["active"] = key(turn.isPlayer, turn.stackIndex);
        state["player_turn"] = turn.isPlayer;
        state["reaction_fire"] = Json::array();   // hexes whose approach draws enemy shots
        for (auto cell : engine_->reachableTiles()) {
            state["reachable"].push_back(hex(cell));
            Json shots = reactions(cell);
            if (!shots.empty()) state["reaction_fire"].push_back({{"cell", hex(cell)}, {"shots", shots}});
        }
        for (auto cell : legal_targets()) state["attackable"].push_back(hex(cell));
        int index = 0;
        for (const auto& slot : engine_->turnOrder()) {
            const auto& unit = (slot.isPlayer ? engine_->playerArmy() : engine_->enemyArmy()).stacks[slot.stackIndex];
            if (!unit.isDead()) state["initiative"].push_back({{"key", key(slot.isPlayer, slot.stackIndex)},
                {"acted", index < engine_->turnIndex()}});
            ++index;
        }
        state["next_round"] = next_round();
        state["previews"] = previews();
    }
    if (engine_->result() == CombatResult::PlayerWon && !reward_.empty()) {
        const auto* item = resources_->item(reward_);
        state["rewards"].push_back({{"id", item->id}, {"name", item->name}, {"description", item->description}});
    }
    return state;
}

Json CombatSession::next_round() const {
    // Same rule as CombatEngine::buildQueue: faster first, the expedition wins ties.
    struct Slot { bool player; int index; int speed; };
    std::vector<Slot> order;
    for (bool player : {true, false}) {
        const auto& army = player ? engine_->playerArmy() : engine_->enemyArmy();
        for (int i = 0; i < static_cast<int>(army.stacks.size()); ++i)
            if (!army.stacks[i].isDead()) order.push_back({player, i, army.stacks[i].effectiveSpeed()});
    }
    std::stable_sort(order.begin(), order.end(), [](const Slot& a, const Slot& b) {
        if (a.speed != b.speed) return a.speed > b.speed;
        return a.player && !b.player;
    });
    Json out = Json::array();
    for (const auto& slot : order) out.push_back(key(slot.player, slot.index));
    return out;
}

Json CombatSession::previews() const {
    // Hover forecast for every stack the active unit may attack this turn.
    // Each entry lists every standing hex it could strike from ("options",
    // each with its walk path and exact forecast — pinning depends on the
    // hex) and the one a plain "attack" order would pick ("best").
    Json out = Json::array();
    const auto& turn = engine_->currentTurn();
    const auto& actor = engine_->activeUnit();
    const auto& enemies = actor.isPlayer ? engine_->enemyArmy() : engine_->playerArmy();
    auto forecast = [](const AttackPreview& p) {
        return Json{{"ranged", p.ranged}, {"pinned", p.pinned}, {"blocked", p.blocked},
            {"guarded", p.guarded}, {"guard_min", p.guardDamage.min}, {"guard_max", p.guardDamage.max},
            {"retaliation_guarded", p.retaliationGuarded},
            {"damage_min", p.damage.min}, {"damage_max", p.damage.max},
            {"kills_min", p.killsMin}, {"kills_max", p.killsMax},
            {"retaliation", p.retaliation},
            {"retaliation_min", p.retaliationDamage.min}, {"retaliation_max", p.retaliationDamage.max},
            {"retaliation_kills_min", p.retKillsMin}, {"retaliation_kills_max", p.retKillsMax}};
    };
    for (int i = 0; i < static_cast<int>(enemies.stacks.size()); ++i) {
        if (!engine_->canAttack(i)) continue;
        const HexCoord best = engine_->bestAttackHex(i);
        Json entry = forecast(engine_->previewAttack(i, best));
        entry["key"] = key(!actor.isPlayer, i);
        entry["cell"] = hex(enemies.stacks[i].pos);
        entry["best"] = hex(best);
        entry["options"] = Json::array();
        std::vector<HexCoord> spots = engine_->attackHexesFor(i);
        if (spots.empty()) spots.push_back(actor.pos);   // a shot from where it stands
        for (const HexCoord& from : spots) {
            const AttackPreview p = engine_->previewAttackUnchecked(i, from);
            if (!p.valid) continue;
            Json option = forecast(p);
            option["from"] = hex(from);
            option["path"] = from == actor.pos ? Json::array()
                : movement_path(actor.pos, from, turn.isPlayer, turn.stackIndex);
            option["reactions"] = from == actor.pos ? Json::array() : reactions(from);
            entry["options"].push_back(std::move(option));
        }
        out.push_back(std::move(entry));
    }
    return out;
}

Json CombatSession::reactions(HexCoord to) const {
    Json out = Json::array();
    const bool player = engine_->activeUnit().isPlayer;
    for (const auto& r : engine_->reactionsTo(to))
        out.push_back({{"key", key(!player, r.shooter)}, {"damage_min", r.damage.min},
                       {"damage_max", r.damage.max}, {"blocked", r.blocked}});
    return out;
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
            {"blocked", event.blockedShot}, {"bodyguard", event.bodyguard}, {"reaction", event.isReaction},
            {"from", hex(event.from)}, {"to", hex(event.to)},
            {"kills", event.kills}, {"remaining", event.remaining}};
        if (event.type == CombatEvent::Type::UnitAttacked) {
            // Attacks never move anyone, so the current positions tell shots from strikes.
            const auto& actor = (event.isPlayer ? engine_->playerArmy() : engine_->enemyArmy()).stacks[event.stackIndex];
            const auto& target = (event.targetIsPlayer ? engine_->playerArmy() : engine_->enemyArmy()).stacks[event.targetIndex];
            item["ranged"] = event.isReaction
                || (!event.isRetaliation && actor.type->isRanged() && actor.pos.distanceTo(target.pos) > 1);
        }
        if (event.type == CombatEvent::Type::UnitMoved) {
            if (!event.path.empty()) {
                item["path"] = Json::array();
                for (const auto& cell : event.path) item["path"].push_back(hex(cell));
            } else item["path"] = movement_path(event.from, event.to, event.isPlayer, event.stackIndex);
        }
        events.push_back(std::move(item));
    }
    awaiting_animation_ = true;
    return {{"ok", true}, {"ticket", ++ticket_}, {"state", snapshot()}, {"events", events}};
}
