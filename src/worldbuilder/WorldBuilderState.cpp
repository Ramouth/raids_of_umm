#include "WorldBuilderState.h"
#include "adventure/AdventureState.h"
#include "core/Application.h"
#include "render/TileVisuals.h"
#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <cstdio>
#include <iostream>

// ── Construction ──────────────────────────────────────────────────────────────

WorldBuilderState::WorldBuilderState(const std::string& mapPath)
    : m_savePath(mapPath), m_loadOnEnter(true) {}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void WorldBuilderState::onEnter() {
    auto& app = Application::get();
    m_cam = Camera2D(app.width(), app.height());
    m_hexRenderer.init();
    m_hexRenderer.loadTerrainTextures();
    m_hud.init();
    m_buildingSpriteRenderer.init();
    m_buildingSpriteRenderer.loadSprite("assets/textures/objects/castle.png");
    m_enemySpriteRenderer.init();
    m_enemySpriteRenderer.loadSprite("assets/textures/units/enemy_scout.png");

    if (m_loadOnEnter) {
        loadMap();
    } else {
        newMap();
    }

    // Load render offsets (missing file is not an error — fresh start)
    if (auto err = m_offsets.load(OFFSETS_PATH))
        std::cerr << "[WorldBuilder] Offsets load warning: " << *err << "\n";

    // Give the palette terrain texture IDs for icon previews
    GLuint texIds[TERRAIN_COUNT];
    for (int i = 0; i < TERRAIN_COUNT; ++i)
        texIds[i] = m_hexRenderer.terrainTex(static_cast<Terrain>(i));
    m_palette.setTerrainTextures(texIds, TERRAIN_COUNT);

    std::cout << "[WorldBuilder] Ready.\n";
    std::cout << "  Tab=cycle tool  1-6=terrain  O=obj type\n";
    std::cout << "  Ctrl+S=save  Ctrl+L=load  Ctrl+N=new  P=test-play  ESC=quit\n";
    std::cout << "  F1=alignment editor\n";
    renderHUD();
}

void WorldBuilderState::onExit() {
    if (m_dirty)
        std::cout << "[WorldBuilder] Warning: unsaved changes.\n";
}

// ── Map operations ────────────────────────────────────────────────────────────

void WorldBuilderState::newMap(int radius) {
    m_map.generateProcedural(radius);
    m_dirty = false;
    std::cout << "[WorldBuilder] New procedural map, radius " << radius
              << "  (" << m_map.tileCount() << " tiles)\n";
    renderHUD();
}

void WorldBuilderState::saveMap() {
    auto err = m_map.saveJson(m_savePath);
    if (err) {
        std::cerr << "[WorldBuilder] Save failed: " << *err << "\n";
    } else {
        m_dirty = false;
        std::cout << "[WorldBuilder] Saved to " << m_savePath << "\n";
    }
}

void WorldBuilderState::loadMap() {
    auto err = m_map.loadJson(m_savePath);
    if (err) {
        std::cerr << "[WorldBuilder] Load failed: " << *err << " — keeping current map.\n";
    } else {
        m_dirty = false;
        std::cout << "[WorldBuilder] Loaded " << m_map.name()
                  << "  (" << m_map.tileCount() << " tiles)\n";
        renderHUD();
    }
}

void WorldBuilderState::launchPlay() {
    std::cout << "[WorldBuilder] Launching test-play...\n";
    // Pass a deep copy so the builder's map isn't affected by game session.
    Application::get().pushState(
        std::make_unique<AdventureState>(m_map.clone()));
}

// ── Tools ─────────────────────────────────────────────────────────────────────

