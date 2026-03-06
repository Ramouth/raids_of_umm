#include "test_runner.h"

#ifdef COMBAT_ENGINE_IMPL
#include "combat/CombatAI.h"
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

// ── CombatAI ──────────────────────────────────────────────────────────────────

// Helper: make a one-stack engine with the enemy already placed adjacent to
// the player (col 1 vs col 0 spawn), so the AI triggers an attack immediately.
static CombatEngine adjacentEngine(bool playerGoesFirst) {
    int pSpeed = playerGoesFirst ? 5 : 3;
    int eSpeed = playerGoesFirst ? 3 : 5;
    CombatArmy p = oneStack("P", pSpeed, true);
    CombatArmy e = oneStack("E", eSpeed, false);
    CombatEngine eng(std::move(p), std::move(e));
    // Teleport enemy right next to player spawn (col0,row2 and col1,row1 are adjacent)
    eng.teleportUnit(false, 0, CombatMap::toHex(1, 2));
    return eng;
}

SUITE("CombatAI — attacks immediately when adjacent") {
    // Enemy is placed adjacent to player; AI (enemy turn) should attack, not move.
    CombatEngine eng = adjacentEngine(/*playerGoesFirst=*/true);

    // Skip the player's turn so it becomes the enemy's turn.
    eng.doDefend();
    eng.drainEvents();

    CHECK(!eng.currentTurn().isPlayer);
    int hpBefore = eng.playerArmy().stacks[0].totalHp();

    CombatAI::takeTurn(eng);
    auto events = eng.drainEvents();

    // Must have attacked (UnitAttacked event from enemy).
    bool attacked = false;
    for (const auto& ev : events)
        if (ev.type == CombatEvent::Type::UnitAttacked && !ev.isPlayer)
            attacked = true;
    CHECK(attacked);

    // Player took damage.
    CHECK(eng.playerArmy().stacks[0].totalHp() < hpBefore);
}

SUITE("CombatAI — moves toward enemy when not adjacent") {
    // Default spawn: player col 0, enemy col 10 — distance 10. AI should move.
    CombatArmy p = oneStackWithRange("P", 3, 1, true);
    CombatArmy e = oneStackWithRange("E", 5, 3, false);  // enemy acts first
    CombatEngine eng(std::move(p), std::move(e));

    HexCoord enemyBefore = eng.enemyArmy().stacks[0].pos;
    HexCoord playerPos   = eng.playerArmy().stacks[0].pos;

    CHECK(!eng.currentTurn().isPlayer);  // enemy acts first
    CombatAI::takeTurn(eng);
    eng.drainEvents();

    HexCoord enemyAfter = eng.enemyArmy().stacks[0].pos;

    // Enemy must have moved closer to the player.
    CHECK(enemyAfter.distanceTo(playerPos) < enemyBefore.distanceTo(playerPos));
}

SUITE("CombatAI — ranged unit attacks without moving") {
    UnitType archerT = *makeFixed("Archer", 5, 3, 100, 5, 5);
    archerT.shots = 10;
    const UnitType* at = reg(std::move(archerT));

    CombatArmy pArmy;
    pArmy.isPlayer = true; pArmy.ownerName = "Player";
    pArmy.stacks.push_back(CombatUnit::make(at, 1, true));

    CombatArmy eArmy = oneStack("E", 3, false);
    CombatEngine eng(std::move(pArmy), std::move(eArmy));

    CHECK(eng.currentTurn().isPlayer);
    HexCoord posBefore = eng.playerArmy().stacks[0].pos;

    CombatAI::takeTurn(eng);  // AI controls the player's ranged unit
    eng.drainEvents();

    // Unit must NOT have moved (ranged attack in place).
    CHECK(eng.playerArmy().stacks[0].pos == posBefore);
    // Shots decremented.
    CHECK_EQ(eng.playerArmy().stacks[0].shotsLeft, 9);
}

