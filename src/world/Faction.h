#pragma once
#include "world/Resources.h"
#include <string>
#include <vector>

struct Faction {
    static constexpr int Neutral = 0;
    static constexpr int Player  = 1;

    int          id   = Neutral;
    std::string  name;
    ResourcePool treasury;

    // Indices into AdventureState's hero list — for future multi-hero support.
    std::vector<int> heroIndices;
};
