#pragma once
#include <string>
#include <vector>

/*
 * EncounterStack — one unit type + count within an encounter definition.
 */
struct EncounterStack {
    std::string unitId;  // references a UnitType id in ResourceManager
    int         count = 0;
};

/*
 * EncounterDef — immutable recipe for an enemy army, loaded from encounters.json.
 *
 * MapObjectDef will eventually carry an encounterId; until then, AdventureState
 * maps ObjType → encounter id with a simple lookup.
 *
 * Loaded once at startup by ResourceManager. Never duplicated.
 */
struct EncounterDef {
    std::string id;    // e.g. "dungeon_default"
    std::string name;  // displayed as the enemy army's ownerName in combat
    std::vector<EncounterStack> stacks;
};