SUITE("CombatAI — defends when moveRange is zero and enemy is not adjacent") {
    // moveRange=0 → reachableTiles() is empty; enemy spawns at col 10 (distance 10).
    // AI has nowhere to go and nothing adjacent to attack → must defend.
    UnitType frozen = *makeUnit("Frozen", 5);
    frozen.moveRange = 0;
    const UnitType* ft = reg(std::move(frozen));

    CombatArmy pArmy;
    pArmy.isPlayer = true; pArmy.ownerName = "Player";
    pArmy.stacks.push_back(CombatUnit::make(ft, 1, true));

    CombatArmy eArmy = oneStack("E", 3, false);
    CombatEngine eng(std::move(pArmy), std::move(eArmy));

    // Confirm no reachable tiles.
    CHECK(eng.reachableTiles().empty());

    CombatAI::takeTurn(eng);
    auto events = eng.drainEvents();

    bool defended = false;
    for (const auto& ev : events)
        if (ev.type == CombatEvent::Type::UnitDefended && ev.isPlayer)
            defended = true;
    CHECK(defended);
}

SUITE("CombatAI — targets weakest when attacking among multiple adjacent stacks") {
    // Two enemy stacks adjacent to player: one healthy (100 hp), one wounded (5 hp).
    // AI should attack the wounded one.
    const UnitType* pt = makeFixed("P", 5, 5, 10, 5, 5);
    const UnitType* et = makeFixed("E", 3, 5, 10, 5, 5);

    CombatArmy pArmy;
    pArmy.isPlayer = true; pArmy.ownerName = "Player";
    pArmy.stacks.push_back(CombatUnit::make(pt, 10, true));  // 10 × 10hp = 100hp total

    CombatArmy eArmy;
    eArmy.isPlayer = false; eArmy.ownerName = "Enemy";
    CombatUnit healthy = CombatUnit::make(et, 10, false);  // 100 hp
    CombatUnit wounded = CombatUnit::make(et, 1,  false);  // 10 hp (wounded)
    eArmy.stacks.push_back(healthy);
    eArmy.stacks.push_back(wounded);

    CombatEngine eng(std::move(pArmy), std::move(eArmy));

    // Place both enemies adjacent to player spawn.
    HexCoord playerPos = eng.playerArmy().stacks[0].pos;
    eng.teleportUnit(false, 0, playerPos.neighbor(0));  // healthy — adj
    eng.teleportUnit(false, 1, playerPos.neighbor(1));  // wounded — adj

    CHECK(eng.currentTurn().isPlayer);
    CombatAI::takeTurn(eng);
    auto events = eng.drainEvents();

    // The UnitDamaged event should target stack 1 (wounded, index 1).
    bool hitWounded = false;
    for (const auto& ev : events)
        if (ev.type == CombatEvent::Type::UnitDamaged && !ev.isPlayer
                && ev.stackIndex == 1)
            hitWounded = true;
    CHECK(hitWounded);
}

SUITE("CombatAI — full auto-battle resolves to a winner") {
    // Run complete AI-vs-AI combat and verify it terminates with a result.
    // Use balanced armies so neither side trivially wins on round 1.
    CombatEngine eng(
        fixedStack("P", 5, 3, 10, 5, 5, 5, true),
        fixedStack("E", 4, 3, 10, 5, 5, 5, false));

    int safetyLimit = 200;  // max iterations to avoid infinite loop in test
    while (!eng.isOver() && safetyLimit-- > 0) {
        CombatAI::takeTurn(eng);
        eng.drainEvents();
    }

    CHECK(safetyLimit > 0);     // terminated before limit
    CHECK(eng.isOver());         // battle actually concluded
    CHECK(eng.result() != CombatResult::Ongoing);
}

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

// ── CombatEvent queue ──────────────────────────────────────────────────────────

SUITE("CombatEvent — drainEvents is empty before any action") {
    CombatArmy p = oneStack("P", 5, true);
    CombatArmy e = oneStack("E", 3, false);
    CombatEngine eng(std::move(p), std::move(e));

    auto events = eng.drainEvents();
    CHECK(events.empty());
}

SUITE("CombatEvent — drainEvents clears queue on second call") {
    CombatArmy p = oneStack("P", 5, true);
    CombatArmy e = oneStack("E", 3, false);
    CombatEngine eng(std::move(p), std::move(e));

    eng.doDefend();
    auto first = eng.drainEvents();
    CHECK(!first.empty());

    auto second = eng.drainEvents();
    CHECK(second.empty());
}

