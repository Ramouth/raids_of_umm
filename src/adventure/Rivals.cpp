// Rivals.cpp — strategic opponents: legacy Shariw raiders and scenario castle lords.
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

constexpr int    MAX_STACKS      = 5;     // Hero::ARMY_SLOTS
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

std::string AdventureSession::adviser() const {
    for (const auto& sc : m_specials) if (sc.id == "ushari") return "Ushari";
    return "Scout";
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
                                                          ArmyBonus attackerBonus,
                                                          const std::function<void(const CombatEngine&)>& watch,
                                                          const std::string& layout) const {
    auto build = [&](const std::vector<Stack>& stacks, bool player) {
        CombatArmy army;
        army.isPlayer = player;
        army.ownerName = player ? "Attackers" : "Defenders";
        for (const auto& s : stacks) {
            if ((int)army.stacks.size() >= MAX_STACKS) break;
            if (const UnitType* u = m_resources->unit(s.id); u && s.count > 0) {
                army.stacks.push_back(CombatUnit::make(u, s.count, player));
                army.stacks.back().bitten = s.bitten;
            }
        }
        return army;
    };
    AutoResult out;
    CombatArmy a = build(attacker, true);
    for (auto& s : a.stacks) {
        s.attackBonus  = attackerBonus.attack;
        s.defenseBonus = attackerBonus.defense;
        s.speedBonus   = attackerBonus.speed;
        s.readiedShot  = attackerBonus.readiedShot;
    }
    if (!a.stacks.empty()) {
        int fielded = 0;
        for (const auto& c : companions)
            if (const UnitType* u = m_resources->unit(c.id); u && fielded < MAX_RIDING) {
                a.stacks.push_back(CombatUnit::companion(u, c.level, true));
                ++fielded;
            }
    }
    CombatArmy d = build(defender, false);
    if (a.stacks.empty()) { out.defender = defender; return out; }
    if (d.stacks.empty()) { out.attackerWon = true; out.attacker = attacker; return out; }
    const CombatLayout* shape = m_resources->combatLayouts().find(layout);
    CombatEngine engine(std::move(a), std::move(d), 0, std::nullopt, shape ? *shape : CombatLayout::field());
    if (watch) watch(engine);
    for (int n = 0; n < 4000 && !engine.isOver(); ++n) {
        CombatAI::takeTurn(engine);
        if (watch) watch(engine);
    }
    out.attackerWon = engine.result() == CombatResult::PlayerWon;
    for (const auto& u : engine.playerArmy().stacks) {
        if (u.isSpecialCharacter) { if (u.isDead()) out.fallen.push_back(u.scId); }
        else if (!u.isDead()) out.attacker.push_back({u.type->id, u.count, u.bitten});
    }
    for (const auto& u : engine.enemyArmy().stacks)  if (!u.isDead()) out.defender.push_back({u.type->id, u.count, u.bitten});
    return out;
}

void AdventureSession::spawnRival(const HexCoord& town, int band) {
    Rival r;
    r.id   = static_cast<int>(m_rivals.size()) + 1;
    r.name = band == 1 ? "Shariw war-band" : "second Shariw war-band";
    r.pos  = town;
    r.home = town;
    r.army = {{"shariw_scout", 30}, {"scorpion_rider", 12}, {"dune_stalker", 6}};
    const auto& lord = m_scenario.rivalLord();
    const auto* home = m_map.objectAt(town);
    if (!lord.empty() && home && home->name == lord.value("home", "")) {
        r.name = lord.at("name");
        r.army.clear();
        for (const auto& stack : lord.at("army")) r.army.push_back({stack.at("id"), stack.at("count")});
        for (const auto& stack : lord.at("garrison")) m_garrisons[town].push_back({stack.at("id"), stack.at("count")});
        for (const auto& obj : m_map.objects()) if (obj.name == lord.value("escort_at", ""))
            for (const auto& stack : lord.value("escort_army", Scenario::Json::array()))
                m_garrisons[obj.pos].push_back({stack.at("id"), stack.at("count")});
    }
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
    if (m_scenario.rivalLord().empty() && t->terrain == Terrain::Dune && !t->road) return 1.0f;
    return t->moveCost;
}

