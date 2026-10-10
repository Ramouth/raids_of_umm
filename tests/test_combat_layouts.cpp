// test_combat_layouts.cpp — battles have a shape (combat_layouts.json): grid
// size, deployment zones, terrain obstacles with hit points, who acts first;
// the player places their stacks before the battle starts; any stack can break
// an obstacle, and the AI breaks through when walled off.
#include "test_runner.h"
#include "combat/CombatAI.h"
#include "combat/CombatEngine.h"
#include "combat/CombatLayout.h"
#include "core/ResourceManager.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

const ResourceManager& registry() {
    static ResourceManager rm;
    static bool loaded = false;
    if (!loaded) { CHECK(!rm.load("data")); loaded = true; }
    return rm;
}

std::string readFile(const std::string& path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// The proposed data file (tests/fixtures), so these tests do not depend on the story's tuning.
const CombatLayouts& fixture() {
    static CombatLayouts layouts;
    static bool loaded = false;
    if (!loaded) { CHECK(!parseCombatLayouts(readFile("tests/fixtures/combat_layouts.json"), layouts)); loaded = true; }
    return layouts;
}

CombatArmy army(bool player, std::vector<std::pair<std::string, int>> stacks, std::vector<std::string> companions = {}) {
    CombatArmy a;
    a.isPlayer = player;
    a.ownerName = player ? "Player" : "Enemy";
    for (const auto& [id, n] : stacks) a.stacks.push_back(CombatUnit::make(registry().unit(id), n, player));
    for (const auto& id : companions) a.stacks.push_back(CombatUnit::companion(registry().unit(id), 1, player));
    return a;
}

bool contains(const std::vector<HexCoord>& v, HexCoord h) { return std::find(v.begin(), v.end(), h) != v.end(); }

int rowOf(HexCoord h) { int col, row; CombatMap::toOffset(h, col, row); return row; }

// An open 11 × 5 field with the given trees (offset cells) and the field's zones.
CombatLayout withTrees(std::vector<std::pair<int, int>> cells, int hp = 30) {
    CombatLayout l = CombatLayout::field();
    l.id = "test";
    for (const auto& [c, r] : cells) l.obstacles.push_back({CombatMap::toHex(c, r), "tree", hp, true});
    return l;
}

std::string mutate(const std::string& field) {
    // A minimal file: one kind, one layout body supplied by the caller.
    return R"({"obstacle_kinds": {"tree": {"hp": 50}}, "layouts": [)" + field + "]}";
}

const char* kSmall = R"("player_zone": [[0,0],[0,1],[0,2],[0,3],[0,4],[1,0],[1,1]],
                        "enemy_zone": [[6,0],[6,1],[6,2],[6,3],[6,4],[5,0],[5,1]])";

} // namespace

SUITE("Combat layouts — the proposed file loads, validates, and its field is the default field") {
    const CombatLayouts& l = fixture();
    CHECK(l.find("field") != nullptr);
    CHECK(l.find("corridor_ambush") != nullptr);
    CHECK(l.kind("tree") && l.kind("barricade") && l.kind("stakes"));
    CHECK(l.kind("tree")->hp > l.kind("stakes")->hp);          // trees are sturdier than stakes
    CHECK(l.kind("tree")->opaque && !l.kind("stakes")->opaque);
    for (const auto& layout : l.layouts) CHECK(!validateCombatLayout(layout, l.kinds));
    const CombatLayout def = CombatLayout::field();
    const CombatLayout* field = l.find("field");
    CHECK(field && field->width == def.width && field->height == def.height && field->obstacles.empty());
    CHECK(field && field->playerZone == def.playerZone && field->enemyZone == def.enemyZone);
    CHECK(field && field->fieldworkZone == def.fieldworkZone);
    CHECK(field && field->firstMove == CombatLayout::FirstMove::Normal);
}

SUITE("Combat layouts — the shipped data file (when present) validates and keeps the default field") {
    const CombatLayouts& shipped = registry().combatLayouts();
    if (!std::filesystem::exists("data/combat_layouts.json")) {
        std::cout << "  NOTE  data/combat_layouts.json is not there yet: every battle is the default field\n";
        CHECK(shipped.layouts.empty());
    } else {
        for (const auto& layout : shipped.layouts) CHECK(!validateCombatLayout(layout, shipped.kinds));
        const CombatLayout* field = shipped.find("field");
        CHECK(field && field->playerZone == CombatLayout::field().playerZone
              && field->enemyZone == CombatLayout::field().enemyZone
              && field->fieldworkZone == CombatLayout::field().fieldworkZone);
        for (const char* kind : {"tree", "barricade", "stakes"}) CHECK(shipped.kind(kind) && shipped.kind(kind)->hp > 0);
    }
}

