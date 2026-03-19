#include "WorldBuilderState.h"
#include "adventure/AdventureState.h"
#include "tilebuilder/TileBuilderState.h"
#include "core/Application.h"
#include "render/TileVisuals.h"
#include "render/Texture.h"
#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <cstdio>
#include <cctype>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cstring>

// ── Construction ──────────────────────────────────────────────────────────────

WorldBuilderState::WorldBuilderState(const std::string& mapPath)
    : m_savePath(mapPath), m_loadOnEnter(true) {}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void WorldBuilderState::onEnter() {
    auto& app = Application::get();
    m_cam = Camera2D(app.width(), app.height());
    m_hexRenderer.init();
    m_hexRenderer.loadTerrainTextures();
    m_hexRenderer.loadEdgeTiles();
    m_hud.init();
    for (int i = 0; i < OBJ_TYPE_COUNT; ++i) {
        ObjType t = static_cast<ObjType>(i);
        m_objRenderers[i].init();
        m_objRenderers[i].loadSprite(objTypeSpritePath(t));
    }

    if (m_loadOnEnter) {
        loadMap();
    } else {
        newMap();
    }

    // Load render offsets (missing file is not an error — fresh start)
    if (auto err = m_offsets.load(OFFSETS_PATH))
        std::cerr << "[WorldBuilder] Offsets load warning: " << *err << "\n";

    // Give the palette per-terrain variant textures and counts
    GLuint textures[TERRAIN_COUNT][MAX_TERRAIN_VARIANTS] = {};
    int    variantCounts[TERRAIN_COUNT] = {};
    for (int i = 0; i < TERRAIN_COUNT; ++i) {
        Terrain t = static_cast<Terrain>(i);
        variantCounts[i] = m_hexRenderer.terrainVariantCount(t);
        for (int v = 0; v < MAX_TERRAIN_VARIANTS; ++v)
            textures[i][v] = m_hexRenderer.terrainTex(t, v);
    }
    m_palette.setTerrainData(textures, variantCounts);

    // Scan assets/textures/assorted/ and pass thumbnails to the palette
    {
        namespace fs = std::filesystem;
        fs::path assortedDir = fs::path("assets") / "textures" / "assorted";
        std::vector<fs::path> pngs;
        if (fs::is_directory(assortedDir)) {
            for (const auto& e : fs::directory_iterator(assortedDir))
                if (e.is_regular_file() && e.path().extension() == ".png")
                    pngs.push_back(e.path());
        }
        std::sort(pngs.begin(), pngs.end());

        int count = (int)std::min(pngs.size(), (size_t)EditorPalette::MAX_ASSORTED);
        GLuint  assortedTex[EditorPalette::MAX_ASSORTED]       = {};
        char    assortedNames[EditorPalette::MAX_ASSORTED][64]  = {};
        for (int i = 0; i < count; ++i) {
            assortedTex[i] = loadTexturePNG(pngs[i].string());
            std::string stem = pngs[i].stem().string();
            std::strncpy(assortedNames[i], stem.c_str(), 63);
            assortedNames[i][63] = '\0';
            m_assortedTexIds.push_back(assortedTex[i]);
        }
        m_palette.setAssortedData(assortedTex, assortedNames, count);
    }

    std::cout << "[WorldBuilder] Ready.\n";
    std::cout << "  Tab=cycle tool  1-0=terrain(desert)  G/F/H=Grass/Forest/Highland  O=obj type\n";
    std::cout << "  Ctrl+S=save  Ctrl+L=load  Ctrl+N=new blank map  P=test-play  T=tile builder  ESC=quit\n";
    std::cout << "  F1=alignment editor\n";
    renderHUD();
}

void WorldBuilderState::onExit() {
    if (m_dirty)
        std::cout << "[WorldBuilder] Warning: unsaved changes.\n";
    for (GLuint id : m_assortedTexIds)
        if (id) glDeleteTextures(1, &id);
    m_assortedTexIds.clear();
}

// ── Map operations ────────────────────────────────────────────────────────────

