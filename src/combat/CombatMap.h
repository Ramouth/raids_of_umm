#pragma once
#include "hex/HexCoord.h"
#include <vector>

/*
 * CombatMap — the shape of one battlefield: a width × height flat-top hex
 * grid in even-q offset coordinates. The size comes from the battle's
 * CombatLayout (default 11 × 5, the open "field" — like HoMM3's field).
 *
 * Coordinate system:
 *   offset (col 0..width-1, row 0..height-1)  <-->  axial HexCoord
 *   using "even-q" flat-top conversion (see redblobgames.com/grids/hexagons).
 *
 * Deployment zones, obstacles and who moves first live in CombatLayout.
 * No render types, no session state — pure grid geometry.
 */
class CombatMap {
public:
    static constexpr int DEFAULT_W = 11;
    static constexpr int DEFAULT_H = 5;

    CombatMap(int width = DEFAULT_W, int height = DEFAULT_H) : m_w(width), m_h(height) {}

    int width()  const { return m_w; }
    int height() const { return m_h; }

    // ── Coordinate conversion ─────────────────────────────────────────────────

    // Offset (col, row) → axial HexCoord   [even-q flat-top]
    static HexCoord toHex(int col, int row) {
        return { col, row - (col - (col & 1)) / 2 };
    }

    // Axial HexCoord → offset (col, row), whatever the grid size.
    static void toOffset(HexCoord c, int& col, int& row) {
        col = c.q;
        row = c.r + (c.q - (c.q & 1)) / 2;
    }

    // Axial HexCoord → offset (col, row).  Returns false if out-of-bounds.
    bool fromHex(HexCoord c, int& col, int& row) const {
        toOffset(c, col, row);
        return col >= 0 && col < m_w && row >= 0 && row < m_h;
    }

    bool inBounds(HexCoord c) const {
        int col, row;
        return fromHex(c, col, row);
    }

    // Dense index (col-major) for per-hex arrays, or -1 if out of bounds.
    int index(HexCoord c) const {
        int col, row;
        return fromHex(c, col, row) ? col * m_h + row : -1;
    }
    int cellCount() const { return m_w * m_h; }

    // ── Grid enumeration ──────────────────────────────────────────────────────

    // Every hex in column-major order (col 0.., row 0.. each).
    std::vector<HexCoord> allHexes() const {
        std::vector<HexCoord> out;
        out.reserve(m_w * m_h);
        for (int col = 0; col < m_w; ++col)
            for (int row = 0; row < m_h; ++row)
                out.push_back(toHex(col, row));
        return out;
    }

private:
    int m_w;
    int m_h;
};
