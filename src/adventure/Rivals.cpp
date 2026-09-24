// Rivals.cpp — the Shariw AI for AdventureSession.
//
// Each war-band picks one goal per turn, in priority order:
//   1. weak (< 35% of its starting strength) → go home and recruit
//   2. the player's hero is close and clearly weaker → attack (ambush)
//   3. the nearest mine or neutral town not flying the Shariw banner → take it
//   4. the nearest unsearched old mine whose guards it can beat → search it
//      (from day 18, searching comes before capturing: the race is on)
//   5. the nearest guard camp it can beat (breaks chokepoints like the ford)
//   6. otherwise, or on growth day, ride home to recruit
// Fights against guards are resolved here with the real CombatEngine. An
// ambush sets the pending encounter so the player fights it on screen.
#include "AdventureSession.h"
#include "combat/CombatAI.h"
#include "combat/CombatEngine.h"
#include <algorithm>

namespace {

constexpr int    MAX_STACKS      = 5;     // CombatMap::GRID_H
constexpr double AMBUSH_MARGIN   = 1.5;   // attack the hero only when this much stronger
constexpr double SEARCH_MARGIN   = 1.15;   // search an old mine only when this much stronger
constexpr double CLEAR_MARGIN    = 1.5;   // attack a guard camp only when this much stronger
constexpr int    AMBUSH_RANGE    = 10;    // hexes
constexpr int    RACE_DAY        = 24;    // from here, searching old mines beats capturing
// The Shariw tribes pay their war-chief daily (their economy beyond the map).
constexpr int    TITHE_GOLD      = 1000;
constexpr int    TITHE_WOOD      = 2;
constexpr int    TITHE_OBSIDIAN  = 1;

bool rivalCaptures(ObjType t) {
    return t == ObjType::GoldMine || t == ObjType::CrystalMine || t == ObjType::Sawmill
        || t == ObjType::Quarry || t == ObjType::ObsidianVent || t == ObjType::Town;
}

} // namespace

void AdventureSession::report(const std::string& speaker, const std::string& text) {
    m_scenario.say(speaker, text);
}

const AdventureSession::Rival* AdventureSession::rivalAt(const HexCoord& c) const {
    for (const auto& r : m_rivals)
        if (r.alive && r.pos == c) return &r;
    return nullptr;
}

double AdventureSession::power(const std::vector<Stack>& army) const {
    double total = 0;
    for (const auto& s : army) {
        const UnitType* u = m_resources->unit(s.id);
        if (!u) continue;
        double hits = (u->minDamage + u->maxDamage) * 0.5 + 1.0;
        // Huge single creatures act once a round and waste damage on overkill;
        // battle_sim shows they fight far below their raw numbers.
        double big = u->hitPoints >= 100 ? 0.35 : 1.0;
        total += s.count * u->hitPoints * (u->attack + u->defense + 5) * hits * (u->shots > 0 ? 1.3 : 1.0) * big;
    }
    return total / 100.0;
}

AdventureSession::AutoResult AdventureSession::autoBattle(const std::vector<Stack>& attacker,
                                                          const std::vector<Stack>& defender,
                                                          const std::vector<Companion>& companions,
                                                          int attackerBonus) const {
    auto build = [&](const std::vector<Stack>& stacks, bool player) {
        CombatArmy army;
        army.isPlayer = player;
        army.ownerName = player ? "Attackers" : "Defenders";
        for (const auto& s : stacks) {
            if ((int)army.stacks.size() >= MAX_STACKS) break;
            if (const UnitType* u = m_resources->unit(s.id); u && s.count > 0)
                army.stacks.push_back(CombatUnit::make(u, s.count, player));
        }
        return army;
    };
    AutoResult out;
    CombatArmy a = build(attacker, true);
    for (auto& s : a.stacks) s.attackBonus = attackerBonus;
    if (!a.stacks.empty())
        for (const auto& c : companions)
            if (const UnitType* u = m_resources->unit(c.id))
                a.stacks.push_back(CombatUnit::companion(u, c.level, true));
    CombatArmy d = build(defender, false);
    if (a.stacks.empty()) { out.defender = defender; return out; }
    if (d.stacks.empty()) { out.attackerWon = true; out.attacker = attacker; return out; }
    CombatEngine engine(std::move(a), std::move(d));
    for (int n = 0; n < 4000 && !engine.isOver(); ++n) CombatAI::takeTurn(engine);
    out.attackerWon = engine.result() == CombatResult::PlayerWon;
    for (const auto& u : engine.playerArmy().stacks) {
        if (u.isSpecialCharacter) { if (u.isDead()) out.fallen.push_back(u.scId); }
        else if (!u.isDead()) out.attacker.push_back({u.type->id, u.count});
    }
    for (const auto& u : engine.enemyArmy().stacks)  if (!u.isDead()) out.defender.push_back({u.type->id, u.count});
    return out;
}

