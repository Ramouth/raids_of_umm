#include "AdventureState.h"
#include "core/Application.h"
#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <iostream>
#include <cmath>
#include <algorithm>

// ── Map generation ────────────────────────────────────────────────────────────

static float pseudoNoise(int q, int r) {
    unsigned int h = static_cast<unsigned int>(q * 374761393 + r * 668265263);
    h = (h ^ (h >> 13)) * 1274126177u;
    return static_cast<float>(h & 0xFFFF) / 65535.0f;
}

void AdventureState::buildMap() {
    for (int q = -MAP_RADIUS; q <= MAP_RADIUS; ++q) {
        int r1 = std::max(-MAP_RADIUS, -q - MAP_RADIUS);
        int r2 = std::min( MAP_RADIUS, -q + MAP_RADIUS);
        for (int r = r1; r <= r2; ++r) {
            HexCoord coord{q, r};
            float n = pseudoNoise(q, r);
            float dist = static_cast<float>(coord.length()) / MAP_RADIUS;

            MapTile tile;
            if      (n < 0.04f)                  { tile.terrain = Terrain::Oasis;    tile.passable = true;  }
            else if (n < 0.14f)                  { tile.terrain = Terrain::Rock;     tile.passable = true;  }
            else if (n < 0.20f)                  { tile.terrain = Terrain::Ruins;    tile.passable = true;  }
            else if (n < 0.24f)                  { tile.terrain = Terrain::Obsidian; tile.passable = false; }
            else if (n < 0.44f && dist > 0.4f)  { tile.terrain = Terrain::Dune;     tile.passable = true;  }
            else                                 { tile.terrain = Terrain::Sand;     tile.passable = true;  }
            m_grid.set(coord, tile);
        }
    }
    std::cout << "[Adventure] Map built — " << m_grid.cellCount() << " tiles\n";
}

