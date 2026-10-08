// StoryObjectives.cpp — story objectives derived from triggers and quests.
#include "StoryObjectives.h"
#include "AdventureSession.h"
#include <set>
#include <string>
#include <tuple>

using Json = Scenario::Json;

namespace {

const MapObjectDef* objectNamed(const AdventureSession& s, const std::string& name) {
    if (name.empty() || s.scenario().vanished().count(name)) return nullptr;
    for (const auto& o : s.map().objects())
        if (o.name == name) return &o;
    return nullptr;
}

// The trigger's own story conditions (not its event) hold right now.
bool armed(const AdventureSession& s, const Json& when) {
    const Scenario& sc = s.scenario();
    const std::string after = when.value("after", "");
    if (!after.empty() && !sc.fired(after)) return false;
    const int delay = when.value("delay", 0);
    if (delay > 0 && (after.empty() || s.day() < sc.firedOn(after) + delay)) return false;
    if (when.contains("flag") && !Scenario::flagsMatch(s.flags(), when["flag"])) return false;
    if (when.contains("unless_flag") && Scenario::flagsMatch(s.flags(), when["unless_flag"])) return false;
    return true;
}

} // namespace

std::vector<StoryObjective> storyObjectives(const AdventureSession& s) {
    const Scenario& sc = s.scenario();
    std::vector<StoryObjective> out;
    std::set<std::tuple<std::string, std::string, std::string>> seen;
    auto add = [&](const std::string& name, HexCoord at, const std::string& event,
                   const std::string& trigger, bool ready) {
        if (seen.insert({name, event, trigger}).second) out.push_back({name, at, event, trigger, ready});
    };

    for (const auto& t : sc.triggers()) {
        const std::string id = t.value("id", "");
        if (sc.fired(id)) continue;
        const Json when = t.value("when", Json::object());
        const std::string unless = when.value("unless", "");
        if (!unless.empty() && sc.fired(unless)) continue;   // overtaken: it can never fire
        const std::string event = when.value("event", "");
        const bool ready = armed(s, when);
        if (event == "visit") {
            if (const auto* o = objectNamed(s, when.value("name", ""))) add(o->name, o->pos, event, id, ready);
        } else if (event == "capture") {
            const auto* o = objectNamed(s, when.value("name", ""));
            if (o && s.control().count(o->pos) && s.owner(o->pos) != Faction::Player)
                add(o->name, o->pos, event, id, ready);
        } else if (event == "encounter_won" || event == "engage") {
            const auto* o = objectNamed(s, when.value("name", ""));
            if (o && s.isEncounter(o->pos)) add(o->name, o->pos, event, id, ready);
        } else if (event == "cleared") {
            for (const auto& n : when.value("names", Json::array())) {
                const auto* o = n.is_string() ? objectNamed(s, n.get<std::string>()) : nullptr;
                if (o && s.isEncounter(o->pos)) add(o->name, o->pos, event, id, ready);
            }
        } else if (event == "enter_q") {
            // Every path east crosses the column q == q_min, so its open tiles are the targets.
            const int q = when.value("q_min", 999);
            for (const auto& [c, tile] : s.map()) {
                if (c.q != q || !tile.passable || s.isEncounter(c) || s.map().objectAt(c)) continue;
                add("the road east (" + std::to_string(c.q) + "," + std::to_string(c.r) + ")", c, event, id, ready);
            }
        } else if (event == "rival_beaten") {
            for (const auto& r : s.rivals())
                if (r.alive && r.name == when.value("name", "")) { add(r.name, r.pos, event, id, ready); break; }
        }
    }
    for (const auto& q : sc.quests()) {
        if (q.done) continue;
        for (const auto& o : s.map().objects()) {
            if (o.name.empty() || sc.vanished().count(o.name)) continue;
            if (q.title.find(o.name) != std::string::npos || q.text.find(o.name) != std::string::npos)
                add(o.name, o.pos, "quest", "", true);
        }
    }
    return out;
}
