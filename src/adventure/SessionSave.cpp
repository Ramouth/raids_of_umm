// SessionSave.cpp — save/load for AdventureSession.
//
// A save names the files the session started from (map, data, encounters,
// triggers, seed); loading replays start() and then applies what changed
// during play. Saves are only taken between actions, never mid-battle.
#include "AdventureSession.h"

using Json = Scenario::Json;

namespace {

Json cellJson(const HexCoord& c) { return Json::array({c.q, c.r}); }
HexCoord cellFrom(const Json& j) { return {j.at(0).get<int>(), j.at(1).get<int>()}; }

Json poolJson(const ResourcePool& p) { return Json(std::vector<int>(p.amounts.begin(), p.amounts.end())); }
ResourcePool poolFrom(const Json& j) {
    ResourcePool p;
    for (int i = 0; i < RESOURCE_COUNT && i < (int)j.size(); ++i) p.amounts[i] = j.at(i).get<int>();
    return p;
}

Json stacksJson(const std::vector<AdventureSession::Stack>& stacks) {
    Json out = Json::array();
    for (const auto& s : stacks) out.push_back({{"id", s.id}, {"count", s.count}});
    return out;
}
std::vector<AdventureSession::Stack> stacksFrom(const Json& j) {
    std::vector<AdventureSession::Stack> out;
    for (const auto& s : j) out.push_back({s.value("id", ""), s.value("count", 0)});
    return out;
}

} // namespace

Json AdventureSession::saveState() const {
    if (!m_startArgs) return nullptr;
    Json control = Json::array();
    for (const auto& [c, ctrl] : m_control) control.push_back({c.q, c.r, ctrl.ownerFaction});
    Json towns = Json::array();
    for (const auto& [c, t] : m_towns) {
        Json pool = Json::object();
        for (const auto& [id, n] : t.recruitPool) pool[id] = n;
        towns.push_back({{"cell", cellJson(c)}, {"pool", pool}});
    }
    Json explored = Json::array();
    for (const auto& c : m_explored) explored.push_back(cellJson(c));
    Json encounters = Json::array();
    for (const auto& [c, e] : m_encounters) encounters.push_back(cellJson(c));
    Json finds = Json::array();
    for (const auto& [c, f] : m_mineFinds) finds.push_back({c.q, c.r, static_cast<int>(f)});
    Json ruled = Json::array();
    for (const auto& c : m_ruledOut) ruled.push_back(cellJson(c));
    Json garrisons = Json::array();
    for (const auto& [c, g] : m_garrisons) if (!g.empty()) garrisons.push_back({{"cell", cellJson(c)}, {"army", stacksJson(g)}});
    Json specials = Json::array();
    for (const auto& sc : m_specials)
        specials.push_back({{"id", sc.id}, {"level", sc.level}, {"xp", sc.xp}, {"unpaid", sc.unpaidDays},
                            {"stationed", sc.stationed ? cellJson(*sc.stationed) : Json(nullptr)}});
    Json rivals = Json::array();
    for (const auto& r : m_rivals)
        rivals.push_back({{"id", r.id}, {"name", r.name}, {"pos", cellJson(r.pos)}, {"home", cellJson(r.home)},
                          {"army", stacksJson(r.army)}, {"start_power", r.startPower}, {"alive", r.alive}});
    Json pickups = Json::array();
    for (const auto& [c, p] : m_pickups) pickups.push_back(cellJson(c));
    Json siteWeeks = Json::array();
    for (const auto& [c, w] : m_siteWeek) siteWeeks.push_back({c.q, c.r, w});
    Json visited = Json::array();
    for (const auto& c : m_visitedOnce) visited.push_back(cellJson(c));
    return {
        {"version", 1},
        {"pickups", pickups}, {"site_weeks", siteWeeks}, {"visited_once", visited},
        {"stables_week", m_stablesWeek},
        {"start", {{"map", m_startArgs->map}, {"data", m_startArgs->data}, {"encounters", m_startArgs->encounters},
                   {"triggers", m_startArgs->triggers}, {"seed", m_startArgs->seed}}},
        {"day", day()},
        {"treasury", poolJson(m_turns.faction(Faction::Player).treasury)},
        {"rival_treasury", poolJson(m_turns.faction(Faction::AI).treasury)},
        {"hero", cellJson(m_hero.pos)}, {"moves", m_moves},
        {"army", stacksJson(army())},
        {"control", control}, {"towns", towns}, {"explored", explored},
        {"encounters", encounters}, {"finds", finds},
        {"won", m_won}, {"lost", m_lost}, {"lost_reason", m_lostReason},
        {"ruled_out", ruled}, {"items", Json(std::vector<std::string>(m_items.begin(), m_items.end()))},
        {"garrisons", garrisons}, {"specials", specials}, {"rivals", rivals},
        {"scenario", m_scenario.saveState()},
    };
}

