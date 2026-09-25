// test_tactics.cpp — the commander's Tactics branch (opening orders), the
// guardian's opportunity strike, the commander's tree, and Maerwen Hale.
#include "test_runner.h"
#include "adventure/AdventureSession.h"
#include "combat/CombatEngine.h"
#include <deque>
#include <queue>
#include <unordered_map>

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

// ── The Cruths: lone wolf, renown, strike and return ─────────────────────────

SUITE("Cruths — a lone wolf hits 25% harder with no friend beside it; the forecast agrees") {
    const UnitType* wolf  = type("PaintedOne", 9, 8, 100, 3, {"lone_wolf"});
    const UnitType* pal   = type("Pal", 1, 1, 100);
    const UnitType* dummy = type("Dummy", 1, 1, 1000);
    for (bool alone : {true, false}) {
        CombatArmy p; p.isPlayer = true;
        p.stacks.push_back(CombatUnit::make(wolf, 1, true));
        p.stacks.push_back(CombatUnit::make(pal, 1, true));
        CombatArmy e; e.isPlayer = false;
        e.stacks.push_back(CombatUnit::make(dummy, 1, false));
        CombatEngine eng(std::move(p), std::move(e));
        const HexCoord at = CombatMap::toHex(4, 2);
        eng.teleportUnit(true, 0, at);
        eng.teleportUnit(false, 0, at.neighbor(0));
        eng.teleportUnit(true, 1, alone ? CombatMap::toHex(0, 0) : at.neighbor(3));
        CHECK_EQ(eng.previewAttack(0).damage.min, alone ? 10 : 8);
        eng.doAttack(0);
        CHECK_EQ(eng.enemyArmy().stacks[0].totalHp(), 1000 - (alone ? 10 : 8));
    }
}

SUITE("Cruths — breaking a stack paints renown: +2 attack (great renown +4) for the battle") {
    const UnitType* blade  = type("Blade", 9, 50, 100, 3, {"renown"});
    const UnitType* chosen = type("Chosen", 9, 50, 100, 3, {"great_renown"});
    const UnitType* victim = type("Victim", 1, 1, 10);
    for (const UnitType* t : {blade, chosen}) {
        CombatArmy p; p.isPlayer = true;
        p.stacks.push_back(CombatUnit::make(t, 1, true));
        CombatArmy e; e.isPlayer = false;
        e.stacks.push_back(CombatUnit::make(victim, 1, false));
        e.stacks.push_back(CombatUnit::make(victim, 1, false));
        CombatEngine eng(std::move(p), std::move(e));
        const HexCoord at = CombatMap::toHex(4, 2);
        eng.teleportUnit(true, 0, at);
        eng.teleportUnit(false, 0, at.neighbor(0));
        eng.teleportUnit(false, 1, CombatMap::toHex(10, 0));
        const int before = eng.playerArmy().stacks[0].effectiveAttack();
        eng.doAttack(0);
        CHECK(eng.enemyArmy().stacks[0].isDead());
        CHECK_EQ(eng.playerArmy().stacks[0].effectiveAttack(), before + (t == chosen ? 4 : 2));
    }
}

SUITE("Cruths — strike and return: walk in, strike, go back to where it started") {
    const UnitType* raider = type("Raider3", 9, 5, 100, 5, {"strike_and_return"});
    const UnitType* dummy  = type("Dummy3", 1, 1, 1000);
    CombatArmy p; p.isPlayer = true;
    p.stacks.push_back(CombatUnit::make(raider, 1, true));
    CombatArmy e; e.isPlayer = false;
    e.stacks.push_back(CombatUnit::make(dummy, 1, false));
    CombatEngine eng(std::move(p), std::move(e));
    const HexCoord start = CombatMap::toHex(2, 2);
    const HexCoord foe   = CombatMap::toHex(5, 2);
    eng.teleportUnit(true, 0, start);
    eng.teleportUnit(false, 0, foe);
    CHECK(eng.canAttack(0));
    eng.doAttack(0);                                           // walk up, strike...
    CHECK(eng.enemyArmy().stacks[0].totalHp() < 1000);
    CHECK(eng.playerArmy().stacks[0].pos == start);           // ...and back
    bool back = false;
    for (const auto& ev : eng.drainEvents())
        back = back || (ev.type == CombatEvent::Type::UnitMoved && ev.to == start);
    CHECK(back);
}