void AdventureSession::spawnRival(const HexCoord& town, int band) {
    Rival r;
    r.id   = static_cast<int>(m_rivals.size()) + 1;
    r.name = band == 1 ? "Shariw war-band" : "second Shariw war-band";
    r.pos  = town;
    r.home = town;
    r.army = {{"shariw_scout", 30}, {"scorpion_rider", 12}, {"dune_stalker", 6}};
    r.startPower = power(r.army);
    m_rivals.push_back(r);
}

std::vector<AdventureSession::Stack> AdventureSession::guardsOf(const HexCoord& c) const {
    std::vector<Stack> out;
    const Encounter* enc = encounterAt(c);
    if (!enc) return out;
    for (const auto& s : Scenario::Json::parse(enc->guardsJson))
        out.push_back({s.value("id", ""), s.value("count", 0)});
    return out;
}

// Shariw know the dunes: dunes cost them no more than sand.
float AdventureSession::rivalStepCost(const HexCoord& c) const {
    const MapTile* t = m_map.tileAt(c);
    if (!t) return 1.0f;
    if (t->terrain == Terrain::Dune && !t->road) return 1.0f;
    return t->moveCost;
}

std::vector<HexCoord> AdventureSession::rivalRoute(const Rival& r, const HexCoord& to) const {
    auto path = m_map.findPathWeighted(r.pos, to, [&](const HexCoord& c) {
        if (c == to) return false;
        if (c == m_hero.pos || isEncounter(c)) return true;
        return owner(c) == Faction::Player && m_control.at(c).objType == ObjType::Town;
    });
    if (path.size() < 2) return {};
    path.erase(path.begin());
    return path;
}

void AdventureSession::rivalRecruit(Rival& r) {
    auto it = m_towns.find(r.home);
    if (it == m_towns.end() || owner(r.home) != Faction::AI || r.pos != r.home) return;
    ResourcePool& purse = m_turns.faction(Faction::AI).treasury;
    std::vector<const UnitType*> roster;
    for (const UnitType* u : m_resources->unitsByTier())
        if (u->faction == rosterFor(Faction::AI)) roster.push_back(u);
    std::reverse(roster.begin(), roster.end());  // best units first
    for (const UnitType* u : roster) {
        int& pool = it->second.recruitPool[u->id];
        int n = pool;
        for (int i = 0; i < RESOURCE_COUNT; ++i)
            if (u->cost.amounts[i] > 0) n = std::min(n, purse.amounts[i] / u->cost.amounts[i]);
        if (n <= 0) continue;
        auto stack = std::find_if(r.army.begin(), r.army.end(), [&](const Stack& s) { return s.id == u->id; });
        if (stack == r.army.end()) {
            if ((int)r.army.size() >= MAX_STACKS) continue;
            r.army.push_back({u->id, 0});
            stack = r.army.end() - 1;
        }
        stack->count += n;
        pool -= n;
        for (int i = 0; i < RESOURCE_COUNT; ++i) purse.amounts[i] -= u->cost.amounts[i] * n;
    }
}

void AdventureSession::runRivals() {
    m_rivalMoves.clear();
    if (day() == SECOND_BAND_DAY - 1)   // rides out on the dawn of day 10
        for (const auto& r : std::vector<Rival>(m_rivals))
            if (owner(r.home) == Faction::AI) { spawnRival(r.home, 2); break; }
    if (!m_rivals.empty()) {
        ResourcePool& purse = m_turns.faction(Faction::AI).treasury;
        purse[Resource::Gold]     += TITHE_GOLD;
        purse[Resource::Wood]     += TITHE_WOOD;
        purse[Resource::Obsidian] += TITHE_OBSIDIAN;
    }
    if (day() < RIVAL_START_DAY - 1) return;   // first ride on the evening of day 2 → seen day 3
    for (auto& r : m_rivals)
        if (r.alive && !m_lost) rivalTurn(r);
}