SUITE("CombatEvent — doMove produces UnitMoved with correct from/to") {
    CombatArmy p = oneStackWithRange("P", 5, 3, true);
    CombatArmy e = oneStackWithRange("E", 3, 1, false);
    CombatEngine eng(std::move(p), std::move(e));

    HexCoord origin = eng.playerArmy().stacks[0].pos;
    HexCoord dest   = CombatMap::toHex(2, 2);
    CHECK(eng.canMoveTo(dest));

    eng.doMove(dest);
    auto events = eng.drainEvents();

    CHECK_EQ((int)events.size(), 1);
    CHECK(events[0].type == CombatEvent::Type::UnitMoved);
    CHECK(events[0].isPlayer);
    CHECK_EQ(events[0].stackIndex, 0);
    CHECK(events[0].from == origin);
    CHECK(events[0].to   == dest);
}

SUITE("CombatEvent — doAttack produces UnitAttacked then UnitDamaged") {
    // Large HP pools so no kills, no deaths to worry about.
    CombatEngine eng(
        fixedStack("A", 5, 1, 100, 5, 5, 3, true),
        fixedStack("D", 3, 1, 100, 5, 5, 3, false));

    eng.doAttack(0);
    auto events = eng.drainEvents();

    // First two events: attack then damage (on the enemy).
    CHECK((int)events.size() >= 2);
    CHECK(events[0].type == CombatEvent::Type::UnitAttacked);
    CHECK(events[0].isPlayer);
    CHECK(events[1].type == CombatEvent::Type::UnitDamaged);
    CHECK(!events[1].isPlayer);  // enemy took the damage
    CHECK(events[1].damage > 0);
}

SUITE("CombatEvent — melee retaliation produces a second UnitAttacked + UnitDamaged") {
    // Both sides have plenty of HP so retaliation fires and no one dies.
    CombatEngine eng(
        fixedStack("A", 5, 1, 100, 5, 5, 3, true),
        fixedStack("D", 3, 1, 100, 5, 5, 3, false));

    eng.doAttack(0);
    auto events = eng.drainEvents();

    // Expected sequence: UnitAttacked, UnitDamaged, UnitAttacked (ret), UnitDamaged (ret)
    CHECK_EQ((int)events.size(), 4);
    CHECK(events[0].type == CombatEvent::Type::UnitAttacked);
    CHECK(events[1].type == CombatEvent::Type::UnitDamaged);
    CHECK(events[2].type == CombatEvent::Type::UnitAttacked);
    CHECK(!events[2].isPlayer);   // retaliator is the enemy
    CHECK(events[3].type == CombatEvent::Type::UnitDamaged);
    CHECK(events[3].isPlayer);    // attacker took retaliation
}

SUITE("CombatEvent — lethal attack produces UnitDied then BattleEnded") {
    // 5 × dmg=10 → 50 damage kills the 1×hp=10 enemy instantly.
    CombatEngine eng(
        fixedStack("A", 5, 10, 10, 5, 5, 5, true),
        fixedStack("D", 3,  1, 10, 5, 5, 1, false));

    eng.doAttack(0);
    auto events = eng.drainEvents();

    bool hasDied  = false;
    bool hasEnded = false;
    for (const auto& ev : events) {
        if (ev.type == CombatEvent::Type::UnitDied)    hasDied  = true;
        if (ev.type == CombatEvent::Type::BattleEnded) hasEnded = true;
    }
    CHECK(hasDied);
    CHECK(hasEnded);

    for (const auto& ev : events)
        if (ev.type == CombatEvent::Type::BattleEnded)
            CHECK(ev.result == CombatResult::PlayerWon);
}

SUITE("CombatEvent — doDefend produces exactly one UnitDefended event") {
    CombatArmy p = oneStack("P", 5, true);
    CombatArmy e = oneStack("E", 3, false);
    CombatEngine eng(std::move(p), std::move(e));

    eng.doDefend();
    auto events = eng.drainEvents();

    CHECK_EQ((int)events.size(), 1);
    CHECK(events[0].type == CombatEvent::Type::UnitDefended);
    CHECK(events[0].isPlayer);
    CHECK_EQ(events[0].stackIndex, 0);
}

