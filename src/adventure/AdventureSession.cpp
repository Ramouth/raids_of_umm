#include "AdventureSession.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <queue>
#include <random>

namespace {

bool isCapturable(ObjType t) {
    switch (t) {
        case ObjType::Town:
        case ObjType::GoldMine:
        case ObjType::CrystalMine:
        case ObjType::Sawmill:
        case ObjType::Quarry:
        case ObjType::ObsidianVent:
        case ObjType::Dwelling:
            return true;
        default:
            return false;
    }
}

bool isEncounterType(ObjType t) {
    return t == ObjType::Guard || t == ObjType::OldMine;
}

bool blocksSight(Terrain t) {
    return t == Terrain::Mountain || t == Terrain::Wall;
}

// HoMM3-sized defaults; <map>.encounters.json "pickups" overrides them by name.
// Unlike HoMM3, experience is rare off the battlefield: chests hold gold, and
// only a "tome" (a war journal) teaches.
AdventureSession::Pickup defaultPickup(const MapObjectDef& obj) {
    AdventureSession::Pickup p;
    const std::string& k = obj.kind;
    if (obj.type == ObjType::Artifact) { p.item = k; return p; }
    if (k == "chest")         p.reward[Resource::Gold] = 1000;
    else if (k == "tome")     p.xp = AdventureSession::TOME_XP;
    else if (k == "campfire") { p.reward[Resource::Gold] = 400; p.reward[Resource::Wood] = 3; }
    else if (k == "wood")     p.reward[Resource::Wood] = 6;
    else if (k == "stone")    p.reward[Resource::Stone] = 6;
    else if (k == "obsidian") p.reward[Resource::Obsidian] = 3;
    else if (k == "crystal")  p.reward[Resource::Crystal] = 3;
    else                      p.reward[Resource::Gold] = 750;
    return p;
}

std::string describe(const ResourcePool& pool) {
    std::string out;
    for (int i = 0; i < RESOURCE_COUNT; ++i) {
        if (pool.amounts[i] <= 0) continue;
        if (!out.empty()) out += ", ";
        out += "+" + std::to_string(pool.amounts[i]) + " " + std::string(resourceName(static_cast<Resource>(i)));
    }
    return out;
}

} // namespace

std::optional<std::string> AdventureSession::start(const std::string& mapPath,
                                                   const std::string& dataDir,
                                                   const std::string& encountersPath,
                                                   uint32_t seed,
                                                   const std::string& triggersPath) {
    WorldMap map;
    if (auto err = map.loadJson(mapPath)) return err;
    auto err = start(std::move(map), dataDir, encountersPath, seed, triggersPath);
    if (!err) m_startArgs = StartArgs{mapPath, dataDir, encountersPath, triggersPath, seed};
    return err;
}

std::optional<std::string> AdventureSession::start(WorldMap map, const std::string& dataDir,
                                                   const std::string& encountersPath,
                                                   uint32_t seed,
                                                   const std::string& triggersPath) {
    auto resources = std::make_unique<ResourceManager>();
    if (auto err = resources->load(dataDir)) return err;

    m_map       = std::move(map);
    m_resources = std::move(resources);
    m_turns.init(STARTING_GOLD);
    m_control.clear();
    m_towns.clear();
    m_visible.clear();
    m_explored.clear();
    m_hero     = Hero{};
    m_movesMax = DEFAULT_MOVES;
    m_moves    = m_movesMax;
    m_encounters.clear();
    m_mineFinds.clear();
    m_pending.reset();
    m_passage.reset();
    m_won = false;
    m_ruledOut.clear();
    m_items.clear();
    m_scenario = Scenario{};
    m_rivals.clear();
    m_rivalMoves.clear();
    m_garrisons.clear();
    m_specials.clear();
    m_heroProgress = HeroProgress{};
    m_learned.clear();
    m_path.clear();
    m_levelUps.clear();
    loadHeroTree(dataDir + "/hero_tree.json");   // no file: no paths
    m_startArgs.reset();
    m_pendingAmbush = false;
    m_lost = false;
    m_lostReason.clear();
    m_pickups.clear();
    m_siteWeek.clear();
    m_visitedOnce.clear();
    m_pendingChest.reset();
    m_stablesWeek = 0;

    const MapObjectDef* spawn = nullptr;
    std::vector<HexCoord> oldMines;
    for (const auto& obj : m_map.objects()) {
        if (isEncounterType(obj.type)) m_encounters[obj.pos];
        if (obj.type == ObjType::OldMine) oldMines.push_back(obj.pos);
        if (isCapturable(obj.type)) {
            ObjectControl ctrl;
            ctrl.objType      = obj.type;
            ctrl.ownerFaction = obj.factionId;
            m_control[obj.pos] = ctrl;
        }
        if (obj.type == ObjType::Pickup || (obj.type == ObjType::Artifact && !obj.kind.empty()))
            m_pickups[obj.pos] = defaultPickup(obj);
        if (obj.type == ObjType::Dwelling) m_towns[obj.pos];
        if (obj.type == ObjType::Town) {
            m_towns[obj.pos];
            bool better = !spawn || (obj.factionId == Faction::Player && spawn->factionId != Faction::Player);
            if (better) spawn = &obj;
        }
    }
    if (spawn) {
        m_hero.pos = spawn->pos;
    } else {
        for (const auto& [coord, tile] : m_map) {
            if (tile.passable) { m_hero.pos = coord; break; }
        }
    }

    // One old mine hides the passage; the others alternate loot / collapse.
    if (!oldMines.empty()) {
        std::mt19937 rng(seed);
        size_t pick = std::uniform_int_distribution<size_t>(0, oldMines.size() - 1)(rng);
        bool loot = true;
        for (size_t i = 0; i < oldMines.size(); ++i) {
            if (i == pick) { m_mineFinds[oldMines[i]] = MineFind::Passage; continue; }
            m_mineFinds[oldMines[i]] = loot ? MineFind::Loot : MineFind::Collapse;
            loot = !loot;
        }
        m_passage = oldMines[pick];
    }
    if (!encountersPath.empty())
        if (auto err = loadEncounters(encountersPath)) return err;
    for (auto& [coord, town] : m_towns) ensureBuildings(coord);
    for (auto& [coord, town] : m_towns) growTown(coord);  // first week's recruits
    for (const auto& obj : m_map.objects())
        if (obj.type == ObjType::Town && obj.factionId == Faction::AI) spawnRival(obj.pos, 1);
    if (!triggersPath.empty())
        if (auto err = m_scenario.load(triggersPath)) return err;

    // The expedition's commander rides from day 1 unless the story brings her in later.
    for (const auto& id : m_scenario.startCompanions().value_or(std::vector<std::string>{"ushari"}))
        joinSpecial(id);
    m_movesMax = DEFAULT_MOVES + movesBonus();
    m_moves    = m_movesMax;
    recomputeVisibility();
    m_scenario.fire(*this, "start");
    fireSightings({m_explored.begin(), m_explored.end()});
    return std::nullopt;
}

