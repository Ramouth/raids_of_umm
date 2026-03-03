#include "test_runner.h"
#include "world/Resources.h"

// ─────────────────────────────────────────────────────────────────────────────
// Resource enum
// ─────────────────────────────────────────────────────────────────────────────

SUITE("Resource — enum values are contiguous from 0") {
    CHECK_EQ((int)Resource::Gold,         0);
    CHECK_EQ((int)Resource::SandCrystal,  1);
    CHECK_EQ((int)Resource::BoneDust,     2);
    CHECK_EQ((int)Resource::DjinnEssence, 3);
    CHECK_EQ((int)Resource::AncientRelic, 4);
    CHECK_EQ((int)Resource::MercuryTears, 5);
    CHECK_EQ((int)Resource::Amber,        6);
    CHECK_EQ(RESOURCE_COUNT,              7);
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
    p[Resource::Gold]        = 1000;
    p[Resource::SandCrystal] = 5;
    p[Resource::Amber]       = 3;

    CHECK_EQ(p[Resource::Gold],        1000);
    CHECK_EQ(p[Resource::SandCrystal], 5);
    CHECK_EQ(p[Resource::Amber],       3);
    // Unset resources remain zero
    CHECK_EQ(p[Resource::BoneDust],    0);
}

// ─────────────────────────────────────────────────────────────────────────────
// ResourcePool — arithmetic
// ─────────────────────────────────────────────────────────────────────────────

SUITE("ResourcePool — operator+") {
    ResourcePool a, b;
    a[Resource::Gold]   = 500;
    a[Resource::Amber]  = 2;
    b[Resource::Gold]   = 300;
    b[Resource::Amber]  = 1;

    ResourcePool sum = a + b;
    CHECK_EQ(sum[Resource::Gold],  800);
    CHECK_EQ(sum[Resource::Amber], 3);
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
    b[Resource::Amber] = 2;

    a += b;
    CHECK_EQ(a[Resource::Gold],  500);
    CHECK_EQ(a[Resource::Amber], 2);
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
    treasury[Resource::Amber]   = 5;
    cost[Resource::Gold]        = 500;
    cost[Resource::Amber]       = 3;
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
    treasury[Resource::Amber]   = 1;   // only 1 but need 3
    cost[Resource::Gold]        = 500;
    cost[Resource::Amber]       = 3;
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
    p[Resource::Gold]       =  500;
    p[Resource::SandCrystal] = -3;  // deficit from overspending
    p[Resource::Amber]       = -1;

    p.clampPositive();

    CHECK_EQ(p[Resource::Gold],        500);  // positive unchanged
    CHECK_EQ(p[Resource::SandCrystal], 0);    // clamped
    CHECK_EQ(p[Resource::Amber],       0);    // clamped
}
