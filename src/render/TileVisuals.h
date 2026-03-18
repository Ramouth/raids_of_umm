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
        // ── Umm'Natur — warm amber/ochre desert palette ───────────────────────
        case Terrain::Sand:     return { 0.78f, 0.63f, 0.35f };  // warm ochre sand
        case Terrain::Dune:     return { 0.68f, 0.50f, 0.25f };  // deep amber dune
        case Terrain::Rock:     return { 0.52f, 0.47f, 0.44f };  // cold weathered stone
        case Terrain::Oasis:    return { 0.22f, 0.58f, 0.42f };  // deep desert oasis
        case Terrain::Ruins:    return { 0.80f, 0.72f, 0.58f };  // bleached sandstone
        case Terrain::Obsidian: return { 0.18f, 0.16f, 0.20f };  // near-black volcanic
        // ── Structural ────────────────────────────────────────────────────────
        case Terrain::Mountain: return { 0.40f, 0.38f, 0.40f };  // cold stone peaks
        case Terrain::River:    return { 0.25f, 0.50f, 0.82f };  // deep blue water
        case Terrain::Wall:     return { 0.42f, 0.40f, 0.38f };  // dark fortress stone
        case Terrain::Battle:   return { 0.72f, 0.18f, 0.18f };  // blood-red
        // ── Verdant Reach — deep blue-green Celtic palette ────────────────────
        case Terrain::Grass:         return { 0.18f, 0.50f, 0.14f };  // deep Celtic meadow
        case Terrain::Forest:        return { 0.09f, 0.28f, 0.12f };  // dark ancient forest
        case Terrain::Highland:      return { 0.35f, 0.46f, 0.28f };  // grey-green moorland
        case Terrain::GrassSandEdge:  return { 0.48f, 0.57f, 0.28f };  // blended grass↔sand border
        case Terrain::GrassSandEdge2: return { 0.30f, 0.52f, 0.18f };  // grass overlay (sand transparent)
        default:                      return { 1.00f, 0.00f, 1.00f };  // magenta = unknown
    }
}

// Packed-earth / stone-block road path overlay color.
inline glm::vec3 roadColor() noexcept {
    return { 0.68f, 0.55f, 0.38f };  // worn sandstone road
}

// Vertical height offset for a terrain tile (visual layering on the map).
inline float terrainHeight(Terrain t) noexcept {
    switch (t) {
        case Terrain::Dune:     return 0.05f;
        case Terrain::Rock:     return 0.12f;
        case Terrain::Ruins:    return 0.04f;
        case Terrain::Mountain: return 0.25f;  // tall mountains
        case Terrain::Wall:     return 0.15f;  // walls are tall
        case Terrain::Highland: return 0.08f;  // gently elevated moorland
        default:                return 0.00f;
    }
}

// ── Map objects ───────────────────────────────────────────────────────────────

inline glm::vec3 objTypeColor(ObjType t) noexcept {
    switch (t) {
        case ObjType::Town:         return { 0.20f, 0.80f, 0.90f };
        case ObjType::Dungeon:      return { 0.75f, 0.12f, 0.12f };
        case ObjType::GoldMine:     return { 0.95f, 0.70f, 0.10f };
        case ObjType::CrystalMine:  return { 0.50f, 0.80f, 0.95f };
        case ObjType::Artifact:     return { 1.00f, 1.00f, 1.00f };
        case ObjType::Sawmill:      return { 0.55f, 0.35f, 0.10f };
        case ObjType::Quarry:       return { 0.65f, 0.60f, 0.55f };
        case ObjType::ObsidianVent: return { 0.20f, 0.10f, 0.30f };
        default:                    return { 1.00f, 0.00f, 1.00f };
    }
}