// Format: { "defaults": { "guard": E, "old_mine": E }, "objects": { "<name>": E } }
// E = { "guards": [{"id","count"}...], "reward": {"Gold": 500, ...}, "item": "id" }
std::optional<std::string> AdventureSession::loadEncounters(const std::string& path) {
    using json = nlohmann::json;
    try {
        std::ifstream f(path);
        if (!f.is_open()) return "Cannot open encounters: " + path;
        json root = json::parse(f);
        auto parse = [](const json& e) {
            Encounter out;
            out.guardsJson = e.value("guards", json::array()).dump();
            const json reward = e.value("reward", json::object());
            for (const auto& [key, amount] : reward.items())
                for (int i = 0; i < RESOURCE_COUNT; ++i)
                    if (key == resourceName(static_cast<Resource>(i)))
                        out.reward.amounts[i] = amount.get<int>();
            out.item = e.value("item", "");
            return out;
        };
        const json defaults = root.value("defaults", json::object());
        const json objects  = root.value("objects",  json::object());
        for (auto& [cell, enc] : m_encounters) {
            const MapObjectDef* obj = m_map.objectAt(cell);
            if (!obj) continue;
            const char* typeKey = obj->type == ObjType::OldMine ? "old_mine" : "guard";
            if (objects.contains(obj->name))  enc = parse(objects.at(obj->name));
            else if (defaults.contains(typeKey)) enc = parse(defaults.at(typeKey));
        }
        // "pickups": { "<name>": { "reward": {...}, "item": "id", "xp": 500 } }
        const json pickups = root.value("pickups", json::object());
        for (auto& [cell, pickup] : m_pickups) {
            const MapObjectDef* obj = m_map.objectAt(cell);
            if (!obj || !pickups.contains(obj->name)) continue;
            const json& e = pickups.at(obj->name);
            Encounter parsed = parse(e);
            if (e.contains("reward")) pickup.reward = parsed.reward;
            if (e.contains("item"))   pickup.item = parsed.item;
            pickup.xp = e.value("xp", pickup.xp);
            pickup.chest = obj->kind == "chest" && pickup.xp > 0;   // a chest that offers a choice
        }
        return std::nullopt;
    } catch (const std::exception& e) {
        return std::string("Encounters parse error: ") + e.what();
    }
}

// ── Movement ──────────────────────────────────────────────────────────────────

std::vector<HexCoord> AdventureSession::route(const HexCoord& to) const {
    if (to == m_hero.pos) return {};
    // Prefer a route that skirts guard zones; if none exists, walk into one.
    auto path = m_map.findPathWeighted(m_hero.pos, to,
        [&](const HexCoord& c) { return c != to && (isEncounter(c) || guardZoneAt(c)); });
    if (path.size() < 2)
        path = m_map.findPathWeighted(m_hero.pos, to,
            [&](const HexCoord& c) { return c != to && isEncounter(c); });
    if (path.size() < 2) return {};
    path.erase(path.begin());
    return path;
}

float AdventureSession::stepCost(const HexCoord& cell) const {
    const MapTile* t = m_map.tileAt(cell);
    return t ? t->moveCost : 1.0f;
}

float AdventureSession::routeCost(const std::vector<HexCoord>& path) const {
    float total = 0.0f;
    for (const auto& c : path) total += stepCost(c);
    return total;
}

int AdventureSession::affordableSteps(const std::vector<HexCoord>& path) const {
    float left = m_moves;
    int   n    = 0;
    for (const auto& c : path) {
        float cost = stepCost(c);
        if (cost > left + 1e-4f) break;
        left -= cost;
        ++n;
    }
    return n;
}

