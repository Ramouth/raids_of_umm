#include "Scenario.h"
#include <algorithm>
#include "AdventureSession.h"
#include "Condition.h"
#include <fstream>
#include <stdexcept>

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
        m_loaded = false;
        m_rivalLord = root.value("rival_lord", Json::object());
        m_surveySite = root.value("survey_site", "");
        m_passageEndsScenario = root.value("passage_ends_scenario", true);
        m_choice = Json::object();
        m_choiceTrigger.clear();
        m_firing.clear();
        m_outcome = Json::object();
        m_lore.clear();
        m_morale.clear();
        m_essential.clear();
        for (const auto& id : root.value("essential_companions", Json::array()))
            m_essential.insert(id.get<std::string>());
        m_startCompanions.reset();
        if (root.contains("start_companions"))
            m_startCompanions = root["start_companions"].get<std::vector<std::string>>();
        m_startArmy = root.value("start_army", Json::array());
        m_startResources.reset();
        if (root.contains("start_resources")) m_startResources = poolFrom(root["start_resources"]);
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
        m_story.clear();
        m_queuedChoices.clear();
        m_won.clear();
        m_vanished.clear();
        m_waiting.clear();
        m_firedOn.clear();
        if (auto err = compile()) return "Triggers " + path + ": " + *err;
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
        for (const auto& name : when.value("names", Json::array()))   // beaten, or gone from the map
            if (!m_won.count(name.get<std::string>()) && !m_vanished.count(name.get<std::string>())) return false;
        return true;
    }
    return true;  // "start"
}

void Scenario::fire(AdventureSession& s, const std::string& event, const Json& detail) {
    if (!m_loaded) return;
    if (event == "encounter_won") m_won.insert(detail.value("name", ""));
    for (size_t i = 0; i < m_triggers.size(); ++i) {
        const Json& trigger = m_triggers[i];
        std::string id = trigger.value("id", "");
        if (m_fired.count(id) || !matches(trigger.value("when", Json::object()), event, detail)) continue;
        const Gate& gate = m_gates[i];
        if (!Condition::eval(gate.order, s)) continue;                                     // story order
        if (Condition::eval(gate.overtaken, s)) { m_fired.insert(id); continue; }         // overtaken by events
        if (!Condition::eval(gate.when, s)) continue;                                      // not yet / not now
        std::string wait = trigger.value("when", Json::object()).value("wait", "");
        if (!wait.empty() && !m_fired.count(wait)) {                // held until that beat
            m_fired.insert(id);
            m_waiting.push_back(id);
            continue;
        }
        fireTrigger(s, trigger);
    }
}

bool Scenario::armed(const AdventureSession& s, size_t trigger) const {
    return trigger < m_gates.size() && Condition::eval(m_gates[trigger].order, s) && Condition::eval(m_gates[trigger].when, s);
}

bool Scenario::overtaken(const AdventureSession& s, size_t trigger) const {
    return trigger < m_gates.size() && Condition::eval(m_gates[trigger].overtaken, s);
}

namespace {

// Walks an action list, checking every condition in it (if-actions, choice options).
std::optional<std::string> checkActions(const Scenario::Json& actions, const std::string& where) {
    using Json = Scenario::Json;
    if (!actions.is_array()) return std::nullopt;
    for (size_t k = 0; k < actions.size(); ++k) {
        const Json& a = actions[k];
        const std::string at = where + "[" + std::to_string(k) + "]";
        if (!a.is_object()) continue;
        if (a.contains("if")) {
            for (const auto& [key, value] : a.items())
                if (key != "if" && key != "then" && key != "else")
                    return at + ": an if-action holds only if / then / else, not '" + key + "'";
            if (auto err = Condition::validate(a["if"], at + ".if")) return err;
            for (const char* branch : {"then", "else"})
                if (a.contains(branch)) {
                    if (!a[branch].is_array()) return at + "." + branch + ": wants a list of actions";
                    if (auto err = checkActions(a[branch], at + "." + branch)) return err;
                }
        }
        if (a.contains("choice")) {
            const Json options = a["choice"].value("options", Json::array());
            // A choice that holds the map still must always offer something: if every
            // option can be hidden ("if" / requires_item), the player could face none.
            if (!a["choice"].value("allow_exploration", false) && !options.empty()
                && std::all_of(options.begin(), options.end(), [](const Json& o) {
                       return o.is_object() && (o.contains("if") || o.contains("requires_item"));
                   }))
                return at + ".choice '" + a["choice"].value("id", "") + "': every option is conditional "
                       "(if / requires_item); a blocking choice needs at least one option without a condition";
            for (size_t i = 0; i < options.size(); ++i) {
                const std::string o = at + ".choice.options[" + std::to_string(i) + "]";
                if (auto err = Condition::validate(Scenario::shownWhen(options[i]), o + ".if")) return err;
                if (auto err = Condition::validate(Scenario::lockedUnless(options[i]), o + ".requires")) return err;
                if (auto err = checkActions(options[i].value("then", Json::array()), o + ".then")) return err;
            }
        }
    }
    return std::nullopt;
}

} // namespace

