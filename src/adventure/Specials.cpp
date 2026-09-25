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
    {"ushari", "Ushari", "Veteran commander"},
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
    static const int table[] = {0, 0, 100, 250, 450, 700, 1000, 1400, 1900};
    return level <= 8 ? table[std::max(level, 1)] : 1900 + (level - 8) * 600;
}

int AdventureSession::upkeepFor(int level) {
    if (level >= 8) return 600;
    if (level >= 5) return 300;
    if (level >= 3) return 200;
    return 100;
}

void AdventureSession::joinSpecial(const std::string& id) {
    for (const auto& sc : m_specials) if (sc.id == id) return;
    for (const auto& def : kSpecials) {
        if (id != def.id) continue;
        m_specials.push_back({def.id, def.name, def.title, 1, 0, std::nullopt, 0});
        if (m_specials.size() > 1)
            report(def.name, std::string("I will ride with the Compact. My upkeep is modest, for now."));
        return;
    }
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
        if (battleLost) {
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
    if (m_stablesWeek != 0 && m_stablesWeek == week()) bonus += STABLES_BONUS;
    return bonus;
}

int AdventureSession::sightBonus() const {
    int bonus = 0;
    if (hasAbility("Eagle Eye")) bonus += 2;
    if (hasAbility("Old Maps")) bonus += 1;
    if (hasAbility("Deep Reading")) bonus += 2;
    return bonus;
}

int AdventureSession::encounterXp(const HexCoord& c) const {
    if (const Rival* r = rivalAt(c)) return std::max(40, static_cast<int>(power(r->army) / 3));
    if (!m_encounters.count(c)) return 0;
    return std::max(20, static_cast<int>(power(guardsOf(c)) / 3));
}

void AdventureSession::grantXp(int xp) {
    if (xp <= 0) return;
    m_heroProgress.xp += xp;
    while (m_heroProgress.xp >= xpForLevel(m_heroProgress.level + 1)) {
        ++m_heroProgress.level;
        ++m_heroProgress.points;
        report("Commander", "You reach level " + std::to_string(m_heroProgress.level)
               + ". One point to spend in the commander's tree (K).");
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
            if (it->unpaidDays >= LEAVE_DAYS) {
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

// ── Commander tree ───────────────────────────────────────────────────────────

std::vector<AdventureSession::TreeBranch> AdventureSession::loadHeroTree(const std::string& path) {
    std::vector<TreeBranch> tree;
    std::ifstream f(path);
    if (!f.is_open()) return tree;
    const auto root = nlohmann::json::parse(f, nullptr, false);
    if (root.is_discarded()) return tree;
    for (const auto& b : root.value("branches", nlohmann::json::array())) {
        TreeBranch branch{b.value("id", ""), b.value("name", ""), b.value("text", ""), b.value("live", false), {}};
        for (const auto& n : b.value("nodes", nlohmann::json::array()))
            branch.nodes.push_back({n.value("id", ""), n.value("name", ""), n.value("text", "")});
        tree.push_back(std::move(branch));
    }
    return tree;
}

bool AdventureSession::learned(const std::string& nodeId) const {
    return std::find(m_learned.begin(), m_learned.end(), nodeId) != m_learned.end();
}

std::string AdventureSession::learnBlocker(const std::string& nodeId) const {
    for (const auto& branch : m_tree)
        for (size_t i = 0; i < branch.nodes.size(); ++i) {
            if (branch.nodes[i].id != nodeId) continue;
            if (learned(nodeId))                            return "Already learned.";
            if (!branch.live)                               return "Not in the demo yet.";
            if (i > 0 && !learned(branch.nodes[i - 1].id))  return "Learn " + branch.nodes[i - 1].name + " first.";
            if (m_heroProgress.points <= 0)                 return "No points to spend: gain a level.";
            return "";
        }
    return "No such skill.";
}

std::optional<std::string> AdventureSession::learn(const std::string& nodeId) {
    if (auto why = learnBlocker(nodeId); !why.empty()) return why;
    m_learned.push_back(nodeId);
    --m_heroProgress.points;
    return std::nullopt;
}

int AdventureSession::tacticsRank() const {
    int rank = 0;
    for (const char* id : {"first_orders", "vanguard", "battle_plan"}) rank += learned(id) ? 1 : 0;
    return rank;
}
