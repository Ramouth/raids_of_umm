#pragma once
#include "adventure/AdventureSession.h"
#include <nlohmann/json.hpp>

// JSON facade over AdventureSession for the GDExtension (no Godot types here,
// so native tests can drive it too). Cells travel as [q, r] pairs.
class AdventureBridge {
public:
    using Json = nlohmann::json;
    Json start(const std::string& map_path, const std::string& data_dir,
               const std::string& encounters_path = "", uint32_t seed = 0,
               const std::string& triggers_path = "");
    Json snapshot() const;
    Json preview(int q, int r) const;
    Json travel(int q, int r);
    Json end_day();
    // battle: the combat snapshot's "battle" record (null = none). The reply adds
    //   "battle": that record with result (victory | defeat | retreat) and xp (what
    //   the commander gained, after the level cap), and
    //   "spoils": {resources: {name: n} (non-zero only), items: [{id, name, kind, text}]}.
    // The record's "tips" (ids the battle showed) are remembered for the campaign.
    // Refused ({ok: false}) while "encounter_awaits_choice" holds.
    Json resolve_encounter(bool victory, const Json& enemy_survivors = nullptr, const Json& battle = nullptr);
    Json tip_shown(const std::string& id);   // a combat tip was shown: never again this campaign
    Json set_army(const Json& stacks);
    Json build(int q, int r, const std::string& id);
    Json equip(const std::string& id);       // commander's paper doll
    Json unequip(const std::string& slot);                              // one per town per day
    Json trade(const std::string& give, const std::string& get, int amount);     // marketplace
    Json trade_quote(const std::string& give, const std::string& get, int amount) const;
    // Companions who fell in the last battle: wounded, or gone if it was lost.
    Json companions_fell(const Json& fallen, bool lost);
    Json recruit(int q, int r, const std::string& unit_id, int count);
    Json accept_offer(const std::string& id);
    Json choose_story(const std::string& id);
    Json transfer(int q, int r, const std::string& unit_id, int count, bool to_garrison);
    Json station(const std::string& id, bool stay);
    // Writes the session (+ caller extras, e.g. Godot inventory) to path.
    Json save(const std::string& path, const Json& extra);
    Json load(const std::string& path);   // snapshot + "extra"
    Json add_item(const std::string& id);
    Json buy_fieldwork(const std::string& kind);
    Json choose_path(const std::string& id);    // the commander's path, chosen once at level 2
    Json join_special(const std::string& id);   // a companion joins (tests, story tools)
    Json claim_chest(bool gold);          // snapshot + "found"

    // ── Campaign: a sequence of chapters of small maps (CampaignState.h) ────
    // Every reply carries "ok" (and "error" when false). "campaign" is
    //   {finished, chapter: {id, title}, map: {id, title, max_level, map,
    //    encounters, triggers} (paths resolved against the data dir; null once
    //    finished), state: CampaignState JSON (flags, carried commander, history)}.
    // campaign_load(path, data_dir)   load a campaign file and stand at its first
    //                                 map with a fresh state; data_dir "" = the
    //                                 file's directory. → {ok, campaign, chapters}
    // campaign_current()              → {ok, campaign}
    // campaign_start_map(seed)        start the current map with the carried state
    //                                 (flags, level cap; commander, army, items,
    //                                 companions, treasury once carried).
    //                                 → snapshot + lines + campaign
    // campaign_advance(next_map)      after the map's ending: carry the session out,
    //                                 record its stats, and move on to next_map, else
    //                                 the ending's "next", else the next in order.
    //                                 → {ok, finished, new_chapter, campaign}
    // campaign_save(path)             the campaign's own small file (not the map save)
    // campaign_load_save(path)        → {ok, campaign}; then campaign_start_map, or
    //                                 load(...) the map save taken during that map
    Json campaign_load(const std::string& path, const std::string& data_dir = "");
    Json campaign_current() const;
    Json campaign_start_map(uint32_t seed = 0);
    Json campaign_advance(const std::string& next_map = "");
    Json campaign_save(const std::string& path) const;
    Json campaign_load_save(const std::string& path);

private:
    // Attaches what the last action produced for the UI: "story" (every event in
    // order, see Scenario::drainStory: line | scene | lore | item | item_lost | party),
    // "lines" and "scenes" (the same events filtered, for older callers), "level_ups".
    Json with_lines(Json out);
    void attach_story(Json& out);   // drains the story queue into story / lines / scenes
    Json campaign_json() const;
    AdventureSession session_;
    Campaign         campaign_;
};
