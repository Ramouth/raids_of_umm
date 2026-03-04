#pragma once
#include "hex/HexCoord.h"
#include <vector>

/*
 * CombatMap — the shape and layout of the combat battlefield.
 *
 * An 11 × 5 flat-top hex grid using even-q offset coordinates.
 * Conceptually similar to HoMM3's rectangular combat field.
 *
 * Coordinate system:
 *   offset (col 0..10, row 0..4)  <-->  axial HexCoord
 *   using "even-q" flat-top conversion (see redblobgames.com/grids/hexagons).
 *
 * Deployment zones:
 *   Player  → columns 0–1   (left edge)
 *   Enemy   → columns 9–10  (right edge)
 *   Neutral → columns 2–8   (open field)
 *
 * No render types, no session state — pure grid geometry.
 */
class CombatMap {
public:
    static constexpr int GRID_W = 11;
    static constexpr int GRID_H = 5;

    // ── Coordinate conversion ─────────────────────────────────────────────────

    // Offset (col, row) → axial HexCoord   [even-q flat-top]
    static HexCoord toHex(int col, int row) {
        return { col, row - (col - (col & 1)) / 2 };
    }

    // Axial HexCoord → offset (col, row).  Returns false if out-of-bounds.
    static bool fromHex(HexCoord c, int& col, int& row) {
        col = c.q;
        row = c.r + (c.q - (c.q & 1)) / 2;
        return col >= 0 && col < GRID_W && row >= 0 && row < GRID_H;
    }

    static bool inBounds(HexCoord c) {
        int col, row;
        return fromHex(c, col, row);
    }

    // ── Grid enumeration ──────────────────────────────────────────────────────

    // All 55 hexes in column-major order (col 0..10, row 0..4 each).
    static std::vector<HexCoord> allHexes() {
        std::vector<HexCoord> out;
        out.reserve(GRID_W * GRID_H);
        for (int col = 0; col < GRID_W; ++col)
            for (int row = 0; row < GRID_H; ++row)
                out.push_back(toHex(col, row));
        return out;
    }

    // ── Deployment spawns ─────────────────────────────────────────────────────

    // Spawn hexes for the player army (col 0, rows 0..4, centred on row 2).
    static std::vector<HexCoord> playerSpawns() {
        return _spawnsForCol(0);
    }

    // Spawn hexes for the enemy army (col 10, rows 0..4, centred on row 2).
    static std::vector<HexCoord> enemySpawns() {
        return _spawnsForCol(GRID_W - 1);
    }

private:
    // Returns up to GRID_H spawn positions in a column, centre-first.
    static std::vector<HexCoord> _spawnsForCol(int col) {
        // Order: row 2, 1, 3, 0, 4  (middle first, then alternate out)
        static constexpr int ORDER[GRID_H] = { 2, 1, 3, 0, 4 };
        std::vector<HexCoord> out;
        out.reserve(GRID_H);
        for (int o : ORDER)
            out.push_back(toHex(col, o));
        return out;
    }
};
