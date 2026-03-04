#include "EditorPalette.h"
#include "../render/HUDRenderer.h"
#include "../render/TileVisuals.h"
#include <cstdio>
#include <cstring>
#include <string>

// ── Theme colors (CurseForge dark navy + amber accent) ───────────────────────

static const glm::vec4 C_BG      = {0.09f, 0.09f, 0.14f, 0.97f};
static const glm::vec4 C_HDR     = {0.06f, 0.06f, 0.10f, 1.00f};
static const glm::vec4 C_TAB_BG  = {0.10f, 0.10f, 0.16f, 1.00f};
static const glm::vec4 C_AMBER   = {0.93f, 0.52f, 0.09f, 1.00f};
static const glm::vec4 C_CARD    = {0.12f, 0.12f, 0.19f, 1.00f};
static const glm::vec4 C_HOVER   = {0.17f, 0.17f, 0.26f, 1.00f};
static const glm::vec4 C_SEL     = {0.20f, 0.20f, 0.31f, 1.00f};
static const glm::vec4 C_DIV     = {0.18f, 0.18f, 0.28f, 1.00f};
static const glm::vec4 C_TEXT    = {0.92f, 0.92f, 0.95f, 1.00f};
static const glm::vec4 C_SUB     = {0.48f, 0.48f, 0.60f, 1.00f};

static const float PW = EditorPalette::PANEL_W;

// ── Setup ─────────────────────────────────────────────────────────────────────

void EditorPalette::setTerrainTextures(const GLuint* texIds, int count) {
    int n = count < TERRAIN_COUNT ? count : TERRAIN_COUNT;
    for (int i = 0; i < n; ++i) m_terrainTex[i] = texIds[i];
}

// ── Layout helpers ────────────────────────────────────────────────────────────

int EditorPalette::itemCount() const {
    switch (m_cat) {
        case Category::Terrain: return TERRAIN_COUNT;
        case Category::Objects: return OBJ_TYPE_COUNT;
        case Category::Units:   return 0;
    }
    return 0;
}

int EditorPalette::tabAtPoint(int x, int y) const {
    if (x < 0 || x >= (int)PW) return -1;
    if (y < (int)HDR_H || y >= (int)(HDR_H + TAB_H)) return -1;
    return (int)(x / (PW / 3.0f));
}

int EditorPalette::cardAtPoint(int x, int y) const {
    if (x < 0 || x >= (int)PW) return -1;
    float ly = listStartY();
    if (y < (int)ly) return -1;
    int idx = (int)((y - ly) / CARD_H);
    if (idx < 0 || idx >= itemCount()) return -1;
    return idx;
}

// ── Mouse events ──────────────────────────────────────────────────────────────

bool EditorPalette::handleMouseMove(int x, int y) {
    if (!containsPoint(x)) {
        m_hoverCard = -1;
        m_hoverTab  = -1;
        return false;
    }
    m_hoverTab  = tabAtPoint(x, y);
    m_hoverCard = (m_hoverTab >= 0) ? -1 : cardAtPoint(x, y);
    return true;
}

bool EditorPalette::handleMouseClick(int x, int y) {
    if (!containsPoint(x)) return false;

    int tab = tabAtPoint(x, y);
    if (tab >= 0) {
        m_cat       = static_cast<Category>(tab);
        m_hoverCard = -1;
        return true;
    }

    int card = cardAtPoint(x, y);
    if (card >= 0) {
        if (m_cat == Category::Terrain && card < TERRAIN_COUNT)
            m_terrain = static_cast<Terrain>(card);
        else if (m_cat == Category::Objects && card < OBJ_TYPE_COUNT)
            m_objType = static_cast<ObjType>(card);
        return true;
    }

    return true;  // always consume clicks inside the panel area
}

// ── Render ────────────────────────────────────────────────────────────────────

void EditorPalette::render(HUDRenderer& hud, int screenH) const {
    // Full-height dark panel background
    hud.drawRect(0.0f, 0.0f, PW, (float)screenH, C_BG);

    renderHeader(hud);
    renderTabs(hud);
    renderItems(hud, screenH);

    // Right-edge separator
    hud.drawRect(PW - 1.0f, 0.0f, 1.0f, (float)screenH, C_DIV);
}

void EditorPalette::renderHeader(HUDRenderer& hud) const {
    // Header background
    hud.drawRect(0.0f, 0.0f, PW, HDR_H, C_HDR);

    // Amber accent bar on left edge
    hud.drawRect(0.0f, 0.0f, 4.0f, HDR_H, C_AMBER);

    // Title
    hud.drawText(14.0f, 10.0f, 2.2f, "RAIDS",     C_AMBER);
    hud.drawText(14.0f, 31.0f, 1.2f, "Map Editor", C_SUB);

    // Bottom border of header
    hud.drawRect(0.0f, HDR_H - 1.0f, PW, 1.0f, C_DIV);
}

