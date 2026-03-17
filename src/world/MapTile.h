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
    Mountain = 6,  // tall, high movement cost
    River    = 7,  // blocks unless bridged
    Wall     = 8,  // completely impassable
    Battle   = 9,  // combat encounter

    // ── Northern biome ────────────────────────────────────────────────────
    Grass    = 10, // lush Highland meadow — base terrain for Verdant Reach
    Forest   = 11, // dense Celtic woodland — slow movement
    Highland = 12, // rocky moorland — elevated, medium cost

    COUNT  // must remain last — used for iteration and bounds checks
};

constexpr int TERRAIN_COUNT        = static_cast<int>(Terrain::COUNT);
constexpr int MAX_TERRAIN_VARIANTS = 8;  // max texture variants per terrain type

struct MapTile {
    Terrain terrain  = Terrain::Sand;
    bool    passable = true;
    float   moveCost = 1.0f;   // 1.0 = normal cost; Obsidian sets passable=false
    uint8_t variant  = 0;      // texture variant index (0 = default)
    bool    road     = false;  // road overlay — halves effective moveCost
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
        case Terrain::Mountain:  return "Mountain";
        case Terrain::River:     return "River";
        case Terrain::Wall:     return "Wall";
        case Terrain::Battle:   return "Battle";
        case Terrain::Grass:    return "Grass";
        case Terrain::Forest:   return "Forest";
        case Terrain::Highland: return "Highland";
        default:                return "Unknown";
    }
}

// Default passability for a freshly generated tile of this terrain type.
// Editor can override per-tile.
constexpr bool terrainDefaultPassable(Terrain t) noexcept {
    return t != Terrain::Obsidian && t != Terrain::Wall;
}

// Default movement cost for a terrain type.
constexpr float terrainDefaultMoveCost(Terrain t) noexcept {
    switch (t) {
        case Terrain::Dune:     return 1.5f;  // costs 1.5 moves to cross
        case Terrain::Rock:      return 1.25f;
        case Terrain::Ruins:    return 1.25f;
        case Terrain::Mountain:  return 2.0f;  // very hard to cross
        case Terrain::River:    return 0.0f;  // impassable without bridge
        case Terrain::Wall:     return 0.0f;  // impassable
        case Terrain::Battle:   return 1.0f;  // triggers combat
        case Terrain::Obsidian: return 0.0f;  // impassable
        case Terrain::Grass:    return 1.0f;  // open meadow — easy movement
        case Terrain::Forest:   return 1.5f;  // dense woodland — slows movement
        case Terrain::Highland: return 1.25f; // rocky moorland — slightly slower
        default:                return 1.0f;
    }
}

// Build a default tile for a terrain type (passable + cost set correctly).
constexpr MapTile makeTile(Terrain t) noexcept {
    return MapTile{ t, terrainDefaultPassable(t), terrainDefaultMoveCost(t) };
}