SUITE("Combat layouts — validation refuses broken layouts") {
    CombatLayouts out;
    const std::string ok = std::string(R"({"id": "ok", "width": 7, "height": 5, )") + kSmall + "}";
    CHECK(!parseCombatLayouts(mutate(ok), out));
    // A wall of trees across the whole grid cuts the open hexes apart.
    std::string wall = std::string(R"({"id": "walled", "width": 7, "height": 5, "obstacles": [)")
        + R"({"cell":[3,0],"kind":"tree"},{"cell":[3,1],"kind":"tree"},{"cell":[3,2],"kind":"tree"},)"
        + R"({"cell":[3,3],"kind":"tree"},{"cell":[3,4],"kind":"tree"}], )" + kSmall + "}";
    CHECK(parseCombatLayouts(mutate(wall), out).has_value());
    // Too small a zone for seven stacks.
    CHECK(parseCombatLayouts(mutate(R"({"id": "small", "width": 7, "height": 5,
        "player_zone": [[0,0],[0,1],[0,2]], "enemy_zone": [[6,0],[6,1],[6,2],[6,3],[6,4],[5,0],[5,1]]})"), out).has_value());
    // A zone hex under a tree, outside the grid, or shared by both sides.
    CHECK(parseCombatLayouts(mutate(std::string(R"({"id": "covered", "width": 7, "height": 5,
        "obstacles": [{"cell":[0,0],"kind":"tree"}], )") + kSmall + "}"), out).has_value());
    CHECK(parseCombatLayouts(mutate(R"({"id": "outside", "width": 7, "height": 5,
        "player_zone": [[0,0],[0,1],[0,2],[0,3],[0,4],[1,0],[9,9]], "enemy_zone": [[6,0],[6,1],[6,2],[6,3],[6,4],[5,0],[5,1]]})"), out).has_value());
    CHECK(parseCombatLayouts(mutate(R"({"id": "shared", "width": 7, "height": 5,
        "player_zone": [[0,0],[0,1],[0,2],[0,3],[0,4],[1,0],[6,0]], "enemy_zone": [[6,0],[6,1],[6,2],[6,3],[6,4],[5,0],[5,1]]})"), out).has_value());
    // Unknown obstacle kind, unknown first_move, an obstacle kind without hit points.
    CHECK(parseCombatLayouts(mutate(std::string(R"({"id": "rock", "width": 7, "height": 5,
        "obstacles": [{"cell":[3,2],"kind":"rock"}], )") + kSmall + "}"), out).has_value());
    CHECK(parseCombatLayouts(mutate(std::string(R"({"id": "when", "width": 7, "height": 5, "first_move": "later", )") + kSmall + "}"), out).has_value());
    CHECK(parseCombatLayouts(R"({"obstacle_kinds": {"tree": {"hp": 0}}, "layouts": []})", out).has_value());
    // The same id twice.
    CHECK(parseCombatLayouts(mutate(ok + "," + ok), out).has_value());
}

SUITE("Combat layouts — corridor ambush: the party in the lane, dogs top and bottom, the dogs move first") {
    const CombatLayout* corridor = fixture().find("corridor_ambush");
    CHECK(corridor != nullptr);
    if (!corridor) return;
    CHECK_EQ(corridor->width, 13);
    CHECK_EQ(corridor->height, 9);
    // A rider as fast as the dogs (the player wins speed ties): the ambush still opens.
    CombatEngine eng(army(true, {{"gododdin_rider", 4}, {"painted_blade", 5}, {"cruth_slinger", 8},
                                 {"woad_runner", 6}, {"levy_spearman", 12}}, {"drest", "ushari"}),
                     army(false, {{"feral_dog", 3}, {"feral_dog", 3}, {"feral_dog", 2}, {"feral_dog", 2}}),
                     0, 7u, *corridor);
    CHECK_EQ(eng.map().width(), 13);
    CHECK_EQ(eng.map().height(), 9);
    CHECK_EQ(eng.layoutId(), std::string("corridor_ambush"));
    std::vector<HexCoord> seen;
    for (const auto& u : eng.playerArmy().stacks) {
        CHECK(contains(corridor->playerZone, u.pos));
        CHECK(rowOf(u.pos) >= 3 && rowOf(u.pos) <= 5);           // in the lane
        CHECK(!contains(seen, u.pos));
        seen.push_back(u.pos);
    }
    bool top = false, bottom = false;
    for (const auto& u : eng.enemyArmy().stacks) {
        CHECK(rowOf(u.pos) == 0 || rowOf(u.pos) == 8);           // out of the trees, at the edges
        top |= rowOf(u.pos) == 0;
        bottom |= rowOf(u.pos) == 8;
        CHECK(!eng.obstacleAt(u.pos));
    }
    CHECK(top && bottom);
    // Companions stand in the lane's middle row, walled in by troops.
    for (const auto& u : eng.playerArmy().stacks)
        if (u.isSpecialCharacter) CHECK_EQ(rowOf(u.pos), 4);
    // Enemy first: every enemy stack acts before any of ours in round one.
    const auto& order = eng.turnOrder();
    CHECK(!order.empty() && !order.front().isPlayer);
    bool playerSeen = false, enemyAfterPlayer = false;
    for (const auto& slot : order) {
        if (slot.isPlayer) playerSeen = true;
        else if (playerSeen) enemyAfterPlayer = true;
    }
    CHECK(!enemyAfterPlayer);
    // Trees stand where the layout puts them, with their hit points.
    CHECK_EQ(eng.obstacles().size(), corridor->obstacles.size());
    for (const auto& o : eng.obstacles()) CHECK(o.kind == "tree" && o.hp == fixture().kind("tree")->hp && o.opaque);
    // From round two on, plain initiative: the rider wins its tie with the dogs.
    while (eng.roundNumber() == 1 && !eng.isOver()) eng.doDefend();
    CHECK_EQ(eng.roundNumber(), 2);
    CHECK(eng.turnOrder().front().isPlayer && eng.turnOrder().front().stackIndex == 0);
}

SUITE("Combat layouts — the default field places armies exactly as before") {
    CombatEngine eng(army(true, {{"woad_runner", 5}}, {"ushari"}), army(false, {{"feral_dog", 3}, {"feral_dog", 3}}));
    CHECK_EQ(eng.map().width(), 11);
    CHECK_EQ(eng.map().height(), 5);
    CHECK(eng.obstacles().empty());
    CHECK(eng.playerArmy().stacks[1].pos == CombatMap::toHex(0, 2));   // the companion: back line, centre
    CHECK_EQ(eng.playerArmy().stacks[0].pos.q, 1);                       // a troop in front of it
    CHECK(eng.enemyArmy().stacks[0].pos == CombatMap::toHex(10, 2));
    CHECK(eng.enemyArmy().stacks[1].pos == CombatMap::toHex(10, 1));
}

SUITE("Deployment — stacks move inside the zone before the battle, never outside or after") {
    CombatEngine eng(army(true, {{"woad_runner", 5}, {"painted_blade", 3}}, {"ushari"}),
                     army(false, {{"feral_dog", 3}}), 0, 3u);
    const HexCoord inside = CombatMap::toHex(1, 4);
    CHECK(eng.canPlaceUnit(0, inside));
    CHECK(eng.placeUnit(0, inside));
    CHECK(eng.playerArmy().stacks[0].pos == inside);
    // Outside the zone, on the enemy's side, a dead or unknown stack: refused.
    CHECK(!eng.placeUnit(0, CombatMap::toHex(2, 2)));
    CHECK(!eng.placeUnit(0, CombatMap::toHex(10, 2)));
    CHECK(!eng.placeUnit(5, CombatMap::toHex(0, 0)));
    CHECK(!eng.placeUnit(-1, CombatMap::toHex(0, 0)));
    CHECK(eng.playerArmy().stacks[0].pos == inside);
    // Onto another of our stacks: the two swap.
    const HexCoord bladeAt = eng.playerArmy().stacks[1].pos;
    CHECK(eng.placeUnit(0, bladeAt));
    CHECK(eng.playerArmy().stacks[0].pos == bladeAt);
    CHECK(eng.playerArmy().stacks[1].pos == inside);
    // Once anyone has acted, the line is set.
    eng.doDefend();
    CHECK(eng.battleStarted());
    CHECK(!eng.canPlaceUnit(1, CombatMap::toHex(0, 0)));
    CHECK(!eng.placeUnit(1, CombatMap::toHex(0, 0)));
}

SUITE("Deployment — the layout's fieldwork zone and the obstacle kinds' hit points") {
    const CombatLayout* corridor = fixture().find("corridor_ambush");
    if (!corridor) { CHECK(corridor != nullptr); return; }
    CombatEngine eng(army(true, {{"woad_runner", 5}}), army(false, {{"feral_dog", 3}}), 0, 3u, *corridor);
    const ObstacleKind& barricade = *fixture().kind("barricade");
    // Free lane hexes take a barricade; the edge rows and the old field columns do not.
    HexCoord free{};
    for (const auto& h : corridor->playerZone)
        if (h != eng.playerArmy().stacks[0].pos) { free = h; break; }
    CHECK(eng.placeFieldwork(free, barricade));
    CHECK(eng.obstacleAt(free) && eng.obstacleAt(free)->hp == barricade.hp && eng.obstacleAt(free)->fieldwork);
    CHECK(!eng.placeFieldwork(CombatMap::toHex(1, 0), barricade));   // edge row: the enemy's
    CHECK(!eng.placeFieldwork(CombatMap::toHex(11, 4), barricade));  // lane, but outside the zone
    CHECK(!eng.placeUnit(0, free));                                     // no standing on a barricade
    CHECK(eng.removeFieldwork(free));
    CHECK(!eng.removeFieldwork(corridor->obstacles.front().cell));      // trees are not fieldworks
    // The field keeps cols 2-4.
    CombatEngine field(army(true, {{"woad_runner", 5}}), army(false, {{"feral_dog", 3}}));
    CHECK(!field.placeFieldwork(CombatMap::toHex(1, 0), barricade));
    CHECK(field.placeFieldwork(CombatMap::toHex(2, 0), barricade));
    CHECK(!field.placeFieldwork(CombatMap::toHex(5, 0), barricade));
}

SUITE("Obstacles — a melee stack breaks a tree next to it; then the hex is open") {
    const HexCoord tree = CombatMap::toHex(5, 2);
    CombatEngine eng(army(true, {{"painted_blade", 10}}), army(false, {{"levy_spearman", 1}}), 0, 11u,
                     withTrees({{5, 2}}, 30));
    eng.teleportUnit(true, 0, CombatMap::toHex(4, 2));
    eng.teleportUnit(false, 0, CombatMap::toHex(10, 0));
    CHECK(eng.currentTurn().isPlayer);
    CHECK(!eng.canMoveTo(tree));
    CHECK(eng.canAttackObstacle(tree));
    CHECK(contains(eng.attackableObstacles(), tree));
    eng.drainEvents();
    CHECK(eng.doAttackObstacle(tree));
    const auto events = eng.drainEvents();
    bool attacked = false, damaged = false;
    for (const auto& ev : events) {
        attacked |= ev.type == CombatEvent::Type::ObstacleAttacked && ev.to == tree && !ev.ranged;
        damaged  |= ev.type == CombatEvent::Type::ObstacleDamaged && ev.to == tree && ev.damage > 0;
    }
    CHECK(attacked && damaged);
    // Ten blades do 30+ a blow (3-7 each): the tree is gone, and the hex can be walked.
    CHECK(eng.obstacleAt(tree) == nullptr);
    while (!eng.currentTurn().isPlayer) eng.doDefend();
    CHECK(eng.canMoveTo(tree));
    CHECK(!eng.canAttackObstacle(tree));
}

SUITE("Obstacles — hit points wear down blow by blow, and a melee stack walks up to strike") {
    const HexCoord tree = CombatMap::toHex(5, 2);
    CombatEngine eng(army(true, {{"woad_runner", 1}}), army(false, {{"levy_spearman", 1}}), 0, 5u,
                     withTrees({{5, 2}}, 12));
    eng.teleportUnit(true, 0, CombatMap::toHex(2, 2));
    eng.teleportUnit(false, 0, CombatMap::toHex(10, 0));
    const auto from = eng.obstacleAttackHexes(tree);
    CHECK(!from.empty());
    for (const auto& h : from) CHECK(h.distanceTo(tree) == 1);
    CHECK(eng.doAttackObstacle(tree));                                  // walk, then strike
    CHECK_EQ(eng.playerArmy().stacks[0].pos.distanceTo(tree), 1);
    CHECK(eng.obstacleAt(tree) && eng.obstacleAt(tree)->hp < 12 && eng.obstacleAt(tree)->hp > 0);   // one runner: 1-3
    int blows = 1;
    while (eng.obstacleAt(tree) && blows < 20) {
        while (!eng.currentTurn().isPlayer) eng.doDefend();
        CHECK(eng.doAttackObstacle(tree));
        ++blows;
    }
    CHECK(eng.obstacleAt(tree) == nullptr);
    CHECK(blows >= 4);                                                  // at most 3 a blow, 12 hit points
}

SUITE("Obstacles — a shooter shoots a tree it can see, not one behind another") {
    const HexCoord near = CombatMap::toHex(5, 2), behind = CombatMap::toHex(7, 2);
    CombatEngine eng(army(true, {{"cruth_slinger", 10}}), army(false, {{"levy_spearman", 1}}), 0, 9u,
                     withTrees({{5, 2}, {7, 2}, {5, 0}}, 40));
    eng.teleportUnit(true, 0, CombatMap::toHex(1, 2));
    eng.teleportUnit(false, 0, CombatMap::toHex(10, 4));
    const int shots = eng.playerArmy().stacks[0].shotsLeft;
    CHECK(eng.canAttackObstacle(near));
    CHECK(!eng.canAttackObstacle(behind));                              // the near tree blocks the line
    CHECK(eng.obstacleAttackHexes(near).empty());                       // shooters shoot, they do not walk up
    CHECK(eng.doAttackObstacle(near));
    CHECK_EQ(eng.playerArmy().stacks[0].shotsLeft, shots - 1);
    CHECK(eng.playerArmy().stacks[0].pos == CombatMap::toHex(1, 2));
    bool ranged = false;
    for (const auto& ev : eng.drainEvents()) ranged |= ev.type == CombatEvent::Type::ObstacleAttacked && ev.ranged;
    CHECK(ranged);
    CHECK(!eng.obstacleAt(near) || eng.obstacleAt(near)->hp < 40);
    // Out of ammunition: only a tree next to it.
    while (!eng.currentTurn().isPlayer) eng.doDefend();
    const_cast<CombatUnit&>(eng.playerArmy().stacks[0]).shotsLeft = 0;
    CHECK(!eng.doAttackObstacleFrom(CombatMap::toHex(1, 2), CombatMap::toHex(5, 0)));
}

SUITE("Obstacles — a fieldwork placed without data cannot be broken (the connectivity guard keeps it fair)") {
    CombatEngine eng(army(true, {{"painted_blade", 10}}), army(false, {{"levy_spearman", 1}}));
    CHECK(eng.placeFieldwork(CombatMap::toHex(2, 2), true));
    eng.teleportUnit(true, 0, CombatMap::toHex(1, 2));
    CHECK(!eng.canAttackObstacle(CombatMap::toHex(2, 2)));
    CHECK(eng.attackableObstacles().empty());
}

SUITE("CombatAI — a melee side walled off by trees breaks through and the battle ends") {
    // A full wall of trees down column 5: nobody can reach anybody without chopping.
    std::vector<std::pair<int, int>> wall;
    for (int row = 0; row < 5; ++row) wall.push_back({5, row});
    int finished = 0;
    for (uint32_t seed = 1; seed <= 5; ++seed) {
        CombatEngine eng(army(true, {{"painted_blade", 8}, {"woad_runner", 10}}),
                         army(false, {{"levy_spearman", 6}, {"feral_dog", 2}}), 0, seed, withTrees(wall, 60));
        // Before anything happens, the AI names a tree of the wall to break.
        const auto target = CombatAI::breachTarget(eng);
        CHECK(target.has_value());
        if (target) {
            int col, row;
            CombatMap::toOffset(*target, col, row);
            CHECK_EQ(col, 5);
        }
        for (int n = 0; n < 2000 && !eng.isOver(); ++n) CombatAI::takeTurn(eng);
        CHECK(eng.isOver());
        CHECK(eng.roundNumber() <= 30);
        CHECK(eng.obstacles().size() < wall.size());                    // a way was cut
        finished += eng.isOver();
    }
    CHECK_EQ(finished, 5);
}

SUITE("CombatAI — with a way around, nobody chops") {
    // The same wall with a gap: the AI walks round (no breach target).
    std::vector<std::pair<int, int>> wall;
    for (int row = 0; row < 4; ++row) wall.push_back({5, row});
    CombatEngine eng(army(true, {{"painted_blade", 8}}), army(false, {{"levy_spearman", 6}}), 0, 2u, withTrees(wall, 60));
    CHECK(!CombatAI::breachTarget(eng).has_value());
    for (int n = 0; n < 2000 && !eng.isOver(); ++n) CombatAI::takeTurn(eng);
    CHECK(eng.isOver());
    CHECK_EQ(eng.obstacles().size(), wall.size());
}