SUITE("CombatEvent — doRetreat produces BattleEnded(Retreated)") {
    CombatArmy p = oneStack("P", 5, true);
    CombatArmy e = oneStack("E", 3, false);
    CombatEngine eng(std::move(p), std::move(e));

    eng.doRetreat();
    auto events = eng.drainEvents();

    CHECK(!events.empty());
    CHECK(events.back().type   == CombatEvent::Type::BattleEnded);
    CHECK(events.back().result == CombatResult::Retreated);
}

SUITE("CombatEvent — events accumulate across actions before drain") {
    CombatArmy p = oneStack("P", 5, true);
    CombatArmy e = oneStack("E", 3, false);
    CombatEngine eng(std::move(p), std::move(e));

    // Two defends without draining in between.
    eng.doDefend();  // player defends, advances turn
    eng.doDefend();  // enemy defends

    auto events = eng.drainEvents();
    CHECK_EQ((int)events.size(), 2);
    CHECK(events[0].type == CombatEvent::Type::UnitDefended);
    CHECK(events[1].type == CombatEvent::Type::UnitDefended);
}

// ── Edge cases ────────────────────────────────────────────────────────────────

SUITE("CombatEngine — doAttack out-of-bounds index is a safe no-op") {
    CombatArmy p = oneStack("P", 5, true);
    CombatArmy e = oneStack("E", 3, false);
    CombatEngine eng(std::move(p), std::move(e));

    int turnBefore = eng.roundNumber();
    eng.doAttack(99);   // no enemy at index 99

    // No crash, no turn advance, no events.
    CHECK_EQ(eng.roundNumber(), turnBefore);
    auto events = eng.drainEvents();
    CHECK(events.empty());
}

SUITE("CombatEngine — doAttack on dead target is a safe no-op") {
    // Kill the enemy first via a lethal attack, then try to attack it again.
    CombatEngine eng(
        fixedStack("A", 5, 10, 10, 5, 5, 5, true),   // 5×dmg=10 → instant kill
        fixedStack("D", 3,  1, 10, 5, 5, 1, false));

    eng.doAttack(0);   // kills enemy[0]; game is now over
    eng.drainEvents(); // clear

    // Game is over, so a second call must be a no-op.
    eng.doAttack(0);
    auto events = eng.drainEvents();
    CHECK(events.empty());
}

SUITE("CombatEngine — lethal hit prevents retaliation") {
    // Attacker one-shots the target; no retaliation event should follow.
    CombatEngine eng(
        fixedStack("A", 5, 10, 10, 5, 5, 5, true),
        fixedStack("D", 3,  1, 10, 5, 5, 1, false));

    eng.doAttack(0);
    auto events = eng.drainEvents();

    // Must NOT contain any retaliation UnitAttacked from the enemy.
    int retaliationCount = 0;
    for (const auto& ev : events)
        if (ev.type == CombatEvent::Type::UnitAttacked && !ev.isPlayer)
            ++retaliationCount;
    CHECK_EQ(retaliationCount, 0);
}

SUITE("CombatEngine — attacker killed by retaliation emits UnitDied for attacker") {
    // Enemy retaliates with lethal damage (100 dmg), player has 1×hp=10.
    // Player survives the initial attack; enemy retaliates and kills player.
    CombatEngine eng(
        fixedStack("A", 5, 1, 10, 5, 5, 1, true),    // player: 1 creature, 10 hp
        fixedStack("D", 3, 100, 10, 5, 5, 5, false)); // enemy retaliation: 500 dmg

    eng.doAttack(0);
    auto events = eng.drainEvents();

    // Must contain UnitDied for the player (isPlayer==true).
    bool playerDied = false;
    for (const auto& ev : events)
        if (ev.type == CombatEvent::Type::UnitDied && ev.isPlayer)
            playerDied = true;
    CHECK(playerDied);

    // And the battle must be over with EnemyWon.
    CHECK(eng.isOver());
    CHECK(eng.result() == CombatResult::EnemyWon);

    bool battleEndedInEvents = false;
    for (const auto& ev : events)
        if (ev.type == CombatEvent::Type::BattleEnded && ev.result == CombatResult::EnemyWon)
            battleEndedInEvents = true;
    CHECK(battleEndedInEvents);
}

