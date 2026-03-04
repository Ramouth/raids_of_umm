#include "test_runner.h"

#ifdef COMBAT_ENGINE_IMPL
#include "combat/CombatEngine.h"
#include "hero/Hero.h"

// ── Test-local type registry ───────────────────────────────────────────────────
// CombatUnit stores const UnitType* — pointers must outlive the engine.
// This vector acts as a mini-ResourceManager for the test process.
static std::vector<UnitType> s_types;
static const UnitType* reg(UnitType t) {
    s_types.push_back(std::move(t));
    return &s_types.back();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

static const UnitType* makeUnit(const std::string& name, int speed) {
    UnitType t;
    t.id        = name;
    t.name      = name;
    t.speed     = speed;
    t.hitPoints = 10;
    t.attack    = 5;
    t.defense   = 3;
    t.minDamage = 1;
    t.maxDamage = 3;
    return reg(std::move(t));
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
    const UnitType* t = makeUnit("X", 5);
    CombatUnit u = CombatUnit::make(t, 4, true);
    // 4 creatures × 10 hp each = 40
    CHECK_EQ(u.totalHp(), 40);
}

SUITE("CombatUnit — isDead only when count == 0") {
    const UnitType* t = makeUnit("X", 5);
    CombatUnit u = CombatUnit::make(t, 3, true);
    CHECK(!u.isDead());
    u.count = 0;
    CHECK(u.isDead());
}

SUITE("CombatArmy — allDead requires all stacks dead") {
    CombatArmy army;
    army.isPlayer = true;
    const UnitType* t = makeUnit("X", 5);
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
    UnitType t = *makeUnit(name, speed);
    t.moveRange = moveRange;
    CombatArmy army;
    army.ownerName = isPlayer ? "Player" : "Enemy";
    army.isPlayer  = isPlayer;
    army.stacks.push_back(CombatUnit::make(reg(std::move(t)), 5, isPlayer));
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

// ── Damage resolution ─────────────────────────────────────────────────────────

// Deterministic UnitType: minDmg == maxDmg so every roll is identical.
static const UnitType* makeFixed(const std::string& name, int speed,
                                 int dmg, int hp, int atk, int def) {
    UnitType t;
    t.id = t.name  = name;
    t.speed        = speed;
    t.hitPoints    = hp;
    t.attack       = atk;
    t.defense      = def;
    t.minDamage    = t.maxDamage = dmg;
    t.moveRange    = 3;
    return reg(std::move(t));
}

static CombatArmy fixedStack(const std::string& name, int speed,
                             int dmg, int hp, int atk, int def,
                             int count, bool isPlayer) {
    CombatArmy army;
    army.ownerName = isPlayer ? "Player" : "Enemy";
    army.isPlayer  = isPlayer;
    army.stacks.push_back(
        CombatUnit::make(makeFixed(name, speed, dmg, hp, atk, def), count, isPlayer));
    return army;
}

SUITE("CombatEngine — applyDamage kills exact creature count") {
    // Attacker: 1×, dmg=10, atk=5; defender: 3×, hp=5, def=5.
    // diff=0 → mult=1.0, damage=10 → kills 2 of 3 (each 5 HP).
    CombatEngine eng(fixedStack("A", 5, 10, 100, 5, 5, 1, true),
                     fixedStack("D", 3,  1,   5, 5, 5, 3, false));
    eng.doAttack(0);

    CHECK_EQ(eng.enemyArmy().stacks[0].count,  1);
    CHECK_EQ(eng.enemyArmy().stacks[0].hpLeft, 5);
}

SUITE("CombatEngine — doAttack reduces target count") {
    // Player 5×dmg=5 → 25 damage vs enemy 3×hp=10 (30 total HP) → kills 2 of 3.
    CombatEngine eng(fixedStack("A", 5, 5, 10, 5, 5, 5, true),
                     fixedStack("D", 3, 1, 10, 5, 5, 3, false));
    eng.doAttack(0);

    const CombatUnit& target = eng.enemyArmy().stacks[0];
    CHECK(!target.isDead());
    CHECK(target.count < 3);
}

SUITE("CombatEngine — retaliation fires once and reduces attacker") {
    // Enemy: 5×dmg=5, strong retaliator (25 retaliation damage).
    // Player: 3×hp=10 (30 total HP) → 25 damage leaves count=1.
    CombatEngine eng(fixedStack("A", 5, 2, 10, 5, 5, 3, true),
                     fixedStack("D", 3, 5, 10, 5, 5, 5, false));
    int before = eng.playerArmy().stacks[0].count;
    eng.doAttack(0);

    const CombatUnit& actor = eng.playerArmy().stacks[0];
    CHECK(actor.count < before || actor.hpLeft < actor.type->hitPoints);
}

SUITE("CombatEngine — no second retaliation in same round") {
    // Two player stacks (P0 speed=5, P1 speed=4) both attack the same enemy.
    // Enemy retaliates on P0. P1 attacks next — enemy must NOT retaliate again.
    // Retaliation is lethal (dmg=100): if it fires on P1, P1 dies.
    const UnitType* pt0 = makeFixed("P0", 5,   5,  10,  5, 5);
    const UnitType* pt1 = makeFixed("P1", 4,   5,  10,  5, 5);

    CombatArmy pArmy;
    pArmy.isPlayer = true; pArmy.ownerName = "Player";
    pArmy.stacks.push_back(CombatUnit::make(pt0, 3, true));
    pArmy.stacks.push_back(CombatUnit::make(pt1, 3, true));

    CombatEngine eng(std::move(pArmy),
                     fixedStack("E", 3, 100, 100, 10, 5, 5, false));

    eng.doAttack(0);  // P0 attacks E → E retaliates (P0 may die)
    eng.doAttack(0);  // P1 attacks E → E must NOT retaliate (hasRetaliated=true)

    CHECK_EQ(eng.playerArmy().stacks[1].count, 3);  // P1 untouched
}

SUITE("CombatEngine — defending unit takes less damage") {
    // Player atk=10, enemy def=4.
    // Normal:    eff_def=4,   diff=6, mult=1.30 → 13 damage.
    // Defending: eff_def=4+1, diff=5, mult=1.25 → 12 damage.
    const UnitType* dt = makeFixed("D", 3, 1, 10, 5, 4);

    // Scenario 1: enemy not defending
    CombatEngine eng1(fixedStack("A", 5, 10, 10, 10, 5, 1, true),
                      fixedStack("D", 3,  1, 10,  5, 4, 100, false));
    eng1.doAttack(0);
    int hp1 = eng1.enemyArmy().stacks[0].totalHp();

    // Scenario 2: enemy pre-set to isDefending
    CombatArmy eArmy2;
    eArmy2.isPlayer = false; eArmy2.ownerName = "Enemy";
    CombatUnit defUnit = CombatUnit::make(dt, 100, false);
    defUnit.isDefending = true;
    eArmy2.stacks.push_back(defUnit);
    CombatEngine eng2(fixedStack("A", 5, 10, 10, 10, 5, 1, true), std::move(eArmy2));
    eng2.doAttack(0);
    int hp2 = eng2.enemyArmy().stacks[0].totalHp();

    CHECK(hp2 > hp1);  // defending unit absorbed more damage
}

SUITE("CombatEngine — win condition triggers after lethal attack") {
    // Single enemy with 10 HP; player 5×dmg=10 → 50 damage → instant kill.
    CombatEngine eng(fixedStack("A", 5, 10, 10, 5, 5, 5, true),
                     fixedStack("D", 3,  1, 10, 5, 5, 1, false));
    CHECK(!eng.isOver());
    eng.doAttack(0);
    CHECK(eng.isOver());
    CHECK(eng.result() == CombatResult::PlayerWon);
}

SUITE("CombatEngine — EnemyWon when player army is eliminated") {
    // Enemy has higher speed and one-shots the player stack.
    CombatEngine eng(fixedStack("P", 3, 1, 10, 5, 5, 1, true),
                     fixedStack("E", 5, 10, 10, 5, 5, 5, false));

    CHECK(!eng.currentTurn().isPlayer);  // enemy acts first (speed 5 > 3)
    eng.doAttack(0);  // enemy: 5×dmg=10=50 damage → player 1×hp=10 dies

    CHECK(eng.isOver());
    CHECK(eng.result() == CombatResult::EnemyWon);
}

SUITE("CombatEngine — no_retaliation ability suppresses enemy retaliation") {
    // Attacker has the "no_retaliation" tag; enemy must not hit back.
    // Enemy retaliation would be lethal (dmg=100), so surviving proves it didn't fire.
    UnitType atBase = *makeFixed("A", 5, 5, 10, 5, 5);
    atBase.abilities = {"no_retaliation"};
    const UnitType* at = reg(std::move(atBase));

    CombatArmy pArmy;
    pArmy.isPlayer = true; pArmy.ownerName = "Player";
    pArmy.stacks.push_back(CombatUnit::make(at, 3, true));

    CombatEngine eng(std::move(pArmy),
                     fixedStack("E", 3, 100, 100, 10, 5, 5, false));

    int hpBefore = eng.playerArmy().stacks[0].totalHp();
    eng.doAttack(0);
    CHECK_EQ(eng.playerArmy().stacks[0].totalHp(), hpBefore);  // no retaliation
}

SUITE("CombatEngine — exhausted ammo falls back to melee with retaliation") {
    // Archer type has shots=24 but unit's shotsLeft is forced to 0.
    // Attack must be treated as melee → enemy retaliates.
    UnitType archerBase = *makeFixed("Archer", 5, 3, 100, 5, 5);
    archerBase.shots = 24;  // type is ranged...
    const UnitType* archerType = reg(std::move(archerBase));

    CombatArmy pArmy;
    pArmy.isPlayer = true; pArmy.ownerName = "Player";
    CombatUnit archerUnit = CombatUnit::make(archerType, 1, true);
    archerUnit.shotsLeft = 0;  // ...but ammo is spent
    pArmy.stacks.push_back(archerUnit);

    // Enemy: 5× dmg=5 retaliator (would deal 25 damage to player's 100 HP)
    CombatEngine eng(std::move(pArmy),
                     fixedStack("E", 3, 5, 100, 5, 5, 5, false));

    int hpBefore = eng.playerArmy().stacks[0].totalHp();
    eng.doAttack(0);  // melee (no shots left) → retaliation fires
    CHECK(eng.playerArmy().stacks[0].totalHp() < hpBefore);
    CHECK_EQ(eng.playerArmy().stacks[0].shotsLeft, 0);  // unchanged (melee path)
}

SUITE("CombatEngine — dead stack turn is skipped within a round") {
    // Player kills enemy[0] on first attack; queue must advance to enemy[1], not dead [0].
    const UnitType* e1t = makeFixed("E1", 3, 1, 10, 5, 5);
    const UnitType* e2t = makeFixed("E2", 3, 1, 10, 5, 5);

    CombatArmy eArmy;
    eArmy.isPlayer = false; eArmy.ownerName = "Enemy";
    eArmy.stacks.push_back(CombatUnit::make(e1t, 1, false));  // dies from 50 dmg
    eArmy.stacks.push_back(CombatUnit::make(e2t, 3, false));  // survives

    CombatEngine eng(fixedStack("P", 5, 10, 10, 5, 5, 5, true), std::move(eArmy));

    CHECK(eng.currentTurn().isPlayer);
    eng.doAttack(0);  // P kills E1 (50 damage, count=1 × hp=10)

    const TurnSlot& next = eng.currentTurn();
    CHECK(!next.isPlayer);
    CHECK_EQ(next.stackIndex, 1);  // E2, not dead E1
}

SUITE("CombatEngine — ranged attack decrements shotsLeft") {
    // Archer (shots=24) fires from col 0 at enemy at col 10 (distance >> 1).
    // Attack is ranged → shotsLeft drops by 1, no retaliation.
    UnitType archerBase2 = *makeFixed("Archer", 5, 3, 10, 5, 5);
    archerBase2.shots = 24;
    const UnitType* archerType2 = reg(std::move(archerBase2));

    CombatArmy pArmy;
    pArmy.isPlayer = true; pArmy.ownerName = "Player";
    pArmy.stacks.push_back(CombatUnit::make(archerType2, 1, true));

    CombatEngine eng(std::move(pArmy),
                     fixedStack("D", 3, 1, 10, 5, 5, 1, false));
    CHECK_EQ(eng.playerArmy().stacks[0].shotsLeft, 24);
    eng.doAttack(0);
    CHECK_EQ(eng.playerArmy().stacks[0].shotsLeft, 23);
}

// ── BFS movement ──────────────────────────────────────────────────────────────

SUITE("CombatEngine — BFS: enemy wall blocks path to tiles behind it") {
    // Player spawns at col 0 row 2.  After construction we teleport three
    // enemy stacks to col 1 rows 1,2,3 forming a complete wall.
    // Player has moveRange=3.  Without BFS, toHex(2,2) would appear reachable
    // (distance 2 ≤ 3). With BFS it is unreachable: every path to col≥2 must
    // pass through col 1 which is fully occupied.

    const UnitType* pt = makeUnit("P", 5);
    const UnitType* et = makeUnit("E", 3);

    CombatArmy pArmy;
    pArmy.isPlayer = true; pArmy.ownerName = "Player";
    pArmy.stacks.push_back(CombatUnit::make(pt, 1, true));

    CombatArmy eArmy;
    eArmy.isPlayer = false; eArmy.ownerName = "Enemy";
    eArmy.stacks.push_back(CombatUnit::make(et, 1, false));
    eArmy.stacks.push_back(CombatUnit::make(et, 1, false));
    eArmy.stacks.push_back(CombatUnit::make(et, 1, false));

    CombatEngine eng(std::move(pArmy), std::move(eArmy));

    // Teleport enemies to form the col-1 wall after placeArmies() runs.
    eng.teleportUnit(false, 0, CombatMap::toHex(1, 1));
    eng.teleportUnit(false, 1, CombatMap::toHex(1, 2));
    eng.teleportUnit(false, 2, CombatMap::toHex(1, 3));

    CHECK(eng.currentTurn().isPlayer);

    auto reachable = eng.reachableTiles();

    // toHex(2,2) is behind the wall — must not be reachable.
    HexCoord blocked = CombatMap::toHex(2, 2);
    bool found = false;
    for (const auto& h : reachable)
        if (h == blocked) { found = true; break; }
    CHECK(!found);
}

SUITE("CombatEngine — BFS: tiles on near side of wall are still reachable") {
    // Same wall setup. Hexes at col 0 rows 0 and 4 are on the near side and
    // must still be reachable.

    const UnitType* pt = makeUnit("P2", 5);
    const UnitType* et = makeUnit("E2", 3);

    CombatArmy pArmy;
    pArmy.isPlayer = true; pArmy.ownerName = "Player";
    pArmy.stacks.push_back(CombatUnit::make(pt, 1, true));

    CombatArmy eArmy;
    eArmy.isPlayer = false; eArmy.ownerName = "Enemy";
    eArmy.stacks.push_back(CombatUnit::make(et, 1, false));
    eArmy.stacks.push_back(CombatUnit::make(et, 1, false));
    eArmy.stacks.push_back(CombatUnit::make(et, 1, false));

    CombatEngine eng(std::move(pArmy), std::move(eArmy));
    eng.teleportUnit(false, 0, CombatMap::toHex(1, 1));
    eng.teleportUnit(false, 1, CombatMap::toHex(1, 2));
    eng.teleportUnit(false, 2, CombatMap::toHex(1, 3));

    CHECK(eng.currentTurn().isPlayer);

    auto reachable = eng.reachableTiles();

    // col 0, rows 0 and 4 are accessible without crossing the wall.
    HexCoord near0 = CombatMap::toHex(0, 0);
    HexCoord near4 = CombatMap::toHex(0, 4);
    bool found0 = false, found4 = false;
    for (const auto& h : reachable) {
        if (h == near0) found0 = true;
        if (h == near4) found4 = true;
    }
    CHECK(found0);
    CHECK(found4);
}

#endif // COMBAT_ENGINE_IMPL
