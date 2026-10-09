// Specials.cpp — special characters for AdventureSession.
//
// SCs travel with the hero (earning XP from victories and granting their
// unlocked abilities) or govern an owned town (+100 gold per level per day,
// no XP). Upkeep rises with level; unpaid SCs sulk, then leave. Who they are
// (name, title, wage, what they say) comes from units.json; at most two ride.
#include "AdventureSession.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace {

// What a companion says when units.json gives no line for the moment ("join": silent).
const char* defaultLine(const std::string& moment) {
    if (moment == "unpaid")  return "Three days without pay. Do not expect my best until the treasury remembers me.";
    if (moment == "paid")    return "Paid at last. Very well, I am with you again.";
    if (moment == "leaves")  return "A week unpaid. I have served better paymasters. Farewell.";
    if (moment == "wounded") return "I will live. Give me three days before you ask me to hold a line again.";
    return "";
}

} // namespace

std::string AdventureSession::companionLine(const std::string& id, const std::string& moment) const {
    if (const UnitType* u = m_resources ? m_resources->unit(id) : nullptr)
        if (auto it = u->lines.find(moment); it != u->lines.end()) return it->second;
    return defaultLine(moment);
}

int AdventureSession::upkeepOf(const Special& sc) const {
    const UnitType* u = m_resources ? m_resources->unit(sc.id) : nullptr;
    const int base = u ? u->upkeep : 100;
    return upkeepFor(sc.level) * base / 100;
}

int AdventureSession::ridingCount() const {
    return static_cast<int>(std::count_if(m_specials.begin(), m_specials.end(),
                                          [](const Special& sc) { return !sc.stationed; }));
}

std::vector<AdventureSession::Ability> AdventureSession::abilitiesOf(const std::string& id) {
    if (id == "ushari") return {
        {1, "Drillmaster",   "+1 movement per day: the column never straggles."},
        {3, "Eagle Eye",     "+2 sight radius for the hero."},
        {5, "Warlord",       "Beaten guards pay 50% more gold."},
        {8, "Legend of the Sands", "+2 more movement per day."},
    };
    if (id == "kharim") return {
        {1, "Old Maps",      "+1 sight radius; he knows where to look."},
        {3, "Surveyor",      "+1 movement per day along forgotten paths."},
        {5, "Deep Reading",  "+2 sight radius."},
    };
    if (id == "maerwen") return {
        {1, "Heron's Patience", "In battle, strikes any enemy that steps out of her reach (once per round)."},
        {3, "Lowland Roads",    "+1 movement per day: she knows every Hale road."},
    };
    return {};
}

int AdventureSession::xpForLevel(int level) {
    // Fewer, weightier levels: the north takes a commander to about level 5,
    // the whole campaign to 10.
    static const int table[] = {0, 0, 100, 250, 450, 700, 1500, 2400, 3600, 5200, 7200};
    return level <= 10 ? table[std::max(level, 1)] : 7200 + (level - 10) * 2500;
}

int AdventureSession::companionXpForLevel(const std::string& id, int level) {
    // Ushari starts powerful; her levels should span several encounters.
    // Keep earned XP and existing saved levels, but slow future advancement.
    return xpForLevel(level) * (id == "ushari" ? 3 : 1);
}

int AdventureSession::upkeepFor(int level) {
    if (level >= 8) return 600;
    if (level >= 5) return 300;
    if (level >= 3) return 200;
    return 100;
}

AdventureSession::Special* AdventureSession::addSpecial(const std::string& id) {
    for (const auto& sc : m_specials) if (sc.id == id) return nullptr;
    auto returning = std::find_if(m_departedSpecials.begin(), m_departedSpecials.end(),
                                 [&](const Special& sc) { return sc.id == id; });
    if (returning != m_departedSpecials.end()) {
        m_specials.push_back(*returning);
        m_specials.back().stationed.reset();
        m_specials.back().waiting = false;
        m_departedSpecials.erase(returning);
        return &m_specials.back();
    }
    // A companion is a unit of faction "companion" (name, title, upkeep, lines).
    const UnitType* unit = m_resources ? m_resources->unit(id) : nullptr;
    if (!unit || !unit->isCompanion()) return nullptr;
    m_specials.push_back({unit->id, unit->name, unit->title, 1, 0, std::nullopt, 0});
    return &m_specials.back();
}

void AdventureSession::joinSpecial(const std::string& id) {
    const bool others = !m_specials.empty();
    Special* sc = addSpecial(id);
    if (!sc) return;
    const std::string name = sc->name;
    if (others)
        if (std::string line = companionLine(id, "join"); !line.empty()) report(name, line);
    if (ridingCount() > MAX_RIDING) askWhoStaysBehind(id);
    recomputeVisibility();
}

