#include "AdventureState.h"
#include "core/Application.h"
#include "render/TileVisuals.h"
#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <iostream>
#include <algorithm>
#include <cmath>

// ── Construction ──────────────────────────────────────────────────────────────

AdventureState::AdventureState(WorldMap map)
    : m_map(std::move(map)), m_externalMap(true) {}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void AdventureState::onEnter() {
    auto& app = Application::get();
    m_cam = Camera2D(app.width(), app.height());

    m_hexRenderer.init();
    m_spriteRenderer.init();
    m_spriteRenderer.loadSprite("assets/textures/units/rider_lance.png");

    if (!m_externalMap)
        initMap();

    // Place hero at origin; ensure it's passable.
    m_hero.pos = {0, 0};
    if (MapTile* t = m_map.tileAt(m_hero.pos))
        t->passable = true;

    // Centre camera on hero.
    float hx, hz;
    m_hero.pos.toWorld(HEX_SIZE, hx, hz);
    m_cam.setPosition({hx, hz});
    m_heroRenderPos = { hx, 0.0f, hz };
    m_heroTargetPos = m_heroRenderPos;

    std::cout << "[Adventure] Day " << m_day << " — " << m_map.name() << "\n";
    std::cout << "  Click hero to select, then click a tile to move.\n";
    std::cout << "  SPACE = end turn  |  WASD = pan  |  scroll = zoom  |  ESC = back to editor\n";
}

void AdventureState::onExit() {
    std::cout << "[Adventure] Exited\n";
}

void AdventureState::initMap() {
    m_map.generateProcedural(12, /*seed=*/0);
}

// ── Game logic ────────────────────────────────────────────────────────────────

void AdventureState::moveHero(const HexCoord& dest) {
    if (dest == m_hero.pos)      return;
    if (!m_map.hasTile(dest))    return;

    auto path = m_map.findPath(m_hero.pos, dest);

    if (path.size() < 2) {
        std::cout << "  [!] No path to that tile.\n";
        return;
    }

    int steps = static_cast<int>(path.size()) - 1;
    if (!m_hero.canReach(steps)) {
        std::cout << "  [!] Not enough movement ("
                  << m_hero.movesLeft << " left, need " << steps << ").\n";
        return;
    }

    m_hero.pos       = dest;
    m_hero.movesLeft -= steps;

    // Queue every waypoint so the render position follows the path hex by hex.
    m_moveQueue    = path;
    m_moveQueueIdx = 1;
    float hx, hz;
    m_moveQueue[1].toWorld(HEX_SIZE, hx, hz);
    m_heroTargetPos = { hx, 0.0f, hz };

    std::cout << "  Hero moved to (" << dest.q << "," << dest.r
              << ")  — " << m_hero.movesLeft << " moves remaining\n";

    onHeroVisit(dest);
}

void AdventureState::onHeroVisit(const HexCoord& coord) {
    const MapObjectDef* obj = m_map.objectAt(coord);
    if (!obj) return;

    bool alreadyVisited = m_visitedObjects.count(coord) > 0;
    if (alreadyVisited) return;

    m_visitedObjects.insert(coord);
    std::cout << "  *** Visited " << obj->typeName() << ": " << obj->name << " ***\n";

    switch (obj->type) {
        case ObjType::Town:
            std::cout << "     (Town screen coming soon)\n";
            break;
        case ObjType::Dungeon:
            std::cout << "     (Combat screen coming soon)\n";
            break;
        case ObjType::GoldMine:
            std::cout << "     +500 Gold per turn secured!\n";
            break;
        case ObjType::CrystalMine:
            std::cout << "     +2 Sand Crystal per turn secured!\n";
            break;
        case ObjType::Artifact:
            std::cout << "     Artifact added to hero's backpack!\n";
            break;
        case ObjType::COUNT:
            break;  // sentinel, never a real object
    }
}

void AdventureState::endTurn() {
    ++m_day;
    m_hero.resetMoves();
    m_heroSelected = false;
    m_previewPath.clear();
    std::cout << "[Adventure] Day " << m_day << " begins. Moves restored.\n";
}

// ── Input ─────────────────────────────────────────────────────────────────────

