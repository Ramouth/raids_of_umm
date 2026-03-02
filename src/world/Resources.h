#pragma once
#include <array>
#include <string_view>
#include <algorithm>

/*
 * Umm'Natur resource types — desert/Egyptian-themed replacements for HoMM3.
 *
 *  Gold         — universal currency
 *  SandCrystal  — replaces wood (construction, training)
 *  BoneDust     — replaces ore  (fortifications, heavy units)
 *  DjinnEssence — replaces gems (spellcasting, arcane items)
 *  AncientRelic — replaces sulfur (siege engines, powerful spells)
 *  MercuryTears — replaces mercury (rare potions, hero advancement)
 *  Amber        — replaces crystal (desert resin, enchantment)
 */
enum class Resource : int {
    Gold         = 0,
    SandCrystal  = 1,
    BoneDust     = 2,
    DjinnEssence = 3,
    AncientRelic = 4,
    MercuryTears = 5,
    Amber        = 6,

    COUNT
};

constexpr int RESOURCE_COUNT = static_cast<int>(Resource::COUNT);

constexpr std::array<std::string_view, RESOURCE_COUNT> RESOURCE_NAMES = {{
    "Gold",
    "Sand Crystal",
    "Bone Dust",
    "Djinn Essence",
    "Ancient Relic",
    "Mercury Tears",
    "Amber"
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
