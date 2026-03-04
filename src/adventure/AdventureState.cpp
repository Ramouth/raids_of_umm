#include "AdventureState.h"
#include "castle/CastleState.h"
#include "combat/CombatState.h"
#include "combat/CombatUnit.h"
#include "core/Application.h"
#include "render/TileVisuals.h"
#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <memory>

// ── Army builders ─────────────────────────────────────────────────────────────

CombatArmy AdventureState::buildPlayerArmy() const {
    CombatArmy army;
    army.ownerName = m_hero.name;
    army.isPlayer  = true;

    for (const auto& slot : m_hero.army) {
        if (!slot.isEmpty())
            army.stacks.push_back(CombatUnit::make(slot.unitType, slot.count, true));
    }

    // Fallback: if hero has no army (shouldn't happen after initHeroArmy), fight bare-handed.
    if (army.stacks.empty()) {
        static const UnitType s_heroAlone = []() {
            UnitType t;
            t.id = "hero_alone"; t.name = "Hero";
            t.hitPoints = 20; t.attack = 5; t.defense = 4;
            t.minDamage = 1;  t.maxDamage = 4; t.speed = 5; t.moveRange = 3;
            return t;
        }();
        army.stacks.push_back(CombatUnit::make(&s_heroAlone, 1, true));
    }
    return army;
}

CombatArmy AdventureState::buildEnemyArmy(const MapObjectDef& obj) const {
    CombatArmy army;
    army.ownerName = obj.name;
    army.isPlayer  = false;

    auto& rm = Application::get().resources();
    if (rm.loaded()) {
        if (const UnitType* t = rm.unit("skeleton_warrior"))
            army.stacks.push_back(CombatUnit::make(t, 12, false));
        if (const UnitType* t = rm.unit("sand_scorpion"))
            army.stacks.push_back(CombatUnit::make(t,  4, false));
    }

    // Fallback if data didn't load.
    if (army.stacks.empty()) {
        static const UnitType s_guard = []() {
            UnitType t;
            t.id = "guard"; t.name = "Dungeon Guard";
            t.hitPoints = 8; t.attack = 5; t.defense = 4;
            t.minDamage = 1; t.maxDamage = 3; t.speed = 4; t.moveRange = 3;
            return t;
        }();
        army.stacks.push_back(CombatUnit::make(&s_guard, 8, false));
    }
    return army;
}

void AdventureState::initHeroArmy() {
    if (m_hero.armySize() > 0) return;  // already initialised (re-enter after combat)

    auto& rm = Application::get().resources();
    if (!rm.loaded()) return;

    if (const UnitType* t = rm.unit("desert_archer"))
        m_hero.addUnit(t, 10);
    if (const UnitType* t = rm.unit("mummy"))
        m_hero.addUnit(t, 3);

    std::cout << "[Adventure] Hero army: " << m_hero.armySize() << " stacks.\n";
}

// ── Construction ──────────────────────────────────────────────────────────────

AdventureState::AdventureState(WorldMap map)
    : m_map(std::move(map)), m_externalMap(true) {}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void AdventureState::onEnter() {
    auto& app = Application::get();
    m_cam = Camera2D(app.width(), app.height());

    m_hexRenderer.init();
    m_hexRenderer.loadTerrainTextures();
    m_spriteRenderer.init();
    m_spriteRenderer.loadSprite("assets/textures/units/armoured_warrior.png");
    m_enemySpriteRenderer.init();
    m_enemySpriteRenderer.loadSprite("assets/textures/units/enemy_scout.png");
    m_buildingSpriteRenderer.init();
    m_buildingSpriteRenderer.loadSprite("assets/textures/objects/castle.png");
    m_hud.init();

    if (!m_externalMap)
        initMap();

    initHeroArmy();

    // Load render offsets (missing file = fresh start, not an error)
    if (auto err = m_offsets.load("assets/render_offsets.json"))
        std::cerr << "[Adventure] Offsets load warning: " << *err << "\n";

    // Place hero at origin; ensure it's passable.
    m_hero.pos = {0, 0};
    if (MapTile* t = m_map.tileAt(m_hero.pos))
        t->passable = true;

    // Always place a test enemy scout 2 hexes east so combat can be tested immediately.
    HexCoord scoutPos{2, 0};
    if (MapTile* t = m_map.tileAt(scoutPos))
        t->passable = true;
    m_map.placeObject({ scoutPos, ObjType::Dungeon, "Enemy Scout" });

    // Centre camera on hero.
    float hx, hz;
    m_hero.pos.toWorld(HEX_SIZE, hx, hz);
    m_cam.setPosition({hx, hz});
    m_heroRenderPos = { hx, 0.0f, hz };
    m_heroTargetPos = m_heroRenderPos;

    std::cout << "[Adventure] Day " << m_day << " — " << m_map.name() << "\n";
    std::cout << "  Click hero to select, then click a tile to move.\n";
    std::cout << "  SPACE = end turn  |  R = restart  |  WASD = pan  |  scroll = zoom  |  ESC = back to editor\n";
}