// Every condition in the file becomes a tree, checked here: legacy fields
// (when.after / delay / unless / flag / unless_flag, requires_*) are translated,
// so play has one code path (Condition::eval).
std::optional<std::string> Scenario::compile() {
    m_gates.clear();
    try {
        for (size_t i = 0; i < m_triggers.size(); ++i) {
            const Json& t = m_triggers[i];
            const Json when = t.value("when", Json::object());
            const std::string where = "triggers[" + std::to_string(i) + "] '" + t.value("id", "") + "'";
            Gate gate;
            const std::string after = when.value("after", "");
            const int delay = when.value("delay", 0);
            Json order = Json::array();
            if (!after.empty()) {
                Json fired = {{"trigger_fired", after}};
                if (delay > 0) fired["days"] = {{">=", delay}};
                order.push_back(fired);
            } else if (delay > 0) {
                order.push_back(false);                    // a delay counts from "after"; without one it never comes
            }
            gate.order = Condition::allOf(order);
            const std::string unless = when.value("unless", "");
            gate.overtaken = unless.empty() ? Json(false) : Json{{"trigger_fired", unless}};
            Json parts = Json::array();
            if (when.contains("flag")) parts.push_back(Condition::fromFlags(when["flag"]));
            if (when.contains("unless_flag")) parts.push_back({{"not", Condition::fromFlags(when["unless_flag"])}});
            if (t.contains("if")) {
                if (auto err = Condition::validate(t["if"], where + ".if")) return err;
                parts.push_back(t["if"]);
            }
            gate.when = Condition::allOf(parts);
            for (const Json* tree : {&gate.order, &gate.overtaken, &gate.when})
                if (auto err = Condition::validate(*tree, where + ".when")) return err;
            if (auto err = checkActions(t.value("do", Json::array()), where + ".do")) return err;
            m_gates.push_back(std::move(gate));
        }
        for (size_t i = 0; i < m_offerDefs.size(); ++i)
            if (auto err = checkActions(m_offerDefs[i].then, "offers[" + std::to_string(i) + "].then")) return err;
    } catch (const std::invalid_argument& e) {
        return std::string(e.what());
    }
    return std::nullopt;
}

