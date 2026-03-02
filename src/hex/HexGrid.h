#pragma once
#include "HexCoord.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>
#include <optional>
#include <limits>

/*
 * HexGrid — generic hex grid with optional passability map and A* pathfinding.
 *
 * The grid stores arbitrary per-cell data T. Cells that are not stored are
 * treated as non-existent (out-of-bounds or impassable if passability is
 * checked externally).
 *
 * This is intentionally not tied to adventure vs combat — both modes share it.
 */
template<typename T>
class HexGrid {
public:
    using PassFunc = std::function<bool(const HexCoord&, const T&)>;

    HexGrid() = default;

    // ── Cell access ──────────────────────────────────────────────────────────
    void set(const HexCoord& c, T value) {
        m_cells[c] = std::move(value);
    }
    T* get(const HexCoord& c) {
        auto it = m_cells.find(c);
        return it != m_cells.end() ? &it->second : nullptr;
    }
    const T* get(const HexCoord& c) const {
        auto it = m_cells.find(c);
        return it != m_cells.end() ? &it->second : nullptr;
    }
    bool has(const HexCoord& c) const {
        return m_cells.count(c) > 0;
    }
    void remove(const HexCoord& c) {
        m_cells.erase(c);
    }
    size_t cellCount() const { return m_cells.size(); }

    // Iterate over all cells
    auto begin() { return m_cells.begin(); }
    auto end()   { return m_cells.end(); }
    auto begin() const { return m_cells.begin(); }
    auto end()   const { return m_cells.end(); }

    // ── Utility ──────────────────────────────────────────────────────────────

    // All passable neighbors of coord (must exist in grid and pass passFunc)
    std::vector<HexCoord> neighbors(const HexCoord& c,
                                     PassFunc passFunc = nullptr) const {
        std::vector<HexCoord> result;
        for (int d = 0; d < 6; ++d) {
            HexCoord n = c.neighbor(d);
            auto* cell = get(n);
            if (!cell) continue;
            if (passFunc && !passFunc(n, *cell)) continue;
            result.push_back(n);
        }
        return result;
    }

    // All cells within Manhattan hex distance <= radius
    std::vector<HexCoord> ring(const HexCoord& center, int radius) const {
        std::vector<HexCoord> result;
        if (radius == 0) {
            if (has(center)) result.push_back(center);
            return result;
        }
        HexCoord cur = center + HexCoord::directions()[4] * radius;
        for (int side = 0; side < 6; ++side) {
            for (int step = 0; step < radius; ++step) {
                if (has(cur)) result.push_back(cur);
                cur = cur.neighbor(side);
            }
        }
        return result;
    }

    std::vector<HexCoord> range(const HexCoord& center, int radius) const {
        std::vector<HexCoord> result;
        for (int r = 0; r <= radius; ++r) {
            auto ring_cells = ring(center, r);
            result.insert(result.end(), ring_cells.begin(), ring_cells.end());
        }
        return result;
    }

    // ── A* Pathfinding ───────────────────────────────────────────────────────
    // Returns the path from start to goal (inclusive), or empty if unreachable.
    // passFunc: return true if a cell is traversable.
    // costFunc: optional per-edge cost (default = 1).
    std::vector<HexCoord> findPath(
        const HexCoord& start,
        const HexCoord& goal,
        PassFunc passFunc,
        std::function<float(const HexCoord&, const HexCoord&)> costFunc = nullptr
    ) const {
        if (!has(start) || !has(goal)) return {};
        if (start == goal) return {start};

        using Pair = std::pair<float, HexCoord>;
        // open set as sorted list (small enough grids don't need a heap)
        std::vector<Pair> open;
        std::unordered_map<HexCoord, HexCoord> cameFrom;
        std::unordered_map<HexCoord, float>    gScore;

        gScore[start] = 0.0f;
        open.push_back({heuristic(start, goal), start});

        while (!open.empty()) {
            // Pop lowest f
            auto minIt = open.begin();
            for (auto it = open.begin(); it != open.end(); ++it)
                if (it->first < minIt->first) minIt = it;
            auto [f, current] = *minIt;
            open.erase(minIt);
            (void)f;

            if (current == goal) {
                return reconstructPath(cameFrom, current);
            }

            for (int d = 0; d < 6; ++d) {
                HexCoord nb = current.neighbor(d);
                auto* cell = get(nb);
                if (!cell) continue;
                if (nb != goal && passFunc && !passFunc(nb, *cell)) continue;

                float edgeCost = costFunc ? costFunc(current, nb) : 1.0f;
                float tentative = gScore[current] + edgeCost;

                auto it = gScore.find(nb);
                if (it == gScore.end() || tentative < it->second) {
                    gScore[nb] = tentative;
                    cameFrom[nb] = current;
                    float fScore = tentative + heuristic(nb, goal);
                    open.push_back({fScore, nb});
                }
            }
        }
        return {}; // no path found
    }

private:
    std::unordered_map<HexCoord, T> m_cells;

    static float heuristic(const HexCoord& a, const HexCoord& b) {
        return static_cast<float>(a.distanceTo(b));
    }

    static std::vector<HexCoord> reconstructPath(
        const std::unordered_map<HexCoord, HexCoord>& cameFrom,
        HexCoord current)
    {
        std::vector<HexCoord> path;
        while (cameFrom.count(current)) {
            path.push_back(current);
            current = cameFrom.at(current);
        }
        path.push_back(current);
        std::reverse(path.begin(), path.end());
        return path;
    }
};
