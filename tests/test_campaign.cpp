// test_campaign.cpp — the campaign layer: state carried from map to map,
// story flags, per-map level caps, chapter order and the campaign save.
// Fixture maps and scripts are written to a temp directory by the tests; the
// data registry is the real data/.
#include "test_runner.h"
#include "adventure/AdventureSession.h"
#include "adventure/CampaignState.h"
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace {

using Json = nlohmann::json;
namespace fs = std::filesystem;

WorldMap smallMap() {
    WorldMap map;
    map.clear(5);
    for (const auto& [c, t] : map) map.setTile(c, makeTile(Terrain::Grass));
    map.placeObject({{-3, 0}, ObjType::Town, "Home", 1});
    return map;
}

// Map A: a choice at the start; "burn" needs the torch flag.
const Json kTriggersA = Json::parse(R"({
  "triggers": [
    {"id": "oath", "when": {"event": "start"}, "do": [
      {"choice": {"id": "oath", "title": "The prisoners", "options": [
        {"id": "spare", "label": "Spare them", "then": [{"flag": {"mercy": true, "spared": 3}}]},
        {"id": "burn",  "label": "Burn the camp", "requires_flag": "torch", "then": []}
      ]}}]}
  ]})");

// Map B: remembers what was decided on map A.
const Json kTriggersB = Json::parse(R"({
  "start_companions": [],
  "triggers": [
    {"id": "remember", "when": {"event": "start", "flag": {"choice.oath": "spare"}},
     "do": [{"say": [["Ushari", "You spared them."]]}, {"give": {"Gold": 111}}]},
    {"id": "ashes", "when": {"event": "start", "unless_flag": "mercy"},
     "do": [{"say": [["Ushari", "Only ashes here."]]}]},
    {"id": "late", "when": {"event": "night", "flag": ["mercy", "debt"]},
     "do": [{"say": [["Ushari", "The debt is called in."]]}]}
  ]})");

struct Fixture {
    std::string dir;
    Fixture() {
        dir = (fs::temp_directory_path() / "umm_campaign_tests").string();
        fs::create_directories(dir + "/maps");
        CHECK(!smallMap().saveJson(dir + "/maps/a.json"));
        CHECK(!smallMap().saveJson(dir + "/maps/b.json"));
        std::ofstream(dir + "/maps/a.triggers.json") << kTriggersA.dump();
        std::ofstream(dir + "/maps/b.triggers.json") << kTriggersB.dump();
        std::ofstream(dir + "/campaign.json") << Json::parse(R"({"chapters": [
          {"id": "marches", "title": "The Marches", "maps": [
            {"id": "1.0", "title": "The last hunt", "map": "maps/a.json", "triggers": "maps/a.triggers.json", "max_level": 3},
            {"id": "1.1", "title": "Varenhold", "map": "maps/b.json", "triggers": "maps/b.triggers.json", "max_level": 5},
            {"id": "1.2", "title": "The camps", "map": "maps/b.json"}]},
          {"id": "east", "title": "The East Road", "maps": [
            {"id": "2.0", "title": "The east road", "map": "maps/a.json"}]}]})").dump();
    }
    std::string path(const std::string& rel) const { return dir + "/" + rel; }
};

bool said(const std::vector<Scenario::Line>& lines, const std::string& text) {
    return std::any_of(lines.begin(), lines.end(), [&](const Scenario::Line& l) { return l.text == text; });
}

int levelOf(const std::vector<CampaignState::Companion>& list, const std::string& id) {
    for (const auto& c : list) if (c.id == id) return c.level;
    return -1;
}

} // namespace

// ── Carried state ─────────────────────────────────────────────────────────────