void Scenario::fireTrigger(AdventureSession& s, const Json& trigger) {
    std::string id = trigger.value("id", "");
    m_fired.insert(id);
    m_firedOn[id] = s.day();
    const std::string outer = m_firing;
    m_firing = id;                              // a choice raised here remembers its trigger
    run(s, trigger.value("do", Json::array()));
    m_firing = outer;
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
        if (a.contains("if")) {                      // a branch: one of its lists runs, nothing else here
            run(s, Condition::eval(a["if"], s) ? a.value("then", Json::array()) : a.value("else", Json::array()));
            continue;
        }
        if (a.contains("say"))
            for (const auto& line : a["say"])
                say(line.at(0).get<std::string>(), line.at(1).get<std::string>());
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
        if (a.contains("flag"))
            for (const auto& [name, value] : a["flag"].items()) s.setFlag(name, value);
        if (a.contains("join")) s.joinSpecial(a["join"].get<std::string>());
        if (a.contains("morale"))
            m_morale[a["morale"].at("id").get<std::string>()] = std::clamp(a["morale"].at("value").get<int>(), -2, 2);
        if (a.contains("leave")) s.leaveSpecial(a["leave"].get<std::string>());
        if (a.contains("stay_behind")) s.stayBehind(a["stay_behind"].get<std::string>());
        if (a.contains("choice") && m_outcome.empty()) {
            Json choice = a["choice"];
            if (choice.value("id", "") == "passage_gate" && s.passageMine()) {
                auto at = *s.passageMine();
                choice["at"] = {at.q, at.r};
            }
            raiseChoice(choice, m_firing);
        }
        if (a.contains("ending")) {
            m_outcome = a["ending"];
            m_outcome["night"] = s.day();
            if (!m_rivalLord.empty()) {
                m_outcome["approach"] = s.departureRoute();
                m_outcome["supply_base"] = s.departureRoute() == "castle_road" ? m_rivalLord.value("home", "") : "";
                m_outcome["patrol_orders"] = firedOn("escort_orders") >= 0;
                m_outcome["southern_track_scouted"] = firedOn("southern_route") >= 0 || firedOn("southern_route_visit") >= 0;
                m_outcome["corvin_supplies_taken"] = offer("rookwatch_supplies") && offer("rookwatch_supplies")->taken;
                bool edricAlive = false;
                for (const auto& r : s.rivals()) if (r.name == m_rivalLord.value("name", "")) edricAlive |= r.alive;
                m_outcome["edric_army_defeated"] = !edricAlive;
            }
            m_choice = Json::object();
            m_choiceTrigger.clear();
            m_queuedChoices.clear();
            s.completeChapter(m_outcome.value("return_to", ""));
        }
        if (a.contains("quest_text"))
            if (Quest* q = quest(a["quest_text"].at(0))) q->text = a["quest_text"].at(1).get<std::string>();
        if (a.contains("reveal")) {
            const auto& r = a["reveal"];
            s.revealArea({r.at(0).get<int>(), r.at(1).get<int>()}, r.at(2).get<int>());
        }
        if (a.contains("give")) s.give(poolFrom(a["give"]));
        if (a.contains("clue")) {
            auto name = s.giveClue();
            if (!a.value("silent", false)) {
                if (name)
                    say("Kharim", "The " + *name + " is a dead end. I would stake my maps on it.");
                else
                    say("Kharim", "You have already ruled out every false mine. Trust the one that remains.");
            }
        }
        if (a.contains("item")) s.addItem(a["item"].get<std::string>());
        if (a.contains("take_item")) s.takeItem(a["take_item"].get<std::string>());   // handed over (none = nothing)
        if (a.contains("lore")) {
            Lore entry{a["lore"].at(0).get<std::string>(), a["lore"].at(1).get<std::string>()};
            if (std::none_of(m_lore.begin(), m_lore.end(), [&](const Lore& l) { return l.title == entry.title; })) {
                m_lore.push_back(entry);
                m_story.push_back({{"kind", "lore"}, {"title", entry.title}, {"text", entry.text}});
            }
        }
        if (a.contains("scene")) {                   // a full-screen moment; the map goes on
            Json frames = a["scene"].value("frames", Json::array());
            if (frames.is_array() && !frames.empty()) m_story.push_back({{"kind", "scene"}, {"frames", frames}});
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
            for (const auto& left : s.setArmy(army))   // five stacks at most: the rest cannot join the column
                say("", "There is no room in the column for " + std::to_string(left.count) + " "
                        + s.unitName(left.id) + "; they stay behind.");
        }
        if (a.contains("ambush_from")) {
            const auto& b = a["ambush_from"];
            if (s.ambushFrom(b.at("at"), b.at("band"), b.value("goal", "") == "hunt")) {
                for (const auto& line : b.value("say", Json::array()))
                    say(line.at(0).get<std::string>(), line.at(1).get<std::string>());
            } else {
                say("Scout", "No escort comes. We will have to make our own way.");
            }
        }
        if (a.contains("betray")) {
            const auto& b = a["betray"];
            std::vector<AdventureSession::Stack> army;
            for (const auto& st : b.value("army", Json::array()))
                army.push_back({st.value("id", ""), st.value("count", 0)});
            s.betray(b.value("at", ""), b.value("band", "war-band"), army, b.value("goal", "") == "hunt");
        }
        if (a.contains("disband")) s.disband(a["disband"].get<std::string>());   // the band rides off
        if (a.contains("turncoats")) {                // hired men turn; their lines only if any did
            const auto& t = a["turncoats"];
            if (s.turncoats(t.value("unit", ""), t.value("band", "turncoats")) > 0)
                for (const auto& line : t.value("say", Json::array()))
                    say(line.at(0).get<std::string>(), line.at(1).get<std::string>());
        }
        if (a.contains("event")) fire(s, a["event"].get<std::string>());
        if (a.contains("xp")) s.grantXp(a["xp"].get<int>());   // a story reward
        for (const char* key : {"vanish", "withdraw"}) {   // withdraw: a guard camp walks away
            if (!a.contains(key)) continue;
            const std::string name = a[key].get<std::string>();
            m_vanished.insert(name);
            if (s.removeEncounter(name)) fire(s, "cleared");   // its hex, zone and any pending fight go with it
        }
        if (a.contains("open"))                       // a gate opens: that tile is passable from now on
            s.openTile({a["open"].at(0).get<int>(), a["open"].at(1).get<int>()});
        if (a.contains("offer"))
            for (const auto& def : m_offerDefs)
                if (def.id == a["offer"] && !offer(def.id)) m_offers.push_back(def);
    }
}

