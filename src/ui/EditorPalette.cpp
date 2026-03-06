#include "EditorPalette.h"
#include "../render/HUDRenderer.h"
#include "../render/TileVisuals.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

// ── Theme colors ──────────────────────────────────────────────────────────────

static const glm::vec4 C_BG      = {0.09f, 0.09f, 0.14f, 0.97f};
static const glm::vec4 C_HDR     = {0.06f, 0.06f, 0.10f, 1.00f};
static const glm::vec4 C_TAB_BG  = {0.10f, 0.10f, 0.16f, 1.00f};
static const glm::vec4 C_AMBER   = {0.93f, 0.52f, 0.09f, 1.00f};
static const glm::vec4 C_CARD    = {0.12f, 0.12f, 0.19f, 1.00f};
static const glm::vec4 C_GRP     = {0.11f, 0.11f, 0.17f, 1.00f};  // group header bg
static const glm::vec4 C_VAR     = {0.10f, 0.10f, 0.16f, 1.00f};  // variant sub-card bg
static const glm::vec4 C_INDENT  = {0.07f, 0.07f, 0.11f, 1.00f};  // variant left indent strip
static const glm::vec4 C_HOVER   = {0.17f, 0.17f, 0.26f, 1.00f};
static const glm::vec4 C_SEL     = {0.20f, 0.20f, 0.31f, 1.00f};
static const glm::vec4 C_DIV     = {0.18f, 0.18f, 0.28f, 1.00f};
static const glm::vec4 C_TEXT    = {0.92f, 0.92f, 0.95f, 1.00f};
static const glm::vec4 C_SUB     = {0.48f, 0.48f, 0.60f, 1.00f};

static const float PW = EditorPalette::PANEL_W;

// ── Setup ─────────────────────────────────────────────────────────────────────

void EditorPalette::setTerrainData(const GLuint textures[TERRAIN_COUNT][MAX_TERRAIN_VARIANTS],
                                    const int variantCounts[TERRAIN_COUNT]) {
    for (int i = 0; i < TERRAIN_COUNT; ++i) {
        m_variantCount[i] = variantCounts[i];
        for (int v = 0; v < MAX_TERRAIN_VARIANTS; ++v)
            m_terrainTex[i][v] = textures[i][v];
    }
}

// ── Layout helpers ────────────────────────────────────────────────────────────

float EditorPalette::contentHeight() const {
    if (m_cat == Category::Terrain) {
        float h = 0.0f;
        for (int i = 0; i < TERRAIN_COUNT; ++i) {
            h += GRP_H;
            if (m_groupExpanded[i])
                h += (float)std::max(1, m_variantCount[i]) * VAR_H;
        }
        return h;
    }
    return (float)OBJ_TYPE_COUNT * CARD_H;
}

void EditorPalette::clampScroll() {
    float visH = (float)m_screenH - listStartY();
    float ch   = contentHeight();
    int maxScroll = (int)std::max(0.0f, ch - visH);
    if (m_scrollY < 0)          m_scrollY = 0;
    if (m_scrollY > maxScroll)  m_scrollY = maxScroll;
}

int EditorPalette::tabAtPoint(int x, int y) const {
    if (x < 0 || x >= (int)PW) return -1;
    if (y < (int)HDR_H || y >= (int)(HDR_H + TAB_H)) return -1;
    return (int)(x / (PW / 3.0f));
}

bool EditorPalette::terrainHitTest(int /*x*/, int y, int& out_ti, int& out_v) const {
    float cy = listStartY() - (float)m_scrollY;
    for (int ti = 0; ti < TERRAIN_COUNT; ++ti) {
        // Group header
        if (y >= (int)cy && y < (int)(cy + GRP_H)) {
            out_ti = ti;
            out_v  = -1;
            return true;
        }
        cy += GRP_H;
        // Variant sub-cards (only if expanded)
        if (m_groupExpanded[ti]) {
            int vc = std::max(1, m_variantCount[ti]);
            for (int v = 0; v < vc; ++v) {
                if (y >= (int)cy && y < (int)(cy + VAR_H)) {
                    out_ti = ti;
                    out_v  = v;
                    return true;
                }
                cy += VAR_H;
            }
        }
    }
    return false;
}

int EditorPalette::objCardAtPoint(int x, int y) const {
    if (x < 0 || x >= (int)PW) return -1;
    float ly = listStartY() - (float)m_scrollY;
    if (y < (int)ly) return -1;
    int idx = (int)((y - ly) / CARD_H);
    if (idx < 0 || idx >= OBJ_TYPE_COUNT) return -1;
    return idx;
}

