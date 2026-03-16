#pragma once
#include "hex/HexCoord.h"
#include "world/Resources.h"
#include "world/WondrousItem.h"
#include "world/UnitType.h"
#include "world/SpecialCharacter.h"
#include "world/Faction.h"
#include <string>
#include <array>
#include <vector>

/*
 * ArmySlot — one of a hero's 7 unit stack slots.
 *
 * unitType points into the ResourceManager registry (never owned here).
 * count is the number of creatures in the stack.
 */
struct ArmySlot {
    const UnitType* unitType = nullptr;
    int             count    = 0;

    bool isEmpty() const { return unitType == nullptr || count <= 0; }
};

/*
 * Hero — a named character on the adventure map.
 *
 * Owns: position, movement budget, army slots, spellbook, personal wallet.
 *
 * Does NOT include anything from render/, core/, or adventure/.
 * May only depend on hex/, world/, and entities/ headers.
 *
 * Pointers in army[] are stable for the lifetime of the ResourceManager
 * that provided them. Never copy a Hero across ResourceManager lifetimes.
 */
struct Hero {
    // ── Identity ──────────────────────────────────────────────────────────────
    std::string name = "Hero";
    int factionId = Faction::Player;

    // ── Adventure map ─────────────────────────────────────────────────────────
    HexCoord pos       { 0, 0 };
    int      movesMax  { 8 };
    int      movesLeft { 8 };

    void resetMoves()             { movesLeft = movesMax; }
    bool hasMoves()         const { return movesLeft > 0; }
    bool canReach(int steps) const { return steps <= movesLeft; }

    // ── Army (up to 7 stacks, HoMM3-style) ───────────────────────────────────
    static constexpr int ARMY_SLOTS = 7;
    std::array<ArmySlot, ARMY_SLOTS> army{};

    // Number of non-empty slots.
    int  armySize() const;

    // True when all 7 slots are occupied.
    bool armyFull() const;

    // Find an existing stack of this unit type (first match), or nullptr.
    ArmySlot*       findStack(const std::string& unitId);
    const ArmySlot* findStack(const std::string& unitId) const;

    // Add units to an existing stack or the first empty slot.
    // Returns false if no empty slot available (army full with different types).
    bool addUnit(const UnitType* type, int count);

    // ── Item inventory ────────────────────────────────────────────────────────
    std::vector<const WondrousItem*> items;

    // Add item to inventory. Returns false if hero already carries this id.
    bool addItem(const WondrousItem* item);

    // True if inventory contains an item with this id.
    bool hasItem(const std::string& id) const;

    // ── Special Characters (up to 3 party members) ───────────────────────────
    static constexpr int SC_SLOTS = 3;
    std::vector<SpecialCharacter> specials;  // max SC_SLOTS entries

    // True when all SC slots are occupied.
    bool scFull() const;

    // Add an SC. Returns false if party is full or id is already present.
    bool addSpecial(const SpecialCharacter& sc);

    // Find an SC by id. Returns nullptr if not in party.
    SpecialCharacter*       findSpecial(const std::string& id);
    const SpecialCharacter* findSpecial(const std::string& id) const;

    // ── Spellbook ─────────────────────────────────────────────────────────────
    int spellPower = 1;
    int mana       = 10;
    int manaMax    = 10;
    std::vector<std::string> knownSpells;  // spell IDs from ResourceManager

    bool knowsSpell(const std::string& id) const;
    void learnSpell(const std::string& id);

    // ── Personal wallet ───────────────────────────────────────────────────────
    // Separate from the town treasury; travels with the hero.
    ResourcePool wallet;
};