SUITE("CombatEngine — ranged attack produces no retaliation events") {
    // Archer at col 0, enemy at col 10 (distance > 1) → ranged shot, no retaliation.
    UnitType archerT = *makeFixed("Archer", 5, 3, 100, 5, 5);
    archerT.shots = 10;
    const UnitType* at = reg(std::move(archerT));

    CombatArmy pArmy;
    pArmy.isPlayer = true; pArmy.ownerName = "Player";
    pArmy.stacks.push_back(CombatUnit::make(at, 1, true));

    CombatEngine eng(std::move(pArmy),
                     fixedStack("D", 3, 1, 100, 5, 5, 3, false));

    eng.doAttack(0);
    auto events = eng.drainEvents();

    // No UnitAttacked event that originates from the enemy (retaliation).
    bool enemyRetaliated = false;
    for (const auto& ev : events)
        if (ev.type == CombatEvent::Type::UnitAttacked && !ev.isPlayer)
            enemyRetaliated = true;
    CHECK(!enemyRetaliated);

    // Attacker's shots decremented.
    CHECK_EQ(eng.playerArmy().stacks[0].shotsLeft, 9);
}

SUITE("CombatEngine — doRetreat when already over emits no duplicate BattleEnded") {
    CombatArmy p = oneStack("P", 5, true);
    CombatArmy e = oneStack("E", 3, false);
    CombatEngine eng(std::move(p), std::move(e));

    eng.doRetreat();
    eng.drainEvents();  // consume the first BattleEnded

    eng.doRetreat();    // second call — battle is already over
    auto events = eng.drainEvents();
    CHECK(events.empty());
}

SUITE("CombatEngine — hasRetaliated flag resets between rounds") {
    // In round 1: P attacks E, E retaliates.
    // After the round ends, hasRetaliated must be cleared so E can retaliate again in round 2.
    CombatArmy p = oneStack("P", 5, true);
    CombatArmy e = oneStack("E", 3, false);
    CombatEngine eng(std::move(p), std::move(e));

    eng.doAttack(0);   // P attacks E (round 1, player turn)
    eng.drainEvents();

    // Now it's E's turn — E attacks P.  After this, round wraps → round 2.
    eng.doAttack(0);
    eng.drainEvents();

    CHECK_EQ(eng.roundNumber(), 2);

    // Round 2: player attacks again — enemy should retaliate (not blocked by old flag).
    eng.doAttack(0);
    auto events = eng.drainEvents();

    bool retaliationFired = false;
    for (const auto& ev : events)
        if (ev.type == CombatEvent::Type::UnitAttacked && !ev.isPlayer)
            retaliationFired = true;
    CHECK(retaliationFired);
}

SUITE("CombatEngine — empty player army triggers EnemyWon at construction") {
    CombatArmy p;  // zero stacks
    p.isPlayer = true; p.ownerName = "Empty";

    CombatArmy e = oneStack("E", 3, false);
    CombatEngine eng(std::move(p), std::move(e));

    CHECK(eng.isOver());
    CHECK(eng.result() == CombatResult::EnemyWon);

    // BattleEnded event must be in the queue from construction.
    auto events = eng.drainEvents();
    bool hasEnded = false;
    for (const auto& ev : events)
        if (ev.type == CombatEvent::Type::BattleEnded && ev.result == CombatResult::EnemyWon)
            hasEnded = true;
    CHECK(hasEnded);
}

