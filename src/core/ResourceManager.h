#pragma once
#include "world/UnitType.h"
#include "world/SpellDef.h"
#include "world/WondrousItem.h"
#include "world/MapObject.h"
#include "world/Resources.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

/*
 * ResourceManager — central registry for all data loaded from JSON.
 *
 * Owns the UnitType and SpellDef tables. All other systems hold
 * const pointers into these tables; they never copy or own the data.
 *
 * Lifetime: must outlive any CombatUnit, ArmySlot, or Hero that
 * references its pointers.
 *
 * Usage:
 *   ResourceManager res;
 *   if (auto err = res.load("data")) { LOG_ERROR(*err); }
 *   const UnitType* u = res.unit("skeleton_warrior");  // nullptr if missing
 */
class ResourceManager {
public:
    // Load units.json and spells.json from dataDir.
    // Returns nullopt on success, error string on failure.
    std::optional<std::string> load(const std::string& dataDir);

    // Point lookups — return nullptr if id not found.
    const UnitType*      unit (const std::string& id) const;
    const SpellDef*      spell(const std::string& id) const;
    const WondrousItem*  item (const std::string& id) const;

    // Returns daily income for a capturable mine type (empty pool if unknown).
    ResourcePool mineIncome(ObjType type) const;

    // Ordered views for UI (palette, spellbook).
    // unitsByTier() is sorted ascending by tier, then alphabetically by name.
    const std::vector<const UnitType*>&     unitsByTier() const { return m_unitsByTier; }
    const std::vector<const SpellDef*>&     allSpells()   const { return m_allSpells; }
    const std::vector<const WondrousItem*>& allItems()    const { return m_allItems; }

    bool loaded() const { return m_loaded; }

private:
    std::unordered_map<std::string, UnitType>     m_units;
    std::unordered_map<std::string, SpellDef>     m_spells;
    std::unordered_map<std::string, WondrousItem> m_items;

    // Stable pointer views (point into m_units / m_spells / m_items after load).
    std::vector<const UnitType*>     m_unitsByTier;
    std::vector<const SpellDef*>     m_allSpells;
    std::vector<const WondrousItem*> m_allItems;

    bool m_loaded = false;

    std::optional<std::string> loadUnits    (const std::string& path);
    std::optional<std::string> loadSpells   (const std::string& path);
    std::optional<std::string> loadBuildings(const std::string& path);
    std::optional<std::string> loadItems    (const std::string& path);
    void rebuildViews();

    // Keyed by ObjType cast to int; populated by loadBuildings().
    std::unordered_map<int, ResourcePool> m_mineIncome;
};
