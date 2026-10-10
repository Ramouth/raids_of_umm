#include "adventure_bridge.h"
#include <fstream>
#include <cmath>
#include <limits>

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

// An item as the spoils screen shows it: kind "keepsake" (text = what it reads)
// or "commander" (text = its description). An id items.json lacks keeps its id as name.
Json itemJson(const ResourceManager& resources, const std::string& id) {
    const WondrousItem* item = resources.item(id);
    if (!item) return {{"id", id}, {"name", id}, {"kind", ""}, {"text", ""}};
    return {{"id", id}, {"name", item->name}, {"kind", item->isKeepsake() ? "keepsake" : "commander"},
            {"text", item->isKeepsake() ? item->text : item->description}};
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
    attach_story(out);
    Json ups = Json::array();            // level-ups since the last reply: the UI pops them up
    for (const auto& u : session_.drainLevelUps())
        ups.push_back({{"level", u.level}, {"skill", u.skill}, {"text", u.text}});
    out["level_ups"] = ups;
    return out;
}

void AdventureBridge::attach_story(Json& out) {
    // One ordered queue (Scenario::drainStory): the UI plays it in this order.
    // "lines" and "scenes" are the same events filtered by kind, for older callers.
    Json story = Json::array(), lines = Json::array(), scenes = Json::array();
    for (auto& e : session_.scenario().drainStory()) {
        const std::string kind = e.value("kind", "");
        if (kind == "line") lines.push_back({{"speaker", e.value("speaker", "")}, {"text", e.value("text", "")}});
        if (kind == "scene") scenes.push_back({{"frames", e.value("frames", Json::array())}});
        story.push_back(std::move(e));
    }
    out["story"] = story;
    out["lines"] = lines;
    out["scenes"] = scenes;
}

