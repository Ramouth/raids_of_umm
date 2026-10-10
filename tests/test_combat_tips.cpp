// test_combat_tips.cpp — companion tips in battle (data/combat_tips.json) are
// decided in C++: the when-keys, the map gate, the speaker who must be fighting
// (a wounded companion stays home and never speaks), once per campaign, one per
// turn; the registry refuses a key the engine cannot evaluate. Banter: replies
// (a fallen speaker's line is dropped), "repeat": "battle", the rushes and
// obstacle_broken keys with their actor / kind filters; reckless companions.
#include "test_runner.h"
#include <algorithm>
#include "adventure/AdventureSession.h"
#include "combat/CombatAI.h"
#include "combat/CombatTips.h"
#include <filesystem>
#include <fstream>

namespace {

using Json = nlohmann::json;
namespace fs = std::filesystem;

const ResourceManager& registry() {
    static ResourceManager rm;
    static bool loaded = false;
    if (!loaded) { CHECK(!rm.load("data")); loaded = true; }
    return rm;
}

// Two tips of our own, so the test does not depend on the story's wording.
const std::vector<CombatTip>& testTips() {
    static const std::vector<CombatTip> tips = {
        {"t_start", "battle_start", "Drest", "First blood.", {"1.0"}},
        {"t_defend", "defend_available", "drest", "Defend.", {"1.0"}},
        {"t_win", "victory_first", "Drest", "Won.", {"1.0", "1.1"}},
    };
    return tips;
}

CombatArmy army(bool player, std::vector<std::pair<std::string, int>> stacks, bool withDrest) {
    CombatArmy a;
    a.isPlayer = player;
    for (const auto& [id, n] : stacks) a.stacks.push_back(CombatUnit::make(registry().unit(id), n, player));
    if (withDrest) a.stacks.push_back(CombatUnit::companion(registry().unit("drest"), 1, player));
    return a;
}

} // namespace

SUITE("Combat tips — the registry loads data/combat_tips.json and refuses unknown when-keys") {
    CHECK(registry().combatTips().size() >= 8);   // the story adds banter; the shape is what matters
    for (const auto& tip : registry().combatTips()) {
        CHECK(!tip.id.empty() && !tip.text.empty());
        CHECK(std::find(combatTipWhenKeys().begin(), combatTipWhenKeys().end(), tip.when) != combatTipWhenKeys().end());
    }
    const fs::path dir = fs::temp_directory_path() / "umm_tips_bad";
    fs::create_directories(dir);
    for (const auto& entry : fs::directory_iterator("data"))
        if (entry.is_regular_file())
            fs::copy_file(entry.path(), dir / entry.path().filename(), fs::copy_options::overwrite_existing);
    std::ofstream(dir / "combat_tips.json") << R"({"tips": [{"id": "x", "when": "moon_rises", "speaker": "Drest", "text": "?"}]})";
    ResourceManager bad;
    auto err = bad.load(dir.string());
    CHECK(err.has_value() && err->find("moon_rises") != std::string::npos);
}

SUITE("Combat tips — first due tip in file order, one per turn, never twice") {
    CombatEngine eng(army(true, {{"woad_runner", 10}}, true), army(false, {{"grey_wolf", 3}}, false), 0, 3u);
    CombatTipWatch watch(&testTips(), "1.0", {}, eng);
    while (!eng.isOver() && !eng.currentTurn().isPlayer) CombatAI::takeTurn(eng);
    const CombatTip* first = watch.check(eng);
    CHECK(first && first->id == "t_start");
    CHECK(watch.check(eng) == nullptr);                    // one per turn
    std::vector<std::string> seen = watch.shownHere();
    for (int n = 0; n < 400 && !eng.isOver(); ++n) {
        CombatAI::takeTurn(eng);
        if (const CombatTip* t = watch.check(eng)) seen.push_back(t->id);
    }
    CHECK(eng.result() == CombatResult::PlayerWon);
    CHECK(watch.shownHere() == (std::vector<std::string>{"t_start", "t_defend", "t_win"}));
    CHECK(std::count(watch.shownHere().begin(), watch.shownHere().end(), "t_start") == 1);
}

