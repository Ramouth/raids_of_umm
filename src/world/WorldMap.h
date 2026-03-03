#pragma once
#include "MapTile.h"
#include "MapObject.h"
#include "hex/HexGrid.h"
#include <string>
#include <vector>
#include <cstdint>
#include <optional>

/*
 * WorldMap — the single source of truth for a map's static data.
 *
 * Owns:
 *   - HexGrid<MapTile>        : terrain + passability for every cell
 *   - vector<MapObjectDef>    : towns, dungeons, mines, artifacts
 *
 * Intentionally knows nothing about:
 *   - Rendering (no GL types, no colours)
 *   - Game session (no heroes, no visited flags, no ownership)
 *   - Application lifecycle (no SDL, no states)
 *
 * Both WorldBuilderState (authoring) and AdventureState (playing) operate
 * on WorldMap. The builder mutates it; the game reads it (and maintains
 * session state alongside it).
 *
 * Serialisation format (JSON v1):
 *   { "version":1, "name":"...", "radius":N,
 *     "tiles":[{"q":Q,"r":R,"terrain":"sand","passable":true,"moveCost":1.0},...],
 *     "objects":[{"q":Q,"r":R,"type":"town","name":"..."},...]  }
 */
class WorldMap {
public:
    WorldMap()  = default;
    ~WorldMap() = default;

    // Non-copyable by default (maps can be large); use clone() explicitly.
    WorldMap(const WorldMap&)            = delete;
    WorldMap& operator=(const WorldMap&) = delete;

    WorldMap(WorldMap&&)            = default;
    WorldMap& operator=(WorldMap&&) = default;

    // Explicit deep copy when needed (e.g. handing a copy to AdventureState).
    WorldMap clone() const;

    // ── Generation ───────────────────────────────────────────────────────────

    // Procedurally fill a hex map of given radius.
    // Same radius + seed always produces identical tile data (deterministic).
    void generateProcedural(int radius, uint32_t seed = 0);

    // Remove all tiles and objects (blank slate for the editor).
    void clear(int radius = 0);

    // ── Serialisation ────────────────────────────────────────────────────────

    // Returns nullopt on success, or an error description on failure.
    std::optional<std::string> saveJson(const std::string& path) const;
    std::optional<std::string> loadJson(const std::string& path);

    // ── Tile access ──────────────────────────────────────────────────────────

    const MapTile* tileAt(const HexCoord& c) const;
    MapTile*       tileAt(const HexCoord& c);
    bool           hasTile(const HexCoord& c) const;
    void           setTile(const HexCoord& c, MapTile tile);
    void           removeTile(const HexCoord& c);
    size_t         tileCount() const;

    // Iterate over all tiles: for (const auto& [coord, tile] : map) { ... }
    auto begin() const { return m_grid.begin(); }
    auto end()   const { return m_grid.end(); }
    auto begin()       { return m_grid.begin(); }
    auto end()         { return m_grid.end(); }

    // ── Object access ────────────────────────────────────────────────────────

    const std::vector<MapObjectDef>& objects() const { return m_objects; }
    const MapObjectDef* objectAt(const HexCoord& c) const;

    // Place or replace an object at obj.pos (one object per tile).
    void placeObject(MapObjectDef obj);

    // Returns true if an object was found and removed.
    bool removeObjectAt(const HexCoord& c);

    // ── Pathfinding ──────────────────────────────────────────────────────────

    // Standard passability-based path (uses MapTile::passable).
    std::vector<HexCoord> findPath(const HexCoord& from,
                                   const HexCoord& to) const;

    // Path with weighted movement costs (uses MapTile::moveCost).
    std::vector<HexCoord> findPathWeighted(const HexCoord& from,
                                           const HexCoord& to) const;

    // ── Metadata ─────────────────────────────────────────────────────────────

    const std::string& name()   const { return m_name; }
    int                radius() const { return m_radius; }

    void setName(std::string n) { m_name = std::move(n); }

private:
    HexGrid<MapTile>        m_grid;
    std::vector<MapObjectDef> m_objects;
    std::string             m_name   = "Untitled Map";
    int                     m_radius = 0;

    // Internal helper used by both generateProcedural and JSON loader.
    void ensureObjectTilesPassable();
};