void WorldBuilderState::newMap(int radius) {
    m_map.clear(radius);
    m_map.setName("Untitled Map");
    m_dirty = false;
    std::cout << "[WorldBuilder] New blank map, radius " << radius
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
            tile.variant  = static_cast<uint8_t>(m_palette.selectedVariant());
            tile.rotation = static_cast<uint8_t>(m_paintRotation);
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

    // ── Exit prompt modal ────────────────────────────────────────────────────
    if (m_showExitPrompt) {
        const int sw = Application::get().width();
        const int sh = Application::get().height();
        const float scale = sh / 600.0f;
        const float bw = 130.f * scale, bh = 36.f * scale, gap = 10.f * scale;
        const float totalW = 4 * bw + 3 * gap;
        const float bx0 = (sw - totalW) / 2.f;
        const float by  = sh * 0.55f;
        auto btnX = [&](int i) { return bx0 + i * (bw + gap); };

        if (e->type == SDL_KEYDOWN && !e->key.repeat) {
            switch (e->key.keysym.sym) {
                case SDLK_ESCAPE: m_showExitPrompt = false; return true;
                case SDLK_RETURN:
                case SDLK_SPACE:
                    if      (m_exitPromptHovered == 0) { Application::get().popState(); }
                    else if (m_exitPromptHovered == 1) { openSaveDialog(); m_showExitPrompt = false; }
                    else if (m_exitPromptHovered == 2) { openMapBrowser(); m_showExitPrompt = false; }
                    else                               { m_showExitPrompt = false; }
                    return true;
                case SDLK_1: Application::get().popState(); return true;
                case SDLK_2: openSaveDialog(); m_showExitPrompt = false; return true;
                case SDLK_3: openMapBrowser(); m_showExitPrompt = false; return true;
                case SDLK_4: m_showExitPrompt = false; return true;
                default: break;
            }
        }
        if (e->type == SDL_MOUSEMOTION) {
            float mx = static_cast<float>(e->motion.x);
            float my = static_cast<float>(e->motion.y);
            m_exitPromptHovered = -1;
            for (int i = 0; i < 4; ++i)
                if (mx >= btnX(i) && mx <= btnX(i)+bw && my >= by && my <= by+bh)
                    m_exitPromptHovered = i;
        }
        if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
            float mx = static_cast<float>(e->button.x);
            float my = static_cast<float>(e->button.y);
            for (int i = 0; i < 4; ++i) {
                if (mx >= btnX(i) && mx <= btnX(i)+bw && my >= by && my <= by+bh) {
                    if      (i == 0) { Application::get().popState(); }
                    else if (i == 1) { openSaveDialog(); m_showExitPrompt = false; }
                    else if (i == 2) { openMapBrowser(); m_showExitPrompt = false; }
                    else             { m_showExitPrompt = false; }
                    return true;
                }
            }
        }
        return true;
    }

    // ── Save-name dialog modal ───────────────────────────────────────────────
    if (m_showSaveDialog) {
        if (e->type == SDL_KEYDOWN) {
            switch (e->key.keysym.sym) {
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    commitSaveDialog();
                    return true;
                case SDLK_ESCAPE:
                    m_showSaveDialog = false;
                    SDL_StopTextInput();
                    return true;
                case SDLK_BACKSPACE:
                    if (!m_saveDialogText.empty())
                        m_saveDialogText.pop_back();
                    return true;
                default: break;
            }
        }
        if (e->type == SDL_TEXTINPUT) {
            // Allow only filename-safe characters
            for (char c : std::string(e->text.text)) {
                if (std::isalnum(static_cast<unsigned char>(c)) ||
                    c == '_' || c == '-' || c == ' ')
                    m_saveDialogText += c;
            }
            return true;
        }
        return true;  // swallow everything else
    }

    // ── Map browser modal ────────────────────────────────────────────────────
    if (m_showMapBrowser) {
        const int sw = Application::get().width();
        const int sh = Application::get().height();
        const float scale = sh / 600.0f;
        const float rowH  = 26.f * scale;
        const float listY = sh * 0.22f + 2 * rowH;  // first file row (after header rows)
        const float listX = sw * 0.25f;
        const float listW = sw * 0.50f;

        auto rowIndex = [&](float my) -> int {
            int rel = static_cast<int>((my - listY) / rowH);
            int idx = rel + m_browserScroll;
            // -1 = "Save as default.json" header row, >= 0 = file list
            int rawRel = static_cast<int>((my - (sh * 0.22f)) / rowH);
            if (rawRel == 0) return -2;  // "Save as default" button
            if (rawRel == 1) return -3;  // spacer — ignore
            if (rel < 0 || idx >= static_cast<int>(m_browserFiles.size())) return -1;
            return idx;
        };
        (void)rowIndex;  // used only in mouse handlers below

        if (e->type == SDL_KEYDOWN && !e->key.repeat) {
            switch (e->key.keysym.sym) {
                case SDLK_ESCAPE:
                    m_showMapBrowser = false;
                    return true;
                case SDLK_UP:
                    if (m_browserScroll > 0) --m_browserScroll;
                    return true;
                case SDLK_DOWN:
                    if (m_browserScroll + BROWSER_ROWS < static_cast<int>(m_browserFiles.size()))
                        ++m_browserScroll;
                    return true;
                default: break;
            }
        }
        if (e->type == SDL_MOUSEWHEEL) {
            m_browserScroll = std::max(0, std::min(
                m_browserScroll - e->wheel.y,
                static_cast<int>(m_browserFiles.size()) - BROWSER_ROWS));
            return true;
        }
        if (e->type == SDL_MOUSEMOTION) {
            float mx = static_cast<float>(e->motion.x);
            float my = static_cast<float>(e->motion.y);
            m_browserHovered = -1;
            if (mx >= listX && mx <= listX + listW) {
                // Check "Save as default" button
                if (my >= sh * 0.22f && my < sh * 0.22f + rowH)
                    m_browserHovered = -2;
                else {
                    int rel = static_cast<int>((my - listY) / rowH);
                    int idx = rel + m_browserScroll;
                    if (rel >= 0 && idx < static_cast<int>(m_browserFiles.size()))
                        m_browserHovered = idx;
                }
            }
            return true;
        }
        if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
            float mx = static_cast<float>(e->button.x);
            float my = static_cast<float>(e->button.y);
            if (mx >= listX && mx <= listX + listW) {
                if (my >= sh * 0.22f && my < sh * 0.22f + rowH) {
                    // Save as default.json
                    m_savePath = "data/maps/default.json";
                    saveMap();
                    m_showMapBrowser = false;
                    return true;
                }
                int rel = static_cast<int>((my - listY) / rowH);
                int idx = rel + m_browserScroll;
                if (rel >= 0 && idx < static_cast<int>(m_browserFiles.size())) {
                    m_savePath = m_browserFiles[idx];
                    loadMap();
                    m_showMapBrowser = false;
                    return true;
                }
            }
            // Click outside panel = cancel
            m_showMapBrowser = false;
            return true;
        }
        return true;  // swallow all other input
    }

    if (e->type == SDL_KEYDOWN) {
        SDL_Keycode sym = e->key.keysym.sym;

        // ── Application ─────────────────────────────────────────────────────
        if (sym == SDLK_ESCAPE) { m_showExitPrompt = true; return true; }

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
            if (sym == SDLK_s) {
                if (SDL_GetModState() & KMOD_SHIFT) openSaveDialog();
                else saveMap();
                return true;
            }
            if (sym == SDLK_l) { openMapBrowser(); return true; }
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
        // ── Verdant terrain shortcuts (G/F/H — only when dev tool inactive) ─
        if (!m_devToolActive) {
            if (sym == SDLK_g) { m_paintTerrain = Terrain::Grass;    m_palette.selectTerrain(Terrain::Grass);    renderHUD(); return true; }
            if (sym == SDLK_f) { m_paintTerrain = Terrain::Forest;   m_palette.selectTerrain(Terrain::Forest);   renderHUD(); return true; }
            if (sym == SDLK_h) { m_paintTerrain = Terrain::Highland; m_palette.selectTerrain(Terrain::Highland); renderHUD(); return true; }
            // R — cycle texture rotation 0→1→2→3→4→5→0 (each step = 60°)
            if (sym == SDLK_r) { m_paintRotation = (m_paintRotation + 1) % 6; renderHUD(); return true; }
        }

        // ── Object type / test-play / tile builder ──────────────────────────
        if (sym == SDLK_o && !m_ctrlHeld) { cyclePlaceObjType(+1); return true; }
        if (sym == SDLK_p)                { launchPlay();           return true; }
        if (sym == SDLK_t && !m_devToolActive) {
            Application::get().pushState(std::make_unique<TileBuilderState>());
            return true;
        }
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
        int delta = (e->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) ? -e->wheel.y : e->wheel.y;
        if (m_palette.containsPoint(m_lastMouseX)) {
            m_palette.scroll(-delta * 40);
        } else {
            m_cam.adjustZoom(-delta * Camera2D::ZOOM_STEP);
        }
        return true;
    }

    if (e->type == SDL_MOUSEMOTION) {
        m_lastMouseX = e->motion.x;
        if (m_palDragging) {
            int delta = e->motion.y - m_palDragStartY;
            if (m_palScrollbarDrag) {
                // Scrollbar-thumb drag: pixel delta maps proportionally to scrollY.
                // Positive delta (drag down) = scroll further into list.
                m_palette.setScrollY(m_palDragScrollY +
                                     m_palette.scrollbarPixelToScrollY(delta));
            } else {
                // Content drag: content follows the finger (drag up = scroll down).
                m_palette.setScrollY(m_palDragScrollY - delta);
            }
            return true;
        }
        // Palette gets first look — blocks map hover when cursor is over panel
        if (!m_palette.handleMouseMove(e->motion.x, e->motion.y)) {
            glm::vec2 wp = m_cam.screenToWorld(e->motion.x, e->motion.y);
            HexCoord  hc = HexCoord::fromWorld(wp.x, wp.y, HEX_SIZE);
            m_hasHovered = m_map.hasTile(hc);
            m_hovered    = hc;

            // Drag-to-paint: apply tool while left button held, skipping same hex
            if (m_leftButtonHeld && !m_devToolActive && m_hasHovered) {
                if (!m_hasLastPainted || !(m_hovered == m_lastPaintedHex)) {
                    applyToolAt(m_hovered);
                    m_lastPaintedHex = m_hovered;
                    m_hasLastPainted = true;
                }
            }
        } else {
            m_hasHovered = false;  // hide map cursor while over palette
        }
    }

    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        if (m_palette.containsPoint(e->button.x)) {
            if (e->button.y < EditorPalette::itemListStartY()) {
                // Header / tab area — fire click immediately; not a scrollable region.
                m_palette.handleMouseClick(e->button.x, e->button.y);
                syncFromPalette();
            } else {
                // Scrollable item list — start drag tracking; selection fires on
                // mouse-up only if travel is small (travelY < PALETTE_DRAG_THRESHOLD).
                m_palDragging      = true;
                m_palScrollbarDrag = m_palette.isOnScrollbar(e->button.x, e->button.y);
                m_palDragStartY    = e->button.y;
                m_palDragScrollY   = m_palette.scrollY();
            }
            return true;
        }

        // Map click — recompute from button position and sync hover so the cursor
        // and the painted tile are always the same hex.
        glm::vec2 wp = m_cam.screenToWorld(e->button.x, e->button.y);
        HexCoord  hc = HexCoord::fromWorld(wp.x, wp.y, HEX_SIZE);
        m_hovered    = hc;
        m_hasHovered = m_map.hasTile(hc);
        m_leftButtonHeld = true;
        if (m_devToolActive) {
            m_devSelected    = hc;
            m_hasDevSelected = m_map.hasTile(hc);
            if (m_hasDevSelected)
                m_devEdit = m_map.objectAt(hc) ? DevEdit::Object : DevEdit::Terrain;
        } else {
            applyToolAt(hc);
            m_lastPaintedHex = hc;
            m_hasLastPainted = true;
        }
        return true;
    }

    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT) {
        m_leftButtonHeld = false;
        m_hasLastPainted = false;
        if (m_palDragging) {
            m_palDragging = false;
            int travelY = e->button.y - m_palDragStartY;
            if (travelY < 0) travelY = -travelY;
            if (travelY < PALETTE_DRAG_THRESHOLD) {
                // Small movement → treat as a click; selection and sync happen here
                m_palette.handleMouseClick(e->button.x, e->button.y);
                syncFromPalette();
            }
            return true;
        }
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

        // Current file label in top-right corner
        {
            namespace fs = std::filesystem;
            std::string label = fs::path(m_savePath).stem().string()
                              + (m_dirty ? "*" : "");
            float sc = app.height() / 600.0f;
            float tw = 8.f * 1.2f * sc * label.size();
            m_hud.drawText(app.width() - tw - 8.f * sc, 6.f * sc,
                           1.2f * sc, label.c_str(), {0.8f, 0.75f, 0.4f, 0.9f});
        }

        if (m_showSaveDialog)
            renderSaveDialog(app.width(), app.height());
        else if (m_showExitPrompt)
            renderExitPrompt(app.width(), app.height());
        else if (m_showMapBrowser)
            renderMapBrowser(app.width(), app.height());

        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
    }
}

