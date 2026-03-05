#pragma once
#include "hex/HexCoord.h"
#include <string>
#include <unordered_map>
#include <set>

/*
 * TownState — per-session mutable state for a Town map object.
 *
 * Lives in AdventureState::m_townStates (keyed by HexCoord).
 * Updated weekly by TurnManager; read/written by CastleState on each visit.
 *
 * recruitPool : unitId → count currently available to recruit.
 *               Seeded with one week's growth on first enter.
 *               Weekly growth fires on days 7, 14, 21 …
 * buildings   : set of built buildingIds — empty for now; unlocks higher growth later.
 */
struct TownState {
    std::unordered_map<std::string, int> recruitPool;  // unitId → count
    std::set<std::string>                buildings;    // buildingIds (future)
};

using TownStateMap = std::unordered_map<HexCoord, TownState>;