void AdventureSession::askWhoStaysBehind(const std::string& newcomer) {
    // The nearest town the player holds is where the one left behind will govern.
    std::string town;
    int best = 1 << 30;
    for (const auto& obj : m_map.objects())
        if (obj.type == ObjType::Town && owner(obj.pos) == Faction::Player && obj.pos.distanceTo(m_hero.pos) < best) {
            best = obj.pos.distanceTo(m_hero.pos);
            town = obj.name;
        }
    Scenario::Json options = Scenario::Json::array();
    std::vector<const Special*> riders;
    for (const auto& sc : m_specials) if (!sc.stationed && sc.id != newcomer) riders.push_back(&sc);
    for (const auto& sc : m_specials) if (sc.id == newcomer) riders.push_back(&sc);   // the newcomer last
    for (const Special* sc : riders) {
        if (m_scenario.essential(sc->id)) continue;   // the story needs them with the hero
        options.push_back({{"id", sc->id}, {"label", sc->name + " stays behind"},
                           {"detail", town.empty() ? sc->name + " waits until you send word."
                                                   : sc->name + " goes to govern " + town + "."},
                           {"then", Scenario::Json::array({{{"stay_behind", sc->id}}})}});
    }
    if (options.empty()) {   // everyone is essential: nobody can be asked to stay
        std::cerr << "[AdventureSession] " << ridingCount() << " companions ride, all essential; none stays behind\n";
        return;
    }
    const std::string newName = m_specials.back().id == newcomer ? m_specials.back().name : newcomer;
    m_scenario.raiseChoice({{"id", "stay_behind"}, {"kind", "party"}, {"newcomer", newcomer},
                            {"title", "Who stays behind?"},
                            {"text", "Only " + std::to_string(MAX_RIDING) + " companions can ride with you. With "
                                     + newName + " there are " + std::to_string(ridingCount()) + ". One stays behind"
                                     + (town.empty() ? "." : ", in " + town + ".")},
                            {"options", options}});
}

bool AdventureSession::stayBehind(const std::string& id) {
    auto it = std::find_if(m_specials.begin(), m_specials.end(),
                           [&](const Special& sc) { return sc.id == id && !sc.stationed; });
    if (it == m_specials.end()) return false;
    const MapObjectDef* town = nullptr;
    for (const auto& obj : m_map.objects())
        if (obj.type == ObjType::Town && owner(obj.pos) == Faction::Player
            && (!town || obj.pos.distanceTo(m_hero.pos) < town->pos.distanceTo(m_hero.pos)))
            town = &obj;
    const std::string name = it->name;
    std::string text;
    if (town) {
        it->stationed = town->pos;
        text = name + " stays behind to govern " + town->name + ".";
    } else {
        m_departedSpecials.push_back(*it);
        m_departedSpecials.back().stationed.reset();
        m_departedSpecials.back().waiting = true;
        m_specials.erase(it);
        text = name + " stays behind and will wait for word from you.";
    }
    m_scenario.pushStory({{"kind", "party"}, {"id", id}, {"name", name},
                          {"where", town ? town->name : std::string{}}, {"text", text}});
    recomputeVisibility();
    return true;
}

void AdventureSession::leaveSpecial(const std::string& id) {
    auto it = std::find_if(m_specials.begin(), m_specials.end(), [&](const Special& sc) { return sc.id == id; });
    if (it == m_specials.end()) return;
    m_departedSpecials.push_back(*it);
    m_departedSpecials.back().stationed.reset();
    m_specials.erase(it);
    recomputeVisibility();
}

std::vector<AdventureSession::Companion> AdventureSession::battleCompanions() const {
    std::vector<Companion> out;
    for (const auto& sc : m_specials)
        if (!sc.stationed && !isWounded(sc) && sc.unpaidDays < SULK_DAYS && m_resources->unit(sc.id)
            && (int)out.size() < MAX_RIDING)
            out.push_back({sc.id, sc.level});
    return out;
}

void AdventureSession::companionsFell(const std::vector<std::string>& fallen, bool battleLost) {
    for (const auto& id : fallen) {
        auto sc = std::find_if(m_specials.begin(), m_specials.end(), [&](const Special& s) { return s.id == id; });
        if (sc == m_specials.end()) continue;
        if (battleLost && !m_scenario.essential(id)) {
            report("Commander", sc->name + " fell, and no one was left to carry " + sc->name
                   + " from the field. " + sc->name + " is gone.");
            m_specials.erase(sc);
        } else {
            sc->woundedUntil = day() + WOUND_DAYS;
            report(sc->name, companionLine(sc->id, "wounded"));
        }
    }
    recomputeVisibility();
}

bool AdventureSession::hasAbility(const std::string& abilityName) const {
    for (const auto& sc : m_specials) {
        if (sc.stationed || sc.unpaidDays >= SULK_DAYS || isWounded(sc)) continue;
        for (const auto& a : abilitiesOf(sc.id))
            if (a.name == abilityName && sc.level >= a.level) return true;
    }
    return false;
}