bool Scenario::flagsMatch(const Json& flags, const Json& condition) {
    try {
        return Condition::evalFlags(Condition::fromFlags(condition), flags);
    } catch (const std::invalid_argument&) {
        return false;                                // malformed: never holds (load rejects it in a file)
    }
}

std::optional<std::string> Scenario::choose(AdventureSession& s, const std::string& id) {
    if (!awaitingChoice() || !m_outcome.empty() || s.won() || s.lost())
        return "There is no story choice to make.";
    if (m_choice.contains("at")) {
        const auto& at = m_choice["at"];
        if (s.heroPos() != HexCoord(at[0].get<int>(), at[1].get<int>()))
            return "Return to the survey to trace the route.";
    }
    for (const auto& option : m_choice.value("options", Json::array())) {
        if (option.value("id", "") != id) continue;
        // Legacy checks of the old northern demo, refused with their own words (not shown locked).
        Json lore = Json::array();
        for (const auto& title : option.value("requires_lore", Json::array())) lore.push_back({{"lore", title}});
        if (!Condition::eval(Condition::allOf(lore), s))
            return "Read the inscriptions at Varenhold, Coldwater bridge and the assembly court with Ushari first.";
        if (option.value("requires_route", false) && !Condition::eval(Json{{"route", true}}, s))
            return "Secure a way onward: hold Rookwatch, clear the southern Troll Gate, or recover the escort's patrol orders.";
        if (!offered(s, option)) return "That is not one of the available choices.";
        if (auto why = locked(s, option)) return why;
        Json actions = option.value("then", Json::array());
        s.setFlag("choice." + m_choice.value("id", ""), id);   // heavy decisions are remembered
        if (option.value("again", false) && !m_choiceTrigger.empty()) {
            // Not now: the trigger stays live and offers the choice on its next event.
            m_fired.erase(m_choiceTrigger);
            m_firedOn.erase(m_choiceTrigger);
            if (auto pending = s.pendingEncounter())     // and a fight it was raised at is not fought
                if (const MapObjectDef* obj = s.map().objectAt(*pending); obj && engagedAt(m_choiceTrigger) == obj->name)
                    s.cancelEncounter();
        }
        m_choice = Json::object();  // commit before executing; a second click cannot take the other path
        m_choiceTrigger.clear();
        run(s, actions);
        if (!awaitingChoice() && !m_queuedChoices.empty() && m_outcome.empty()) {   // the one waiting behind it
            m_choice = m_queuedChoices.front().value("choice", Json::object());
            m_choiceTrigger = m_queuedChoices.front().value("trigger", "");
            m_queuedChoices.erase(m_queuedChoices.begin());
        }
        return std::nullopt;
    }
    return "That is not one of the available choices.";
}

