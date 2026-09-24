#include "adventure_bridge.h"
#include <fstream>

namespace {
using Json = AdventureBridge::Json;

Json cell(const HexCoord& c) { return Json::array({c.q, c.r}); }

template <typename Cells>
Json cells(const Cells& input) {
    Json out = Json::array();
    for (const auto& c : input) out.push_back(cell(c));
    return out;
}

Json pool(const ResourcePool& p) {
    Json out = Json::object();
    for (int i = 0; i < RESOURCE_COUNT; ++i)
        out[std::string(resourceName(static_cast<Resource>(i)))] = p.amounts[i];
    return out;
}
} // namespace

Json AdventureBridge::start(const std::string& map_path, const std::string& data_dir,
                            const std::string& encounters_path, uint32_t seed,
                            const std::string& triggers_path) {
    if (auto err = session_.start(map_path, data_dir, encounters_path, seed, triggers_path))
        return {{"ok", false}, {"error", *err}};
    return with_lines(snapshot());
}

Json AdventureBridge::with_lines(Json out) {
    Json lines = Json::array();
    for (const auto& l : session_.scenario().drainLines())
        lines.push_back({{"speaker", l.speaker}, {"text", l.text}});
    out["lines"] = lines;
    return out;
}

Json AdventureBridge::snapshot() const {
    Json owners = Json::array();
    for (const auto& [coord, ctrl] : session_.control())
        owners.push_back({coord.q, coord.r, ctrl.ownerFaction});
    Json guarded = Json::array();
    Json encounter_xp = Json::array();   // [q, r, xp]: what beating each camp is worth
    for (const auto& obj : session_.map().objects())
        if (session_.isEncounter(obj.pos)) {
            guarded.push_back(cell(obj.pos));
            encounter_xp.push_back({obj.pos.q, obj.pos.r, session_.encounterXp(obj.pos)});
        }
    for (const auto& r : session_.rivals())
        if (r.alive && session_.isVisible(r.pos))
            encounter_xp.push_back({r.pos.q, r.pos.r, session_.encounterXp(r.pos)});
    const auto& hp = session_.heroProgress();
    Json hero_progress = {{"level", hp.level}, {"xp", hp.xp}, {"points", hp.points},
                          {"prev", AdventureSession::xpForLevel(hp.level)},
                          {"next", AdventureSession::xpForLevel(hp.level + 1)}};
    Json army = Json::array();
    for (const auto& s : session_.army()) army.push_back({{"id", s.id}, {"count", s.count}});
    Json towns = Json::array();
    for (const auto& obj : session_.map().objects()) {
        const TownState* t = session_.town(obj.pos);
        if (!t) continue;
        Json pool = Json::object();
        for (const auto& [id, n] : t->recruitPool) pool[id] = n;
        Json built = Json::array();
        for (const auto& id : t->buildings) built.push_back(id);
        Json blockers = Json::object();          // why each unbuilt building cannot go up today
        Json locked = Json::array();             // units whose dwelling is missing
        if (obj.type == ObjType::Town && session_.owner(obj.pos) == 1) {
            for (const BuildingDef* b : session_.resources().buildingsFor(AdventureSession::rosterFor(1)))
                if (!t->buildings.count(b->id)) blockers[b->id] = session_.buildBlocker(obj.pos, b->id);
            for (const UnitType* u : session_.resources().unitsByTier())
                if (u->faction == AdventureSession::rosterFor(1) && !session_.canRecruitHere(obj.pos, u->id))
                    locked.push_back(u->id);
        }
        towns.push_back({{"cell", cell(obj.pos)}, {"name", obj.name},
                         {"owner", session_.owner(obj.pos)}, {"pool", pool},
                         {"buildings", built}, {"blockers", blockers}, {"locked", locked},
                         {"built_today", t->builtOnDay == session_.day()},
                         {"built_today_name", [&] {
                             const BuildingDef* b = session_.resources().building(t->lastBuilt);
                             return t->builtOnDay == session_.day() && b ? b->name : std::string{};
                         }()},
                         {"title", [&] { const TownDef* d = session_.resources().townDef(obj.name); return d ? d->title : std::string{}; }()},
                         {"art", [&] { const TownDef* d = session_.resources().townDef(obj.name); return d ? d->art : std::string{}; }()},
                         {"gold", obj.type == ObjType::Town ? session_.townGold(obj.pos) : 0},
                         {"growth_bonus", session_.growthBonus(obj.pos)}});
    }
    Json building_defs = Json::array();
    for (const BuildingDef* b : session_.resources().buildingsFor(AdventureSession::rosterFor(1)))
        building_defs.push_back({{"id", b->id}, {"name", b->name}, {"description", b->description},
                                 {"cost", pool(b->cost)}, {"requires", b->requires}, {"unlocks", b->unlocks},
                                 {"income", b->income}, {"growth", b->growth}, {"market", b->market},
                                 {"attack_bonus", b->attackBonus}});
    Json quests = Json::array();
    for (const auto& q : session_.scenario().quests())
        quests.push_back({{"id", q.id}, {"title", q.title}, {"text", q.text},
                          {"main", q.main}, {"done", q.done}});
    Json offers = Json::array();
    if (const MapObjectDef* here = session_.map().objectAt(session_.heroPos()))
        for (const auto* o : session_.scenario().offersAt(here->name))
            offers.push_back({{"id", o->id}, {"label", o->label}});
    Json items = Json::array();
    for (const auto& id : session_.items()) items.push_back(id);
    Json lore = Json::array();
    for (const auto& l : session_.scenario().lore()) lore.push_back({{"title", l.title}, {"text", l.text}});
    Json garrisons = Json::array();
    for (const auto& [coord, ctrl] : session_.control()) {
        const auto* held = session_.garrison(coord);
        if (!held || ctrl.ownerFaction != Faction::Player) continue;
        Json stacks = Json::array();
        for (const auto& s : *held) stacks.push_back({{"id", s.id}, {"count", s.count}});
        garrisons.push_back({{"cell", cell(coord)}, {"army", stacks}});
    }
    Json specials = Json::array();
    for (const auto& sc : session_.specials()) {
        Json abilities = Json::array();
        for (const auto& a : AdventureSession::abilitiesOf(sc.id))
            abilities.push_back({{"level", a.level}, {"name", a.name}, {"text", a.text},
                                 {"unlocked", sc.level >= a.level}});
        specials.push_back({{"id", sc.id}, {"name", sc.name}, {"title", sc.title},
                            {"level", sc.level}, {"xp", sc.xp},
                            {"next", AdventureSession::xpForLevel(sc.level + 1)},
                            {"stationed", sc.stationed ? cell(*sc.stationed) : Json(nullptr)},
                            {"unpaid", sc.unpaidDays}, {"upkeep", AdventureSession::upkeepFor(sc.level)},
                            {"wounded_until", sc.woundedUntil}, {"wounded", session_.isWounded(sc)},
                            {"abilities", abilities}});
    }
    Json battle_companions = Json::array();
    for (const auto& c : session_.battleCompanions())
        battle_companions.push_back({{"id", c.id}, {"count", 1}, {"level", c.level}, {"companion", true}});
    Json rivals = Json::array();
    for (const auto& r : session_.rivals()) {
        if (!r.alive || !session_.isVisible(r.pos)) continue;   // hidden in fog
        Json army_json = Json::array();
        for (const auto& s : r.army) army_json.push_back({{"id", s.id}, {"count", s.count}});
        rivals.push_back({{"id", r.id}, {"name", r.name}, {"cell", cell(r.pos)}, {"army", army_json}});
    }
    Json moves = Json::array();
    for (const auto& m : session_.lastRivalMoves()) {
        Json path = Json::array();
        for (const auto& c : m.path)
            if (session_.isVisible(c)) path.push_back(cell(c));   // only what the player saw
        if (!path.empty()) moves.push_back({{"id", m.id}, {"path", path}});
    }
    // Adventure sites: used = weekly site spent this week / one-time site done.
    Json sites = Json::array();
    for (const auto& obj : session_.map().objects()) {
        switch (obj.type) {
            case ObjType::Pickup: case ObjType::Artifact: case ObjType::Mill:
            case ObjType::Stables: case ObjType::Watchtower: case ObjType::LearningStone:
            case ObjType::Obelisk:
                sites.push_back({{"cell", cell(obj.pos)}, {"used", session_.siteUsed(obj.pos)}});
                break;
            default: break;
        }
    }
    Json chest = nullptr;
    if (auto at = session_.pendingChest())
        if (const auto* p = session_.pickupAt(*at))
            chest = {{"cell", cell(*at)}, {"gold", p->reward[Resource::Gold]}, {"xp", p->xp}};
    Json pending = nullptr;
    if (auto at = session_.pendingEncounter()) {
        const auto* enc = session_.encounterAt(*at);
        const MapObjectDef* obj = session_.map().objectAt(*at);
        const auto* rival = session_.rivalAt(*at);
        std::string type = rival ? (session_.pendingIsAmbush() ? "ambush" : "rival")
                         : obj && obj->type == ObjType::OldMine ? "old_mine" : "guard";
        pending = {{"cell", cell(*at)}, {"name", rival ? rival->name : obj ? obj->name : ""},
                   {"type", type},
                   {"guards", enc ? Json::parse(enc->guardsJson) : Json::array()},
                   {"reward", enc ? pool(enc->reward) : Json::object()},
                   {"item", enc ? enc->item : ""},
                   {"xp", session_.encounterXp(*at)}};
    }
    return {
        {"ok", true},
        {"day", session_.day()}, {"week", session_.week()},
        {"day_of_week", session_.dayOfWeek()}, {"month", session_.month()},
        {"moves", session_.moves()}, {"moves_max", session_.movesMax()},
        {"hero", cell(session_.heroPos())},
        {"treasury", pool(session_.treasury())},
        {"income", pool(session_.dailyIncome())},
        {"owners", owners},
        {"visible", cells(session_.visible())},
        {"explored", cells(session_.explored())},
        {"guarded", guarded},
        {"encounter_xp", encounter_xp},
        {"hero_progress", hero_progress},
        {"encounter", pending},
        {"sites", sites},
        {"chest", chest},
        {"won", session_.won()},
        {"army", army},
        {"towns", towns},
        {"building_defs", building_defs},
        {"market", session_.hasMarket()},
        {"army_bonus", [&] {
            auto b = session_.armyBonus();
            return Json{{"attack", b.attack}, {"defense", b.defense}, {"speed", b.speed}};
        }()},
        {"equipped", [&] {
            Json slots = Json::object();
            for (const auto& slot : AdventureSession::equipSlots()) {
                auto it = session_.equipped().find(slot);
                slots[slot] = it == session_.equipped().end() ? Json(nullptr) : Json(it->second);
            }
            return slots;
        }()},
        {"backpack", session_.backpack()},
        {"quests", quests},
        {"offers", offers},
        {"items", items},
        {"lore", lore},
        {"ruled_out", cells(session_.ruledOut())},
        {"rivals", rivals},
        {"garrisons", garrisons},
        {"specials", specials},
        {"battle_companions", battle_companions},
        {"upkeep", session_.upkeepPerDay()},
        {"rival_moves", moves},
        {"lost", session_.lost()},
        {"lost_reason", session_.lostReason()},
    };
}