SUITE("Campaign — commander, army, items, companions and treasury survive two map starts") {
    Fixture f;
    AdventureSession a;
    CHECK(!a.start(f.path("maps/a.json"), "data", "", 1, f.path("maps/a.triggers.json"), CampaignState{}));
    CHECK(!a.scenario().choose(a, "spare"));
    a.grantXp(AdventureSession::xpForLevel(3));
    CHECK(!a.choosePath("siegemaster"));
    a.setArmy({{"woad_runner", 12}, {"cruth_slinger", 5}});
    a.addItem("greyfang_helm");
    a.addItem("hale_signet");
    CHECK(!a.equip("greyfang_helm"));
    a.joinSpecial("kharim");
    a.scenario().run(a, Json::parse(R"([{"morale": {"id": "ushari", "value": -1}}])"));
    a.give([] { ResourcePool p; p[Resource::Gold] = 500; p[Resource::Wood] = 7; return p; }());
    const int ushariLevel = a.specials().front().level;
    const auto gold = a.treasury()[Resource::Gold];
    CampaignState out = a.carryOut();
    CHECK(out.carried);

    AdventureSession b;   // second start: map B, carried
    CHECK(!b.start(f.path("maps/b.json"), "data", "", 1, f.path("maps/b.triggers.json"), out));
    AdventureSession c;   // third: the same carried state again, out of map B
    CHECK(!c.start(f.path("maps/a.json"), "data", "", 2, "", b.carryOut()));
    for (AdventureSession* s : {&b, &c}) {
        CHECK_EQ(s->heroProgress().level, 3);
        CHECK(s->heroPath() == "siegemaster");
        for (const auto& p : s->heroPaths())                  // the path's skills up to level 3
            if (p.id == "siegemaster")
                for (const auto& n : p.nodes) CHECK_EQ(s->learned(n.id), n.level <= 3);
        CHECK_EQ(s->armyBonus().readiedShot, a.armyBonus().readiedShot);
        auto army = s->army();
        CHECK_EQ((int)army.size(), 2);
        CHECK(army[0].id == "woad_runner" && army[0].count == 12);
        CHECK(s->hasItem("greyfang_helm") && s->hasItem("hale_signet"));
        CHECK(s->equipped().at("helm") == "greyfang_helm");
        CHECK_EQ((int)s->specials().size(), 2);              // Ushari and Kharim, not a fresh Ushari
        CHECK_EQ(s->specials().front().level, ushariLevel);
        CHECK_EQ(s->scenario().morale("ushari"), -1);
        CHECK_EQ(s->treasury()[Resource::Wood], a.treasury()[Resource::Wood]);
        CHECK(s->flags().value("choice.oath", "") == "spare");
    }
    CHECK_EQ(b.treasury()[Resource::Gold], gold + 111);    // map B's flag-gated gift on top of the carried gold
    CHECK_EQ(c.treasury()[Resource::Gold], gold + 111);
}

SUITE("Campaign — old start() still resets everything (no carry, Ushari by default)") {
    Fixture f;
    AdventureSession a;
    CHECK(!a.start(f.path("maps/b.json"), "data", "", 1, f.path("maps/b.triggers.json")));
    CHECK(a.flags().empty());
    CHECK_EQ(a.levelCap(), AdventureSession::MAX_HERO_LEVEL);
    CHECK_EQ(a.treasury()[Resource::Gold], AdventureSession::STARTING_GOLD);
    AdventureSession b;
    CHECK(!b.start(smallMap(), "data"));
    CHECK_EQ((int)b.specials().size(), 1);
    CHECK(b.specials().front().id == "ushari");
}

// ── Flags ─────────────────────────────────────────────────────────────────────

SUITE("Flags — a resolved choice records choice.<id>; requires_flag locks an option") {
    Fixture f;
    AdventureSession s;
    CHECK(!s.start(f.path("maps/a.json"), "data", "", 1, f.path("maps/a.triggers.json"), CampaignState{}));
    CHECK(s.scenario().awaitingChoice());
    CHECK(s.scenario().choose(s, "burn").has_value());      // no torch flag
    CHECK(s.scenario().awaitingChoice());
    CHECK(!s.flags().contains("choice.oath"));
    CHECK(!s.scenario().choose(s, "spare"));
    CHECK(s.flags().at("choice.oath") == "spare");
    CHECK(s.flags().at("mercy") == true);
    CHECK(s.flags().at("spared") == 3);
}

SUITE("Flags — requires_flag opens the option once the flag is set") {
    Fixture f;
    CampaignState carry;
    carry.flags["torch"] = "lit";
    AdventureSession s;
    CHECK(!s.start(f.path("maps/a.json"), "data", "", 1, f.path("maps/a.triggers.json"), carry));
    CHECK(!s.scenario().choose(s, "burn"));
    CHECK(s.flags().at("choice.oath") == "burn");
}

SUITE("Flags — a flag set on map A gates triggers on map B") {
    Fixture f;
    AdventureSession a;
    CHECK(!a.start(f.path("maps/a.json"), "data", "", 1, f.path("maps/a.triggers.json"), CampaignState{}));
    CHECK(!a.scenario().choose(a, "spare"));
    AdventureSession b;
    CHECK(!b.start(f.path("maps/b.json"), "data", "", 1, f.path("maps/b.triggers.json"), a.carryOut()));
    auto lines = b.scenario().drainLines();
    CHECK(said(lines, "You spared them."));
    CHECK(!said(lines, "Only ashes here."));                 // unless_flag: mercy is set

    AdventureSession plain;                                    // the same map with no memory
    CHECK(!plain.start(f.path("maps/b.json"), "data", "", 1, f.path("maps/b.triggers.json"), CampaignState{}));
    lines = plain.scenario().drainLines();
    CHECK(!said(lines, "You spared them."));
    CHECK(said(lines, "Only ashes here."));
}