void EditorPalette::renderTabs(HUDRenderer& hud) const {
    float tabW = PW / 3.0f;
    float ty   = HDR_H;

    hud.drawRect(0.0f, ty, PW, TAB_H, C_TAB_BG);

    const char* labels[] = { "Terrain", "Objects", "Units" };
    for (int i = 0; i < 3; ++i) {
        float tx     = i * tabW;
        bool  active = (static_cast<int>(m_cat) == i);
        bool  hov    = (m_hoverTab == i);

        if (active)      hud.drawRect(tx, ty, tabW, TAB_H, C_SEL);
        else if (hov)    hud.drawRect(tx, ty, tabW, TAB_H, C_HOVER);

        // Amber bottom bar on active tab
        if (active)
            hud.drawRect(tx, ty + TAB_H - 3.0f, tabW, 3.0f, C_AMBER);

        // Label — approximate centering
        float lw   = (float)strlen(labels[i]) * 7.2f;  // ~7px per char at scale 1.2
        float tx2  = tx + (tabW - lw) / 2.0f;
        float ty2  = ty + (TAB_H - 11.0f) / 2.0f;
        hud.drawText(tx2, ty2, 1.2f, labels[i], active ? C_AMBER : C_SUB);

        // Vertical divider between tabs
        if (i < 2)
            hud.drawRect(tx + tabW - 1.0f, ty, 1.0f, TAB_H, C_DIV);
    }

    // Bottom border of tab row
    hud.drawRect(0.0f, ty + TAB_H - 1.0f, PW, 1.0f, C_DIV);
}

void EditorPalette::renderItems(HUDRenderer& hud, int screenH) const {
    float ly = listStartY();

    if (m_cat == Category::Terrain) {
        for (int i = 0; i < TERRAIN_COUNT; ++i) {
            float cardY = ly + i * CARD_H;
            if (cardY > (float)screenH) break;
            Terrain t = static_cast<Terrain>(i);
            renderCard(hud, cardY,
                       (m_terrain == t), (m_hoverCard == i),
                       terrainColor(t), m_terrainTex[i],
                       std::string(terrainName(t)).c_str(), "Terrain");
        }
    } else if (m_cat == Category::Objects) {
        for (int i = 0; i < OBJ_TYPE_COUNT; ++i) {
            float cardY = ly + i * CARD_H;
            if (cardY > (float)screenH) break;
            ObjType t = static_cast<ObjType>(i);
            renderCard(hud, cardY,
                       (m_objType == t), (m_hoverCard == i),
                       objTypeColor(t), 0,
                       std::string(objTypeName(t)).c_str(), "Object");
        }
    } else {
        // Units — placeholder
        hud.drawRect(0.0f, ly, PW, 48.0f, C_CARD);
        hud.drawText(PAD + 4.0f, ly + 16.0f, 1.3f, "Coming soon...", C_SUB);
    }
}

void EditorPalette::renderCard(HUDRenderer& hud, float y,
                                bool selected, bool hovered,
                                glm::vec3 iconColor, GLuint iconTex,
                                const char* name, const char* sub) const {
    // Card background
    glm::vec4 bg = selected ? C_SEL : (hovered ? C_HOVER : C_CARD);
    hud.drawRect(0.0f, y, PW, CARD_H, bg);

    // Amber left accent bar when selected
    if (selected)
        hud.drawRect(0.0f, y, 4.0f, CARD_H, C_AMBER);

    // Bottom divider line
    hud.drawRect(0.0f, y + CARD_H - 1.0f, PW, 1.0f, C_DIV);

    // Icon position — nudge right when selected so it clears the accent bar
    float iconX = PAD + (selected ? 4.0f : 0.0f);
    float iconY = y + (CARD_H - ICON_S) / 2.0f;

    // Icon shadow / dark backing
    hud.drawRect(iconX, iconY, ICON_S, ICON_S,
                 {iconColor.r * 0.30f, iconColor.g * 0.30f, iconColor.b * 0.30f, 1.0f});

    // Texture preview if available, else solid color swatch
    if (iconTex != 0) {
        hud.drawTexturedRect(iconX, iconY, ICON_S, ICON_S, iconTex);
    } else {
        float inset = 4.0f;
        hud.drawRect(iconX + inset, iconY + inset,
                     ICON_S - 2.0f * inset, ICON_S - 2.0f * inset,
                     {iconColor.r, iconColor.g, iconColor.b, 1.0f});
    }

    // Text — name + subtitle
    float textX = iconX + ICON_S + PAD;
    float nameY = y + CARD_H / 2.0f - 10.0f;
    float subY  = nameY + 13.0f;

    hud.drawText(textX, nameY, 1.5f, name, selected ? C_AMBER : C_TEXT);
    hud.drawText(textX, subY,  1.2f, sub,  C_SUB);
}
