#pragma once
#include "hex/HexCoord.h"
#include "world/Resources.h"
#include <nlohmann/json.hpp>
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
 *     "start_companions": [id, ...] }   — who rides with the hero on day 1
 *                                          (absent = the default, Ushari)
 *
 * when.event:  "start" | "day" (day) | "see" (cell) | "see_type" (type)
 *              | "enter_q" (q_min) | "visit" (name) | "capture" (name)
 *              | "encounter_won" (name) | "mines_held" (count) | "item" (item)
 *              | "cleared" (names: [...]) — every named guard camp has been beaten
 *              | "engage" (name) — the hero is about to fight that guard camp
 * do actions:  "say": [[speaker, text], ...]   — dialogue lines for the UI
 *              "quest": id / "quest_done": id / "quest_text": [id, text]
 *              "reveal": [q, r, radius]         — lift fog
 *              "give": {"Gold": 500, ...}       — resources
 *              "clue": true                     — rule out one wrong old mine
 *              "offer": id                      — a choice the UI shows at a visit
 *              "join": id                       — a special character joins the hero
 *              "troops": [{"id","count"}]      — soldiers join the hero's army
 *              "item": id                       — an item is handed to the hero
 *              "lore": [title, text]            — a codex entry (the journal's Lore page)
 *              "betray": {"at": name, "band": name, "army": [{"id","count"}]}
 *                                               — that site turns rival; a war-band rides out
 *              "vanish": name                   — that map object leaves the map (a figure walks off)
 *              "turncoats": {"unit": id, "band": name, "say": [[speaker, text], ...]}
 *                                               — hired men of that unit turn and ambush the hero
 * (event "quests_done" (count) fires as optional quests complete;
 *  event "rival_beaten" (name) fires when the hero breaks a war-band)
 *
 * when.after: id — only after that trigger has fired (story order).
 * when.unless: id — never, once that trigger has fired (a beat overtaken by events).
 * when.delay: N — with "after": only N or more days after that trigger fired.
 * when.wait: id — if that trigger has not fired yet, hold this one and run it
 *                 right after it does (e.g. a companion's line before she has joined).
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
    // Companions with the hero at the start; nullopt = the file does not say.
    const std::optional<std::vector<std::string>>& startCompanions() const { return m_startCompanions; }
    // Map objects a "vanish" action has taken off the map.
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

    bool                            m_loaded = false;
    Json                            m_triggers = Json::array();
    std::unordered_set<std::string> m_fired;
    std::vector<Quest>              m_quests;       // active or done, in order added
    std::vector<Quest>              m_questDefs;
    std::vector<Offer>              m_offers;
    std::vector<Offer>              m_offerDefs;
    std::vector<Line>               m_lines;
    std::vector<Lore>               m_lore;
    std::optional<std::vector<std::string>> m_startCompanions;
    std::unordered_set<std::string> m_won;          // guard camps beaten, by name
    std::unordered_set<std::string> m_vanished;     // map objects gone from the map
    std::vector<std::string>        m_waiting;      // triggers held by when.wait
    std::unordered_map<std::string, int> m_firedOn; // trigger id → day it fired
};
