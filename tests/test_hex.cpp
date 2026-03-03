#include "test_runner.h"
#include "hex/HexCoord.h"
#include "hex/HexGrid.h"
#include <unordered_set>

// ─────────────────────────────────────────────────────────────────────────────
// HexCoord
// ─────────────────────────────────────────────────────────────────────────────

SUITE("HexCoord — cube invariant") {
    // s must always equal -(q+r)
    HexCoord a{3, -1};
    CHECK_EQ(a.s(), -3 - (-1));  // s = -(3 + -1) = -2

    HexCoord b{0, 0};
    CHECK_EQ(b.s(), 0);

    HexCoord c{-5, 5};
    CHECK_EQ(c.s(), 0);
}

SUITE("HexCoord — arithmetic") {
    HexCoord a{2, -1};
    HexCoord b{1, 3};

    HexCoord sum = a + b;
    CHECK_EQ(sum.q, 3);
    CHECK_EQ(sum.r, 2);

    HexCoord diff = a - b;
    CHECK_EQ(diff.q, 1);
    CHECK_EQ(diff.r, -4);

    HexCoord scaled = a * 3;
    CHECK_EQ(scaled.q, 6);
    CHECK_EQ(scaled.r, -3);
}

SUITE("HexCoord — equality") {
    CHECK(HexCoord(1, 2) == HexCoord(1, 2));
    CHECK(HexCoord(1, 2) != HexCoord(1, 3));
    CHECK(HexCoord(0, 0) == HexCoord(0, 0));
}

SUITE("HexCoord — distance") {
    // distance from origin to self is 0
    CHECK_EQ(HexCoord(0, 0).distanceTo(HexCoord(0, 0)), 0);

    // adjacent hex is distance 1 in all 6 directions
    for (const auto& dir : HexCoord::directions()) {
        CHECK_EQ(HexCoord(0, 0).distanceTo(dir), 1);
    }

    // known distances
    CHECK_EQ(HexCoord(0, 0).distanceTo(HexCoord(3, 0)), 3);
    CHECK_EQ(HexCoord(0, 0).distanceTo(HexCoord(3, -3)), 3);
    CHECK_EQ(HexCoord(2, -4).distanceTo(HexCoord(-2, 4)), 8);
}

SUITE("HexCoord — neighbor") {
    HexCoord origin{0, 0};

    // Each neighbor is distance 1 from origin
    for (int d = 0; d < 6; ++d) {
        HexCoord n = origin.neighbor(d);
        CHECK_EQ(origin.distanceTo(n), 1);
    }

    // No two neighbors are the same (pairwise — doesn't rely on hash)
    for (int i = 0; i < 6; ++i)
        for (int j = i + 1; j < 6; ++j)
            CHECK(origin.neighbor(i) != origin.neighbor(j));

    // Wraps: neighbor(6) == neighbor(0), neighbor(-1) == neighbor(5)
    CHECK(origin.neighbor(6)  == origin.neighbor(0));
    CHECK(origin.neighbor(-1) == origin.neighbor(5));
}

SUITE("HexCoord — world round-trip") {
    // toWorld then fromWorld must return the original coord.
    float hexSize = 1.5f;
    HexCoord coords[] = { {0,0}, {3,-2}, {-5,4}, {1,1}, {0,7} };
    for (const auto& c : coords) {
        float wx, wz;
        c.toWorld(hexSize, wx, wz);
        HexCoord back = HexCoord::fromWorld(wx, wz, hexSize);
        CHECK(back == c);
    }
}

SUITE("HexCoord — hash uniqueness") {
    // Adjacent coords must not collide (not a strict guarantee but
    // any collision here would be a terrible hash)
    std::unordered_set<HexCoord> set;
    for (int q = -5; q <= 5; ++q)
        for (int r = -5; r <= 5; ++r)
            set.insert(HexCoord{q, r});
    // 121 distinct coords should all hash differently
    CHECK_EQ((int)set.size(), 121);
}

// ─────────────────────────────────────────────────────────────────────────────
// HexGrid<int>  (T = int for simplicity)
// ─────────────────────────────────────────────────────────────────────────────

SUITE("HexGrid — set / get / has / remove") {
    HexGrid<int> g;

    CHECK(!g.has({0, 0}));
    g.set({0, 0}, 42);
    CHECK(g.has({0, 0}));

    int* val = g.get({0, 0});
    CHECK(val != nullptr);
    CHECK_EQ(*val, 42);

    // Absent cell returns null
    CHECK(g.get({9, 9}) == nullptr);

    g.remove({0, 0});
    CHECK(!g.has({0, 0}));
    CHECK(g.get({0, 0}) == nullptr);
}

SUITE("HexGrid — cell count") {
    HexGrid<int> g;
    CHECK_EQ((int)g.cellCount(), 0);
    g.set({0,0}, 1);
    g.set({1,0}, 2);
    g.set({0,1}, 3);
    CHECK_EQ((int)g.cellCount(), 3);
    g.remove({1,0});
    CHECK_EQ((int)g.cellCount(), 2);
}

SUITE("HexGrid — neighbors (no passability filter)") {
    HexGrid<int> g;
    // Place origin + all 6 neighbors
    g.set({0,0}, 0);
    for (const auto& dir : HexCoord::directions())
        g.set(dir, 1);

    auto nbrs = g.neighbors({0,0});
    CHECK_EQ((int)nbrs.size(), 6);

    // Remove one neighbor — count drops
    g.remove(HexCoord::directions()[0]);
    nbrs = g.neighbors({0,0});
    CHECK_EQ((int)nbrs.size(), 5);
}

