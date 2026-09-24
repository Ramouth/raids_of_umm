#include "CombatAI.h"
#include "CombatEngine.h"
#include "CombatUnit.h"
#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <queue>

// ── Tuning ────────────────────────────────────────────────────────────────────
namespace {
constexpr double kWipeBonus        = 0.35;  // × target threat, for destroying a stack outright
constexpr double kShooterThreat    = 1.25;  // shooters are worth more: they hit anything, anywhere
constexpr double kNextTurn         = 0.90;  // value of an attack one turn from now
constexpr double kPerExtraTurn     = 0.75;  // further decay per extra turn of walking
constexpr double kSecondTarget     = 0.25;  // hexes threatening two stacks are better
constexpr double kClaimPenalty     = 0.35;  // per ally already heading for the same target
constexpr double kCrowdPenalty     = 0.03;  // × own threat, per ally next to the destination
constexpr double kApproach         = 0.01;  // × own threat, per hex from the nearest enemy
constexpr double kDangerWeight     = 0.40;  // exposure to strikes, fades out by round 4
constexpr double kDefendShield     = 0.10;  // defending takes roughly 10% less damage
constexpr double kPoolFraction     = 0.08;  // candidates within 8% of the best are eligible
constexpr double kTemperature      = 0.025; // softmax temperature (fraction of score scale)
constexpr double kCompanionValue   = 1.25;  // companions: their aura and their loss count beyond their damage

constexpr int kCells = CombatMap::GRID_W * CombatMap::GRID_H;
constexpr int kFar   = INT_MAX / 4;

int cellIndex(HexCoord h) {
    int col, row;
    if (!CombatMap::fromHex(h, col, row)) return -1;
    return col * CombatMap::GRID_H + row;
}

// Everything the scorer needs about the current actor, computed once per turn.
struct Ctx {
    const CombatEngine& eng;
    const CombatUnit&   actor;
    bool                side;          // actor.isPlayer
    const std::vector<CombatUnit>& own;
    const std::vector<CombatUnit>& foes;
    int                 actorIdx;
    std::vector<double> foeThreat;     // damage/turn each foe deals to our side
    double              ownThreat = 0; // damage/turn the actor deals to the foes
    std::vector<bool>   foeActed;      // foe already acted this round
    std::vector<int>    claims;        // allies heading for each foe
    std::vector<std::array<int, kCells>> reach;  // per foe: walking distance to an attack hex
    std::array<bool, kCells> blocked{};          // living stacks except the actor
    double              dangerWeight = 0;
    double              patience = 1;            // weight of plans vs acting now
    bool                shooter = false;         // actor can shoot this turn

