#include "test_runner.h"

#ifdef COMBAT_ENGINE_IMPL
#include "combat/CombatEngine.h"

// ── Helpers ───────────────────────────────────────────────────────────────────

static UnitType makeUnit(const std::string& name, int speed) {
    UnitType t;
    t.id        = name;
    t.name      = name;
    t.speed     = speed;
    t.hitPoints = 10;
    t.attack    = 5;
    t.defense   = 3;
    t.minDamage = 1;
    t.maxDamage = 3;
    return t;
}

static CombatArmy oneStack(const std::string& name, int speed, bool isPlayer) {
    CombatArmy army;
    army.ownerName = isPlayer ? "Player" : "Enemy";
    army.isPlayer  = isPlayer;
    army.stacks.push_back(CombatUnit::make(makeUnit(name, speed), 5, isPlayer));
    return army;
}

// ── Initiative order ──────────────────────────────────────────────────────────

SUITE("CombatEngine — higher speed acts first") {
    // Player speed 4, enemy speed 8 → enemy should be first
    CombatArmy p = oneStack("Slow", 4, true);
    CombatArmy e = oneStack("Fast", 8, false);
    CombatEngine eng(std::move(p), std::move(e));

    const TurnSlot& first = eng.currentTurn();
    CHECK(!first.isPlayer);  // enemy goes first (higher speed)
}

SUITE("CombatEngine — player wins speed tie") {
    CombatArmy p = oneStack("PlayerUnit", 6, true);
    CombatArmy e = oneStack("EnemyUnit",  6, false);
    CombatEngine eng(std::move(p), std::move(e));

    const TurnSlot& first = eng.currentTurn();
    CHECK(first.isPlayer);  // equal speed → player goes first
}

SUITE("CombatEngine — advance cycles through both stacks") {
    CombatArmy p = oneStack("P", 5, true);
    CombatArmy e = oneStack("E", 5, false);
    CombatEngine eng(std::move(p), std::move(e));

    bool firstIsPlayer = eng.currentTurn().isPlayer;
    eng.doAttack(0);  // advance past first actor

    bool secondIsPlayer = eng.currentTurn().isPlayer;
    CHECK(firstIsPlayer != secondIsPlayer);  // alternating actors
}

SUITE("CombatEngine — defend sets isDefending on active unit") {
    CombatArmy p = oneStack("P", 5, true);
    CombatArmy e = oneStack("E", 3, false);  // player acts first (higher speed)
    CombatEngine eng(std::move(p), std::move(e));

    CHECK(eng.currentTurn().isPlayer);  // player's turn
    eng.doDefend();

    // After defending, the player stack should have isDefending set
    CHECK(eng.playerArmy().stacks[0].isDefending);
}

SUITE("CombatEngine — doRetreat sets result to Retreated") {
    CombatArmy p = oneStack("P", 5, true);
    CombatArmy e = oneStack("E", 3, false);
    CombatEngine eng(std::move(p), std::move(e));

    CHECK(!eng.isOver());
    eng.doRetreat();
    CHECK(eng.isOver());
    CHECK(eng.result() == CombatResult::Retreated);
}

SUITE("CombatEngine — advance wraps to round 2") {
    CombatArmy p = oneStack("P", 5, true);
    CombatArmy e = oneStack("E", 3, false);
    CombatEngine eng(std::move(p), std::move(e));

    CHECK_EQ(eng.roundNumber(), 1);

    eng.doAttack(0);  // player acts
    eng.doAttack(0);  // enemy acts → round ends, rebuilds queue

    CHECK_EQ(eng.roundNumber(), 2);
}

SUITE("CombatUnit — totalHp full stack") {
    UnitType t = makeUnit("X", 5);
    CombatUnit u = CombatUnit::make(t, 4, true);
    // 4 creatures × 10 hp each = 40
    CHECK_EQ(u.totalHp(), 40);
}

SUITE("CombatUnit — isDead only when count == 0") {
    UnitType t = makeUnit("X", 5);
    CombatUnit u = CombatUnit::make(t, 3, true);
    CHECK(!u.isDead());
    u.count = 0;
    CHECK(u.isDead());
}

