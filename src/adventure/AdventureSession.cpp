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
    m_startArgs.reset();
    m_pendingAmbush = false;
    m_lost = false;
    m_lostReason.clear();

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
    for (auto& [coord, town] : m_towns) growTown(coord);  // first week's recruits
    for (const auto& obj : m_map.objects())
        if (obj.type == ObjType::Town && obj.factionId == Faction::AI) spawnRival(obj.pos, 1);
    if (!triggersPath.empty())
        if (auto err = m_scenario.load(triggersPath)) return err;

    joinSpecial("ushari");                 // the expedition's commander
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
        return std::nullopt;
    } catch (const std::exception& e) {
        return std::string("Encounters parse error: ") + e.what();
    }
}

// ── Movement ──────────────────────────────────────────────────────────────────

std::vector<HexCoord> AdventureSession::route(const HexCoord& to) const {
    if (to == m_hero.pos) return {};
    auto path = m_map.findPathWeighted(m_hero.pos, to,
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
    auto path = route(to);
    int  n    = affordableSteps(path);
    for (int i = 0; i < n; ++i) {
        if (isEncounter(path[i])) {   // attack from the neighbouring hex
            m_pending = path[i];
            break;
        }
        m_moves    = std::max(0.0f, m_moves - stepCost(path[i]));
        m_hero.pos = path[i];
        Step step{path[i], enter(path[i]), {}};
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
        if (const MapObjectDef* obj = m_map.objectAt(path[i]))
            m_scenario.fire(*this, "visit", {{"name", obj->name}});
        steps.push_back(std::move(step));
    }
    return steps;
}

std::string AdventureSession::enter(const HexCoord& cell) {
    auto it = m_control.find(cell);
    if (it == m_control.end() || it->second.ownerFaction == Faction::Player) return "";
    it->second.ownerFaction = Faction::Player;
    if (it->second.objType == ObjType::Town) {   // new owner, new roster
        m_towns[cell].recruitPool.clear();
        growTown(cell);
    }
    const MapObjectDef* obj = m_map.objectAt(cell);
    return obj ? obj->name : std::string(objTypeName(it->second.objType));
}

// ── Encounters ────────────────────────────────────────────────────────────────

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
    m_pending.reset();
    m_pendingAmbush = false;
    if (auto it = std::find_if(m_rivals.begin(), m_rivals.end(),
            [&](const Rival& r) { return r.alive && r.pos == cell; }); it != m_rivals.end()) {
        if (victory) {
            grantXp(std::max(40, static_cast<int>(power(it->army) / 3)));
            it->alive = false;
            report("Ushari", "The " + it->name + " is broken. The desert is quieter tonight.");
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
    grantXp(std::max(20, static_cast<int>(power(guardsOf(cell)) / 3)));
    std::string item = m_encounters[cell].item;
    m_encounters.erase(cell);
    if (!item.empty()) addItem(item);
    if (const MapObjectDef* obj = m_map.objectAt(cell))
        m_scenario.fire(*this, "encounter_won", {{"name", obj->name}});
    m_moves    = std::max(0.0f, m_moves - stepCost(cell));
    m_hero.pos = cell;
    enter(cell);
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
                         : obj->type == ObjType::Town ? "town" : "other";
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
        if (ctrl.ownerFaction == faction && ctrl.objType != ObjType::Town) ++n;
    return n;
}

// ── Calendar ──────────────────────────────────────────────────────────────────

std::string AdventureSession::endDay() {
    runRivals();
    TownStateMap none;  // weekly growth is per-roster here, not TurnManager's all-units tick
    payUpkeep();
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
        case Faction::Player: return "ivory_compact";
        case Faction::AI:     return "shariw";
        default:              return "";
    }
}

void AdventureSession::growTown(const HexCoord& c) {
    std::string roster = rosterFor(owner(c));
    if (roster.empty()) return;
    auto& pool = m_towns[c].recruitPool;
    for (const UnitType* u : m_resources->unitsByTier())
        if (u->faction == roster) pool[u->id] += u->weeklyGrowth;
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
    if (faction == Faction::Player)
        for (const auto& sc : m_specials)   // governors
            if (sc.stationed && owner(*sc.stationed) == Faction::Player && sc.unpaidDays < SULK_DAYS)
                total[Resource::Gold] += 100 * sc.level;
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
