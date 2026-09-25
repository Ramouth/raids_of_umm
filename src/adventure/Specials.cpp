// Specials.cpp — special characters for AdventureSession.
//
// SCs travel with the hero (earning XP from victories and granting their
// unlocked abilities) or govern an owned town (+100 gold per level per day,
// no XP). Upkeep rises with level; unpaid SCs sulk, then leave.
#include "AdventureSession.h"
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

namespace {

struct SpecialDef { const char* id; const char* name; const char* title; };
constexpr SpecialDef kSpecials[] = {
    {"ushari", "Ushari", "Keeper of the old ways"},
    {"kharim", "Kharim", "Scholar of Pha'raxh"},
    {"maerwen", "Maerwen Hale", "Shield-captain of the heron"},
};

} // namespace

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

int AdventureSession::upkeepFor(int level) {
    if (level >= 8) return 600;
    if (level >= 5) return 300;
    if (level >= 3) return 200;
    return 100;
}

void AdventureSession::joinSpecial(const std::string& id) {
    for (const auto& sc : m_specials) if (sc.id == id) return;
    auto returning = std::find_if(m_departedSpecials.begin(), m_departedSpecials.end(),
                                 [&](const Special& sc) { return sc.id == id; });
    if (returning != m_departedSpecials.end()) {
        m_specials.push_back(*returning);
        m_specials.back().stationed.reset();
        m_departedSpecials.erase(returning);
        return;
    }
    for (const auto& def : kSpecials) {
        if (id != def.id) continue;
        m_specials.push_back({def.id, def.name, def.title, 1, 0, std::nullopt, 0});
        if (m_specials.size() > 1)
            report(def.name, std::string("I will ride with the Compact. My upkeep is modest, for now."));
        return;
    }
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
        if (!sc.stationed && !isWounded(sc) && sc.unpaidDays < SULK_DAYS && m_resources->unit(sc.id))
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
            report(sc->name, "I will live. Give me three days before you ask me to hold a line again.");
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
    if (!m_encounters.count(c)) return 0;
    return std::max(20, static_cast<int>(power(guardsOf(c)) / 3));
}

void AdventureSession::grantXp(int xp) {
    if (xp <= 0) return;
    m_heroProgress.xp += xp;
    while (m_heroProgress.level < MAX_HERO_LEVEL && m_heroProgress.xp >= xpForLevel(m_heroProgress.level + 1)) {
        LevelUp up{++m_heroProgress.level, "", ""};
        for (const auto& p : m_paths)
            if (p.id == m_path)
                for (const auto& n : p.nodes)
                    if (n.level == up.level) { m_learned.push_back(n.id); up.skill = n.name; up.text = n.text; }
        m_levelUps.push_back(up);
    }
    for (auto& sc : m_specials) {
        if (sc.stationed) continue;
        sc.xp += xp;
        while (sc.xp >= xpForLevel(sc.level + 1)) {
            ++sc.level;
            std::string line = sc.name + " reaches level " + std::to_string(sc.level) + ".";
            for (const auto& a : abilitiesOf(sc.id))
                if (a.level == sc.level) line += " New ability: " + a.name + " — " + a.text;
            report(sc.name, line);
        }
    }
    recomputeVisibility();
}

int AdventureSession::upkeepPerDay() const {
    int total = 0;
    for (const auto& sc : m_specials) total += upkeepFor(sc.level);
    return total;
}

void AdventureSession::payUpkeep() {
    ResourcePool& treasury = m_turns.playerFaction().treasury;
    for (auto it = m_specials.begin(); it != m_specials.end();) {
        int cost = upkeepFor(it->level);
        bool crystal = it->level >= 8 && dayOfWeek() == 7;  // legends also want a crystal a week
        bool paid = treasury[Resource::Gold] >= cost && (!crystal || treasury[Resource::Crystal] >= 1);
        if (paid) {
            treasury[Resource::Gold] -= cost;
            if (crystal) treasury[Resource::Crystal] -= 1;
            if (it->unpaidDays >= SULK_DAYS) report(it->name, "Paid at last. Very well, I am with you again.");
            it->unpaidDays = 0;
        } else {
            ++it->unpaidDays;
            if (it->unpaidDays == SULK_DAYS)
                report(it->name, "Three days without pay. Do not expect my best until the treasury remembers me.");
            if (it->unpaidDays >= LEAVE_DAYS && !m_scenario.essential(it->id)) {
                report(it->name, "A week unpaid. I have served better paymasters. Farewell.");
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
    if (won() || lost() || m_scenario.awaitingChoice()) return "Finish the current story first.";
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