// ── Mouse events ──────────────────────────────────────────────────────────────

bool EditorPalette::handleMouseMove(int x, int y) {
    if (!containsPoint(x)) {
        m_hoverGroup   = -1;
        m_hoverVariant = -1;
        m_hoverCard    = -1;
        m_hoverTab     = -1;
        return false;
    }

    m_hoverTab = tabAtPoint(x, y);
    if (m_hoverTab >= 0) {
        m_hoverGroup = m_hoverCard = -1;
        return true;
    }

    if (m_cat == Category::Terrain) {
        m_hoverCard = -1;
        int ti, v;
        if (terrainHitTest(x, y, ti, v)) {
            m_hoverGroup   = ti;
            m_hoverVariant = v;
        } else {
            m_hoverGroup   = -1;
            m_hoverVariant = -1;
        }
    } else {
        m_hoverGroup = -1;
        m_hoverCard  = objCardAtPoint(x, y);
    }
    return true;
}

bool EditorPalette::handleMouseClick(int x, int y) {
    if (!containsPoint(x)) return false;

    // Tab row
    int tab = tabAtPoint(x, y);
    if (tab >= 0) {
        Category newCat = static_cast<Category>(tab);
        if (newCat != m_cat) m_scrollY = 0;
        m_cat        = newCat;
        m_hoverGroup = m_hoverCard = -1;
        return true;
    }

    if (m_cat == Category::Terrain) {
        int ti, v;
        if (!terrainHitTest(x, y, ti, v)) return true;

        Terrain t = static_cast<Terrain>(ti);
        if (v < 0) {
            // Group header clicked
            int vc = m_variantCount[ti];
            if (vc <= 1) {
                // Single-variant: direct select, no expand
                m_terrain = t;
                m_variant = 0;
            } else {
                // Multi-variant: toggle expand
                bool wasExpanded = m_groupExpanded[ti];
                m_groupExpanded[ti] = !wasExpanded;
                if (!wasExpanded) {
                    // Expanding: also select variant 0 of this terrain
                    m_terrain = t;
                    m_variant = 0;
                }
            }
        } else {
            // Variant card clicked — select it
            m_terrain = t;
            m_variant = v;
        }
        clampScroll();
        return true;
    }

    if (m_cat == Category::Objects) {
        int card = objCardAtPoint(x, y);
        if (card >= 0)
            m_objType = static_cast<ObjType>(card);
        return true;
    }

    return true;  // consume all clicks inside panel
}

// ── Render ────────────────────────────────────────────────────────────────────

void EditorPalette::render(HUDRenderer& hud, int screenH) const {
    m_screenH = screenH;
    hud.drawRect(0.0f, 0.0f, PW, (float)screenH, C_BG);
    renderHeader(hud);
    renderTabs(hud);
    renderItems(hud, screenH);
    hud.drawRect(PW - 1.0f, 0.0f, 1.0f, (float)screenH, C_DIV);
}

void EditorPalette::renderHeader(HUDRenderer& hud) const {
    hud.drawRect(0.0f, 0.0f, PW, HDR_H, C_HDR);
    hud.drawRect(0.0f, 0.0f, 4.0f, HDR_H, C_AMBER);
    hud.drawText(14.0f, 10.0f, 2.2f, "RAIDS",     C_AMBER);
    hud.drawText(14.0f, 31.0f, 1.2f, "Map Editor", C_SUB);
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

        if (active)   hud.drawRect(tx, ty, tabW, TAB_H, C_SEL);
        else if (hov) hud.drawRect(tx, ty, tabW, TAB_H, C_HOVER);

        if (active)
            hud.drawRect(tx, ty + TAB_H - 3.0f, tabW, 3.0f, C_AMBER);

        float lw  = (float)strlen(labels[i]) * 7.2f;
        float tx2 = tx + (tabW - lw) / 2.0f;
        float ty2 = ty + (TAB_H - 11.0f) / 2.0f;
        hud.drawText(tx2, ty2, 1.2f, labels[i], active ? C_AMBER : C_SUB);

        if (i < 2)
            hud.drawRect(tx + tabW - 1.0f, ty, 1.0f, TAB_H, C_DIV);
    }
    hud.drawRect(0.0f, ty + TAB_H - 1.0f, PW, 1.0f, C_DIV);
}