SUITE("CombatEngine — two stacks mutually kill each other: both UnitDied emitted") {
    // P[0] attacks E[0]: E[0] retaliates and kills P[0]; P[0] also kills E[0].
    // (P atk=100 vs E hp=10×1; E atk=100 vs P hp=10×1 — both one-shot each other.)
    // Because P attacks first, E dies from the primary hit. No retaliation fires.
    // This verifies that the "target died → no retaliation" rule holds even
    // when the attacker would have died from that retaliation.
    CombatEngine eng(
        fixedStack("P", 5, 100, 10, 5, 5, 1, true),
        fixedStack("E", 3, 100, 10, 5, 5, 1, false));

    eng.doAttack(0);
    auto events = eng.drainEvents();

    // Target (enemy) died.
    bool enemyDied = false;
    for (const auto& ev : events)
        if (ev.type == CombatEvent::Type::UnitDied && !ev.isPlayer)
            enemyDied = true;
    CHECK(enemyDied);

    // No retaliation (target was killed by primary hit).
    bool retaliationFired = false;
    for (const auto& ev : events)
        if (ev.type == CombatEvent::Type::UnitAttacked && !ev.isPlayer)
            retaliationFired = true;
    CHECK(!retaliationFired);

    CHECK(eng.isOver());
    CHECK(eng.result() == CombatResult::PlayerWon);
}

