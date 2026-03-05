#pragma once
#include "hex/HexCoord.h"
#include "world/MapObject.h"
#include <unordered_map>

/*
 * ObjectControl — per-session ownership/state for a map object.
 *
 * Lives in AdventureState (keyed by HexCoord), NOT in WorldMap.
 * TurnManager will iterate this map to compute daily income.
 *
 *  ownerFaction  : 0 = neutral, 1 = player, 2+ = AI factions
 *  guardDefeated : true once a Dungeon's guardian has been beaten;
 *                  for mines/towns this field is unused (ownerFaction drives logic)
 *  objType       : cached at insert time so TurnManager doesn't need WorldMap
 */
struct ObjectControl {
    int     ownerFaction  = 0;              // 0=neutral, 1=player, 2+=AI
    bool    guardDefeated = false;          // Dungeon enemy beaten; mine/town: ignore
    ObjType objType       = ObjType::Town;  // cached at insert time
};

using ObjectControlMap = std::unordered_map<HexCoord, ObjectControl>;
