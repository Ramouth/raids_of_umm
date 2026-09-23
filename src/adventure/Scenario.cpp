#include "Scenario.h"
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
    if (event == "see" || event == "visit" || event == "capture" || event == "encounter_won")
        return detail.value("name", "") == when.value("name", "");
    return true;  // "start"
}

void Scenario::fire(AdventureSession& s, const std::string& event, const Json& detail) {
    if (!m_loaded) return;
    for (const auto& trigger : m_triggers) {
        std::string id = trigger.value("id", "");
        if (m_fired.count(id) || !matches(trigger.value("when", Json::object()), event, detail)) continue;
        m_fired.insert(id);
        run(s, trigger.value("do", Json::array()));
    }
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
        if (a.contains("offer"))
            for (const auto& def : m_offerDefs)
                if (def.id == a["offer"] && !offer(def.id)) m_offers.push_back(def);
    }
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
