#include "DungeonState.h"
#include "combat/CombatState.h"
#include "combat/CombatUnit.h"
#include "world/WondrousItem.h"
#include "core/Application.h"
#include "render/TileVisuals.h"
#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <iostream>
#include <cmath>

// ── Construction ──────────────────────────────────────────────────────────────

DungeonState::DungeonState(Hero                            heroSnapshot,
                           SpecialCharacter                dungeonSC,
                           std::vector<std::string>        lootTable,
                           std::shared_ptr<DungeonOutcome> outcome)
    : m_hero(std::move(heroSnapshot))
    , m_dungeonSC(std::move(dungeonSC))
    , m_lootTable(std::move(lootTable))
    , m_outcome(std::move(outcome))
{}

// ── Map generation ─────────────────────────────────────────────────────────────
//
// Layout (10 cols × 5 rows, even-q offset):
//
//   col:  0  1  2  3  4  5  6  7  8  9
//   row0: O  O  O  O  O  O  O  O  O  O
//   row1: O  .  .  .  O  O  O  .  .  O
//   row2: .  .  .  .  .  G  .  S  .  O
//   row3: O  .  .  .  O  O  O  .  .  O
//   row4: O  O  O  O  O  O  O  O  O  O
//
// Obsidian walls at cols 4-6 rows 1,3 force the hero through the guard (G).

