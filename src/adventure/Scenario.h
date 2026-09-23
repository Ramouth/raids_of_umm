#pragma once
#include "hex/HexCoord.h"
#include "world/Resources.h"
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

class AdventureSession;

/*
 * Scenario — Warcraft 3-style trigger script for one map.
 *
 * Loaded from data/maps/<map>.triggers.json:
 *
 *   { "quests":   [ { "id", "title", "text", "main": bool } ],
 *     "triggers": [ { "id", "when": {...}, "do": [ {...}, ... ] } ] }
 *
 * when.event:  "start" | "day" (day) | "see" (cell) | "see_type" (type)
 *              | "enter_q" (q_min) | "visit" (name) | "capture" (name)
 *              | "encounter_won" (name) | "mines_held" (count) | "item" (item)
 * do actions:  "say": [[speaker, text], ...]   — dialogue lines for the UI
 *              "quest": id / "quest_done": id / "quest_text": [id, text]
 *              "reveal": [q, r, radius]         — lift fog
 *              "give": {"Gold": 500, ...}       — resources
 *              "clue": true                     — rule out one wrong old mine
 *              "offer": id                      — a choice the UI shows at a visit
 *              "join": id                       — a special character joins the hero
 * (event "quests_done" (count) fires as optional quests complete)
 *
 * Each trigger fires once. Offers are player choices (e.g. pay a tribute)
 * made through AdventureSession::acceptOffer().
 */
class Scenario {
public:
    using Json = nlohmann::json;

    struct Line  { std::string speaker, text; };
    struct Quest { std::string id, title, text; bool main = false; bool done = false; };
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

    // Event hooks — AdventureSession calls these; matching triggers fire.
    void fire(AdventureSession& s, const std::string& event, const Json& detail = Json::object());

    // Dialogue produced since the last drain (UI shows it WC3-transmission style).
    std::vector<Line> drainLines();
    const std::vector<Quest>& quests() const { return m_quests; }
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

    bool                            m_loaded = false;
    Json                            m_triggers = Json::array();
    std::unordered_set<std::string> m_fired;
    std::vector<Quest>              m_quests;       // active or done, in order added
    std::vector<Quest>              m_questDefs;
    std::vector<Offer>              m_offers;
    std::vector<Offer>              m_offerDefs;
    std::vector<Line>               m_lines;
};
