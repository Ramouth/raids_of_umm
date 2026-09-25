#pragma once
#include "CombatArmy.h"
#include "CombatEvent.h"
#include "CombatMap.h"
#include <cstdint>
#include <optional>
#include <random>
#include <vector>

/*
 * TurnSlot — identifies which stack in which army is currently acting.
 */
struct TurnSlot {
    bool isPlayer;
    int  stackIndex;   // index into CombatArmy::stacks
};

/*
 * DamageRange — the exact damage bounds of one strike, plus its expected value.
 * Mirrors CombatEngine::calcDamage so previews never disagree with real rolls.
 */
struct DamageRange {
    int    min = 0;
    int    max = 0;
    double avg = 0.0;
};

/*
 * AttackPreview — what the active stack's attack on one enemy stack would do.
 * Used by the UI hover preview and by CombatAI's scoring.
 *
 * Kill ranges are creatures killed (min from the low roll, max from the high).
 * Retaliation is the target's counter-strike after surviving the hit; its
 * range spans "target took the high roll" .. "target took the low roll".
 */
struct AttackPreview {
    bool        valid       = false;  // target exists, alive, and legally attackable now
    bool        ranged      = false;  // shot (no retaliation) vs melee strike
    bool        pinned      = false;  // flanked: +50% damage, no retaliation
    bool        blocked     = false;  // shot with no clear line of sight: half damage
    DamageRange damage;
    int         killsMin    = 0;
    int         killsMax    = 0;
    bool        retaliation = false;  // the target can strike back (if it survives)
    DamageRange retaliationDamage;
    int         retKillsMin = 0;
    int         retKillsMax = 0;
    // A melee blow on a companion is split with an adjacent friendly stack:
    // damage/kills above are the companion's share; the bodyguard takes guardDamage.
    bool        guarded     = false;
    DamageRange guardDamage;
    bool        retaliationGuarded = false;  // same, for the counter-strike on our companion
};

/*
 * CombatEngine — pure combat logic.
 *
 * Owns two armies and a CombatMap.  Drives turn order via an initiative
 * queue sorted by UnitType::speed.  Places units on spawn hexes at startup.
 * Provides reachable- and attackable-hex queries for highlighting.
 *
 * Action methods are stubs for this pass; damage resolution and ability
 * tags will be added in the next iteration.
 *
 * No GL, no SDL, no render types — fully unit-testable via raids_tests.
 */
class CombatEngine {
public:
    CombatEngine(CombatArmy player, CombatArmy enemy);

    // Reseed damage rolls and the AI's tie-break RNG.  Same seed + same
    // commands ⇒ identical battle (tests, replays, balance sims).
    // Without a call both streams are seeded from std::random_device.
    void setSeed(uint32_t seed);

    // RNG reserved for CombatAI decisions (separate stream so AI jitter never
    // shifts damage rolls).
    std::mt19937& aiRng() { return m_aiRng; }

    struct Fieldwork { HexCoord cell; bool opaque = true; };
    const std::vector<Fieldwork>& fieldworks() const { return m_fieldworks; }
    bool placeFieldwork(HexCoord cell, bool opaque);
    bool removeFieldwork(HexCoord cell);
    bool fieldworkAt(HexCoord cell) const;
    bool clearTerrainSight(HexCoord from, HexCoord to) const;

    // ── Queries ──────────────────────────────────────────────────────────────

    CombatResult      result()      const { return m_result; }
    bool              isOver()      const { return m_result != CombatResult::Ongoing; }
    const TurnSlot&   currentTurn() const { return m_queue[m_turn]; }
    const CombatArmy& playerArmy()  const { return m_player; }
    const CombatArmy& enemyArmy()   const { return m_enemy; }
    const CombatMap&  map()         const { return m_map; }
    int               roundNumber() const { return m_round; }
    const std::vector<TurnSlot>& turnOrder() const { return m_queue; }
    int               turnIndex() const { return m_turn; }

    // Returns the unit currently acting.
    const CombatUnit& activeUnit() const;

    // Hexes the active unit can move to this turn (within moveRange, not occupied by friendlies).
    std::vector<HexCoord> reachableTiles()  const;

    // Hexes of every enemy the active unit can attack this turn: anything a
    // shooter with ammo can see, plus (HoMM3 move-and-attack) any enemy a
    // melee stack can walk next to within its move range.
    std::vector<HexCoord> attackableTiles() const;

