#pragma once
#include <cmath>
#include <functional>
#include <array>

/*
 * HexCoord — strongly-typed axial coordinate (q, r).
 *
 * Flat-top hex orientation. Cube coordinates satisfy q + r + s = 0.
 * s is computed on-the-fly as -(q+r).
 *
 * Reference: https://www.redblobgames.com/grids/hexagons/
 */
struct HexCoord {
    int q = 0;
    int r = 0;

    constexpr HexCoord() = default;
    constexpr HexCoord(int q, int r) : q(q), r(r) {}

    constexpr int s() const { return -q - r; }

    // Arithmetic
    constexpr HexCoord operator+(const HexCoord& o) const { return {q + o.q, r + o.r}; }
    constexpr HexCoord operator-(const HexCoord& o) const { return {q - o.q, r - o.r}; }
    constexpr HexCoord operator*(int k)             const { return {q * k,   r * k  }; }
    constexpr bool operator==(const HexCoord& o)    const { return q == o.q && r == o.r; }
    constexpr bool operator!=(const HexCoord& o)    const { return !(*this == o); }

    // Hex distance
    constexpr int length() const {
        return (std::abs(q) + std::abs(r) + std::abs(s())) / 2;
    }
    constexpr int distanceTo(const HexCoord& o) const {
        return (*this - o).length();
    }

    // Six axial directions (flat-top): E NE NW W SW SE
    static constexpr std::array<HexCoord, 6> directions() {
        return {{ {1,0},{1,-1},{0,-1},{-1,0},{-1,1},{0,1} }};
    }

    HexCoord neighbor(int dir) const {
        auto dirs = directions();
        return *this + dirs[dir & 5];
    }

    // Convert axial → world XZ position (flat-top, size = hex_radius)
    // Returns {x, z} — Y is handled by terrain height
    void toWorld(float hexSize, float& outX, float& outZ) const {
        outX = hexSize * (1.5f * static_cast<float>(q));
        outZ = hexSize * (sqrtf(3.0f) * (static_cast<float>(r) + static_cast<float>(q) * 0.5f));
    }

    // Convert world XZ → nearest hex (flat-top)
    static HexCoord fromWorld(float x, float z, float hexSize) {
        float fq = (2.0f / 3.0f * x) / hexSize;
        float fr = (-1.0f / 3.0f * x + sqrtf(3.0f) / 3.0f * z) / hexSize;
        return axialRound(fq, fr);
    }

private:
    static HexCoord axialRound(float fq, float fr) {
        float fs = -fq - fr;
        int rq = static_cast<int>(std::round(fq));
        int rr = static_cast<int>(std::round(fr));
        int rs = static_cast<int>(std::round(fs));
        float dq = std::abs(static_cast<float>(rq) - fq);
        float dr = std::abs(static_cast<float>(rr) - fr);
        float ds = std::abs(static_cast<float>(rs) - fs);
        if (dq > dr && dq > ds) rq = -rr - rs;
        else if (dr > ds)        rr = -rq - rs;
        return {rq, rr};
    }
};

// Hash support so HexCoord can be used in unordered containers
namespace std {
    template<>
    struct hash<HexCoord> {
        size_t operator()(const HexCoord& h) const noexcept {
            size_t hq = std::hash<int>{}(h.q);
            size_t hr = std::hash<int>{}(h.r);
            return hq ^ (hr * 2654435761u);
        }
    };
}