// ── Routing around obstacles ──────────────────────────────────────────────────

SUITE("Routing — a walk is routed when it happens: never through a stack that dies later") {
    const UnitType* killer = type("Killer", 9, 500, 100, 5);
    const UnitType* victim = type("Victim4", 1, 1, 10);
    const UnitType* wall   = type("Wall4", 1, 1, 1000);
    CombatArmy p; p.isPlayer = true;
    p.stacks.push_back(CombatUnit::make(killer, 1, true));
    CombatArmy e; e.isPlayer = false;
    e.stacks.push_back(CombatUnit::make(victim, 1, false));
    e.stacks.push_back(CombatUnit::make(wall, 1, false));
    CombatEngine eng(std::move(p), std::move(e));
    const HexCoord start = CombatMap::toHex(2, 2);
    eng.teleportUnit(true, 0, start);
    eng.teleportUnit(false, 0, CombatMap::toHex(3, 2));       // right in the way...
    eng.teleportUnit(false, 1, CombatMap::toHex(10, 0));
    const HexCoord from = CombatMap::toHex(4, 2);              // ...strike it from the far side
    CHECK(eng.doAttackFrom(from, 0));
    CHECK(eng.enemyArmy().stacks[0].isDead());
    for (const auto& ev : eng.drainEvents()) {
        if (ev.type != CombatEvent::Type::UnitMoved) continue;
        CHECK(!ev.path.empty());
        CHECK(ev.path.back() == from);
        for (const auto& h : ev.path) CHECK(h != CombatMap::toHex(3, 2));   // walked round, not through
    }
}

SUITE("Routing — map routes are the cheapest there are (roads at 0.5 included)") {
    WorldMap map;
    CHECK(!map.loadJson("data/maps/old_passage.json"));
    std::vector<HexCoord> cells;
    for (const auto& [c, t] : map) if (t.passable) cells.push_back(c);
    auto cost = [&](const std::vector<HexCoord>& p) {
        float total = 0;
        for (size_t i = 1; i < p.size(); ++i) total += map.tileAt(p[i])->moveCost;
        return total;
    };
    int worse = 0, checked = 0;
    for (size_t i = 0; i < cells.size(); i += 13)
        for (size_t j = 5; j < cells.size(); j += 17) {
            auto route = map.findPathWeighted(cells[i], cells[j], nullptr);
            if (route.size() < 2) continue;
            // Reference: plain Dijkstra over the same tiles.
            std::unordered_map<HexCoord, float> best{{cells[i], 0.0f}};
            using Entry = std::pair<float, HexCoord>;
            auto later = [](const Entry& a, const Entry& b) { return a.first > b.first; };
            std::priority_queue<Entry, std::vector<Entry>, decltype(later)> open(later);
            open.push({0.0f, cells[i]});
            while (!open.empty()) {
                auto [g, at] = open.top();
                open.pop();
                if (g > best[at]) continue;
                for (int d = 0; d < 6; ++d) {
                    HexCoord n = at.neighbor(d);
                    const MapTile* t = map.tileAt(n);
                    if (!t || !t->passable) continue;
                    float ng = g + t->moveCost;
                    if (!best.count(n) || ng < best[n] - 1e-4f) { best[n] = ng; open.push({ng, n}); }
                }
            }
            ++checked;
            if (cost(route) > best[cells[j]] + 1e-3f) ++worse;
        }
    CHECK(checked > 500);
    CHECK_EQ(worse, 0);
}

// ── The commander's path ──────────────────────────────────────────────────────