    // Legal standing hexes for the active unit to strike enemy `targetIndex`
    // from: its current hex if already adjacent, plus every reachable empty
    // hex adjacent to the target.  Shooters with ammo never walk-and-strike,
    // so for them this is only the current hex (when adjacent).
    std::vector<HexCoord> attackHexesFor(int targetIndex) const;

    // True if the active unit may attack enemy `targetIndex` standing on `from`
    // (a shooter with ammo may also shoot from its current hex at any range).
    bool canAttackFrom(HexCoord from, int targetIndex) const;

    // The standing hex doAttack() would pick: current hex if adjacent (or a
    // shooter with ammo), otherwise a pinning hex if any, then the shortest walk.
    // Returns the current hex when the target cannot be reached.
    HexCoord bestAttackHex(int targetIndex) const;

    // True if the active unit could attack the enemy stack at targetIndex this
    // turn (shot with ammo, adjacent strike, or walk-then-strike).
    bool canAttack(int targetIndex) const;

    // Preview of the active unit attacking enemy stack targetIndex from
    // bestAttackHex() (see AttackPreview).
    AttackPreview previewAttack(int targetIndex) const;
    // Same, standing on `from` (pinning depends on where the attacker stands).
    AttackPreview previewAttack(int targetIndex, HexCoord from) const;
    // previewAttack(target, from) without the legality check (no path search):
    // only for hexes already taken from attackHexesFor()/the current hex.
    AttackPreview previewAttackUnchecked(int targetIndex, HexCoord from) const;

    // Damage bounds for `attacker` striking `defender` with the current stats
    // (defending bonus, item bonuses, bypass).  `pinned` applies the ×1.5.
    static DamageRange damageRange(const CombatUnit& attacker, const CombatUnit& defender,
                                   bool pinned = false);

    // HoMM3: shooters deal half damage hand to hand (their strikes and their
    // retaliation), unless their type has "no_melee_penalty".
    static bool meleePenalty(const CombatUnit& u) {
        return u.type->isRanged() && !u.type->hasAbility("no_melee_penalty");
    }
    // damageRange() for a melee blow: halved for a shooter.
    static DamageRange meleeRange(const CombatUnit& attacker, const CombatUnit& defender, bool pinned = false);

    // Creatures that `damage` would kill in `target` (cascading through the stack).
    static int killsFor(const CombatUnit& target, int damage);

    // ── Readied shot (reaction fire) ─────────────────────────────────────────
    // An advanced ability, not every archer's: a shooter with readiedShot (or
    // the "readied_shot" ability) fires once per round at an enemy stack that
    // ends a move closer to it — before a walk-and-strike lands.  It must have
    // ammo and not already be engaged by someone else; line of sight applies.
    static constexpr double kReactionFactor = 1.0;
    struct ReactionPreview { int shooter = -1; DamageRange damage; bool blocked = false; bool opportunity = false; };
    // Shots the active stack would draw by walking from its hex to `to`.
    std::vector<ReactionPreview> reactionsTo(HexCoord to) const;
    static bool hasReadiedShot(const CombatUnit& u) { return u.readiedShot || u.type->hasAbility("readied_shot"); }

    // ── Opportunity strike ───────────────────────────────────────────────────
    // A guardian ("opportunity_strike" ability) strikes, once per round, an
    // enemy stack that steps out of its reach: starts a step next to it and
    // ends that step not next to it. The blow lands before the step (a stack
    // it kills never gets away) and draws no retaliation.
    static bool hasOpportunityStrike(const CombatUnit& u) { return u.type->hasAbility("opportunity_strike"); }
    // Guardians the active stack would provoke by walking `route` (hexes after
    // its own, ending on the destination; empty = straight to `to`).
    std::vector<int> opportunityStrikers(HexCoord to, const std::vector<HexCoord>& route = {}) const;

    // ── The Cruths: glory fought alone ───────────────────────────────────────
    // lone_wolf: +25% damage when no living friendly stack stands next to the
    //   attacker's hex (they fight as individuals, not as a line).
    // renown / great_renown: destroying an enemy stack paints a new stripe,
    //   +2 / +4 attack for the rest of the battle.
    // strike_and_return: a stack that walked in to strike goes back to where
    //   it started, if that hex is still free (hit and run).
    static constexpr int kRenownGain = 2;
    static bool fightsAlone(const std::vector<CombatUnit>& friends, int self, HexCoord at);
    static int  loneWolfDamage(int damage) { return damage + damage / 4; }