SUITE("Flags — when.flag holds a trigger until the flag is set, then it fires") {
    Fixture f;
    CampaignState carry;
    carry.flags["mercy"] = true;
    AdventureSession s;
    CHECK(!s.start(f.path("maps/b.json"), "data", "", 1, f.path("maps/b.triggers.json"), carry));
    s.scenario().drainLines();
    s.endDay();
    CHECK(!said(s.scenario().drainLines(), "The debt is called in."));   // debt not set: held, not spent
    s.scenario().run(s, Json::parse(R"([{"flag": {"debt": 1}}])"));
    s.endDay();
    CHECK(said(s.scenario().drainLines(), "The debt is called in."));
    CHECK(Scenario::flagsMatch(Json{{"a", 0}}, "a") == false);          // 0 is not truthy
    CHECK(Scenario::flagsMatch(Json{{"a", 0}}, Json{{"a", 0}}));         // but it can be matched
}

SUITE("Flags — flags and the cap stats survive a save; the save is version 2") {
    Fixture f;
    CampaignState carry;
    carry.levelCap = 2;
    AdventureSession a;
    CHECK(!a.start(f.path("maps/a.json"), "data", "", 1, f.path("maps/a.triggers.json"), carry));
    CHECK(!a.scenario().choose(a, "spare"));
    a.grantXp(AdventureSession::xpForLevel(2) + 40);
    Json save = a.saveState();
    CHECK_EQ(save.at("version").get<int>(), 2);
    AdventureSession b;
    CHECK(!b.loadState(save));
    CHECK(b.flags().at("choice.oath") == "spare");
    CHECK_EQ(b.levelCap(), 2);
    CHECK_EQ(b.xpLostToCap(), 40);
    CHECK_EQ(b.capReachedDay(), 1);
    CHECK(!b.scenario().awaitingChoice());
}

SUITE("Flags — a version 1 save (no flags, no cap) still loads") {
    Fixture f;
    AdventureSession a;
    CHECK(!a.start(f.path("maps/a.json"), "data", "", 1, f.path("maps/a.triggers.json")));
    a.grantXp(120);
    Json save = a.saveState();
    save["version"] = 1;
    save.erase("flags");
    save.erase("level_cap");
    AdventureSession b;
    CHECK(!b.loadState(save));
    CHECK(b.flags().empty());
    CHECK_EQ(b.levelCap(), AdventureSession::MAX_HERO_LEVEL);
    CHECK_EQ(b.heroProgress().xp, 120);
    save["version"] = 3;
    CHECK(b.loadState(save).has_value());
}

// ── Level cap ─────────────────────────────────────────────────────────────────

SUITE("Level cap — stops levelling, loses the excess, records the day; a higher cap resumes") {
    Fixture f;
    CampaignState carry;
    carry.levelCap = 3;
    AdventureSession a;
    CHECK(!a.start(smallMap(), "data", "", 1, "", carry));
    CHECK_EQ(a.levelCap(), 3);
    CHECK_EQ(a.capReachedDay(), -1);
    a.grantXp(AdventureSession::xpForLevel(2));
    a.endDay();
    a.grantXp(AdventureSession::xpForLevel(3) - AdventureSession::xpForLevel(2) + 50);   // day 2: reaches 3, 50 over
    CHECK_EQ(a.heroProgress().level, 3);
    CHECK_EQ(a.heroProgress().xp, AdventureSession::xpForLevel(3));
    CHECK_EQ(a.xpLostToCap(), 50);
    CHECK_EQ(a.capReachedDay(), 2);
    a.endDay();
    a.grantXp(AdventureSession::xpForLevel(6));                                          // far past the cap
    CHECK_EQ(a.heroProgress().level, 3);
    CHECK_EQ(a.xpLostToCap(), 50 + AdventureSession::xpForLevel(6));
    CHECK_EQ(a.capReachedDay(), 2);                                                      // first day only

    CampaignState out = a.carryOut();
    CHECK_EQ(out.lastMap.at("cap_reached_day").get<int>(), 2);
    CHECK_EQ(out.lastMap.at("xp_lost_to_cap").get<int>(), 50 + AdventureSession::xpForLevel(6));
    out.levelCap = 5;
    AdventureSession b;
    CHECK(!b.start(smallMap(), "data", "", 1, "", out));
    CHECK_EQ(b.heroProgress().level, 3);
    CHECK_EQ(b.xpLostToCap(), 0);                               // stats are per map
    CHECK_EQ(b.capReachedDay(), -1);
    b.grantXp(AdventureSession::xpForLevel(4) - AdventureSession::xpForLevel(3));
    CHECK_EQ(b.heroProgress().level, 4);                        // levelling resumes from the carried level
}