Json AdventureBridge::preview(int q, int r) const {
    auto path = session_.route({q, r});
    return {{"path", cells(path)}, {"cost", session_.routeCost(path)},
            {"reachable", session_.affordableSteps(path)}};
}

Json AdventureBridge::travel(int q, int r) {
    Json steps = Json::array();
    for (const auto& step : session_.travel({q, r}))
        steps.push_back({{"cell", cell(step.cell)}, {"capture", step.capture},
                         {"revealed", cells(step.revealed)}, {"found", step.found}});
    Json out = snapshot();
    out["steps"] = steps;
    return with_lines(out);
}

Json AdventureBridge::resolve_encounter(bool victory) {
    static const char* kFinds[] = {"none", "passage", "loot", "collapse"};
    auto find = session_.resolveEncounter(victory);
    Json out = snapshot();
    out["find"] = kFinds[static_cast<int>(find)];
    return with_lines(out);
}

Json AdventureBridge::companions_fell(const Json& fallen, bool lost) {
    std::vector<std::string> ids;
    for (const auto& id : fallen) ids.push_back(id.get<std::string>());
    session_.companionsFell(ids, lost);
    return with_lines(snapshot());
}

Json AdventureBridge::equip(const std::string& id) {
    auto err = session_.equip(id);
    Json out = snapshot();
    if (err) { out["ok"] = false; out["error"] = *err; }
    return out;
}