    // ── Engaged shooters (HoMM3) ─────────────────────────────────────────────
    // A shooter with a living enemy next to it cannot shoot: it must deal
    // with that enemy in melee first (and fires no reaction shots).
    bool isEngaged(const CombatUnit& u) const;
    // Has ammo and is not engaged.
    bool canShoot(const CombatUnit& u) const;

    // ── Line of sight ────────────────────────────────────────────────────────
    // A shot needs a clear line: any living stack (friend or foe) on a hex
    // between shooter and target blocks it, and a blocked shot does half damage.
    // The line is clear if either side-nudged line is (edge-grazing is fair).
    // `mover` (if given) is treated as standing on `from` instead of its own
    // hex — for judging a shot from a hex the unit has not walked to yet.
    bool hasLineOfSight(HexCoord from, HexCoord to, const CombatUnit* mover = nullptr) const;

    // ── Companions ───────────────────────────────────────────────────────────

    // Defence a friendly aura gives stack `self` of `friends` standing on pos
    // (the strongest aura in range; auras do not stack, nor apply to their source).
    static int auraAt(const std::vector<CombatUnit>& friends, int self, HexCoord pos);

    // The stack that shields companion `self` from a melee blow: the adjacent
    // living non-companion friend with the most HP, or -1.
    static int bodyguardFor(const std::vector<CombatUnit>& friends, int self);

    // Enemy stacks that could attack stack `index` of the given side on their
    // next turn: shooters with ammo and a clear line of sight, and melee stacks
    // that can walk next to it.
    std::vector<int> threatsTo(bool isPlayer, int index) const;

    // ── Movement ─────────────────────────────────────────────────────────────

    // True if dest is within the active unit's moveRange, in bounds, and not
    // blocked by a friendly stack.
    bool canMoveTo(HexCoord dest) const;

    // Move the active unit to dest and advance the turn.
    // Only call after canMoveTo returns true.
    void doMove(HexCoord dest);

    // ── Actions (stubs — damage resolution added in next pass) ───────────────

    // Attack target stack in the opposing army by index.  A melee stack that
    // is not adjacent first walks to bestAttackHex() (one move + one strike,
    // then the turn ends).  An unreachable target is struck from where the
    // unit stands (legacy/test behaviour — UI callers validate with canAttack).
    void doAttack(int targetIndex);

    // Walk to `from` (if different from the current hex), then attack.
    // Emits UnitMoved before the attack events so animations play in order.
    // Returns false (no action) unless canAttackFrom(from, targetIndex).
    bool doAttackFrom(HexCoord from, int targetIndex);

    // Find the enemy stack standing on targetHex and attack it.
    // Returns true if an enemy was found and attacked.
    bool doAttackAt(HexCoord targetHex);

    // Walk an exact route (hexes after the current one, ending on the
    // destination): each step adjacent to the last, in bounds, free of living
    // stacks, no repeats, at most moveRange steps.  Returns false (no action)
    // if the route is illegal.  doAttackAlong then strikes enemy targetIndex
    // from the route's end (or from where it stands if the route is empty).
    bool isLegalRoute(const std::vector<HexCoord>& route) const;
    bool doMoveAlong(const std::vector<HexCoord>& route);
    bool doAttackAlong(const std::vector<HexCoord>& route, int targetIndex);

    // Take a defensive stance: +25% defence until this stack's next turn
    // (it carries into the next round), then the turn passes.
    void doDefend();

    // Immediately end combat with the player retreating.
    void doRetreat();

    // Advance to the next actor; rebuilds the queue when a round ends.
    void advance();

    // ── Tactics: opening orders ──────────────────────────────────────────────
    // Before anyone has acted in round 1, the player may name up to `limit`
    // of their stacks (player army indices, in order) to act first, ahead of
    // every enemy. Returns false (nothing changes) once the battle is under
    // way, or for a bad list (unknown/dead/duplicate index, too many).
    bool setOpeningOrder(const std::vector<int>& playerStacks, int limit);
    bool battleStarted() const { return m_round > 1 || m_turn > 0; }

    // True if target has friendly attackers on two opposite hex sides.
    // Pinned units take 150% damage and cannot retaliate.
    static bool isFlanked(const CombatUnit& target,
                          const std::vector<CombatUnit>& attackers);

    // ── Event queue ──────────────────────────────────────────────────────────
    // Returns all events produced since the last drain, then clears the queue.
    // Call once per update() tick; consume each event to drive animations.
    std::vector<CombatEvent> drainEvents();