std::vector<AdventureSession::Step> AdventureSession::travel(const HexCoord& to) {
    std::vector<Step> steps;
    m_pending.reset();
    m_pendingZone = false;
    if (m_pendingChest) claimChest(true);  // walked away from the choice: take the gold
    auto path = route(to);
    int  n    = affordableSteps(path);
    for (int i = 0; i < n; ++i) {
        if (isEncounter(path[i])) {   // attack from the neighbouring hex
            m_pending = path[i];
            break;
        }
        m_moves    = std::max(0.0f, m_moves - stepCost(path[i]));
        m_hero.pos = path[i];
        Step step{path[i], enter(path[i]), {}, {}};
        step.found = visitSite(path[i]);
        auto before = m_explored;
        recomputeVisibility();
        for (const auto& c : m_explored)
            if (!before.count(c)) step.revealed.push_back(c);
        m_scenario.fire(*this, "enter_q", {{"q", path[i].q}});
        fireSightings(step.revealed);
        if (!step.capture.empty()) {
            m_scenario.fire(*this, "capture", {{"name", step.capture}});
            m_scenario.fire(*this, "mines_held", {{"count", minesHeld()}});
        }
        if (const MapObjectDef* obj = m_map.objectAt(path[i])) {
            m_scenario.fire(*this, "visit", {{"name", obj->name}});
            m_scenario.fire(*this, "mines_held", {{"count", minesHeld()}});  // gated triggers may now apply
        }
        steps.push_back(std::move(step));
        if (m_pendingChest) break;   // the chest's choice waits for the player
        if (auto guard = guardZoneAt(path[i])) {
            // Walking at the camp is an attack; passing by, the camp attacks you.
            m_pending     = *guard;
            m_pendingZone = *guard != to;
            break;
        }
    }
    if (m_pending)                    // a fight is about to start: last words first
        if (const MapObjectDef* obj = m_map.objectAt(*m_pending))
            m_scenario.fire(*this, "engage", {{"name", obj->name}});
    return steps;
}

// ── Adventure sites ───────────────────────────────────────────────────────────

const AdventureSession::Pickup* AdventureSession::pickupAt(const HexCoord& c) const {
    auto it = m_pickups.find(c);
    return it == m_pickups.end() ? nullptr : &it->second;
}

bool AdventureSession::siteUsed(const HexCoord& c) const {
    const MapObjectDef* obj = m_map.objectAt(c);
    if (!obj) return false;
    switch (obj->type) {
        case ObjType::Mill:     { auto it = m_siteWeek.find(c); return it != m_siteWeek.end() && it->second == week(); }
        case ObjType::Stables:  return m_stablesWeek == week();
        case ObjType::Watchtower:
        case ObjType::Obelisk:
        case ObjType::LearningStone: return m_visitedOnce.count(c) > 0;
        case ObjType::Pickup:
        case ObjType::Artifact: return !m_pickups.count(c);
        default:                return false;
    }
}

std::string AdventureSession::visitSite(const HexCoord& cell) {
    const MapObjectDef* obj = m_map.objectAt(cell);
    if (!obj) return "";
    switch (obj->type) {
        case ObjType::Pickup:
        case ObjType::Artifact: {
            auto it = m_pickups.find(cell);
            if (it == m_pickups.end()) return "";
            if (it->second.chest) { m_pendingChest = cell; return ""; }
            return collect(cell, true);
        }
        case ObjType::Mill: {
            if (siteUsed(cell)) return obj->name + ": nothing more until next week.";
            m_siteWeek[cell] = week();
            ResourcePool pay;
            if (obj->kind == "watermill") pay[Resource::Gold] = 500;
            else { pay[Resource::Wood] = 2; pay[Resource::Stone] = 2; }
            give(pay);
            return obj->name + ": " + describe(pay);
        }
        case ObjType::Stables:
            if (siteUsed(cell)) return obj->name + ": your horses are already fresh this week.";
            m_stablesWeek = week();
            m_movesMax += STABLES_BONUS;
            m_moves    += STABLES_BONUS;
            return obj->name + ": fresh horses, +" + std::to_string((int)STABLES_BONUS) + " movement until the week ends.";
        case ObjType::Watchtower:
            if (!m_visitedOnce.insert(cell).second) return "";
            revealArea(cell, WATCHTOWER_RADIUS);
            return obj->name + ": the land for leagues around is revealed.";
        case ObjType::Obelisk: {
            // HoMM3's puzzle map: each obelisk read shows where the passage is not.
            if (!m_visitedOnce.insert(cell).second) return obj->name + ": the markings are the same as before. You are almost sure of it.";
            auto ruled = giveClue();
            return obj->name + (ruled ? ": the veins in the stone trace the tunnels below. The " + *ruled + " leads nowhere."
                                      : ": the veins in the stone point where you already know to look.");
        }
        case ObjType::LearningStone:
            if (!m_visitedOnce.insert(cell).second) return obj->name + ": you have already read these runes.";
            grantXp(LEARNING_XP);
            return obj->name + ": the companions gain " + std::to_string(LEARNING_XP) + " experience.";
        default:
            return "";
    }
}

std::string AdventureSession::claimChest(bool gold) {
    if (!m_pendingChest) return "";
    HexCoord cell = *m_pendingChest;
    m_pendingChest.reset();
    return collect(cell, gold);
}

