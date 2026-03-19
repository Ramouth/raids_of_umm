#pragma once
#include "core/StateMachine.h"
#include "render/HUDRenderer.h"
#include "render/Texture.h"
#include <glad/glad.h>
#include <string>
#include <vector>
#include <set>
#include <utility>

/*
 * TileBuilderState — pixel-art spritesheet → custom tile palette.
 *
 * Workflow:
 *   1. Open a PNG from assets/  (O key or auto-opens on entry)
 *   2. Set cell size to match the spritesheet grid  (+/- for W, [/] for H)
 *   3. Click / drag to select cells
 *   4. Press Enter → name the tileset → export
 *      • Crops each selected cell → assets/textures/custom/<name>/cC_rR.png
 *      • Writes  data/tilesets/<name>.json
 *
 * Controls
 *   O              open file browser
 *   Arrow keys     pan
 *   Scroll         zoom (centred on cursor)
 *   R              fit sheet to screen
 *   +/=  /  -      widen / narrow cell width  (step 8 px)
 *   ]  /  [        taller / shorter cell height (step 8 px)
 *   Left click     toggle cell (drag = extend toggle in same direction)
 *   Enter          open export dialog
 *   ESC            close dialog / pop state
 */
class TileBuilderState final : public GameState {
public:
    TileBuilderState() = default;

    void onEnter() override;
    void onExit()  override;
    void update(float dt) override;
    void render()  override;
    bool handleEvent(void* sdlEvent) override;

private:
    // ── Sheet ─────────────────────────────────────────────────────────────────
    void loadSheet(const std::string& path);
    void fitToScreen();
    void screenToCell(int mx, int my, int& col, int& row) const;
    bool inSheetArea(int mx) const;  // false when cursor is over preview panel

    HUDRenderer m_hud;
    PNGData     m_sheet;
    std::string m_sheetPath;

    // ── View ──────────────────────────────────────────────────────────────────
    float m_offsetX = 0.f, m_offsetY = 0.f;  // sheet top-left in screen pixels
    float m_zoom    = 1.f;

    // ── Grid ──────────────────────────────────────────────────────────────────
    int m_cellW = 64, m_cellH = 64;

    // ── Selection ─────────────────────────────────────────────────────────────
    std::set<std::pair<int,int>> m_selected;  // (col, row)

    // Drag-select state
    bool m_leftHeld    = false;
    bool m_toggleState = true;   // true=add, false=remove (set on first click)
    int  m_lastDragCol = -1, m_lastDragRow = -1;

    // ── Render helpers ────────────────────────────────────────────────────────
    void renderSheet();
    void renderGrid();
    void renderSelection();
    void renderPreviewPanel();
    void renderStatusBar();

    // ── File browser ──────────────────────────────────────────────────────────
    void openBrowser();
    void renderBrowser(int sw, int sh);

    bool                     m_showBrowser   = false;
    std::vector<std::string> m_browserFiles;
    int                      m_browserHovered = -1;
    int                      m_browserScroll  = 0;
    static constexpr int     BROWSER_ROWS     = 14;

    // ── Export dialog ─────────────────────────────────────────────────────────
    void openExportDialog();
    void commitExport();
    void renderExportDialog(int sw, int sh);

    bool        m_showExportDialog = false;
    std::string m_exportName;

    // ── Key state ─────────────────────────────────────────────────────────────
    bool m_keyLeft = false, m_keyRight = false;
    bool m_keyUp   = false, m_keyDown  = false;

    // ── Layout constants ──────────────────────────────────────────────────────
    static constexpr float PREVIEW_W = 210.f;
    static constexpr float STATUS_H  =  28.f;
    static constexpr int   CELL_MIN  =   4;
    static constexpr int   CELL_MAX  = 512;
    static constexpr int   CELL_STEP =   8;
    static constexpr float PAN_SPEED = 400.f;   // px/s
    static constexpr float ZOOM_STEP = 0.12f;   // fraction of current zoom per scroll tick
    static constexpr float ZOOM_MIN  = 0.05f;
    static constexpr float ZOOM_MAX  = 20.f;
};
