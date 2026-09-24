#pragma once
#include "world/Resources.h"
#include <string>
#include <vector>

/*
 * BuildingDef — one town building from data/buildings.json ("buildings").
 *
 * A town builds at most one per day.  Effects are data:
 *   income       gold per day while built (halls: the highest hall counts)
 *   growth       weekly recruit growth bonus (forts: the highest counts; 0.5 = +50%)
 *   unlocks      unit id this dwelling lets the town recruit
 *   market       enables resource trading at the marketplace
 *   attackBonus  +attack for the hero's troops while the player holds the town
 * A unit that no building unlocks needs no dwelling (e.g. factions without a
 * building tree yet).
 */
struct BuildingDef {
    std::string              id, name, description, faction;
    ResourcePool             cost;
    std::vector<std::string> requires;
    int                      income      = 0;
    double                   growth      = 0.0;
    std::string              unlocks;
    bool                     market      = false;
    int                      attackBonus = 0;
    bool                     starting    = false;   // every town of the faction begins with it
};