std::string AdventureSession::collect(const HexCoord& cell, bool gold) {
    auto it = m_pickups.find(cell);
    if (it == m_pickups.end()) return "";
    Pickup p = it->second;
    m_pickups.erase(it);
    const MapObjectDef* obj = m_map.objectAt(cell);
    std::string text = obj ? obj->name + ": " : std::string();
    if (p.chest && !gold) {
        grantXp(p.xp);
        text += "+" + std::to_string(p.xp) + " experience";
    } else {
        give(p.reward);
        text += describe(p.reward);
        if (!p.chest && p.xp > 0) {                  // a tome: read, and remembered
            grantXp(p.xp);
            text += (describe(p.reward).empty() ? "" : ", ") + std::string("+") + std::to_string(p.xp) + " experience";
        }
    }
    if (!p.item.empty()) {
        addItem(p.item);
        text += (text.back() == ' ' ? "" : ", ") + std::string("found ") + p.item;
    }
    return text;
}

bool AdventureSession::betray(const std::string& objectName, const std::string& bandName,
                              const std::vector<Stack>& army) {
    const MapObjectDef* obj = nullptr;
    for (const auto& o : m_map.objects()) if (o.name == objectName) { obj = &o; break; }
    if (!obj) return false;
    if (auto it = m_control.find(obj->pos); it != m_control.end()) {
        it->second.ownerFaction = Faction::AI;
        m_garrisons.erase(obj->pos);
        for (auto& sc : m_specials)
            if (sc.stationed == obj->pos) sc.stationed.reset();   // the governor flees to the hero
        if (obj->type == ObjType::Town) { m_towns[obj->pos].recruitPool.clear(); growTown(obj->pos); }
    }
    HexCoord at = obj->pos;
    if (at == m_hero.pos)                     // the band forms up beside the hero
        for (int d = 0; d < 6; ++d) {
            HexCoord nb = obj->pos.neighbor(d);
            const MapTile* t = m_map.tileAt(nb);
            if (t && t->passable && !isEncounter(nb) && !m_map.objectAt(nb)) { at = nb; break; }
        }
    Rival r;
    r.id   = static_cast<int>(m_rivals.size()) + 1;
    r.name = bandName;
    r.pos  = at;
    r.home = obj->pos;
    r.army = army;
    r.startPower = power(r.army);
    m_rivals.push_back(r);
    recomputeVisibility();
    return true;
}

int AdventureSession::turncoats(const std::string& unitId, const std::string& bandName) {
    int count = 0;
    std::vector<Stack> loyal;
    for (const auto& st : army()) {
        if (st.id == unitId) count += st.count;
        else loyal.push_back(st);
    }
    setArmy(loyal);
    for (auto& [cell, held] : m_garrisons)
        for (auto it = held.begin(); it != held.end();)
            if (it->id == unitId) { count += it->count; it = held.erase(it); } else ++it;
    if (count == 0) return 0;
    HexCoord at = m_hero.pos;
    for (int d = 0; d < 6; ++d) {
        HexCoord nb = m_hero.pos.neighbor(d);
        const MapTile* t = m_map.tileAt(nb);
        if (t && t->passable && !isEncounter(nb) && !m_map.objectAt(nb) && !rivalAt(nb)) { at = nb; break; }
    }
    Rival r;
    r.id   = static_cast<int>(m_rivals.size()) + 1;
    r.name = bandName;
    r.pos  = at;
    r.home = at;
    r.army = {{unitId, count}};
    r.startPower = power(r.army);
    m_rivals.push_back(r);
    if (!m_pending) {                        // they fall on the camp at once
        m_pending       = r.pos;
        m_pendingAmbush = true;
    }
    return count;
}

std::string AdventureSession::enter(const HexCoord& cell) {
    auto it = m_control.find(cell);
    if (it == m_control.end() || it->second.ownerFaction == Faction::Player) return "";
    it->second.ownerFaction = Faction::Player;
    if (it->second.objType == ObjType::Town) {   // new owner, new roster
        ensureBuildings(cell);
        m_towns[cell].recruitPool.clear();
        growTown(cell);
    }
    const MapObjectDef* obj = m_map.objectAt(cell);
    return obj ? obj->name : std::string(objTypeName(it->second.objType));
}

// ── Encounters ────────────────────────────────────────────────────────────────

std::optional<HexCoord> AdventureSession::guardZoneAt(const HexCoord& c) const {
    for (int d = 0; d < 6; ++d) {
        HexCoord nb = c.neighbor(d);
        if (!m_encounters.count(nb)) continue;
        if (const MapObjectDef* obj = m_map.objectAt(nb); obj && obj->type == ObjType::Guard) return nb;
    }
    return std::nullopt;
}

bool AdventureSession::isEncounter(const HexCoord& c) const {
    return m_encounters.count(c) > 0 || rivalAt(c) != nullptr;
}

const AdventureSession::Encounter* AdventureSession::encounterAt(const HexCoord& c) const {
    if (const Rival* r = rivalAt(c)) {
        Scenario::Json army = Scenario::Json::array();
        for (const auto& s : r->army) army.push_back({{"id", s.id}, {"count", s.count}});
        m_rivalEncounter = Encounter{army.dump(), {}, ""};
        return &m_rivalEncounter;
    }
    auto it = m_encounters.find(c);
    return it == m_encounters.end() ? nullptr : &it->second;
}

