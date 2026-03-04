#include "RenderOffsets.h"

#include <fstream>
#include <sstream>

// nlohmann/json — header-only, included via local_deps
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// ── Helpers ──────────────────────────────────────────────────────────────────

static std::string coordKey(const HexCoord& c) {
    return std::to_string(c.q) + "," + std::to_string(c.r);
}

static HexCoord coordFromKey(const std::string& key) {
    auto comma = key.find(',');
    int q = std::stoi(key.substr(0, comma));
    int r = std::stoi(key.substr(comma + 1));
    return {q, r};
}

static json offsetToJson(const RenderOffset& o) {
    return json{{"dx", o.dx}, {"dz", o.dz}, {"dy", o.dy}};
}

static RenderOffset offsetFromJson(const json& j) {
    RenderOffset o;
    o.dx = j.value("dx", 0.0f);
    o.dz = j.value("dz", 0.0f);
    o.dy = j.value("dy", 0.0f);
    return o;
}

// ── Lookup ───────────────────────────────────────────────────────────────────

RenderOffset RenderOffsetConfig::forTerrain(const HexCoord& c, Terrain t) const {
    auto it = tileOverride.find(c);
    if (it != tileOverride.end()) return it->second;
    return terrainGlobal[static_cast<int>(t)];
}

RenderOffset RenderOffsetConfig::forObject(const HexCoord& c, ObjType t) const {
    auto it = objectOverride.find(c);
    if (it != objectOverride.end()) return it->second;
    return objectGlobal[static_cast<int>(t)];
}

RenderOffset& RenderOffsetConfig::terrainRef(const HexCoord& c, Terrain t, bool perTile) {
    if (perTile) return tileOverride[c];          // inserts zero entry if missing
    return terrainGlobal[static_cast<int>(t)];
}

RenderOffset& RenderOffsetConfig::objectRef(const HexCoord& c, ObjType t, bool perTile) {
    if (perTile) return objectOverride[c];
    return objectGlobal[static_cast<int>(t)];
}

// ── Persistence ──────────────────────────────────────────────────────────────

static const char* terrainKey(Terrain t) {
    switch (t) {
        case Terrain::Sand:     return "Sand";
        case Terrain::Dune:     return "Dune";
        case Terrain::Rock:     return "Rock";
        case Terrain::Oasis:    return "Oasis";
        case Terrain::Ruins:    return "Ruins";
        case Terrain::Obsidian: return "Obsidian";
        case Terrain::Mountain: return "Mountain";
        case Terrain::River:    return "River";
        case Terrain::Wall:     return "Wall";
        case Terrain::Battle:   return "Battle";
        default:                return "Unknown";
    }
}

static const char* objKey(ObjType t) {
    switch (t) {
        case ObjType::Town:        return "Town";
        case ObjType::Dungeon:     return "Dungeon";
        case ObjType::GoldMine:    return "GoldMine";
        case ObjType::CrystalMine: return "CrystalMine";
        case ObjType::Artifact:    return "Artifact";
        default:                   return "Unknown";
    }
}

std::optional<std::string> RenderOffsetConfig::save(const std::string& path) const {
    json root;

    // terrainGlobal
    json tg;
    for (int i = 0; i < TERRAIN_COUNT; ++i)
        tg[terrainKey(static_cast<Terrain>(i))] = offsetToJson(terrainGlobal[i]);
    root["terrainGlobal"] = tg;

    // objectGlobal
    json og;
    for (int i = 0; i < OBJ_TYPE_COUNT; ++i)
        og[objKey(static_cast<ObjType>(i))] = offsetToJson(objectGlobal[i]);
    root["objectGlobal"] = og;

    // tileOverride
    json to_map;
    for (auto& [coord, off] : tileOverride)
        to_map[coordKey(coord)] = offsetToJson(off);
    root["tileOverride"] = to_map;

    // objectOverride
    json oo_map;
    for (auto& [coord, off] : objectOverride)
        oo_map[coordKey(coord)] = offsetToJson(off);
    root["objectOverride"] = oo_map;

    std::ofstream f(path);
    if (!f.is_open())
        return std::string("Cannot write: ") + path;
    f << root.dump(2);
    return std::nullopt;
}

std::optional<std::string> RenderOffsetConfig::load(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return std::nullopt;  // missing file = fresh start, not an error

    json root;
    try { root = json::parse(f); }
    catch (const std::exception& e) {
        return std::string("JSON parse error in ") + path + ": " + e.what();
    }

    // terrainGlobal
    if (root.contains("terrainGlobal")) {
        static const std::pair<const char*, Terrain> tmap[] = {
            {"Sand",Terrain::Sand},{"Dune",Terrain::Dune},{"Rock",Terrain::Rock},
            {"Oasis",Terrain::Oasis},{"Ruins",Terrain::Ruins},{"Obsidian",Terrain::Obsidian},
            {"Mountain",Terrain::Mountain},{"River",Terrain::River},
            {"Wall",Terrain::Wall},{"Battle",Terrain::Battle}
        };
        for (auto& [k, t] : tmap)
            if (root["terrainGlobal"].contains(k))
                terrainGlobal[static_cast<int>(t)] = offsetFromJson(root["terrainGlobal"][k]);
    }

    // objectGlobal
    if (root.contains("objectGlobal")) {
        static const std::pair<const char*, ObjType> omap[] = {
            {"Town",ObjType::Town},{"Dungeon",ObjType::Dungeon},
            {"GoldMine",ObjType::GoldMine},{"CrystalMine",ObjType::CrystalMine},
            {"Artifact",ObjType::Artifact}
        };
        for (auto& [k, t] : omap)
            if (root["objectGlobal"].contains(k))
                objectGlobal[static_cast<int>(t)] = offsetFromJson(root["objectGlobal"][k]);
    }

    // tileOverride
    tileOverride.clear();
    if (root.contains("tileOverride"))
        for (auto& [k, v] : root["tileOverride"].items())
            tileOverride[coordFromKey(k)] = offsetFromJson(v);

    // objectOverride
    objectOverride.clear();
    if (root.contains("objectOverride"))
        for (auto& [k, v] : root["objectOverride"].items())
            objectOverride[coordFromKey(k)] = offsetFromJson(v);

    return std::nullopt;
}

void RenderOffsetConfig::reset() {
    terrainGlobal.fill({});
    objectGlobal.fill({});
    tileOverride.clear();
    objectOverride.clear();
}