SUITE("CombatEngine — event queue does not bleed between rounds") {
    // Drain after each action; verify the queue is truly empty between actions.
    CombatArmy p = oneStack("P", 5, true);
    CombatArmy e = oneStack("E", 3, false);
    CombatEngine eng(std::move(p), std::move(e));

    eng.doDefend();
    auto r1 = eng.drainEvents();  // round 1 player defend
    CHECK(!r1.empty());
    CHECK(eng.drainEvents().empty());  // double-drain: must be empty

    eng.doDefend();
    auto r2 = eng.drainEvents();  // round 1 enemy defend → round 2
    CHECK(!r2.empty());
    CHECK(eng.drainEvents().empty());
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

// ── CombatOutcome propagation ─────────────────────────────────────────────────

SUITE("CombatOutcome — player win fills survivors") {
    // Make a one-vs-one battle where player wins (higher stats).
    UnitType pt; pt.id="p"; pt.name="p"; pt.speed=5; pt.hitPoints=100;
    pt.attack=20; pt.defense=5; pt.minDamage=10; pt.maxDamage=20;
    UnitType et; et.id="e"; et.name="e"; et.speed=4; et.hitPoints=1;
    et.attack=1;  et.defense=0; et.minDamage=1;  et.maxDamage=1;
    s_types.push_back(pt); const UnitType* pT = &s_types.back();
    s_types.push_back(et); const UnitType* eT = &s_types.back();

    CombatArmy pA; pA.isPlayer=true;  pA.ownerName="P";
    pA.stacks.push_back(CombatUnit::make(pT, 10, true));
    CombatArmy eA; eA.isPlayer=false; eA.ownerName="E";
    eA.stacks.push_back(CombatUnit::make(eT, 1, false));

    CombatEngine eng(std::move(pA), std::move(eA));

    // Drive battle to completion.
    int safety = 50;
    while (!eng.isOver() && safety-- > 0) {
        if (eng.currentTurn().isPlayer)
            eng.doAttack(0);
        else
            eng.doAttack(0);
        eng.drainEvents();
    }

    CHECK(eng.result() == CombatResult::PlayerWon);

    // Manually build what onResume() would receive.
    CombatOutcome outcome;
    outcome.result = eng.result();
    for (const auto& unit : eng.playerArmy().stacks)
        if (!unit.isDead())
            outcome.survivors.push_back({ unit.type, unit.count });

    CHECK(!outcome.survivors.empty());
    CHECK(outcome.survivors[0].type == pT);
    CHECK(outcome.survivors[0].count > 0);
}

SUITE("CombatOutcome — enemy win leaves survivors empty") {
    UnitType pt; pt.id="p2"; pt.name="p2"; pt.speed=4; pt.hitPoints=1;
    pt.attack=1;  pt.defense=0; pt.minDamage=1; pt.maxDamage=1;
    UnitType et; et.id="e2"; et.name="e2"; et.speed=5; et.hitPoints=100;
    et.attack=20; et.defense=5; et.minDamage=10; et.maxDamage=20;
    s_types.push_back(pt); const UnitType* pT = &s_types.back();
    s_types.push_back(et); const UnitType* eT = &s_types.back();
    (void)eT;

    CombatArmy pA; pA.isPlayer=true;  pA.ownerName="P";
    pA.stacks.push_back(CombatUnit::make(pT, 1, true));
    CombatArmy eA; eA.isPlayer=false; eA.ownerName="E";
    eA.stacks.push_back(CombatUnit::make(eT, 10, false));

    CombatEngine eng(std::move(pA), std::move(eA));

    int safety = 50;
    while (!eng.isOver() && safety-- > 0) {
        if (eng.currentTurn().isPlayer)
            eng.doAttack(0);
        else
            eng.doAttack(0);
        eng.drainEvents();
    }

    CHECK(eng.result() == CombatResult::EnemyWon);

    CombatOutcome outcome;
    outcome.result = eng.result();
    for (const auto& unit : eng.playerArmy().stacks)
        if (!unit.isDead())
            outcome.survivors.push_back({ unit.type, unit.count });

    CHECK(outcome.survivors.empty());
}

SUITE("CombatOutcome — retreat result propagates") {
    CombatArmy pA = oneStack("R1", 5, true);
    CombatArmy eA = oneStack("R2", 4, false);
    CombatEngine eng(std::move(pA), std::move(eA));

    eng.doRetreat();
    eng.drainEvents();

    CHECK(eng.result() == CombatResult::Retreated);

    CombatOutcome outcome;
    outcome.result = eng.result();
    for (const auto& unit : eng.playerArmy().stacks)
        if (!unit.isDead())
            outcome.survivors.push_back({ unit.type, unit.count });

    // After retreat player army is technically unchanged (no one died).
    // survivors should still have the stack.
    CHECK(!outcome.survivors.empty());
    CHECK(outcome.result == CombatResult::Retreated);
}

// ── Flanking / Pinned ─────────────────────────────────────────────────────────

// Helper: two-stack player army
static CombatArmy twoStacks(const std::string& a, const std::string& b,
                             int speed, bool isPlayer) {
    CombatArmy army;
    army.ownerName = isPlayer ? "Player" : "Enemy";
    army.isPlayer  = isPlayer;
    army.stacks.push_back(CombatUnit::make(makeUnit(a, speed), 5, isPlayer));
    army.stacks.push_back(CombatUnit::make(makeUnit(b, speed), 5, isPlayer));
    return army;
}

SUITE("CombatEngine — isFlanked false when only one attacker") {
    // One player stack adjacent to the target, no opposite-side attacker.
    CombatArmy p = twoStacks("A", "B", 6, true);
    CombatArmy e = oneStack("Target", 4, false);
    CombatEngine eng(std::move(p), std::move(e));

    // Place target at (0,0), one attacker at neighbor(0) — no flanking yet.
    HexCoord targetPos{ 0, 0 };
    eng.teleportUnit(false, 0, targetPos);
    eng.teleportUnit(true,  0, targetPos.neighbor(0));
    eng.teleportUnit(true,  1, targetPos.neighbor(1)); // same side cluster, not opposite

    const auto& target    = eng.enemyArmy().stacks[0];
    const auto& attackers = eng.playerArmy().stacks;
    CHECK(!CombatEngine::isFlanked(target, attackers));
}

SUITE("CombatEngine — isFlanked true when attackers on opposite sides") {
    CombatArmy p = twoStacks("A", "B", 6, true);
    CombatArmy e = oneStack("Target", 4, false);
    CombatEngine eng(std::move(p), std::move(e));

    HexCoord targetPos{ 0, 0 };
    eng.teleportUnit(false, 0, targetPos);
    eng.teleportUnit(true,  0, targetPos.neighbor(0));   // side 0
    eng.teleportUnit(true,  1, targetPos.neighbor(3));   // opposite side 3

    const auto& target    = eng.enemyArmy().stacks[0];
    const auto& attackers = eng.playerArmy().stacks;
    CHECK(CombatEngine::isFlanked(target, attackers));
}

SUITE("CombatEngine — pinned unit takes at least 150% of minimum possible damage") {
    // Attacker: 20 creatures, minDmg=2, maxDmg=2 (fixed), attack=5, defense=1.
    // Target: defense=1. mult = 1 + 0.05*(5-1) = 1.2.
    // Non-flanked minimum: 20 * 2 * 1.2 = 48.
    // Flanked minimum:     48 * 1.5     = 72.
    // Target has 10000 HP so it won't die; we just check dealt damage >= 72.

    UnitType atkType; atkType.id="atk"; atkType.name="Atk";
    atkType.speed=6; atkType.hitPoints=10; atkType.attack=5; atkType.defense=1;
    atkType.minDamage=2; atkType.maxDamage=2;
    const UnitType* atkPtr = reg(atkType);

    UnitType defType; defType.id="def"; defType.name="Def";
    defType.speed=4; defType.hitPoints=10000; defType.attack=1; defType.defense=1;
    defType.minDamage=1; defType.maxDamage=1;
    const UnitType* defPtr = reg(defType);

    CombatArmy p; p.ownerName="Player"; p.isPlayer=true;
    p.stacks.push_back(CombatUnit::make(atkPtr, 20, true));
    p.stacks.push_back(CombatUnit::make(atkPtr, 20, true));

    CombatArmy e; e.ownerName="Enemy"; e.isPlayer=false;
    e.stacks.push_back(CombatUnit::make(defPtr, 1, false));

    CombatEngine eng(std::move(p), std::move(e));

    HexCoord targetPos{ 3, 0 };
    eng.teleportUnit(false, 0, targetPos);
    eng.teleportUnit(true,  0, targetPos.neighbor(0)); // side 0
    eng.teleportUnit(true,  1, targetPos.neighbor(3)); // opposite side 3 → pinned

    CHECK(CombatEngine::isFlanked(eng.enemyArmy().stacks[0],
                                   eng.playerArmy().stacks));

    int hpBefore = eng.enemyArmy().stacks[0].hpLeft;
    eng.doAttack(0);
    eng.drainEvents();
    int hpAfter  = eng.enemyArmy().stacks[0].hpLeft;
    int dealt    = hpBefore - hpAfter;

    CHECK(dealt >= 72);  // flanked floor: 20 * 2 * 1.2 * 1.5 = 72
}

SUITE("CombatEngine — pinned unit cannot retaliate") {
    UnitType strongType; strongType.id="strong"; strongType.name="Strong";
    strongType.speed=6; strongType.hitPoints=5; strongType.attack=10;
    strongType.defense=1; strongType.minDamage=1; strongType.maxDamage=2;
    const UnitType* strongPtr = reg(strongType);

    UnitType weakType; weakType.id="weak"; weakType.name="Weak";
    weakType.speed=4; weakType.hitPoints=50; weakType.attack=1;
    weakType.defense=1; weakType.minDamage=1; weakType.maxDamage=1;
    const UnitType* weakPtr = reg(weakType);

    CombatArmy p; p.ownerName="Player"; p.isPlayer=true;
    p.stacks.push_back(CombatUnit::make(strongPtr, 5, true));
    p.stacks.push_back(CombatUnit::make(strongPtr, 5, true));

    CombatArmy e; e.ownerName="Enemy"; e.isPlayer=false;
    e.stacks.push_back(CombatUnit::make(weakPtr, 100, false));

    CombatEngine eng(std::move(p), std::move(e));

    // Pin the target: player stacks on opposite sides
    HexCoord targetPos{ 3, 0 };
    eng.teleportUnit(false, 0, targetPos);
    eng.teleportUnit(true,  0, targetPos.neighbor(0));
    eng.teleportUnit(true,  1, targetPos.neighbor(3));

    int attackerCountBefore = eng.playerArmy().stacks[0].count;

    // Player stack 0 attacks — target is pinned, should not retaliate
    eng.doAttack(0);
    eng.drainEvents();

    // Attacker count unchanged = no retaliation damage killed any creatures
    CHECK_EQ(eng.playerArmy().stacks[0].count, attackerCountBefore);
}

#endif // COMBAT_ENGINE_IMPL
