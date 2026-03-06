#include "ResourceManager.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>

using json = nlohmann::json;

// JSON key names for each Resource enum slot (matches data/units.json).
static const char* kResourceKeys[] = {
    "Gold", "SandCrystal", "BoneDust",
    "DjinnEssence", "AncientRelic", "MercuryTears", "Amber"
};
static_assert(sizeof(kResourceKeys) / sizeof(kResourceKeys[0]) == RESOURCE_COUNT,
              "kResourceKeys must match RESOURCE_COUNT");

// ── Public ────────────────────────────────────────────────────────────────────

std::optional<std::string> ResourceManager::load(const std::string& dataDir) {
    m_units.clear();
    m_spells.clear();
    m_items.clear();
    m_mineIncome.clear();
    m_unitsByTier.clear();
    m_allSpells.clear();
    m_allItems.clear();
    m_loaded = false;

    if (auto err = loadUnits    (dataDir + "/units.json"))     return err;
    if (auto err = loadSpells   (dataDir + "/spells.json"))    return err;
    if (auto err = loadBuildings(dataDir + "/buildings.json")) return err;
    if (auto err = loadItems    (dataDir + "/items.json"))     return err;

    rebuildViews();
    m_loaded = true;
    return std::nullopt;
}

const UnitType* ResourceManager::unit(const std::string& id) const {
    auto it = m_units.find(id);
    return it != m_units.end() ? &it->second : nullptr;
}

const SpellDef* ResourceManager::spell(const std::string& id) const {
    auto it = m_spells.find(id);
    return it != m_spells.end() ? &it->second : nullptr;
}

const WondrousItem* ResourceManager::item(const std::string& id) const {
    auto it = m_items.find(id);
    return it != m_items.end() ? &it->second : nullptr;
}

// ── Private ───────────────────────────────────────────────────────────────────

std::optional<std::string> ResourceManager::loadItems(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open())
        return "ResourceManager: cannot open " + path;

    json root;
    try { root = json::parse(f); }
    catch (const json::exception& e) {
        return std::string("ResourceManager: JSON parse error in ") + path + ": " + e.what();
    }

    for (const auto& j : root.value("items", json::array())) {
        WondrousItem it;
        it.id          = j.value("id",          "");
        it.name        = j.value("name",        "");
        it.description = j.value("description", "");
        it.slot        = j.value("slot",        "");
        it.cursed      = j.value("cursed",      false);

        if (j.contains("passiveEffects") && j["passiveEffects"].is_array()) {
            for (const auto& ej : j["passiveEffects"]) {
                ItemEffect e;
                e.stat   = ej.value("stat",   "");
                e.amount = ej.value("amount", 0);
                it.passiveEffects.push_back(std::move(e));
            }
        }

        if (!it.id.empty())
            m_items.emplace(it.id, std::move(it));
    }
    return std::nullopt;
}

std::optional<std::string> ResourceManager::loadUnits(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open())
        return "ResourceManager: cannot open " + path;

    json root;
    try { root = json::parse(f); }
    catch (const json::exception& e) {
        return std::string("ResourceManager: JSON parse error in ") + path + ": " + e.what();
    }

    for (const auto& j : root.value("units", json::array())) {
        UnitType u;
        u.id          = j.value("id",          "");
        u.name        = j.value("name",        "");
        u.description = j.value("description", "");
        u.tier        = j.value("tier",        1);
        u.attack      = j.value("attack",      1);
        u.defense     = j.value("defense",     1);
        u.minDamage   = j.value("minDamage",   1);
        u.maxDamage   = j.value("maxDamage",   1);
        u.hitPoints   = j.value("hitPoints",   1);
        u.speed       = j.value("speed",       4);
        u.moveRange   = j.value("moveRange",   3);
        u.shots       = j.value("shots",       0);
        u.weeklyGrowth= j.value("weeklyGrowth",0);
        u.meshId      = j.value("meshId",      "");
        u.textureId   = j.value("textureId",   "");

        if (j.contains("abilities") && j["abilities"].is_array())
            u.abilities = j["abilities"].get<std::vector<std::string>>();

        if (j.contains("cost") && j["cost"].is_object()) {
            const auto& c = j["cost"];
            for (int i = 0; i < RESOURCE_COUNT; ++i)
                u.cost[static_cast<Resource>(i)] = c.value(kResourceKeys[i], 0);
        }

        if (!u.id.empty())
            m_units.emplace(u.id, std::move(u));
    }
    return std::nullopt;
}