float AdventureSession::movesBonus() const {
    float bonus = 0;
    if (hasAbility("Drillmaster")) bonus += 1;
    if (hasAbility("Legend of the Sands")) bonus += 2;
    if (hasAbility("Surveyor")) bonus += 1;
    if (hasAbility("Lowland Roads")) bonus += 1;
    bonus += heroEffects().moves;
    if (m_stablesWeek != 0 && m_stablesWeek == week()) bonus += STABLES_BONUS;
    return bonus;
}

int AdventureSession::sightBonus() const {
    int bonus = 0;
    if (hasAbility("Eagle Eye")) bonus += 2;
    if (hasAbility("Old Maps")) bonus += 1;
    if (hasAbility("Deep Reading")) bonus += 2;
    return bonus + heroEffects().sight;
}

int AdventureSession::encounterXp(const HexCoord& c) const {
    if (const Rival* r = rivalAt(c)) return std::max(40, static_cast<int>(power(r->army) / 3));
    if (owner(c) == Faction::AI && garrison(c)) return std::max(40, static_cast<int>(power(guardsOf(c)) / 3));
    if (!m_encounters.count(c)) return 0;
    return std::max(20, static_cast<int>(power(guardsOf(c)) / 3));
}

void AdventureSession::grantXp(int xp) {
    if (xp <= 0) return;
    m_heroProgress.xp += xp;
    while (m_heroProgress.level < m_levelCap && m_heroProgress.xp >= xpForLevel(m_heroProgress.level + 1)) {
        LevelUp up{++m_heroProgress.level, "", ""};
        for (const auto& p : m_paths)
            if (p.id == m_path)
                for (const auto& n : p.nodes)
                    if (n.level == up.level) { m_learned.push_back(n.id); up.skill = n.name; up.text = n.text; }
        m_levelUps.push_back(up);
    }
    capXp();
    for (auto& sc : m_specials) {
        if (sc.stationed || isWounded(sc) || sc.unpaidDays >= SULK_DAYS) continue;
        sc.xp += xp;
        while (sc.xp >= companionXpForLevel(sc.id, sc.level + 1)) {
            ++sc.level;
            std::string line = sc.name + " reaches level " + std::to_string(sc.level) + ".";
            for (const auto& a : abilitiesOf(sc.id))
                if (a.level == sc.level) line += " New ability: " + a.name + " — " + a.text;
            report("", line);   // the narrator says it; the companion does not announce herself
        }
    }
    recomputeVisibility();
}

void AdventureSession::capXp() {
    if (m_heroProgress.level < m_levelCap) return;
    if (m_capReachedDay < 0) m_capReachedDay = day();
    int ceiling = xpForLevel(std::max(m_heroProgress.level, m_levelCap));
    if (m_heroProgress.xp > ceiling) {
        m_xpLostToCap += m_heroProgress.xp - ceiling;
        m_heroProgress.xp = ceiling;
    }
}

int AdventureSession::upkeepPerDay() const {
    int total = 0;
    for (const auto& sc : m_specials) total += upkeepOf(sc);
    return total;
}

void AdventureSession::payUpkeep() {
    ResourcePool& treasury = m_turns.playerFaction().treasury;
    for (auto it = m_specials.begin(); it != m_specials.end();) {
        int cost = upkeepOf(*it);
        if (cost == 0) { it->unpaidDays = 0; ++it; continue; }   // household: no wage, no sulk
        bool crystal = it->level >= 8 && dayOfWeek() == 7;  // legends also want a crystal a week
        bool paid = treasury[Resource::Gold] >= cost && (!crystal || treasury[Resource::Crystal] >= 1);
        if (paid) {
            treasury[Resource::Gold] -= cost;
            if (crystal) treasury[Resource::Crystal] -= 1;
            if (it->unpaidDays >= SULK_DAYS) report(it->name, companionLine(it->id, "paid"));
            it->unpaidDays = 0;
        } else {
            ++it->unpaidDays;
            if (it->unpaidDays == SULK_DAYS)
                report(it->name, companionLine(it->id, "unpaid"));
            if (it->unpaidDays >= LEAVE_DAYS && !m_scenario.essential(it->id)) {
                report(it->name, companionLine(it->id, "leaves"));
                it = m_specials.erase(it);
                continue;
            }
        }
        ++it;
    }
}

