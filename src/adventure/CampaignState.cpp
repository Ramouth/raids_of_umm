// CampaignState.cpp — the carried state and the chapter/map sequence.
#include "CampaignState.h"
#include <fstream>

using Json = nlohmann::json;

namespace {

Json companionsJson(const std::vector<CampaignState::Companion>& list) {
    Json out = Json::array();
    for (const auto& c : list) {
        out.push_back({{"id", c.id}, {"name", c.name}, {"title", c.title},
                       {"level", c.level}, {"xp", c.xp}, {"morale", c.morale}});
        if (c.waiting) out.back()["waiting"] = true;   // omitted when false
    }
    return out;
}

std::vector<CampaignState::Companion> companionsFrom(const Json& j) {
    std::vector<CampaignState::Companion> out;
    for (const auto& c : j)
        out.push_back({c.value("id", ""), c.value("name", ""), c.value("title", ""),
                       c.value("level", 1), c.value("xp", 0), c.value("morale", 0), c.value("waiting", false)});
    return out;
}

} // namespace

Json CampaignState::toJson() const {
    Json stacks = Json::array();
    for (const auto& s : army) {
        stacks.push_back({{"id", s.id}, {"count", s.count}});
        if (s.bitten) stacks.back()["bitten"] = true;
    }
    return {{"chapter", chapter}, {"map", map}, {"carried", carried},
            {"level", level}, {"xp", xp}, {"path", path}, {"learned", learned},
            {"army", stacks}, {"items", items}, {"equipped", equipped},
            {"companions", companionsJson(companions)}, {"departed", companionsJson(departed)},
            {"treasury", std::vector<int>(treasury.amounts.begin(), treasury.amounts.end())},
            {"flags", flags}, {"shown_tips", shownTips}, {"level_cap", levelCap}, {"last_map", lastMap}, {"history", history}};
}

CampaignState CampaignState::fromJson(const Json& j) {
    CampaignState s;
    s.chapter  = j.value("chapter", "");
    s.map      = j.value("map", "");
    s.carried  = j.value("carried", false);
    s.level    = j.value("level", 1);
    s.xp       = j.value("xp", 0);
    s.path     = j.value("path", "");
    s.learned  = j.value("learned", std::vector<std::string>{});
    for (const auto& st : j.value("army", Json::array()))
        s.army.push_back({st.value("id", ""), st.value("count", 0), st.value("bitten", false)});
    s.items    = j.value("items", std::vector<std::string>{});
    s.equipped = j.value("equipped", std::map<std::string, std::string>{});
    s.companions = companionsFrom(j.value("companions", Json::array()));
    s.departed   = companionsFrom(j.value("departed", Json::array()));
    const Json t = j.value("treasury", Json::array());
    for (int i = 0; i < RESOURCE_COUNT && i < (int)t.size(); ++i) s.treasury.amounts[i] = t.at(i).get<int>();
    s.flags    = j.value("flags", Json::object());
    s.shownTips = j.value("shown_tips", std::vector<std::string>{});
    s.levelCap = j.value("level_cap", 10);
    s.lastMap  = j.value("last_map", Json::object());
    s.history  = j.value("history", Json::array());
    return s;
}

// ── Campaign ──────────────────────────────────────────────────────────────────

std::optional<std::string> Campaign::load(const std::string& path, const std::string& dataDir) {
    try {
        std::ifstream f(path);
        if (!f.is_open()) return "Cannot open campaign: " + path;
        Json root = Json::parse(f);
        std::vector<Chapter> chapters;
        for (const auto& c : root.value("chapters", Json::array())) {
            Chapter chapter{c.value("id", ""), c.value("title", ""), {}};
            for (const auto& m : c.value("maps", Json::array())) {
                MapEntry e{m.value("id", ""), m.value("title", ""), m.value("map", ""),
                           m.value("encounters", ""), m.value("triggers", ""), m.value("max_level", 10)};
                if (e.id.empty() || e.map.empty()) return "Campaign map without an id or a map file in " + chapter.id;
                chapter.maps.push_back(std::move(e));
            }
            if (!chapter.maps.empty()) chapters.push_back(std::move(chapter));
        }
        if (chapters.empty()) return "The campaign has no maps: " + path;
        m_chapters = std::move(chapters);
        m_path = path;
        m_dataDir = dataDir;
        if (m_dataDir.empty()) {
            auto slash = path.find_last_of('/');
            m_dataDir = slash == std::string::npos ? "." : path.substr(0, slash);
        }
        m_state = CampaignState{};
        m_state.chapter = m_chapters.front().id;
        m_state.map = m_chapters.front().maps.front().id;
        m_finished = false;
        return std::nullopt;
    } catch (const std::exception& e) {
        return std::string("Campaign parse error: ") + e.what();
    }
}

