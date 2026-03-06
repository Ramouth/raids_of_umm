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

// ── Hero item inventory ───────────────────────────────────────────────────────

SUITE("Hero — starts with empty item inventory") {
    Hero h;
    CHECK(h.items.empty());
}

SUITE("Hero — addItem stores the pointer") {
    ResourceManager rm;
    rm.load("data");
    Hero h;
    const WondrousItem* it = rm.item("scarab_amulet");
    CHECK(h.addItem(it));
    CHECK_EQ((int)h.items.size(), 1);
    CHECK(h.items[0] == it);
}

SUITE("Hero — hasItem returns true after add") {
    ResourceManager rm;
    rm.load("data");
    Hero h;
    h.addItem(rm.item("scarab_amulet"));
    CHECK(h.hasItem("scarab_amulet"));
    CHECK(!h.hasItem("veil_of_sands"));
}

SUITE("Hero — addItem rejects duplicate id") {
    ResourceManager rm;
    rm.load("data");
    Hero h;
    const WondrousItem* it = rm.item("scarab_amulet");
    CHECK(h.addItem(it));
    CHECK(!h.addItem(it));
    CHECK_EQ((int)h.items.size(), 1);
}

SUITE("Hero — addItem rejects nullptr") {
    Hero h;
    CHECK(!h.addItem(nullptr));
    CHECK(h.items.empty());
}

SUITE("Hero — can carry multiple different items") {
    ResourceManager rm;
    rm.load("data");
    Hero h;
    h.addItem(rm.item("scarab_amulet"));
    h.addItem(rm.item("obsidian_edge"));
    h.addItem(rm.item("serpent_band"));
    CHECK_EQ((int)h.items.size(), 3);
}

SUITE("Hero — cursed item added same as normal item") {
    ResourceManager rm;
    rm.load("data");
    Hero h;
    const WondrousItem* it = rm.item("cursed_eye_of_set");
    CHECK(it != nullptr);
    CHECK(it->cursed);
    CHECK(h.addItem(it));
    CHECK(h.hasItem("cursed_eye_of_set"));
}

// ── WondrousItem loading ──────────────────────────────────────────────────────

SUITE("ResourceManager — loads all 5 items from data/items.json") {
    ResourceManager rm;
    rm.load("data");
    CHECK_EQ((int)rm.allItems().size(), 5);
}

SUITE("ResourceManager — known item lookup returns correct fields") {
    ResourceManager rm;
    rm.load("data");
    const WondrousItem* it = rm.item("scarab_amulet");
    CHECK(it != nullptr);
    CHECK(it->name == std::string("Scarab Amulet"));
    CHECK(it->slot == std::string("amulet"));
    CHECK(!it->cursed);
}

SUITE("ResourceManager — item passive effects are parsed") {
    ResourceManager rm;
    rm.load("data");
    const WondrousItem* it = rm.item("scarab_amulet");
    CHECK(it != nullptr);
    CHECK_EQ((int)it->passiveEffects.size(), 1);
    CHECK(it->passiveEffects[0].stat == std::string("attack"));
    CHECK_EQ(it->passiveEffects[0].amount, 1);
}

SUITE("ResourceManager — cursed item flag is parsed") {
    ResourceManager rm;
    rm.load("data");
    const WondrousItem* it = rm.item("cursed_eye_of_set");
    CHECK(it != nullptr);
    CHECK(it->cursed);
    CHECK_EQ((int)it->passiveEffects.size(), 2);
}

SUITE("ResourceManager — cursed item has mixed bonus and penalty") {
    ResourceManager rm;
    rm.load("data");
    const WondrousItem* it = rm.item("cursed_eye_of_set");
    CHECK(it != nullptr);
    // One positive, one negative effect.
    int positives = 0, negatives = 0;
    for (const auto& e : it->passiveEffects) {
        if (e.amount > 0) ++positives;
        if (e.amount < 0) ++negatives;
    }
    CHECK_EQ(positives, 1);
    CHECK_EQ(negatives, 1);
}

SUITE("ResourceManager — unknown item returns nullptr") {
    ResourceManager rm;
    rm.load("data");
    CHECK(rm.item("does_not_exist") == nullptr);
}