void Scenario::raiseChoice(const Json& choice, const std::string& trigger) {
    if (!m_outcome.empty()) return;
    // The party choice ("Who stays behind?") and a story choice never overwrite
    // each other: the later one waits until the open one is answered. Two story
    // choices behave as before (the later replaces the earlier).
    const bool party = choice.value("kind", "") == "party";
    if (awaitingChoice() && (party || m_choice.value("kind", "") == "party")) {
        m_queuedChoices.push_back({{"choice", choice}, {"trigger", trigger}});
        return;
    }
    m_choice = choice;
    m_choiceTrigger = trigger;
}

Scenario::Json Scenario::shownWhen(const Json& option) {
    Json parts = Json::array();
    if (option.contains("requires_item")) parts.push_back({{"item", option["requires_item"]}});
    if (option.contains("if")) parts.push_back(option["if"]);
    return Condition::allOf(parts);
}

Scenario::Json Scenario::lockedUnless(const Json& option) {
    Json parts = Json::array();
    if (option.contains("requires_flag")) parts.push_back(Condition::fromFlags(option["requires_flag"]));
    if (option.contains("requires")) parts.push_back(option["requires"]);
    return Condition::allOf(parts);
}

bool Scenario::offered(const AdventureSession& s, const Json& option) {
    return Condition::eval(shownWhen(option), s);
}

std::optional<std::string> Scenario::locked(const AdventureSession& s, const Json& option) {
    if (Condition::eval(lockedUnless(option), s)) return std::nullopt;
    return option.value("locked_text", std::string("That path is not open to you."));
}

Scenario::Json Scenario::offeredChoice(const AdventureSession& s) const {
    Json out = m_choice;
    if (out.contains("options") && out["options"].is_array()) {
        Json kept = Json::array();
        for (const auto& option : out["options"]) if (offered(s, option)) kept.push_back(option);
        out["options"] = kept;
    }
    return out;
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
            {"waiting", Json(m_waiting)}, {"fired_on", Json(m_firedOn)},
            {"choice", m_choice}, {"choice_trigger", m_choiceTrigger}, {"outcome", m_outcome}, {"morale", m_morale},
            {"queued_choices", Json(m_queuedChoices)}, {"story", Json(m_story)}};
}

void Scenario::loadState(const Json& state) {
    m_morale = state.value("morale", std::unordered_map<std::string, int>{});
    m_choice = state.value("choice", Json::object());
    m_choiceTrigger = state.value("choice_trigger", "");
    m_outcome = state.value("outcome", Json::object());
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
    m_queuedChoices = state.value("queued_choices", std::vector<Json>{});
    m_story.clear();
    if (state.contains("story")) {
        m_story = state.value("story", std::vector<Json>{});
    } else {                                     // older saves kept only the undrained scenes
        for (const auto& scene : state.value("scenes", Json::array()))
            m_story.push_back({{"kind", "scene"}, {"frames", scene.value("frames", Json::array())}});
    }
}

std::vector<Scenario::Json> Scenario::drainStory() {
    std::vector<Json> out;
    out.swap(m_story);
    return out;
}

std::vector<Scenario::Json> Scenario::drainKind(const std::string& kind) {
    std::vector<Json> out, rest;
    for (auto& e : m_story) (e.value("kind", "") == kind ? out : rest).push_back(std::move(e));
    m_story.swap(rest);
    return out;
}

std::vector<Scenario::Line> Scenario::drainLines() {
    std::vector<Line> out;
    for (const auto& e : drainKind("line")) out.push_back({e.value("speaker", ""), e.value("text", "")});
    return out;
}

std::vector<Scenario::Json> Scenario::drainScenes() {
    std::vector<Json> out;
    for (auto& e : drainKind("scene")) out.push_back({{"frames", e.value("frames", Json::array())}});
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

std::string Scenario::engagedAt(const std::string& triggerId) const {
    for (const auto& t : m_triggers)
        if (t.value("id", "") == triggerId) {
            const Json when = t.value("when", Json::object());
            return when.value("event", "") == "engage" ? when.value("name", "") : "";
        }
    return "";
}
