#pragma once
#include "core/StateMachine.h"
#include "world/WorldMap.h"
#include "render/HexRenderer.h"
#include "render/Camera2D.h"
#include "hex/HexCoord.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>

/*
 * EditorTool — the active editing mode in WorldBuilderState.
 *
 * PaintTile   : left-click sets the hovered tile to m_paintTerrain.
 * PlaceObject : left-click places an object of m_placeObjType.
 * Erase       : left-click removes any object; resets tile to default Sand.
 * Select      : left-click prints tile/object info to console (inspect mode).
 */
enum class EditorTool : uint8_t {
    PaintTile   = 0,
    PlaceObject = 1,
    Erase       = 2,
    Select      = 3,
};

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

    // ── Rendering helpers ────────────────────────────────────────────────────
    void renderTerrain();
    void renderObjects();
    void renderCursor();
    void renderHUD();      // console-only HUD for now; proper UI comes later

    // ── Members ──────────────────────────────────────────────────────────────

    WorldMap    m_map;
    Camera2D    m_cam;
    HexRenderer m_hexRenderer;

    EditorTool  m_tool         = EditorTool::PaintTile;
    Terrain     m_paintTerrain = Terrain::Sand;
    ObjType     m_placeObjType = ObjType::Town;

    HexCoord    m_hovered;
    bool        m_hasHovered   = false;

    bool        m_dirty        = false;  // unsaved changes
    std::string m_savePath     = "data/maps/editor.json";

    // Was this state constructed with an explicit file path?
    bool        m_loadOnEnter  = false;

    // Key states
    bool m_keyW=false, m_keyA=false, m_keyS=false, m_keyD=false;
    bool m_ctrlHeld = false;

    static constexpr float HEX_SIZE  = 1.0f;
    static constexpr float CAM_SPEED = 8.0f;
};