Json AdventureBridge::unequip(const std::string& slot) {
    auto err = session_.unequip(slot);
    Json out = snapshot();
    if (err) { out["ok"] = false; out["error"] = *err; }
    return out;
}

Json AdventureBridge::build(int q, int r, const std::string& id) {
    auto err = session_.build({q, r}, id);
    Json out = snapshot();
    if (err) { out["ok"] = false; out["error"] = *err; }
    return with_lines(out);
}

Json AdventureBridge::trade(const std::string& give, const std::string& get, int amount) {
    auto parse = [](const std::string& name, Resource& out) {
        static const char* names[] = {"Gold", "Wood", "Stone", "Obsidian", "Crystal"};
        for (int i = 0; i < RESOURCE_COUNT; ++i) if (name == names[i]) { out = static_cast<Resource>(i); return true; }
        return false;
    };
    Resource from, to;
    if (!parse(give, from) || !parse(get, to)) return {{"ok", false}, {"error", "Unknown resource."}};
    auto result = session_.trade(from, to, amount);
    Json out = snapshot();
    out["received"] = result.received;
    if (!result.error.empty()) { out["ok"] = false; out["error"] = result.error; }
    return out;
}

Json AdventureBridge::trade_quote(const std::string& give, const std::string& get, int amount) const {
    static const char* names[] = {"Gold", "Wood", "Stone", "Obsidian", "Crystal"};
    int a = -1, b = -1;
    for (int i = 0; i < RESOURCE_COUNT; ++i) { if (give == names[i]) a = i; if (get == names[i]) b = i; }
    if (a < 0 || b < 0) return {{"ok", false}, {"error", "Unknown resource."}};
    return {{"ok", true}, {"received", AdventureSession::tradeQuote(static_cast<Resource>(a), static_cast<Resource>(b), amount)}};
}