    // ── Setup helpers (tests / scenario editor) ───────────────────────────────

    // Forcibly place a stack at a position after construction.
    // Useful in tests where placeArmies() needs to be overridden.
    void teleportUnit(bool isPlayer, int stackIdx, HexCoord pos);

    // ── SC branch choice ──────────────────────────────────────────────────────

    // True while waiting for the player to resolve a level-up branch choice.
    // Combat can finish, but XP awards and AI turns are paused until resolved.
    bool hasPendingChoice() const { return m_pendingChoice; }

    // Resolve the pending branch choice for the given SC stack.
    // Applies the chosen BranchOption's effects, records the choice on the unit,
    // then resumes any remaining level-up processing.
    // No-op if no choice is pending or the indices don't match.
    void resolveScChoice(bool isPlayer, int stackIdx, const std::string& branchId);

private:
    std::vector<Fieldwork> m_fieldworks;
    // Award XP to a SC stack; handles level-up logic and emits ScXpGained /
    // ScLevelUp events.  No-ops if the unit has no SC def or amount <= 0.
    void awardScXp(CombatUnit& unit, const TurnSlot& slot, int amount);

    // Process any pending level-ups for `unit`, driving the skill tree.
    // Emits ScLevelUp for each level gained; emits ScChoicePending and pauses
    // if a choice node is reached.
    void processLevelUps(CombatUnit& unit, const TurnSlot& slot);

    // Apply a list of NodeEffects directly to a CombatUnit.
    // stat_mod → bonus fields / scExtraStats;  unlock → scUnlocked;
    // unknown types → logged and skipped (open extension point).
    void applyNodeEffects(CombatUnit& unit, const std::vector<NodeEffect>& effects);

    // Returns true if `node` passes its requiresBranch gate for `unit`.
    bool nodePassesBranchGate(const SCLevelNode& node, const CombatUnit& unit) const;

    // Assign each stack its spawn position on the CombatMap.
    void placeArmies();

    // Build m_queue sorted by speed desc; player wins speed ties.
    void buildQueue();

    // Check if either side is fully dead and update m_result accordingly.
    void checkWinCondition();

    // Resolve the active unit's attack (and retaliation) from where it stands,
    // then advance the turn.
    void resolveAttack(int targetIndex);

    // ATK/DEF multiplier shared by calcDamage and damageRange.
    static double damageMultiplier(const CombatUnit& attacker, const CombatUnit& defender);

    // HoMM3-style damage roll: sum rand(min,max) over each creature in attacker.
    static int calcDamage(const CombatUnit& attacker, const CombatUnit& defender,
                          std::mt19937& rng);

    // Cascade damage through the stack, decrementing count as creatures die.
    static void applyDamage(CombatUnit& target, int damage);

    // Deal `damage` to a stack (splitting a melee blow on a companion with its
    // bodyguard) and emit the Damaged/Died events.  Returns true if it died.
    bool hitStack(bool targetIsPlayer, int targetIndex, int damage, bool melee);

    // A stack that destroyed an enemy stack gains its renown (if it has any).
    void gainRenown(CombatUnit& victor);

    // Move the active stack (event + position + auras), then let enemy
    // shooters react.  Returns false if the stack died on the way in.
    bool arrive(HexCoord to, const std::vector<HexCoord>& path);

    // Recompute every stack's auraBonus from current positions.
    void refreshAuras();

    // Shortest walk for `unit` to `to` around every living stack as they stand
    // now (hexes after its own, ending on `to`; empty if there is none).
    std::vector<HexCoord> walkTo(const CombatUnit& unit, HexCoord to) const;

    // Hexes `unit` can walk to (same rules as reachableTiles), ignoring `self`.
    std::vector<HexCoord> reachableFor(const CombatUnit& unit) const;


    CombatArmy            m_player;
    CombatArmy            m_enemy;
    CombatMap             m_map;
    std::vector<TurnSlot> m_queue;
    int                   m_turn  = 0;
    int                   m_round = 1;
    CombatResult          m_result = CombatResult::Ongoing;
    std::optional<HexCoord> m_strikeOrigin;   // where a walk-and-strike began (strike_and_return)
    std::mt19937          m_rng;
    std::mt19937          m_aiRng;

    std::vector<CombatEvent> m_events;   // output queue; drained by CombatState

    // Pending branch choice state.
    bool m_pendingChoice         = false;
    bool m_pendingChoiceIsPlayer = false;
    int  m_pendingChoiceStackIdx = -1;
};
