// test_tactics.cpp — the commander's Tactics branch (opening orders), the
// guardian's opportunity strike, the commander's tree, and Maerwen Hale.
#include "test_runner.h"
#include "adventure/AdventureSession.h"
#include "combat/CombatEngine.h"
#include <deque>

namespace {

std::deque<UnitType> s_types;

const UnitType* type(const std::string& name, int speed, int dmg, int hp, int moveRange = 3,
                     std::vector<std::string> abilities = {}) {
    UnitType t;
    t.id = t.name = name;
    t.speed = speed; t.minDamage = t.maxDamage = dmg; t.hitPoints = hp;
    t.attack = 5; t.defense = 5; t.moveRange = moveRange;
    t.abilities = std::move(abilities);
    s_types.push_back(std::move(t));
    return &s_types.back();
}

WorldMap plainMap() {
    WorldMap map;
    map.clear(6);
    for (const auto& [c, t] : map) map.setTile(c, makeTile(Terrain::Grass));
    map.placeObject({{-4, 0}, ObjType::Town, "Home", 1});
    return map;
}

} // namespace

// ── Opening orders ────────────────────────────────────────────────────────────

SUITE("Tactics — chosen stacks open the battle ahead of faster enemies, in the order set") {
    const UnitType* slow = type("Slowpoke", 2, 5, 10);
    const UnitType* fast = type("Sprinter", 9, 5, 10);
    CombatArmy p; p.isPlayer = true;
    p.stacks.push_back(CombatUnit::make(slow, 5, true));
    p.stacks.push_back(CombatUnit::make(slow, 5, true));
    p.stacks.push_back(CombatUnit::make(slow, 5, true));
    CombatArmy e; e.isPlayer = false;
    e.stacks.push_back(CombatUnit::make(fast, 5, false));
    CombatEngine eng(std::move(p), std::move(e));
    CHECK(!eng.currentTurn().isPlayer);                       // the sprinter would go first
    CHECK(!eng.setOpeningOrder({2, 0, 1}, 2));                // more than the rank allows
    CHECK(!eng.setOpeningOrder({1, 1}, 2));                   // the same stack twice
    CHECK(!eng.setOpeningOrder({7}, 2));                      // no such stack
    CHECK(eng.setOpeningOrder({2, 0}, 2));
    const auto& q = eng.turnOrder();
    CHECK(q[0].isPlayer && q[0].stackIndex == 2);
    CHECK(q[1].isPlayer && q[1].stackIndex == 0);
    CHECK(!q[2].isPlayer);                                    // then initiative as usual
    CHECK_EQ((int)q.size(), 4);
    eng.doDefend();
    CHECK(!eng.setOpeningOrder({1}, 2));                      // too late: the battle is under way
}

SUITE("Tactics — an empty order changes nothing") {
    const UnitType* slow = type("Slowpoke2", 2, 5, 10);
    const UnitType* fast = type("Sprinter2", 9, 5, 10);
    CombatArmy p; p.isPlayer = true;
    p.stacks.push_back(CombatUnit::make(slow, 5, true));
    CombatArmy e; e.isPlayer = false;
    e.stacks.push_back(CombatUnit::make(fast, 5, false));
    CombatEngine eng(std::move(p), std::move(e));
    CHECK(eng.setOpeningOrder({}, 1));
    CHECK(!eng.currentTurn().isPlayer);
}

// ── Opportunity strike ────────────────────────────────────────────────────────

SUITE("Opportunity — stepping out of a guardian's reach draws a blow first; the forecast warns") {
    const UnitType* guardian = type("Guardian", 1, 7, 100, 3, {"opportunity_strike"});
    const UnitType* raider   = type("Raider", 9, 3, 10, 4);
    CombatArmy p; p.isPlayer = true;
    p.stacks.push_back(CombatUnit::make(guardian, 1, true));
    CombatArmy e; e.isPlayer = false;
    e.stacks.push_back(CombatUnit::make(raider, 5, false));   // 50 hp
    CombatEngine eng(std::move(p), std::move(e));
    eng.setSeed(3);
    const HexCoord g = CombatMap::toHex(4, 2);
    eng.teleportUnit(true, 0, g);
    eng.teleportUnit(false, 0, g.neighbor(0));
    CHECK(!eng.currentTurn().isPlayer);                       // the raider moves first
    const HexCoord away = g.neighbor(0).neighbor(0);          // two hexes from the guardian
    const HexCoord along = g.neighbor(1);                     // still next to her
    CHECK(eng.opportunityStrikers(along).empty());
    auto warn = eng.reactionsTo(away);
    CHECK_EQ((int)warn.size(), 1);
    if (!warn.empty()) { CHECK(warn[0].opportunity); CHECK_EQ(warn[0].damage.min, 7); }
    eng.drainEvents();
    eng.doMove(away);
    CHECK_EQ(eng.enemyArmy().stacks[0].totalHp(), 43);        // struck for 7 on the way out
    CHECK(eng.enemyArmy().stacks[0].pos == away);
    CHECK_EQ(eng.playerArmy().stacks[0].totalHp(), 100);      // no retaliation
    bool flagged = false;
    for (const auto& ev : eng.drainEvents()) flagged = flagged || ev.isOpportunity;
    CHECK(flagged);
    CHECK(eng.playerArmy().stacks[0].hasReacted);             // once per round
}