    explicit Ctx(const CombatEngine& e)
        : eng(e), actor(e.activeUnit()), side(actor.isPlayer),
          own(side ? e.playerArmy().stacks : e.enemyArmy().stacks),
          foes(side ? e.enemyArmy().stacks : e.playerArmy().stacks),
          actorIdx(e.currentTurn().stackIndex) {}
};

double threatOf(const CombatUnit& u, const std::vector<CombatUnit>& victims) {
    double sum = 0; int n = 0;
    for (const auto& v : victims) {
        if (v.isDead()) continue;
        sum += CombatEngine::damageRange(u, v).avg;
        ++n;
    }
    double t = n ? sum / n : 0.0;
    if (u.type->isRanged() && u.shotsLeft > 0) t *= kShooterThreat;
    return t;
}

// Threat removed by dealing `dmg` to foe i (+ chance of wiping the stack).
double killValue(const Ctx& c, int i, const DamageRange& dmg) {
    const CombatUnit& e = c.foes[i];
    const double hp = std::max(1, e.totalHp());
    double v = c.foeThreat[i] * std::min(dmg.avg, hp) / hp;
    double wipe = 0;
    if (dmg.min >= hp)      wipe = 1;
    else if (dmg.max >= hp) wipe = double(dmg.max - hp) / std::max(1, dmg.max - dmg.min);
    return v + kWipeBonus * c.foeThreat[i] * wipe;
}

// Threat the actor loses by taking `dmg`.
double lossValue(const Ctx& c, double dmg) {
    const double hp = std::max(1, c.actor.totalHp());
    return c.ownThreat * std::min(dmg, hp) / hp;
}

// `stack` after losing `dmg` HP (approximate: whole creatures only).
CombatUnit afterDamage(const CombatUnit& stack, double dmg) {
    CombatUnit copy = stack;
    copy.count -= CombatEngine::killsFor(stack, static_cast<int>(dmg));
    return copy;
}

bool allyAt(const Ctx& c, HexCoord h) {
    for (int i = 0; i < (int)c.own.size(); ++i)
        if (i != c.actorIdx && !c.own[i].isDead() && c.own[i].pos == h) return true;
    return false;
}

// Would foe i be pinned if the actor struck it from `from`?
bool pinnedFrom(const Ctx& c, int i, HexCoord from) {
    const HexCoord p = c.foes[i].pos;
    for (int dir = 0; dir < 3; ++dir) {
        HexCoord a = p.neighbor(dir), b = p.neighbor(dir + 3);
        bool hasA = a == from || allyAt(c, a);
        bool hasB = b == from || allyAt(c, b);
        if (hasA && hasB) return true;
    }
    return false;
}

// Does foe i strike before the actor's next turn?
bool strikesFirst(const Ctx& c, int i) {
    if (!c.foeActed[i]) return true;
    const int fs = c.foes[i].effectiveSpeed(), as = c.actor.effectiveSpeed();
    return fs > as || (fs == as && c.foes[i].isPlayer);
}

// Can foe e strike a stack standing on h before that stack acts again?
// Move-and-attack: any melee foe within its move range + 1 (ignoring blockers).
bool canReach(const CombatUnit& e, HexCoord h) {
    return e.pos.distanceTo(h) - 1 <= e.type->moveRange;
}

// Net expected loss of ending our turn on h: strikes from melee foes that act
// before our next turn and can reach h (move + attack), each splitting its
// attention between all of our stacks it can reach, minus the value of our
// retaliation.  Foe `ignore` (a stack we expect to destroy) is skipped.
double exposureAt(const Ctx& c, HexCoord h, bool defending, int ignore = -1) {
    double danger = 0;
    CombatUnit me = c.actor;
    me.pos = h;
    me.isDefending = defending;
    for (int i = 0; i < (int)c.foes.size(); ++i) {
        const CombatUnit& e = c.foes[i];
        if (i == ignore || e.isDead() || !strikesFirst(c, i) || !canReach(e, h)) continue;
        // A shooter hurts us wherever we stand (adjacency only swaps its shot
        // for a strike we can answer), so it is never a reason to hold back.
        if (e.type->isRanged() && e.shotsLeft > 0) continue;
        int targets = 1;
        for (int j = 0; j < (int)c.own.size(); ++j)
            if (j != c.actorIdx && !c.own[j].isDead() && canReach(e, c.own[j].pos)) ++targets;
        const double share = 1.0 / targets;
        const double incoming = CombatEngine::damageRange(e, me).avg;
        danger += share * lossValue(c, incoming);
        // Retaliation, unless it is already spent for the round the strike lands in.
        const bool spent = c.actor.hasRetaliated && !c.foeActed[i];
        if (!spent && !e.type->hasAbility("no_retaliation")) {
            CombatUnit hurt = afterDamage(me, incoming);
            if (!hurt.isDead())
                danger -= 0.8 * share * killValue(c, i, CombatEngine::damageRange(hurt, e));
        }
    }
    return danger;
}

// Value of attacking foe i next turn from hex h (melee), or of shooting it.
double futureAttackValue(const Ctx& c, int i, HexCoord h, bool shot) {
    const CombatUnit& e = c.foes[i];
    CombatUnit me = c.actor;
    me.pos = h;
    const bool pinned = !shot && pinnedFrom(c, i, h);
    DamageRange dmg = CombatEngine::damageRange(me, e, pinned);
    if (shot && !c.eng.hasLineOfSight(h, e.pos)) {   // a blocked shot loses half
        dmg.min /= 2; dmg.max /= 2; dmg.avg /= 2;
    }
    double v = killValue(c, i, dmg);
    if (!shot && !pinned && !me.type->hasAbility("no_retaliation")) {
        CombatUnit hurt = afterDamage(e, dmg.avg);
        if (!hurt.isDead()) v -= lossValue(c, CombatEngine::damageRange(hurt, me).avg);
    }
    // Moving toward something is never worthless, even into a bad trade.
    return std::max(v, 0.15 * killValue(c, i, dmg));
}

// Best discounted attack the actor sets up by ending its turn on h.
double positionValue(const Ctx& c, HexCoord h) {
    const int idx = cellIndex(h);
    const int range = std::max(1, c.actor.type->moveRange);
    double best = 0, second = 0;
    for (int i = 0; i < (int)c.foes.size(); ++i) {
        const CombatUnit& e = c.foes[i];
        if (e.isDead()) continue;
        double v;
        if (c.shooter) {
            // A melee foe that can close in before our next turn turns the shot
            // into a melee strike — so stepping away from it (kiting) gains
            // nothing, and the scorer never plays keep-away.
            const bool chaser = !(e.type->isRanged() && e.shotsLeft > 0) && strikesFirst(c, i)
                                && e.pos.distanceTo(h) - 1 <= e.type->moveRange;
            const bool shot = e.pos.distanceTo(h) > 1 && !chaser;
            v = futureAttackValue(c, i, h, shot) * kNextTurn;
        } else {
            const int d = idx >= 0 ? c.reach[i][idx] : kFar;
            if (d >= kFar) continue;
            // Move-and-attack: anything within one move is struck next turn.
            const int turns = d <= range ? 0 : (d - range + range - 1) / range;
            v = futureAttackValue(c, i, h, false) * kNextTurn * std::pow(kPerExtraTurn, turns);
            v /= 1.0 + kClaimPenalty * c.claims[i];
        }
        if (v > best) { second = best; best = v; }
        else if (v > second) second = v;
    }
    double value = best + kSecondTarget * second;

    int nearest = kFar, crowd = 0;
    for (const auto& e : c.foes)
        if (!e.isDead()) nearest = std::min(nearest, e.pos.distanceTo(h));
    for (int j = 0; j < (int)c.own.size(); ++j)
        if (j != c.actorIdx && !c.own[j].isDead() && c.own[j].pos.distanceTo(h) == 1) ++crowd;
    if (!c.shooter && nearest < kFar) value -= kApproach * c.ownThreat * nearest;
    value -= kCrowdPenalty * c.ownThreat * crowd;
    return value;
}

// Multi-source BFS from the free hexes around foe i.
void buildReach(Ctx& c, int i) {
    auto& dist = c.reach[i];
    dist.fill(kFar);
    std::queue<HexCoord> frontier;
    const HexCoord p = c.foes[i].pos;
    for (int dir = 0; dir < 6; ++dir) {
        HexCoord n = p.neighbor(dir);
        int k = cellIndex(n);
        if (k < 0 || c.blocked[k] || dist[k] == 0) continue;
        dist[k] = 0;
        frontier.push(n);
    }
    while (!frontier.empty()) {
        HexCoord h = frontier.front(); frontier.pop();
        const int d = dist[cellIndex(h)];
        for (int dir = 0; dir < 6; ++dir) {
            HexCoord n = h.neighbor(dir);
            int k = cellIndex(n);
            if (k < 0 || c.blocked[k] || dist[k] <= d + 1) continue;
            dist[k] = d + 1;
            frontier.push(n);
        }
    }
}

void prepare(Ctx& c) {
    const auto& order = c.eng.turnOrder();
    c.foeActed.assign(c.foes.size(), false);
    for (int k = 0; k < c.eng.turnIndex() && k < (int)order.size(); ++k)
        if (order[k].isPlayer != c.side) c.foeActed[order[k].stackIndex] = true;

    c.foeThreat.resize(c.foes.size());
    for (int i = 0; i < (int)c.foes.size(); ++i)
        c.foeThreat[i] = c.foes[i].isDead() ? 0 : threatOf(c.foes[i], c.own);
    c.ownThreat = threatOf(c.actor, c.foes);
    // Hunt the other side's companions; keep our own out of harm's way.
    for (int i = 0; i < (int)c.foes.size(); ++i)
        if (c.foes[i].isSpecialCharacter) c.foeThreat[i] *= kCompanionValue;
    if (c.actor.isSpecialCharacter) c.ownThreat *= kCompanionValue;
    c.shooter = c.actor.type->isRanged() && c.actor.shotsLeft > 0;

    // Claims: which foe is each ally (melee) currently going for?
    c.claims.assign(c.foes.size(), 0);
    for (int j = 0; j < (int)c.own.size(); ++j) {
        const CombatUnit& a = c.own[j];
        if (j == c.actorIdx || a.isDead() || (a.type->isRanged() && a.shotsLeft > 0)) continue;
        int best = -1, bestDist = kFar;
        for (int i = 0; i < (int)c.foes.size(); ++i) {
            if (c.foes[i].isDead()) continue;
            int d = a.pos.distanceTo(c.foes[i].pos);
            if (d < bestDist) { bestDist = d; best = i; }
        }
        if (best >= 0) ++c.claims[best];
    }

    auto mark = [&](const std::vector<CombatUnit>& stacks, bool isOwn) {
        for (int j = 0; j < (int)stacks.size(); ++j) {
            if (stacks[j].isDead() || (isOwn && j == c.actorIdx)) continue;
            int k = cellIndex(stacks[j].pos);
            if (k >= 0) c.blocked[k] = true;
        }
    };
    mark(c.own, true);
    mark(c.foes, false);
    c.reach.resize(c.foes.size());
    for (int i = 0; i < (int)c.foes.size(); ++i)
        if (!c.foes[i].isDead()) buildReach(c, i);

    // Plans lose value as the battle drags on: late rounds favour striking now.
    c.patience = std::max(0.4, 1.0 - 0.05 * (c.eng.roundNumber() - 1));

    // Caution fades each round so cautious AIs cannot stall a battle forever.
    c.dangerWeight = kDangerWeight * std::max(0.0, 1.0 - 0.34 * (c.eng.roundNumber() - 1));
    bool foeShoots = false, weShoot = false;
    for (const auto& e : c.foes) foeShoots |= !e.isDead() && e.type->isRanged() && e.shotsLeft > 0;
    for (const auto& a : c.own)  weShoot   |= !a.isDead() && a.type->isRanged() && a.shotsLeft > 0;
    if (foeShoots && !weShoot) c.dangerWeight = 0;   // waiting only feeds their archers
}
} // namespace

