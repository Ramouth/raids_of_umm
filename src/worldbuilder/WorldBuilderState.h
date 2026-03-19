#pragma once
#include "core/StateMachine.h"
#include "world/WorldMap.h"
#include "render/HexRenderer.h"
#include "render/HUDRenderer.h"
#include "render/SpriteRenderer.h"
#include "render/Camera2D.h"
#include "render/RenderOffsets.h"
#include "ui/EditorPalette.h"
#include "hex/HexCoord.h"
#include "worldbuilder/EditorTool.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

/*
 * WorldBuilderState — the map authoring GameState.
 *
 * Owns a WorldMap exclusively — no hero, no turns, no game rules.
 * The result of authoring is a WorldMap that can be:
 *   - Saved to disk (Ctrl+S)
 *   - Loaded from disk (Ctrl+L)
 *   - Test-played immediately (P — pushes AdventureState with a copy)
 *
 * Controls
 *   WASD / arrows      pan camera
 *   scroll             zoom
 *   1–6                cycle paint terrain type
 *   O                  cycle place-object type
 *   Tab                cycle tools (Paint → Place → Erase → Select → ...)
 *   Left click         apply active tool
 *   Ctrl+S             save to m_savePath
 *   Ctrl+L             load from m_savePath
 *   Ctrl+N             new empty map (default radius)
 *   P                  test-play in AdventureState
 *   ESC                quit
 */
class WorldBuilderState final : public GameState {
public:
    // Defaults: new procedural map, save path "data/maps/editor.json".
    WorldBuilderState() = default;

    // Start with an existing map file loaded.
    explicit WorldBuilderState(const std::string& mapPath);

    void onEnter() override;
    void onExit()  override;
    void update(float dt) override;
    void render()  override;
    bool handleEvent(void* sdlEvent) override;

private:
    // ── Tools ────────────────────────────────────────────────────────────────
    void applyToolAt(const HexCoord& coord);
    void cycleTool();
    void cyclePaintTerrain(int delta);
    void cyclePlaceObjType(int delta);

    // ── Map operations ───────────────────────────────────────────────────────
    void newMap(int radius = 12);
    void saveMap();
    void loadMap();
    void launchPlay();     // push AdventureState with a copy of m_map

    // ── Palette sync ─────────────────────────────────────────────────────────
    void syncFromPalette();  // apply palette selection → m_tool / m_paintTerrain / m_placeObjType

    // ── Rendering helpers ────────────────────────────────────────────────────
    void renderTerrain();
    void renderObjects();
    void renderCursor();
    void renderHUD();           // console-only HUD
    void renderDevToolHUD();    // on-screen overlay for the alignment dev tool

    // ── Dev tool helpers ─────────────────────────────────────────────────────
    void devNudge(float dx, float dz, float dy);   // apply a nudge to selected tile
    void devSaveOffsets();
    void devResetSelected();

    // ── Members ──────────────────────────────────────────────────────────────

    WorldMap       m_map;
    Camera2D       m_cam;
    HexRenderer    m_hexRenderer;
    HUDRenderer    m_hud;
    SpriteRenderer m_objRenderers[OBJ_TYPE_COUNT];  // indexed by ObjType
    EditorPalette  m_palette;

    EditorTool  m_tool         = EditorTool::PaintTile;
    Terrain     m_paintTerrain = Terrain::Sand;
    ObjType     m_placeObjType = ObjType::Town;
    int         m_paintRotation = 0;  // 0-5: texture rotation steps (60° each)

    HexCoord    m_hovered;
    bool        m_hasHovered   = false;

    bool        m_dirty        = false;  // unsaved changes
    std::string m_savePath     = "data/maps/editor.json";

    // Was this state constructed with an explicit file path?
    bool        m_loadOnEnter  = false;

    // ── Exit prompt (ESC) ────────────────────────────────────────────────────
    void renderExitPrompt(int sw, int sh);
    bool m_showExitPrompt    = false;
    int  m_exitPromptHovered = -1;  // 0=Exit 1=Save 2=Load 3=Continue

    // ── Save-name dialog ─────────────────────────────────────────────────────
    void openSaveDialog();
    void commitSaveDialog();
    void renderSaveDialog(int sw, int sh);
    bool        m_showSaveDialog = false;
    std::string m_saveDialogText;   // current typed filename (no extension)

    // ── Map browser overlay (Ctrl+L) ─────────────────────────────────────────
    void openMapBrowser();
    void renderMapBrowser(int sw, int sh);

    bool                     m_showMapBrowser    = false;
    std::vector<std::string> m_browserFiles;     // full paths of data/maps/*.json
    int                      m_browserHovered    = -1;
    int                      m_browserScroll     = 0;
    static constexpr int     BROWSER_ROWS        = 10;

    // Key states
    bool m_keyW=false, m_keyA=false, m_keyS=false, m_keyD=false;
    bool m_ctrlHeld = false;

    // Drag-to-paint state
    bool     m_leftButtonHeld  = false;
    HexCoord m_lastPaintedHex;
    bool     m_hasLastPainted  = false;

    // Palette drag-scroll state
    bool m_palDragging      = false;
    bool m_palScrollbarDrag = false;  // true when drag started on the scrollbar thumb
    int  m_palDragStartY    = 0;
    int  m_palDragScrollY   = 0;
    int  m_lastMouseX       = 0;  // updated from MOUSEMOTION; used by wheel handler

    // Assorted texture IDs (owned here, freed in onExit)
    std::vector<GLuint> m_assortedTexIds;

    // ── Dev tool (F1) ────────────────────────────────────────────────────────
    enum class DevMode { Global, PerTile };
    enum class DevEdit { Terrain, Object  };

    bool        m_devToolActive  = false;
    HexCoord    m_devSelected;
    bool        m_hasDevSelected = false;
    DevMode     m_devMode        = DevMode::Global;
    DevEdit     m_devEdit        = DevEdit::Terrain;
    RenderOffsetConfig m_offsets;

    static constexpr float NUDGE_STEP = 0.02f;
    static constexpr float HEX_SIZE   = 1.0f;
    static constexpr float CAM_SPEED  = 8.0f;
    static constexpr int   PALETTE_DRAG_THRESHOLD = 10;  // px; distinguishes click from scroll
    static constexpr const char* OFFSETS_PATH = "assets/render_offsets.json";
};
