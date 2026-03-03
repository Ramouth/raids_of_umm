#pragma once
#include "hex/HexCoord.h"
#include <cstdint>
#include <string>
#include <string_view>

/*
 * MapObjectDef — static definition of an object on the adventure map.
 *
 * This is WORLD DATA, not game state. It describes what exists at a map
 * location — not whether a hero has visited it, who owns it, or any
 * other per-session information.
 *
 * Session-level state (visited, controlled, looted) lives in AdventureState
 * or a dedicated GameWorld object, keyed by object ID.
 */

enum class ObjType : uint8_t {
    Town        = 0,
    Dungeon     = 1,
    GoldMine    = 2,
    CrystalMine = 3,
    Artifact    = 4,

    COUNT  // must remain last
};

constexpr int OBJ_TYPE_COUNT = static_cast<int>(ObjType::COUNT);

constexpr std::string_view objTypeName(ObjType t) noexcept {
    switch (t) {
        case ObjType::Town:        return "Town";
        case ObjType::Dungeon:     return "Dungeon";
        case ObjType::GoldMine:    return "Gold Mine";
        case ObjType::CrystalMine: return "Crystal Mine";
        case ObjType::Artifact:    return "Artifact";
        default:                   return "Unknown";
    }
}

struct MapObjectDef {
    HexCoord    pos;
    ObjType     type = ObjType::Town;
    std::string name;

    // Convenience
    std::string_view typeName() const noexcept { return objTypeName(type); }
};
