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
    Town         = 0,
    Dungeon      = 1,
    GoldMine     = 2,
    CrystalMine  = 3,
    Artifact     = 4,
    Sawmill      = 5,
    Quarry       = 6,
    ObsidianVent = 7,
    OldMine      = 8,   // abandoned mine — may hide the old passage
    QuestGiver   = 9,   // SC camp that offers quests
    Guard        = 10,  // neutral guard stack (placeholder until guard data lands)
    // ── HoMM3-style adventure objects (kind refines each, see MapObjectDef) ──
    Pickup        = 11, // one-time: resource pile (kind = resource), "chest", "campfire"
    Mill          = 12, // weekly: kind "windmill" (wood + stone) / "watermill" (gold)
    Watchtower    = 13, // one-time: lifts the fog in a wide radius
    Stables       = 14, // weekly: extra movement until the week ends
    LearningStone = 15, // one-time: experience for the travelling companions
    Dwelling      = 16, // capturable: weekly recruits of one unit (kind = unit id)
    Obelisk       = 17, // one-time: an inscription (story) + one piece of the passage map

    COUNT  // must remain last
};

constexpr int OBJ_TYPE_COUNT = static_cast<int>(ObjType::COUNT);

constexpr std::string_view objTypeName(ObjType t) noexcept {
    switch (t) {
        case ObjType::Town:         return "Town";
        case ObjType::Dungeon:      return "Dungeon";
        case ObjType::GoldMine:     return "Gold Mine";
        case ObjType::CrystalMine:  return "Crystal Mine";
        case ObjType::Artifact:     return "Artifact";
        case ObjType::Sawmill:      return "Sawmill";
        case ObjType::Quarry:       return "Quarry";
        case ObjType::ObsidianVent: return "Obsidian Vent";
        case ObjType::OldMine:      return "Old Mine";
        case ObjType::QuestGiver:   return "Quest Giver";
        case ObjType::Guard:        return "Guard";
        case ObjType::Pickup:       return "Pickup";
        case ObjType::Mill:         return "Mill";
        case ObjType::Watchtower:   return "Watchtower";
        case ObjType::Stables:      return "Stables";
        case ObjType::LearningStone:return "Learning Stone";
        case ObjType::Dwelling:     return "Dwelling";
        case ObjType::Obelisk:      return "Obelisk";
        default:                    return "Unknown";
    }
}

struct MapObjectDef {
    HexCoord    pos;
    ObjType     type      = ObjType::Town;
    std::string name;
    int         factionId = 0;  // starting owner (0=neutral, 1=player, 2+=AI)
    std::string kind;           // subtype: pile resource, mill kind, dwelling unit id

    // Convenience
    std::string_view typeName() const noexcept { return objTypeName(type); }
};