void WorldBuilderState::applyToolAt(const HexCoord& coord) {
    if (!m_map.hasTile(coord)) return;

    switch (m_tool) {
        case EditorTool::PaintTile: {
            MapTile tile = makeTile(m_paintTerrain);
            // Preserve any existing object's passability requirement.
            if (m_map.objectAt(coord))
                tile.passable = true;
            m_map.setTile(coord, tile);
            m_dirty = true;
            break;
        }
        case EditorTool::PlaceObject: {
            // Ensure the tile is passable before placing an object.
            if (MapTile* t = m_map.tileAt(coord)) {
                if (!t->passable) {
                    t->terrain  = Terrain::Sand;
                    t->passable = true;
                    t->moveCost = 1.0f;
                }
            }
            MapObjectDef obj;
            obj.pos  = coord;
            obj.type = m_placeObjType;
            obj.name = std::string(objTypeName(m_placeObjType));
            m_map.placeObject(std::move(obj));
            m_dirty = true;
            break;
        }
        case EditorTool::Erase: {
            m_map.removeObjectAt(coord);
            m_map.setTile(coord, makeTile(Terrain::Sand));
            m_dirty = true;
            break;
        }
        case EditorTool::Select: {
            const MapTile* tile = m_map.tileAt(coord);
            std::cout << "  [Select] (" << coord.q << "," << coord.r << ")"
                      << "  terrain=" << terrainName(tile ? tile->terrain : Terrain::Sand)
                      << "  passable=" << (tile && tile->passable ? "yes" : "no");
            if (const MapObjectDef* obj = m_map.objectAt(coord))
                std::cout << "  object=" << obj->typeName() << " \"" << obj->name << "\"";
            std::cout << "\n";
            break;
        }
    }
}

void WorldBuilderState::cycleTool() {
    int next = (static_cast<int>(m_tool) + 1) % 4;
    m_tool   = static_cast<EditorTool>(next);
    renderHUD();
}

void WorldBuilderState::cyclePaintTerrain(int delta) {
    int t = (static_cast<int>(m_paintTerrain) + delta + TERRAIN_COUNT) % TERRAIN_COUNT;
    m_paintTerrain = static_cast<Terrain>(t);
    renderHUD();
}

void WorldBuilderState::cyclePlaceObjType(int delta) {
    int t = (static_cast<int>(m_placeObjType) + delta + OBJ_TYPE_COUNT) % OBJ_TYPE_COUNT;
    m_placeObjType = static_cast<ObjType>(t);
    renderHUD();
}

// ── Input ─────────────────────────────────────────────────────────────────────

