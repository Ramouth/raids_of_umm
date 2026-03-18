#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "../world/MapTile.h"    // Terrain, TERRAIN_COUNT, MAX_TERRAIN_VARIANTS
#include "../world/MapObject.h"  // ObjType, OBJ_TYPE_COUNT

class HUDRenderer;

/*
 * EditorPalette — left-side panel for the WorldBuilderState.
 *
 * Terrain tab: accordion-style groups — one row per terrain type.
 * Clicking a group header expands it to show texture variant sub-cards.
 * Terrains with only 1 variant are selected directly on header click.
 *
 * Scrolling: click-and-drag (primary) or mousewheel (secondary).
 * The drag state lives in WorldBuilderState; EditorPalette exposes
 * scrollY() / setScrollY() so the parent can drive it.
 *
 * Usage per frame:
 *   m_palette.render(m_hud, screenH);
 *
 * Mouse routing:
 *   // On MOUSEBUTTONDOWN: if containsPoint → start drag tracking (in WorldBuilderState)
 *   // On MOUSEMOTION    : handleMouseMove (hover) or setScrollY (while dragging)
 *   // On MOUSEBUTTONUP  : if tiny drag → handleMouseClick + syncFromPalette
 */
class EditorPalette {
public:
    enum class Category { Terrain = 0, Objects = 1, Units = 2, Assorted = 3 };

    static constexpr float PANEL_W      = 240.0f;
    static constexpr int   MAX_ASSORTED = 64;

    // ── Setup ─────────────────────────────────────────────────────────────────

    // Call after HexRenderer::loadTerrainTextures().
    void setTerrainData(const GLuint textures[TERRAIN_COUNT][MAX_TERRAIN_VARIANTS],
                        const int variantCounts[TERRAIN_COUNT]);

    // Call after scanning assets/textures/assorted/.
    // names: null-terminated stem of each file (e.g. "for_sorting_r0_c0").
    void setAssortedData(const GLuint* texs, const char names[][64], int count);

    // Keyboard shortcut sync — expands the group for t, collapses others.
    void selectTerrain(Terrain t) {
        if (m_cat != Category::Terrain) m_scrollY = 0;
        m_terrain = t; m_variant = 0; m_cat = Category::Terrain;
        for (int i = 0; i < TERRAIN_COUNT; ++i)
            m_groupExpanded[i] = (i == static_cast<int>(t));
        clampScroll();
    }
    void selectObjType(ObjType t) {
        if (m_cat != Category::Objects) m_scrollY = 0;
        m_objType = t; m_cat = Category::Objects;
    }

    int  selectedAssorted() const { return m_assortedSelected; }

    void scroll(int delta)  { m_scrollY += delta; clampScroll(); }
    void resetScroll()      { m_scrollY = 0; }
    int  scrollY()    const { return m_scrollY; }
    void setScrollY(int y)  { m_scrollY = y; clampScroll(); }

    // ── Mouse events ──────────────────────────────────────────────────────────

    // Returns true if the event falls inside the palette and is consumed.
    bool handleMouseMove (int screenX, int screenY);
    bool handleMouseClick(int screenX, int screenY);

    bool containsPoint(int screenX) const { return screenX >= 0 && screenX < (int)PANEL_W; }

    // Y coordinate where the scrollable item list starts (below header + tab bar).
    // Use this to distinguish header/tab-area clicks from list-area clicks.
    static constexpr int itemListStartY() { return static_cast<int>(HDR_H + TAB_H); }

    // True when (x,y) is within the scrollbar track on the right edge of the panel.
    // Use this at MOUSEBUTTONDOWN to decide between content-drag and scrollbar-drag.
    bool isOnScrollbar(int screenX, int screenY) const;

    // Scroll delta (in scrollY units) for moving the scrollbar thumb by pixelDelta px.
    // Use this when the user is dragging the scrollbar thumb directly.
    int scrollbarPixelToScrollY(int pixelDelta) const;