SUITE("Opportunity — a stack the blow kills never gets away; staying close is safe") {
    const UnitType* guardian = type("Guardian2", 1, 50, 100, 3, {"opportunity_strike"});
    const UnitType* raider   = type("Raider2", 9, 3, 10, 4);
    for (bool leave : {true, false}) {
        CombatArmy p; p.isPlayer = true;
        p.stacks.push_back(CombatUnit::make(guardian, 1, true));
        CombatArmy e; e.isPlayer = false;
        e.stacks.push_back(CombatUnit::make(raider, 2, false));   // 20 hp: one blow kills
        e.stacks.push_back(CombatUnit::make(raider, 2, false));
        CombatEngine eng(std::move(p), std::move(e));
        const HexCoord g = CombatMap::toHex(4, 2);
        eng.teleportUnit(true, 0, g);
        eng.teleportUnit(false, 0, g.neighbor(0));
        eng.teleportUnit(false, 1, CombatMap::toHex(9, 0));
        const HexCoord start = g.neighbor(0);
        eng.doMove(leave ? start.neighbor(0) : g.neighbor(1));
        CHECK_EQ(eng.enemyArmy().stacks[0].isDead(), leave);
        if (leave) CHECK(eng.enemyArmy().stacks[0].pos == start);   // cut down mid-step
    }
}

// ── The commander's tree ──────────────────────────────────────────────────────

SUITE("Tree — points buy Tactics in order; other branches wait; the rank drives opening orders") {
    AdventureSession s;
    CHECK(!s.start(plainMap(), "data"));
    CHECK_EQ((int)s.heroTree().size(), 4);
    CHECK_EQ(s.tacticsRank(), 0);
    CHECK(s.learnBlocker("first_orders").find("points") != std::string::npos);
    s.grantXp(AdventureSession::xpForLevel(4));           // three levels: three points
    CHECK_EQ(s.heroProgress().points, 3);
    CHECK(s.learn("vanguard").has_value());                    // First Orders first
    CHECK(s.learn("banner").has_value());                      // Command is not in the demo yet
    CHECK(!s.learn("first_orders"));
    CHECK(s.learn("first_orders").has_value());                // already learned
    CHECK(!s.learn("vanguard"));
    CHECK_EQ(s.tacticsRank(), 2);
    CHECK_EQ(s.heroProgress().points, 1);
}

SUITE("Tree — learned skills survive a save") {
    AdventureSession a;
    CHECK(!a.start("data/maps/old_passage.json", "data", "data/maps/old_passage.encounters.json", 1,
                   "data/maps/old_passage.triggers.json"));
    a.grantXp(AdventureSession::xpForLevel(2));
    CHECK(!a.learn("first_orders"));
    AdventureSession b;
    CHECK(!b.loadState(Scenario::Json::parse(a.saveState().dump())));
    CHECK(b.learned("first_orders"));
    CHECK_EQ(b.tacticsRank(), 1);
    CHECK_EQ(b.heroProgress().points, 0);
}

// ── Maerwen Hale ──────────────────────────────────────────────────────────────

SUITE("Maerwen — joins as a companion who fights with the opportunity strike") {
    AdventureSession s;
    CHECK(!s.start(plainMap(), "data"));
    s.joinSpecial("maerwen");
    bool found = false;
    for (const auto& c : s.battleCompanions()) found = found || c.id == "maerwen";
    CHECK(found);
    const UnitType* u = s.resources().unit("maerwen");
    CHECK(u != nullptr);
    if (u) { CHECK(u->isCompanion()); CHECK(CombatEngine::hasOpportunityStrike(CombatUnit::companion(u, 1, true))); }
}