std::optional<std::string> ResourceManager::loadSpells(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open())
        return "ResourceManager: cannot open " + path;

    json root;
    try { root = json::parse(f); }
    catch (const json::exception& e) {
        return std::string("ResourceManager: JSON parse error in ") + path + ": " + e.what();
    }

    for (const auto& j : root.value("spells", json::array())) {
        SpellDef s;
        s.id          = j.value("id",          "");
        s.name        = j.value("name",        "");
        s.school      = j.value("school",      "");
        s.level       = j.value("level",       1);
        s.manaCost    = j.value("manaCost",    0);
        s.targeting   = j.value("targeting",   "");
        s.radius      = j.value("radius",      0);
        s.description = j.value("description", "");

        if (j.contains("effects") && j["effects"].is_array()) {
            for (const auto& ej : j["effects"]) {
                SpellEffect e;
                e.type     = ej.value("type",     "");
                e.stat     = ej.value("stat",     "");
                e.amount   = ej.value("amount",   0);
                e.duration = ej.value("duration", 0);
                e.unitId   = ej.value("unitId",   "");
                e.formula  = ej.value("formula",  "");
                e.mode     = ej.value("mode",     "");
                s.effects.push_back(std::move(e));
            }
        }

        if (!s.id.empty())
            m_spells.emplace(s.id, std::move(s));
    }
    return std::nullopt;
}

std::optional<std::string> ResourceManager::loadBuildings(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open())
        return "ResourceManager: cannot open " + path;

    json root;
    try { root = json::parse(f); }
    catch (const json::exception& e) {
        return std::string("ResourceManager: JSON parse error in ") + path + ": " + e.what();
    }

    // String → ObjType lookup table.
    static const std::unordered_map<std::string, ObjType> kObjTypeMap = {
        { "GoldMine",    ObjType::GoldMine    },
        { "CrystalMine", ObjType::CrystalMine },
        { "Town",        ObjType::Town        },
        { "Dungeon",     ObjType::Dungeon     },
        { "Artifact",    ObjType::Artifact    },
    };

    for (const auto& j : root.value("mineIncome", json::array())) {
        std::string typeName = j.value("type", "");
        auto typeIt = kObjTypeMap.find(typeName);
        if (typeIt == kObjTypeMap.end()) continue;

        ResourcePool pool;
        if (j.contains("dailyIncome") && j["dailyIncome"].is_object()) {
            const auto& inc = j["dailyIncome"];
            for (int i = 0; i < RESOURCE_COUNT; ++i)
                pool[static_cast<Resource>(i)] = inc.value(kResourceKeys[i], 0);
        }
        m_mineIncome[static_cast<int>(typeIt->second)] = pool;
    }
    return std::nullopt;
}

ResourcePool ResourceManager::mineIncome(ObjType type) const {
    auto it = m_mineIncome.find(static_cast<int>(type));
    return it != m_mineIncome.end() ? it->second : ResourcePool{};
}

void ResourceManager::rebuildViews() {
    m_unitsByTier.clear();
    for (const auto& kv : m_units)
        m_unitsByTier.push_back(&kv.second);
    std::sort(m_unitsByTier.begin(), m_unitsByTier.end(),
        [](const UnitType* a, const UnitType* b) {
            if (a->tier != b->tier) return a->tier < b->tier;
            return a->name < b->name;
        });

    m_allSpells.clear();
    for (const auto& kv : m_spells)
        m_allSpells.push_back(&kv.second);
    std::sort(m_allSpells.begin(), m_allSpells.end(),
        [](const SpellDef* a, const SpellDef* b) {
            if (a->level != b->level) return a->level < b->level;
            return a->name < b->name;
        });

    m_allItems.clear();
    for (const auto& kv : m_items)
        m_allItems.push_back(&kv.second);
    std::sort(m_allItems.begin(), m_allItems.end(),
        [](const WondrousItem* a, const WondrousItem* b) {
            return a->name < b->name;
        });
}