AdventureSession::MineFind AdventureSession::resolveEncounter(bool victory) {
    if (!m_pending) return MineFind::None;
    HexCoord cell = *m_pending;
    bool ambush = m_pendingAmbush;
    bool zone   = m_pendingZone;
    m_pending.reset();
    m_pendingAmbush = false;
    m_pendingZone   = false;
    if (auto it = std::find_if(m_rivals.begin(), m_rivals.end(),
            [&](const Rival& r) { return r.alive && r.pos == cell; }); it != m_rivals.end()) {
        if (victory) {
            grantXp(encounterXp(cell));
            it->alive = false;
            m_scenario.fire(*this, "rival_beaten", {{"name", it->name}});
            report(adviser(), "We have broken the " + it->name + ". The land is quieter tonight.");
            if (!ambush) {
                m_moves    = std::max(0.0f, m_moves - stepCost(cell));
                m_hero.pos = cell;
                enter(cell);
                recomputeVisibility();
            }
        }
        return MineFind::None;
    }
    if (!victory) return MineFind::None;

    ResourcePool reward = m_encounters[cell].reward;
    if (hasAbility("Warlord")) reward[Resource::Gold] += reward[Resource::Gold] / 2;
    m_turns.playerFaction().treasury += reward;
    grantXp(encounterXp(cell));
    std::string item = m_encounters[cell].item;
    m_encounters.erase(cell);
    if (!item.empty()) addItem(item);
    if (const MapObjectDef* obj = m_map.objectAt(cell)) {
        m_scenario.fire(*this, "encounter_won", {{"name", obj->name}});
        m_scenario.fire(*this, "cleared");
    }
    if (!zone) {   // attacked in the camp's zone: the hero holds its ground
        m_moves    = std::max(0.0f, m_moves - stepCost(cell));
        m_hero.pos = cell;
        enter(cell);
    }
    recomputeVisibility();

    auto find = m_mineFinds.find(cell);
    if (find == m_mineFinds.end()) return MineFind::None;
    MineFind result = find->second;
    if (result == MineFind::Loot) {
        ResourcePool loot;
        loot[Resource::Gold]     = 1500;
        loot[Resource::Obsidian] = 2;
        loot[Resource::Crystal]  = 2;
        m_turns.playerFaction().treasury += loot;
    }
    if (result == MineFind::Passage) m_won = true;
    m_mineFinds.erase(find);
    return result;
}

// ── Story ─────────────────────────────────────────────────────────────────────

void AdventureSession::fireSightings(const std::vector<HexCoord>& revealed) {
    for (const auto& c : revealed) {
        const MapObjectDef* obj = m_map.objectAt(c);
        if (!obj) continue;
        m_scenario.fire(*this, "see", {{"name", obj->name}});
        std::string type = obj->type == ObjType::OldMine ? "old_mine"
                         : obj->type == ObjType::QuestGiver ? "quest_giver"
                         : obj->type == ObjType::Town ? "town"
                         : obj->type == ObjType::Obelisk ? "obelisk" : "other";
        m_scenario.fire(*this, "see_type", {{"type", type}});
    }
}

void AdventureSession::revealArea(const HexCoord& center, int radius) {
    for (const auto& [coord, tile] : m_map)
        if (coord.distanceTo(center) <= radius) m_explored.insert(coord);
}

void AdventureSession::give(const ResourcePool& resources) {
    m_turns.playerFaction().treasury += resources;
}

std::optional<std::string> AdventureSession::giveClue() {
    // Prefer mines the player has not searched yet, in map order for stable clues.
    for (const auto& obj : m_map.objects()) {
        if (obj.type != ObjType::OldMine || m_passage == obj.pos) continue;
        if (m_ruledOut.count(obj.pos) || !m_mineFinds.count(obj.pos)) continue;
        m_ruledOut.insert(obj.pos);
        return obj.name;
    }
    return std::nullopt;
}

void AdventureSession::addItem(const std::string& id) {
    if (id.empty() || !m_items.insert(id).second) return;
    m_scenario.fire(*this, "item", {{"item", id}});
}

std::optional<std::string> AdventureSession::acceptOffer(const std::string& id) {
    Scenario::Offer* offer = m_scenario.offer(id);
    if (!offer || offer->taken) return "That offer is no longer open.";
    const MapObjectDef* here = m_map.objectAt(m_hero.pos);
    if (!here || here->name != offer->at) return "You must be at " + offer->at + ".";
    ResourcePool& treasury = m_turns.playerFaction().treasury;
    if (!treasury.canAfford(offer->cost)) return "You cannot afford it.";
    if (!offer->item.empty() && !hasItem(offer->item)) return "You do not carry what is asked for.";
    treasury -= offer->cost;
    if (!offer->item.empty()) m_items.erase(offer->item);
    offer->taken = true;
    Scenario::Json then = offer->then;
    if (!offer->quest.empty()) then.push_back({{"quest_done", offer->quest}});
    m_scenario.run(*this, then);
    return std::nullopt;
}

int AdventureSession::minesHeld(int faction) const {
    int n = 0;
    for (const auto& [coord, ctrl] : m_control)
        if (ctrl.ownerFaction == faction && ctrl.objType != ObjType::Town && ctrl.objType != ObjType::Dwelling) ++n;
    return n;
}

// ── Calendar ──────────────────────────────────────────────────────────────────