bool WorldBuilderState::handleEvent(void* sdlEvent) {
    SDL_Event* e = static_cast<SDL_Event*>(sdlEvent);

    // ── Track modifier keys ──────────────────────────────────────────────────
    if (e->type == SDL_KEYDOWN || e->type == SDL_KEYUP) {
        SDL_Keymod mod = SDL_GetModState();
        m_ctrlHeld = (mod & KMOD_CTRL) != 0;
    }

    if (e->type == SDL_KEYDOWN) {
        SDL_Keycode sym = e->key.keysym.sym;

        // ── Application ─────────────────────────────────────────────────────
        if (sym == SDLK_ESCAPE) { Application::get().quit(); return true; }

        // ── F1: toggle alignment dev tool ────────────────────────────────────
        // Use scancode (physical key) — more reliable than keycode on Linux/Wayland.
        if (e->key.keysym.scancode == SDL_SCANCODE_F1) {
            m_devToolActive = !m_devToolActive;
            if (!m_devToolActive) m_hasDevSelected = false;
            std::cout << "[DevTool] " << (m_devToolActive ? "ON" : "OFF") << "\n";
            return true;
        }

        // ── Ctrl+letter shortcuts ─────────────────────────────────────────────
        if (m_ctrlHeld) {
            if (sym == SDLK_s) { saveMap(); return true; }
            if (sym == SDLK_l) { loadMap(); return true; }
            if (sym == SDLK_n) { newMap();  return true; }
            if (sym == SDLK_o) { devSaveOffsets(); return true; }
        }

        // ── Dev tool keys (only when active) ─────────────────────────────────
        if (m_devToolActive) {
            // Arrow keys: nudge X/Z offset (steal from camera pan)
            if (sym == SDLK_LEFT)  { devNudge(-NUDGE_STEP, 0, 0); return true; }
            if (sym == SDLK_RIGHT) { devNudge(+NUDGE_STEP, 0, 0); return true; }
            if (sym == SDLK_UP)    { devNudge(0, -NUDGE_STEP, 0); return true; }
            if (sym == SDLK_DOWN)  { devNudge(0, +NUDGE_STEP, 0); return true; }
            // [ / ] : nudge Y (height)
            if (sym == SDLK_LEFTBRACKET)  { devNudge(0, 0, -NUDGE_STEP); return true; }
            if (sym == SDLK_RIGHTBRACKET) { devNudge(0, 0, +NUDGE_STEP); return true; }
            // Mode / target toggles
            if (sym == SDLK_g) { m_devMode = DevMode::Global;  return true; }
            if (sym == SDLK_t) { m_devMode = DevMode::PerTile; return true; }
            if (sym == SDLK_b) {
                m_devEdit = (m_devEdit == DevEdit::Terrain) ? DevEdit::Object : DevEdit::Terrain;
                return true;
            }
            if (sym == SDLK_r) { devResetSelected(); return true; }
        }

        // ── Camera pan (WASD always; arrows only when dev tool is inactive) ──
        if (sym == SDLK_w)                                    { m_keyW = true; return true; }
        if (sym == SDLK_s && !m_ctrlHeld)                     { m_keyS = true; return true; }
        if (sym == SDLK_a)                                    { m_keyA = true; return true; }
        if (sym == SDLK_d)                                    { m_keyD = true; return true; }
        if (!m_devToolActive) {
            if (sym == SDLK_UP)    { m_keyW = true; return true; }
            if (sym == SDLK_DOWN)  { m_keyS = true; return true; }
            if (sym == SDLK_LEFT)  { m_keyA = true; return true; }
            if (sym == SDLK_RIGHT) { m_keyD = true; return true; }
        }

        // ── Tool cycling ────────────────────────────────────────────────────
        if (sym == SDLK_TAB) { cycleTool(); return true; }

        // ── Terrain shortcuts 1–6 ────────────────────────────────────────────
        if (sym == SDLK_1) { m_paintTerrain = Terrain::Sand;     m_palette.selectTerrain(Terrain::Sand);     renderHUD(); return true; }
        if (sym == SDLK_2) { m_paintTerrain = Terrain::Dune;     m_palette.selectTerrain(Terrain::Dune);     renderHUD(); return true; }
        if (sym == SDLK_3) { m_paintTerrain = Terrain::Rock;     m_palette.selectTerrain(Terrain::Rock);     renderHUD(); return true; }
        if (sym == SDLK_4) { m_paintTerrain = Terrain::Oasis;    m_palette.selectTerrain(Terrain::Oasis);    renderHUD(); return true; }
        if (sym == SDLK_5) { m_paintTerrain = Terrain::Ruins;    m_palette.selectTerrain(Terrain::Ruins);    renderHUD(); return true; }
        if (sym == SDLK_6) { m_paintTerrain = Terrain::Obsidian; m_palette.selectTerrain(Terrain::Obsidian); renderHUD(); return true; }
        if (sym == SDLK_7) { m_paintTerrain = Terrain::Mountain; m_palette.selectTerrain(Terrain::Mountain); renderHUD(); return true; }
        if (sym == SDLK_8) { m_paintTerrain = Terrain::River;    m_palette.selectTerrain(Terrain::River);    renderHUD(); return true; }
        if (sym == SDLK_9) { m_paintTerrain = Terrain::Wall;     m_palette.selectTerrain(Terrain::Wall);     renderHUD(); return true; }
        if (sym == SDLK_0) { m_paintTerrain = Terrain::Battle;   m_palette.selectTerrain(Terrain::Battle);   renderHUD(); return true; }

        // ── Object type / test-play ──────────────────────────────────────────
        if (sym == SDLK_o && !m_ctrlHeld) { cyclePlaceObjType(+1); return true; }
        if (sym == SDLK_p)                { launchPlay();           return true; }
    }

    if (e->type == SDL_KEYUP) {
        switch (e->key.keysym.sym) {
            case SDLK_w:                              m_keyW = false; return true;
            case SDLK_s:                              m_keyS = false; return true;
            case SDLK_a:                              m_keyA = false; return true;
            case SDLK_d:                              m_keyD = false; return true;
            case SDLK_UP:    if (!m_devToolActive) { m_keyW = false; return true; } break;
            case SDLK_DOWN:  if (!m_devToolActive) { m_keyS = false; return true; } break;
            case SDLK_LEFT:  if (!m_devToolActive) { m_keyA = false; return true; } break;
            case SDLK_RIGHT: if (!m_devToolActive) { m_keyD = false; return true; } break;
        }
    }

    if (e->type == SDL_MOUSEWHEEL) {
        m_cam.adjustZoom(-e->wheel.y * Camera2D::ZOOM_STEP);
        return true;
    }

    if (e->type == SDL_MOUSEMOTION) {
        // Palette gets first look — blocks map hover when cursor is over panel
        if (!m_palette.handleMouseMove(e->motion.x, e->motion.y)) {
            glm::vec2 wp = m_cam.screenToWorld(e->motion.x, e->motion.y);
            HexCoord  hc = HexCoord::fromWorld(wp.x, wp.y, HEX_SIZE);
            m_hasHovered = m_map.hasTile(hc);
            m_hovered    = hc;
        } else {
            m_hasHovered = false;  // hide map cursor while over palette
        }
    }

    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        // Palette click — sync selection to editor tool state
        if (m_palette.containsPoint(e->button.x)) {
            m_palette.handleMouseClick(e->button.x, e->button.y);
            syncFromPalette();
            return true;
        }

        // Map click
        glm::vec2 wp = m_cam.screenToWorld(e->button.x, e->button.y);
        HexCoord  hc = HexCoord::fromWorld(wp.x, wp.y, HEX_SIZE);
        if (m_devToolActive) {
            m_devSelected    = hc;
            m_hasDevSelected = m_map.hasTile(hc);
            if (m_hasDevSelected)
                m_devEdit = m_map.objectAt(hc) ? DevEdit::Object : DevEdit::Terrain;
        } else {
            applyToolAt(hc);
        }
        return true;
    }

    if (e->type == SDL_WINDOWEVENT &&
        e->window.event == SDL_WINDOWEVENT_RESIZED) {
        m_cam.resize(e->window.data1, e->window.data2);
    }

    return false;
}