void AdventureSession::rivalTurn(Rival& r) {
    rivalRecruit(r);
    const double strength = power(r.army);
    const double hero     = power(army());

    // Pick the goal.
    std::optional<HexCoord> goal;
    std::vector<HexCoord> path;
    auto consider = [&](const HexCoord& target, const std::string& why) {
        auto p = rivalRoute(r, target);
        if (p.empty()) return;
        float cost = 0;
        for (const auto& c : p) cost += rivalStepCost(c);
        float best = 0;
        for (const auto& c : path) best += rivalStepCost(c);
        if (!goal || cost < best) { goal = target; path = p; r.goal = why; }
    };
    bool growthDay = dayOfWeek() == 7 || dayOfWeek() == 1;
    if (r.pos != r.home && owner(r.home) == Faction::AI &&
        (strength < 0.35 * r.startPower || (growthDay && r.pos.distanceTo(r.home) <= 12))) {
        consider(r.home, "regroup");
    }
    if (!goal && !m_pending && strength > AMBUSH_MARGIN * hero && r.pos.distanceTo(m_hero.pos) <= AMBUSH_RANGE) {
        consider(m_hero.pos, "ambush");
    }
    // From the third week the war-chief hunts the passage before loot.
    auto searchMines = [&] {
        for (const auto& [cell, find] : m_mineFinds)
            if (strength > SEARCH_MARGIN * power(guardsOf(cell))) consider(cell, "search");
    };
    if (!goal && day() >= RACE_DAY) searchMines();
    if (!goal) {
        for (const auto& [cell, ctrl] : m_control) {
            if (ctrl.ownerFaction == Faction::AI || !rivalCaptures(ctrl.objType)) continue;
            if (ctrl.objType == ObjType::Town && ctrl.ownerFaction == Faction::Player) continue;
            if (const auto* held = garrison(cell); held && strength < CLEAR_MARGIN * power(*held)) continue;
            consider(cell, "capture");
        }
    }
    if (!goal) searchMines();
    if (!goal) {
        for (const auto& [cell, enc] : m_encounters) {
            const MapObjectDef* obj = m_map.objectAt(cell);
            if (obj && obj->type == ObjType::Guard && strength > CLEAR_MARGIN * power(guardsOf(cell)))
                consider(cell, "clear");
        }
    }
    if (!goal && r.pos != r.home && owner(r.home) == Faction::AI) consider(r.home, "regroup");
    if (!goal) { r.goal = "idle"; return; }

    // Walk as far as today's points allow.
    RivalMove move{r.id, {}};
    float left = DEFAULT_MOVES;
    for (const auto& cell : path) {
        float cost = rivalStepCost(cell);
        if (cost > left + 1e-4f) break;
        if (cell == m_hero.pos) {                       // ambush: the player fights on screen
            if (m_pending) break;                       // one ambush per night
            m_pending       = r.pos;
            m_pendingAmbush = true;
            report("Scout", "Ambush! The " + r.name + " falls on our camp!");
            break;
        }
        if (isEncounter(cell)) {                        // guards of an old mine
            auto fight = autoBattle(r.army, guardsOf(cell));
            r.army = fight.attacker;
            const MapObjectDef* site = m_map.objectAt(cell);
            std::string siteName = site ? site->name : "the guards";
            if (!fight.attackerWon) {
                r.alive = false;
                if (isVisible(cell)) report("Scout", "The guards of the " + siteName + " have destroyed a Shariw war-band!");
                break;
            }
            m_encounters.erase(cell);
            if (site && site->type == ObjType::Guard)
                report("Scout", "The Shariw have cut through the " + siteName + ". The way is open for them now.");
            auto find = m_mineFinds.find(cell);
            const MapObjectDef* obj = m_map.objectAt(cell);
            std::string name = obj ? obj->name : "an old mine";
            if (find != m_mineFinds.end()) {
                if (find->second == MineFind::Passage) {
                    m_lost = true;
                    m_lostReason = "The Shariw reached the passage beneath the " + name + " first, and sealed it.";
                    report("Ushari", "Smoke over the " + name + "... they have collapsed the passage. It is over.");
                } else {
                    report("Scout", "Shariw riders have searched the " + name + ". They found nothing there.");
                }
                m_mineFinds.erase(find);
            }
        }
        left -= cost;
        r.pos = cell;
        move.path.push_back(cell);
        auto ctrl = m_control.find(cell);
        if (ctrl != m_control.end() && ctrl->second.ownerFaction == Faction::Player && garrison(cell)) {
            const MapObjectDef* obj = m_map.objectAt(cell);
            std::string name = obj ? obj->name : "the mine";
            auto fight = autoBattle(r.army, m_garrisons[cell]);
            r.army = fight.attacker;
            m_garrisons[cell] = fight.defender;
            if (!fight.attackerWon) {
                r.alive = false;
                report("Scout", "The garrison of the " + name + " held! The Shariw war-band is destroyed.");
                break;
            }
            report("Scout", "The garrison of the " + name + " has fallen to the Shariw.");
        }
        if (ctrl != m_control.end() && ctrl->second.ownerFaction != Faction::AI && rivalCaptures(ctrl->second.objType)) {
            bool wasOurs = ctrl->second.ownerFaction == Faction::Player;
            ctrl->second.ownerFaction = Faction::AI;
            if (ctrl->second.objType == ObjType::Town) { m_towns[cell].recruitPool.clear(); growTown(cell); }
            const MapObjectDef* obj = m_map.objectAt(cell);
            std::string name = obj ? obj->name : "a mine";
            if (wasOurs) report("Scout", "The Shariw have taken the " + name + "! Its income is theirs now.");
            else if (isVisible(cell)) report("Scout", "Shariw riders raise their banner over the " + name + ".");
        }
        if (m_lost || cell == *goal) break;
    }
    if (!move.path.empty()) m_rivalMoves.push_back(std::move(move));
    recomputeVisibility();  // lost mines stop revealing fog
}