std::string AdventureSession::endDay() {
    runRivals();
    TownStateMap none;  // weekly growth is per-roster here, not TurnManager's all-units tick
    payUpkeep();
    m_turns.playerFaction().treasury += extraIncome(Faction::Player);   // halls, governors
    m_turns.nextDay(m_hero, m_control, *m_resources, none);
    m_movesMax = DEFAULT_MOVES + movesBonus();
    if (dayOfWeek() == 7)
        for (auto& [coord, town] : m_towns) growTown(coord);
    m_moves = m_movesMax;
    std::string event = m_turns.lastEvent();
    // TurnManager ticks weekly growth on day 7, 14, 21 …
    if (event.empty() && dayOfWeek() == 7)
        event = "Recruits gather in the towns for the coming week.";
    m_scenario.fire(*this, "day", {{"day", day()}});
    return event;
}

// ── Army ──────────────────────────────────────────────────────────────────────

std::vector<AdventureSession::Stack> AdventureSession::army() const {
    std::vector<Stack> out;
    for (const auto& slot : m_hero.army)
        if (!slot.isEmpty()) out.push_back({slot.unitType->id, slot.count});
    return out;
}

void AdventureSession::setArmy(const std::vector<Stack>& stacks) {
    m_hero.army = {};
    for (const auto& s : stacks)
        if (const UnitType* u = m_resources->unit(s.id); u && s.count > 0)
            m_hero.addUnit(u, s.count);
}

// ── Garrisons ─────────────────────────────────────────────────────────────────

const std::vector<AdventureSession::Stack>* AdventureSession::garrison(const HexCoord& c) const {
    auto it = m_garrisons.find(c);
    return it == m_garrisons.end() || it->second.empty() ? nullptr : &it->second;
}

std::optional<std::string> AdventureSession::transfer(const HexCoord& site, const std::string& unitId,
                                                      int count, bool toGarrison) {
    if (owner(site) != Faction::Player) return "You can only garrison mines and towns you hold.";
    if (m_hero.pos != site)             return "Your hero must be there to move troops.";
    if (count <= 0)                     return "Choose how many to move.";
    auto& held = m_garrisons[site];
    auto inGarrison = std::find_if(held.begin(), held.end(), [&](const Stack& s) { return s.id == unitId; });
    if (toGarrison) {
        ArmySlot* slot = m_hero.findStack(unitId);
        if (!slot || slot->count < count) return "Your army does not have that many.";
        if (m_hero.armySize() == 1 && slot->count == count) return "The hero cannot travel without an army.";
        if (inGarrison == held.end() && (int)held.size() >= 5) return "The garrison has no free slot.";
        slot->count -= count;
        if (slot->count == 0) *slot = ArmySlot{};
        if (inGarrison == held.end()) held.push_back({unitId, count});
        else inGarrison->count += count;
    } else {
        if (inGarrison == held.end() || inGarrison->count < count) return "The garrison does not have that many.";
        const UnitType* u = m_resources->unit(unitId);
        if (!u) return "Unknown unit.";
        if (!m_hero.findStack(unitId) && m_hero.armyFull()) return "Your army has no free slot.";
        m_hero.addUnit(u, count);
        inGarrison->count -= count;
        if (inGarrison->count == 0) held.erase(inGarrison);
    }
    return std::nullopt;
}

// ── Towns ─────────────────────────────────────────────────────────────────────

std::string AdventureSession::rosterFor(int owner) {
    switch (owner) {
        case Faction::Player: return "cruths";
        case Faction::AI:     return "shariw";
        default:              return "";
    }
}

void AdventureSession::growTown(const HexCoord& c) {
    if (const MapObjectDef* obj = m_map.objectAt(c); obj && obj->type == ObjType::Dwelling) {
        if (const UnitType* u = m_resources->unit(obj->kind)) m_towns[c].recruitPool[u->id] += u->weeklyGrowth;
        return;
    }
    std::string roster = rosterFor(owner(c));
    if (roster.empty()) return;
    auto& pool = m_towns[c].recruitPool;
    const double bonus = growthBonus(c);
    for (const UnitType* u : m_resources->unitsByTier())
        if (u->faction == roster && canRecruitHere(c, u->id))
            pool[u->id] += static_cast<int>(u->weeklyGrowth * (1.0 + bonus) + 0.5);
}

void AdventureSession::ensureBuildings(const HexCoord& c) {
    const MapObjectDef* obj = m_map.objectAt(c);
    if (!obj || obj->type != ObjType::Town) return;
    const std::string roster = rosterFor(owner(c) == Faction::AI ? Faction::AI : Faction::Player);
    auto& built = m_towns[c].buildings;
    for (const BuildingDef* b : m_resources->buildingsFor(roster))
        if (built.count(b->id)) return;           // already has this faction's buildings
    const TownDef* own = m_resources->townDef(obj->name);
    bool any = false;                            // the town's own starting set, if it has one for this faction
    if (own)
        for (const auto& id : own->starting)
            if (const BuildingDef* b = m_resources->building(id); b && b->faction == roster) { built.insert(id); any = true; }
    if (any) return;
    for (const BuildingDef* b : m_resources->buildingsFor(roster))
        if (b->starting) built.insert(b->id);
}

bool AdventureSession::canRecruitHere(const HexCoord& c, const std::string& unitId) const {
    const BuildingDef* dwelling = m_resources->dwellingFor(unitId);
    if (!dwelling) return true;
    const TownState* t = town(c);
    return t && t->buildings.count(dwelling->id);
}