bool AdventureState::handleEvent(void* sdlEvent) {
    SDL_Event* e = static_cast<SDL_Event*>(sdlEvent);

    if (e->type == SDL_KEYDOWN) {
        switch (e->key.keysym.sym) {
            case SDLK_ESCAPE: Application::get().popState(); return true;
            case SDLK_SPACE:  endTurn(); return true;
            case SDLK_w: case SDLK_UP:    m_keyW = true; return true;
            case SDLK_s: case SDLK_DOWN:  m_keyS = true; return true;
            case SDLK_a: case SDLK_LEFT:  m_keyA = true; return true;
            case SDLK_d: case SDLK_RIGHT: m_keyD = true; return true;
        }
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
        glm::vec2 wp  = m_cam.screenToWorld(e->motion.x, e->motion.y);
        HexCoord  hc  = HexCoord::fromWorld(wp.x, wp.y, HEX_SIZE);
        m_hasHovered  = m_map.hasTile(hc);
        m_hovered     = hc;

        m_previewPath.clear();
        if (m_heroSelected && m_hasHovered && hc != m_hero.pos)
            m_previewPath = m_map.findPath(m_hero.pos, hc);
    }

    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        glm::vec2 wp      = m_cam.screenToWorld(e->button.x, e->button.y);
        HexCoord  clicked = HexCoord::fromWorld(wp.x, wp.y, HEX_SIZE);

        float heroWx, heroWz;
        m_hero.pos.toWorld(HEX_SIZE, heroWx, heroWz);
        float dx = wp.x - heroWx;
        float dz = wp.y - heroWz;
        bool clickedHero = (dx*dx + dz*dz) < (HEX_SIZE * HEX_SIZE * 1.5f);

        if (clickedHero) {
            m_heroSelected = !m_heroSelected;
            m_previewPath.clear();
            std::cout << "  Hero " << (m_heroSelected ? "selected" : "deselected") << "\n";
        } else if (m_heroSelected && m_map.hasTile(clicked)) {
            moveHero(clicked);
            m_heroSelected = false;
            m_previewPath.clear();
        } else {
            m_heroSelected = false;
            m_previewPath.clear();
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

void AdventureState::update(float dt) {
    float speed = CAM_SPEED * m_cam.zoom() / 12.0f;
    float dx = 0.0f, dz = 0.0f;
    if (m_keyW) dz -= speed * dt;
    if (m_keyS) dz += speed * dt;
    if (m_keyA) dx -= speed * dt;
    if (m_keyD) dx += speed * dt;
    if (dx != 0.0f || dz != 0.0f)
        m_cam.pan(dx, dz);

    // Smooth hero animation — step through each hex in the path, not straight-line.
    glm::vec3 delta = m_heroTargetPos - m_heroRenderPos;
    float dist = glm::length(delta);
    if (dist > 0.001f) {
        float step = HERO_MOVE_SPEED * dt;
        m_heroRenderPos += (step >= dist) ? delta : (delta / dist) * step;

        // Camera lags behind the moving rider — rider visibly leads the
        // camera so the map scrolls behind it rather than under a static sprite.
        glm::vec2 heroXZ = { m_heroRenderPos.x, m_heroRenderPos.z };
        glm::vec2 camPos = m_cam.position();
        float lerpFactor = 1.0f - std::exp(-CAM_FOLLOW_SPEED * dt);
        m_cam.setPosition(camPos + (heroXZ - camPos) * lerpFactor);
    } else if (!m_moveQueue.empty() &&
               m_moveQueueIdx + 1 < static_cast<int>(m_moveQueue.size())) {
        // Arrived at waypoint — snap render pos and advance to next hex.
        m_heroRenderPos = m_heroTargetPos;
        ++m_moveQueueIdx;
        float hx, hz;
        m_moveQueue[m_moveQueueIdx].toWorld(HEX_SIZE, hx, hz);
        m_heroTargetPos = { hx, 0.0f, hz };
    }
}

// ── Render ────────────────────────────────────────────────────────────────────

void AdventureState::render() {
    glm::mat4 view = m_cam.viewMatrix();
    glm::mat4 proj = m_cam.projMatrix();
    glm::mat4 vp   = m_cam.viewProjMatrix();

    m_hexRenderer.beginFrame(vp, HEX_SIZE);
    renderTerrain();
    renderPathPreview();
    renderObjects();
    m_hexRenderer.endFrame();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    renderHero();
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    (void)view; (void)proj; // used via camera
}

void AdventureState::renderTerrain() {
    for (const auto& [coord, tile] : m_map) {
        glm::vec3 color = terrainColor(tile.terrain);
        float h = terrainHeight(tile.terrain);
        m_hexRenderer.drawTile(coord, color, HEX_SIZE, h);
    }

    // Hover outline (yellow).
    if (m_hasHovered)
        m_hexRenderer.drawOutline(m_hovered, {1.0f, 0.9f, 0.3f}, HEX_SIZE * 0.95f);
}

void AdventureState::renderPathPreview() {
    for (size_t i = 1; i < m_previewPath.size(); ++i) {
        bool reachable = (static_cast<int>(i) <= m_hero.movesLeft);
        glm::vec3 col  = reachable
            ? glm::vec3{0.3f, 0.7f, 1.0f}
            : glm::vec3{0.9f, 0.2f, 0.2f};
        m_hexRenderer.drawOutline(m_previewPath[i], col, HEX_SIZE * 0.95f);
    }
}

void AdventureState::renderObjects() {
    for (const auto& obj : m_map.objects()) {
        glm::vec3 col = objTypeColor(obj.type);
        bool visited  = m_visitedObjects.count(obj.pos) > 0;
        if (visited) col *= 0.55f;
        m_hexRenderer.drawTile(obj.pos, col, HEX_SIZE * 0.55f, 0.25f);
    }
}

void AdventureState::renderHero() {
    HexCoord spriteHex = HexCoord::fromWorld(
        m_heroRenderPos.x, m_heroRenderPos.z, HEX_SIZE);

    if (m_heroSelected)
        m_hexRenderer.drawOutline(spriteHex, {1.0f, 1.0f, 1.0f}, HEX_SIZE * 0.65f);

    glm::vec3 moveCol = m_hero.hasMoves()
        ? glm::vec3{0.2f, 1.0f, 0.3f}
        : glm::vec3{0.45f, 0.45f, 0.45f};
    m_hexRenderer.drawOutline(spriteHex, moveCol, HEX_SIZE * 0.65f);

    m_spriteRenderer.draw(m_heroRenderPos, HEX_SIZE,
                          m_cam.viewMatrix(), m_cam.projMatrix());
}