void EditorPalette::renderItems(HUDRenderer& hud, int screenH) const {
    float listStart  = listStartY();
    float visBottom  = (float)screenH;
    float cy         = listStart - (float)m_scrollY;

    if (m_cat == Category::Terrain) {
        for (int ti = 0; ti < TERRAIN_COUNT; ++ti) {
            // Group header
            if (cy + GRP_H > listStart && cy < visBottom) {
                bool grpSel     = (m_terrain == static_cast<Terrain>(ti));
                bool grpHovered = (m_hoverGroup == ti && m_hoverVariant == -1);
                renderTerrainGroup(hud, cy, ti, grpSel, grpHovered);
            }
            cy += GRP_H;

            // Variant cards when expanded
            if (m_groupExpanded[ti]) {
                int vc = std::max(1, m_variantCount[ti]);
                for (int v = 0; v < vc; ++v) {
                    if (cy + VAR_H > listStart && cy < visBottom) {
                        bool varSel  = (m_terrain == static_cast<Terrain>(ti) && m_variant == v);
                        bool varHov  = (m_hoverGroup == ti && m_hoverVariant == v);
                        renderVariantCard(hud, cy, ti, v, varSel, varHov);
                    }
                    cy += VAR_H;
                }
            }
        }

    } else if (m_cat == Category::Objects) {
        for (int i = 0; i < OBJ_TYPE_COUNT; ++i) {
            float cardY = listStart - (float)m_scrollY + i * CARD_H;
            if (cardY + CARD_H < listStart) continue;
            if (cardY > visBottom) break;
            ObjType ot = static_cast<ObjType>(i);
            renderObjCard(hud, cardY,
                          (m_objType == ot), (m_hoverCard == i),
                          objTypeColor(ot), 0,
                          std::string(objTypeName(ot)).c_str(), "Object");
        }

    } else {
        // Units placeholder
        hud.drawRect(0.0f, listStart, PW, 48.0f, C_CARD);
        hud.drawText(PAD + 4.0f, listStart + 16.0f, 1.3f, "Coming soon...", C_SUB);
    }

    // Scrollbar
    float ch   = contentHeight();
    float visH = visBottom - listStart;
    if (ch > visH) {
        float sbH = (visH / ch) * visH;
        if (sbH < 20.0f) sbH = 20.0f;
        float sbY = listStart + ((float)m_scrollY / (ch - visH)) * (visH - sbH);
        hud.drawRect(PW - 7.0f, sbY, 5.0f, sbH, C_SUB);
    }
}

void EditorPalette::renderTerrainGroup(HUDRenderer& hud, float y, int ti,
                                        bool selected, bool hovered) const {
    Terrain t        = static_cast<Terrain>(ti);
    bool    expanded = m_groupExpanded[ti];
    int     vc       = m_variantCount[ti];

    // Background
    glm::vec4 bg = selected ? C_SEL : (hovered ? C_HOVER : C_GRP);
    hud.drawRect(0.0f, y, PW, GRP_H, bg);

    // Amber left accent when selected
    if (selected)
        hud.drawRect(0.0f, y, 4.0f, GRP_H, C_AMBER);

    // Bottom divider
    hud.drawRect(0.0f, y + GRP_H - 1.0f, PW, 1.0f, C_DIV);

    // Icon
    float iconX = PAD + (selected ? 4.0f : 0.0f);
    float iconY = y + (GRP_H - GRP_ICON) / 2.0f;
    glm::vec3 tc = terrainColor(t);
    hud.drawRect(iconX, iconY, GRP_ICON, GRP_ICON,
                 {tc.r * 0.30f, tc.g * 0.30f, tc.b * 0.30f, 1.0f});
    GLuint tex0 = m_terrainTex[ti][0];
    if (tex0) {
        hud.drawTexturedRect(iconX, iconY, GRP_ICON, GRP_ICON, tex0);
    } else {
        float inset = 4.0f;
        hud.drawRect(iconX + inset, iconY + inset,
                     GRP_ICON - 2.0f * inset, GRP_ICON - 2.0f * inset,
                     {tc.r, tc.g, tc.b, 1.0f});
    }

    // Terrain name
    float textX = iconX + GRP_ICON + PAD;
    float nameY = y + (GRP_H - 11.0f) / 2.0f - (vc > 1 ? 5.0f : 0.0f);
    hud.drawText(textX, nameY, 1.5f,
                 std::string(terrainName(t)).c_str(),
                 selected ? C_AMBER : C_TEXT);

    // Variant count badge ("x3") below name when multiple variants exist
    if (vc > 1) {
        char badge[16];
        std::snprintf(badge, sizeof(badge), "x%d", vc);
        hud.drawText(textX, nameY + 14.0f, 1.1f, badge, C_SUB);
    }

    // Expand chevron on right edge (only for multi-variant terrains)
    if (vc > 1) {
        const char* chev = expanded ? "v" : ">";
        hud.drawText(PW - 18.0f, y + (GRP_H - 11.0f) / 2.0f, 1.3f, chev, C_SUB);
    }
}