SUITE("Combat tips — no speaker in the battle (wounded, absent), a map not listed, or shown before: silent") {
    CombatEngine without(army(true, {{"woad_runner", 10}}, false), army(false, {{"grey_wolf", 3}}, false), 0, 3u);
    CombatTipWatch absent(&testTips(), "1.0", {}, without);
    for (int n = 0; n < 400 && !without.isOver(); ++n) {
        CHECK(absent.check(without) == nullptr);
        CombatAI::takeTurn(without);
    }
    CHECK(absent.check(without) == nullptr && absent.shownHere().empty());

    CombatEngine elsewhere(army(true, {{"woad_runner", 10}}, true), army(false, {{"grey_wolf", 3}}, false), 0, 3u);
    CombatTipWatch other(&testTips(), "2.0", {}, elsewhere);
    CHECK(other.check(elsewhere) == nullptr);
    CombatTipWatch outside(&testTips(), "", {}, elsewhere);  // a map outside a campaign: no tips
    CHECK(outside.check(elsewhere) == nullptr);
    CombatTipWatch seen(&testTips(), "1.0", {"t_start", "t_defend"}, elsewhere);
    while (!elsewhere.isOver() && !elsewhere.currentTurn().isPlayer) CombatAI::takeTurn(elsewhere);
    CHECK(seen.check(elsewhere) == nullptr);              // only the victory tip is left

    // The real AdventureSession never fields a wounded companion, so its tips never come.
    CHECK(!CombatTipWatch::speakerFighting(without, "Drest"));
    CHECK(CombatTipWatch::speakerFighting(elsewhere, "DREST") && CombatTipWatch::speakerFighting(elsewhere, "drest"));
}

SUITE("Combat tips — conditions: adjacent, flanked, low stack, shooter, companion hit") {
    CombatEngine eng(army(true, {{"cruth_slinger", 8}, {"woad_runner", 20}}, true),
                     army(false, {{"grey_wolf", 2}, {"grey_wolf", 2}}, false), 0, 11u);
    CombatTipWatch watch(&testTips(), "1.0", {}, eng);
    const HexCoord mid = CombatMap::toHex(5, 2);
    eng.teleportUnit(true, 1, mid);
    eng.teleportUnit(false, 0, mid.neighbor(0));
    eng.teleportUnit(false, 1, mid.neighbor(3));
    auto holds = watch.conditions(eng);
    CHECK(holds.count("enemy_adjacent") && holds.count("flanked") && holds.count("battle_start"));
    CHECK(!holds.count("low_stack") && !holds.count("companion_hit") && !holds.count("victory_first"));
    eng.teleportUnit(false, 1, mid.neighbor(1));          // not opposite: adjacent, not pinned
    holds = watch.conditions(eng);
    CHECK(holds.count("enemy_adjacent") && !holds.count("flanked"));

    // Played out: low_stack, companion_hit and the shooter's turn follow the units.
    bool sawShooter = false;
    for (int n = 0; n < 400 && !eng.isOver(); ++n) {
        const auto& own = eng.playerArmy().stacks;
        holds = watch.conditions(eng);
        const auto& active = eng.activeUnit();
        const bool shooterTurn = eng.currentTurn().isPlayer && !active.mad && active.type->isRanged()
                                 && active.shotsLeft > 0 && !eng.isEngaged(active);
        CHECK_EQ(holds.count("first_turn_shooter") == 1, shooterTurn);
        sawShooter |= shooterTurn;
        CHECK_EQ(holds.count("low_stack") == 1, own[1].count > 0 && own[1].count * 4 < 20);
        CHECK_EQ(holds.count("companion_hit") == 1, own[2].totalHp() < own[2].maxHp());
        CombatAI::takeTurn(eng);
    }
    CHECK(sawShooter);
}

namespace {

// A party: troops, then companions by id; `reckless` adds the "reckless" ability to Aldren.
const UnitType* aldrenType(bool reckless) {
    // Either way regardless of what units.json says about him today.
    static UnitType wild = [] {
        UnitType t = *registry().unit("aldren");
        if (!t.isReckless()) t.abilities.push_back("reckless");
        return t;
    }();
    static UnitType plain = [] {
        UnitType t = *registry().unit("aldren");
        t.abilities.erase(std::remove(t.abilities.begin(), t.abilities.end(), "reckless"), t.abilities.end());
        return t;
    }();
    return reckless ? &wild : &plain;
}

CombatArmy party(std::vector<std::pair<std::string, int>> troops, std::vector<std::string> companions, bool reckless = false) {
    CombatArmy a = army(true, troops, false);
    for (const auto& id : companions)
        a.stacks.push_back(CombatUnit::companion(id == "aldren" ? aldrenType(reckless) : registry().unit(id), 1, true));
    return a;
}

// Defend through every turn until the player's stack `index` is the one to act.
void untilTurnOf(CombatEngine& eng, int index) {
    for (int n = 0; n < 50 && !eng.isOver() && !(eng.currentTurn().isPlayer && eng.currentTurn().stackIndex == index); ++n)
        eng.doDefend();
}

// The free hex next to `foe` that `eng`'s active stack can walk to, nearest it.
HexCoord besideReachable(const CombatEngine& eng, HexCoord foe) {
    HexCoord best = eng.activeUnit().pos;
    int bestDist = 1000;
    for (int dir = 0; dir < 6; ++dir) {
        const HexCoord h = foe.neighbor(dir);
        const int d = h.distanceTo(eng.activeUnit().pos);
        if (eng.canMoveTo(h) && d < bestDist) { best = h; bestDist = d; }
    }
    return best;
}

CombatLayout fieldWith(std::vector<std::tuple<int, int, int>> trees) {
    CombatLayout l = CombatLayout::field();
    l.id = "test";
    for (const auto& [c, r, hp] : trees) l.obstacles.push_back({CombatMap::toHex(c, r), "tree", hp, true});
    return l;
}

} // namespace

