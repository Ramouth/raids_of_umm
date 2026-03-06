#include "Hero.h"
#include <algorithm>

int Hero::armySize() const {
    int n = 0;
    for (const auto& slot : army)
        if (!slot.isEmpty()) ++n;
    return n;
}

bool Hero::armyFull() const {
    for (const auto& slot : army)
        if (slot.isEmpty()) return false;
    return true;
}

ArmySlot* Hero::findStack(const std::string& unitId) {
    for (auto& slot : army)
        if (!slot.isEmpty() && slot.unitType->id == unitId)
            return &slot;
    return nullptr;
}

const ArmySlot* Hero::findStack(const std::string& unitId) const {
    for (const auto& slot : army)
        if (!slot.isEmpty() && slot.unitType->id == unitId)
            return &slot;
    return nullptr;
}

bool Hero::addUnit(const UnitType* type, int count) {
    if (!type || count <= 0) return false;

    // Merge into existing stack of the same type.
    if (auto* slot = findStack(type->id)) {
        slot->count += count;
        return true;
    }

    // Place in first empty slot.
    for (auto& slot : army) {
        if (slot.isEmpty()) {
            slot.unitType = type;
            slot.count    = count;
            return true;
        }
    }
    return false;  // army full
}

bool Hero::hasItem(const std::string& id) const {
    for (const auto* it : items)
        if (it && it->id == id) return true;
    return false;
}

bool Hero::addItem(const WondrousItem* item) {
    if (!item || hasItem(item->id)) return false;
    items.push_back(item);
    return true;
}

bool Hero::knowsSpell(const std::string& id) const {
    return std::find(knownSpells.begin(), knownSpells.end(), id) != knownSpells.end();
}

void Hero::learnSpell(const std::string& id) {
    if (!knowsSpell(id))
        knownSpells.push_back(id);
}
