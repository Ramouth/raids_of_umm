#include "test_runner.h"

#ifdef RESOURCE_MANAGER_IMPL
#include "core/ResourceManager.h"
#include "hero/Hero.h"

// ── ResourceManager loading ───────────────────────────────────────────────────

SUITE("ResourceManager — loads without error") {
    ResourceManager rm;
    auto err = rm.load("data");
    CHECK(!err.has_value());
    CHECK(rm.loaded());
}

SUITE("ResourceManager — known unit lookup") {
    ResourceManager rm;
    rm.load("data");
    const UnitType* u = rm.unit("skeleton_warrior");
    CHECK(u != nullptr);
    CHECK(u->name == std::string("Skeleton Warrior"));
    CHECK_EQ(u->tier, 1);
}

SUITE("ResourceManager — unit stats are parsed correctly") {
    ResourceManager rm;
    rm.load("data");
    const UnitType* u = rm.unit("skeleton_warrior");
    CHECK(u != nullptr);
    CHECK_EQ(u->attack,    5);
    CHECK_EQ(u->defense,   4);
    CHECK_EQ(u->hitPoints, 6);
    CHECK_EQ(u->speed,     5);
    CHECK_EQ(u->shots,     0);
}

SUITE("ResourceManager — unit cost is parsed") {
    ResourceManager rm;
    rm.load("data");
    const UnitType* u = rm.unit("skeleton_warrior");
    CHECK(u != nullptr);
    CHECK_EQ(u->cost[Resource::Gold], 60);
    CHECK_EQ(u->cost[Resource::BoneDust], 0);
}

SUITE("ResourceManager — abilities are parsed") {
    ResourceManager rm;
    rm.load("data");
    const UnitType* u = rm.unit("skeleton_warrior");
    CHECK(u != nullptr);
    CHECK(u->hasAbility("undead"));
    CHECK(!u->hasAbility("flying"));
}

SUITE("ResourceManager — ranged unit has shots") {
    ResourceManager rm;
    rm.load("data");
    const UnitType* u = rm.unit("desert_archer");
    CHECK(u != nullptr);
    CHECK(u->isRanged());
    CHECK_EQ(u->shots, 24);
}

SUITE("ResourceManager — unknown unit returns nullptr") {
    ResourceManager rm;
    rm.load("data");
    CHECK(rm.unit("does_not_exist") == nullptr);
}

SUITE("ResourceManager — unitsByTier is sorted ascending") {
    ResourceManager rm;
    rm.load("data");
    const auto& units = rm.unitsByTier();
    CHECK(!units.empty());
    for (size_t i = 1; i < units.size(); ++i)
        CHECK(units[i-1]->tier <= units[i]->tier);
}

SUITE("ResourceManager — loads all 7 units from data/units.json") {
    ResourceManager rm;
    rm.load("data");
    CHECK_EQ((int)rm.unitsByTier().size(), 7);
}

SUITE("ResourceManager — known spell lookup") {
    ResourceManager rm;
    rm.load("data");
    const SpellDef* s = rm.spell("sandstorm");
    CHECK(s != nullptr);
    CHECK(s->name == std::string("Sandstorm"));
    CHECK(s->school == std::string("earth"));
    CHECK_EQ(s->manaCost, 9);
}

SUITE("ResourceManager — spell effects are parsed") {
    ResourceManager rm;
    rm.load("data");
    const SpellDef* s = rm.spell("sandstorm");
    CHECK(s != nullptr);
    CHECK_EQ((int)s->effects.size(), 2);
    CHECK(s->effects[0].type == std::string("debuff"));
}

SUITE("ResourceManager — unknown spell returns nullptr") {
    ResourceManager rm;
    rm.load("data");
    CHECK(rm.spell("does_not_exist") == nullptr);
}

SUITE("ResourceManager — allSpells sorted by level") {
    ResourceManager rm;
    rm.load("data");
    const auto& spells = rm.allSpells();
    CHECK(!spells.empty());
    for (size_t i = 1; i < spells.size(); ++i)
        CHECK(spells[i-1]->level <= spells[i]->level);
}

// ── Hero army management ──────────────────────────────────────────────────────

SUITE("Hero — starts with empty army") {
    Hero h;
    CHECK_EQ(h.armySize(), 0);
    CHECK(!h.armyFull());
}

SUITE("Hero — addUnit fills a slot") {
    ResourceManager rm;
    rm.load("data");
    Hero h;
    const UnitType* u = rm.unit("skeleton_warrior");
    bool ok = h.addUnit(u, 10);
    CHECK(ok);
    CHECK_EQ(h.armySize(), 1);
    CHECK(!h.army[0].isEmpty());
    CHECK_EQ(h.army[0].count, 10);
}

SUITE("Hero — addUnit merges same type") {
    ResourceManager rm;
    rm.load("data");
    Hero h;
    const UnitType* u = rm.unit("skeleton_warrior");
    h.addUnit(u, 5);
    h.addUnit(u, 3);
    CHECK_EQ(h.armySize(), 1);   // still one slot
    CHECK_EQ(h.army[0].count, 8);
}

SUITE("Hero — addUnit uses separate slots for different types") {
    ResourceManager rm;
    rm.load("data");
    Hero h;
    h.addUnit(rm.unit("skeleton_warrior"), 5);
    h.addUnit(rm.unit("mummy"), 2);
    CHECK_EQ(h.armySize(), 2);
}

SUITE("Hero — findStack returns existing slot") {
    ResourceManager rm;
    rm.load("data");
    Hero h;
    h.addUnit(rm.unit("skeleton_warrior"), 5);
    ArmySlot* slot = h.findStack("skeleton_warrior");
    CHECK(slot != nullptr);
    CHECK_EQ(slot->count, 5);
}

SUITE("Hero — findStack returns nullptr for absent type") {
    Hero h;
    CHECK(h.findStack("anything") == nullptr);
}

SUITE("Hero — addUnit returns false when army full with new type") {
    ResourceManager rm;
    rm.load("data");
    Hero h;
    const char* ids[] = {
        "skeleton_warrior","mummy","djinn",
        "sand_scorpion","desert_archer","ancient_guardian","pharaoh_lich"
    };
    for (const char* id : ids)
        h.addUnit(rm.unit(id), 1);
    CHECK(h.armyFull());
    // A genuinely new unit type (not yet in any slot) must fail to add.
    UnitType newType;
    newType.id = "ghost";
    newType.hitPoints = 1;
    CHECK(!h.addUnit(&newType, 1));
}

SUITE("Hero — spellbook learn and query") {
    Hero h;
    CHECK(!h.knowsSpell("sandstorm"));
    h.learnSpell("sandstorm");
    CHECK(h.knowsSpell("sandstorm"));
    h.learnSpell("sandstorm");   // duplicate — should not double-add
    CHECK_EQ((int)h.knownSpells.size(), 1);
}

SUITE("Hero — wallet is initially zeroed") {
    Hero h;
    for (int i = 0; i < RESOURCE_COUNT; ++i)
        CHECK_EQ(h.wallet[static_cast<Resource>(i)], 0);
}

#endif // RESOURCE_MANAGER_IMPL
