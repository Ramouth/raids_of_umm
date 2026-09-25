#include "Scenario.h"
#include <algorithm>
#include "AdventureSession.h"
#include <fstream>

namespace {

ResourcePool poolFrom(const nlohmann::json& j) {
    ResourcePool out;
    if (!j.is_object()) return out;
    for (const auto& [key, amount] : j.items())
        for (int i = 0; i < RESOURCE_COUNT; ++i)
            if (key == resourceName(static_cast<Resource>(i)))
                out.amounts[i] = amount.get<int>();
    return out;
}

} // namespace

std::optional<std::string> Scenario::load(const std::string& path) {
    try {
        std::ifstream f(path);
        if (!f.is_open()) return "Cannot open triggers: " + path;
        Json root = Json::parse(f);
        m_triggers = root.value("triggers", Json::array());
        m_startCompanions.reset();
        if (root.contains("start_companions"))
            m_startCompanions = root["start_companions"].get<std::vector<std::string>>();
        m_questDefs.clear();
        for (const auto& q : root.value("quests", Json::array()))
            m_questDefs.push_back({q.value("id", ""), q.value("title", ""), q.value("text", ""),
                                   q.value("main", false), false});
        m_offerDefs.clear();
        for (const auto& o : root.value("offers", Json::array())) {
            Offer offer;
            offer.id    = o.value("id", "");
            offer.label = o.value("label", "");
            offer.at    = o.value("at", "");
            offer.cost  = poolFrom(o.value("cost", Json::object()));
            offer.item  = o.value("item", "");
            offer.quest = o.value("quest", "");
            offer.then  = o.value("then", Json::array());
            m_offerDefs.push_back(std::move(offer));
        }
        m_fired.clear();
        m_quests.clear();
        m_offers.clear();
        m_lines.clear();
        m_won.clear();
        m_vanished.clear();
        m_waiting.clear();
        m_firedOn.clear();
        m_loaded = true;
        return std::nullopt;
    } catch (const std::exception& e) {
        return std::string("Triggers parse error: ") + e.what();
    }
}

bool Scenario::matches(const Json& when, const std::string& event, const Json& detail) const {
    if (when.value("event", "") != event) return false;
    if (event == "day")         return detail.value("day", 0) >= when.value("day", 0);
    if (event == "enter_q")     return detail.value("q", -999) >= when.value("q_min", 999);
    if (event == "see_type")    return detail.value("type", "") == when.value("type", "");
    if (event == "mines_held" || event == "quests_done")
        return detail.value("count", 0) >= when.value("count", 99);
    if (event == "item")        return detail.value("item", "") == when.value("item", "");
    if (event == "see" || event == "visit" || event == "capture" || event == "encounter_won"
        || event == "rival_beaten" || event == "engage")
        return detail.value("name", "") == when.value("name", "");
    if (event == "cleared") {
        for (const auto& name : when.value("names", Json::array()))
            if (!m_won.count(name.get<std::string>())) return false;
        return true;
    }
    return true;  // "start"
}

void Scenario::fire(AdventureSession& s, const std::string& event, const Json& detail) {
    if (!m_loaded) return;
    if (event == "encounter_won") m_won.insert(detail.value("name", ""));
    for (const auto& trigger : m_triggers) {
        std::string id = trigger.value("id", "");
        if (m_fired.count(id) || !matches(trigger.value("when", Json::object()), event, detail)) continue;
        std::string after = trigger.value("when", Json::object()).value("after", "");
        if (!after.empty() && !m_fired.count(after)) continue;   // story order
        int delay = trigger.value("when", Json::object()).value("delay", 0);
        if (delay > 0 && (after.empty() || s.day() < m_firedOn[after] + delay)) continue;   // days after it
        std::string unless = trigger.value("when", Json::object()).value("unless", "");
        if (!unless.empty() && m_fired.count(unless)) { m_fired.insert(id); continue; }  // overtaken by events
        std::string wait = trigger.value("when", Json::object()).value("wait", "");
        if (!wait.empty() && !m_fired.count(wait)) {                // held until that beat
            m_fired.insert(id);
            m_waiting.push_back(id);
            continue;
        }
        fireTrigger(s, trigger);
    }
}

void Scenario::fireTrigger(AdventureSession& s, const Json& trigger) {
    std::string id = trigger.value("id", "");
    m_fired.insert(id);
    m_firedOn[id] = s.day();
    run(s, trigger.value("do", Json::array()));
    // Beats that were waiting for this one play now, in the order they came up.
    std::vector<std::string> ready;
    for (auto it = m_waiting.begin(); it != m_waiting.end();) {
        std::string waitFor = "";
        for (const auto& t : m_triggers)
            if (t.value("id", "") == *it) waitFor = t.value("when", Json::object()).value("wait", "");
        if (waitFor == id) { ready.push_back(*it); it = m_waiting.erase(it); }
        else ++it;
    }
    for (const auto& r : ready)
        for (const auto& t : m_triggers)
            if (t.value("id", "") == r) fireTrigger(s, t);
}