void AdventureState::onExit() {
    std::cout << "[Adventure] Exited\n";
}

void AdventureState::initMap() {
    m_map.generateProcedural(12, /*seed=*/0);
}

// ── Game logic ────────────────────────────────────────────────────────────────

namespace {

float angleFromDirection(const glm::vec3& dir) {
    if (glm::length(dir) < 0.001f) return 0.0f;
    return std::atan2(dir.x, -dir.z);
}

float lerpAngle(float from, float to, float t) {
    float diff = to - from;
    constexpr float PI = 3.14159265358979f;
    while (diff > PI) diff -= 2.0f * PI;
    while (diff < -PI) diff += 2.0f * PI;
    return from + diff * t;
}

} // namespace

void AdventureState::moveHero(const HexCoord& dest) {
    if (dest == m_hero.pos)      return;
    if (!m_map.hasTile(dest))    return;

    auto path = m_map.findPath(m_hero.pos, dest);

    if (path.size() < 2) {
        std::cout << "  [!] No path to that tile.\n";
        return;
    }

    int steps = static_cast<int>(path.size()) - 1;
    if (!m_infiniteMoves && !m_hero.canReach(steps)) {
        std::cout << "  [!] Not enough movement ("
                  << m_hero.movesLeft << " left, need " << steps << ").\n";
        return;
    }

    m_hero.pos       = dest;
    if (!m_infiniteMoves) {
        m_hero.movesLeft -= steps;
    }

    // Queue every waypoint so the render position follows the path hex by hex.
    m_moveQueue    = path;
    m_moveQueueIdx = 1;
    float hx, hz;
    m_moveQueue[1].toWorld(HEX_SIZE, hx, hz);
    m_heroTargetPos = { hx, 0.0f, hz };
    
    // Initialize facing direction toward first waypoint
    glm::vec3 dir = m_heroTargetPos - m_heroRenderPos;
    m_heroFacingAngle = angleFromDirection(dir);
    m_walkCycle = 0.0f;
    m_isWalking = true;

    // Set pending visit to trigger when movement finishes
    m_pendingVisit = dest;
    m_hasPendingVisit = true;

    std::cout << "  [Day " << m_day << "] Hero moved to (" << dest.q << "," << dest.r
              << ") — Moves: " << m_hero.movesLeft << "/" << m_hero.movesMax 
              << " | Visited: " << m_visitedObjects.size() << "\n";
}