std::optional<std::string> AdventureSession::station(const std::string& id, bool stay) {
    auto sc = std::find_if(m_specials.begin(), m_specials.end(), [&](const Special& s) { return s.id == id; });
    if (sc == m_specials.end()) return "No such companion.";
    if (stay && m_scenario.essential(id)) return sc->name + " must stay with the expedition until the search is settled.";
    if (stay) {
        const MapObjectDef* here = m_map.objectAt(m_hero.pos);
        if (!here || here->type != ObjType::Town || owner(m_hero.pos) != Faction::Player)
            return "Companions can only govern a town you hold.";
        sc->stationed = m_hero.pos;
    } else {
        if (!sc->stationed) return "Already travelling with you.";
        if (m_hero.pos != *sc->stationed) return "Go to the town where they govern to recall them.";
        if (ridingCount() >= MAX_RIDING)
            return "Two companions already ride with you. Leave one here before " + sc->name + " can come.";
        sc->stationed.reset();
    }
    recomputeVisibility();
    return std::nullopt;
}

// ── Commander's paths ────────────────────────────────────────────────────────

void AdventureSession::loadHeroTree(const std::string& path) {
    m_paths.clear();
    m_wildcard = TreePath{};
    std::ifstream f(path);
    if (!f.is_open()) return;
    const auto root = nlohmann::json::parse(f, nullptr, false);
    if (root.is_discarded()) return;
    auto parsePath = [](const nlohmann::json& p) {
        TreePath out{p.value("id", ""), p.value("name", ""), p.value("text", ""), {}};
        for (const auto& n : p.value("nodes", nlohmann::json::array())) {
            TreeNode node;
            node.level = n.value("level", 0);
            node.id    = n.value("id", "");
            node.name  = n.value("name", "");
            node.text  = n.value("text", "");
            if (n.contains("effect")) {
                const auto& e = n["effect"];
                node.live = true;
                node.effect.fieldworks = e.value("fieldworks", 0);
                node.effect.tactics     = e.value("tactics", 0);
                node.effect.attack      = e.value("attack", 0);
                node.effect.defense     = e.value("defense", 0);
                node.effect.moves       = e.value("moves", 0);
                node.effect.sight       = e.value("sight", 0);
                node.effect.gold        = e.value("gold", 0);
                node.effect.growth      = e.value("growth", 0.0);
                node.effect.readiedShot = e.value("readied_shot", false);
            }
            out.nodes.push_back(node);
        }
        return out;
    };
    for (const auto& p : root.value("paths", nlohmann::json::array())) m_paths.push_back(parsePath(p));
    if (root.contains("wildcard")) m_wildcard = parsePath(root["wildcard"]);
}

std::optional<std::string> AdventureSession::choosePath(const std::string& pathId) {
    if (m_heroProgress.level < 2) return "Your path opens at level 2.";
    if (!m_path.empty())          return "Your path is already chosen.";
    for (const auto& p : m_paths) {
        if (p.id != pathId) continue;
        m_path = pathId;
        for (const auto& n : p.nodes)                 // every skill up to the current level
            if (n.level <= m_heroProgress.level) m_learned.push_back(n.id);
        return std::nullopt;
    }
    return "No such path.";
}

std::optional<std::string> AdventureSession::buyFieldwork(const std::string& kind) {
    if (won() || lost() || m_scenario.blocksAdventure()) return "Finish the current story first.";
    if (kind != "barricade" && kind != "stakes") return "Unknown fieldwork.";
    if (fieldworkCapacity() == 0 || (kind == "stakes" && fieldworkCapacity() < 2))
        return "Learn the required Siegemaster skill first.";
    int owned = 0;
    for (const auto& [id, count] : m_fieldworkStock) owned += count;
    if (owned >= fieldworkCapacity()) return "Your fieldwork wagons are full.";
    ResourcePool cost;
    cost[Resource::Gold] = kind == "barricade" ? 250 : 150;
    cost[Resource::Wood] = kind == "barricade" ? 3 : 2;
    auto& treasury = m_turns.playerFaction().treasury;
    if (!treasury.canAfford(cost)) return "Not enough gold or wood.";
    treasury -= cost;
    ++m_fieldworkStock[kind];
    return std::nullopt;
}

bool AdventureSession::learned(const std::string& nodeId) const {
    return std::find(m_learned.begin(), m_learned.end(), nodeId) != m_learned.end();
}

AdventureSession::HeroEffects AdventureSession::heroEffects() const {
    HeroEffects total;
    for (const auto& p : m_paths)
        for (const auto& n : p.nodes) {
            if (!n.live || !learned(n.id)) continue;
            total.fieldworks += n.effect.fieldworks;
            total.tactics += n.effect.tactics;
            total.attack  += n.effect.attack;
            total.defense += n.effect.defense;
            total.moves   += n.effect.moves;
            total.sight   += n.effect.sight;
            total.gold    += n.effect.gold;
            total.growth  += n.effect.growth;
            total.readiedShot = total.readiedShot || n.effect.readiedShot;
        }
    return total;
}

std::vector<AdventureSession::LevelUp> AdventureSession::drainLevelUps() {
    std::vector<LevelUp> out;
    out.swap(m_levelUps);
    return out;
}