SUITE("ResourceManager — allItems sorted alphabetically by name") {
    ResourceManager rm;
    rm.load("data");
    const auto& items = rm.allItems();
    CHECK(!items.empty());
    for (size_t i = 1; i < items.size(); ++i)
        CHECK(items[i-1]->name <= items[i]->name);
}

// ── SpecialCharacter ──────────────────────────────────────────────────────────

SUITE("SpecialCharacter — default is empty") {
    SpecialCharacter sc;
    CHECK(sc.isEmpty());
    for (int i = 0; i < 4; ++i)
        CHECK(sc.equipped[i] == nullptr);
}

SUITE("SpecialCharacter — fields round-trip") {
    SpecialCharacter sc;
    sc.id        = "kharim";
    sc.name      = "Kha'Rim the Wanderer";
    sc.archetype = "tank";
    CHECK(!sc.isEmpty());
    CHECK(sc.id        == std::string("kharim"));
    CHECK(sc.name      == std::string("Kha'Rim the Wanderer"));
    CHECK(sc.archetype == std::string("tank"));
}

SUITE("SpecialCharacter — equipped slots hold item pointers") {
    ResourceManager rm;
    rm.load("data");
    SpecialCharacter sc;
    sc.id = "kharim";
    sc.equipped[0] = rm.item("scarab_amulet");
    CHECK(sc.equipped[0] != nullptr);
    CHECK(sc.equipped[0]->id == std::string("scarab_amulet"));
    CHECK(sc.equipped[1] == nullptr);
}

// ── Hero SC party management ──────────────────────────────────────────────────

SUITE("Hero — starts with empty specials") {
    Hero h;
    CHECK(h.specials.empty());
    CHECK(!h.scFull());
}

SUITE("Hero — addSpecial stores an SC") {
    Hero h;
    SpecialCharacter sc;
    sc.id        = "kharim";
    sc.name      = "Kha'Rim the Wanderer";
    sc.archetype = "tank";
    CHECK(h.addSpecial(sc));
    CHECK_EQ((int)h.specials.size(), 1);
    CHECK(h.specials[0].id == std::string("kharim"));
}

SUITE("Hero — addSpecial rejects empty id") {
    Hero h;
    SpecialCharacter sc;   // id is ""
    CHECK(!h.addSpecial(sc));
    CHECK(h.specials.empty());
}

SUITE("Hero — addSpecial rejects duplicate id") {
    Hero h;
    SpecialCharacter sc;
    sc.id = "kharim";
    CHECK(h.addSpecial(sc));
    CHECK(!h.addSpecial(sc));
    CHECK_EQ((int)h.specials.size(), 1);
}

SUITE("Hero — scFull after 3 specials, rejects 4th") {
    Hero h;
    for (int i = 0; i < Hero::SC_SLOTS; ++i) {
        SpecialCharacter sc;
        sc.id = std::string("sc") + std::to_string(i);
        CHECK(h.addSpecial(sc));
    }
    CHECK(h.scFull());
    SpecialCharacter extra;
    extra.id = "extra";
    CHECK(!h.addSpecial(extra));
    CHECK_EQ((int)h.specials.size(), 3);
}

SUITE("Hero — findSpecial returns pointer when present") {
    Hero h;
    SpecialCharacter sc;
    sc.id        = "kharim";
    sc.archetype = "tank";
    h.addSpecial(sc);
    SpecialCharacter* found = h.findSpecial("kharim");
    CHECK(found != nullptr);
    CHECK(found->archetype == std::string("tank"));
}

SUITE("Hero — findSpecial returns nullptr when absent") {
    Hero h;
    CHECK(h.findSpecial("nobody") == nullptr);
}

SUITE("Hero — findSpecial const overload works") {
    Hero h;
    SpecialCharacter sc;
    sc.id = "kharim";
    h.addSpecial(sc);
    const Hero& ch = h;
    CHECK(ch.findSpecial("kharim") != nullptr);
    CHECK(ch.findSpecial("other")  == nullptr);
}

#endif // RESOURCE_MANAGER_IMPL