std::optional<std::string> AdventureSession::loadState(const Json& save) {
    try {
        if (save.value("version", 0) != 1) return "Unsupported save version.";
        const Json& st = save.at("start");
        if (auto err = start(st.at("map").get<std::string>(), st.at("data").get<std::string>(),
                             st.at("encounters").get<std::string>(), st.at("seed").get<uint32_t>(),
                             st.at("triggers").get<std::string>()))
            return err;
        m_scenario.drainLines();                       // the replayed intro is not news
        m_turns.setDay(save.at("day").get<int>());
        m_turns.faction(Faction::Player).treasury = poolFrom(save.at("treasury"));
        m_turns.faction(Faction::AI).treasury     = poolFrom(save.at("rival_treasury"));
        m_hero.pos = cellFrom(save.at("hero"));
        setArmy(stacksFrom(save.at("army")));
        for (const auto& c : save.at("control")) {
            HexCoord cell{c.at(0).get<int>(), c.at(1).get<int>()};
            if (auto it = m_control.find(cell); it != m_control.end()) it->second.ownerFaction = c.at(2).get<int>();
        }
        for (auto& [c, t] : m_towns) t.recruitPool.clear();
        for (const auto& t : save.at("towns"))
            for (const auto& [id, n] : t.at("pool").items()) m_towns[cellFrom(t.at("cell"))].recruitPool[id] = n.get<int>();
        m_explored.clear();
        for (const auto& c : save.at("explored")) m_explored.insert(cellFrom(c));
        std::unordered_set<HexCoord> standing;
        for (const auto& c : save.at("encounters")) standing.insert(cellFrom(c));
        for (auto it = m_encounters.begin(); it != m_encounters.end();)
            it = standing.count(it->first) ? std::next(it) : m_encounters.erase(it);
        m_mineFinds.clear();
        for (const auto& f : save.at("finds"))
            m_mineFinds[{f.at(0).get<int>(), f.at(1).get<int>()}] = static_cast<MineFind>(f.at(2).get<int>());
        m_won = save.at("won").get<bool>();
        m_lost = save.at("lost").get<bool>();
        m_lostReason = save.at("lost_reason").get<std::string>();
        m_ruledOut.clear();
        for (const auto& c : save.at("ruled_out")) m_ruledOut.insert(cellFrom(c));
        m_items.clear();
        for (const auto& id : save.at("items")) m_items.insert(id.get<std::string>());
        m_garrisons.clear();
        for (const auto& g : save.at("garrisons")) m_garrisons[cellFrom(g.at("cell"))] = stacksFrom(g.at("army"));
        m_specials.clear();
        for (const auto& s : save.at("specials")) {
            joinSpecial(s.at("id").get<std::string>());
            Special& sc = m_specials.back();
            sc.level = s.at("level").get<int>();
            sc.xp = s.at("xp").get<int>();
            sc.unpaidDays = s.at("unpaid").get<int>();
            if (!s.at("stationed").is_null()) sc.stationed = cellFrom(s.at("stationed"));
        }
        m_rivals.clear();
        for (const auto& r : save.at("rivals"))
            m_rivals.push_back({r.at("id").get<int>(), r.at("name").get<std::string>(), cellFrom(r.at("pos")),
                                cellFrom(r.at("home")), stacksFrom(r.at("army")), r.at("start_power").get<double>(),
                                r.at("alive").get<bool>(), ""});
        if (save.contains("pickups")) {            // older saves predate sites
            std::unordered_set<HexCoord> left;
            for (const auto& c : save.at("pickups")) left.insert(cellFrom(c));
            for (auto it = m_pickups.begin(); it != m_pickups.end();)
                it = left.count(it->first) ? std::next(it) : m_pickups.erase(it);
            for (const auto& w : save.at("site_weeks")) m_siteWeek[{w.at(0).get<int>(), w.at(1).get<int>()}] = w.at(2).get<int>();
            for (const auto& c : save.at("visited_once")) m_visitedOnce.insert(cellFrom(c));
            m_stablesWeek = save.value("stables_week", 0);
        }
        m_scenario.loadState(save.at("scenario"));
        m_scenario.drainLines();
        m_movesMax = DEFAULT_MOVES + movesBonus();
        m_moves = save.at("moves").get<float>();
        m_pending.reset();
        m_pendingAmbush = false;
        recomputeVisibility();
        return std::nullopt;
    } catch (const std::exception& e) {
        return std::string("Save file is damaged: ") + e.what();
    }
}
