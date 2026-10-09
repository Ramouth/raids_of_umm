#pragma once
#include "world/Resources.h"
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

/*
 * CampaignState — what carries from one map of the campaign to the next.
 *
 * AdventureSession::start(..., const CampaignState&) applies it after the
 * per-map reset; AdventureSession::carryOut() reads it back when the map ends.
 * Campaign keeps one, advances its position and writes it to its own small
 * save file (separate from the map's session save).
 *
 *   carried = false   a new campaign: the map starts as it would on its own
 *                     (its own treasury, army and start_companions); only the
 *                     flags and the level cap apply.
 *   carried = true    everything below replaces the map's defaults. Companions
 *                     arrive travelling, unwounded and paid; the file's
 *                     start_companions still join (a returning companion keeps
 *                     her progression), but the default Ushari does not.
 *
 * At most two companions ride (AdventureSession::MAX_RIDING): a third is
 * carried in departed with waiting = true. shownTips: combat tip ids already
 * shown; they carry even on a new campaign's first map (as flags do).
 *
 * flags are the story's memory (name → any JSON value), set by the "flag"
 * action and by every resolved choice ("choice.<id>" = option id). Triggers
 * read them with when.flag / when.unless_flag (see Scenario.h).
 *
 * levelCap is the current map's max_level: the commander cannot level past it
 * there, and experience beyond it is lost (counted in the map's stats).
 * history holds one entry per finished map:
 *   {"map", "level_cap", "cap_reached_day", "xp_lost_to_cap", "days"}
 */
struct CampaignState {
    using Json = nlohmann::json;
    struct Stack { std::string id; int count = 0; bool bitten = false; };   // bitten: rabies
    struct Companion {
        std::string id, name, title;
        int level = 1, xp = 0, morale = 0;
        bool waiting = false;   // departed: stayed behind for want of room, not left
    };

    // Position in the campaign.
    std::string chapter, map;
    bool        carried = false;

    // Commander.
    int level = 1, xp = 0;
    std::string path;                         // "" = not chosen yet
    std::vector<std::string> learned;         // path skills, in order
    std::vector<Stack> army;
    std::vector<std::string> items;           // every item owned (worn ones too)
    std::map<std::string, std::string> equipped;   // slot → item id
    std::vector<Companion> companions;        // riding with the commander
    std::vector<Companion> departed;          // gone for now; keep progression for a reunion
    ResourcePool treasury;

    Json flags = Json::object();
    std::vector<std::string> shownTips;       // combat tips already shown (once per campaign)
    int  levelCap = 10;                       // AdventureSession::MAX_HERO_LEVEL unless the map says less
    Json lastMap = Json::object();            // stats of the map carryOut() was taken from
    Json history = Json::array();             // stats of every finished map, in order

    Json toJson() const;
    static CampaignState fromJson(const Json& j);
};

/*
 * Campaign — a sequence of chapters, each a sequence of small maps.
 *
 * Loaded from a campaign file (data/campaign.json in the game; tests write
 * their own). Paths inside it are relative to the data directory (which is
 * also the registry the maps load units from); absolute paths are kept:
 *
 *   { "chapters": [
 *       { "id": "marches", "title": "The Marches",
 *         "maps": [ { "id": "1.0", "title": "The last hunt",
 *                     "map": "maps/x.json",
 *                     "encounters": "maps/x.encounters.json",   (optional)
 *                     "triggers": "maps/x.triggers.json",       (optional)
 *                     "max_level": 3 } ] } ] }                   (optional, default 10)
 *
 * Map ids are unique across the whole campaign. A map's ending may name the
 * next map ("ending": {"next": "1.2"}); otherwise the campaign goes to the
 * next map in order, then to the next chapter's first map, then ends.
 */
class Campaign {
public:
    using Json = nlohmann::json;
    struct MapEntry {
        std::string id, title, map, encounters, triggers;   // paths as written in the file
        int maxLevel = 10;
    };
    struct Chapter { std::string id, title; std::vector<MapEntry> maps; };
    struct Advance {
        std::optional<std::string> error;   // unknown map id, nothing loaded
        bool finished = false;               // the campaign is over
        bool newChapter = false;             // moved into another chapter
    };

    // dataDir "" = the directory the campaign file is in. Puts the campaign
    // at its first map with a fresh (not carried) state.
    std::optional<std::string> load(const std::string& path, const std::string& dataDir = "");
    bool loaded() const { return !m_chapters.empty(); }
    const std::vector<Chapter>& chapters() const { return m_chapters; }
    const std::string& dataDir() const { return m_dataDir; }
    const std::string& path() const { return m_path; }

    const MapEntry* current() const;          // nullptr once finished
    const Chapter*  currentChapter() const;
    const MapEntry* find(const std::string& mapId) const;
    bool finished() const { return m_finished; }
    // dataDir + "/" + relative ("" and absolute paths stay as they are).
    std::string resolve(const std::string& relative) const;

    // What the next map starts from (the current map's level cap applied).
    CampaignState startState() const;
    // Takes what the finished map carried out; keeps the campaign position and
    // records the map's stats in history.
    void finishMap(const CampaignState& out);
    // Moves to 'nextMapId', or (empty) the next map in order.
    Advance advance(const std::string& nextMapId = "");

    CampaignState&       state()       { return m_state; }
    const CampaignState& state() const { return m_state; }

    // Small save file: the campaign file, data dir, finished, and the state.
    // loadSave() reloads the campaign definition the save names.
    Json saveJson() const;
    std::optional<std::string> saveFile(const std::string& path) const;
    std::optional<std::string> loadSave(const Json& save);
    std::optional<std::string> loadFile(const std::string& path);

    Json describe(const MapEntry& entry) const;   // entry with resolved paths, for the UI

private:
    std::vector<Chapter> m_chapters;
    std::string          m_path, m_dataDir;
    CampaignState        m_state;
    bool                 m_finished = false;
};
