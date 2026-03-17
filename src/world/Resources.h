#pragma once
#include <array>
#include <string_view>
#include <algorithm>

/*
 * Umm'Natur resource types — natural/geological, faction-agnostic.
 *
 *  Gold     — universal currency
 *  Wood     — construction, siege engines          (+2/day Sawmill)
 *  Stone    — fortification, heavy units            (+2/day Quarry)
 *  Obsidian — refined weapons, rare volcanic glass  (+1/day Obsidian Vent)
 *  Crystal  — magical, arcane components            (+1/day Crystal Cavern)
 */
enum class Resource : int {
    Gold     = 0,
    Wood     = 1,
    Stone    = 2,
    Obsidian = 3,
    Crystal  = 4,

    COUNT
};

constexpr int RESOURCE_COUNT = static_cast<int>(Resource::COUNT);

constexpr std::array<std::string_view, RESOURCE_COUNT> RESOURCE_NAMES = {{
    "Gold",
    "Wood",
    "Stone",
    "Obsidian",
    "Crystal"
}};

inline std::string_view resourceName(Resource r) {
    int idx = static_cast<int>(r);
    if (idx < 0 || idx >= RESOURCE_COUNT) return "Unknown";
    return RESOURCE_NAMES[idx];
}

/*
 * ResourcePool — holds one amount per resource type.
 */
struct ResourcePool {
    std::array<int, RESOURCE_COUNT> amounts{};

    ResourcePool() { amounts.fill(0); }

    int& operator[](Resource r)       { return amounts[static_cast<int>(r)]; }
    int  operator[](Resource r) const { return amounts[static_cast<int>(r)]; }

    ResourcePool operator+(const ResourcePool& o) const {
        ResourcePool result;
        for (int i = 0; i < RESOURCE_COUNT; ++i)
            result.amounts[i] = amounts[i] + o.amounts[i];
        return result;
    }
    ResourcePool operator-(const ResourcePool& o) const {
        ResourcePool result;
        for (int i = 0; i < RESOURCE_COUNT; ++i)
            result.amounts[i] = amounts[i] - o.amounts[i];
        return result;
    }
    ResourcePool& operator+=(const ResourcePool& o) {
        for (int i = 0; i < RESOURCE_COUNT; ++i) amounts[i] += o.amounts[i];
        return *this;
    }
    ResourcePool& operator-=(const ResourcePool& o) {
        for (int i = 0; i < RESOURCE_COUNT; ++i) amounts[i] -= o.amounts[i];
        return *this;
    }

    // True if this pool can afford the cost
    bool canAfford(const ResourcePool& cost) const {
        for (int i = 0; i < RESOURCE_COUNT; ++i)
            if (amounts[i] < cost.amounts[i]) return false;
        return true;
    }

    // Clamp all values to [0, INT_MAX]
    void clampPositive() {
        for (auto& v : amounts) v = std::max(0, v);
    }
};
