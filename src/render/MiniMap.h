#pragma once
#include "hex/HexCoord.h"
#include "world/WorldMap.h"
#include "world/ObjectControl.h"
#include "hero/Hero.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <unordered_set>
#include <vector>

class HUDRenderer;

/*
 * MiniMap — HoMM3-style overview in the bottom-right corner.
 *
 * Maintains a CPU-side RGBA pixel buffer (TEX_W × TEX_H).  Each hex maps to
 * a small rectangular block.  On update() the buffer is rebuilt and uploaded
 * to a GL_TEXTURE_2D; on render() it is drawn via HUDRenderer::drawTexturedRect.
 *
 * Fog-of-war tiers (matches AdventureState visibility sets):
 *   unexplored  → near-black
 *   explored    → 35% terrain colour
 *   visible     → full terrain colour
 *
 * Object dots (drawn on top of terrain colour):
 *   Town    → cyan   Mine (gold/crystal/sawmill…) → gold
 *   Dungeon → red    Artifact                     → white
 *
 * Hero → bright white dot (1 px radius).
 *
 * Click-to-pan: screenToHex() converts a mouse click inside the minimap
 * rect to the nearest HexCoord so the caller can recentre the camera.
 */
class MiniMap {
public:
    MiniMap() = default;
    ~MiniMap();

    MiniMap(const MiniMap&) = delete;
    MiniMap& operator=(const MiniMap&) = delete;

    // Call once after GL context is ready.  Computes hex→pixel mapping from map.
    void init(const WorldMap& map);

    // Rebuild the pixel buffer and re-upload to GPU.
    // Call whenever visibility, hero pos, or object ownership changes.
    void update(const WorldMap&           map,
                const std::unordered_set<HexCoord>& explored,
                const std::unordered_set<HexCoord>& visible,
                const Hero&              hero,
                const ObjectControlMap&  ctrl,
                const std::vector<Hero>& aiHeroes = {});

    // Draw the minimap panel in the bottom-right corner.
    // size: side length of the rendered square in screen pixels.
    void render(HUDRenderer& hud, int screenW, int screenH, int size = 180) const;

    // Convert a screen-space mouse position to the nearest HexCoord on the minimap.
    // Returns {INT_MIN, 0} if the click is outside the minimap rect.
    HexCoord screenToHex(float mx, float my,
                         int screenW, int screenH, int size = 180) const;

    bool ready() const { return m_tex != 0; }

private:
    // Texture dimensions (fixed at init).
    static constexpr int TEX_W = 128;
    static constexpr int TEX_H = 128;

    GLuint m_tex = 0;
    std::vector<uint8_t> m_pixels;  // RGBA, TEX_W × TEX_H

    // Pre-computed mapping: for each hex, the pixel-space centre.
    // Stored as (px, py) floats in [0, TEX_W) × [0, TEX_H).
    // Also used in reverse for click-to-pan.
    float m_worldMinX = 0, m_worldMinZ = 0;
    float m_worldSpanX = 1, m_worldSpanZ = 1;

    // Hex radius in pixels (each hex is drawn as a filled square of this size).
    int m_hexPx = 3;

    // Convert hex → pixel centre in the texture.
    glm::vec2 hexToPixel(const HexCoord& c) const;

    // Paint a filled square centred at (px, py) with radius r (inclusive).
    void paintSquare(int cx, int cy, int r, uint8_t R, uint8_t G, uint8_t B);

    // Clip-safe pixel write (skips out-of-bounds).
    void setPixel(int x, int y, uint8_t R, uint8_t G, uint8_t B);
};