// ── Update ────────────────────────────────────────────────────────────────────

void WorldBuilderState::update(float dt) {
    float speed = CAM_SPEED * m_cam.zoom() / 12.0f;
    float dx = 0.0f, dz = 0.0f;
    if (m_keyW) dz -= speed * dt;
    if (m_keyS) dz += speed * dt;
    if (m_keyA) dx -= speed * dt;
    if (m_keyD) dx += speed * dt;
    if (dx != 0.0f || dz != 0.0f)
        m_cam.pan(dx, dz);
}

// ── Render ────────────────────────────────────────────────────────────────────

void WorldBuilderState::render() {
    glm::mat4 vp = m_cam.viewProjMatrix();
    m_hexRenderer.beginFrame(vp, HEX_SIZE, {0.4f, 1.0f, 0.3f}, {1.0f, 0.92f, 0.70f}, {0.35f, 0.30f, 0.22f}, {m_cam.position().x, 0.0f, m_cam.position().y});
    renderTerrain();
    renderObjects();
    renderCursor();
    m_hexRenderer.endFrame();

    // 2D overlay — palette always visible, dev tool panel only when active
    {
        auto& app = Application::get();
        m_hud.begin(app.width(), app.height());
        m_palette.render(m_hud, app.height());
        if (m_devToolActive)
            renderDevToolHUD();
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
    }
}

void WorldBuilderState::renderTerrain() {
    for (const auto& [coord, tile] : m_map) {
        RenderOffset off = m_offsets.forTerrain(coord, tile.terrain);
        GLuint tex      = m_hexRenderer.terrainTex(tile.terrain);
        glm::vec3 color = (tex != 0) ? glm::vec3(1.0f) : terrainColor(tile.terrain);
        float h         = terrainHeight(tile.terrain) + off.dy;
        m_hexRenderer.drawTile(coord, color, HEX_SIZE, h, tex, {off.dx, off.dz});
    }
    // Always-on grid lines
    for (const auto& [coord, tile] : m_map) {
        float h = terrainHeight(tile.terrain);
        m_hexRenderer.drawOutline(coord, {0.05f, 0.04f, 0.03f}, HEX_SIZE, h);
    }
}

