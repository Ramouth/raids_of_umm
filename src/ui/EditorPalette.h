#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "../world/MapTile.h"    // Terrain, TERRAIN_COUNT
#include "../world/MapObject.h"  // ObjType, OBJ_TYPE_COUNT

class HUDRenderer;

/*
 * EditorPalette — CurseForge-style left-side panel for the WorldBuilderState.
 *
 * Renders a dark panel with three tabs (Terrain / Objects / Units) and a
 * scrollable list of item cards. Handles its own mouse events and exposes
 * the currently-selected item so WorldBuilderState can sync its tool state.
 *
 * Usage per frame:
 *   // after hud.begin():
 *   m_palette.render(m_hud, screenH);
 *
 * Mouse routing (before the map sees the event):
 *   if (!m_palette.handleMouseMove(x, y))  // → update map hover
 *   if (m_palette.containsPoint(x))       { m_palette.handleMouseClick(x,y); }
 */
class EditorPalette {
public:
    enum class Category { Terrain = 0, Objects = 1, Units = 2 };

    static constexpr float PANEL_W = 240.0f;

    // ── Setup ─────────────────────────────────────────────────────────────────

    // Call after HexRenderer::loadTerrainTextures() so cards show texture previews.
    void setTerrainTextures(const GLuint* texIds, int count);

    // Sync selections from outside (keyboard shortcuts, etc.)
    void selectTerrain(Terrain t) { m_terrain = t; m_cat = Category::Terrain; }
    void selectObjType(ObjType  t) { m_objType = t; m_cat = Category::Objects; }

    // ── Mouse events ──────────────────────────────────────────────────────────

    // Returns true if the event falls inside the palette and is consumed.
    // When handleMouseMove returns true, the caller should clear map hover.
    bool handleMouseMove (int screenX, int screenY);
    bool handleMouseClick(int screenX, int screenY);

    bool containsPoint(int screenX) const { return screenX >= 0 && screenX < (int)PANEL_W; }

    // ── Render ────────────────────────────────────────────────────────────────

    // Call after hud.begin(). Draws the full panel over the left PANEL_W pixels.
    void render(HUDRenderer& hud, int screenH) const;

    // ── Queries ───────────────────────────────────────────────────────────────

    Category activeCategory()  const { return m_cat; }
    Terrain  selectedTerrain() const { return m_terrain; }
    ObjType  selectedObjType() const { return m_objType; }

private:
    // ── Layout constants ──────────────────────────────────────────────────────
    static constexpr float HDR_H  = 48.0f;  // header bar height
    static constexpr float TAB_H  = 30.0f;  // tab row height
    static constexpr float CARD_H = 56.0f;  // height of each item card
    static constexpr float ICON_S = 38.0f;  // icon square size inside card
    static constexpr float PAD    =  8.0f;  // general padding

    // ── Render helpers ────────────────────────────────────────────────────────
    void renderHeader  (HUDRenderer& hud) const;
    void renderTabs    (HUDRenderer& hud) const;
    void renderItems   (HUDRenderer& hud, int screenH) const;
    void renderCard    (HUDRenderer& hud, float y, bool selected, bool hovered,
                        glm::vec3 iconColor, GLuint iconTex,
                        const char* name, const char* sub) const;

    // ── Layout helpers ────────────────────────────────────────────────────────
    float listStartY()          const { return HDR_H + TAB_H; }
    int   itemCount()           const;
    int   tabAtPoint (int x, int y) const;
    int   cardAtPoint(int x, int y) const;

    // ── State ─────────────────────────────────────────────────────────────────
    Category m_cat     = Category::Terrain;
    Terrain  m_terrain = Terrain::Sand;
    ObjType  m_objType = ObjType::Town;

    int m_hoverCard = -1;
    int m_hoverTab  = -1;

    GLuint m_terrainTex[TERRAIN_COUNT] = {};
};