void WorldBuilderState::renderTerrain() {
    // Pass 1: solid dither base for every tile (no texture).
    for (const auto& [coord, tile] : m_map) {
        RenderOffset off = m_offsets.forTerrain(coord, tile.terrain);
        float h = terrainHeight(tile.terrain) + off.dy;
        m_hexRenderer.drawTile(coord, terrainColor(tile.terrain), HEX_SIZE, h, 0,
                                {off.dx, off.dz});
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthFunc(GL_LEQUAL);

    // Pass 2: soft-edge textured overlay for all tiles.
    for (const auto& [coord, tile] : m_map) {
        GLuint tex = m_hexRenderer.terrainTex(tile.terrain, tile.variant);
        if (!tex) continue;
        RenderOffset off = m_offsets.forTerrain(coord, tile.terrain);
        float h = terrainHeight(tile.terrain) + off.dy;
        m_hexRenderer.drawTile(coord, terrainColor(tile.terrain), HEX_SIZE, h, tex,
                                {off.dx, off.dz}, 0, /*softEdge=*/true, tile.rotation);
    }

    // Pass 3 (grass↔sand edge tiles) is intentionally omitted in the editor.
    // Edge-tile rendering is a game-view effect (AdventureState); showing it here
    // would make grass border tiles appear as a mystery "grass_sand_edge" terrain
    // type with no corresponding palette entry — confusing to the author.

    glDepthFunc(GL_LESS);
    glDisable(GL_BLEND);

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

        m_objRenderers[static_cast<int>(obj.type)].draw(
            {wx, off.dy, wz}, HEX_SIZE * objTypeRenderScale(obj.type), view, proj);
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
    float cursorH = 0.0f;
    if (const MapTile* t = m_map.tileAt(m_hovered)) cursorH = terrainHeight(t->terrain);
    m_hexRenderer.drawOutline(m_hovered, cursorCol, HEX_SIZE * 0.95f, cursorH);
}

void WorldBuilderState::renderHUD() {
    const char* toolNames[] = { "PaintTile", "PlaceObject", "Erase", "Select" };
    std::cout << "  [HUD] tool=" << toolNames[static_cast<int>(m_tool)]
              << "  terrain=" << terrainName(m_paintTerrain)
              << "  rot=" << m_paintRotation << "x60"
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
        case EditorPalette::Category::Assorted:
            m_tool = EditorTool::Select;
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

// ── Save-name dialog ──────────────────────────────────────────────────────────

void WorldBuilderState::openSaveDialog() {
    namespace fs = std::filesystem;
    // Pre-fill with current filename stem (without extension)
    m_saveDialogText = fs::path(m_savePath).stem().string();
    m_showSaveDialog = true;
    SDL_StartTextInput();
}

void WorldBuilderState::commitSaveDialog() {
    std::string name = m_saveDialogText;
    // Trim trailing spaces
    while (!name.empty() && name.back() == ' ') name.pop_back();
    if (name.empty()) name = "untitled";

    namespace fs = std::filesystem;
    fs::create_directories("data/maps");
    m_savePath = "data/maps/" + name + ".json";
    saveMap();
    m_showSaveDialog = false;
    SDL_StopTextInput();
}

void WorldBuilderState::renderSaveDialog(int sw, int sh) {
    m_hud.begin(sw, sh);
    m_hud.drawRect(0, 0, static_cast<float>(sw), static_cast<float>(sh),
                   {0.f, 0.f, 0.f, 0.60f});

    const float scale = sh / 600.0f;
    const float panW  = sw * 0.46f;
    const float panH  = 110.f * scale;
    const float panX  = (sw - panW) / 2.f;
    const float panY  = sh * 0.38f;
    const float brd   = 2.f * scale;

    // Panel
    m_hud.drawRect(panX, panY, panW, panH, {0.07f, 0.05f, 0.02f, 0.97f});
    m_hud.drawRect(panX,        panY,        panW, brd,  {0.7f,0.6f,0.15f,1.f});
    m_hud.drawRect(panX,        panY+panH-brd, panW, brd, {0.7f,0.6f,0.15f,1.f});
    m_hud.drawRect(panX,        panY,        brd, panH, {0.7f,0.6f,0.15f,1.f});
    m_hud.drawRect(panX+panW-brd, panY,      brd, panH, {0.7f,0.6f,0.15f,1.f});

    // Title
    m_hud.drawText(panX + 10.f*scale, panY + 8.f*scale,
                   1.8f*scale, "Save Map As", {0.95f,0.85f,0.40f,1.f});

    // Text field
    const float fieldX = panX + 10.f*scale;
    const float fieldY = panY + 36.f*scale;
    const float fieldW = panW - 20.f*scale;
    const float fieldH = 28.f*scale;

    m_hud.drawRect(fieldX, fieldY, fieldW, fieldH, {0.12f,0.10f,0.03f,1.f});
    m_hud.drawRect(fieldX,          fieldY,          fieldW, brd,    {0.8f,0.7f,0.2f,1.f});
    m_hud.drawRect(fieldX,          fieldY+fieldH-brd, fieldW, brd,  {0.8f,0.7f,0.2f,1.f});
    m_hud.drawRect(fieldX,          fieldY,          brd, fieldH,    {0.8f,0.7f,0.2f,1.f});
    m_hud.drawRect(fieldX+fieldW-brd, fieldY,        brd, fieldH,    {0.8f,0.7f,0.2f,1.f});

    // Typed text + blinking cursor
    std::string display = m_saveDialogText + "_";
    m_hud.drawText(fieldX + 6.f*scale, fieldY + 8.f*scale,
                   1.6f*scale, display.c_str(), {1.f,0.95f,0.75f,1.f});

    // Hint
    m_hud.drawText(panX + 10.f*scale, panY + 74.f*scale,
                   scale, "Enter = save   ESC = cancel   Backspace = delete",
                   {0.55f,0.55f,0.45f,0.85f});
}

// ── Exit prompt ───────────────────────────────────────────────────────────────

void WorldBuilderState::renderExitPrompt(int sw, int sh) {
    m_hud.begin(sw, sh);
    m_hud.drawRect(0, 0, static_cast<float>(sw), static_cast<float>(sh),
                   {0.f, 0.f, 0.f, 0.55f});

    const float scale = sh / 600.0f;
    float bannerH = 38.f * scale, bannerY = sh * 0.44f;
    m_hud.drawRect(0, bannerY, static_cast<float>(sw), bannerH,
                   {0.10f, 0.08f, 0.02f, 0.95f});
    float titleW = 8.f * 2.0f * scale * 16;
    m_hud.drawText((sw - titleW) / 2.f, bannerY + 10.f * scale, 2.0f * scale,
                   "Exit to main menu?", {0.95f, 0.85f, 0.50f, 1.0f});

    static const char* labels[] = { "Exit", "Save", "Load", "Continue" };
    const float bw = 130.f * scale, bh = 36.f * scale, gap = 10.f * scale;
    const float totalW = 4 * bw + 3 * gap;
    const float bx0 = (sw - totalW) / 2.f;
    const float by  = sh * 0.55f;

    for (int i = 0; i < 4; ++i) {
        float bx = bx0 + i * (bw + gap);
        bool  hov = (i == m_exitPromptHovered);

        m_hud.drawRect(bx+2, by+2, bw, bh, {0.f,0.f,0.f,0.5f});
        m_hud.drawRect(bx, by, bw, bh,
                       hov ? glm::vec4{0.75f,0.60f,0.15f,1.f}
                           : glm::vec4{0.30f,0.23f,0.06f,1.f});

        float brd = 2.f * scale;
        glm::vec4 border = hov ? glm::vec4{1.f,0.9f,0.4f,1.f}
                               : glm::vec4{0.55f,0.45f,0.10f,1.f};
        m_hud.drawRect(bx,        by,        bw,  brd,  border);
        m_hud.drawRect(bx,        by+bh-brd, bw,  brd,  border);
        m_hud.drawRect(bx,        by,        brd, bh,   border);
        m_hud.drawRect(bx+bw-brd, by,        brd, bh,   border);

        float ts = 1.6f * scale;
        float tx = bx + (bw - 8.f * ts * strlen(labels[i])) / 2.f;
        float ty = by + (bh - 8.f * ts) / 2.f;
        m_hud.drawText(tx, ty, ts, labels[i], {1.f,0.95f,0.75f,1.f});

        char hint[3] = { static_cast<char>('1' + i), '\0', '\0' };
        m_hud.drawText(bx + 5.f*scale, ty + 1.f*scale,
                       1.1f*scale, hint, {0.8f,0.8f,0.4f,0.85f});
    }

    m_hud.drawText(8.f*scale, static_cast<float>(sh) - 18.f*scale,
                   scale, "1 = Exit   2 = Save   3 = Load   4 / ESC = Continue",
                   {0.6f,0.6f,0.5f,0.8f});
}

// ── Map browser ───────────────────────────────────────────────────────────────

void WorldBuilderState::openMapBrowser() {
    namespace fs = std::filesystem;
    m_browserFiles.clear();
    fs::create_directories("data/maps");
    for (const auto& entry : fs::directory_iterator("data/maps")) {
        if (entry.is_regular_file() && entry.path().extension() == ".json")
            m_browserFiles.push_back(entry.path().string());
    }
    std::sort(m_browserFiles.begin(), m_browserFiles.end());
    m_browserScroll  = 0;
    m_browserHovered = -1;
    m_showMapBrowser = true;
}

void WorldBuilderState::renderMapBrowser(int sw, int sh) {
    namespace fs = std::filesystem;
    const float scale = sh / 600.0f;
    const float rowH  = 26.f * scale;
    const float panW  = sw * 0.50f;
    const float panX  = (sw - panW) / 2.f;
    const float panY  = sh * 0.15f;
    const float listY = panY + 2 * rowH;  // two header rows
    const int   rows  = std::min(BROWSER_ROWS, static_cast<int>(m_browserFiles.size()));
    const float panH  = (rows + 2) * rowH + rowH;  // header + files + hint
    const float brd   = 2.f * scale;

    // Veil
    m_hud.drawRect(0, 0, static_cast<float>(sw), static_cast<float>(sh),
                   {0.f, 0.f, 0.f, 0.55f});

    // Panel background
    m_hud.drawRect(panX, panY, panW, panH, {0.08f, 0.06f, 0.02f, 0.97f});
    m_hud.drawRect(panX, panY, panW, brd,  {0.6f, 0.5f, 0.1f, 1.f});
    m_hud.drawRect(panX, panY+panH-brd, panW, brd, {0.6f, 0.5f, 0.1f, 1.f});
    m_hud.drawRect(panX, panY, brd, panH, {0.6f, 0.5f, 0.1f, 1.f});
    m_hud.drawRect(panX+panW-brd, panY, brd, panH, {0.6f, 0.5f, 0.1f, 1.f});

    // Title row
    m_hud.drawText(panX + 8.f * scale, panY + 6.f * scale,
                   1.8f * scale, "Load Map", {0.95f, 0.85f, 0.40f, 1.f});

    // "Save as default.json" button (row 1)
    bool hovDefault = (m_browserHovered == -2);
    m_hud.drawRect(panX + 4*scale, panY + rowH + 2*scale, panW - 8*scale, rowH - 4*scale,
                   hovDefault ? glm::vec4{0.5f,0.4f,0.1f,1.f}
                              : glm::vec4{0.2f,0.16f,0.04f,1.f});
    m_hud.drawText(panX + 12.f * scale, panY + rowH + 8.f * scale,
                   1.4f * scale, "[ Save current map as default.json ]",
                   {0.9f, 0.8f, 0.3f, 1.f});

    // File rows
    for (int i = 0; i < rows; ++i) {
        int idx = i + m_browserScroll;
        if (idx >= static_cast<int>(m_browserFiles.size())) break;

        float ry = listY + i * rowH;
        bool  hov = (idx == m_browserHovered);
        bool  cur = (m_browserFiles[idx] == m_savePath);

        m_hud.drawRect(panX + 4*scale, ry + 2*scale, panW - 8*scale, rowH - 4*scale,
                       hov ? glm::vec4{0.40f,0.32f,0.08f,1.f}
                           : glm::vec4{0.13f,0.10f,0.03f,1.f});

        std::string stem = fs::path(m_browserFiles[idx]).stem().string();
        if (cur) stem = "> " + stem;
        m_hud.drawText(panX + 12.f * scale, ry + 7.f * scale,
                       1.4f * scale, stem.c_str(),
                       cur ? glm::vec4{1.f,0.9f,0.4f,1.f}
                           : glm::vec4{0.85f,0.80f,0.65f,1.f});
    }

    // Hint
    float hintY = listY + rows * rowH + 4.f * scale;
    m_hud.drawText(panX + 8.f * scale, hintY,
                   scale, "Click to load   |   ESC = cancel   |   scroll = browse",
                   {0.55f, 0.55f, 0.45f, 0.85f});
}