SUITE("Path — chosen once at level 2; each level then grants the path's skill") {
    AdventureSession s;
    CHECK(!s.start(plainMap(), "data"));
    CHECK_EQ((int)s.heroPaths().size(), 3);
    CHECK(s.wildcardPath().id == "veined");
    CHECK(!s.needsPath());
    CHECK(s.choosePath("marshal").has_value());                // not before level 2
    s.grantXp(AdventureSession::xpForLevel(2));
    auto ups = s.drainLevelUps();
    CHECK_EQ((int)ups.size(), 1);
    if (!ups.empty()) { CHECK_EQ(ups[0].level, 2); CHECK(ups[0].skill.empty()); }   // choose a path
    CHECK(s.needsPath());
    CHECK(s.choosePath("veined").has_value());                 // the wildcard is the story's to offer
    CHECK(!s.choosePath("marshal"));
    CHECK(s.choosePath("siegemaster").has_value());            // for good
    CHECK(s.learned("first_orders"));
    CHECK_EQ(s.tacticsRank(), 1);
    s.grantXp(AdventureSession::xpForLevel(4) - s.heroProgress().xp);
    ups = s.drainLevelUps();
    CHECK_EQ((int)ups.size(), 2);
    if (ups.size() == 2) { CHECK(ups[0].skill == "Rally"); CHECK(ups[1].skill == "Vanguard"); }
    CHECK_EQ(s.tacticsRank(), 2);
    CHECK_EQ(s.armyBonus().attack, 1);
}

SUITE("Path — a late choice catches up on the skills of every level so far") {
    AdventureSession s;
    CHECK(!s.start(plainMap(), "data"));
    s.grantXp(AdventureSession::xpForLevel(5));
    CHECK(!s.choosePath("quartermaster"));
    for (const char* id : {"forced_march", "pathfinders", "requisition", "supply_lines"}) CHECK(s.learned(id));
    CHECK_EQ(s.heroEffects().moves, 2);
    CHECK_EQ(s.heroEffects().gold, 250);
    CHECK(s.dailyIncome()[Resource::Gold] >= 250);
}

SUITE("Path — Siegemaster's skills reach the army; the level is capped at 10") {
    AdventureSession s;
    CHECK(!s.start(plainMap(), "data"));
    s.grantXp(AdventureSession::xpForLevel(3));
    CHECK(!s.choosePath("siegemaster"));
    CHECK_EQ(s.armyBonus().defense, 2);
    CHECK(s.armyBonus().readiedShot);
    s.grantXp(1000000);
    CHECK_EQ(s.heroProgress().level, AdventureSession::MAX_HERO_LEVEL);
}

