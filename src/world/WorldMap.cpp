#include "WorldMap.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <stdexcept>

using json = nlohmann::json;

// ── Internal helpers ──────────────────────────────────────────────────────────

// Deterministic noise: returns float in [0, 1) for any (q, r).
static float pseudoNoise(int q, int r, uint32_t seed) noexcept {
    auto h = static_cast<uint32_t>(q * 374761393 + r * 668265263 + seed * 2246822519u);
    h = (h ^ (h >> 13)) * 1274126177u;
    return static_cast<float>(h & 0xFFFFu) / 65536.0f;
}

// ── Terrain / ObjType string tables ──────────────────────────────────────────

static const char* terrainToStr(Terrain t) noexcept {
    switch (t) {
        case Terrain::Sand:     return "sand";
        case Terrain::Dune:     return "dune";
        case Terrain::Rock:     return "rock";
        case Terrain::Oasis:    return "oasis";
        case Terrain::Ruins:    return "ruins";
        case Terrain::Obsidian: return "obsidian";
        case Terrain::Mountain: return "mountain";
        case Terrain::River:    return "river";
        case Terrain::Wall:     return "wall";
        case Terrain::Battle:   return "battle";
        default:                return "sand";
    }
}

static Terrain terrainFromStr(const std::string& s) noexcept {
    if (s == "dune")     return Terrain::Dune;
    if (s == "rock")    return Terrain::Rock;
    if (s == "oasis")   return Terrain::Oasis;
    if (s == "ruins")   return Terrain::Ruins;
    if (s == "obsidian") return Terrain::Obsidian;
    if (s == "mountain") return Terrain::Mountain;
    if (s == "river")   return Terrain::River;
    if (s == "wall")    return Terrain::Wall;
    if (s == "battle")  return Terrain::Battle;
    return Terrain::Sand;
}

static const char* objTypeToStr(ObjType t) noexcept {
    switch (t) {
        case ObjType::Town:        return "town";
        case ObjType::Dungeon:     return "dungeon";
        case ObjType::GoldMine:    return "gold_mine";
        case ObjType::CrystalMine: return "crystal_mine";
        case ObjType::Artifact:    return "artifact";
        default:                   return "town";
    }
}

static ObjType objTypeFromStr(const std::string& s) noexcept {
    if (s == "dungeon")      return ObjType::Dungeon;
    if (s == "gold_mine")    return ObjType::GoldMine;
    if (s == "crystal_mine") return ObjType::CrystalMine;
    if (s == "artifact")     return ObjType::Artifact;
    return ObjType::Town;
}

// ── WorldMap ──────────────────────────────────────────────────────────────────

WorldMap WorldMap::clone() const {
    WorldMap copy;
    copy.m_name   = m_name;
    copy.m_radius = m_radius;
    copy.m_objects = m_objects;
    for (const auto& [coord, tile] : m_grid)
        copy.m_grid.set(coord, tile);
    return copy;
}

void WorldMap::clear(int radius) {
    m_grid   = HexGrid<MapTile>{};
    m_objects.clear();
    m_radius = radius;

    // Fill with bare Sand tiles so the editor has a canvas.
    for (int q = -radius; q <= radius; ++q) {
        int r1 = std::max(-radius, -q - radius);
        int r2 = std::min( radius, -q + radius);
        for (int r = r1; r <= r2; ++r)
            m_grid.set({q, r}, makeTile(Terrain::Sand));
    }
}

void WorldMap::generateProcedural(int radius, uint32_t seed) {
    m_radius = radius;
    m_grid   = HexGrid<MapTile>{};
    m_objects.clear();

    // ── Terrain generation ───────────────────────────────────────────────────
    for (int q = -radius; q <= radius; ++q) {
        int r1 = std::max(-radius, -q - radius);
        int r2 = std::min( radius, -q + radius);
        for (int r = r1; r <= r2; ++r) {
            HexCoord coord{q, r};
            float n    = pseudoNoise(q, r, seed);
            float dist = static_cast<float>(coord.length()) / static_cast<float>(radius);

            Terrain t;
            if      (n < 0.04f)                  t = Terrain::Oasis;
            else if (n < 0.14f)                  t = Terrain::Rock;
            else if (n < 0.20f)                  t = Terrain::Ruins;
            else if (n < 0.24f)                  t = Terrain::Obsidian;
            else if (n < 0.44f && dist > 0.4f)  t = Terrain::Dune;
            else                                 t = Terrain::Sand;

            m_grid.set(coord, makeTile(t));
        }
    }

    // ── Object placement ─────────────────────────────────────────────────────
    // Seeded positions so the same seed always gives the same layout.
    auto placed = [&](int q, int r, ObjType type, const char* name) {
        HexCoord pos{q, r};
        if (m_grid.has(pos))
            m_objects.push_back({pos, type, name});
    };

    placed(  0,  4, ObjType::Town,        "Umm'Natur"          );
    placed(  5, -3, ObjType::Dungeon,     "Tomb of Kha'Set"    );
    placed( -4,  6, ObjType::Dungeon,     "Buried Sanctum"     );
    placed(  3,  3, ObjType::GoldMine,    "Gold Vein"          );
    placed( -6, -2, ObjType::CrystalMine, "Crystal Seam"       );
    placed(  7, -6, ObjType::Artifact,    "Scarab of Eternity" );
    placed( -8,  4, ObjType::Artifact,    "Pharaoh's Eye"      );
    placed(  1, -7, ObjType::GoldMine,    "Sunken Treasury"    );

    ensureObjectTilesPassable();

    // Ensure hero spawn tile (origin) is always passable sand.
    m_grid.set({0, 0}, makeTile(Terrain::Sand));
}