void DungeonState::generateMap() {
    for (int col = 0; col < GRID_W; ++col) {
        for (int row = 0; row < GRID_H; ++row) {
            HexCoord c = toHex(col, row);

            bool passable =
                (col == 0 && row == 2)                               // entrance
                || (col >= 1 && col <= 3 && row >= 1 && row <= 3)   // left chamber
                || (col >= 4 && col <= 6 && row == 2)               // bottleneck corridor
                || (col >= 7 && col <= 8 && row >= 1 && row <= 3);  // right chamber (SC room)

            m_map.setTile(c, makeTile(passable ? Terrain::Ruins : Terrain::Obsidian));
        }
    }

    m_entrancePos = toHex(0, 2);
    m_enemyPos    = toHex(5, 2);
    m_scPos       = toHex(7, 2);
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void DungeonState::onEnter() {
    auto& app = Application::get();
    m_cam = Camera2D(app.width(), app.height());

    m_hexRenderer.init();
    m_hexRenderer.loadTerrainTextures("assets");

    m_heroSprite.init();
    m_heroSprite.loadSprite("assets/textures/units/armoured_warrior.png");

    m_enemySprite.init();
    m_enemySprite.loadSprite("assets/textures/units/enemy_scout.png");

    m_scSprite.init();
    m_scSprite.loadSprite("assets/textures/units/kharim.png");

    m_hud.init();

    generateMap();

    // Place hero at entrance.
    m_hero.pos = m_entrancePos;
    float hx, hz;
    m_entrancePos.toWorld(HEX_SIZE, hx, hz);
    m_heroRenderPos = { hx, 0.f, hz };
    m_heroTargetPos = m_heroRenderPos;

    // Centre camera on dungeon midpoint (col 4, row 2).
    HexCoord mid = toHex(4, 2);
    float cx, cz;
    mid.toWorld(HEX_SIZE, cx, cz);
    m_cam.setPosition({ cx, cz });
    m_cam.setZoom(6.5f);

    std::cout << "[Dungeon] Entered. Guard at "
              << m_enemyPos.q << "," << m_enemyPos.r
              << " | SC at " << m_scPos.q << "," << m_scPos.r << "\n";
}

void DungeonState::onExit() {
    if (!m_outcome) return;

    m_outcome->completed = m_enemyDefeated;

    // Snapshot hero army as survivors.
    m_outcome->survivors.clear();
    for (const auto& slot : m_hero.army)
        if (!slot.isEmpty())
            m_outcome->survivors.push_back({ slot.unitType, slot.count });

    if (m_scRecruited)
        m_outcome->scRecruited.push_back(m_dungeonSC);

    m_outcome->itemsFound = std::move(m_collectedItems);
}

void DungeonState::onResume() {
    if (!m_pendingCombat) return;

    CombatOutcome result = std::move(*m_pendingCombat);
    m_pendingCombat.reset();

    // Sync hero army from combat survivors (covers PlayerWon, EnemyWon, Retreated).
    for (auto& slot : m_hero.army) slot = {};
    for (const auto& s : result.survivors)
        if (s.type && s.count > 0)
            m_hero.addUnit(s.type, s.count);

    switch (result.result) {
        case CombatResult::PlayerWon:
            m_enemyDefeated = true;
            for (const auto& id : result.itemsFound)
                m_collectedItems.push_back(id);
            std::cout << "[Dungeon] Guard defeated! Approach the figure to recruit.\n";
            break;

        case CombatResult::EnemyWon:
            std::cout << "[Dungeon] Hero defeated — exiting dungeon.\n";
            m_wantsDismiss = true;
            break;

        case CombatResult::Retreated:
            std::cout << "[Dungeon] Retreated from guard.\n";
            break;

        default: break;
    }
}

// ── Game logic ────────────────────────────────────────────────────────────────

void DungeonState::moveHero(const HexCoord& dest) {
    if (dest == m_hero.pos) return;
    if (!m_map.hasTile(dest)) return;

    auto path = m_map.findPath(m_hero.pos, dest);
    if (path.size() < 2) return;

    m_hero.pos     = dest;
    m_moveQueue    = path;
    m_moveQueueIdx = 1;
    float hx, hz;
    m_moveQueue[1].toWorld(HEX_SIZE, hx, hz);
    m_heroTargetPos   = { hx, 0.f, hz };
    m_pendingVisit    = dest;
    m_hasPendingVisit = true;
}

void DungeonState::onHeroVisit(const HexCoord& coord) {
    // Enemy guard — only if alive.
    if (coord == m_enemyPos && !m_enemyDefeated) {
        m_pendingCombat = std::make_shared<CombatOutcome>();
        Application::get().pushState(
            std::make_unique<CombatState>(buildPlayerArmy(), buildEnemyArmy(),
                                          m_pendingCombat, m_lootTable));
        return;
    }

    // SC recruitment — only after guard is cleared.
    if (coord == m_scPos && m_enemyDefeated && !m_scRecruited) {
        m_scRecruited = true;
        std::cout << "[Dungeon] " << m_dungeonSC.name << " joins your party!\n";
    }
}

CombatArmy DungeonState::buildPlayerArmy() const {
    CombatArmy army;
    army.ownerName = m_hero.name;
    army.isPlayer  = true;
    for (const auto& slot : m_hero.army)
        if (!slot.isEmpty())
            army.stacks.push_back(CombatUnit::make(slot.unitType, slot.count, true));
    for (const auto& sc : m_hero.specials) {
        if (sc.isEmpty()) continue;
        CombatUnit cu = CombatUnit::make(&sc.combatStats, 1, true);
        for (const WondrousItem* item : sc.equipped) {
            if (!item) continue;
            for (const ItemEffect& ef : item->passiveEffects) {
                if      (ef.stat == "attack")  cu.attackBonus  += ef.amount;
                else if (ef.stat == "defense") cu.defenseBonus += ef.amount;
                else if (ef.stat == "damage")  cu.damageBonus  += ef.amount;
                else if (ef.stat == "speed")   cu.speedBonus   += ef.amount;
            }
        }
        army.stacks.push_back(cu);
    }
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

CombatArmy DungeonState::buildEnemyArmy() const {
    CombatArmy army;
    army.ownerName = "Dungeon Guard";
    army.isPlayer  = false;
    auto& rm = Application::get().resources();
    if (rm.loaded()) {
        if (const UnitType* t = rm.unit("skeleton_warrior"))
            army.stacks.push_back(CombatUnit::make(t, 12, false));
        if (const UnitType* t = rm.unit("sand_scorpion"))
            army.stacks.push_back(CombatUnit::make(t,  4, false));
    }
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

// ── Input ─────────────────────────────────────────────────────────────────────

bool DungeonState::handleEvent(void* sdlEvent) {
    const SDL_Event* e = static_cast<SDL_Event*>(sdlEvent);

    if (e->type == SDL_KEYDOWN) {
        switch (e->key.keysym.sym) {
            case SDLK_ESCAPE:
                m_wantsDismiss = true;
                return true;
            case SDLK_UP:    case SDLK_w: m_keyW = true; return true;
            case SDLK_DOWN:  case SDLK_s: m_keyS = true; return true;
            case SDLK_LEFT:  case SDLK_a: m_keyA = true; return true;
            case SDLK_RIGHT: case SDLK_d: m_keyD = true; return true;
            default: break;
        }
    }
    if (e->type == SDL_KEYUP) {
        switch (e->key.keysym.sym) {
            case SDLK_UP:    case SDLK_w: m_keyW = false; return true;
            case SDLK_DOWN:  case SDLK_s: m_keyS = false; return true;
            case SDLK_LEFT:  case SDLK_a: m_keyA = false; return true;
            case SDLK_RIGHT: case SDLK_d: m_keyD = false; return true;
            default: break;
        }
    }
    if (e->type == SDL_MOUSEWHEEL) {
        m_cam.adjustZoom(-e->wheel.y * Camera2D::ZOOM_STEP);
        return true;
    }
    if (e->type == SDL_MOUSEMOTION) {
        glm::vec2 wp = m_cam.screenToWorld(
            static_cast<float>(e->motion.x), static_cast<float>(e->motion.y));
        HexCoord hc = HexCoord::fromWorld(wp.x, wp.y, HEX_SIZE);
        m_hasHovered = m_map.hasTile(hc);
        m_hoveredHex = hc;
        m_previewPath.clear();
        if (m_heroSelected && m_hasHovered && hc != m_hero.pos)
            m_previewPath = m_map.findPath(m_hero.pos, hc);
        return false;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        glm::vec2 wp = m_cam.screenToWorld(
            static_cast<float>(e->button.x), static_cast<float>(e->button.y));
        HexCoord clicked = HexCoord::fromWorld(wp.x, wp.y, HEX_SIZE);

        float heroWx, heroWz;
        m_hero.pos.toWorld(HEX_SIZE, heroWx, heroWz);
        float dx = wp.x - heroWx, dz = wp.y - heroWz;
        bool clickedHero = (dx*dx + dz*dz) < (HEX_SIZE * HEX_SIZE * 1.5f);

        if (clickedHero) {
            m_heroSelected = !m_heroSelected;
            m_previewPath.clear();
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
    if (e->type == SDL_WINDOWEVENT && e->window.event == SDL_WINDOWEVENT_RESIZED)
        m_cam.resize(e->window.data1, e->window.data2);
    return false;
}

// ── Update ────────────────────────────────────────────────────────────────────

void DungeonState::update(float dt) {
    if (m_wantsDismiss) {
        Application::get().popState();
        return;
    }

    // Camera pan.
    float dx = 0.f, dz = 0.f;
    if (m_keyW) dz -= 1.f;
    if (m_keyS) dz += 1.f;
    if (m_keyA) dx -= 1.f;
    if (m_keyD) dx += 1.f;
    if (dx != 0.f || dz != 0.f) {
        float speed = CAM_SPEED * m_cam.zoom() / 12.f;
        m_cam.pan(dx * speed * dt, dz * speed * dt);
    }

    // Hero smooth movement.
    glm::vec3 delta = m_heroTargetPos - m_heroRenderPos;
    float dist = glm::length(delta);

    if (dist > 0.001f) {
        float t = std::min(HERO_MOVE_SPEED * dt / dist, 1.f);
        m_heroRenderPos += delta * t;
    } else if (!m_moveQueue.empty() &&
               m_moveQueueIdx + 1 < static_cast<int>(m_moveQueue.size())) {
        m_heroRenderPos = m_heroTargetPos;
        ++m_moveQueueIdx;
        float hx, hz;
        m_moveQueue[m_moveQueueIdx].toWorld(HEX_SIZE, hx, hz);
        m_heroTargetPos = { hx, 0.f, hz };
    } else {
        if (m_hasPendingVisit) {
            onHeroVisit(m_pendingVisit);
            m_hasPendingVisit = false;
        }
    }
}

// ── Render ────────────────────────────────────────────────────────────────────

void DungeonState::render() {
    glClearColor(0.04f, 0.03f, 0.07f, 1.0f);  // deep dungeon dark
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    auto& app = Application::get();
    m_cam.resize(app.width(), app.height());

    const glm::mat4 view = m_cam.viewMatrix();
    const glm::mat4 proj = m_cam.projMatrix();
    const glm::mat4 vp   = m_cam.viewProjMatrix();

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    // Dungeon lighting: cool torchlight, heavy shadow.
    m_hexRenderer.beginFrame(
        vp, HEX_SIZE,
        glm::vec3(0.3f, 0.8f, 0.5f),      // light direction
        glm::vec3(0.65f, 0.50f, 0.85f),   // cool purple torchlight
        glm::vec3(0.10f, 0.07f, 0.15f),   // deep shadow ambient
        glm::vec3(m_cam.position().x, 0.f, m_cam.position().y)
    );

    // Terrain tiles.
    for (const auto& [coord, tile] : m_map) {
        GLuint tex      = m_hexRenderer.terrainTex(tile.terrain);
        glm::vec3 color = (tex != 0) ? glm::vec3(1.f) : terrainColor(tile.terrain);
        m_hexRenderer.drawTile(coord, color, HEX_SIZE, terrainHeight(tile.terrain), tex);
    }

    // Entrance glow — green outline so player knows where to ESC.
    m_hexRenderer.drawOutline(m_entrancePos, { 0.2f, 0.9f, 0.3f }, HEX_SIZE * 0.88f);

    // Enemy outline (red) while alive.
    if (!m_enemyDefeated)
        m_hexRenderer.drawOutline(m_enemyPos, { 0.9f, 0.15f, 0.15f }, HEX_SIZE * 0.88f);

    // SC outline (gold) when recruitable.
    if (m_enemyDefeated && !m_scRecruited)
        m_hexRenderer.drawOutline(m_scPos, { 0.95f, 0.80f, 0.15f }, HEX_SIZE * 0.88f);

    // Path preview.
    for (size_t i = 1; i < m_previewPath.size(); ++i)
        m_hexRenderer.drawOutline(m_previewPath[i], { 0.3f, 0.6f, 1.f }, HEX_SIZE * 0.9f);

    // Hero selection ring.
    HexCoord heroHex = HexCoord::fromWorld(m_heroRenderPos.x, m_heroRenderPos.z, HEX_SIZE);
    if (m_heroSelected)
        m_hexRenderer.drawOutline(heroHex, { 1.f, 1.f, 1.f }, HEX_SIZE * 0.65f);

    // Hover outline.
    if (m_hasHovered && m_map.hasTile(m_hoveredHex))
        m_hexRenderer.drawOutline(m_hoveredHex, { 0.8f, 0.8f, 0.8f }, HEX_SIZE * 0.88f);

    m_hexRenderer.endFrame();

    // ── Sprites ───────────────────────────────────────────────────────────────
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    if (!m_enemyDefeated) {
        float wx, wz;
        m_enemyPos.toWorld(HEX_SIZE, wx, wz);
        m_enemySprite.draw({ wx, 0.f, wz }, HEX_SIZE * 0.85f, view, proj);
    }

    if (!m_scRecruited) {
        float wx, wz;
        m_scPos.toWorld(HEX_SIZE, wx, wz);
        m_scSprite.draw({ wx, 0.f, wz }, HEX_SIZE * 1.05f, view, proj);
    }

    m_heroSprite.draw(m_heroRenderPos, HEX_SIZE, view, proj);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    // ── HUD ───────────────────────────────────────────────────────────────────
    int sw = app.width(), sh = app.height();
    float sc = sh / 600.f;
    m_hud.begin(sw, sh);

    // Top bar.
    m_hud.drawRect(0.f, 0.f, static_cast<float>(sw), 28.f * sc,
                   { 0.f, 0.f, 0.f, 0.72f });
    m_hud.drawText(8.f * sc, 5.f * sc, sc * 1.1f,
                   "DUNGEON  —  ESC to exit", { 0.75f, 0.60f, 0.95f, 1.f });

    // Status hint.
    if (!m_enemyDefeated) {
        m_hud.drawText(static_cast<float>(sw) * 0.45f, 5.f * sc, sc * 1.1f,
                       "Defeat the guard to proceed", { 0.95f, 0.50f, 0.30f, 1.f });
    } else if (!m_scRecruited) {
        m_hud.drawText(static_cast<float>(sw) * 0.45f, 5.f * sc, sc * 1.1f,
                       "Approach the glowing figure", { 0.95f, 0.85f, 0.25f, 1.f });
    } else {
        m_hud.drawText(static_cast<float>(sw) * 0.45f, 5.f * sc, sc * 1.1f,
                       "Dungeon cleared!  ESC to return", { 0.35f, 1.f, 0.45f, 1.f });
    }
}
