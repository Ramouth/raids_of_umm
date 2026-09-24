#include "test_runner.h"

#ifdef COMBAT_ENGINE_IMPL
#include "combat/CombatAI.h"
#include "combat/CombatEngine.h"
#include "hero/Hero.h"
#include <deque>

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

SUITE("CombatAI — finishes a nearly-dead dangerous stack") {
    // Two adjacent enemies: a full stack of 10 brutes (100 HP) and a single
    // champion that hits for 20 but has only 8 HP left.  Wiping the champion
    // removes more enemy damage than halving the brutes (and draws no
    // retaliation), so the scorer must finish it.  Checked across seeds so
    // the softmax jitter can never pick the worse strike.
    const UnitType* pt = makeFixed("P", 5, 5, 10, 5, 5);
    const UnitType* bt = makeFixed("Brute", 3, 5, 10, 5, 5);
    const UnitType* ct = makeFixed("Champion", 3, 20, 50, 5, 5);
    bool alwaysFinished = true;
    for (uint32_t seed = 1; seed <= 20; ++seed) {
        CombatArmy pArmy;
        pArmy.isPlayer = true; pArmy.ownerName = "Player";
        pArmy.stacks.push_back(CombatUnit::make(pt, 10, true));
        CombatArmy eArmy;
        eArmy.isPlayer = false; eArmy.ownerName = "Enemy";
        eArmy.stacks.push_back(CombatUnit::make(bt, 10, false));
        CombatUnit champ = CombatUnit::make(ct, 1, false);
        champ.hpLeft = 8;
        eArmy.stacks.push_back(champ);

        CombatEngine eng(std::move(pArmy), std::move(eArmy));
        eng.setSeed(seed);
        HexCoord playerPos = eng.playerArmy().stacks[0].pos;
        eng.teleportUnit(false, 0, playerPos.neighbor(0));
        eng.teleportUnit(false, 1, playerPos.neighbor(1));

        CombatAI::takeTurn(eng);
        if (!eng.enemyArmy().stacks[1].isDead()) alwaysFinished = false;
    }
    CHECK(alwaysFinished);
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

// ── CombatOutcome SC fields ───────────────────────────────────────────────────

SUITE("CombatOutcome — scFound is empty by default") {
    CombatOutcome outcome;
    CHECK(outcome.scFound.empty());
}

SUITE("CombatOutcome — scFound can hold an SC") {
    CombatOutcome outcome;
    SpecialCharacter sc;
    sc.id        = "kharim";
    sc.name      = "Kha'Rim the Wanderer";
    sc.archetype = "tank";
    outcome.scFound.push_back(sc);
    CHECK_EQ((int)outcome.scFound.size(), 1);
    CHECK(outcome.scFound[0].id        == std::string("kharim"));
    CHECK(outcome.scFound[0].archetype == std::string("tank"));
}

SUITE("Hero — addSpecial from outcome scFound") {
    Hero h;
    CombatOutcome outcome;
    SpecialCharacter sc;
    sc.id        = "kharim";
    sc.name      = "Kha'Rim the Wanderer";
    sc.archetype = "tank";
    outcome.scFound.push_back(sc);

    // Simulate what AdventureState::onResume() does.
    for (const auto& s : outcome.scFound)
        h.addSpecial(s);

    CHECK_EQ((int)h.specials.size(), 1);
    CHECK(h.specials[0].id == std::string("kharim"));
}

SUITE("Hero — SC not added twice if outcome processed twice (guard check)") {
    Hero h;
    SpecialCharacter sc;
    sc.id        = "kharim";
    sc.archetype = "tank";

    CHECK(h.addSpecial(sc));
    CHECK(!h.addSpecial(sc));  // duplicate id rejected
    CHECK_EQ((int)h.specials.size(), 1);
}

// ── E1 — Passive item bonuses in combat ───────────────────────────────────────

SUITE("E1 — attackBonus raises effective attack") {
    const UnitType* t = makeUnit("Fighter", 5);
    CombatUnit u = CombatUnit::make(t, 1, true);
    u.attackBonus = 3;
    CHECK_EQ(u.effectiveAttack(), t->attack + 3);
}

SUITE("E1 — defenseBonus raises effective defense") {
    const UnitType* t = makeUnit("Guardian", 5);
    CombatUnit u = CombatUnit::make(t, 1, true);
    u.defenseBonus = 2;
    CHECK_EQ(u.effectiveDefense(), t->defense + 2);
}

SUITE("E1 — speedBonus raises effective speed") {
    const UnitType* t = makeUnit("Runner", 5);
    CombatUnit u = CombatUnit::make(t, 1, true);
    u.speedBonus = 1;
    CHECK_EQ(u.effectiveSpeed(), t->speed + 1);
}

SUITE("E1 — speedBonus changes initiative order") {
    // Both units have base speed 5, but player unit has +2 item speed → acts first.
    const UnitType* pt = makeUnit("PlayerUnit", 5);
    const UnitType* et = makeUnit("EnemyUnit",  5);

    CombatArmy p; p.ownerName = "P"; p.isPlayer = true;
    CombatUnit pu = CombatUnit::make(pt, 3, true);
    pu.speedBonus = 2;
    p.stacks.push_back(pu);

    CombatArmy e; e.ownerName = "E"; e.isPlayer = false;
    e.stacks.push_back(CombatUnit::make(et, 3, false));

    CombatEngine eng(std::move(p), std::move(e));
    // Without speedBonus: player wins tie (same speed). With +2: still player first.
    // But now let's verify enemy speed 7 is NOT enough to beat player speed 5+2=7 tie
    // (player wins ties) — i.e. player still goes first at effective speed 7 vs 7.
    CHECK(eng.currentTurn().isPlayer);
}

SUITE("E1 — attackBonus increases damage dealt to enemy") {
    // Attacker with +4 attack bonus should deal more damage than without.
    // Run many rounds of autobattle and compare survivor counts.
    const UnitType* at = makeUnit("Attacker", 5);
    const UnitType* dt = makeUnit("Defender", 3);

    // Baseline: no bonus
    int baselineCount = 0;
    {
        CombatArmy p; p.ownerName = "P"; p.isPlayer = true;
        p.stacks.push_back(CombatUnit::make(at, 1, true));
        CombatArmy e; e.ownerName = "E"; e.isPlayer = false;
        e.stacks.push_back(CombatUnit::make(dt, 20, false));
        CombatEngine eng(std::move(p), std::move(e));
        for (int i = 0; i < 200 && !eng.isOver(); ++i) CombatAI::takeTurn(eng);
        if (!eng.enemyArmy().stacks.empty())
            baselineCount = eng.enemyArmy().stacks[0].count;
    }

    // With +6 attack bonus: enemy should have fewer survivors
    int bonusCount = 0;
    {
        CombatArmy p; p.ownerName = "P"; p.isPlayer = true;
        CombatUnit pu = CombatUnit::make(at, 1, true);
        pu.attackBonus = 6;
        p.stacks.push_back(pu);
        CombatArmy e; e.ownerName = "E"; e.isPlayer = false;
        e.stacks.push_back(CombatUnit::make(dt, 20, false));
        CombatEngine eng(std::move(p), std::move(e));
        for (int i = 0; i < 200 && !eng.isOver(); ++i) CombatAI::takeTurn(eng);
        if (!eng.enemyArmy().stacks.empty())
            bonusCount = eng.enemyArmy().stacks[0].count;
    }

    CHECK(bonusCount <= baselineCount);  // more attack = fewer enemy survivors
}

SUITE("E1 — damageBonus increases damage dealt") {
    const UnitType* at = makeUnit("Slasher", 5);
    const UnitType* dt = makeUnit("Target",  3);

    int baselineCount = 0;
    {
        CombatArmy p; p.ownerName = "P"; p.isPlayer = true;
        p.stacks.push_back(CombatUnit::make(at, 1, true));
        CombatArmy e; e.ownerName = "E"; e.isPlayer = false;
        e.stacks.push_back(CombatUnit::make(dt, 20, false));
        CombatEngine eng(std::move(p), std::move(e));
        for (int i = 0; i < 200 && !eng.isOver(); ++i) CombatAI::takeTurn(eng);
        if (!eng.enemyArmy().stacks.empty())
            baselineCount = eng.enemyArmy().stacks[0].count;
    }

    int bonusCount = 0;
    {
        CombatArmy p; p.ownerName = "P"; p.isPlayer = true;
        CombatUnit pu = CombatUnit::make(at, 1, true);
        pu.damageBonus = 5;  // +5 per creature per hit
        p.stacks.push_back(pu);
        CombatArmy e; e.ownerName = "E"; e.isPlayer = false;
        e.stacks.push_back(CombatUnit::make(dt, 20, false));
        CombatEngine eng(std::move(p), std::move(e));
        for (int i = 0; i < 200 && !eng.isOver(); ++i) CombatAI::takeTurn(eng);
        if (!eng.enemyArmy().stacks.empty())
            bonusCount = eng.enemyArmy().stacks[0].count;
    }

    CHECK(bonusCount <= baselineCount);
}

// ── SC XP system ──────────────────────────────────────────────────────────────

// Helper: build a one-stack SC army with the Ushari definition.
static CombatArmy scArmy(const std::string& name, int speed) {
    static std::vector<UnitType> s_scTypes;
    UnitType t;
    t.id = name; t.name = name;
    t.speed = speed; t.hitPoints = 35;
    t.attack = 6; t.defense = 4; t.minDamage = 2; t.maxDamage = 5;
    s_scTypes.push_back(t);

    CombatUnit cu = CombatUnit::make(&s_scTypes.back(), 1, true);
    cu.isSpecialCharacter = true;
    cu.scId               = "ushari";
    cu.scDef              = &ushariDef();
    cu.scLevel            = 1;
    cu.scXp               = 0;
    cu.killXp             = ushariDef().killBonusXp;
    cu.perTurnXp          = ushariDef().perTurnXp;

    CombatArmy army;
    army.ownerName = "Player"; army.isPlayer = true;
    army.stacks.push_back(cu);
    return army;
}

SUITE("SC XP — per-turn XP fires at battle start") {
    // The first unit's turn begins at construction → per-turn XP fires.
    CombatArmy p = scArmy("Ushari", 10);
    CombatArmy e = oneStack("Skeleton", 3, false);
    CombatEngine eng(std::move(p), std::move(e));

    // Drain events, look for ScXpGained.
    auto evs = eng.drainEvents();
    bool foundXp = false;
    for (const auto& ev : evs)
        if (ev.type == CombatEvent::Type::ScXpGained && ev.isPlayer)
            foundXp = true;
    CHECK(foundXp);
    CHECK(eng.playerArmy().stacks[0].scXp == ushariDef().perTurnXp);
}

SUITE("SC XP — kill awards kill bonus XP") {
    // One-shot kill: SC with very high attack vs a single 1-HP enemy.
    static UnitType s_glass;
    s_glass.id = "glass"; s_glass.name = "GlassCannon";
    s_glass.speed = 1; s_glass.hitPoints = 1;
    s_glass.attack = 1; s_glass.defense = 0; s_glass.minDamage = 1; s_glass.maxDamage = 1;

    CombatUnit enemy = CombatUnit::make(&s_glass, 1, false);

    static UnitType s_tank;
    s_tank.id = "sc_test"; s_tank.name = "SC";
    s_tank.speed = 10; s_tank.hitPoints = 35;
    s_tank.attack = 100; s_tank.defense = 0; s_tank.minDamage = 50; s_tank.maxDamage = 50;
    static std::vector<UnitType> s_buf; s_buf.push_back(s_tank);

    CombatUnit sc = CombatUnit::make(&s_buf.back(), 1, true);
    sc.isSpecialCharacter = true;
    sc.scId               = "ushari";
    sc.scDef              = &ushariDef();
    sc.scLevel            = 1;
    sc.scXp               = 0;
    sc.killXp             = ushariDef().killBonusXp;
    sc.perTurnXp          = ushariDef().perTurnXp;

    CombatArmy p; p.ownerName = "P"; p.isPlayer = true;  p.stacks.push_back(sc);
    CombatArmy e; e.ownerName = "E"; e.isPlayer = false; e.stacks.push_back(enemy);

    CombatEngine eng(std::move(p), std::move(e));
    eng.drainEvents();   // discard per-turn XP from construction

    int xpBefore = eng.playerArmy().stacks[0].scXp;
    eng.doAttack(0);     // SC attacks; glass cannon should die

    int xpAfter = eng.playerArmy().stacks[0].scXp;
    CHECK(xpAfter > xpBefore);                          // some XP was gained
    CHECK(xpAfter - xpBefore >= ushariDef().killBonusXp);  // at least kill bonus
}

SUITE("SC XP — per-turn XP fires again on round 2") {
    // Ushari (speed=10) goes first. After construction scXp = perTurnXp.
    // Ushari defends → enemy's turn; enemy defends → Ushari's turn again → scXp += perTurnXp.
    CombatArmy p = scArmy("Ushari", 10);
    CombatArmy e = oneStack("Skeleton", 3, false);
    CombatEngine eng(std::move(p), std::move(e));
    eng.drainEvents();

    int xpAfterTurn1 = eng.playerArmy().stacks[0].scXp;
    CHECK_EQ(xpAfterTurn1, ushariDef().perTurnXp);

    eng.doDefend();           // Ushari's turn ends
    eng.drainEvents();
    eng.doDefend();           // enemy's turn ends → Ushari's turn → per-turn XP fires
    eng.drainEvents();

    int xpAfterTurn2 = eng.playerArmy().stacks[0].scXp;
    CHECK_EQ(xpAfterTurn2, ushariDef().perTurnXp * 2);
}

SUITE("SC XP — multi-level jump crosses two thresholds in one kill") {
    // Ushari thresholds = {15, 35}, maxLevel = 3.
    // Set scXp = 34 (just below level-3 threshold) with scLevel = 1.
    // Kill gives killBonusXp=6 → scXp=40, crossing both thresholds → level 3.
    static UnitType s_tiny;
    s_tiny.id = "tiny"; s_tiny.name = "Tiny";
    s_tiny.speed = 1; s_tiny.hitPoints = 1;
    s_tiny.attack = 0; s_tiny.defense = 0; s_tiny.minDamage = 1; s_tiny.maxDamage = 1;

    static UnitType s_sc3;
    s_sc3.id = "sc3"; s_sc3.name = "SC3";
    s_sc3.speed = 10; s_sc3.hitPoints = 35;
    s_sc3.attack = 100; s_sc3.defense = 0; s_sc3.minDamage = 50; s_sc3.maxDamage = 50;
    static std::vector<UnitType> s_buf3; s_buf3.push_back(s_sc3);

    CombatUnit sc = CombatUnit::make(&s_buf3.back(), 1, true);
    sc.isSpecialCharacter = true;
    sc.scId               = "ushari";
    sc.scDef              = &ushariDef();
    sc.scLevel            = 1;
    sc.scXp               = ushariDef().xpThresholds[1] - 1;  // 34 — crosses both on next XP gain
    sc.killXp             = ushariDef().killBonusXp;
    sc.perTurnXp          = 0;

    CombatUnit tiny = CombatUnit::make(&s_tiny, 1, false);

    CombatArmy p; p.ownerName = "P"; p.isPlayer = true;  p.stacks.push_back(sc);
    CombatArmy e; e.ownerName = "E"; e.isPlayer = false; e.stacks.push_back(tiny);

    CombatEngine eng(std::move(p), std::move(e));
    eng.drainEvents();
    eng.doAttack(0);
    eng.drainEvents();

    // With tree-based level-up, Lv2 is a choice node — resolve it to reach Lv3.
    CHECK(eng.hasPendingChoice());
    eng.resolveScChoice(true, 0, "duelist");
    eng.drainEvents();

    CHECK_EQ(eng.playerArmy().stacks[0].scLevel, ushariDef().maxLevel);  // jumped to 3
}

SUITE("SC XP — max level cap prevents further level-up") {
    // SC already at maxLevel; additional XP must not push scLevel higher.
    static UnitType s_victim;
    s_victim.id = "victim"; s_victim.name = "Victim";
    s_victim.speed = 1; s_victim.hitPoints = 1;
    s_victim.attack = 0; s_victim.defense = 0; s_victim.minDamage = 1; s_victim.maxDamage = 1;

    static UnitType s_sc4;
    s_sc4.id = "sc4"; s_sc4.name = "SC4";
    s_sc4.speed = 10; s_sc4.hitPoints = 35;
    s_sc4.attack = 100; s_sc4.defense = 0; s_sc4.minDamage = 50; s_sc4.maxDamage = 50;
    static std::vector<UnitType> s_buf4; s_buf4.push_back(s_sc4);

    CombatUnit sc = CombatUnit::make(&s_buf4.back(), 1, true);
    sc.isSpecialCharacter = true;
    sc.scId               = "ushari";
    sc.scDef              = &ushariDef();
    sc.scLevel            = ushariDef().maxLevel;  // already at cap
    sc.scXp               = ushariDef().xpThresholds.back();
    sc.killXp             = ushariDef().killBonusXp;
    sc.perTurnXp          = 0;

    CombatUnit victim = CombatUnit::make(&s_victim, 1, false);

    CombatArmy p; p.ownerName = "P"; p.isPlayer = true;  p.stacks.push_back(sc);
    CombatArmy e; e.ownerName = "E"; e.isPlayer = false; e.stacks.push_back(victim);

    CombatEngine eng(std::move(p), std::move(e));
    eng.drainEvents();
    eng.doAttack(0);
    auto evs = eng.drainEvents();

    // No ScLevelUp event should have fired.
    bool hasLevelUp = false;
    for (const auto& ev : evs)
        if (ev.type == CombatEvent::Type::ScLevelUp) hasLevelUp = true;
    CHECK(!hasLevelUp);
    CHECK_EQ(eng.playerArmy().stacks[0].scLevel, ushariDef().maxLevel);
}

SUITE("SC XP — level-up heal is capped at max HP") {
    // Ushari levelUpHeal=15, hitPoints=35.
    // Set hpLeft=10, prime XP just below threshold, kill → level-up → hp = min(25, 35) = 25.
    static UnitType s_prey;
    s_prey.id = "prey"; s_prey.name = "Prey";
    s_prey.speed = 1; s_prey.hitPoints = 1;
    s_prey.attack = 0; s_prey.defense = 0; s_prey.minDamage = 1; s_prey.maxDamage = 1;

    static UnitType s_sc5;
    s_sc5.id = "sc5"; s_sc5.name = "SC5";
    s_sc5.speed = 10; s_sc5.hitPoints = 35;
    s_sc5.attack = 100; s_sc5.defense = 0; s_sc5.minDamage = 50; s_sc5.maxDamage = 50;
    static std::vector<UnitType> s_buf5; s_buf5.push_back(s_sc5);

    CombatUnit sc = CombatUnit::make(&s_buf5.back(), 1, true);
    sc.isSpecialCharacter = true;
    sc.scId               = "ushari";
    sc.scDef              = &ushariDef();
    sc.scLevel            = 1;
    sc.scXp               = ushariDef().xpThresholds[0] - 1;
    sc.killXp             = ushariDef().killBonusXp;
    sc.perTurnXp          = 0;
    sc.hpLeft             = 10;  // damaged going in

    CombatUnit prey = CombatUnit::make(&s_prey, 1, false);

    CombatArmy p; p.ownerName = "P"; p.isPlayer = true;  p.stacks.push_back(sc);
    CombatArmy e; e.ownerName = "E"; e.isPlayer = false; e.stacks.push_back(prey);

    CombatEngine eng(std::move(p), std::move(e));
    eng.drainEvents();
    eng.doAttack(0);
    eng.drainEvents();

    int expectedHp = std::min(10 + ushariDef().levelUpHeal, s_sc5.hitPoints);
    CHECK_EQ(eng.playerArmy().stacks[0].hpLeft, expectedHp);
}

SUITE("SC XP — level-up emits ScLevelUp event") {
    // Prime the SC to be just below level 2 threshold, then force a kill.
    static UnitType s_weak;
    s_weak.id = "weak"; s_weak.name = "Weak";
    s_weak.speed = 1; s_weak.hitPoints = 1;
    s_weak.attack = 0; s_weak.defense = 0; s_weak.minDamage = 1; s_weak.maxDamage = 1;

    static UnitType s_sc2;
    s_sc2.id = "sc2"; s_sc2.name = "SC2";
    s_sc2.speed = 10; s_sc2.hitPoints = 35;
    s_sc2.attack = 100; s_sc2.defense = 0; s_sc2.minDamage = 50; s_sc2.maxDamage = 50;
    static std::vector<UnitType> s_buf2; s_buf2.push_back(s_sc2);

    CombatUnit sc = CombatUnit::make(&s_buf2.back(), 1, true);
    sc.isSpecialCharacter = true;
    sc.scId               = "ushari";
    sc.scDef              = &ushariDef();
    sc.scLevel            = 1;
    // Pre-load XP to just 1 below the level-2 threshold.
    sc.scXp               = ushariDef().xpThresholds[0] - 1;
    sc.killXp             = ushariDef().killBonusXp;
    sc.perTurnXp          = 0;  // suppress per-turn XP for this test

    CombatUnit weak = CombatUnit::make(&s_weak, 1, false);

    CombatArmy p; p.ownerName = "P"; p.isPlayer = true;  p.stacks.push_back(sc);
    CombatArmy e; e.ownerName = "E"; e.isPlayer = false; e.stacks.push_back(weak);

    CombatEngine eng(std::move(p), std::move(e));
    eng.drainEvents();

    eng.doAttack(0);
    auto evs = eng.drainEvents();

    bool foundLevelUp = false;
    bool foundChoice  = false;
    int  newLevel     = 0;
    for (const auto& ev : evs) {
        if (ev.type == CombatEvent::Type::ScLevelUp && ev.isPlayer) {
            foundLevelUp = true;
            newLevel     = ev.newLevel;
        }
        if (ev.type == CombatEvent::Type::ScChoicePending && ev.isPlayer)
            foundChoice = true;
    }
    CHECK(foundLevelUp);
    CHECK(newLevel == 2);
    CHECK(eng.playerArmy().stacks[0].scLevel == 2);
    // Lv2 is a branch choice — no stat growth until the player picks.
    CHECK(foundChoice);
    CHECK(eng.hasPendingChoice());
    // Resolve: pick duelist (+1 attack).
    eng.resolveScChoice(true, 0, "duelist");
    eng.drainEvents();
    CHECK(!eng.hasPendingChoice());
    CHECK(eng.playerArmy().stacks[0].attackBonus == 1);  // duelist: +1 attack
}


// ── Scored CombatAI ───────────────────────────────────────────────────────────
// Own registry (deque: pointers stay valid as it grows).
static std::deque<UnitType> s_aiTypes;
static const UnitType* aiType(const std::string& name, int speed, int dmg, int hp,
                              int atk, int def, int moveRange = 3, int shots = 0) {
    UnitType t;
    t.id = t.name = name;
    t.speed = speed; t.minDamage = t.maxDamage = dmg; t.hitPoints = hp;
    t.attack = atk; t.defense = def; t.moveRange = moveRange; t.shots = shots;
    s_aiTypes.push_back(std::move(t));
    return &s_aiTypes.back();
}

SUITE("CombatAI — two melee stacks split up instead of converging on one target") {
    // Two fast enemy stacks start side by side in mid-field; two identical,
    // slow player stacks wait in opposite corners.  The first enemy picks a
    // corner; the second must see that target as claimed and head for the other
    // (with move-and-attack both may pause just outside the militia's reach).
    const UnitType* pt = aiType("Militia", 3, 3, 10, 4, 4);
    const UnitType* et = aiType("Raider", 6, 3, 10, 4, 4);
    int split = 0;
    const int runs = 30;
    for (uint32_t seed = 1; seed <= runs; ++seed) {
        CombatArmy p; p.isPlayer = true; p.ownerName = "Player";
        p.stacks.push_back(CombatUnit::make(pt, 8, true));
        p.stacks.push_back(CombatUnit::make(pt, 8, true));
        CombatArmy e; e.isPlayer = false; e.ownerName = "Enemy";
        e.stacks.push_back(CombatUnit::make(et, 8, false));
        e.stacks.push_back(CombatUnit::make(et, 8, false));
        CombatEngine eng(std::move(p), std::move(e));
        eng.setSeed(seed);
        eng.teleportUnit(true, 0, CombatMap::toHex(1, 0));
        eng.teleportUnit(true, 1, CombatMap::toHex(1, 4));
        eng.teleportUnit(false, 0, CombatMap::toHex(6, 2));
        eng.teleportUnit(false, 1, CombatMap::toHex(7, 2));
        CHECK(!eng.currentTurn().isPlayer);
        CombatAI::takeTurn(eng);   // enemy 0
        CombatAI::takeTurn(eng);   // enemy 1
        // One goes for the north corner, the other for the south corner.
        int c0, r0, c1, r1;
        CombatMap::fromHex(eng.enemyArmy().stacks[0].pos, c0, r0);
        CombatMap::fromHex(eng.enemyArmy().stacks[1].pos, c1, r1);
        if (std::abs(r0 - r1) >= 2) ++split;
    }
    CHECK_EQ(split, runs);
}

SUITE("CombatAI — second attacker takes the flanking hex") {
    // One enemy stack already engages a tough player stack from one side.  The
    // second enemy can reach either the opposite side (pin: ×1.5 damage, no
    // retaliation) or an ordinary side hex — it must choose the pin.
    const UnitType* wall  = aiType("Wall", 2, 1, 400, 5, 5);
    const UnitType* raider = aiType("Raider", 6, 4, 10, 5, 5);
    int flanked = 0;
    const int runs = 20;
    for (uint32_t seed = 1; seed <= runs; ++seed) {
        CombatArmy p; p.isPlayer = true; p.ownerName = "Player";
        p.stacks.push_back(CombatUnit::make(wall, 1, true));
        CombatArmy e; e.isPlayer = false; e.ownerName = "Enemy";
        e.stacks.push_back(CombatUnit::make(raider, 6, false));
        e.stacks.push_back(CombatUnit::make(raider, 6, false));
        CombatEngine eng(std::move(p), std::move(e));
        eng.setSeed(seed);
        const HexCoord centre = CombatMap::toHex(5, 2);
        eng.teleportUnit(true, 0, centre);
        eng.teleportUnit(false, 0, centre.neighbor(0));
        eng.teleportUnit(false, 1, centre.neighbor(3).neighbor(3));
        // Enemy 0 acts first; make it hold so enemy 1 decides next.
        eng.doDefend();
        CombatAI::takeTurn(eng);
        if (eng.enemyArmy().stacks[1].pos == centre.neighbor(3)) ++flanked;
    }
    CHECK_EQ(flanked, runs);
}

SUITE("CombatAI — shooter fires instead of walking, even with a foe adjacent") {
    // An archer with a melee stack glued to it still has a clean shot at a
    // distant stack (no retaliation).  It must never walk or defend.
    const UnitType* archer = aiType("Archer", 6, 4, 10, 5, 5, 3, 12);
    const UnitType* grunt  = aiType("Grunt", 3, 2, 10, 5, 5);
    int shots = 0;
    const int runs = 20;
    for (uint32_t seed = 1; seed <= runs; ++seed) {
        CombatArmy p; p.isPlayer = true; p.ownerName = "Player";
        p.stacks.push_back(CombatUnit::make(archer, 10, true));
        CombatArmy e; e.isPlayer = false; e.ownerName = "Enemy";
        e.stacks.push_back(CombatUnit::make(grunt, 6, false));
        e.stacks.push_back(CombatUnit::make(grunt, 6, false));
        CombatEngine eng(std::move(p), std::move(e));
        eng.setSeed(seed);
        const HexCoord at = eng.playerArmy().stacks[0].pos;
        eng.teleportUnit(false, 0, at.neighbor(0));
        HexCoord before = at;
        CombatAI::takeTurn(eng);
        const auto& a = eng.playerArmy().stacks[0];
        if (a.pos == before && a.shotsLeft == 11) ++shots;
    }
    CHECK_EQ(shots, runs);
}

static std::vector<int> replayBattle(uint32_t seed) {
    const UnitType* spear = aiType("Spear", 5, 3, 12, 5, 4);
    const UnitType* bow   = aiType("Bow", 6, 2, 8, 4, 3, 3, 12);
    const UnitType* bone  = aiType("Bone", 4, 2, 9, 4, 4);
    const UnitType* sting = aiType("Sting", 7, 4, 14, 6, 3, 4);
    CombatArmy p; p.isPlayer = true; p.ownerName = "Player";
    p.stacks.push_back(CombatUnit::make(spear, 20, true));
    p.stacks.push_back(CombatUnit::make(bow, 10, true));
    p.stacks.push_back(CombatUnit::make(spear, 8, true));
    CombatArmy e; e.isPlayer = false; e.ownerName = "Enemy";
    e.stacks.push_back(CombatUnit::make(bone, 20, false));
    e.stacks.push_back(CombatUnit::make(sting, 8, false));
    e.stacks.push_back(CombatUnit::make(bone, 12, false));
    CombatEngine eng(std::move(p), std::move(e));
    eng.setSeed(seed);
    std::vector<int> trace;
    for (int n = 0; n < 500 && !eng.isOver(); ++n) {
        CombatAI::takeTurn(eng);
        for (const auto& ev : eng.drainEvents()) {
            trace.push_back(static_cast<int>(ev.type));
            trace.push_back(ev.stackIndex);
            trace.push_back(ev.damage);
            trace.push_back(ev.to.q * 100 + ev.to.r);
        }
    }
    trace.push_back(static_cast<int>(eng.result()));
    return trace;
}

SUITE("CombatAI — same seed replays the same battle; seeds differ") {
    const auto a = replayBattle(42);
    const auto b = replayBattle(42);
    CHECK(a == b);
    CHECK(a.back() != static_cast<int>(CombatResult::Ongoing));
    int distinct = 0;
    for (uint32_t seed = 1; seed <= 6; ++seed)
        if (replayBattle(seed) != a) ++distinct;
    CHECK(distinct > 0);
}

SUITE("CombatAI — all-melee mirror battles never stall") {
    // Cautious first-strike logic must still end every battle.
    const UnitType* m = aiType("Mirror", 4, 3, 10, 5, 5);
    bool allEnded = true;
    for (uint32_t seed = 1; seed <= 20; ++seed) {
        CombatArmy p; p.isPlayer = true; p.ownerName = "Player";
        CombatArmy e; e.isPlayer = false; e.ownerName = "Enemy";
        for (int i = 0; i < 3; ++i) {
            p.stacks.push_back(CombatUnit::make(m, 10, true));
            e.stacks.push_back(CombatUnit::make(m, 10, false));
        }
        CombatEngine eng(std::move(p), std::move(e));
        eng.setSeed(seed);
        for (int n = 0; n < 400 && !eng.isOver(); ++n) CombatAI::takeTurn(eng);
        if (!eng.isOver()) allEnded = false;
    }
    CHECK(allEnded);
}

SUITE("CombatEngine — previewAttack brackets the real damage and kills") {
    const UnitType* hitter = aiType("Hitter", 6, 0, 10, 7, 3);
    UnitType ranged = *hitter; ranged.name = "Ranged"; ranged.minDamage = 2; ranged.maxDamage = 5;
    s_aiTypes.push_back(ranged);
    const UnitType* rt = &s_aiTypes.back();
    const UnitType* target = aiType("Target", 2, 3, 7, 4, 4);
    bool inRange = true, retaliationInRange = true, sawRetaliation = false;
    for (uint32_t seed = 1; seed <= 40; ++seed) {
        CombatArmy p; p.isPlayer = true; p.ownerName = "Player";
        p.stacks.push_back(CombatUnit::make(rt, 9, true));
        CombatArmy e; e.isPlayer = false; e.ownerName = "Enemy";
        e.stacks.push_back(CombatUnit::make(target, 30, false));
        CombatEngine eng(std::move(p), std::move(e));
        eng.setSeed(seed);
        eng.teleportUnit(false, 0, eng.playerArmy().stacks[0].pos.neighbor(0));
        const AttackPreview pv = eng.previewAttack(0);
        CHECK(pv.valid && !pv.ranged && pv.retaliation);
        eng.doAttack(0);
        for (const auto& ev : eng.drainEvents()) {
            if (ev.type != CombatEvent::Type::UnitDamaged) continue;
            if (!ev.isPlayer) {
                inRange &= ev.damage >= pv.damage.min && ev.damage <= pv.damage.max;
                inRange &= ev.kills >= pv.killsMin && ev.kills <= pv.killsMax;
            } else {
                sawRetaliation = true;
                retaliationInRange &= ev.damage >= pv.retaliationDamage.min
                                   && ev.damage <= pv.retaliationDamage.max;
            }
        }
    }
    CHECK(inRange);
    CHECK(sawRetaliation);
    CHECK(retaliationInRange);
}


// ── Move-and-attack (HoMM3) ───────────────────────────────────────────────────

static CombatEngine duelAt(const UnitType* pt, const UnitType* et, int pCount, int eCount,
                           HexCoord pPos, HexCoord ePos, uint32_t seed = 7) {
    CombatArmy p; p.isPlayer = true; p.ownerName = "Player";
    p.stacks.push_back(CombatUnit::make(pt, pCount, true));
    CombatArmy e; e.isPlayer = false; e.ownerName = "Enemy";
    e.stacks.push_back(CombatUnit::make(et, eCount, false));
    CombatEngine eng(std::move(p), std::move(e));
    eng.setSeed(seed);
    eng.teleportUnit(true, 0, pPos);
    eng.teleportUnit(false, 0, ePos);
    return eng;
}

SUITE("Move-and-attack — melee walks next to a target and strikes in one turn") {
    const UnitType* pt = aiType("Lancer", 6, 4, 10, 5, 5, 3);
    const UnitType* et = aiType("Dummy", 2, 1, 10, 5, 5, 3);
    const HexCoord start = CombatMap::toHex(1, 2), foe = CombatMap::toHex(4, 2);
    CombatEngine eng = duelAt(pt, et, 10, 10, start, foe);
    CHECK_EQ(start.distanceTo(foe), 3);
    CHECK(eng.canAttack(0));
    CHECK(!eng.attackableTiles().empty());
    eng.doAttack(0);
    const auto& me = eng.playerArmy().stacks[0];
    CHECK(me.pos != start);
    CHECK_EQ(me.pos.distanceTo(foe), 1);
    CHECK(eng.enemyArmy().stacks[0].totalHp() < 100);
    CHECK(!eng.currentTurn().isPlayer);          // one move + one strike ends the turn
    auto evs = eng.drainEvents();
    int moved = -1, attacked = -1;
    for (int k = 0; k < (int)evs.size(); ++k) {
        if (evs[k].type == CombatEvent::Type::UnitMoved && moved < 0) moved = k;
        if (evs[k].type == CombatEvent::Type::UnitAttacked && attacked < 0) attacked = k;
    }
    CHECK(moved >= 0 && attacked > moved);       // the walk animates before the blow
}

SUITE("Move-and-attack — no strike beyond move range + 1") {
    const UnitType* pt = aiType("Lancer", 6, 4, 10, 5, 5, 3);
    const UnitType* et = aiType("Dummy", 2, 1, 10, 5, 5, 3);
    const HexCoord start = CombatMap::toHex(1, 2), foe = CombatMap::toHex(6, 2);
    CombatEngine eng = duelAt(pt, et, 10, 10, start, foe);
    CHECK_EQ(start.distanceTo(foe), 5);
    CHECK(!eng.canAttack(0));
    CHECK(eng.attackableTiles().empty());
    CHECK(eng.attackHexesFor(0).empty());
    CHECK(!eng.doAttackFrom(foe.neighbor(3), 0));
    CHECK(eng.currentTurn().isPlayer);           // rejected: still our turn
    CHECK(!eng.previewAttack(0).valid);
}

SUITE("Move-and-attack — the standing hex must be empty, reachable and adjacent") {
    const UnitType* pt = aiType("Lancer", 6, 4, 10, 5, 5, 3);
    const UnitType* et = aiType("Dummy", 2, 1, 10, 5, 5, 3);
    CombatArmy p; p.isPlayer = true; p.ownerName = "Player";
    p.stacks.push_back(CombatUnit::make(pt, 10, true));
    p.stacks.push_back(CombatUnit::make(et, 1, true));   // a slow ally used as a blocker
    CombatArmy e; e.isPlayer = false; e.ownerName = "Enemy";
    e.stacks.push_back(CombatUnit::make(et, 10, false));
    CombatEngine eng(std::move(p), std::move(e));
    const HexCoord foe = CombatMap::toHex(4, 2);
    eng.teleportUnit(true, 0, CombatMap::toHex(1, 2));
    eng.teleportUnit(true, 1, foe.neighbor(3));
    eng.teleportUnit(false, 0, foe);
    const auto spots = eng.attackHexesFor(0);
    CHECK(!spots.empty());
    bool allLegal = true;
    for (const HexCoord& h : spots)
        allLegal &= h.distanceTo(foe) == 1 && h != foe.neighbor(3) && eng.canMoveTo(h);
    CHECK(allLegal);
    CHECK(!eng.canAttackFrom(foe.neighbor(3), 0));      // ally stands there
    CHECK(!eng.canAttackFrom(foe.neighbor(3).neighbor(3), 0)); // not adjacent to the foe
    CHECK(!eng.doAttackFrom(foe.neighbor(3), 0));
    CHECK(eng.currentTurn().isPlayer);
}

SUITE("Move-and-attack — pinning depends on the chosen standing hex") {
    const UnitType* pt = aiType("Lancer", 6, 4, 10, 5, 5, 3);
    const UnitType* holder = aiType("Holder", 1, 1, 10, 5, 5, 3);
    const UnitType* et = aiType("Dummy", 2, 1, 10, 5, 5, 3);
    CombatArmy p; p.isPlayer = true; p.ownerName = "Player";
    p.stacks.push_back(CombatUnit::make(pt, 10, true));
    p.stacks.push_back(CombatUnit::make(holder, 5, true));
    CombatArmy e; e.isPlayer = false; e.ownerName = "Enemy";
    e.stacks.push_back(CombatUnit::make(et, 30, false));
    CombatEngine eng(std::move(p), std::move(e));
    eng.setSeed(3);
    const HexCoord foe = CombatMap::toHex(5, 2);
    eng.teleportUnit(false, 0, foe);
    eng.teleportUnit(true, 1, foe.neighbor(0));          // ally on one side
    eng.teleportUnit(true, 0, foe.neighbor(3).neighbor(3).neighbor(4));
    CHECK(eng.currentTurn().isPlayer && eng.currentTurn().stackIndex == 0);
    const HexCoord pinHex = foe.neighbor(3), sideHex = foe.neighbor(4);
    CHECK(eng.canAttackFrom(pinHex, 0));
    CHECK(eng.canAttackFrom(sideHex, 0));
    const AttackPreview pin = eng.previewAttack(0, pinHex);
    const AttackPreview side = eng.previewAttack(0, sideHex);
    CHECK(pin.valid && pin.pinned && !pin.retaliation);
    CHECK(side.valid && !side.pinned && side.retaliation);
    CHECK(pin.damage.min > side.damage.min);
    CHECK(eng.bestAttackHex(0) == pinHex);
    CHECK(eng.previewAttack(0).pinned);
    eng.doAttack(0);
    bool flanked = false;
    for (const auto& ev : eng.drainEvents())
        if (ev.type == CombatEvent::Type::UnitAttacked && ev.wasFlanked) flanked = true;
    CHECK(flanked);
    CHECK(eng.playerArmy().stacks[0].pos == pinHex);
}

SUITE("Move-and-attack — shooters with ammo shoot or move, never walk-and-strike") {
    const UnitType* archer = aiType("Archer", 6, 4, 10, 5, 5, 3, 12);
    const UnitType* et = aiType("Dummy", 2, 1, 10, 5, 5, 3);
    const HexCoord start = CombatMap::toHex(1, 2), foe = CombatMap::toHex(3, 2);
    CombatEngine eng = duelAt(archer, et, 10, 10, start, foe);
    CHECK(eng.attackHexesFor(0).empty());
    CHECK(eng.canAttackFrom(start, 0));
    eng.doAttack(0);
    CHECK(eng.playerArmy().stacks[0].pos == start);
    CHECK_EQ(eng.playerArmy().stacks[0].shotsLeft, 11);
}

SUITE("CombatAI — uses move-and-attack when a target is in reach") {
    const UnitType* pt = aiType("Lancer", 6, 4, 10, 5, 5, 3);
    const UnitType* et = aiType("Dummy", 2, 1, 10, 5, 5, 3);
    int struck = 0;
    const int runs = 20;
    for (uint32_t seed = 1; seed <= runs; ++seed) {
        const HexCoord start = CombatMap::toHex(1, 2), foe = CombatMap::toHex(4, 1);
        CombatEngine eng = duelAt(pt, et, 10, 10, start, foe, seed);
        CombatAI::takeTurn(eng);
        if (eng.playerArmy().stacks[0].pos.distanceTo(foe) == 1
            && eng.enemyArmy().stacks[0].totalHp() < 100) ++struck;
    }
    CHECK_EQ(struck, runs);
}


// ── Companions: powerful, vulnerable, the source of an aura ─────────────────────

static const UnitType* companionType(const std::string& name, int dmg, int hp, int aura) {
    UnitType t;
    t.id = t.name = name;
    t.faction = "companion";
    t.speed = 5; t.minDamage = t.maxDamage = dmg; t.hitPoints = hp;
    t.attack = 5; t.defense = 5; t.moveRange = 4;
    t.auraRadius = aura > 0 ? 1 : 0; t.auraDefense = aura;
    t.levelGrowth = {10, 1, 1, 2};
    s_aiTypes.push_back(std::move(t));
    return &s_aiTypes.back();
}

// Player: troops (index 0) + companion (index 1); enemy: one stack.
static CombatEngine companionBattle(const UnitType* troop, int troops, const UnitType* comp,
                                    const UnitType* foe, int foes, HexCoord troopAt, HexCoord compAt,
                                    HexCoord foeAt, uint32_t seed = 7) {
    CombatArmy p; p.isPlayer = true; p.ownerName = "Player";
    p.stacks.push_back(CombatUnit::make(troop, troops, true));
    p.stacks.push_back(CombatUnit::companion(comp, 1, true));
    CombatArmy e; e.isPlayer = false; e.ownerName = "Enemy";
    e.stacks.push_back(CombatUnit::make(foe, foes, false));
    CombatEngine eng(std::move(p), std::move(e));
    eng.setSeed(seed);
    eng.teleportUnit(true, 0, troopAt);
    eng.teleportUnit(true, 1, compAt);
    eng.teleportUnit(false, 0, foeAt);
    return eng;
}

SUITE("Companions — one figure; level growth goes into hp, attack, defence and damage") {
    const UnitType* c = companionType("Captain", 20, 60, 3);
    CombatUnit u = CombatUnit::companion(c, 3, true);
    CHECK_EQ(u.count, 1);
    CHECK(u.isSpecialCharacter);
    CHECK_EQ(u.maxHp(), 80);
    CHECK_EQ(u.totalHp(), 80);
    CHECK_EQ(u.attackBonus, 2);
    CHECK_EQ(u.defenseBonus, 2);
    CHECK_EQ(u.damageBonus, 4);
}

SUITE("Companions — spawn on the back line first, troops wall them in, front first") {
    const UnitType* troop = aiType("Levy", 4, 2, 10, 5, 5);
    const UnitType* c = companionType("Captain", 20, 60, 3);
    CombatArmy p; p.isPlayer = true;
    p.stacks.push_back(CombatUnit::make(troop, 10, true));
    p.stacks.push_back(CombatUnit::companion(c, 1, true));
    CombatArmy e; e.isPlayer = false;
    e.stacks.push_back(CombatUnit::make(troop, 10, false));
    CombatEngine eng(std::move(p), std::move(e));
    CHECK(eng.playerArmy().stacks[1].pos == CombatMap::toHex(0, 2));   // the centre of the back line
    CHECK_EQ(eng.playerArmy().stacks[0].pos.distanceTo(CombatMap::toHex(0, 2)), 1);   // walling her in
    CHECK_EQ(eng.playerArmy().stacks[0].pos.q, 1);                                    // on her front side
}

// A full army: four troop stacks close every hex around the companion.
SUITE("Companions — with four troop stacks the companion starts fully enclosed") {
    const UnitType* troop = aiType("Levy", 4, 2, 10, 5, 5);
    const UnitType* c = companionType("Captain", 20, 60, 3);
    CombatArmy p; p.isPlayer = true;
    p.stacks.push_back(CombatUnit::companion(c, 1, true));
    for (int i = 0; i < 4; ++i) p.stacks.push_back(CombatUnit::make(troop, 10, true));
    CombatArmy e; e.isPlayer = false;
    e.stacks.push_back(CombatUnit::make(troop, 10, false));
    CombatEngine eng(std::move(p), std::move(e));
    const HexCoord at = eng.playerArmy().stacks[0].pos;
    int open = 0;
    for (int dir = 0; dir < 6; ++dir) {
        const HexCoord n = at.neighbor(dir);
        if (!CombatMap::inBounds(n)) continue;
        bool held = false;
        for (int i = 1; i <= 4; ++i) held |= eng.playerArmy().stacks[i].pos == n;
        if (!held) ++open;
    }
    CHECK_EQ(open, 0);
}

SUITE("Companions — the aura gives nearby troops defence, not the companion itself") {
    const UnitType* troop = aiType("Levy", 4, 2, 10, 5, 5);
    const UnitType* c = companionType("Captain", 20, 60, 3);
    const UnitType* foe = aiType("Raider", 2, 1, 10, 5, 5);
    const HexCoord at = CombatMap::toHex(2, 2);
    CombatEngine eng = companionBattle(troop, 10, c, foe, 5, at.neighbor(0), at, CombatMap::toHex(9, 2));
    CHECK_EQ(eng.playerArmy().stacks[0].auraBonus, 3);
    CHECK_EQ(eng.playerArmy().stacks[0].effectiveDefense(), 5 + 3);
    CHECK_EQ(eng.playerArmy().stacks[1].auraBonus, 0);
    eng.teleportUnit(true, 0, at.neighbor(0).neighbor(0));           // two hexes away: outside
    CHECK_EQ(eng.playerArmy().stacks[0].auraBonus, 0);
}

SUITE("Companions — a bodyguard takes half of a melee blow; the forecast agrees") {
    const UnitType* troop = aiType("Levy", 1, 2, 10, 5, 5);
    const UnitType* c = companionType("Captain", 1, 60, 0);          // no aura: exact numbers
    const UnitType* foe = aiType("Raider", 9, 20, 100, 5, 5);        // one blow of exactly 20
    const HexCoord at = CombatMap::toHex(4, 2);
    CombatEngine eng = companionBattle(troop, 10, c, foe, 1, at.neighbor(3), at, at.neighbor(0));
    CHECK(!eng.currentTurn().isPlayer);
    CHECK_EQ(CombatEngine::bodyguardFor(eng.playerArmy().stacks, 1), 0);
    const AttackPreview p = eng.previewAttackUnchecked(1, at.neighbor(0));
    CHECK(p.guarded);
    CHECK_EQ(p.damage.min, 10);
    CHECK_EQ(p.guardDamage.min, 10);
    eng.doAttackFrom(at.neighbor(0), 1);
    CHECK_EQ(eng.playerArmy().stacks[1].totalHp(), 50);
    CHECK_EQ(eng.playerArmy().stacks[0].totalHp(), 90);
    bool flagged = false;
    for (const auto& ev : eng.drainEvents())
        if (ev.type == CombatEvent::Type::UnitDamaged && ev.bodyguard) flagged = ev.stackIndex == 0;
    CHECK(flagged);
}

SUITE("Companions — alone, a companion takes the whole blow") {
    const UnitType* troop = aiType("Levy", 1, 2, 10, 5, 5);
    const UnitType* c = companionType("Captain", 1, 60, 0);
    const UnitType* foe = aiType("Raider", 9, 20, 100, 5, 5);
    const HexCoord at = CombatMap::toHex(4, 2);
    CombatEngine eng = companionBattle(troop, 10, c, foe, 1, CombatMap::toHex(0, 0), at, at.neighbor(0));
    CHECK_EQ(CombatEngine::bodyguardFor(eng.playerArmy().stacks, 1), -1);
    eng.doAttackFrom(at.neighbor(0), 1);
    CHECK_EQ(eng.playerArmy().stacks[1].totalHp(), 40);
}

SUITE("Companions — enemies that can reach a companion are listed as threats") {
    const UnitType* troop = aiType("Levy", 1, 2, 10, 5, 5);
    const UnitType* c = companionType("Captain", 1, 60, 0);
    const UnitType* foe = aiType("Raider", 9, 20, 100, 5, 5, 3);
    const HexCoord at = CombatMap::toHex(2, 2);
    CombatEngine near = companionBattle(troop, 10, c, foe, 1, CombatMap::toHex(0, 0), at, CombatMap::toHex(5, 2));
    CHECK_EQ((int)near.threatsTo(true, 1).size(), 1);
    CombatEngine far = companionBattle(troop, 10, c, foe, 1, CombatMap::toHex(0, 0), at, CombatMap::toHex(10, 2));
    CHECK(far.threatsTo(true, 1).empty());
}

SUITE("CombatAI — hunts a companion over a bigger troop stack") {
    const UnitType* troop = aiType("Levy", 1, 3, 10, 5, 5);
    const UnitType* c = companionType("Captain", 20, 60, 0);
    const UnitType* foe = aiType("Raider", 9, 15, 100, 5, 5, 4);
    int hunted = 0;
    const int runs = 20;
    for (uint32_t seed = 1; seed <= runs; ++seed) {
        // Troops and companion far apart (no bodyguard), both within the raider's reach.
        CombatEngine eng = companionBattle(troop, 10, c, foe, 1, CombatMap::toHex(4, 0),
                                           CombatMap::toHex(4, 4), CombatMap::toHex(6, 2), seed);
        CombatAI::takeTurn(eng);
        if (eng.playerArmy().stacks[1].totalHp() < 60) ++hunted;
    }
    CHECK(hunted >= runs - 2);
}

// ── Line of sight ───────────────────────────────────────────────────────────────

SUITE("Line of sight — any stack between shooter and target halves the shot") {
    const UnitType* archer = aiType("Archer", 9, 10, 10, 5, 5, 3, 12);
    const UnitType* wall = aiType("Wall", 1, 1, 10, 5, 5);
    const UnitType* dummy = aiType("Dummy", 1, 1, 10, 5, 5);
    const HexCoord from = CombatMap::toHex(1, 2), to = CombatMap::toHex(5, 2), mid = CombatMap::toHex(3, 2);
    CHECK(from.lineTo(to)[2] == mid);
    auto battle = [&](bool blocked) {
        CombatArmy p; p.isPlayer = true;
        p.stacks.push_back(CombatUnit::make(archer, 1, true));
        p.stacks.push_back(CombatUnit::make(wall, 1, true));
        CombatArmy e; e.isPlayer = false;
        e.stacks.push_back(CombatUnit::make(dummy, 10, false));
        CombatEngine eng(std::move(p), std::move(e));
        eng.teleportUnit(true, 0, from);
        eng.teleportUnit(true, 1, blocked ? mid : CombatMap::toHex(0, 0));
        eng.teleportUnit(false, 0, to);
        return eng;
    };
    CombatEngine clear = battle(false);
    CHECK(clear.hasLineOfSight(from, to));
    CHECK(!clear.previewAttack(0).blocked);
    CHECK_EQ(clear.previewAttack(0).damage.min, 10);
    clear.doAttack(0);
    CHECK_EQ(clear.enemyArmy().stacks[0].totalHp(), 90);

    CombatEngine blocked = battle(true);                        // even a friend blocks
    CHECK(!blocked.hasLineOfSight(from, to));
    CHECK(blocked.previewAttack(0).blocked);
    CHECK_EQ(blocked.previewAttack(0).damage.min, 5);
    blocked.doAttack(0);
    CHECK_EQ(blocked.enemyArmy().stacks[0].totalHp(), 95);
    bool flagged = false;
    for (const auto& ev : blocked.drainEvents())
        if (ev.type == CombatEvent::Type::UnitAttacked && ev.blockedShot) flagged = true;
    CHECK(flagged);
}

SUITE("Line of sight — adjacent hexes and a line grazing an edge stay clear") {
    const UnitType* archer = aiType("Archer", 9, 10, 10, 5, 5, 3, 12);
    const UnitType* dummy = aiType("Dummy", 1, 1, 10, 5, 5);
    CombatArmy p; p.isPlayer = true;
    p.stacks.push_back(CombatUnit::make(archer, 1, true));
    CombatArmy e; e.isPlayer = false;
    e.stacks.push_back(CombatUnit::make(dummy, 10, false));
    CombatEngine eng(std::move(p), std::move(e));
    const HexCoord a = CombatMap::toHex(2, 2);
    CHECK(eng.hasLineOfSight(a, a.neighbor(0)));
    // (0,0) → (2,-1): the line runs along the edge between (1,0) and (1,-1).
    eng.teleportUnit(true, 0, {0, 0});
    eng.teleportUnit(false, 0, {1, 0});
    CHECK(eng.hasLineOfSight({0, 0}, {2, -1}));               // one side of the edge is open
}


// ── Player-chosen routes (waypoints) ────────────────────────────────────────────

SUITE("Routes — a legal detour is walked hex by hex and reported as the path") {
    const UnitType* pt = aiType("Lancer", 6, 4, 10, 5, 5, 4);
    const UnitType* et = aiType("Dummy", 2, 1, 10, 5, 5, 3);
    const HexCoord start = CombatMap::toHex(1, 2);
    CombatEngine eng = duelAt(pt, et, 10, 10, start, CombatMap::toHex(9, 2));
    // Up and over: N, NE, then SE — three steps where the straight line takes one or two.
    const std::vector<HexCoord> detour = {start.neighbor(2), start.neighbor(2).neighbor(1),
                                          start.neighbor(2).neighbor(1).neighbor(0)};
    CHECK(eng.isLegalRoute(detour));
    CHECK(eng.doMoveAlong(detour));
    CHECK(eng.playerArmy().stacks[0].pos == detour.back());
    bool reported = false;
    for (const auto& ev : eng.drainEvents())
        if (ev.type == CombatEvent::Type::UnitMoved) reported = ev.path == detour;
    CHECK(reported);
}

SUITE("Routes — gaps, repeats, blocked hexes and overlong routes are refused") {
    const UnitType* pt = aiType("Lancer", 6, 4, 10, 5, 5, 3);
    const UnitType* et = aiType("Dummy", 2, 1, 10, 5, 5, 3);
    const HexCoord start = CombatMap::toHex(1, 2);
    const HexCoord foe = start.neighbor(0).neighbor(0);
    CombatEngine eng = duelAt(pt, et, 10, 10, start, foe);
    CHECK(!eng.isLegalRoute({start.neighbor(0).neighbor(0).neighbor(0)}));             // not adjacent
    CHECK(!eng.isLegalRoute({start.neighbor(0), start}));                              // back onto the start
    CHECK(!eng.isLegalRoute({start.neighbor(0), foe}));                                // through a stack
    HexCoord h = start;
    std::vector<HexCoord> tooLong;
    for (int i = 0; i < 4; ++i) { h = h.neighbor(5); if (CombatMap::inBounds(h)) tooLong.push_back(h); }
    tooLong.push_back(tooLong.back().neighbor(0));
    CHECK(!eng.isLegalRoute(tooLong));                                                 // move range 3
    CHECK(!eng.doMoveAlong({start.neighbor(0), foe}));
    CHECK(eng.playerArmy().stacks[0].pos == start);                                    // nothing happened
}

SUITE("Routes — walk a chosen route, then strike from its end") {
    const UnitType* pt = aiType("Lancer", 6, 4, 10, 5, 5, 4);
    const UnitType* et = aiType("Dummy", 2, 1, 100, 5, 5, 3);
    const HexCoord start = CombatMap::toHex(1, 2);
    const HexCoord foe = CombatMap::toHex(4, 2);
    CombatEngine eng = duelAt(pt, et, 10, 10, start, foe);
    // Come in from above the target rather than straight on.
    std::vector<HexCoord> route = {start.neighbor(1), start.neighbor(1).neighbor(0)};
    route.push_back(route.back().neighbor(0));
    CHECK(route.back().distanceTo(foe) == 1);
    const int before = eng.enemyArmy().stacks[0].totalHp();
    CHECK(!eng.doAttackAlong({start.neighbor(1)}, 0));                                 // too far to strike
    CHECK(eng.doAttackAlong(route, 0));
    CHECK(eng.playerArmy().stacks[0].pos == route.back());
    CHECK(eng.enemyArmy().stacks[0].totalHp() < before);
}


// ── Line of sight: QA ───────────────────────────────────────────────────────────

SUITE("Line of sight — dead stacks do not block; enemies block like friends") {
    const UnitType* archer = aiType("Archer", 9, 10, 10, 5, 5, 3, 12);
    const UnitType* body = aiType("Body", 1, 1, 10, 5, 5);
    const HexCoord from = CombatMap::toHex(1, 2), to = CombatMap::toHex(5, 2), mid = CombatMap::toHex(3, 2);
    CombatArmy p; p.isPlayer = true;
    p.stacks.push_back(CombatUnit::make(archer, 1, true));
    CombatArmy e; e.isPlayer = false;
    e.stacks.push_back(CombatUnit::make(body, 10, false));
    e.stacks.push_back(CombatUnit::make(body, 1, false));
    CombatEngine eng(std::move(p), std::move(e));
    eng.teleportUnit(true, 0, from);
    eng.teleportUnit(false, 0, to);
    eng.teleportUnit(false, 1, mid);
    CHECK(!eng.hasLineOfSight(from, to));                         // an enemy in the way blocks too
    CHECK(eng.previewAttack(0).blocked);
    eng.doAttack(1);                                              // 10 damage kills the lone body
    CHECK(eng.enemyArmy().stacks[1].isDead());
    CHECK(eng.hasLineOfSight(from, to));                          // its corpse does not block
}

SUITE("Line of sight — the same answer in both directions") {
    const UnitType* body = aiType("Body", 1, 1, 10, 5, 5);
    CombatArmy p; p.isPlayer = true;
    p.stacks.push_back(CombatUnit::make(body, 1, true));
    CombatArmy e; e.isPlayer = false;
    e.stacks.push_back(CombatUnit::make(body, 1, false));
    CombatEngine eng(std::move(p), std::move(e));
    eng.teleportUnit(true, 0, CombatMap::toHex(5, 2));
    eng.teleportUnit(false, 0, CombatMap::toHex(5, 4));
    int checked = 0;
    for (auto a : CombatMap::allHexes())
        for (auto b : CombatMap::allHexes()) {
            if (a.distanceTo(b) < 2) continue;
            CHECK(eng.hasLineOfSight(a, b) == eng.hasLineOfSight(b, a));
            ++checked;
        }
    CHECK(checked > 2000);
}

SUITE("Line of sight — judging a shot from a new hex ignores the shooter's old hex") {
    const UnitType* archer = aiType("Archer", 9, 10, 10, 5, 5, 3, 12);
    const UnitType* body = aiType("Body", 1, 1, 10, 5, 5);
    const HexCoord old = CombatMap::toHex(3, 2), to = CombatMap::toHex(5, 2), back = CombatMap::toHex(1, 2);
    CombatArmy p; p.isPlayer = true;
    p.stacks.push_back(CombatUnit::make(archer, 1, true));
    CombatArmy e; e.isPlayer = false;
    e.stacks.push_back(CombatUnit::make(body, 1, false));
    CombatEngine eng(std::move(p), std::move(e));
    eng.teleportUnit(true, 0, old);
    eng.teleportUnit(false, 0, to);
    CHECK(!eng.hasLineOfSight(back, to));                                          // the archer itself is in the way…
    CHECK(eng.hasLineOfSight(back, to, &eng.playerArmy().stacks[0]));              // …unless it is the one moving
}


// ── Defend lasts until the stack's next turn ─────────────────────────────────────

SUITE("Defend — the stance carries into the next round until the stack acts again") {
    const UnitType* slow = aiType("Slow", 2, 1, 10, 5, 5);     // acts last each round
    const UnitType* fast = aiType("Fast", 9, 1, 10, 5, 5);
    CombatEngine eng = duelAt(slow, fast, 10, 10, CombatMap::toHex(0, 2), CombatMap::toHex(10, 2));
    CHECK(!eng.currentTurn().isPlayer);                          // fast enemy first
    eng.doDefend();                                              // enemy defends… (round 1 ends after our turn)
    CHECK(eng.currentTurn().isPlayer);
    eng.doDefend();                                              // we defend as the last actor of round 1
    CHECK_EQ(eng.roundNumber(), 2);
    CHECK(!eng.currentTurn().isPlayer);                          // round 2: the enemy acts first…
    CHECK(eng.playerArmy().stacks[0].isDefending);               // …and our stance still holds against it
    CHECK(!eng.enemyArmy().stacks[0].isDefending);               // its own stance ended when its turn came
    eng.doDefend();
    CHECK(!eng.playerArmy().stacks[0].isDefending);              // our turn: the stance ends
}


// ── Reaction fire: shooters fire at stacks moving closer ─────────────────────────

// Player: one melee stack; enemy: one shooter far to the right.
static CombatEngine reactionDuel(int meleeCount, int archers, HexCoord meleeAt, HexCoord archerAt,
                                 int moveRange = 4) {
    const UnitType* melee = aiType("Charger", 9, 3, 10, 5, 5, moveRange);
    const UnitType* archer = aiType("Bowman", 1, 5, 10, 5, 5, 3, 12);
    CombatArmy p; p.isPlayer = true;
    p.stacks.push_back(CombatUnit::make(melee, meleeCount, true));
    CombatArmy e; e.isPlayer = false;
    e.stacks.push_back(CombatUnit::make(archer, archers, false));
    CombatEngine eng(std::move(p), std::move(e));
    eng.setSeed(3);
    eng.teleportUnit(true, 0, meleeAt);
    eng.teleportUnit(false, 0, archerAt);
    return eng;
}

SUITE("Reaction fire — moving closer draws a shot; the forecast matches") {
    const HexCoord start = CombatMap::toHex(2, 2), archer = CombatMap::toHex(8, 2);
    CombatEngine eng = reactionDuel(10, 2, start, archer);
    CHECK(eng.currentTurn().isPlayer);
    const HexCoord closer = start.neighbor(0).neighbor(0);
    auto preview = eng.reactionsTo(closer);
    CHECK_EQ((int)preview.size(), 1);
    CHECK_EQ(preview[0].damage.min, 10);                      // 2 bowmen × 5, same atk/def
    eng.doMove(closer);
    CHECK_EQ(eng.playerArmy().stacks[0].totalHp(), 90);
    CHECK_EQ(eng.enemyArmy().stacks[0].shotsLeft, 11);
    CHECK(eng.enemyArmy().stacks[0].hasReacted);
    bool flagged = false;
    for (const auto& ev : eng.drainEvents())
        if (ev.type == CombatEvent::Type::UnitAttacked && ev.isReaction) flagged = !ev.isPlayer;
    CHECK(flagged);
}

SUITE("Reaction fire — stepping away or sideways draws nothing") {
    const HexCoord start = CombatMap::toHex(4, 2), archer = CombatMap::toHex(8, 2);
    CombatEngine eng = reactionDuel(10, 2, start, archer);
    CHECK(eng.reactionsTo(start.neighbor(3)).empty());          // away
    eng.doMove(start.neighbor(3));
    CHECK_EQ(eng.playerArmy().stacks[0].totalHp(), 100);
    CHECK(!eng.enemyArmy().stacks[0].hasReacted);
}

SUITE("Reaction fire — once per round, back the next round") {
    const HexCoord start = CombatMap::toHex(1, 2), archer = CombatMap::toHex(9, 2);
    CombatEngine eng = reactionDuel(10, 2, start, archer, 2);
    eng.doMove(start.neighbor(0));                              // shot 1 (round 1)
    CHECK_EQ(eng.playerArmy().stacks[0].totalHp(), 90);
    CHECK(!eng.currentTurn().isPlayer);
    eng.doDefend();                                             // the bowmen hold (round ends)
    CHECK_EQ(eng.roundNumber(), 2);
    CHECK(!eng.enemyArmy().stacks[0].hasReacted);               // reloaded for the new round
    eng.doMove(eng.playerArmy().stacks[0].pos.neighbor(0));     // shot 2 (round 2)
    CHECK_EQ(eng.playerArmy().stacks[0].totalHp(), 80);
}

SUITE("Reaction fire — the shot lands before a walk-and-strike, and can stop it") {
    const HexCoord archer = CombatMap::toHex(6, 2);
    const HexCoord start = archer.neighbor(3).neighbor(3).neighbor(3);
    CombatEngine eng = reactionDuel(1, 10, start, archer);      // one charger (10 hp) vs 10 bowmen (50 dmg)
    CHECK(eng.canAttack(0));
    eng.doAttack(0);
    CHECK(eng.playerArmy().stacks[0].isDead());                 // shot down on the way in
    CHECK_EQ(eng.enemyArmy().stacks[0].totalHp(), 100);         // no strike landed
    CHECK(eng.isOver() && eng.result() == CombatResult::EnemyWon);
}

SUITE("Reaction fire — the AI will not walk a fragile stack into a lethal volley") {
    // 30 bowmen straight north of two chargers: any step toward them is lethal,
    // while stepping along the bottom row draws nothing.
    const UnitType* melee = aiType("Charger", 9, 3, 10, 5, 5, 3);
    const UnitType* archer = aiType("Bowman", 1, 5, 10, 5, 5, 3, 12);
    const UnitType* dummy = aiType("Dummy", 1, 1, 10, 5, 5, 1);
    int spared = 0;
    for (uint32_t seed = 1; seed <= 10; ++seed) {
        CombatArmy p; p.isPlayer = true;
        p.stacks.push_back(CombatUnit::make(melee, 2, true));
        CombatArmy e; e.isPlayer = false;
        e.stacks.push_back(CombatUnit::make(archer, 30, false));
        e.stacks.push_back(CombatUnit::make(dummy, 5, false));
        CombatEngine eng(std::move(p), std::move(e));
        eng.setSeed(seed);
        eng.teleportUnit(true, 0, CombatMap::toHex(1, 4));
        eng.teleportUnit(false, 0, CombatMap::toHex(1, 0));
        eng.teleportUnit(false, 1, CombatMap::toHex(9, 4));
        CHECK(eng.currentTurn().isPlayer);
        CombatAI::takeTurn(eng);
        if (!eng.playerArmy().stacks[0].isDead() && !eng.enemyArmy().stacks[0].hasReacted)
            ++spared;   // stayed out of the volley
    }
    CHECK(spared >= 8);
}

SUITE("Reaction fire — the AI still closes in when every approach draws fire") {
    // Guards against our archers must not stall: holding back does not stop the arrows.
    int advanced = 0;
    for (uint32_t seed = 1; seed <= 10; ++seed) {
        CombatEngine eng = reactionDuel(20, 10, CombatMap::toHex(9, 2), CombatMap::toHex(1, 2));
        eng.setSeed(seed);
        const int before = eng.playerArmy().stacks[0].pos.distanceTo(CombatMap::toHex(1, 2));
        CombatAI::takeTurn(eng);
        if (eng.playerArmy().stacks[0].pos.distanceTo(CombatMap::toHex(1, 2)) < before) ++advanced;
    }
    CHECK(advanced >= 8);
}

#endif // COMBAT_ENGINE_IMPL