SUITE("Path — the choice and its skills survive a save") {
    AdventureSession a;
    CHECK(!a.start("data/maps/old_passage.json", "data", "data/maps/old_passage.encounters.json", 1,
                   "data/maps/old_passage.triggers.json"));
    a.grantXp(AdventureSession::xpForLevel(3));
    CHECK(!a.choosePath("marshal"));
    AdventureSession b;
    CHECK(!b.loadState(Scenario::Json::parse(a.saveState().dump())));
    CHECK(b.heroPath() == "marshal");
    CHECK(b.learned("rally"));
    CHECK_EQ(b.tacticsRank(), 1);
    CHECK(!b.needsPath());
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

SUITE("Fieldworks — purchases require skills, cost resources, respect capacity, and survive saves") {
    AdventureSession s;
    CHECK(!s.start("data/maps/old_passage.json", "data"));
    CHECK(s.buyFieldwork("barricade").has_value());
    s.grantXp(AdventureSession::xpForLevel(2));
    CHECK(!s.choosePath("siegemaster"));
    const int gold = s.treasury()[Resource::Gold];
    const int wood = s.treasury()[Resource::Wood];
    CHECK(s.buyFieldwork("stakes").has_value());
    CHECK(!s.buyFieldwork("barricade"));
    CHECK_EQ(s.treasury()[Resource::Gold], gold - 250);
    CHECK_EQ(s.treasury()[Resource::Wood], wood - 3);
    CHECK(s.buyFieldwork("barricade").has_value());
    CHECK_EQ(s.treasury()[Resource::Gold], gold - 250);
    s.grantXp(AdventureSession::xpForLevel(4) - s.heroProgress().xp);
    CHECK_EQ(s.fieldworkCapacity(), 2);
    CHECK(!s.buyFieldwork("stakes"));
    CHECK(s.buyFieldwork("nonsense").has_value());
    s.grantXp(AdventureSession::xpForLevel(5) - s.heroProgress().xp);
    CHECK_EQ(s.fieldworkCapacity(), 3);
    CHECK(s.buyFieldwork("barricade").has_value()); // wood exhausted; no charge
    AdventureSession loaded;
    CHECK(!loaded.loadState(Scenario::Json::parse(s.saveState().dump())));
    CHECK_EQ(loaded.fieldworkCapacity(), 3);
    CHECK_EQ(loaded.fieldworkStock().at("barricade"), 1);
    CHECK_EQ(loaded.fieldworkStock().at("stakes"), 1);
    for (const auto& path : s.heroPaths())
        for (const auto& node : path.nodes) CHECK(node.live);
}

SUITE("Fieldworks — barricades stop shots and reactions; stakes allow shots; both block routes") {
    UnitType bow;
    bow.id = bow.name = "Test bow"; bow.speed = 9; bow.hitPoints = 100;
    bow.attack = 5; bow.defense = 5; bow.minDamage = bow.maxDamage = 5;
    bow.moveRange = 5; bow.shots = 12; bow.abilities = {"ranged", "readied_shot"};
    const UnitType* dummy = type("Fieldwork target", 1, 1, 100, 5);
    CombatArmy p; p.isPlayer = true; p.stacks.push_back(CombatUnit::make(&bow, 1, true));
    CombatArmy e; e.isPlayer = false; e.stacks.push_back(CombatUnit::make(dummy, 1, false));
    CombatEngine eng(std::move(p), std::move(e));
    const HexCoord barrier = CombatMap::toHex(2, 2);
    CHECK(eng.canShoot(eng.activeUnit()));
    CHECK(eng.canAttack(0));
    CHECK(eng.placeFieldwork(barrier, true));
    CHECK(!eng.canMoveTo(barrier));
    CHECK(!eng.canAttack(0));
    CHECK(!eng.previewAttack(0).valid);
    CHECK(!eng.canAttackFrom(eng.activeUnit().pos, 0));
    CHECK(!eng.hasLineOfSight(eng.playerArmy().stacks[0].pos, eng.enemyArmy().stacks[0].pos));
    eng.doDefend(); // enemy moves toward our readied archer
    CHECK(eng.reactionsTo(CombatMap::toHex(6, 2)).empty());
    CHECK(eng.removeFieldwork(barrier));
    CHECK(eng.placeFieldwork(barrier, false));
    CHECK(!eng.reactionsTo(CombatMap::toHex(6, 2)).empty());
    eng.doDefend();
    CHECK(eng.canAttack(0));
    CHECK(!eng.previewAttack(0).blocked);
    CHECK(!eng.canMoveTo(barrier));
    CHECK(eng.canMoveTo(CombatMap::toHex(3, 2)));
    eng.drainEvents();
    eng.doMove(CombatMap::toHex(3, 2));
    for (const auto& event : eng.drainEvents())
        if (event.type == CombatEvent::Type::UnitMoved)
            for (const auto& cell : event.path) CHECK(cell != barrier);
}

SUITE("Fieldworks — cannot seal off the battlefield") {
    const UnitType* unit = type("Builder test", 3, 1, 100);
    CombatArmy p; p.isPlayer = true; p.stacks.push_back(CombatUnit::make(unit, 1, true));
    CombatArmy e; e.isPlayer = false; e.stacks.push_back(CombatUnit::make(unit, 1, false));
    CombatEngine eng(std::move(p), std::move(e));
    CHECK(!eng.placeFieldwork({0, 2}, true));
    CHECK(!eng.placeFieldwork({99, 99}, true));
    for (int row = 0; row < 4; ++row) CHECK(eng.placeFieldwork(CombatMap::toHex(3, row), true));
    CHECK(!eng.placeFieldwork(CombatMap::toHex(3, 4), true));
    CHECK_EQ((int)eng.fieldworks().size(), 4);
}
