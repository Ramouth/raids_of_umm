#pragma once
#include <cstdint>
#include <string_view>

/*
 * MapTile — immutable-ish data for a single hex cell in the world.
 *
 * terrain  : visual / gameplay category of the cell
 * passable : whether units can enter (set by terrain type by default,
 *            may be overridden by the editor or map events)
 * moveCost : fractional movement cost (1.0 = normal; >1 = slower terrain
 *            e.g. dunes cost 1.5 moves). Used by pathfinding.
 *
 * NOTE: visited flags, ownership, and any game-session state do NOT belong
 * here — those live in the game-state layer (AdventureState, CombatState).
 */

enum class Terrain : uint8_t {
    Sand     = 0,
    Dune     = 1,
    Rock     = 2,
    Oasis    = 3,
    Ruins    = 4,
    Obsidian = 5,

    COUNT  // must remain last — used for iteration and bounds checks
};

constexpr int TERRAIN_COUNT = static_cast<int>(Terrain::COUNT);

struct MapTile {
    Terrain terrain  = Terrain::Sand;
    bool    passable = true;
    float   moveCost = 1.0f;  // 1.0 = normal cost; Obsidian sets passable=false
};

// ── Helpers (data queries — not presentation) ─────────────────────────────────

constexpr std::string_view terrainName(Terrain t) noexcept {
    switch (t) {
        case Terrain::Sand:     return "Sand";
        case Terrain::Dune:     return "Dune";
        case Terrain::Rock:     return "Rock";
        case Terrain::Oasis:    return "Oasis";
        case Terrain::Ruins:    return "Ruins";
        case Terrain::Obsidian: return "Obsidian";
        default:                return "Unknown";
    }
}

// Default passability for a freshly generated tile of this terrain type.
// Editor can override per-tile.
constexpr bool terrainDefaultPassable(Terrain t) noexcept {
    return t != Terrain::Obsidian;
}

// Default movement cost for a terrain type.
constexpr float terrainDefaultMoveCost(Terrain t) noexcept {
    switch (t) {
        case Terrain::Dune:     return 1.5f;  // costs 1.5 moves to cross
        case Terrain::Rock:     return 1.25f;
        case Terrain::Ruins:    return 1.25f;
        case Terrain::Obsidian: return 0.0f;  // impassable — cost unused
        default:                return 1.0f;
    }
}

// Build a default tile for a terrain type (passable + cost set correctly).
constexpr MapTile makeTile(Terrain t) noexcept {
    return MapTile{ t, terrainDefaultPassable(t), terrainDefaultMoveCost(t) };
}
