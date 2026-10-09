// test_combat_tips.cpp — companion tips in battle (data/combat_tips.json) are
// decided in C++: the when-keys, the map gate, the speaker who must be fighting
// (a wounded companion stays home and never speaks), once per campaign, one per
// turn; the registry refuses a key the engine cannot evaluate.
#include "test_runner.h"
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
    CHECK(registry().combatTips().size() == 8);
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
