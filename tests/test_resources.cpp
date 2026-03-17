#include "test_runner.h"
#include "world/Resources.h"

// ─────────────────────────────────────────────────────────────────────────────
// Resource enum
// ─────────────────────────────────────────────────────────────────────────────

SUITE("Resource — enum values are contiguous from 0") {
    CHECK_EQ((int)Resource::Gold,     0);
    CHECK_EQ((int)Resource::Wood,     1);
    CHECK_EQ((int)Resource::Stone,    2);
    CHECK_EQ((int)Resource::Obsidian, 3);
    CHECK_EQ((int)Resource::Crystal,  4);
    CHECK_EQ(RESOURCE_COUNT,          5);
}

SUITE("Resource — resourceName covers all values") {
    // Every valid enum value must return a non-empty name
    for (int i = 0; i < RESOURCE_COUNT; ++i) {
        auto name = resourceName(static_cast<Resource>(i));
        CHECK(!name.empty());
    }
    // Out-of-range returns "Unknown"
    auto bad = resourceName(static_cast<Resource>(99));
    CHECK(bad == "Unknown");
}

// ─────────────────────────────────────────────────────────────────────────────
// ResourcePool — construction
// ─────────────────────────────────────────────────────────────────────────────

SUITE("ResourcePool — default construction is all zeros") {
    ResourcePool p;
    for (int i = 0; i < RESOURCE_COUNT; ++i)
        CHECK_EQ(p[static_cast<Resource>(i)], 0);
}

SUITE("ResourcePool — operator[] read / write") {
    ResourcePool p;
    p[Resource::Gold]    = 1000;
    p[Resource::Wood]    = 5;
    p[Resource::Crystal] = 3;

    CHECK_EQ(p[Resource::Gold],    1000);
    CHECK_EQ(p[Resource::Wood],    5);
    CHECK_EQ(p[Resource::Crystal], 3);
    // Unset resources remain zero
    CHECK_EQ(p[Resource::Stone],   0);
}

// ─────────────────────────────────────────────────────────────────────────────
// ResourcePool — arithmetic
// ─────────────────────────────────────────────────────────────────────────────

SUITE("ResourcePool — operator+") {
    ResourcePool a, b;
    a[Resource::Gold]    = 500;
    a[Resource::Obsidian] = 2;
    b[Resource::Gold]    = 300;
    b[Resource::Obsidian] = 1;

    ResourcePool sum = a + b;
    CHECK_EQ(sum[Resource::Gold],    800);
    CHECK_EQ(sum[Resource::Obsidian], 3);
    // Original pools unchanged
    CHECK_EQ(a[Resource::Gold],  500);
    CHECK_EQ(b[Resource::Gold],  300);
}

SUITE("ResourcePool — operator-") {
    ResourcePool a, b;
    a[Resource::Gold] = 1000;
    b[Resource::Gold] = 250;

    ResourcePool diff = a - b;
    CHECK_EQ(diff[Resource::Gold], 750);
}

SUITE("ResourcePool — operator+=") {
    ResourcePool a, b;
    a[Resource::Gold] = 400;
    b[Resource::Gold] = 100;
    b[Resource::Obsidian] = 2;

    a += b;
    CHECK_EQ(a[Resource::Gold],    500);
    CHECK_EQ(a[Resource::Obsidian], 2);
}

SUITE("ResourcePool — operator-=") {
    ResourcePool a, b;
    a[Resource::Gold] = 400;
    b[Resource::Gold] = 100;

    a -= b;
    CHECK_EQ(a[Resource::Gold], 300);
}

// ─────────────────────────────────────────────────────────────────────────────
// ResourcePool — canAfford
// ─────────────────────────────────────────────────────────────────────────────

SUITE("ResourcePool — canAfford: sufficient funds") {
    ResourcePool treasury, cost;
    treasury[Resource::Gold]    = 1000;
    treasury[Resource::Obsidian] = 5;
    cost[Resource::Gold]        = 500;
    cost[Resource::Obsidian]    = 3;
    CHECK(treasury.canAfford(cost));
}

SUITE("ResourcePool — canAfford: exact funds") {
    ResourcePool treasury, cost;
    treasury[Resource::Gold] = 500;
    cost[Resource::Gold]     = 500;
    CHECK(treasury.canAfford(cost));
}

SUITE("ResourcePool — canAfford: insufficient one resource") {
    ResourcePool treasury, cost;
    treasury[Resource::Gold]    = 1000;
    treasury[Resource::Obsidian] = 1;   // only 1 but need 3
    cost[Resource::Gold]        = 500;
    cost[Resource::Obsidian]    = 3;
    CHECK(!treasury.canAfford(cost));
}

SUITE("ResourcePool — canAfford: insufficient gold") {
    ResourcePool treasury, cost;
    treasury[Resource::Gold] = 200;
    cost[Resource::Gold]     = 500;
    CHECK(!treasury.canAfford(cost));
}

SUITE("ResourcePool — canAfford: empty cost is always affordable") {
    ResourcePool treasury;   // all zeros
    ResourcePool cost;       // all zeros
    CHECK(treasury.canAfford(cost));
}

// ─────────────────────────────────────────────────────────────────────────────
// ResourcePool — clampPositive
// ─────────────────────────────────────────────────────────────────────────────

SUITE("ResourcePool — clampPositive removes negatives") {
    ResourcePool p;
    p[Resource::Gold]    =  500;
    p[Resource::Wood]    = -3;  // deficit from overspending
    p[Resource::Obsidian] = -1;

    p.clampPositive();

    CHECK_EQ(p[Resource::Gold],    500);  // positive unchanged
    CHECK_EQ(p[Resource::Wood],    0);    // clamped
    CHECK_EQ(p[Resource::Obsidian], 0);   // clamped
}