void WorldMap::ensureObjectTilesPassable() {
    for (const auto& obj : m_objects) {
        MapTile* t = m_grid.get(obj.pos);
        if (!t) continue;
        if (!t->passable) {
            // Demote Obsidian to Ruins so the tile is accessible.
            t->terrain  = (t->terrain == Terrain::Obsidian) ? Terrain::Ruins : t->terrain;
            t->passable = true;
            t->moveCost = terrainDefaultMoveCost(t->terrain);
        }
    }
}

// ── Serialisation ─────────────────────────────────────────────────────────────

std::optional<std::string> WorldMap::saveJson(const std::string& path) const {
    try {
        json j;
        j["version"] = 1;
        j["name"]    = m_name;
        j["radius"]  = m_radius;

        json tiles = json::array();
        for (const auto& [coord, tile] : m_grid) {
            tiles.push_back({
                {"q",        coord.q},
                {"r",        coord.r},
                {"terrain",  terrainToStr(tile.terrain)},
                {"passable", tile.passable},
                {"moveCost", tile.moveCost}
            });
        }
        j["tiles"] = std::move(tiles);

        json objs = json::array();
        for (const auto& obj : m_objects) {
            objs.push_back({
                {"q",    obj.pos.q},
                {"r",    obj.pos.r},
                {"type", objTypeToStr(obj.type)},
                {"name", obj.name}
            });
        }
        j["objects"] = std::move(objs);

        std::ofstream f(path);
        if (!f.is_open())
            return "Cannot open file for writing: " + path;

        f << j.dump(2);
        return std::nullopt;   // success

    } catch (const std::exception& e) {
        return std::string("saveJson exception: ") + e.what();
    }
}

std::optional<std::string> WorldMap::loadJson(const std::string& path) {
    try {
        std::ifstream f(path);
        if (!f.is_open())
            return "Cannot open file for reading: " + path;

        json j = json::parse(f);

        int version = j.value("version", 0);
        if (version != 1)
            return "Unsupported map version: " + std::to_string(version);

        m_grid = HexGrid<MapTile>{};
        m_objects.clear();

        m_name   = j.value("name",   "Untitled Map");
        m_radius = j.value("radius", 0);

        for (const auto& t : j.at("tiles")) {
            HexCoord coord{ t.at("q").get<int>(), t.at("r").get<int>() };
            MapTile  tile;
            tile.terrain  = terrainFromStr(t.value("terrain",  "sand"));
            tile.passable = t.value("passable", true);
            tile.moveCost = t.value("moveCost", 1.0f);
            m_grid.set(coord, tile);
        }

        for (const auto& o : j.at("objects")) {
            MapObjectDef obj;
            obj.pos  = { o.at("q").get<int>(), o.at("r").get<int>() };
            obj.type = objTypeFromStr(o.value("type", "town"));
            obj.name = o.value("name", "");
            m_objects.push_back(std::move(obj));
        }

        return std::nullopt;   // success

    } catch (const std::exception& e) {
        return std::string("loadJson exception: ") + e.what();
    }
}

// ── Tile access ───────────────────────────────────────────────────────────────

const MapTile* WorldMap::tileAt(const HexCoord& c) const { return m_grid.get(c); }
MapTile*       WorldMap::tileAt(const HexCoord& c)       { return m_grid.get(c); }
bool           WorldMap::hasTile(const HexCoord& c) const { return m_grid.has(c); }
void           WorldMap::setTile(const HexCoord& c, MapTile tile) { m_grid.set(c, std::move(tile)); }
void           WorldMap::removeTile(const HexCoord& c) { m_grid.remove(c); }
size_t         WorldMap::tileCount() const { return m_grid.cellCount(); }

// ── Object access ─────────────────────────────────────────────────────────────

const MapObjectDef* WorldMap::objectAt(const HexCoord& c) const {
    for (const auto& obj : m_objects)
        if (obj.pos == c) return &obj;
    return nullptr;
}

void WorldMap::placeObject(MapObjectDef obj) {
    // Enforce one object per tile: replace any existing object at that position.
    removeObjectAt(obj.pos);
    m_objects.push_back(std::move(obj));
}

bool WorldMap::removeObjectAt(const HexCoord& c) {
    auto it = std::find_if(m_objects.begin(), m_objects.end(),
                           [&c](const MapObjectDef& o){ return o.pos == c; });
    if (it == m_objects.end()) return false;
    m_objects.erase(it);
    return true;
}

// ── Pathfinding ───────────────────────────────────────────────────────────────

std::vector<HexCoord> WorldMap::findPath(const HexCoord& from,
                                         const HexCoord& to) const {
    return m_grid.findPath(from, to,
        [](const HexCoord&, const MapTile& t) { return t.passable; });
}

std::vector<HexCoord> WorldMap::findPathWeighted(const HexCoord& from,
                                                  const HexCoord& to) const {
    return m_grid.findPath(
        from, to,
        [](const HexCoord&, const MapTile& t) { return t.passable; },
        [this](const HexCoord& /*from*/, const HexCoord& to) {
            const MapTile* t = m_grid.get(to);
            return t ? t->moveCost : 1.0f;
        });
}
