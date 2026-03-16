#include "DungeonState.h"
#include "combat/CombatState.h"
#include "party/PartyState.h"
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

DungeonState::DungeonState(Hero                                    heroSnapshot,
                           std::optional<SpecialCharacter>         dungeonSC,
                           std::vector<std::string>                lootTable,
                           std::shared_ptr<DungeonOutcome>         outcome)
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

    // Hero: same 8-frame sheet as adventure map (idle 0-3, walk 4-7).
    m_heroSprite.init();
    m_heroSprite.loadSprite("assets/textures/units/armoured_warrior_anim.png", 8);
    m_heroSprite.addClip("idle", {0, 4, 6.0f,  true});
    m_heroSprite.addClip("walk", {4, 4, 10.0f, true});
    m_heroSprite.play("idle");

    // Enemy guard: single-frame idle loop for now.
    m_enemySprite.init();
    m_enemySprite.loadSprite("assets/textures/units/enemy_scout.png", 1);
    m_enemySprite.addClip("idle", {0, 1, 6.0f, true});
    m_enemySprite.play("idle");

    // Dungeon SC portrait: single-frame idle loop for now.
    m_scSprite.init();
    m_scSprite.loadSprite("assets/textures/units/kharim.png", 1);
    m_scSprite.addClip("idle", {0, 1, 6.0f, true});
    m_scSprite.play("idle");

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

    if (m_scRecruited && m_dungeonSC.has_value())
        m_outcome->scRecruited.push_back(*m_dungeonSC);

    m_outcome->itemsFound = std::move(m_collectedItems);

    // Pass SC progression earned inside this dungeon back to the adventure map.
    m_outcome->scUpdates.clear();
    for (const auto& sc : m_hero.specials) {
        if (sc.isEmpty() || !sc.def) continue;
        m_outcome->scUpdates.push_back({ sc.id, sc.level, sc.xp, sc.unlockedActions, sc.chosenBranches });
    }
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

    // Build result display — capture old levels before syncing SC stats.
    m_lastCombatResult = {};
    m_lastCombatResult.result = result.result;

    // Sync SC progression earned in combat back to the hero snapshot.
    for (const auto& upd : result.scUpdates) {
        SpecialCharacter* sc = m_hero.findSpecial(upd.scId);
        if (!sc || !sc->def) continue;

        CombatResultDisplay::ScGain gain;
        gain.name     = sc->name;
        gain.oldLevel = sc->level;
        gain.newLevel = upd.level;
        gain.newXp    = upd.xp;
        m_lastCombatResult.scGains.push_back(gain);

        int levelDelta = upd.level - sc->level;
        for (int i = 0; i < levelDelta; ++i) {
            sc->combatStats.attack    += sc->def->attackGrowth;
            sc->combatStats.defense   += sc->def->defenseGrowth;
            sc->combatStats.hitPoints += sc->def->maxHpGrowth;
        }
        sc->level             = upd.level;
        sc->xp                = upd.xp;
        sc->unlockedActions = upd.unlockedActions;
            for (const auto& [lv, br] : upd.chosenBranches)
                sc->chosenBranches[lv] = br;
        if (levelDelta > 0)
            std::cout << "[Dungeon] " << sc->name
                      << " is now level " << sc->level << "!\n";
    }

    switch (result.result) {
        case CombatResult::PlayerWon:
            m_enemyDefeated = true;
            for (const auto& id : result.itemsFound) {
                m_collectedItems.push_back(id);
                auto& rm = Application::get().resources();
                if (const WondrousItem* it = rm.item(id))
                    m_lastCombatResult.itemNames.push_back(it->name);
                else
                    m_lastCombatResult.itemNames.push_back(id);
            }
            std::cout << "[Dungeon] Guard defeated! Approach the figure to recruit.\n";
            break;

        case CombatResult::EnemyWon:
            std::cout << "[Dungeon] Hero defeated — exiting dungeon.\n";
            m_lastCombatResult.wantsDismissAfter = true;
            break;

        case CombatResult::Retreated:
            std::cout << "[Dungeon] Retreated from guard.\n";
            break;

        default: break;
    }

    m_showCombatResult = true;
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
    if (coord == m_scPos && m_enemyDefeated && !m_scRecruited && m_dungeonSC.has_value()) {
        m_scRecruited = true;
        std::cout << "[Dungeon] " << m_dungeonSC->name << " joins your party!\n";
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
        cu.isSpecialCharacter = true;
        cu.scId               = sc.id;
        cu.scDef              = sc.def;
        cu.scLevel            = sc.level;
        cu.scXp               = sc.xp;
        cu.scUnlocked         = sc.unlockedActions;
        if (sc.def) {
            cu.killXp    = sc.def->killBonusXp;
            cu.perTurnXp = sc.def->perTurnXp;
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

    // Dismiss post-combat result overlay on any key or left-click.
    if (m_showCombatResult) {
        if (e->type == SDL_KEYDOWN ||
            (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT)) {
            m_showCombatResult = false;
            if (m_lastCombatResult.wantsDismissAfter)
                m_wantsDismiss = true;
        }
        return true;
    }

    if (e->type == SDL_KEYDOWN) {
        switch (e->key.keysym.sym) {
            case SDLK_ESCAPE:
                m_wantsDismiss = true;
                return true;
            case SDLK_f:
                Application::get().pushState(std::make_unique<PartyState>(m_hero));
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

    bool isWalking = false;
    if (dist > 0.001f) {
        isWalking = true;
        float t = std::min(HERO_MOVE_SPEED * dt / dist, 1.f);
        m_heroRenderPos += delta * t;
    } else if (!m_moveQueue.empty() &&
               m_moveQueueIdx + 1 < static_cast<int>(m_moveQueue.size())) {
        m_heroRenderPos = m_heroTargetPos;
        ++m_moveQueueIdx;
        float hx, hz;
        m_moveQueue[m_moveQueueIdx].toWorld(HEX_SIZE, hx, hz);
        m_heroTargetPos = { hx, 0.f, hz };
        isWalking = true;
    } else {
        if (m_hasPendingVisit) {
            onHeroVisit(m_pendingVisit);
            m_hasPendingVisit = false;
        }
    }

    // Advance all sprite animations.
    m_heroSprite.play(isWalking ? "walk" : "idle");
    m_heroSprite.update(dt);
    m_enemySprite.update(dt);
    m_scSprite.update(dt);
}

// ── Post-combat result overlay ─────────────────────────────────────────────────

void DungeonState::renderCombatResult(int sw, int sh) {
    const float sc     = sh / 600.f;
    const float lineH  = 20.f * sc;
    const float padX   = 18.f * sc;
    const float padY   = 14.f * sc;

    // Count rows: title + result + blank + items + SC gains + prompt
    int dataRows = static_cast<int>(m_lastCombatResult.itemNames.size())
                 + static_cast<int>(m_lastCombatResult.scGains.size());
    const float panelW = 320.f * sc;
    const float panelH = padY * 2.f + lineH * (4 + dataRows);
    const float panelX = (sw - panelW) * 0.5f;
    const float panelY = (sh - panelH) * 0.5f;

    m_hud.begin(sw, sh);

    // Dark veil over dungeon.
    m_hud.drawRect(0.f, 0.f, static_cast<float>(sw), static_cast<float>(sh),
                   { 0.f, 0.f, 0.f, 0.55f });

    // Panel background + accent border.
    m_hud.drawRect(panelX, panelY, panelW, panelH, { 0.05f, 0.04f, 0.10f, 0.95f });
    m_hud.drawRect(panelX, panelY, panelW, 2.f * sc, { 0.7f, 0.6f, 0.2f, 1.f });

    float y = panelY + padY;

    // Result headline.
    static constexpr glm::vec4 COL_WIN     = { 0.35f, 1.f,  0.45f, 1.f };
    static constexpr glm::vec4 COL_LOSE    = { 0.9f,  0.15f,0.1f,  1.f };
    static constexpr glm::vec4 COL_RET     = { 0.9f,  0.8f, 0.3f,  1.f };
    static constexpr glm::vec4 COL_LABEL   = { 0.75f, 0.65f,0.95f, 1.f };
    static constexpr glm::vec4 COL_VALUE   = { 0.95f, 0.90f,0.70f, 1.f };
    static constexpr glm::vec4 COL_LVUP    = { 1.f,   0.9f, 0.2f,  1.f };
    static constexpr glm::vec4 COL_PROMPT  = { 0.55f, 0.55f,0.55f, 1.f };

    const char* headline;
    glm::vec4   hlColor;
    switch (m_lastCombatResult.result) {
        case CombatResult::PlayerWon:
            headline = "VICTORY!"; hlColor = COL_WIN;  break;
        case CombatResult::EnemyWon:
            headline = "DEFEATED"; hlColor = COL_LOSE; break;
        case CombatResult::Retreated:
            headline = "RETREATED"; hlColor = COL_RET; break;
        default:
            headline = "BATTLE OVER"; hlColor = COL_VALUE; break;
    }
    m_hud.drawText(panelX + padX, y, sc * 2.0f, headline, hlColor);
    y += lineH * 1.6f;

    // Items found.
    if (!m_lastCombatResult.itemNames.empty()) {
        m_hud.drawText(panelX + padX, y, sc * 1.1f, "Found:", COL_LABEL);
        y += lineH;
        for (const auto& name : m_lastCombatResult.itemNames) {
            m_hud.drawText(panelX + padX * 1.6f, y, sc * 1.0f,
                           name.c_str(), COL_VALUE);
            y += lineH;
        }
    }

    // SC progression.
    for (const auto& g : m_lastCombatResult.scGains) {
        char buf[128];
        if (g.newLevel > g.oldLevel) {
            std::snprintf(buf, sizeof(buf), "%s  Lv %d → %d  *** LEVEL UP! ***",
                          g.name.c_str(), g.oldLevel, g.newLevel);
            m_hud.drawText(panelX + padX, y, sc * 1.0f, buf, COL_LVUP);
        } else {
            std::snprintf(buf, sizeof(buf), "%s  Lv %d  (%d XP)",
                          g.name.c_str(), g.newLevel, g.newXp);
            m_hud.drawText(panelX + padX, y, sc * 1.0f, buf, COL_VALUE);
        }
        y += lineH;
    }

    // Dismiss prompt.
    y += lineH * 0.4f;
    m_hud.drawText(panelX + padX, y, sc * 0.95f,
                   "Press any key to continue", COL_PROMPT);
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
    if (m_enemyDefeated && !m_scRecruited && m_dungeonSC.has_value())
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

    if (!m_scRecruited && m_dungeonSC.has_value()) {
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
    } else if (!m_scRecruited && m_dungeonSC.has_value()) {
        m_hud.drawText(static_cast<float>(sw) * 0.45f, 5.f * sc, sc * 1.1f,
                       "Approach the glowing figure", { 0.95f, 0.85f, 0.25f, 1.f });
    } else {
        m_hud.drawText(static_cast<float>(sw) * 0.45f, 5.f * sc, sc * 1.1f,
                       "Dungeon cleared!  ESC to return", { 0.35f, 1.f, 0.45f, 1.f });
    }

    if (m_showCombatResult)
        renderCombatResult(sw, sh);
}
