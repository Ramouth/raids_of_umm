#pragma once
#include "world/MapTile.h"
#include "world/MapObject.h"
#include <glm/glm.hpp>

/*
 * TileVisuals — maps world-data enums to rendering properties.
 *
 * Rendering concerns (colours, heights, visual scales) are intentionally
 * separated from world data so that:
 *   - Changing a tile colour never touches MapTile.h
 *   - A renderer can provide its own mapping (e.g. mini-map vs main map)
 *   - World data has zero dependency on GLM or any render library
 */

// ── Terrain ───────────────────────────────────────────────────────────────────

inline glm::vec3 terrainColor(Terrain t) noexcept {
    switch (t) {
        case Terrain::Sand:     return { 0.96f, 0.87f, 0.58f };  // bright sand
        case Terrain::Dune:     return { 0.85f, 0.72f, 0.45f };  // golden dune
        case Terrain::Rock:     return { 0.65f, 0.58f, 0.50f };  // warm rock
        case Terrain::Oasis:    return { 0.35f, 0.75f, 0.55f };  // vibrant green-blue
        case Terrain::Ruins:    return { 0.78f, 0.70f, 0.58f };  // sun-bleached stone
        case Terrain::Obsidian: return { 0.22f, 0.20f, 0.25f };  // dark volcanic
        case Terrain::Mountain: return { 0.50f, 0.45f, 0.42f };  // dusty brown-gray
        case Terrain::River:    return { 0.30f, 0.55f, 0.85f };  // bright blue water
        case Terrain::Wall:     return { 0.60f, 0.58f, 0.55f };  // pale stone
        case Terrain::Battle:   return { 0.75f, 0.20f, 0.20f };  // blood-red
        default:                return { 1.00f, 0.00f, 1.00f }; // magenta = unknown
    }
}

// Vertical height offset for a terrain tile (visual layering on the map).
inline float terrainHeight(Terrain t) noexcept {
    switch (t) {
        case Terrain::Dune:     return 0.05f;
        case Terrain::Rock:     return 0.12f;
        case Terrain::Ruins:    return 0.04f;
        case Terrain::Mountain: return 0.25f;  // tall mountains
        case Terrain::Wall:     return 0.15f;  // walls are tall
        default:                return 0.00f;
    }
}

// ── Map objects ───────────────────────────────────────────────────────────────

inline glm::vec3 objTypeColor(ObjType t) noexcept {
    switch (t) {
        case ObjType::Town:        return { 0.20f, 0.80f, 0.90f };
        case ObjType::Dungeon:     return { 0.75f, 0.12f, 0.12f };
        case ObjType::GoldMine:    return { 0.95f, 0.70f, 0.10f };
        case ObjType::CrystalMine: return { 0.50f, 0.80f, 0.95f };
        case ObjType::Artifact:    return { 1.00f, 1.00f, 1.00f };
        default:                   return { 1.00f, 0.00f, 1.00f };
    }
}