void WorldBuilderState::renderObjects() {
    glm::mat4 view = m_cam.viewMatrix();
    glm::mat4 proj = m_cam.projMatrix();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (const auto& obj : m_map.objects()) {
        RenderOffset off = m_offsets.forObject(obj.pos, obj.type);
        float wx, wz;
        obj.pos.toWorld(HEX_SIZE, wx, wz);
        wx += off.dx;  wz += off.dz;

        if (obj.type == ObjType::Dungeon) {
            m_enemySpriteRenderer.draw({wx, off.dy, wz}, HEX_SIZE, view, proj);
        } else {
            m_buildingSpriteRenderer.draw({wx, off.dy, wz}, HEX_SIZE * 1.8f, view, proj);
        }
    }

    glDisable(GL_BLEND);
}

void WorldBuilderState::renderCursor() {
    // Dev tool: draw a bright yellow ring around the selected tile
    if (m_devToolActive && m_hasDevSelected) {
        m_hexRenderer.drawOutline(m_devSelected, {1.0f, 1.0f, 0.0f}, HEX_SIZE * 1.02f);
    }

    if (!m_hasHovered) return;

    // Hover cursor — cyan in dev mode, tool-colour otherwise
    glm::vec3 cursorCol;
    if (m_devToolActive) {
        cursorCol = {0.0f, 1.0f, 1.0f};
    } else {
        switch (m_tool) {
            case EditorTool::PaintTile:   cursorCol = terrainColor(m_paintTerrain); break;
            case EditorTool::PlaceObject: cursorCol = objTypeColor(m_placeObjType); break;
            case EditorTool::Erase:       cursorCol = { 1.0f, 0.2f, 0.2f }; break;
            case EditorTool::Select:      cursorCol = { 1.0f, 1.0f, 1.0f }; break;
        }
    }
    m_hexRenderer.drawOutline(m_hovered, cursorCol, HEX_SIZE * 0.95f);
}

void WorldBuilderState::renderHUD() {
    const char* toolNames[] = { "PaintTile", "PlaceObject", "Erase", "Select" };
    std::cout << "  [HUD] tool=" << toolNames[static_cast<int>(m_tool)]
              << "  terrain=" << terrainName(m_paintTerrain)
              << "  objType=" << objTypeName(m_placeObjType)
              << (m_dirty ? "  *unsaved*" : "")
              << "\n";
}

// ── Palette sync ─────────────────────────────────────────────────────────────

void WorldBuilderState::syncFromPalette() {
    switch (m_palette.activeCategory()) {
        case EditorPalette::Category::Terrain:
            m_paintTerrain = m_palette.selectedTerrain();
            m_tool = EditorTool::PaintTile;
            break;
        case EditorPalette::Category::Objects:
            m_placeObjType = m_palette.selectedObjType();
            m_tool = EditorTool::PlaceObject;
            break;
        case EditorPalette::Category::Units:
            break;
    }
    renderHUD();
}

// ── Dev Tool helpers ──────────────────────────────────────────────────────────

void WorldBuilderState::devNudge(float dx, float dz, float dy) {
    if (!m_hasDevSelected) return;

    const MapTile* tile = m_map.tileAt(m_devSelected);
    if (!tile) return;

    bool perTile = (m_devMode == DevMode::PerTile);

    if (m_devEdit == DevEdit::Terrain) {
        RenderOffset& off = m_offsets.terrainRef(m_devSelected, tile->terrain, perTile);
        off.dx += dx;  off.dz += dz;  off.dy += dy;
    } else {
        const MapObjectDef* obj = m_map.objectAt(m_devSelected);
        if (!obj) return;
        RenderOffset& off = m_offsets.objectRef(m_devSelected, obj->type, perTile);
        off.dx += dx;  off.dz += dz;  off.dy += dy;
    }
    devSaveOffsets();
}

void WorldBuilderState::devSaveOffsets() {
    if (auto err = m_offsets.save(OFFSETS_PATH))
        std::cerr << "[DevTool] Save failed: " << *err << "\n";
}

void WorldBuilderState::devResetSelected() {
    if (!m_hasDevSelected) return;
    const MapTile* tile = m_map.tileAt(m_devSelected);
    if (!tile) return;
    bool perTile = (m_devMode == DevMode::PerTile);
    if (m_devEdit == DevEdit::Terrain) {
        m_offsets.terrainRef(m_devSelected, tile->terrain, perTile) = {};
    } else {
        const MapObjectDef* obj = m_map.objectAt(m_devSelected);
        if (obj) m_offsets.objectRef(m_devSelected, obj->type, perTile) = {};
    }
    devSaveOffsets();
}

