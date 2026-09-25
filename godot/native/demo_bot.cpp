#include <cstdlib>
// demo_bot — plays "The Old Passage" (stage 1, the northern marches) through AdventureSession with a simple
// greedy strategy, to check the demo is winnable and how long it takes.
// Usage: demo_bot <repo_root> [seeds] [verbose]
// Strategy: clear the vale's wolves (level 1), recruit on growth days, take mines and towns in order, fight any
// guard it is clearly stronger than, visit Kharim, search old mines (ruled-
// out ones last). Battles are auto-resolved with the real CombatEngine.
#include "adventure/AdventureSession.h"
#include <iostream>

using Stack = AdventureSession::Stack;

static bool fight(AdventureSession& s) {
    auto at = *s.pendingEncounter();
    std::vector<Stack> guards;
    for (const auto& g : Scenario::Json::parse(s.encounterAt(at)->guardsJson))
        guards.push_back({g["id"], g["count"]});
    for (const auto& id : s.backpack()) s.equip(id);   // wear whatever fits a free slot
    auto r = s.autoBattle(s.army(), guards, s.battleCompanions(), s.armyBonus());
    s.setArmy(r.attacker);
    s.companionsFell(r.fallen, !r.attackerWon);
    s.resolveEncounter(r.attackerWon);
    return r.attackerWon;
}

static void recruitAll(AdventureSession& s) {
    const TownState* t = s.town(s.heroPos());
    if (!t) return;
    std::vector<std::pair<int, std::string>> order;
    for (const auto& [id, n] : t->recruitPool) order.push_back({-s.resources().unit(id)->tier, id});
    std::sort(order.begin(), order.end());
    for (const auto& [tier, id] : order)
        for (int n = t->recruitPool.at(id); n > 0; --n)
            if (!s.recruit(s.heroPos(), id, n)) break;
}

// Every town the bot holds builds the first thing on this list it can afford today.
static void buildAll(AdventureSession& s) {
    static const char* order[] = {"town_hall", "armoury", "citadel", "horse_lines", "marketplace",
                                  "drill_yard", "city_hall", "knights_hall", "castle"};
    for (const auto& obj : s.map().objects()) {
        if (obj.type != ObjType::Town || s.owner(obj.pos) != 1) continue;
        for (const char* id : order)
            if (s.buildBlocker(obj.pos, id).empty()) { s.build(obj.pos, id); break; }
    }
}

