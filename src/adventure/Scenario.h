#pragma once
#include "hex/HexCoord.h"
#include "world/Resources.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class AdventureSession;

/*
 * Scenario — Warcraft 3-style trigger script for one map.
 *
 * Loaded from data/maps/<map>.triggers.json:
 *
 *   { "quests":   [ { "id", "title", "text", "main": bool } ],
 *     "triggers": [ { "id", "when": {...}, "do": [ {...}, ... ] } ],
 *     "start_companions": [id, ...],    — who rides with the hero on day 1
 *                                          (absent = the default, Ushari)
 *     "start_army": [{"id","count"}] }  — the hero's army when the map starts
 *                                          without carried state (a new campaign
 *                                          or a lone map); carried state wins
 *
 * when.event:  "start" | "night" (End Day) | "day" (day) | "see" (cell) | "see_type" (type)
 *              | "enter_q" (q_min) | "visit" (name) | "capture" (name)
 *              | "encounter_won" (name) | "mines_held" (count) | "item" (item)
 *              | "cleared" (names: [...]) — every named guard camp has been beaten
 *                                             or has left the map (vanish / withdraw)
 *              | "engage" (name) — the hero is about to fight that guard camp
 * do actions:  "say": [[speaker, text], ...]   — dialogue lines for the UI
 *              "quest": id / "quest_done": id / "quest_text": [id, text]
 *              "reveal": [q, r, radius]         — lift fog
 *              "give": {"Gold": 500, ...}       — resources
 *              "clue": true                     — rule out one wrong old mine
 *              "offer": id                      — a choice the UI shows at a visit
 *              "choice": {id, title, text, options: [{id, label, detail, then}]}
 *              "ending": {id, heading, body, frames, return_to?, next?} — complete the map;
 *                                               next: the campaign map that follows ("" = next in order)
 *              "flag": {"name": value, ...}     — remember something for later maps (any JSON value)
 *              "leave": id — companion departs, retaining progression for a reunion
 *              "morale": {id, value} — chapter-persistent companion attack modifier (-2 to 2)
 *              "join": id                       — a special character joins the hero
 *              "troops": [{"id","count"}]      — soldiers join the hero's army
 *              "item": id                       — an item is handed to the hero
 *              "lore": [title, text]            — a codex entry (the journal's Lore page)
 *              "ambush_from": {"at": town, "band": name, "say": [[speaker,text]]}
 *                                               — half its garrison attacks as an escort
 *              "betray": {"at": name, "band": name, "army": [{"id","count"}]}
 *                                               — that site turns rival; a war-band rides out
 *              "xp": n                          — experience for the commander and travelling companions
 *              "vanish": name                   — that map object leaves the map (a figure walks off);
 *                                                 a guard camp takes its hex, zone of control and
 *                                                 any fight about to start with it along
 *              "withdraw": name                 — a guard camp walks away: a fight pending with it
 *                                                 is called off and it leaves the map (as vanish).
 *                                                 Use in a choice option's "then" at an "engage".
 *              "turncoats": {"unit": id, "band": name, "say": [[speaker, text], ...]}
 *                                               — hired men of that unit turn and ambush the hero
 *              "open": [q, r]                   — that closed tile (map "passable": false, e.g. a
 *                                                 gatekeeper's bridge) is passable for the rest of
 *                                                 this map; saved, not carried to the next map
 * (event "quests_done" (count) fires as optional quests complete;
 *  event "rival_beaten" (name) fires when the hero breaks a war-band)
 *
 * when.after: id — only after that trigger has fired (story order).
 * when.unless: id — never, once that trigger has fired (a beat overtaken by events).
 * when.delay: N — with "after": only N or more days after that trigger fired.
 * when.wait: id — if that trigger has not fired yet, hold this one and run it
 *                 right after it does (e.g. a companion's line before she has joined).
 * when.flag: condition — only while the story flags match it; checked each time
 *                 the event comes up, so it can fire later once the flag is set.
 * when.unless_flag: condition — not while the flags match it.
 *   A condition is a name (that flag is set and truthy: not null/false/0/""),
 *   a list of names (all of them), or {"name": value, ...} (all equal).
 * Choice options take "requires_flag": condition — refused until it holds; the
 * snapshot marks such options "locked". Every resolved choice records the flag
 * "choice.<choice id>" = "<option id>" before its "then" actions run.
 * An option with "again": true closes the choice without using up the trigger
 * that raised it: that trigger fires again on its next matching event ("Turn
 * back", "Not yet"). If it was raised at an "engage", the fight is not fought;
 * the camp stays where it is.
 * Flags live in AdventureSession: they persist in saves and carry to the next
 * campaign map (CampaignState).
 * Root rival_lord configures a castle opponent: home, name, army, garrison,
 * mobilize_after (trigger id), territory_q_min (opening patrol boundary).
 * Root passage_ends_scenario defaults to true; false emits passage_found instead.
 * Root essential_companions keeps named companions with the search until a leave action.
 * A choice with allow_exploration permits travel and turns while unresolved.
 * The passage gate records its mine in at:[q,r]; it can only be solved there.
 * Pending choices and outcomes persist in saves.
 * Each trigger fires once. Offers are player choices (e.g. pay a tribute)
 * made through AdventureSession::acceptOffer().
 */
