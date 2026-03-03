#include "WorldBuilderState.h"
#include "adventure/AdventureState.h"
#include "core/Application.h"
#include "render/TileVisuals.h"
#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <iostream>

// ── Construction ──────────────────────────────────────────────────────────────

WorldBuilderState::WorldBuilderState(const std::string& mapPath)
    : m_savePath(mapPath), m_loadOnEnter(true) {}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void WorldBuilderState::onEnter() {
    auto& app = Application::get();
    m_cam = Camera2D(app.width(), app.height());
    m_hexRenderer.init();

    if (m_loadOnEnter) {
        loadMap();
    } else {
        newMap();
    }

    std::cout << "[WorldBuilder] Ready.\n";
    std::cout << "  Tab=cycle tool  1-6=terrain  O=obj type\n";
    std::cout << "  Ctrl+S=save  Ctrl+L=load  Ctrl+N=new  P=test-play  ESC=quit\n";
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

        // ── Ctrl+letter shortcuts (check before single-key pan) ─────────────
        if (m_ctrlHeld) {
            if (sym == SDLK_s) { saveMap(); return true; }
            if (sym == SDLK_l) { loadMap(); return true; }
            if (sym == SDLK_n) { newMap();  return true; }
        }

        // ── Camera pan (no modifier required) ───────────────────────────────
        if (sym == SDLK_w || sym == SDLK_UP)    { m_keyW = true; return true; }
        if (sym == SDLK_s || sym == SDLK_DOWN)  { m_keyS = true; return true; }
        if (sym == SDLK_a || sym == SDLK_LEFT)  { m_keyA = true; return true; }
        if (sym == SDLK_d || sym == SDLK_RIGHT) { m_keyD = true; return true; }

        // ── Tool cycling ────────────────────────────────────────────────────
        if (sym == SDLK_TAB) { cycleTool(); return true; }

        // ── Terrain shortcuts 1–6 ────────────────────────────────────────────
        if (sym == SDLK_1) { m_paintTerrain = Terrain::Sand;     renderHUD(); return true; }
        if (sym == SDLK_2) { m_paintTerrain = Terrain::Dune;     renderHUD(); return true; }
        if (sym == SDLK_3) { m_paintTerrain = Terrain::Rock;     renderHUD(); return true; }
        if (sym == SDLK_4) { m_paintTerrain = Terrain::Oasis;    renderHUD(); return true; }
        if (sym == SDLK_5) { m_paintTerrain = Terrain::Ruins;    renderHUD(); return true; }
        if (sym == SDLK_6) { m_paintTerrain = Terrain::Obsidian; renderHUD(); return true; }

        // ── Object type / test-play ──────────────────────────────────────────
        if (sym == SDLK_o) { cyclePlaceObjType(+1); return true; }
        if (sym == SDLK_p) { launchPlay();           return true; }
    }

    if (e->type == SDL_KEYUP) {
        switch (e->key.keysym.sym) {
            case SDLK_w: case SDLK_UP:    m_keyW = false; return true;
            case SDLK_s: case SDLK_DOWN:  m_keyS = false; return true;
            case SDLK_a: case SDLK_LEFT:  m_keyA = false; return true;
            case SDLK_d: case SDLK_RIGHT: m_keyD = false; return true;
        }
    }

    if (e->type == SDL_MOUSEWHEEL) {
        m_cam.adjustZoom(-e->wheel.y * Camera2D::ZOOM_STEP);
        return true;
    }

    if (e->type == SDL_MOUSEMOTION) {
        glm::vec2 wp = m_cam.screenToWorld(e->motion.x, e->motion.y);
        HexCoord  hc = HexCoord::fromWorld(wp.x, wp.y, HEX_SIZE);
        m_hasHovered = m_map.hasTile(hc);
        m_hovered    = hc;
    }

    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        glm::vec2 wp = m_cam.screenToWorld(e->button.x, e->button.y);
        HexCoord  hc = HexCoord::fromWorld(wp.x, wp.y, HEX_SIZE);
        applyToolAt(hc);
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
    m_hexRenderer.beginFrame(vp, HEX_SIZE);
    renderTerrain();
    renderObjects();
    renderCursor();
    m_hexRenderer.endFrame();
}

void WorldBuilderState::renderTerrain() {
    for (const auto& [coord, tile] : m_map) {
        glm::vec3 color = terrainColor(tile.terrain);
        float h = terrainHeight(tile.terrain);
        m_hexRenderer.drawTile(coord, color, HEX_SIZE, h);
    }
    // Always-on grid lines — makes individual hex cells visible when painting.
    for (const auto& [coord, tile] : m_map) {
        m_hexRenderer.drawOutline(coord, {0.05f, 0.04f, 0.03f}, HEX_SIZE * 0.97f);
    }
}

void WorldBuilderState::renderObjects() {
    for (const auto& obj : m_map.objects()) {
        glm::vec3 col = objTypeColor(obj.type);
        m_hexRenderer.drawTile(obj.pos, col, HEX_SIZE * 0.55f, 0.25f);
    }
}

void WorldBuilderState::renderCursor() {
    if (!m_hasHovered) return;

    // Cursor colour depends on active tool.
    glm::vec3 cursorCol;
    switch (m_tool) {
        case EditorTool::PaintTile:   cursorCol = terrainColor(m_paintTerrain); break;
        case EditorTool::PlaceObject: cursorCol = objTypeColor(m_placeObjType); break;
        case EditorTool::Erase:       cursorCol = { 1.0f, 0.2f, 0.2f }; break;
        case EditorTool::Select:      cursorCol = { 1.0f, 1.0f, 1.0f }; break;
    }
    m_hexRenderer.drawOutline(m_hovered, cursorCol, HEX_SIZE * 0.95f);
}

void WorldBuilderState::renderHUD() {
    // No graphical HUD yet — print state to console.
    // This will be replaced by an on-screen panel when the UI system exists.
    const char* toolNames[] = { "PaintTile", "PlaceObject", "Erase", "Select" };
    std::cout << "  [HUD] tool=" << toolNames[static_cast<int>(m_tool)]
              << "  terrain=" << terrainName(m_paintTerrain)
              << "  objType=" << objTypeName(m_placeObjType)
              << (m_dirty ? "  *unsaved*" : "")
              << "\n";
}
