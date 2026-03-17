#include "MiniMap.h"
#include "HUDRenderer.h"
#include "TileVisuals.h"
#include "hex/HexCoord.h"
#include "world/MapTile.h"
#include "world/MapObject.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

MiniMap::~MiniMap() {
    if (m_tex) glDeleteTextures(1, &m_tex);
}

void MiniMap::init(const WorldMap& map) {
    // Compute world-space bounding box of all hex centres.
    float minX =  1e9f, maxX = -1e9f;
    float minZ =  1e9f, maxZ = -1e9f;

    for (const auto& [coord, tile] : map) {
        float wx, wz;
        coord.toWorld(1.0f, wx, wz);
        minX = std::min(minX, wx); maxX = std::max(maxX, wx);
        minZ = std::min(minZ, wz); maxZ = std::max(maxZ, wz);
    }

    m_worldMinX  = minX;
    m_worldMinZ  = minZ;
    m_worldSpanX = std::max(maxX - minX, 1.0f);
    m_worldSpanZ = std::max(maxZ - minZ, 1.0f);

    // Hex pixel radius — small enough that the whole map fits, at least 1 px.
    // We want approximately TEX_W / (mapWidthInHexes) per hex.
    float hexesX  = (maxX - minX) / 1.0f + 2.0f;  // approx hex count along X
    m_hexPx = std::max(1, static_cast<int>(TEX_W / hexesX));

    m_pixels.assign(TEX_W * TEX_H * 4, 0);  // all black (unexplored)

    // Create GL texture.
    glGenTextures(1, &m_tex);
    glBindTexture(GL_TEXTURE_2D, m_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 TEX_W, TEX_H, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, m_pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

glm::vec2 MiniMap::hexToPixel(const HexCoord& c) const {
    float wx, wz;
    c.toWorld(1.0f, wx, wz);
    float px = (wx - m_worldMinX) / m_worldSpanX * (TEX_W - 1);
    float py = (wz - m_worldMinZ) / m_worldSpanZ * (TEX_H - 1);
    return { px, py };
}

void MiniMap::setPixel(int x, int y, uint8_t R, uint8_t G, uint8_t B) {
    if (x < 0 || x >= TEX_W || y < 0 || y >= TEX_H) return;
    int idx = (y * TEX_W + x) * 4;
    m_pixels[idx + 0] = R;
    m_pixels[idx + 1] = G;
    m_pixels[idx + 2] = B;
    m_pixels[idx + 3] = 255;
}

void MiniMap::paintSquare(int cx, int cy, int r, uint8_t R, uint8_t G, uint8_t B) {
    for (int dy = -r; dy <= r; ++dy)
        for (int dx = -r; dx <= r; ++dx)
            setPixel(cx + dx, cy + dy, R, G, B);
}

void MiniMap::update(const WorldMap&                      map,
                     const std::unordered_set<HexCoord>&  explored,
                     const std::unordered_set<HexCoord>&  visible,
                     const Hero&                          hero,
                     const ObjectControlMap&              ctrl,
                     const std::vector<Hero>&             aiHeroes) {
    if (!m_tex) return;

    // Clear to near-black (unexplored void).
    std::fill(m_pixels.begin(), m_pixels.end(), 0);
    // Give unexplored tiles a very faint warm black so the panel is visible.
    for (int i = 0; i < TEX_W * TEX_H; ++i) {
        m_pixels[i * 4 + 0] = 8;
        m_pixels[i * 4 + 1] = 7;
        m_pixels[i * 4 + 2] = 6;
        m_pixels[i * 4 + 3] = 255;
    }

    // --- Terrain layer ---
    for (const auto& [coord, tile] : map) {
        bool exp = explored.count(coord) > 0;
        bool vis = visible.count(coord)  > 0;
        if (!exp) continue;

        glm::vec2 p = hexToPixel(coord);
        int cx = static_cast<int>(std::round(p.x));
        int cy = static_cast<int>(std::round(p.y));

        glm::vec3 col = tile.road ? roadColor() : terrainColor(tile.terrain);
        if (!vis) col *= 0.35f;  // explored-but-fogged: 35% brightness

        uint8_t R = static_cast<uint8_t>(std::min(255.0f, col.r * 255.0f));
        uint8_t G = static_cast<uint8_t>(std::min(255.0f, col.g * 255.0f));
        uint8_t B = static_cast<uint8_t>(std::min(255.0f, col.b * 255.0f));
        paintSquare(cx, cy, m_hexPx, R, G, B);
    }

    // --- Object dots (on top of terrain, only when visible or player-owned) ---
    for (const auto& obj : map.objects()) {
        bool vis = visible.count(obj.pos) > 0;
        if (!vis) {
            auto it = ctrl.find(obj.pos);
            bool playerOwned = it != ctrl.end() && it->second.ownerFaction == 1;
            if (!playerOwned) continue;
        }

        glm::vec2 p = hexToPixel(obj.pos);
        int cx = static_cast<int>(std::round(p.x));
        int cy = static_cast<int>(std::round(p.y));

        // Dot colours: Town=cyan, Dungeon=red, Mine types=gold, Artifact=white
        uint8_t R, G, B;
        switch (obj.type) {
            case ObjType::Town:
                R=  0; G=220; B=230; break;
            case ObjType::Dungeon:
                R=220; G= 30; B= 30; break;
            case ObjType::GoldMine:
            case ObjType::CrystalMine:
            case ObjType::Sawmill:
            case ObjType::Quarry:
            case ObjType::ObsidianVent:
                R=220; G=180; B= 20; break;
            case ObjType::Artifact:
                R=255; G=255; B=255; break;
            default:
                R=200; G=200; B=200; break;
        }
        // Draw a 1-px dot (radius 0 = single pixel, radius 1 for towns/dungeons).
        int dotR = (obj.type == ObjType::Town || obj.type == ObjType::Dungeon) ? 1 : 0;
        paintSquare(cx, cy, dotR, R, G, B);
    }

    // --- AI hero dots — orange, radius 1 ---
    for (const Hero& ai : aiHeroes) {
        if (visible.count(ai.pos) > 0) {
            glm::vec2 p = hexToPixel(ai.pos);
            paintSquare(static_cast<int>(std::round(p.x)),
                        static_cast<int>(std::round(p.y)),
                        1, 255, 130, 20);
        }
    }

    // --- Hero dot — bright white, radius 1 ---
    if (explored.count(hero.pos) > 0) {
        glm::vec2 p = hexToPixel(hero.pos);
        int cx = static_cast<int>(std::round(p.x));
        int cy = static_cast<int>(std::round(p.y));
        paintSquare(cx, cy, 1, 255, 255, 200);
    }

    // Upload to GPU (full re-upload; 128×128 RGBA is negligible cost).
    glBindTexture(GL_TEXTURE_2D, m_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 TEX_W, TEX_H, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, m_pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

void MiniMap::render(HUDRenderer& hud, int screenW, int screenH, int size) const {
    if (!m_tex) return;
    float x = static_cast<float>(screenW - size - 8);
    float y = static_cast<float>(screenH - size - 8);

    // Dark border.
    hud.drawRect(x - 2, y - 2, static_cast<float>(size + 4), static_cast<float>(size + 4),
                 {0.0f, 0.0f, 0.0f, 0.85f});
    hud.drawTexturedRect(x, y, static_cast<float>(size), static_cast<float>(size), m_tex);
    // Thin gold frame.
    hud.drawRect(x - 1, y - 1, static_cast<float>(size + 2), 1,         {0.75f, 0.60f, 0.20f, 1.0f});
    hud.drawRect(x - 1, y + size, static_cast<float>(size + 2), 1,      {0.75f, 0.60f, 0.20f, 1.0f});
    hud.drawRect(x - 1, y - 1, 1, static_cast<float>(size + 2),         {0.75f, 0.60f, 0.20f, 1.0f});
    hud.drawRect(x + size, y - 1, 1, static_cast<float>(size + 2),      {0.75f, 0.60f, 0.20f, 1.0f});
}

HexCoord MiniMap::screenToHex(float mx, float my,
                               int screenW, int screenH, int size) const {
    float rx = static_cast<float>(screenW - size - 8);
    float ry = static_cast<float>(screenH - size - 8);

    if (mx < rx || mx > rx + size || my < ry || my > ry + size)
        return { INT_MIN, 0 };

    // Normalise to [0,1] within the minimap rect.
    float u = (mx - rx) / size;
    float v = (my - ry) / size;

    // Map back to world space.
    float wx = m_worldMinX + u * m_worldSpanX;
    float wz = m_worldMinZ + v * m_worldSpanZ;

    return HexCoord::fromWorld(wx, wz, 1.0f);
}