SUITE("HexGrid — neighbors with passability filter") {
    HexGrid<int> g;
    g.set({0,0}, 0);
    auto dirs = HexCoord::directions();
    for (int d = 0; d < 6; ++d)
        g.set(dirs[d], d);   // values 0–5

    // Only pass neighbors whose value > 2  → dirs 3,4,5
    auto nbrs = g.neighbors({0,0}, [](const HexCoord&, const int& v) {
        return v > 2;
    });
    CHECK_EQ((int)nbrs.size(), 3);
}

SUITE("HexGrid — ring") {
    HexGrid<int> g;
    // Fill a radius-3 hex area
    for (int q = -3; q <= 3; ++q) {
        int r1 = std::max(-3, -q - 3);
        int r2 = std::min( 3, -q + 3);
        for (int r = r1; r <= r2; ++r)
            g.set({q, r}, 1);
    }

    // ring(0) = just the center
    auto r0 = g.ring({0,0}, 0);
    CHECK_EQ((int)r0.size(), 1);

    // ring(1) = 6 tiles
    auto r1 = g.ring({0,0}, 1);
    CHECK_EQ((int)r1.size(), 6);

    // ring(2) = 12 tiles
    auto r2 = g.ring({0,0}, 2);
    CHECK_EQ((int)r2.size(), 12);

    // ring(3) = 18 tiles
    auto r3 = g.ring({0,0}, 3);
    CHECK_EQ((int)r3.size(), 18);
}

SUITE("HexGrid — range") {
    HexGrid<int> g;
    for (int q = -3; q <= 3; ++q) {
        int r1 = std::max(-3, -q - 3);
        int r2 = std::min( 3, -q + 3);
        for (int r = r1; r <= r2; ++r)
            g.set({q, r}, 1);
    }

    // range(0) = 1 tile
    CHECK_EQ((int)g.range({0,0}, 0).size(), 1);
    // range(1) = 7 tiles
    CHECK_EQ((int)g.range({0,0}, 1).size(), 7);
    // range(2) = 19 tiles
    CHECK_EQ((int)g.range({0,0}, 2).size(), 19);
}

// ─────────────────────────────────────────────────────────────────────────────
// HexGrid — A* pathfinding
// ─────────────────────────────────────────────────────────────────────────────

// Build a small passable grid (radius 5), value=1 means passable
static HexGrid<int> makeGrid(int radius) {
    HexGrid<int> g;
    for (int q = -radius; q <= radius; ++q) {
        int r1 = std::max(-radius, -q - radius);
        int r2 = std::min( radius, -q + radius);
        for (int r = r1; r <= r2; ++r)
            g.set({q, r}, 1);
    }
    return g;
}

static auto alwaysPass = [](const HexCoord&, const int& v) { return v == 1; };

SUITE("HexGrid::findPath — start == goal") {
    auto g = makeGrid(3);
    auto path = g.findPath({0,0}, {0,0}, alwaysPass);
    CHECK_EQ((int)path.size(), 1);
    CHECK(path[0] == HexCoord(0,0));
}

SUITE("HexGrid::findPath — adjacent tiles") {
    auto g = makeGrid(3);
    auto path = g.findPath({0,0}, {1,0}, alwaysPass);
    CHECK_EQ((int)path.size(), 2);
    CHECK(path.front() == HexCoord(0,0));
    CHECK(path.back()  == HexCoord(1,0));
}

SUITE("HexGrid::findPath — known distance") {
    auto g = makeGrid(5);
    // Straight-line path from (0,0) to (3,0) should be 4 steps (3 moves)
    auto path = g.findPath({0,0}, {3,0}, alwaysPass);
    CHECK_EQ((int)path.size(), 4);   // includes start and end
    CHECK(path.front() == HexCoord(0,0));
    CHECK(path.back()  == HexCoord(3,0));
}

SUITE("HexGrid::findPath — path around obstacle") {
    auto g = makeGrid(3);

    // Block the direct path between (0,0) and (2,0):
    // mark (1,0) impassable (value = 0)
    g.set({1,0}, 0);

    auto path = g.findPath({0,0}, {2,0}, alwaysPass);

    // A path must exist (going around)
    CHECK(!path.empty());
    CHECK(path.front() == HexCoord(0,0));
    CHECK(path.back()  == HexCoord(2,0));

    // The blocked tile must not be in the path
    bool containsBlocked = false;
    for (const auto& c : path)
        if (c == HexCoord(1,0)) containsBlocked = true;
    CHECK(!containsBlocked);
}

SUITE("HexGrid::findPath — no path returns empty") {
    auto g = makeGrid(3);

    // Completely surround (0,0) with impassable tiles
    for (const auto& dir : HexCoord::directions())
        g.set(dir, 0);

    auto path = g.findPath({0,0}, {2,0}, alwaysPass);
    CHECK(path.empty());
}

SUITE("HexGrid::findPath — goal outside grid") {
    auto g = makeGrid(2);
    // {10,10} is not in the grid
    auto path = g.findPath({0,0}, {10,10}, alwaysPass);
    CHECK(path.empty());
}