// ── Public ────────────────────────────────────────────────────────────────────

std::vector<CombatAI::Candidate> CombatAI::scoreActions(const CombatEngine& engine) {
    std::vector<Candidate> out;
    if (engine.isOver()) return out;
    Ctx c(engine);
    prepare(c);
    const HexCoord here = c.actor.pos;
    const double w = c.dangerWeight;

    // Attacks: a shot or strike from where we stand, or (melee) a walk to any
    // standing hex next to the target followed by a strike — one candidate
    // per (standing hex, target) pair.
    for (int i = 0; i < (int)c.foes.size(); ++i) {
        if (!engine.canAttack(i)) continue;
        std::vector<HexCoord> spots = c.shooter ? std::vector<HexCoord>{here}
                                                : engine.attackHexesFor(i);
        for (const HexCoord& from : spots) {
            const AttackPreview p = engine.previewAttackUnchecked(i, from);
            if (!p.valid) continue;
            const double gain = killValue(c, i, p.damage);
            double v = gain;
            if (p.retaliation) v -= lossValue(c, p.retaliationDamage.avg);
            // Like HoMM3 creatures, never refuse a fight outright: an engaged
            // stack that only defends deadlocks against another that does too.
            v = std::max(v, 0.15 * gain);
            const bool wipes = p.killsMin >= c.foes[i].count;
            v += c.patience * positionValue(c, from) - w * exposureAt(c, from, false, wipes ? i : -1);
            out.push_back({Candidate::Kind::Attack, i, from, v});
        }
    }
    const bool canStrike = !out.empty();

    // Moves.  A shooter with a clear shot never walks away from it.
    bool anyShot = false;
    for (const auto& k : out)
        anyShot |= c.shooter && c.foes[k.target].pos.distanceTo(here) > 1;
    if (!anyShot) {
        for (const HexCoord& h : engine.reachableTiles()) {
            // Walking costs a little (less than one hex of approach), so
            // equal-value shuffling loses to holding but advancing does not.
            double v = c.patience * positionValue(c, h) - w * exposureAt(c, h, false)
                       - 0.3 * kApproach * c.ownThreat;
            out.push_back({Candidate::Kind::Move, -1, h, v});
        }
    }

    // Hold position: same prospects as staying, a little less damage taken.
    // Defending while a strike is available is only a tie-breaker loser.
    const double stay = c.patience * positionValue(c, here);
    const double exposed = exposureAt(c, here, false);
    const double shielded = exposureAt(c, here, true);
    const double defend = canStrike ? stay - w * exposed - kCrowdPenalty * c.ownThreat
                                    : stay - w * shielded + kDefendShield * std::max(0.0, exposed - shielded);
    out.push_back({Candidate::Kind::Defend, -1, here, defend});
    return out;
}