void WorldBuilderState::renderDevToolHUD() {
    auto& app = Application::get();
    int sw = app.width();

    // Panel background
    float pw = 360.0f, ph = 210.0f;
    float px = (float)sw - pw - 10.0f, py = 10.0f;
    m_hud.drawRect(px, py, pw, ph, {0.0f, 0.0f, 0.0f, 0.78f});
    m_hud.drawRect(px, py, pw, 3.0f, {1.0f, 1.0f, 0.0f, 1.0f}); // yellow top border

    float ts = 1.5f;
    float tx = px + 8.0f;
    float ty = py + 8.0f;
    float ls = 14.0f; // line spacing

    glm::vec4 white  = {1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 yellow = {1.0f, 1.0f, 0.0f, 1.0f};
    glm::vec4 cyan   = {0.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 grey   = {0.6f, 0.6f, 0.6f, 1.0f};

    m_hud.drawText(tx, ty, ts, "=== ALIGNMENT EDITOR (F1=close) ===", yellow); ty += ls;
    m_hud.drawText(tx, ty, ts, "Click=select  Arrows=X/Z  [/]=Y  R=reset", grey); ty += ls;
    m_hud.drawText(tx, ty, ts, "G=Global  T=Per-tile  B=Terrain/Object", grey); ty += ls * 1.4f;

    if (!m_hasDevSelected) {
        m_hud.drawText(tx, ty, ts, "No tile selected — click a tile", cyan);
        return;
    }

    const MapTile* tile = m_map.tileAt(m_devSelected);
    if (!tile) return;

    // Tile info
    char buf[128];
    std::snprintf(buf, sizeof(buf), "Tile  (%d, %d)  terrain=%s",
                  m_devSelected.q, m_devSelected.r,
                  std::string(terrainName(tile->terrain)).c_str());
    m_hud.drawText(tx, ty, ts, buf, white); ty += ls;

    const char* modeStr = (m_devMode == DevMode::Global)  ? "Global"   : "Per-tile";
    const char* editStr = (m_devEdit == DevEdit::Terrain) ? "Terrain" : "Object";
    std::snprintf(buf, sizeof(buf), "Mode: %s   Editing: %s   (B=toggle)", modeStr, editStr);
    m_hud.drawText(tx, ty, ts, buf, cyan); ty += ls * 1.2f;

    const MapObjectDef* obj = m_map.objectAt(m_devSelected);

    // Terrain offset row
    {
        RenderOffset t = m_offsets.forTerrain(m_devSelected, tile->terrain);
        glm::vec4 rowCol = (m_devEdit == DevEdit::Terrain) ? yellow : grey;
        std::snprintf(buf, sizeof(buf), "Terrain  dx=%.3f  dz=%.3f  dy=%.3f",
                      t.dx, t.dz, t.dy);
        m_hud.drawText(tx, ty, ts, buf, rowCol); ty += ls;
    }

    // Object offset row (only shown if object present)
    if (obj) {
        RenderOffset o = m_offsets.forObject(m_devSelected, obj->type);
        glm::vec4 rowCol = (m_devEdit == DevEdit::Object) ? yellow : grey;
        std::snprintf(buf, sizeof(buf), "Object   dx=%.3f  dz=%.3f  dy=%.3f  [%s]",
                      o.dx, o.dz, o.dy, std::string(objTypeName(obj->type)).c_str());
        m_hud.drawText(tx, ty, ts, buf, rowCol); ty += ls;
    } else {
        m_hud.drawText(tx, ty, ts, "Object   (none on this tile)", grey); ty += ls;
    }
    ty += ls * 0.4f;

    // Override status hint in per-tile mode
    bool perTile = (m_devMode == DevMode::PerTile);
    if (perTile) {
        bool hasTileOvr = m_offsets.tileOverride.count(m_devSelected) > 0;
        bool hasObjOvr  = m_offsets.objectOverride.count(m_devSelected) > 0;
        bool hasOverride = (m_devEdit == DevEdit::Terrain) ? hasTileOvr : hasObjOvr;
        m_hud.drawText(tx, ty, ts,
                       hasOverride ? "(per-tile override active)" : "(using global default)",
                       grey);
    }
}