SUITE("CombatArmy — allDead requires all stacks dead") {
    CombatArmy army;
    army.isPlayer = true;
    UnitType t = makeUnit("X", 5);
    army.stacks.push_back(CombatUnit::make(t, 2, true));
    army.stacks.push_back(CombatUnit::make(t, 3, true));

    CHECK(!army.allDead());

    army.stacks[0].count = 0;
    CHECK(!army.allDead());  // second stack still alive

    army.stacks[1].count = 0;
    CHECK(army.allDead());   // both dead
}

// ── Movement ──────────────────────────────────────────────────────────────────

static CombatArmy oneStackWithRange(const std::string& name, int speed, int moveRange, bool isPlayer) {
    UnitType t = makeUnit(name, speed);
    t.moveRange = moveRange;
    CombatArmy army;
    army.ownerName = isPlayer ? "Player" : "Enemy";
    army.isPlayer  = isPlayer;
    army.stacks.push_back(CombatUnit::make(t, 5, isPlayer));
    return army;
}

SUITE("CombatEngine — canMoveTo reachable hex") {
    // Player has moveRange=3, enemy has moveRange=1
    CombatArmy p = oneStackWithRange("P", 5, 3, true);
    CombatArmy e = oneStackWithRange("E", 3, 1, false);
    CombatEngine eng(std::move(p), std::move(e));

    CHECK(eng.currentTurn().isPlayer);  // player acts first

    // A hex within moveRange that is not the spawn should be reachable
    HexCoord dest = CombatMap::toHex(2, 2);  // col 2 = just inside player side
    CHECK(eng.canMoveTo(dest));
}

SUITE("CombatEngine — canMoveTo beyond moveRange is false") {
    CombatArmy p = oneStackWithRange("P", 5, 1, true);
    CombatArmy e = oneStackWithRange("E", 3, 1, false);
    CombatEngine eng(std::move(p), std::move(e));

    CHECK(eng.currentTurn().isPlayer);

    // Enemy spawn is at col 10 — far beyond moveRange=1
    HexCoord far = CombatMap::toHex(10, 2);
    CHECK(!eng.canMoveTo(far));
}

SUITE("CombatEngine — doMove updates position and advances turn") {
    CombatArmy p = oneStackWithRange("P", 5, 3, true);
    CombatArmy e = oneStackWithRange("E", 3, 1, false);
    CombatEngine eng(std::move(p), std::move(e));

    CHECK(eng.currentTurn().isPlayer);
    HexCoord dest = CombatMap::toHex(2, 2);
    CHECK(eng.canMoveTo(dest));

    eng.doMove(dest);

    // Player stack should now be at dest
    CHECK(eng.playerArmy().stacks[0].pos == dest);
    // Turn should have advanced to enemy
    CHECK(!eng.currentTurn().isPlayer);
}

SUITE("CombatEngine — doAttackAt finds enemy by hex") {
    CombatArmy p = oneStackWithRange("P", 5, 3, true);
    CombatArmy e = oneStackWithRange("E", 3, 1, false);
    CombatEngine eng(std::move(p), std::move(e));

    CHECK(eng.currentTurn().isPlayer);

    // Enemy is at its spawn (col 10, row 2)
    HexCoord enemyHex = eng.enemyArmy().stacks[0].pos;

    // Player has moveRange=3 — enemy spawn is not adjacent, so no attackable tile
    // But doAttackAt still finds the unit by position regardless
    bool found = eng.doAttackAt(enemyHex);
    CHECK(found);
}

SUITE("CombatEngine — doAttackAt returns false for empty hex") {
    CombatArmy p = oneStackWithRange("P", 5, 3, true);
    CombatArmy e = oneStackWithRange("E", 3, 1, false);
    CombatEngine eng(std::move(p), std::move(e));

    // An in-bounds hex with no enemy on it
    HexCoord empty = CombatMap::toHex(5, 2);
    CHECK(!eng.doAttackAt(empty));
}

#endif // COMBAT_ENGINE_IMPL