const Campaign::MapEntry* Campaign::find(const std::string& mapId) const {
    for (const auto& c : m_chapters)
        for (const auto& m : c.maps)
            if (m.id == mapId) return &m;
    return nullptr;
}

const Campaign::MapEntry* Campaign::current() const {
    return m_finished ? nullptr : find(m_state.map);
}

const Campaign::Chapter* Campaign::currentChapter() const {
    for (const auto& c : m_chapters)
        if (c.id == m_state.chapter) return &c;
    return nullptr;
}

std::string Campaign::resolve(const std::string& relative) const {
    if (relative.empty() || relative.front() == '/') return relative;   // absolute paths stand
    return m_dataDir + "/" + relative;
}

CampaignState Campaign::startState() const {
    CampaignState s = m_state;
    if (const MapEntry* e = current()) s.levelCap = e->maxLevel;
    return s;
}

void Campaign::finishMap(const CampaignState& out) {
    CampaignState next = out;
    next.chapter = m_state.chapter;
    next.map     = m_state.map;
    next.carried = true;
    next.history = m_state.history;
    Json stats = out.lastMap;
    stats["map"] = m_state.map;
    next.history.push_back(stats);
    m_state = std::move(next);
}

Campaign::Advance Campaign::advance(const std::string& nextMapId) {
    Advance out;
    if (!loaded() || m_finished) { out.error = "There is no campaign map to leave."; return out; }
    if (!nextMapId.empty()) {
        for (const auto& c : m_chapters)
            for (const auto& m : c.maps)
                if (m.id == nextMapId) {
                    out.newChapter = c.id != m_state.chapter;
                    m_state.chapter = c.id;
                    m_state.map = m.id;
                    return out;
                }
        out.error = "The campaign has no map " + nextMapId + ".";
        return out;
    }
    for (size_t ci = 0; ci < m_chapters.size(); ++ci)
        for (size_t mi = 0; mi < m_chapters[ci].maps.size(); ++mi) {
            if (m_chapters[ci].maps[mi].id != m_state.map) continue;
            if (mi + 1 < m_chapters[ci].maps.size()) {
                m_state.map = m_chapters[ci].maps[mi + 1].id;
            } else if (ci + 1 < m_chapters.size()) {
                out.newChapter = true;
                m_state.chapter = m_chapters[ci + 1].id;
                m_state.map = m_chapters[ci + 1].maps.front().id;
            } else {
                out.finished = m_finished = true;
            }
            return out;
        }
    out.error = "The campaign's current map is not in the campaign.";
    return out;
}

Json Campaign::describe(const MapEntry& e) const {
    return {{"id", e.id}, {"title", e.title}, {"max_level", e.maxLevel},
            {"map", resolve(e.map)}, {"encounters", resolve(e.encounters)}, {"triggers", resolve(e.triggers)}};
}

Json Campaign::saveJson() const {
    return {{"version", 1}, {"campaign", m_path}, {"data", m_dataDir},
            {"finished", m_finished}, {"state", m_state.toJson()}};
}

std::optional<std::string> Campaign::saveFile(const std::string& path) const {
    if (!loaded()) return "No campaign is loaded.";
    std::ofstream f(path);
    if (!f.is_open()) return "Cannot write " + path;
    f << saveJson().dump(2);
    return std::nullopt;
}

std::optional<std::string> Campaign::loadSave(const Json& save) {
    try {
        if (save.value("version", 0) != 1) return "Unsupported campaign save version.";
        if (auto err = load(save.at("campaign").get<std::string>(), save.value("data", ""))) return err;
        m_state = CampaignState::fromJson(save.at("state"));
        m_finished = save.value("finished", false);
        if (!m_finished && !find(m_state.map)) return "The saved map " + m_state.map + " is not in the campaign.";
        return std::nullopt;
    } catch (const std::exception& e) {
        return std::string("Campaign save is damaged: ") + e.what();
    }
}

std::optional<std::string> Campaign::loadFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "No campaign save: " + path;
    Json save = Json::parse(f, nullptr, false);
    if (save.is_discarded()) return "Campaign save is damaged.";
    return loadSave(save);
}