CombatAI::Candidate CombatAI::chooseAction(CombatEngine& engine) {
    std::vector<Candidate> all = scoreActions(engine);
    if (all.empty()) return {};
    double best = all[0].score;
    for (const auto& a : all) best = std::max(best, a.score);
    double scale = std::abs(best);
    for (const auto& a : all) scale = std::max(scale, std::abs(a.score) * 0.25);
    scale = std::max(scale, 1e-6);

    // Softmax over the near-best pool: varied play, never a blunder.
    std::vector<const Candidate*> pool;
    std::vector<double> weights;
    for (const auto& a : all) {
        if (a.score < best - kPoolFraction * scale) continue;
        pool.push_back(&a);
        weights.push_back(std::exp((a.score - best) / (kTemperature * scale)));
    }
    std::discrete_distribution<int> pick(weights.begin(), weights.end());
    return *pool[pick(engine.aiRng())];
}

void CombatAI::takeTurn(CombatEngine& engine) {
    if (engine.isOver()) return;
    const Candidate choice = chooseAction(engine);
    switch (choice.kind) {
    case Candidate::Kind::Attack:
        if (!engine.doAttackFrom(choice.hex, choice.target)) engine.doAttack(choice.target);
        return;
    case Candidate::Kind::Move:   engine.doMove(choice.hex);      return;
    case Candidate::Kind::Defend: engine.doDefend();              return;
    }
}