SUITE("Combat tips — the registry reads reply, repeat, actor and kind, and refuses bad ones") {
    const fs::path dir = fs::temp_directory_path() / "umm_tips_banter";
    fs::create_directories(dir);
    for (const auto& entry : fs::directory_iterator("data"))
        if (entry.is_regular_file())
            fs::copy_file(entry.path(), dir / entry.path().filename(), fs::copy_options::overwrite_existing);
    std::ofstream(dir / "combat_tips.json") << R"({"tips": [{"id": "x", "when": "rushes", "speaker": "Drest", "text": "Back!",
        "actor": "aldren", "repeat": "battle", "maps": ["1.0"], "reply": [{"speaker": "Aldren", "text": "Keep up."}]},
        {"id": "y", "when": "obstacle_broken", "kind": "tree", "speaker": "Aldren", "text": "Through."}]})";
    ResourceManager good;
    CHECK(!good.load(dir.string()));
    const auto& tips = good.combatTips();
    CHECK(tips.size() == 2);
    if (tips.size() == 2) {
        CHECK(tips[0].perBattle && tips[0].actor == "aldren" && tips[0].reply.size() == 1);
        CHECK(tips[0].reply.size() == 1 && tips[0].reply[0].speaker == "Aldren" && tips[0].reply[0].text == "Keep up.");
        CHECK(!tips[1].perBattle && tips[1].kind == "tree" && tips[1].reply.empty());
    }
    std::ofstream(dir / "combat_tips.json") << R"({"tips": [{"id": "x", "when": "rushes", "speaker": "Drest", "text": "?", "repeat": "always"}]})";
    ResourceManager badRepeat;
    auto err = badRepeat.load(dir.string());
    CHECK(err && err->find("repeat") != std::string::npos);
    std::ofstream(dir / "combat_tips.json") << R"({"tips": [{"id": "x", "when": "rushes", "speaker": "Drest", "text": "?",
        "reply": [{"speaker": "A", "text": "1"}, {"speaker": "B", "text": "2"}, {"speaker": "C", "text": "3"}]}]})";
    ResourceManager tooMany;
    err = tooMany.load(dir.string());
    CHECK(err && err->find("reply") != std::string::npos);
}

SUITE("Combat tips — replies follow the line; a fallen or absent speaker's reply is dropped") {
    static const std::vector<CombatTip> tips = {
        {"t_ex", "battle_start", "Drest", "Stay back.", {"1.0"}, {{"Aldren", "Keep up."}, {"Kharim", "Not here."}}},
    };
    CombatEngine eng(party({{"woad_runner", 10}}, {"drest", "aldren"}), army(false, {{"grey_wolf", 3}}, false), 0, 3u);
    CombatTipWatch watch(&tips, "1.0", {}, eng);
    CHECK(watch.check(eng) != nullptr);
    CHECK(watch.lines().size() == 2);   // Kharim is not in this battle
    if (watch.lines().size() == 2) {
        CHECK(watch.lines()[0].speaker == "Drest" && watch.lines()[0].text == "Stay back.");
        CHECK(watch.lines()[1].speaker == "Aldren" && watch.lines()[1].text == "Keep up.");
    }
    CombatEngine fallen(party({{"woad_runner", 10}}, {"drest", "aldren"}), army(false, {{"grey_wolf", 3}}, false), 0, 3u);
    const_cast<CombatUnit&>(fallen.playerArmy().stacks[2]).count = 0;   // Aldren has fallen
    CombatTipWatch after(&tips, "1.0", {}, fallen);
    CHECK(after.check(fallen) != nullptr);   // Drest still speaks
    CHECK(after.lines().size() == 1 && after.lines()[0].speaker == "Drest");
}