Json AdventureBridge::snapshot() const {
    Json owners = Json::array();
    for (const auto& [coord, ctrl] : session_.control())
        owners.push_back({coord.q, coord.r, ctrl.ownerFaction});
    Json guarded = Json::array();
    Json defenders = Json::array();
    Json encounter_xp = Json::array();   // [q, r, xp]: what beating each camp is worth
    for (const auto& obj : session_.map().objects())
        if (session_.isEncounter(obj.pos)) {
            guarded.push_back(cell(obj.pos));
            defenders.push_back({{"cell", cell(obj.pos)}, {"army", Json::parse(session_.encounterAt(obj.pos)->guardsJson)}});
            encounter_xp.push_back({obj.pos.q, obj.pos.r, session_.encounterXp(obj.pos)});
        }
    for (const auto& r : session_.rivals())
        if (r.alive && session_.isVisible(r.pos))
            encounter_xp.push_back({r.pos.q, r.pos.r, session_.encounterXp(r.pos)});
    const auto& hp = session_.heroProgress();
    Json hero_progress = {{"level", hp.level}, {"xp", hp.xp}, {"max", AdventureSession::MAX_HERO_LEVEL},
                          {"cap", session_.levelCap()}, {"cap_reached_day", session_.capReachedDay()},
                          {"xp_lost_to_cap", session_.xpLostToCap()},
                          {"prev", AdventureSession::xpForLevel(hp.level)},
                          {"next", AdventureSession::xpForLevel(hp.level + 1)}};
    Json army = Json::array();
    for (const auto& s : session_.army()) army.push_back({{"id", s.id}, {"count", s.count}, {"bitten", s.bitten}});
    Json opened = Json::array();             // closed tiles the story has opened (a gate)
    for (const auto& c : session_.opened()) opened.push_back(cell(c));
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
                                 {"attack_bonus", b->attackBonus}, {"readied_shot", b->readiedShot}});
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
    Json keepsakes = Json::array();      // read, never worn: the inventory's keepsake shelf
    for (const WondrousItem* k : session_.keepsakes())
        keepsakes.push_back({{"id", k->id}, {"name", k->name}, {"text", k->text}});
    Json lore = Json::array();
    for (const auto& l : session_.scenario().lore()) lore.push_back({{"title", l.title}, {"text", l.text}});
    auto path_json = [&](const AdventureSession::TreePath& p) {
        Json nodes = Json::array();
        for (const auto& n : p.nodes)
            nodes.push_back({{"id", n.id}, {"name", n.name}, {"text", n.text}, {"level", n.level},
                             {"live", n.live}, {"learned", session_.learned(n.id)}});
        return Json{{"id", p.id}, {"name", p.name}, {"text", p.text}, {"nodes", nodes}};
    };
    Json paths = Json::array();          // the faction's paths: one is chosen at level 2
    for (const auto& p : session_.heroPaths()) paths.push_back(path_json(p));
    Json tree = {{"paths", paths}, {"wildcard", path_json(session_.wildcardPath())},
                 {"chosen", session_.heroPath()}, {"needs_path", session_.needsPath()}};
    Json vanished = Json::array();       // story figures who have left the map
    for (const auto& obj : session_.map().objects())
        if (session_.scenario().vanished().count(obj.name)) vanished.push_back(cell(obj.pos));
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
                            {"level", sc.level}, {"xp", sc.xp}, {"morale", session_.scenario().morale(sc.id)},
                            {"next", AdventureSession::companionXpForLevel(sc.id, sc.level + 1)},
                            {"stationed", sc.stationed ? cell(*sc.stationed) : Json(nullptr)},
                            {"unpaid", sc.unpaidDays}, {"upkeep", session_.upkeepOf(sc)},
                            {"wounded_until", sc.woundedUntil}, {"wounded", session_.isWounded(sc)},
                            {"abilities", abilities}});
    }
    Json departed = Json::array();       // companions gone for now: waiting (stayed behind) or left
    for (const auto& sc : session_.departedSpecials())
        departed.push_back({{"id", sc.id}, {"name", sc.name}, {"title", sc.title}, {"level", sc.level},
                            {"waiting", sc.waiting}});
    Json battle_companions = Json::array();
    for (const auto& c : session_.battleCompanions())
        battle_companions.push_back({{"id", c.id}, {"count", 1}, {"level", c.level}, {"companion", true},
                                   {"morale", session_.scenario().morale(c.id)}});
    auto rivalRoster = [&](const AdventureSession::Rival& r) {
        auto roster = session_.townRoster(r.home);
        if (roster.empty() && !r.army.empty())
            if (const auto* unit = session_.resources().unit(r.army.front().id)) roster = unit->faction;
        return roster;
    };
    Json rivals = Json::array();
    for (const auto& r : session_.rivals()) {
        if (!r.alive || !session_.isVisible(r.pos)) continue;   // hidden in fog
        Json army_json = Json::array();
        for (const auto& s : r.army) army_json.push_back({{"id", s.id}, {"count", s.count}});
        rivals.push_back({{"id", r.id}, {"name", r.name}, {"cell", cell(r.pos)}, {"roster", rivalRoster(r)}, {"army", army_json}});
    }
    Json moves = Json::array();
    for (const auto& m : session_.lastRivalMoves()) {
        Json path = Json::array();
        for (const auto& c : m.path)
            if (session_.isVisible(c)) path.push_back(cell(c));   // only what the player saw
        if (!path.empty()) {
            for (const auto& r : session_.rivals()) if (r.id == m.id)
                moves.push_back({{"id", m.id}, {"name", r.name}, {"roster", rivalRoster(r)}, {"path", path}});
        }
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
                   {"items", enc ? Json(enc->items) : Json::array()},
                   {"layout", enc ? enc->layout : std::string("field")},   // pass to CombatSession.start
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
        {"guarded", guarded}, {"defenders", defenders},
        {"vanished", vanished},
        {"encounter_xp", encounter_xp},
        {"hero_progress", hero_progress},
        {"fieldwork_stock", session_.fieldworkStock()}, {"fieldwork_capacity", session_.fieldworkCapacity()},
        {"tree", tree}, {"tactics_rank", session_.tacticsRank()},
        {"encounter", pending},
        // The story choice raised at this fight's engage is still open: answer it
        // first; resolve_encounter refuses until then.
        {"encounter_awaits_choice", session_.encounterAwaitsChoice()},
        // Pass to CombatSession.start as encounter "tips": the campaign map ("" = no
        // tips) and the tips already shown this campaign.
        {"tip_context", {{"map", session_.campaignMap()},
                         {"shown", Json(std::vector<std::string>(session_.shownTips().begin(), session_.shownTips().end()))}}},
        {"sites", sites},
        {"chest", chest},
        {"won", session_.won()},
        {"story_choice", [&] {
            Json choice = session_.scenario().offeredChoice(session_);   // "if" / requires_item: shown only while it holds
            if (!choice.empty()) choice["route_ready"] = !session_.departureRoute().empty();
            if (choice.contains("options") && choice["options"].is_array())
                for (auto& option : choice["options"])       // "requires" / requires_flag not met yet
                    option["locked"] = Scenario::locked(session_, option).has_value();
            return choice;
        }()},
        {"departure_route", session_.departureRoute()},
        {"story_outcome", session_.scenario().outcome()},
        {"next_map", session_.nextMap()},
        {"flags", session_.flags()},
        {"level_cap", session_.levelCap()}, {"cap_reached_day", session_.capReachedDay()},
        {"xp_lost_to_cap", session_.xpLostToCap()},
        {"army", army},
        // The map names its own start_army: a fresh start fields that, not the client's default.
        {"map_start_army", !session_.scenario().startArmy().empty()},
        {"opened", opened},
        {"towns", towns},
        {"building_defs", building_defs},
        {"market", session_.hasMarket()},
        {"army_bonus", [&] {
            auto b = session_.armyBonus();
            return Json{{"attack", b.attack}, {"defense", b.defense}, {"speed", b.speed}, {"readied_shot", b.readiedShot}};
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
        {"keepsakes", keepsakes},
        {"lore", lore},
        {"ruled_out", cells(session_.ruledOut())},
        {"rivals", rivals},
        {"garrisons", garrisons},
        {"specials", specials},
        {"departed", departed},
        {"max_riding", AdventureSession::MAX_RIDING},
        {"max_stacks", Hero::ARMY_SLOTS},
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

Json AdventureBridge::resolve_encounter(bool victory, const Json& enemy_survivors, const Json& battle) {
    static const char* kFinds[] = {"none", "passage", "loot", "collapse"};
    std::optional<std::vector<AdventureSession::Stack>> survivors;
    if (!enemy_survivors.is_null()) {
        if (!enemy_survivors.is_array()) return {{"ok", false}, {"error", "Enemy survivors must be an array."}};
        survivors.emplace();
        for (const auto& stack : enemy_survivors) {
            if (!stack.is_object() || !stack.contains("id") || !stack["id"].is_string()
                    || !stack.contains("count") || !stack["count"].is_number()
                    || !std::isfinite(stack["count"].get<double>())
                    || stack["count"].get<double>() < 0
                    || stack["count"].get<double>() > std::numeric_limits<int>::max()
                    || std::floor(stack["count"].get<double>()) != stack["count"].get<double>())
                return {{"ok", false}, {"error", "Invalid enemy survivor stack."}};
            survivors->push_back({stack["id"], stack["count"]});
        }
    }
    if (session_.encounterAwaitsChoice()) {
        Json out = snapshot();
        out["ok"] = false;
        out["error"] = "Answer the story choice before the battle.";
        return out;
    }
    Json record = battle.is_object() ? battle : Json::object();
    for (const auto& id : record.value("tips", Json::array()))   // tips the battle showed: once per campaign
        if (id.is_string()) session_.markTipShown(id.get<std::string>());
    auto find = session_.resolveEncounter(victory, survivors);
    Json out = snapshot();
    out["find"] = kFinds[static_cast<int>(find)];
    // The result screens: the combat's own record, plus what the map pays for it.
    record["result"] = victory ? "victory" : record.value("result", "") == "retreat" ? "retreat" : "defeat";
    record["xp"] = session_.lastSpoils().xp;   // granted, after the level cap
    if (!record.contains("losses")) record["losses"] = {{"player", Json::array()}, {"enemy", Json::array()}};
    if (!record.contains("total_damage")) record["total_damage"] = {{"player", 0}, {"enemy", 0}};
    if (!record.contains("max_hit")) record["max_hit"] = {{"amount", 0}, {"unit", ""}, {"name", ""}, {"key", ""}, {"side", ""}};
    if (!record.contains("per_stack")) record["per_stack"] = Json::array();
    out["battle"] = record;
    const auto& spoils = session_.lastSpoils();
    Json resources = Json::object();
    for (int i = 0; i < RESOURCE_COUNT; ++i)
        if (spoils.resources.amounts[i] != 0)
            resources[std::string(resourceName(static_cast<Resource>(i)))] = spoils.resources.amounts[i];
    Json found = Json::array();
    for (const auto& id : spoils.items) found.push_back(itemJson(session_.resources(), id));
    out["spoils"] = {{"resources", resources}, {"items", found}};
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
    for (const auto& s : stacks) out.push_back({s.value("id", ""), s.value("count", 0), s.value("bitten", false)});
    session_.setArmy(out);
    return snapshot();
}

Json AdventureBridge::recruit(int q, int r, const std::string& unit_id, int count) {
    auto err = session_.recruit({q, r}, unit_id, count);
    Json out = snapshot();
    if (err) { out["ok"] = false; out["error"] = *err; }
    return with_lines(out);
}

Json AdventureBridge::choose_story(const std::string& id) {
    auto err = session_.scenario().choose(session_, id);
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

Json AdventureBridge::buy_fieldwork(const std::string& kind) {
    auto err = session_.buyFieldwork(kind);
    Json out = snapshot();
    if (err) { out["ok"] = false; out["error"] = *err; }
    return out;
}

Json AdventureBridge::choose_path(const std::string& id) {
    auto err = session_.choosePath(id);
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
    attach_story(out);                   // story still pending when the game was saved
    return out;
}

Json AdventureBridge::add_item(const std::string& id) {
    session_.addItem(id);
    return with_lines(snapshot());
}

Json AdventureBridge::tip_shown(const std::string& id) {
    session_.markTipShown(id);
    return snapshot();
}

Json AdventureBridge::join_special(const std::string& id) {
    session_.joinSpecial(id);
    return with_lines(snapshot());
}

Json AdventureBridge::end_day() {
    std::string event = session_.endDay();
    Json out = snapshot();
    out["event"] = event;
    out["tended"] = session_.tendedIn();   // town where the bitten were cured ("" = none)
    return with_lines(out);
}

Json AdventureBridge::claim_chest(bool gold) {
    std::string found = session_.claimChest(gold);
    Json out = snapshot();
    out["found"] = found;
    return with_lines(out);
}

// ── Campaign ──────────────────────────────────────────────────────────────────

Json AdventureBridge::campaign_json() const {
    const Campaign::Chapter* chapter = campaign_.currentChapter();
    const Campaign::MapEntry* entry = campaign_.current();
    return {{"finished", campaign_.finished()},
            {"chapter", chapter ? Json{{"id", chapter->id}, {"title", chapter->title}} : Json(nullptr)},
            {"map", entry ? campaign_.describe(*entry) : Json(nullptr)},
            {"state", campaign_.state().toJson()}};
}

Json AdventureBridge::campaign_load(const std::string& path, const std::string& data_dir) {
    if (auto err = campaign_.load(path, data_dir)) return {{"ok", false}, {"error", *err}};
    Json chapters = Json::array();
    for (const auto& c : campaign_.chapters()) {
        Json maps = Json::array();
        for (const auto& m : c.maps) maps.push_back(campaign_.describe(m));
        chapters.push_back({{"id", c.id}, {"title", c.title}, {"maps", maps}});
    }
    return {{"ok", true}, {"campaign", campaign_json()}, {"chapters", chapters}};
}

Json AdventureBridge::campaign_current() const {
    if (!campaign_.loaded()) return {{"ok", false}, {"error", "No campaign is loaded."}};
    return {{"ok", true}, {"campaign", campaign_json()}};
}

Json AdventureBridge::campaign_start_map(uint32_t seed) {
    const Campaign::MapEntry* entry = campaign_.current();
    if (!entry) return {{"ok", false}, {"error", campaign_.loaded() ? "The campaign is over." : "No campaign is loaded."}};
    if (auto err = session_.start(campaign_.resolve(entry->map), campaign_.dataDir(),
                                  campaign_.resolve(entry->encounters), seed,
                                  campaign_.resolve(entry->triggers), campaign_.startState()))
        return {{"ok", false}, {"error", *err}};
    Json out = with_lines(snapshot());
    out["campaign"] = campaign_json();
    return out;
}

Json AdventureBridge::campaign_advance(const std::string& next_map) {
    if (!campaign_.current()) return {{"ok", false}, {"error", "There is no campaign map to leave."}};
    if (!session_.won()) return {{"ok", false}, {"error", "The map is not finished yet."}};
    campaign_.finishMap(session_.carryOut());
    auto step = campaign_.advance(next_map.empty() ? session_.nextMap() : next_map);
    if (step.error) return {{"ok", false}, {"error", *step.error}};
    return {{"ok", true}, {"finished", step.finished}, {"new_chapter", step.newChapter},
            {"campaign", campaign_json()}};
}

Json AdventureBridge::campaign_save(const std::string& path) const {
    if (auto err = campaign_.saveFile(path)) return {{"ok", false}, {"error", *err}};
    return {{"ok", true}};
}

Json AdventureBridge::campaign_load_save(const std::string& path) {
    if (auto err = campaign_.loadFile(path)) return {{"ok", false}, {"error", *err}};
    return {{"ok", true}, {"campaign", campaign_json()}};
}