// ── Campaign sequence ─────────────────────────────────────────────────────────

SUITE("Campaign — loads chapters; the ending's next map advances it") {
    Fixture f;
    Campaign camp;
    CHECK(!camp.load(f.path("campaign.json")));
    CHECK(camp.dataDir() == f.dir);
    CHECK(camp.current()->id == "1.0");
    CHECK_EQ(camp.current()->maxLevel, 3);
    CHECK_EQ(camp.startState().levelCap, 3);
    const auto* e = camp.current();
    AdventureSession s;
    CHECK(!s.start(camp.resolve(e->map), "data", camp.resolve(e->encounters), 1, camp.resolve(e->triggers),
                   camp.startState()));
    CHECK_EQ(s.levelCap(), 3);
    CHECK(!s.scenario().choose(s, "spare"));
    CHECK(s.nextMap().empty());
    s.scenario().run(s, Json::parse(R"([{"ending": {"id": "a_end", "heading": "Dawn", "next": "1.2"}}])"));
    CHECK(s.won());                                            // ending still completes the map
    CHECK(s.nextMap() == "1.2");
    camp.finishMap(s.carryOut());
    auto step = camp.advance(s.nextMap());
    CHECK(!step.error && !step.finished && !step.newChapter);
    CHECK(camp.current()->id == "1.2");                        // 1.1 skipped
    CHECK(camp.state().carried);
    CHECK(camp.state().flags.at("choice.oath") == "spare");
    CHECK_EQ((int)camp.state().history.size(), 1);
    CHECK(camp.state().history[0].at("map") == "1.0");
    CHECK(camp.state().map == "1.2");
}

SUITE("Campaign — chapter rollover, end of campaign, unknown map") {
    Fixture f;
    Campaign camp;
    CHECK(!camp.load(f.path("campaign.json")));
    CHECK(camp.advance().newChapter == false);
    CHECK(camp.current()->id == "1.1");
    camp.advance();
    CHECK(camp.current()->id == "1.2");
    CHECK_EQ(camp.current()->maxLevel, AdventureSession::MAX_HERO_LEVEL);   // max_level absent
    auto step = camp.advance();
    CHECK(step.newChapter);
    CHECK(camp.currentChapter()->id == "east");
    CHECK(camp.current()->id == "2.0");
    CHECK(camp.advance("nowhere").error.has_value());
    CHECK(camp.current()->id == "2.0");
    step = camp.advance();
    CHECK(step.finished);
    CHECK(camp.finished());
    CHECK(camp.current() == nullptr);
    CHECK(camp.advance().error.has_value());
}

SUITE("Campaign — save and load round trip") {
    Fixture f;
    Campaign camp;
    CHECK(!camp.load(f.path("campaign.json")));
    AdventureSession s;
    CHECK(!s.start(f.path("maps/a.json"), "data", "", 1, f.path("maps/a.triggers.json"), camp.startState()));
    CHECK(!s.scenario().choose(s, "spare"));
    s.joinSpecial("kharim");
    s.grantXp(AdventureSession::xpForLevel(2));
    camp.finishMap(s.carryOut());
    camp.advance();
    CHECK(!camp.saveFile(f.path("campaign.save.json")));

    Campaign back;
    CHECK(!back.loadFile(f.path("campaign.save.json")));
    CHECK(back.current() && back.current()->id == "1.1");
    CHECK(back.currentChapter()->id == "marches");
    CHECK(back.dataDir() == f.dir);
    CHECK(back.state().carried);
    CHECK(back.state().flags.at("choice.oath") == "spare");
    CHECK(back.state().flags.at("mercy") == true);
    CHECK_EQ(back.state().level, 2);
    CHECK_EQ(levelOf(back.state().companions, "kharim"), 2);   // he shared the 100 xp
    CHECK_EQ((int)back.state().history.size(), 1);
    CHECK(back.state().toJson() == camp.state().toJson());
    CHECK(back.loadFile(f.path("missing.json")).has_value());
}