void EditorPalette::renderVariantCard(HUDRenderer& hud, float y, int ti, int v,
                                       bool selected, bool hovered) const {
    Terrain t = static_cast<Terrain>(ti);

    // Dark indent strip on the left
    hud.drawRect(0.0f, y, VAR_INDENT, VAR_H, C_INDENT);

    // Card background after indent
    glm::vec4 bg = selected ? C_SEL : (hovered ? C_HOVER : C_VAR);
    hud.drawRect(VAR_INDENT, y, PW - VAR_INDENT, VAR_H, bg);

    // Amber accent inside indent when selected
    if (selected)
        hud.drawRect(VAR_INDENT, y, 3.0f, VAR_H, C_AMBER);

    // Bottom divider
    hud.drawRect(0.0f, y + VAR_H - 1.0f, PW, 1.0f, C_DIV);

    // Icon
    float iconX = VAR_INDENT + PAD + (selected ? 3.0f : 0.0f);
    float iconY = y + (VAR_H - VAR_ICON) / 2.0f;
    glm::vec3 tc = terrainColor(t);
    hud.drawRect(iconX, iconY, VAR_ICON, VAR_ICON,
                 {tc.r * 0.30f, tc.g * 0.30f, tc.b * 0.30f, 1.0f});
    GLuint tex = m_terrainTex[ti][v];
    if (tex) {
        hud.drawTexturedRect(iconX, iconY, VAR_ICON, VAR_ICON, tex);
    } else {
        float inset = 3.0f;
        hud.drawRect(iconX + inset, iconY + inset,
                     VAR_ICON - 2.0f * inset, VAR_ICON - 2.0f * inset,
                     {tc.r, tc.g, tc.b, 1.0f});
    }

    // Label
    float textX = iconX + VAR_ICON + PAD;
    float textY = y + (VAR_H - 11.0f) / 2.0f;
    char label[32];
    std::snprintf(label, sizeof(label), "Variant %d", v + 1);
    hud.drawText(textX, textY, 1.3f, label, selected ? C_AMBER : C_TEXT);
}

void EditorPalette::renderObjCard(HUDRenderer& hud, float y,
                                   bool selected, bool hovered,
                                   glm::vec3 iconColor, GLuint iconTex,
                                   const char* name, const char* sub) const {
    glm::vec4 bg = selected ? C_SEL : (hovered ? C_HOVER : C_CARD);
    hud.drawRect(0.0f, y, PW, CARD_H, bg);

    if (selected)
        hud.drawRect(0.0f, y, 4.0f, CARD_H, C_AMBER);

    hud.drawRect(0.0f, y + CARD_H - 1.0f, PW, 1.0f, C_DIV);

    float iconX = PAD + (selected ? 4.0f : 0.0f);
    float iconY = y + (CARD_H - ICON_S) / 2.0f;

    hud.drawRect(iconX, iconY, ICON_S, ICON_S,
                 {iconColor.r * 0.30f, iconColor.g * 0.30f, iconColor.b * 0.30f, 1.0f});

    if (iconTex != 0) {
        hud.drawTexturedRect(iconX, iconY, ICON_S, ICON_S, iconTex);
    } else {
        float inset = 4.0f;
        hud.drawRect(iconX + inset, iconY + inset,
                     ICON_S - 2.0f * inset, ICON_S - 2.0f * inset,
                     {iconColor.r, iconColor.g, iconColor.b, 1.0f});
    }

    float textX = iconX + ICON_S + PAD;
    float nameY = y + CARD_H / 2.0f - 10.0f;
    hud.drawText(textX, nameY,        1.5f, name, selected ? C_AMBER : C_TEXT);
    hud.drawText(textX, nameY + 13.0f, 1.2f, sub,  C_SUB);
}