SUITE("Combat tips — repeat: battle comes again in a later battle, once per battle; the default once per campaign") {
    static const std::vector<CombatTip> tips = [] {
        std::vector<CombatTip> t = {{"t_once", "battle_start", "Drest", "Once.", {"1.0"}},
                                    {"t_banter", "battle_start", "Drest", "Again.", {"1.0"}}};
        t[1].perBattle = true;
        return t;
    }();
    auto fresh = [] { return CombatEngine(party({{"woad_runner", 10}}, {"drest"}), army(false, {{"grey_wolf", 3}}, false), 0, 3u); };
    CombatEngine first = fresh();
    CombatTipWatch watch(&tips, "1.0", {}, first);
    const CombatTip* a = watch.check(first);
    CHECK(a && a->id == "t_once");
    first.doDefend();
    const CombatTip* b = watch.check(first);
    CHECK(b && b->id == "t_banter");
    for (int n = 0; n < 4 && first.roundNumber() == 1; ++n) {   // still round one: neither comes again
        first.doDefend();
        CHECK(watch.check(first) == nullptr);
    }
    // A later battle of the same campaign (both ids recorded as shown): only the banter.
    CombatEngine second = fresh();
    CombatTipWatch later(&tips, "1.0", {"t_once", "t_banter"}, second);
    const CombatTip* c = later.check(second);
    CHECK(c && c->id == "t_banter");
    second.doDefend();
    CHECK(later.check(second) == nullptr);
}

SUITE("Combat tips — rushes: a player stack walks next to an enemy with no friend within two hexes; actor filter") {
    // woad runners, Drest, Aldren vs one slow spearman far off.
    CombatEngine eng(party({{"woad_runner", 10}}, {"drest", "aldren"}), army(false, {{"levy_spearman", 4}}, false), 0, 7u);
    static const std::vector<CombatTip> tips = [] {
        std::vector<CombatTip> t = {{"t_runner", "rushes", "Drest", "Not the runners.", {"1.0", "1.1"}},
                                    {"t_aldren", "rushes", "Drest", "Stay back behind the lines!", {"1.0"}, {{"Aldren", "Then keep up, old man."}}},
                                    {"t_any", "rushes", "Drest", "Your brother would have done that. Don't.", {"1.1"}}};
        t[0].actor = "woad_runner";
        t[1].actor = "Aldren";   // a name, any case, works as well as the id
        return t;
    }();
    CombatTipWatch watch(&tips, "1.0", {}, eng);
    CombatTipWatch elsewhere(&tips, "1.1", {}, eng);
    eng.teleportUnit(true, 0, CombatMap::toHex(0, 2));
    eng.teleportUnit(true, 1, CombatMap::toHex(0, 1));
    eng.teleportUnit(true, 2, CombatMap::toHex(1, 2));
    eng.teleportUnit(false, 0, CombatMap::toHex(6, 2));
    CHECK(!watch.conditions(eng).count("rushes"));          // nobody has acted yet
    untilTurnOf(eng, 2);
    const HexCoord spot = besideReachable(eng, eng.enemyArmy().stacks[0].pos);
    CHECK(spot != eng.activeUnit().pos);
    eng.doMove(spot);
    CHECK(eng.lastTurn().isPlayer && eng.lastTurn().stackIndex == 2 && eng.lastTurn().moved);
    CHECK(watch.conditions(eng).count("rushes"));
    const CombatTip* said = watch.check(eng);
    CHECK(said && said->id == "t_aldren");                   // the runner's tip waits for its own actor
    CHECK(watch.lines().size() == 2 && watch.lines()[1].text == "Then keep up, old man.");
    const CombatTip* other = elsewhere.check(eng);
    CHECK(other && other->id == "t_any");                     // no actor: any player stack
    // A friend within two hexes: not alone, not rushing.
    HexCoord near = spot;
    for (const HexCoord& h : eng.map().allHexes())
        if (h.distanceTo(spot) == 2 && h.distanceTo(eng.enemyArmy().stacks[0].pos) > 1) { near = h; break; }
    CHECK(near != spot);
    eng.teleportUnit(true, 0, near);
    CHECK(!watch.conditions(eng).count("rushes"));
    eng.teleportUnit(true, 0, CombatMap::toHex(0, 2));
    CHECK(watch.conditions(eng).count("rushes"));
    // Its next turn without walking (it holds next to the enemy): no rush.
    untilTurnOf(eng, 2);
    CHECK(!eng.isOver());
    eng.doDefend();
    CHECK(!eng.lastTurn().moved && !watch.conditions(eng).count("rushes"));
    // An enemy's turn just played: no rush.
    while (!eng.isOver() && eng.currentTurn().isPlayer) eng.doDefend();
    eng.doDefend();
    CHECK(!eng.lastTurn().isPlayer && !watch.conditions(eng).count("rushes"));
}