double AdventureSession::growthBonus(const HexCoord& c) const {
    double best = 0;
    if (const TownState* t = town(c))
        for (const auto& id : t->buildings)
            if (const BuildingDef* b = m_resources->building(id)) best = std::max(best, b->growth);
    if (owner(c) == Faction::Player) best += heroEffects().growth;   // Quartermaster: Supply Lines
    return best;
}

int AdventureSession::townGold(const HexCoord& c) const {
    int best = 0;
    if (const TownState* t = town(c))
        for (const auto& id : t->buildings)
            if (const BuildingDef* b = m_resources->building(id)) best = std::max(best, b->income);
    return best > 0 ? best : m_resources->mineIncome(ObjType::Town)[Resource::Gold];
}

bool AdventureSession::hasMarket() const {
    for (const auto& [c, t] : m_towns) {
        if (owner(c) != Faction::Player) continue;
        for (const auto& id : t.buildings)
            if (const BuildingDef* b = m_resources->building(id); b && b->market) return true;
    }
    return false;
}

AdventureSession::ArmyBonus AdventureSession::armyBonus() const {
    ArmyBonus out;
    for (const auto& [c, t] : m_towns) {
        if (owner(c) != Faction::Player) continue;
        for (const auto& id : t.buildings)
            if (const BuildingDef* b = m_resources->building(id)) {
                out.attack = std::max(out.attack, b->attackBonus);
                out.readiedShot |= b->readiedShot;
            }
    }
    const HeroEffects skills = heroEffects();       // the commander's path
    out.attack  += skills.attack;
    out.defense += skills.defense;
    out.readiedShot |= skills.readiedShot;
    for (const auto& [slot, id] : m_equipped)
        if (const WondrousItem* item = m_resources->item(id))
            for (const auto& e : item->passiveEffects) {
                if (e.stat == "attack")       out.attack  += e.amount;
                else if (e.stat == "defense") out.defense += e.amount;
                else if (e.stat == "speed")   out.speed   += e.amount;
            }
    return out;
}

const std::vector<std::string>& AdventureSession::equipSlots() {
    static const std::vector<std::string> slots = {"helm", "amulet", "armor", "weapon", "boots", "trinket1", "trinket2"};
    return slots;
}