void AdventureState::placeObjects() {
    // Fixed interesting positions across the map
    m_objects = {
        { {  0,  4 }, ObjType::Town,        "Umm'Natur"         },
        { {  5, -3 }, ObjType::Dungeon,     "Tomb of Kha'Set"   },
        { { -4,  6 }, ObjType::Dungeon,     "Buried Sanctum"    },
        { {  3,  3 }, ObjType::GoldMine,    "Gold Vein"         },
        { { -6, -2 }, ObjType::CrystalMine, "Crystal Seam"      },
        { {  7, -6 }, ObjType::Artifact,    "Scarab of Eternity"},
        { { -8,  4 }, ObjType::Artifact,    "Pharaoh's Eye"     },
        { {  1, -7 }, ObjType::GoldMine,    "Sunken Treasury"   },
    };

    // Ensure object tiles are passable sand (so the hero can reach them)
    for (auto& obj : m_objects) {
        if (auto* t = m_grid.get(obj.pos)) {
            t->passable = true;
            if (t->terrain == Terrain::Obsidian)
                t->terrain = Terrain::Ruins;
        }
    }

    // Place hero on a passable tile near centre
    m_hero.pos = { 0, 0 };
    if (auto* t = m_grid.get(m_hero.pos)) t->passable = true;
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void AdventureState::onEnter() {
    std::cout << "[Adventure] Day " << m_day << " — Raids of Umm'Natur\n";
    std::cout << "  Left-click hero to select, then click a tile to move.\n";
    std::cout << "  SPACE = end turn  |  WASD/arrows = pan  |  scroll = zoom  |  ESC = quit\n";

    auto& app = Application::get();
    m_vpW = app.width();
    m_vpH = app.height();

    m_hexRenderer.init();
    m_spriteRenderer.init();
    m_spriteRenderer.loadSprite("assets/textures/units/rider_lance.png");

    buildMap();
    placeObjects();

    // Start camera centred on hero
    float hx, hz;
    m_hero.pos.toWorld(m_hexSize, hx, hz);
    m_camPos = { hx, hz };

    // Initialise animated position to match the hero's starting tile
    m_heroRenderPos = glm::vec3(hx, 0.0f, hz);
    m_heroTargetPos = m_heroRenderPos;
}

void AdventureState::onExit() {
    std::cout << "[Adventure] Exited\n";
}

// ── Camera & picking ──────────────────────────────────────────────────────────

glm::mat4 AdventureState::viewMatrix() const {
    float eyeX = m_camPos.x;
    float eyeZ = m_camPos.y + m_zoom * 0.5f;
    float eyeY = m_zoom * 1.8f;
    return glm::lookAt(
        glm::vec3(eyeX, eyeY, eyeZ),
        glm::vec3(m_camPos.x, 0.0f, m_camPos.y),
        glm::vec3(0.0f, 0.0f, -1.0f)
    );
}

glm::mat4 AdventureState::projMatrix() const {
    float aspect = static_cast<float>(m_vpW) / static_cast<float>(m_vpH);
    float hw = m_zoom * aspect;
    float hh = m_zoom;
    return glm::ortho(-hw, hw, -hh, hh, 0.1f, 200.0f);
}

glm::mat4 AdventureState::viewProjMatrix() const {
    return projMatrix() * viewMatrix();
}

glm::vec2 AdventureState::screenToWorld(int px, int py) const {
    float ndcX =  (static_cast<float>(px) / m_vpW) * 2.0f - 1.0f;
    float ndcY = -(static_cast<float>(py) / m_vpH) * 2.0f + 1.0f;

    glm::mat4 invVP = glm::inverse(viewProjMatrix());
    glm::vec4 near4 = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 far4  = invVP * glm::vec4(ndcX, ndcY,  1.0f, 1.0f);
    glm::vec3 rayO  = glm::vec3(near4) / near4.w;
    glm::vec3 rayD  = glm::normalize(glm::vec3(far4) / far4.w - rayO);

    // Intersect ray with Y=0 plane
    if (std::abs(rayD.y) < 1e-6f) return { rayO.x, rayO.z };
    float t = -rayO.y / rayD.y;
    glm::vec3 hit = rayO + rayD * t;
    return { hit.x, hit.z };
}

// ── Game logic ────────────────────────────────────────────────────────────────

void AdventureState::moveHero(const HexCoord& dest) {
    if (dest == m_hero.pos) return;
    if (!m_grid.has(dest)) return;

    auto path = m_grid.findPath(
        m_hero.pos, dest,
        [](const HexCoord&, const MapTile& t) { return t.passable; }
    );

    if (path.size() < 2) {
        std::cout << "  [!] No path to that tile.\n";
        return;
    }

    int steps = static_cast<int>(path.size()) - 1;
    if (steps > m_hero.movesLeft) {
        std::cout << "  [!] Not enough movement (" << m_hero.movesLeft
                  << " left, need " << steps << ").\n";
        return;
    }

    m_hero.pos       = dest;
    m_hero.movesLeft -= steps;

    // Set animated target — camera will follow the sprite in update()
    float hx, hz;
    dest.toWorld(m_hexSize, hx, hz);
    m_heroTargetPos = glm::vec3(hx, 0.0f, hz);

    std::cout << "  Hero moved to (" << dest.q << "," << dest.r
              << ")  — " << m_hero.movesLeft << " moves remaining\n";

    onHeroVisit(dest);
}

void AdventureState::onHeroVisit(const HexCoord& coord) {
    for (auto& obj : m_objects) {
        if (obj.pos == coord && !obj.visited) {
            obj.visited = true;
            std::cout << "  *** Visited " << obj.typeName() << ": " << obj.name << " ***\n";
            switch (obj.type) {
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
            }
        }
    }
}

void AdventureState::endTurn() {
    ++m_day;
    m_hero.movesLeft = m_hero.movesMax;
    m_heroSelected   = false;
    m_previewPath.clear();
    std::cout << "[Adventure] Day " << m_day << " begins. Moves restored.\n";
}

// ── Input ─────────────────────────────────────────────────────────────────────

bool AdventureState::handleEvent(void* sdlEvent) {
    SDL_Event* e = static_cast<SDL_Event*>(sdlEvent);

    // ── Keyboard ──
    if (e->type == SDL_KEYDOWN) {
        switch (e->key.keysym.sym) {
            case SDLK_ESCAPE: Application::get().quit(); return true;
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

    // ── Mouse wheel — zoom ──
    if (e->type == SDL_MOUSEWHEEL) {
        m_zoom -= e->wheel.y * ZOOM_STEP;
        m_zoom  = std::max(3.0f, std::min(m_zoom, 40.0f));
        return true;
    }

    // ── Mouse motion — hover + path preview ──
    if (e->type == SDL_MOUSEMOTION) {
        glm::vec2 wp = screenToWorld(e->motion.x, e->motion.y);
        HexCoord hc = HexCoord::fromWorld(wp.x, wp.y, m_hexSize);
        m_hasHovered = m_grid.has(hc);
        m_hovered    = hc;

        // Update path preview when hero is selected
        m_previewPath.clear();
        if (m_heroSelected && m_hasHovered && hc != m_hero.pos) {
            m_previewPath = m_grid.findPath(
                m_hero.pos, hc,
                [](const HexCoord&, const MapTile& t) { return t.passable; }
            );
        }
    }

    // ── Left click — select / move ──
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        glm::vec2 wp = screenToWorld(e->button.x, e->button.y);
        HexCoord clicked = HexCoord::fromWorld(wp.x, wp.y, m_hexSize);

        // Hero selection: use world-space radius so clicking anywhere on the
        // billboard sprite (not just its ground-level base hex) works.
        float heroWx, heroWz;
        m_hero.pos.toWorld(m_hexSize, heroWx, heroWz);
        float dx = wp.x - heroWx;
        float dz = wp.y - heroWz;
        bool clickedHero = (dx*dx + dz*dz) < (m_hexSize * m_hexSize * 1.5f);

        if (clickedHero) {
            // Click on hero toggles selection
            m_heroSelected = !m_heroSelected;
            m_previewPath.clear();
            std::cout << "  Hero " << (m_heroSelected ? "selected" : "deselected") << "\n";
        } else if (m_heroSelected && m_grid.has(clicked)) {
            moveHero(clicked);
            m_heroSelected = false;
            m_previewPath.clear();
        } else {
            m_heroSelected = false;
            m_previewPath.clear();
        }
        return true;
    }

    // ── Window resize ──
    if (e->type == SDL_WINDOWEVENT &&
        e->window.event == SDL_WINDOWEVENT_RESIZED) {
        m_vpW = e->window.data1;
        m_vpH = e->window.data2;
    }

    return false;
}

// ── Update ────────────────────────────────────────────────────────────────────

void AdventureState::update(float dt) {
    float speed = CAM_SPEED * m_zoom / 12.0f;
    if (m_keyW) m_camPos.y -= speed * dt;
    if (m_keyS) m_camPos.y += speed * dt;
    if (m_keyA) m_camPos.x -= speed * dt;
    if (m_keyD) m_camPos.x += speed * dt;

    // Smoothly animate the hero sprite toward its target position,
    // keeping the camera locked on the sprite so the walk is always visible.
    glm::vec3 delta = m_heroTargetPos - m_heroRenderPos;
    float dist = glm::length(delta);
    if (dist > 0.001f) {
        float step = HERO_MOVE_SPEED * dt;
        if (step >= dist)
            m_heroRenderPos = m_heroTargetPos;
        else
            m_heroRenderPos += (delta / dist) * step;
        m_camPos = { m_heroRenderPos.x, m_heroRenderPos.z };
    }
}

// ── Render ────────────────────────────────────────────────────────────────────

void AdventureState::render() {
    glm::mat4 view = viewMatrix();
    glm::mat4 proj = projMatrix();
    glm::mat4 vp   = proj * view;

    // worldHexSize tells the renderer what tile spacing to use for toWorld() positioning.
    // All visual scales below are relative to this, so tokens land on the correct tiles.
    m_hexRenderer.beginFrame(vp, m_hexSize);

    // Terrain tiles — visual scale == world scale (tiles fill the grid exactly)
    for (auto& [coord, tile] : m_grid) {
        glm::vec3 color = terrainColor(tile.terrain);
        float h = 0.0f;
        if (tile.terrain == Terrain::Dune) h = 0.05f;
        if (tile.terrain == Terrain::Rock) h = 0.12f;
        m_hexRenderer.drawTile(coord, color, m_hexSize, h);
    }

    // Path preview outlines — drawn on the ground, colour shows reachability
    for (size_t i = 1; i < m_previewPath.size(); ++i) {
        bool reachable = (static_cast<int>(i) <= m_hero.movesLeft);
        glm::vec3 pathCol = reachable
            ? glm::vec3(0.3f, 0.7f, 1.0f)   // blue  = reachable
            : glm::vec3(0.9f, 0.2f, 0.2f);   // red   = out of range
        m_hexRenderer.drawOutline(m_previewPath[i], pathCol, m_hexSize * 0.95f);
    }

    // Hover outline (yellow)
    if (m_hasHovered)
        m_hexRenderer.drawOutline(m_hovered, { 1.0f, 0.9f, 0.3f }, m_hexSize * 0.95f);

    // Map object tokens — smaller hex, elevated above terrain
    // visualScale < worldHexSize so they look like standing tokens, not floor tiles
    for (auto& obj : m_objects) {
        glm::vec3 col = obj.color();
        if (obj.visited) col *= 0.55f;
        m_hexRenderer.drawTile(obj.pos, col, m_hexSize * 0.55f, 0.25f);
    }

    // Selection and movement rings drawn at the hero's current animated hex
    HexCoord spriteHex = HexCoord::fromWorld(m_heroRenderPos.x, m_heroRenderPos.z, m_hexSize);

    if (m_heroSelected)
        m_hexRenderer.drawOutline(spriteHex, { 1.0f, 1.0f, 1.0f }, m_hexSize * 0.65f);

    glm::vec3 moveCol = m_hero.movesLeft > 0
        ? glm::vec3(0.2f, 1.0f, 0.3f)
        : glm::vec3(0.45f, 0.45f, 0.45f);
    m_hexRenderer.drawOutline(spriteHex, moveCol, m_hexSize * 0.65f);

    m_hexRenderer.endFrame();

    // Draw hero sprite as a billboard after hex pass (requires alpha blending)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    m_spriteRenderer.draw(m_heroRenderPos, m_hexSize, view, proj);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}