SUITE("Combat tips — obstacle_broken: a player stack destroys an obstacle; kind and actor filters") {
    CombatEngine eng(party({{"woad_runner", 10}}, {"drest", "aldren"}), army(false, {{"levy_spearman", 4}}, false), 0, 7u,
                     fieldWith({{3, 2, 1}, {3, 0, 900}}));
    static const std::vector<CombatTip> tips = [] {
        std::vector<CombatTip> t = {{"t_wall", "obstacle_broken", "Aldren", "A wall.", {"1.0"}},
                                    {"t_drest", "obstacle_broken", "Drest", "Drest's axe.", {"1.0"}},
                                    {"t_tree", "obstacle_broken", "Aldren", "Through, not round.", {"1.0"}}};
        t[0].kind = "barricade";
        t[1].actor = "drest";
        t[2].kind = "tree";
        t[2].actor = "aldren";
        return t;
    }();
    CombatTipWatch watch(&tips, "1.0", {}, eng);
    eng.teleportUnit(true, 2, CombatMap::toHex(2, 2));
    eng.teleportUnit(false, 0, CombatMap::toHex(10, 4));
    // A blow that leaves the obstacle standing breaks nothing.
    untilTurnOf(eng, 2);
    CHECK(eng.doAttackObstacle(CombatMap::toHex(3, 0)));
    CHECK(eng.obstacleAt(CombatMap::toHex(3, 0)) && !watch.conditions(eng).count("obstacle_broken"));
    CHECK(watch.check(eng) == nullptr);
    untilTurnOf(eng, 2);
    CHECK(eng.doAttackObstacle(CombatMap::toHex(3, 2)));
    CHECK(!eng.obstacleAt(CombatMap::toHex(3, 2)));
    CHECK(eng.lastTurn().broken.size() == 1 && eng.lastTurn().broken[0].kind == "tree");
    CHECK(watch.conditions(eng).count("obstacle_broken"));
    const CombatTip* said = watch.check(eng);
    CHECK(said && said->id == "t_tree");                      // not a barricade, not Drest's blow
    eng.doDefend();                                           // the next turn: nothing broken in it
    CHECK(!watch.conditions(eng).count("obstacle_broken"));
}

SUITE("CombatAI — a reckless stack charges the nearest enemy it can reach") {
    CombatEngine eng(party({{"woad_runner", 10}}, {"aldren"}, true),
                     army(false, {{"levy_spearman", 2}, {"levy_spearman", 2}}, false), 0, 5u);
    eng.teleportUnit(true, 1, CombatMap::toHex(2, 2));
    eng.teleportUnit(false, 0, CombatMap::toHex(7, 0));   // farther
    eng.teleportUnit(false, 1, CombatMap::toHex(5, 2));   // nearer
    untilTurnOf(eng, 1);
    eng.drainEvents();
    CombatAI::takeTurn(eng);
    bool struckNearest = false;
    for (const auto& ev : eng.drainEvents())
        struckNearest |= ev.type == CombatEvent::Type::UnitAttacked && ev.isPlayer && ev.stackIndex == 1
                         && !ev.isRetaliation && !ev.targetIsPlayer && ev.targetIndex == 1;
    CHECK(struckNearest);
}

SUITE("CombatAI — a reckless stack hacks through a tree when that is the shorter way; a plain one walks round") {
    // Trees down column 5 except row 4: the way round is long, the tree in front is one blow or three.
    std::vector<std::tuple<int, int, int>> wall;
    for (int row = 0; row < 4; ++row) wall.push_back({5, row, 60});
    auto play = [&](bool reckless) {
        CombatEngine eng(party({{"woad_runner", 10}}, {"aldren"}, reckless), army(false, {{"levy_spearman", 2}}, false),
                         0, 5u, fieldWith(wall));
        eng.teleportUnit(true, 1, CombatMap::toHex(4, 0));
        eng.teleportUnit(false, 0, CombatMap::toHex(6, 0));
        untilTurnOf(eng, 1);
        eng.drainEvents();
        CombatAI::takeTurn(eng);
        bool chopped = false;
        for (const auto& ev : eng.drainEvents())
            chopped |= ev.type == CombatEvent::Type::ObstacleAttacked && ev.isPlayer && ev.stackIndex == 1;
        return chopped;
    };
    CHECK(play(true));
    CHECK(!play(false));
}