void Scenario::run(AdventureSession& s, const Json& actions) {
    for (const auto& a : actions) {
        if (a.contains("say"))
            for (const auto& line : a["say"])
                m_lines.push_back({line.at(0).get<std::string>(), line.at(1).get<std::string>()});
        if (a.contains("quest"))
            for (const auto& def : m_questDefs)
                if (def.id == a["quest"] && !quest(def.id)) m_quests.push_back(def);
        if (a.contains("quest_done"))
            if (Quest* q = quest(a["quest_done"]); q && !q->done) {
                q->done = true;
                int optional = 0;
                for (const auto& other : m_quests) optional += (!other.main && other.done) ? 1 : 0;
                fire(s, "quests_done", {{"count", optional}});
            }
        if (a.contains("join")) s.joinSpecial(a["join"].get<std::string>());
        if (a.contains("quest_text"))
            if (Quest* q = quest(a["quest_text"].at(0))) q->text = a["quest_text"].at(1).get<std::string>();
        if (a.contains("reveal")) {
            const auto& r = a["reveal"];
            s.revealArea({r.at(0).get<int>(), r.at(1).get<int>()}, r.at(2).get<int>());
        }
        if (a.contains("give")) s.give(poolFrom(a["give"]));
        if (a.contains("clue")) {
            if (auto name = s.giveClue())
                m_lines.push_back({"Kharim", "The " + *name + " is a dead end. I would stake my maps on it."});
            else
                m_lines.push_back({"Kharim", "You have already ruled out every false mine. Trust the one that remains."});
        }
        if (a.contains("item")) s.addItem(a["item"].get<std::string>());
        if (a.contains("lore")) {
            Lore entry{a["lore"].at(0).get<std::string>(), a["lore"].at(1).get<std::string>()};
            if (std::none_of(m_lore.begin(), m_lore.end(), [&](const Lore& l) { return l.title == entry.title; }))
                m_lore.push_back(entry);
        }
        if (a.contains("troops")) {                  // allies send soldiers to the hero
            auto army = s.army();
            for (const auto& st : a["troops"]) {
                std::string id = st.value("id", "");
                int n = st.value("count", 0);
                auto it = std::find_if(army.begin(), army.end(), [&](const auto& x) { return x.id == id; });
                if (it != army.end()) it->count += n;
                else army.push_back({id, n});
            }
            s.setArmy(army);
        }
        if (a.contains("betray")) {
            const auto& b = a["betray"];
            std::vector<AdventureSession::Stack> army;
            for (const auto& st : b.value("army", Json::array()))
                army.push_back({st.value("id", ""), st.value("count", 0)});
            s.betray(b.value("at", ""), b.value("band", "war-band"), army);
        }
        if (a.contains("turncoats")) {                // hired men turn; their lines only if any did
            const auto& t = a["turncoats"];
            if (s.turncoats(t.value("unit", ""), t.value("band", "turncoats")) > 0)
                for (const auto& line : t.value("say", Json::array()))
                    m_lines.push_back({line.at(0).get<std::string>(), line.at(1).get<std::string>()});
        }
        if (a.contains("vanish")) m_vanished.insert(a["vanish"].get<std::string>());
        if (a.contains("offer"))
            for (const auto& def : m_offerDefs)
                if (def.id == a["offer"] && !offer(def.id)) m_offers.push_back(def);
    }
}

Scenario::Json Scenario::saveState() const {
    Json quests = Json::array();
    for (const auto& q : m_quests) quests.push_back({{"id", q.id}, {"text", q.text}, {"done", q.done}});
    Json offers = Json::array();
    for (const auto& o : m_offers) offers.push_back({{"id", o.id}, {"taken", o.taken}});
    Json lore = Json::array();
    for (const auto& l : m_lore) lore.push_back({l.title, l.text});
    return {{"fired", Json(std::vector<std::string>(m_fired.begin(), m_fired.end()))},
            {"quests", quests}, {"offers", offers}, {"lore", lore},
            {"won", Json(std::vector<std::string>(m_won.begin(), m_won.end()))},
            {"vanished", Json(std::vector<std::string>(m_vanished.begin(), m_vanished.end()))},
            {"waiting", Json(m_waiting)}, {"fired_on", Json(m_firedOn)}};
}

void Scenario::loadState(const Json& state) {
    m_fired.clear();
    for (const auto& id : state.value("fired", Json::array())) m_fired.insert(id.get<std::string>());
    m_quests.clear();
    for (const auto& q : state.value("quests", Json::array()))
        for (const auto& def : m_questDefs)
            if (def.id == q.value("id", "")) {
                Quest copy = def;
                copy.text = q.value("text", def.text);
                copy.done = q.value("done", false);
                m_quests.push_back(copy);
            }
    m_offers.clear();
    for (const auto& o : state.value("offers", Json::array()))
        for (const auto& def : m_offerDefs)
            if (def.id == o.value("id", "")) {
                Offer copy = def;
                copy.taken = o.value("taken", false);
                m_offers.push_back(copy);
            }
    m_won.clear();
    for (const auto& n : state.value("won", Json::array())) m_won.insert(n.get<std::string>());
    m_vanished.clear();
    for (const auto& n : state.value("vanished", Json::array())) m_vanished.insert(n.get<std::string>());
    m_waiting = state.value("waiting", std::vector<std::string>{});
    m_firedOn = state.value("fired_on", std::unordered_map<std::string, int>{});
    m_lore.clear();
    for (const auto& l : state.value("lore", Json::array()))
        m_lore.push_back({l.at(0).get<std::string>(), l.at(1).get<std::string>()});
    m_lines.clear();
}

std::vector<Scenario::Line> Scenario::drainLines() {
    std::vector<Line> out;
    out.swap(m_lines);
    return out;
}

std::vector<const Scenario::Offer*> Scenario::offersAt(const std::string& objectName) const {
    std::vector<const Offer*> out;
    for (const auto& o : m_offers)
        if (!o.taken && o.at == objectName) out.push_back(&o);
    return out;
}

Scenario::Offer* Scenario::offer(const std::string& id) {
    for (auto& o : m_offers)
        if (o.id == id) return &o;
    return nullptr;
}

Scenario::Quest* Scenario::quest(const std::string& id) {
    for (auto& q : m_quests)
        if (q.id == id) return &q;
    return nullptr;
}
