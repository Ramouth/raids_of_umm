#pragma once
#include <array>
#include <optional>
#include <string>
#include <unordered_map>

#include "../hex/HexCoord.h"
#include "../world/MapTile.h"    // Terrain, TERRAIN_COUNT
#include "../world/MapObject.h"  // ObjType, OBJ_TYPE_COUNT

/*
 * RenderOffset — per-element world-space nudge applied at draw time.
 *
 * dx / dz : horizontal shift in the XZ plane (screen left/right and depth)
 * dy      : vertical shift stacked on top of the terrain height value
 *
 * These are intentionally small values (typically ±0.05 – ±0.2 world units)
 * used to fix pixel-alignment of terrain tiles and object sprites.
 */
struct RenderOffset {
    float dx = 0.0f;
    float dz = 0.0f;
    float dy = 0.0f;

    bool isZero() const { return dx == 0.0f && dz == 0.0f && dy == 0.0f; }
};

/*
 * RenderOffsetConfig — stores the full table of offsets for every terrain
 * type, object type, and per-tile override.
 *
 * Lookup priority:
 *   1. Per-tile override (tileOverride / objectOverride)  — most specific
 *   2. Global default for that Terrain / ObjType          — fallback
 *   3. Zero offset                                        — implicit default
 *
 * Persisted to / loaded from JSON (assets/render_offsets.json).
 */
class RenderOffsetConfig {
public:
    std::array<RenderOffset, TERRAIN_COUNT>    terrainGlobal{};
    std::array<RenderOffset, OBJ_TYPE_COUNT>   objectGlobal{};
    std::unordered_map<HexCoord, RenderOffset> tileOverride;
    std::unordered_map<HexCoord, RenderOffset> objectOverride;

    // --- Lookup ---------------------------------------------------------------

    RenderOffset forTerrain(const HexCoord& c, Terrain t) const;
    RenderOffset forObject (const HexCoord& c, ObjType  t) const;

    // Mutable references — used by the dev tool to nudge in place
    RenderOffset& terrainRef(const HexCoord& c, Terrain t, bool perTile);
    RenderOffset& objectRef (const HexCoord& c, ObjType  t, bool perTile);

    // --- Persistence ----------------------------------------------------------

    // Returns nullopt on success, error string on failure.
    std::optional<std::string> save(const std::string& path) const;
    std::optional<std::string> load(const std::string& path);

    // Zero every offset and clear all per-tile maps
    void reset();
};