Json AdventureBridge::set_army(const Json& stacks) {
    std::vector<AdventureSession::Stack> out;
    for (const auto& s : stacks) out.push_back({s.value("id", ""), s.value("count", 0)});
    session_.setArmy(out);
    return snapshot();
}

Json AdventureBridge::recruit(int q, int r, const std::string& unit_id, int count) {
    auto err = session_.recruit({q, r}, unit_id, count);
    Json out = snapshot();
    if (err) { out["ok"] = false; out["error"] = *err; }
    return with_lines(out);
}

Json AdventureBridge::accept_offer(const std::string& id) {
    auto err = session_.acceptOffer(id);
    Json out = snapshot();
    if (err) { out["ok"] = false; out["error"] = *err; }
    return with_lines(out);
}

Json AdventureBridge::transfer(int q, int r, const std::string& unit_id, int count, bool to_garrison) {
    auto err = session_.transfer({q, r}, unit_id, count, to_garrison);
    Json out = snapshot();
    if (err) { out["ok"] = false; out["error"] = *err; }
    return out;
}

Json AdventureBridge::station(const std::string& id, bool stay) {
    auto err = session_.station(id, stay);
    Json out = snapshot();
    if (err) { out["ok"] = false; out["error"] = *err; }
    return with_lines(out);
}

Json AdventureBridge::save(const std::string& path, const Json& extra) {
    Json state = session_.saveState();
    if (state.is_null()) return {{"ok", false}, {"error", "This session cannot be saved."}};
    state["extra"] = extra;
    std::ofstream f(path);
    if (!f.is_open()) return {{"ok", false}, {"error", "Cannot write " + path}};
    f << state.dump();
    return {{"ok", true}};
}

Json AdventureBridge::load(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return {{"ok", false}, {"error", "No saved game."}};
    Json state = Json::parse(f, nullptr, false);
    if (state.is_discarded()) return {{"ok", false}, {"error", "Save file is damaged."}};
    if (auto err = session_.loadState(state)) return {{"ok", false}, {"error", *err}};
    Json out = snapshot();
    out["extra"] = state.value("extra", Json::object());
    return out;
}

Json AdventureBridge::add_item(const std::string& id) {
    session_.addItem(id);
    return with_lines(snapshot());
}

Json AdventureBridge::end_day() {
    std::string event = session_.endDay();
    Json out = snapshot();
    out["event"] = event;
    return with_lines(out);
}

Json AdventureBridge::claim_chest(bool gold) {
    std::string found = session_.claimChest(gold);
    Json out = snapshot();
    out["found"] = found;
    return with_lines(out);
}