int main(int argc, char** argv) {
    std::string root = argc > 1 ? argv[1] : ".";
    int seeds = argc > 2 ? std::stoi(argv[2]) : 8;
    bool verbose = argc > 3;
    int wins = 0, sealed = 0, dead = 0, timeouts = 0, totalDays = 0;
    for (int seed = 0; seed < seeds; ++seed) {
        AdventureSession s;
        if (auto e = s.start(root + "/data/maps/old_passage.json", root + "/data", root + "/data/maps/old_passage.encounters.json",
                             seed, root + "/data/maps/old_passage.triggers.json")) { std::cerr << *e << "\n"; return 1; }
        s.setArmy({{"levy_spearman", 24}, {"desert_archer", 10}, {"armoured_warrior", 3}});
        recruitAll(s);
        bool visitedCamp = false;
        std::vector<std::string> plan = {"Hill Wolves", "Den Wolves", "Hermit's Wolves",   // level 1: Ushari's road
            "Varen Gold Mine", "Log Pile", "Pinewood Sawmill", "Hunters' Camp",
            "Varen Windmill", "Fallen Standing Stone", "Tarn Obelisk", "Bridge Wardens", "Hallowmere", "Toll Coins",
            "Tithe Silver", "A Sergeant's Field Book", "Hallow Quarry", "Quarry Obelisk", "Mere Sawmill", "Mere Watermill",
            "Drowned Obelisk", "Blackglass Seam", "Old Mine of Dunmere", "Old Mine of Carrow", "Greyfang Pass",
            "Greyfang Watch", "The Greyfang Campaigns", "Kharim's Camp", "Frostglass Cavern", "Greyfang Obelisk", "Old Mine of Kaldur",
            "Old Mine of Brannoc"};
        for (int guard = 0; guard < 400 && !s.won() && !s.lost() && !s.army().empty() && s.day() < 60; ++guard) {
            // Weekly: walk home to recruit.
            if (s.dayOfWeek() == 7 || s.dayOfWeek() == 1) {
                HexCoord home = s.heroPos();
                int best = 1 << 30;
                for (const auto& obj : s.map().objects())
                    if (obj.type == ObjType::Town && s.owner(obj.pos) == 1) {
                        int d = obj.pos.distanceTo(s.heroPos());
                        if (d < best) { best = d; home = obj.pos; }
                    }
                if (best <= 12 && s.heroPos() != home) { s.travel(home); }
                if (s.heroPos() == home) recruitAll(s);
            }
            // Next objective the army can handle.
            std::optional<HexCoord> target;
            for (const auto& name : plan) {
                const MapObjectDef* obj = nullptr;
                for (const auto& o : s.map().objects()) if (o.name == name) obj = &o;
                if (!obj) continue;
                bool done = (s.owner(obj->pos) == 1) || (obj->type == ObjType::Guard && !s.isEncounter(obj->pos))
                         || (obj->type == ObjType::OldMine && !s.isEncounter(obj->pos))
                         || (obj->type == ObjType::QuestGiver && visitedCamp)
                         || (obj->type >= ObjType::Pickup && obj->type != ObjType::Dwelling && s.siteUsed(obj->pos));
                if (obj->type == ObjType::QuestGiver && s.heroPos() == obj->pos) {
                    std::vector<std::string> ids;
                    for (const auto* o : s.scenario().offersAt(name)) ids.push_back(o->id);
                    for (const auto& id : ids) s.acceptOffer(id);   // pays what it can
                    visitedCamp = done = true;
                }
                if (s.heroPos() == obj->pos && !s.scenario().offersAt(name).empty() && obj->type == ObjType::Town)
                    s.acceptOffer("hire_hale");                    // a trusting cousin buys Corvin's men
                if (done) continue;
                if (obj->type == ObjType::OldMine && s.ruledOut().count(obj->pos)) continue;
                if (s.isEncounter(obj->pos)) {
                    std::vector<Stack> guards;
                    for (const auto& g : Scenario::Json::parse(s.encounterAt(obj->pos)->guardsJson)) guards.push_back({g["id"], g["count"]});
                    double threat = 0;                                     // war-bands near the target
                    for (const auto& r : s.rivals())
                        if (r.alive && r.pos.distanceTo(obj->pos) <= 12) threat = std::max(threat, s.power(r.army));
                    if (s.power(s.army()) < 1.1 * s.power(guards) + 0.6 * threat) {       // not ready: grow
                        if (verbose && s.dayOfWeek() == 3)
                            std::cout << "  d" << s.day() << " waiting for " << name << ": ours " << (int)s.power(s.army())
                                      << " guards " << (int)s.power(guards) << " threat " << (int)threat << "\n";
                        break;
                    }
                }
                target = obj->pos;
                break;
            }
            if (target) {
                if (verbose && s.dayOfWeek() == 3) std::cout << "  d" << s.day() << " heading to (" << target->q << "," << target->r << ") from (" << s.heroPos().q << "," << s.heroPos().r << ") route " << s.route(*target).size() << "\n";
                s.travel(*target);
                if (s.pendingChest()) s.claimChest(true);
                if (s.pendingEncounter()) fight(s);
            }
            if (s.needsPath()) s.choosePath(std::getenv("PATH_ID") ? std::getenv("PATH_ID") : "marshal");
            if (s.won() || s.lost() || s.army().empty()) break;
            if (!std::getenv("NO_BUILD")) buildAll(s);
            s.endDay();
            if (s.pendingEncounter()) fight(s);              // ambush
            if (verbose)
                for (const auto& l : s.scenario().drainLines()) std::cout << "  d" << s.day() << " [" << l.speaker << "] " << l.text << "\n";
            else s.scenario().drainLines();
        }
        if (std::getenv("SHOW_BUILDS"))
            for (const auto& obj : s.map().objects())
                if (obj.type == ObjType::Town && s.town(obj.pos) && s.owner(obj.pos) == 1) {
                    std::cout << "  " << obj.name << ":";
                    for (const auto& id : s.town(obj.pos)->buildings) std::cout << " " << id;
                    std::cout << "\n";
                }
        std::string result = s.won() ? "WON" : s.lost() ? "SEALED" : s.army().empty() ? "ARMY LOST" : "TIMEOUT";
        std::cout << "seed " << seed << ": " << result << " on day " << s.day()
                  << "  (hero L" << s.heroProgress().level << " xp " << s.heroProgress().xp
                  << ", Ushari L" << (s.specials().empty() ? 0 : s.specials()[0].level) << ", army power "
                  << (int)s.power(s.army()) << ", clues " << s.ruledOut().size() << ")\n";
        if (s.won()) { ++wins; totalDays += s.day(); }
        else if (s.lost()) ++sealed; else if (s.army().empty()) ++dead; else ++timeouts;
    }
    std::cout << "won " << wins << "/" << seeds << ", sealed " << sealed << ", army lost " << dead
              << ", timeout " << timeouts << (wins ? ", average win day " + std::to_string(totalDays / wins) : "") << "\n";
}