class Scenario {
public:
    using Json = nlohmann::json;

    struct Line  { std::string speaker, text; };
    struct Quest { std::string id, title, text; bool main = false; bool done = false; };
    struct Lore  { std::string title, text; };   // codex entry: what the player has learned
    struct Offer {
        std::string id, label;       // label shown on the button
        std::string at;              // object name where it is offered
        ResourcePool cost;           // paid on accept
        std::string item;            // item handed over on accept ("" = none)
        std::string quest;           // quest completed on accept
        Json        then;            // actions run on accept
        bool        taken = false;
    };

    std::optional<std::string> load(const std::string& path);
    bool loaded() const { return m_loaded; }
    // Optional castle opponent. Definitions reload from the scenario; trigger dates persist.
    const Json& rivalLord() const { return m_rivalLord; }
    const std::string& surveySite() const { return m_surveySite; }
    int firedOn(const std::string& id) const {
        auto it = m_firedOn.find(id);
        return it == m_firedOn.end() ? -1 : it->second;
    }
    // True once the trigger has fired, been overtaken (when.unless) or is held (when.wait).
    bool fired(const std::string& id) const { return m_fired.count(id) > 0; }
    const Json& triggers() const { return m_triggers; }   // definitions, as loaded
    bool essential(const std::string& id) const { return m_essential.count(id) > 0; }
    bool passageEndsScenario() const { return m_passageEndsScenario; }
    const Json& choice() const { return m_choice; }
    int morale(const std::string& id) const { return m_morale.count(id) ? m_morale.at(id) : 0; }
    void setMorale(const std::string& id, int value) { m_morale[id] = std::clamp(value, -2, 2); }
    const Json& outcome() const { return m_outcome; }
    // The campaign map the ending names; "" = none named (or no ending yet).
    std::string nextMap() const { return m_outcome.value("next", ""); }
    // True when the flags satisfy a when.flag / requires_flag condition (see above).
    static bool flagsMatch(const Json& flags, const Json& condition);
    bool awaitingChoice() const { return !m_choice.empty(); }
    bool blocksAdventure() const { return awaitingChoice() && !m_choice.value("allow_exploration", false); }
    std::optional<std::string> choose(AdventureSession& s, const std::string& id);
    // Companions with the hero at the start; nullopt = the file does not say.
    const std::optional<std::vector<std::string>>& startCompanions() const { return m_startCompanions; }
    // The army the hero starts with absent carried state ([{"id","count"}]; empty = none named).
    const Json& startArmy() const { return m_startArmy; }
    // The engage trigger's guard name ("" = that trigger is not an engage).
    std::string engagedAt(const std::string& triggerId) const;
    // The trigger that raised the open choice ("" = none / raised by a choice or offer).
    const std::string& choiceTrigger() const { return m_choiceTrigger; }
    // Map objects a "vanish" or "withdraw" action has taken off the map.
    const std::unordered_set<std::string>& vanished() const { return m_vanished; }

    // Event hooks — AdventureSession calls these; matching triggers fire.
    void fire(AdventureSession& s, const std::string& event, const Json& detail = Json::object());

    // Dialogue produced since the last drain (UI shows it WC3-transmission style).
    std::vector<Line> drainLines();
    const std::vector<Quest>& quests() const { return m_quests; }
    const std::vector<Lore>&  lore()   const { return m_lore; }
    // Offers available at an object right now.
    std::vector<const Offer*> offersAt(const std::string& objectName) const;
    Offer* offer(const std::string& id);
    void run(AdventureSession& s, const Json& actions);
    void say(const std::string& speaker, const std::string& text) { m_lines.push_back({speaker, text}); }
    // Save/load: fired triggers, quest progress, offers taken (defs come from the file).
    Json saveState() const;
    void loadState(const Json& state);

private:
    bool matches(const Json& when, const std::string& event, const Json& detail) const;
    Quest* quest(const std::string& id);
    void fireTrigger(AdventureSession& s, const Json& trigger);

    std::string                     m_surveySite;
    Json                            m_rivalLord = Json::object();
    bool                            m_loaded = false;
    bool                            m_passageEndsScenario = true;
    std::unordered_set<std::string> m_essential;
    std::unordered_map<std::string, int> m_morale; // persistent companion combat attack modifier
    Json                            m_choice = Json::object();
    std::string                     m_choiceTrigger;  // trigger that raised m_choice (saved)
    std::string                     m_firing;         // trigger whose actions are running
    Json                            m_outcome = Json::object();
    Json                            m_triggers = Json::array();
    std::unordered_set<std::string> m_fired;
    std::vector<Quest>              m_quests;       // active or done, in order added
    std::vector<Quest>              m_questDefs;
    std::vector<Offer>              m_offers;
    std::vector<Offer>              m_offerDefs;
    std::vector<Line>               m_lines;
    std::vector<Lore>               m_lore;
    std::optional<std::vector<std::string>> m_startCompanions;
    Json                            m_startArmy = Json::array();
    std::unordered_set<std::string> m_won;          // guard camps beaten, by name
    std::unordered_set<std::string> m_vanished;     // map objects gone from the map
    std::vector<std::string>        m_waiting;      // triggers held by when.wait
    std::unordered_map<std::string, int> m_firedOn; // trigger id → day it fired
};