    // ── Render ────────────────────────────────────────────────────────────────

    void render(HUDRenderer& hud, int screenH) const;

    // ── Queries ───────────────────────────────────────────────────────────────

    Category activeCategory()  const { return m_cat; }
    Terrain  selectedTerrain() const { return m_terrain; }
    int      selectedVariant() const { return m_variant; }
    ObjType  selectedObjType() const { return m_objType; }

private:
    // ── Layout constants ──────────────────────────────────────────────────────
    static constexpr float HDR_H      = 48.0f;  // header bar
    static constexpr float TAB_H      = 30.0f;  // tab row
    static constexpr float GRP_H      = 44.0f;  // terrain group header
    static constexpr float VAR_H      = 40.0f;  // terrain variant sub-card
    static constexpr float VAR_INDENT = 14.0f;  // indent for variant cards
    static constexpr float GRP_ICON   = 28.0f;  // icon size in group header
    static constexpr float VAR_ICON   = 24.0f;  // icon size in variant card
    static constexpr float CARD_H          = 56.0f;  // object card height
    static constexpr float ICON_S          = 38.0f;  // icon size in object card
    static constexpr float PAD             =  8.0f;
    static constexpr float ASSORTED_CARD_H = 106.0f; // assorted thumbnail card (2-col grid)

    // ── Render helpers ────────────────────────────────────────────────────────
    void renderHeader       (HUDRenderer& hud) const;
    void renderTabs         (HUDRenderer& hud) const;
    void renderItems        (HUDRenderer& hud, int screenH) const;
    void renderTerrainGroup (HUDRenderer& hud, float y, int ti,
                             bool selected, bool hovered) const;
    void renderVariantCard  (HUDRenderer& hud, float y, int ti, int v,
                             bool selected, bool hovered) const;
    void renderObjCard      (HUDRenderer& hud, float y, bool selected, bool hovered,
                             glm::vec3 iconColor, GLuint iconTex,
                             const char* name, const char* sub) const;
    void renderAssortedGrid (HUDRenderer& hud, int screenH) const;

    // ── Layout helpers ────────────────────────────────────────────────────────
    float listStartY()    const { return HDR_H + TAB_H; }
    float contentHeight() const;

    // 2-column assorted grid: card index (-1 = miss) at screen coords.
    int  assortedCardAtPoint(int x, int y) const;

    int  tabAtPoint(int x, int y) const;

    // Terrain hit-test: fills ti (terrain index) and v (-1 = group header, >=0 = variant).
    bool terrainHitTest(int x, int y, int& ti, int& v) const;
    int  objCardAtPoint(int x, int y) const;

    // ── State ─────────────────────────────────────────────────────────────────
    Category m_cat     = Category::Terrain;
    Terrain  m_terrain = Terrain::Sand;
    int      m_variant = 0;
    ObjType  m_objType = ObjType::Town;

    bool m_groupExpanded[TERRAIN_COUNT] = {};

    // Hover state — terrain accordion
    int m_hoverGroup   = -1;  // terrain index being hovered (-1 = none)
    int m_hoverVariant = -1;  // -1 = group header hovered, >=0 = variant card
    // Hover state — objects tab
    int m_hoverCard    = -1;
    int m_hoverTab     = -1;

    int m_scrollY = 0;
    mutable int m_screenH = 600;  // cached from render(); used by clampScroll()

    GLuint m_terrainTex[TERRAIN_COUNT][MAX_TERRAIN_VARIANTS] = {};
    int    m_variantCount[TERRAIN_COUNT] = {};

    GLuint m_assortedTex[MAX_ASSORTED]      = {};
    char   m_assortedNames[MAX_ASSORTED][64] = {};
    int    m_assortedCount   = 0;
    int    m_assortedSelected = -1;
    int    m_assortedHover    = -1;

    void clampScroll();
};