void AdventureState::onHeroVisit(const HexCoord& coord) {
    const MapObjectDef* obj = m_map.objectAt(coord);
    if (!obj) return;

    bool alreadyVisited = m_visitedObjects.count(coord) > 0;

    // Towns are always re-enterable (recruit, build, etc.).
    // One-time objects (dungeons, mines, artifacts) block re-entry once visited.
    if (alreadyVisited && obj->type != ObjType::Town) return;

    if (obj->type != ObjType::Town)
        m_visitedObjects.insert(coord);
    std::cout << "  *** Visited " << obj->typeName() << ": " << obj->name << " ***\n";

    switch (obj->type) {
        case ObjType::Town:
            Application::get().pushState(std::make_unique<CastleState>(obj->name));
            break;
        case ObjType::Dungeon:
            std::cout << "     Entering combat!\n";
            Application::get().pushState(
                std::make_unique<CombatState>(buildPlayerArmy(), buildEnemyArmy(*obj)));
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

void AdventureState::wait() {
    m_hero.resetMoves();
    m_heroSelected = false;
    m_previewPath.clear();
    std::cout << "[Adventure] Waited. Moves restored.\n";
}

// ── Input ─────────────────────────────────────────────────────────────────────

bool AdventureState::handleEvent(void* sdlEvent) {
    SDL_Event* e = static_cast<SDL_Event*>(sdlEvent);

    if (e->type == SDL_KEYDOWN) {
        switch (e->key.keysym.sym) {
            case SDLK_ESCAPE: Application::get().popState(); return true;
            case SDLK_SPACE:  endTurn(); return true;
            case SDLK_h:      m_showHUD = !m_showHUD; return true;
            case SDLK_w:      wait(); return true;
            case SDLK_p: {
                float now = SDL_GetTicks() / 1000.0f;
                if (now - m_lastPTime < 0.5f) {
                    m_infiniteMoves = !m_infiniteMoves;
                    std::cout << "  [DEBUG] Infinite moves: " << (m_infiniteMoves ? "ON" : "OFF") << "\n";
                }
                m_lastPTime = now;
                return true;
            }
            case SDLK_r:
                Application::get().replaceState(std::make_unique<AdventureState>(std::move(m_map)));
                return true;
            case SDLK_UP:    m_keyW = true; return true;
            case SDLK_DOWN:  m_keyS = true; return true;
            case SDLK_LEFT:  m_keyA = true; return true;
            case SDLK_RIGHT: m_keyD = true; return true;
        }
    }
    if (e->type == SDL_KEYUP) {
        switch (e->key.keysym.sym) {
            case SDLK_UP:    m_keyW = false; return true;
            case SDLK_DOWN:  m_keyS = false; return true;
            case SDLK_LEFT:  m_keyA = false; return true;
            case SDLK_RIGHT: m_keyD = false; return true;
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
        // Hero is moving between hexes
        m_isWalking = true;
        
        // Calculate progress with easing (ease-in-out for smoother starts/stops)
        float stepDist = HERO_MOVE_SPEED * dt;
        float t = (stepDist >= dist) ? 1.0f : stepDist / dist;
        
        // Use smoother easing: smoothstep-like curve
        float easedT = t * t * (3.0f - 2.0f * t);
        
        glm::vec3 movement = delta * easedT;
        m_heroRenderPos += movement;
        
        // Update facing direction based on movement direction
        float targetAngle = angleFromDirection(delta);
        m_heroFacingAngle = lerpAngle(m_heroFacingAngle, targetAngle, 8.0f * dt);
        
        // Walk bob animation - oscillate based on distance traveled
        float walkSpeed = 15.0f;
        m_walkCycle += dt * walkSpeed;
        
        // Camera does NOT move at all - for testing
        // glm::vec2 heroXZ = { m_heroRenderPos.x, m_heroRenderPos.z };
        // m_cam.setPosition(heroXZ);
        
    } else if (!m_moveQueue.empty() && 
               m_moveQueueIdx + 1 < static_cast<int>(m_moveQueue.size())) {
        // Arrived at waypoint — snap render pos and advance to next hex
        m_heroRenderPos = m_heroTargetPos;
        
        // Brief pause at each hex (like HoMM stepping rhythm)
        m_walkCycle = 0.0f;
        
        ++m_moveQueueIdx;
        float hx, hz;
        m_moveQueue[m_moveQueueIdx].toWorld(HEX_SIZE, hx, hz);
        m_heroTargetPos = { hx, 0.0f, hz };
    } else {
        // Hero arrived at final destination
        if (m_hasPendingVisit) {
            onHeroVisit(m_pendingVisit);
            m_hasPendingVisit = false;
        }
        
        // Check for battle terrain
        MapTile* currentTile = m_map.tileAt(m_hero.pos);
        if (currentTile && currentTile->terrain == Terrain::Battle) {
            MapObjectDef battleObj{ m_hero.pos, ObjType::Dungeon, "Battle Encounter" };
            Application::get().pushState(
                std::make_unique<CombatState>(buildPlayerArmy(), buildEnemyArmy(battleObj)));
        }
    }
}

// ── Render ────────────────────────────────────────────────────────────────────

void AdventureState::render() {
    auto& app = Application::get();
    m_cam.resize(app.width(), app.height());
    
    glm::mat4 view = m_cam.viewMatrix();
    glm::mat4 proj = m_cam.projMatrix();
    glm::mat4 vp   = m_cam.viewProjMatrix();

    m_hexRenderer.beginFrame(vp, HEX_SIZE, {0.4f, 1.0f, 0.3f}, {1.0f, 0.92f, 0.70f}, {0.35f, 0.30f, 0.22f}, {m_cam.position().x, 0.0f, m_cam.position().y});
    renderTerrain();
    renderPathPreview();
    m_hexRenderer.endFrame();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    renderObjects();
    renderEnemySprites();
    renderHero();
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    if (m_showHUD)
        m_hud.render(app.width(), app.height(), m_day, m_hero.movesLeft, m_hero.movesMax,
                     static_cast<int>(m_visitedObjects.size()),
                     m_hero.pos.q, m_hero.pos.r, m_infiniteMoves);
}

void AdventureState::renderTerrain() {
    for (const auto& [coord, tile] : m_map) {
        RenderOffset off = m_offsets.forTerrain(coord, tile.terrain);
        GLuint tex      = m_hexRenderer.terrainTex(tile.terrain);
        glm::vec3 color = (tex != 0) ? glm::vec3(1.0f) : terrainColor(tile.terrain);
        float h         = terrainHeight(tile.terrain) + off.dy;
        m_hexRenderer.drawTile(coord, color, HEX_SIZE, h, tex, {off.dx, off.dz});
    }

    // Hover outline (yellow).
    if (m_hasHovered) {
        float h = 0.0f;
        if (const MapTile* t = m_map.tileAt(m_hovered)) h = terrainHeight(t->terrain);
        m_hexRenderer.drawOutline(m_hovered, {1.0f, 0.9f, 0.3f}, HEX_SIZE * 0.95f, h);
    }
}

void AdventureState::renderPathPreview() {
    for (size_t i = 1; i < m_previewPath.size(); ++i) {
        float h = 0.0f;
        if (const MapTile* t = m_map.tileAt(m_previewPath[i])) h = terrainHeight(t->terrain);
        bool reachable = (static_cast<int>(i) <= m_hero.movesLeft);
        glm::vec3 col  = reachable
            ? glm::vec3{0.3f, 0.7f, 1.0f}
            : glm::vec3{0.9f, 0.2f, 0.2f};
        m_hexRenderer.drawOutline(m_previewPath[i], col, HEX_SIZE * 0.95f, h);
    }
}

void AdventureState::renderObjects() {
    for (const auto& obj : m_map.objects()) {
        if (obj.type == ObjType::Dungeon) continue;

        RenderOffset off = m_offsets.forObject(obj.pos, obj.type);
        float wx, wz;
        obj.pos.toWorld(HEX_SIZE, wx, wz);
        wx += off.dx;  wz += off.dz;

        float spriteScale = HEX_SIZE * 1.8f;
        m_buildingSpriteRenderer.draw({wx, off.dy, wz}, spriteScale,
                                     m_cam.viewMatrix(), m_cam.projMatrix());
    }
}

void AdventureState::renderEnemySprites() {
    for (const auto& obj : m_map.objects()) {
        if (obj.type != ObjType::Dungeon) continue;
        if (m_visitedObjects.count(obj.pos)) continue;  // defeated — don't render

        RenderOffset off = m_offsets.forObject(obj.pos, obj.type);
        float wx, wz;
        obj.pos.toWorld(HEX_SIZE, wx, wz);
        wx += off.dx;  wz += off.dz;

        m_enemySpriteRenderer.draw({wx, off.dy, wz}, HEX_SIZE,
                                   m_cam.viewMatrix(), m_cam.projMatrix());
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

    // Apply walk bobbing animation when moving
    glm::vec3 renderPos = m_heroRenderPos;
    if (m_isWalking) {
        float bob = std::sin(m_walkCycle) * WALK_BOB_HEIGHT;
        renderPos.y += bob;
    }

    m_spriteRenderer.draw(renderPos, HEX_SIZE,
                          m_cam.viewMatrix(), m_cam.projMatrix());
}