std::vector<HexCoord> AdventureSession::rivalRoute(const Rival& r, const HexCoord& to) const {
    auto path = m_map.findPathWeighted(r.pos, to, [&](const HexCoord& c) {
        if (const auto* band = rivalAt(c); band && band->id != r.id) return true;
        if (c == to) return false;
        if (c == m_hero.pos || m_encounters.count(c)) return true;
        if (!m_scenario.rivalLord().empty())
            if (auto zone = guardZoneAt(c); zone && *zone != to) return true;
        return owner(c) == Faction::Player && m_control.at(c).objType == ObjType::Town;
    }, [&](const HexCoord& c) { return m_opened.count(c) > 0; });
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
        if (u->faction == townRoster(r.home) && canRecruitHere(r.home, u->id)) roster.push_back(u);
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

// Same costs, prerequisites, one-building-per-day limit and first recruit growth
// as player towns. No off-map subsidy for a castle opponent.
void AdventureSession::rivalBuild() {
    static const char* plan[] = {"town_hall", "marketplace", "horse_lines", "city_hall",
        "armoury", "citadel", "drill_yard", "knights_hall", "castle", "marksmens_tower"};
    auto& purse = m_turns.faction(Faction::AI).treasury;
    for (auto& [cell, town] : m_towns) {
        const auto* obj = m_map.objectAt(cell);
        if (!obj || obj->type != ObjType::Town || owner(cell) != Faction::AI || town.builtOnDay == day()) continue;
        for (const char* id : plan) {
            const auto* b = m_resources->building(id);
            if (!b || b->faction != townRoster(cell) || town.buildings.count(id) || !purse.canAfford(b->cost)) continue;
            if (!std::all_of(b->requires.begin(), b->requires.end(), [&](const std::string& need) { return town.buildings.count(need); })) continue;
            purse -= b->cost;
            town.buildings.insert(id);
            town.builtOnDay = day();
            town.lastBuilt = id;
            if (const auto* u = m_resources->unit(b->unlocks))
                town.recruitPool[u->id] += static_cast<int>(u->weeklyGrowth * (1.0 + growthBonus(cell)) + 0.5);
            break;
        }
    }
}

void AdventureSession::runRivals() {
    m_rivalMoves.clear();
    if (!rivalMobilized()) return;
    const bool castleLord = !m_scenario.rivalLord().empty();
    if (!castleLord && day() == SECOND_BAND_DAY - 1)
        for (const auto& r : std::vector<Rival>(m_rivals))
            if (owner(r.home) == Faction::AI) { spawnRival(r.home, 2); break; }
    if (!castleLord && !m_rivals.empty()) {
        ResourcePool& purse = m_turns.faction(Faction::AI).treasury;
        purse[Resource::Gold]     += TITHE_GOLD;
        purse[Resource::Wood]     += TITHE_WOOD;
        purse[Resource::Obsidian] += TITHE_OBSIDIAN;
    }
    if (castleLord) {
        rivalBuild();
        // A defeated field army does not make an enemy castle inert. Its steward
        // spends the same treasury/recruit pool reinforcing the permanent garrison.
        for (auto& [cell, town] : m_towns) {
            if (owner(cell) != Faction::AI || !m_map.objectAt(cell) || m_map.objectAt(cell)->type != ObjType::Town) continue;
            bool lordAlive = false;
            for (const auto& r : m_rivals) lordAlive |= r.alive && r.home == cell;
            if (!lordAlive) {
                Rival steward;
                steward.home = steward.pos = cell;
                steward.army = m_garrisons[cell];
                rivalRecruit(steward);
                m_garrisons[cell] = steward.army;
            }
        }
    }
    if (day() < RIVAL_START_DAY - 1) return;
    for (auto& r : m_rivals)
        if (r.alive && !m_lost) rivalTurn(r);
}

void AdventureSession::rivalTurn(Rival& r) {
    rivalRecruit(r);
    const double strength = power(r.army);
    const double hero     = power(army());
    const auto& lord = m_scenario.rivalLord();
    const bool castleLord = !lord.empty();
    const int contact = m_scenario.firedOn(lord.value("mobilize_after", ""));
    const bool mobilized = !castleLord || (contact >= 0 && day() >= contact + 2);

    // Pick the goal.
    std::optional<HexCoord> goal;
    std::vector<HexCoord> path;
    auto consider = [&](const HexCoord& target, const std::string& why) {
        if (castleLord && !mobilized && target.q < lord.value("territory_q_min", 5)) return;
        if (const auto* band = rivalAt(target); band && band->id != r.id) return;
        if (m_encounters.count(target) && strength < CLEAR_MARGIN * power(guardsOf(target))) return;
        auto p = rivalRoute(r, target);
        if (castleLord && !mobilized && std::any_of(p.begin(), p.end(), [&](const HexCoord& c) {
                return c.q < lord.value("territory_q_min", 5);
            })) return;
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
    if (!goal && castleLord && owner(r.home) == Faction::AI && r.pos != r.home
            && m_hero.pos.distanceTo(r.home) <= 4) consider(r.home, "defend castle");
    if (!goal && !m_pending && strength > AMBUSH_MARGIN * hero && r.pos.distanceTo(m_hero.pos) <= (castleLord ? SIGHT_RADIUS : AMBUSH_RANGE)) {
        consider(m_hero.pos, "ambush");
    }
    // From the third week the war-chief hunts the passage before loot.
    auto searchMines = [&] {
        if (castleLord) return; // Northern lords dispute estates, not the sealed passage.
        for (const auto& [cell, find] : m_mineFinds)
            if (strength > SEARCH_MARGIN * power(guardsOf(cell))) consider(cell, "search");
    };
    if (!goal && day() >= RACE_DAY) searchMines();
    if (!goal) {
        for (const auto& [cell, ctrl] : m_control) {
            if (ctrl.ownerFaction == Faction::AI || !rivalCaptures(ctrl.objType)) continue;
            if (!castleLord && ctrl.objType == ObjType::Town && ctrl.ownerFaction == Faction::Player) continue;
            if (const auto* held = garrison(cell); held && strength < CLEAR_MARGIN * power(*held)) continue;
            consider(cell, "capture");
        }
    }
    if (!goal) searchMines();
    if (!goal && castleLord) {
        for (const auto& [cell, pickup] : m_pickups)
            if (pickup.item.empty() && (pickup.reward[Resource::Gold] > 0 || pickup.reward[Resource::Wood] > 0 || pickup.reward[Resource::Stone] > 0))
                consider(cell, "supplies");
    }
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
        if (const auto* band = rivalAt(cell); band && band->id != r.id) break;
        if (m_encounters.count(cell)) {                 // neutral guards
            auto fight = autoBattle(r.army, guardsOf(cell));
            r.army = fight.attacker;
            const MapObjectDef* site = m_map.objectAt(cell);
            std::string siteName = site ? site->name : "the guards";
            if (!fight.attackerWon) {
                r.alive = false;
                if (isVisible(cell)) report("Scout", "The guards of the " + siteName + " have destroyed " + r.name + "!");
                break;
            }
            m_encounters.erase(cell);
            if (site && site->type == ObjType::Guard)
                report("Scout", r.name + " has cut through the " + siteName + ". The way is open for them now.");
            auto find = m_mineFinds.find(cell);
            const MapObjectDef* obj = m_map.objectAt(cell);
            std::string name = obj ? obj->name : "an old mine";
            if (find != m_mineFinds.end()) {
                if (find->second == MineFind::Passage) {
                    m_lost = true;
                    m_lostReason = "The Shariw reached the passage beneath the " + name + " first, and sealed it.";
                    report(adviser(), "Smoke over the " + name + "... they have collapsed the passage. It is over.");
                } else {
                    report("Scout", "Shariw riders have searched the " + name + ". They found nothing there.");
                }
                m_mineFinds.erase(find);
            }
        }
        left -= cost;
        r.pos = cell;
        move.path.push_back(cell);
        if (castleLord) {
            auto pickup = m_pickups.find(cell);
            if (pickup != m_pickups.end() && pickup->second.item.empty()) {
                m_turns.faction(Faction::AI).treasury += pickup->second.reward;
                m_pickups.erase(pickup);
            }
        }
        auto ctrl = m_control.find(cell);
        if (ctrl != m_control.end() && ctrl->second.ownerFaction == Faction::Player && garrison(cell)) {
            const MapObjectDef* obj = m_map.objectAt(cell);
            std::string name = obj ? obj->name : "the mine";
            auto fight = autoBattle(r.army, m_garrisons[cell]);
            r.army = fight.attacker;
            m_garrisons[cell] = fight.defender;
            if (!fight.attackerWon) {
                r.alive = false;
                report("Scout", "The garrison of the " + name + " held! " + r.name + " is defeated.");
                break;
            }
            report("Scout", "The garrison of the " + name + " has fallen to " + r.name + ".");
        }
        if (ctrl != m_control.end() && ctrl->second.ownerFaction != Faction::AI && rivalCaptures(ctrl->second.objType)) {
            bool wasOurs = ctrl->second.ownerFaction == Faction::Player;
            const auto previousRoster = townRoster(cell);
            ctrl->second.ownerFaction = Faction::AI;
            if (ctrl->second.objType == ObjType::Town) {
                ensureBuildings(cell);
                if (previousRoster != townRoster(cell)) { m_towns[cell].recruitPool.clear(); growTown(cell); }
            }
            for (auto& sc : m_specials) if (sc.stationed == cell) sc.stationed.reset();
            const MapObjectDef* obj = m_map.objectAt(cell);
            std::string name = obj ? obj->name : "a mine";
            if (wasOurs) report("Scout", r.name + " has taken the " + name + "! Its income is theirs now.");
            else if (isVisible(cell)) report("Scout", r.name + " raises a banner over the " + name + ".");
        }
        if (m_lost || cell == *goal) break;
    }
    if (!move.path.empty()) m_rivalMoves.push_back(std::move(move));
    recomputeVisibility();  // lost mines stop revealing fog
}

// A pre-existing road company turns on the expedition at the agreed meeting
// site. The reserve is saved with garrisons and consumed once, never respawned.
bool AdventureSession::ambushFrom(const std::string& townName, const std::string& bandName) {
    if (m_pending) return false;
    const MapObjectDef* home = nullptr;
    for (const auto& obj : m_map.objects()) if (obj.name == townName) home = &obj;
    if (!home || m_hero.pos != home->pos || !garrison(home->pos)) return false;
    std::optional<HexCoord> at;
    for (int d = 0; d < 6; ++d) {
        auto cell = m_hero.pos.neighbor(d);
        if (passable(cell) && !isEncounter(cell) && !m_map.objectAt(cell)) { at = cell; break; }
    }
    if (!at && !isEncounter(m_hero.pos)) at = m_hero.pos;
    if (!at) return false;
    Rival escort;
    escort.id = static_cast<int>(m_rivals.size()) + 1;
    escort.name = bandName;
    escort.home = home->pos;
    escort.pos = *at;
    auto& held = m_garrisons[home->pos];
    for (auto& stack : held) {
        int count = stack.count;
        if (count > 0) escort.army.push_back({stack.id, count});
        stack.count -= count;
    }
    held.erase(std::remove_if(held.begin(), held.end(), [](const Stack& s) { return s.count <= 0; }), held.end());
    escort.startPower = power(escort.army);
    m_rivals.push_back(escort);
    m_pending = *at;
    m_pendingAmbush = true;
    m_pendingZone = false;
    return true;
}