std::vector<std::string> AdventureSession::backpack() const {
    std::vector<std::string> out;
    for (const auto& id : m_items) {
        bool worn = false;
        for (const auto& [slot, w] : m_equipped) worn |= w == id;
        if (!worn) out.push_back(id);
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::optional<std::string> AdventureSession::equip(const std::string& itemId) {
    if (!m_items.count(itemId)) return "You do not carry that.";
    for (const auto& [slot, w] : m_equipped) if (w == itemId) return "Already worn.";
    const WondrousItem* item = m_resources->item(itemId);
    if (!item) return "Unknown item.";
    std::vector<std::string> fits;
    if (item->slot == "trinket") fits = {"trinket1", "trinket2"};
    else fits = {item->slot};
    for (const auto& slot : fits)
        if (!m_equipped.count(slot)) { m_equipped[slot] = itemId; return std::nullopt; }
    // Full: swap with the first slot, unless what is there is cursed.
    const std::string& slot = fits.front();
    const WondrousItem* worn = m_resources->item(m_equipped[slot]);
    if (worn && worn->cursed) return worn->name + " is cursed and will not come off.";
    m_equipped[slot] = itemId;
    return std::nullopt;
}

std::optional<std::string> AdventureSession::unequip(const std::string& slot) {
    auto it = m_equipped.find(slot);
    if (it == m_equipped.end()) return "Nothing is worn there.";
    const WondrousItem* worn = m_resources->item(it->second);
    if (worn && worn->cursed) return worn->name + " is cursed and will not come off.";
    m_equipped.erase(it);
    return std::nullopt;
}

std::string AdventureSession::buildBlocker(const HexCoord& c, const std::string& id) const {
    const TownState* t = town(c);
    const MapObjectDef* obj = m_map.objectAt(c);
    if (!t || !obj || obj->type != ObjType::Town) return "There is no town here.";
    if (owner(c) != Faction::Player) return "This town does not answer to you.";
    const BuildingDef* b = m_resources->building(id);
    if (!b || b->faction != rosterFor(Faction::Player)) return "Unknown building.";
    if (t->buildings.count(id)) return "Already built.";
    for (const auto& need : b->requires)
        if (!t->buildings.count(need)) {
            const BuildingDef* n = m_resources->building(need);
            return "Needs " + (n ? n->name : need) + ".";
        }
    if (t->builtOnDay == day()) {
        const BuildingDef* today = m_resources->building(t->lastBuilt);
        return "Tomorrow: " + (today ? today->name : std::string("a building")) + " went up here today.";
    }
    if (!m_turns.playerFaction().treasury.canAfford(b->cost)) return "You cannot afford it.";
    return "";
}

std::optional<std::string> AdventureSession::build(const HexCoord& c, const std::string& id) {
    if (std::string why = buildBlocker(c, id); !why.empty()) return why;
    const BuildingDef* b = m_resources->building(id);
    TownState& t = m_towns[c];
    m_turns.playerFaction().treasury -= b->cost;
    t.buildings.insert(id);
    t.builtOnDay = day();
    t.lastBuilt = id;
    if (!b->unlocks.empty())                     // a new dwelling: its first week's recruits
        if (const UnitType* u = m_resources->unit(b->unlocks))
            t.recruitPool[u->id] += static_cast<int>(u->weeklyGrowth * (1.0 + growthBonus(c)) + 0.5);
    return std::nullopt;
}

namespace {
// Marketplace value of one unit of each resource, in gold.
int resourceValue(Resource r) {
    switch (r) {
        case Resource::Gold:     return 1;
        case Resource::Wood:     case Resource::Stone:   return 150;
        default:                 return 400;
    }
}
}

int AdventureSession::tradeQuote(Resource give, Resource get, int amount) {
    if (give == get || amount <= 0) return 0;
    // The market buys at half value and sells at one and a half; gold is gold.
    const double sold = amount * (give == Resource::Gold ? 1.0 : 0.5 * resourceValue(give));
    const double price = get == Resource::Gold ? 1.0 : 1.5 * resourceValue(get);
    return static_cast<int>(sold / price);
}

AdventureSession::TradeResult AdventureSession::trade(Resource give, Resource get, int amount) {
    if (!hasMarket()) return {0, "You need a Marketplace in a town you hold."};
    ResourcePool& treasury = m_turns.playerFaction().treasury;
    if (amount <= 0 || treasury[give] < amount) return {0, "You do not have that much to trade."};
    const int received = tradeQuote(give, get, amount);
    if (received <= 0) return {0, "That is not enough to buy even one."};
    treasury[give] -= amount;
    treasury[get] += received;
    return {received, ""};
}

ResourcePool AdventureSession::extraIncome(int faction) const {
    ResourcePool extra;
    if (faction != Faction::Player) return extra;
    const int flat = m_resources->mineIncome(ObjType::Town)[Resource::Gold];
    for (const auto& [c, ctrl] : m_control)
        if (ctrl.objType == ObjType::Town && ctrl.ownerFaction == faction)
            extra[Resource::Gold] += townGold(c) - flat;       // halls beyond the flat town income
    for (const auto& sc : m_specials)   // governors
        if (sc.stationed && owner(*sc.stationed) == Faction::Player && sc.unpaidDays < SULK_DAYS)
            extra[Resource::Gold] += 100 * sc.level;
    extra[Resource::Gold] += heroEffects().gold;   // Quartermaster: Requisition
    return extra;
}

const TownState* AdventureSession::town(const HexCoord& c) const {
    auto it = m_towns.find(c);
    return it == m_towns.end() ? nullptr : &it->second;
}

std::optional<std::string> AdventureSession::recruit(const HexCoord& at, const std::string& unitId, int count) {
    auto it = m_towns.find(at);
    if (it == m_towns.end())            return "There is no town here.";
    if (owner(at) != Faction::Player)   return "This town does not answer to you.";
    if (m_hero.pos != at)               return "Your hero must be in the town to recruit.";
    const UnitType* u = m_resources->unit(unitId);
    if (!u || count <= 0)               return "Unknown unit.";
    if (!canRecruitHere(at, unitId)) {
        const BuildingDef* d = m_resources->dwellingFor(unitId);
        return "Build the " + (d ? d->name : std::string("dwelling")) + " to recruit them.";
    }
    int& available = it->second.recruitPool[unitId];
    if (count > available)              return "Not enough recruits available this week.";
    ResourcePool cost;
    for (int i = 0; i < RESOURCE_COUNT; ++i) cost.amounts[i] = u->cost.amounts[i] * count;
    ResourcePool& treasury = m_turns.playerFaction().treasury;
    if (!treasury.canAfford(cost))      return "You cannot afford that many.";
    if (!m_hero.findStack(unitId) && m_hero.armyFull()) return "Your army has no free slot.";
    m_hero.addUnit(u, count);
    treasury -= cost;
    available -= count;
    return std::nullopt;
}

// ── State ─────────────────────────────────────────────────────────────────────

ResourcePool AdventureSession::dailyIncome(int faction) const {
    ResourcePool total;
    for (const auto& [coord, ctrl] : m_control)
        if (ctrl.ownerFaction == faction)
            total += m_resources->mineIncome(ctrl.objType);
    total += extraIncome(faction);
    return total;
}

int AdventureSession::owner(const HexCoord& c) const {
    auto it = m_control.find(c);
    return it == m_control.end() ? Faction::Neutral : it->second.ownerFaction;
}

// ── Fog of war ────────────────────────────────────────────────────────────────

void AdventureSession::recomputeVisibility() {
    m_visible.clear();
    revealFrom(m_hero.pos, SIGHT_RADIUS + sightBonus());
    for (const auto& [coord, ctrl] : m_control)
        if (ctrl.ownerFaction == Faction::Player)
            revealFrom(coord, OWNED_SIGHT_RADIUS);
    m_explored.insert(m_visible.begin(), m_visible.end());
}

// BFS out to 'radius'. Mountains and walls are seen but block sight past them.
void AdventureSession::revealFrom(const HexCoord& origin, int radius) {
    std::unordered_set<HexCoord> seen{origin};
    std::queue<std::pair<HexCoord, int>> frontier;
    frontier.push({origin, 0});
    m_visible.insert(origin);
    while (!frontier.empty()) {
        auto [coord, dist] = frontier.front();
        frontier.pop();
        if (dist >= radius) continue;
        for (int d = 0; d < 6; ++d) {
            HexCoord nb = coord.neighbor(d);
            const MapTile* t = m_map.tileAt(nb);
            if (!t || !seen.insert(nb).second) continue;
            m_visible.insert(nb);
            if (!blocksSight(t->terrain)) frontier.push({nb, dist + 1});
        }
    }
}
