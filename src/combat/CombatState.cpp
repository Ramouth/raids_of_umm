#include "CombatState.h"
#include "CombatAI.h"
#include "core/Application.h"
#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <iostream>
#include <cmath>
#include <cstdio>
#include <cstdlib>

CombatState::CombatState(CombatArmy player, CombatArmy enemy,
                         std::shared_ptr<CombatOutcome> outcome,
                         std::vector<std::string>       lootTable)
    : m_engine(std::move(player), std::move(enemy))
    , m_outcome(std::move(outcome))
    , m_lootTable(std::move(lootTable))
{}

// ── Log helpers ───────────────────────────────────────────────────────────────

void CombatState::pushLog(std::string text, glm::vec4 color) {
    m_log.push_back({ std::move(text), color });
    while (static_cast<int>(m_log.size()) > MAX_LOG)
        m_log.pop_front();
}

void CombatState::renderLog(int screenW, int screenH) {
    if (m_log.empty()) return;

    const float scale    = screenH / 600.0f;
    const float lineH    = 16.f * scale;
    const float padX     = 8.f  * scale;
    const float padY     = 6.f  * scale;
    const float panelW   = 320.f * scale;
    const float panelH   = static_cast<float>(m_log.size()) * lineH + padY * 2.f;
    const float panelX   = screenW - panelW - 8.f * scale;
    const float panelY   = 8.f * scale;

    m_hud.begin(screenW, screenH);

    // Background + top border
    m_hud.drawRect(panelX, panelY, panelW, panelH, {0.0f, 0.0f, 0.0f, 0.72f});
    m_hud.drawRect(panelX, panelY, panelW, 2.f * scale, {0.5f, 0.45f, 0.25f, 1.0f});

    float ts = scale * 1.3f;
    for (int i = 0; i < static_cast<int>(m_log.size()); ++i) {
        float ty = panelY + padY + i * lineH;
        m_hud.drawText(panelX + padX, ty, ts,
                       m_log[i].text.c_str(), m_log[i].color);
    }
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void CombatState::onEnter() {
    m_renderer.init();
    m_hud.init();

    // Centre the camera on the combat grid.
    // Grid is 11 cols × 5 rows; centre hex ≈ col 5, row 2.
    // toHex(5, 2) → q=5, r=2-(5-1)/2=0  →  toWorld: x=7.5, z=√3*2.5≈4.33
    m_cam.setPosition({ 7.5f, 4.3f });
    m_cam.setZoom(6.5f);

    std::cout << "[CombatState] Battle started\n";
}

void CombatState::onExit() {
    std::cout << "[CombatState] Battle ended — ";
    switch (m_engine.result()) {
        case CombatResult::PlayerWon: std::cout << "player won\n";  break;
        case CombatResult::EnemyWon:  std::cout << "enemy won\n";   break;
        case CombatResult::Retreated: std::cout << "retreated\n";   break;
        default:                      std::cout << "ongoing\n";     break;
    }

    // Write result + surviving player stacks so the caller can update the hero.
    if (m_outcome) {
        m_outcome->result = m_engine.result();
        m_outcome->survivors.clear();
        for (const auto& unit : m_engine.playerArmy().stacks) {
            if (!unit.isDead())
                m_outcome->survivors.push_back({ unit.type, unit.count });
        }

        // On victory, award one random item from the loot table (if any).
        if (m_engine.result() == CombatResult::PlayerWon && !m_lootTable.empty()) {
            int idx = std::rand() % static_cast<int>(m_lootTable.size());
            m_outcome->itemsFound.push_back(m_lootTable[idx]);
        }
    }
}

void CombatState::tickAnimation(float dt) {
    // 1. Drain any events the engine produced since last tick.
    for (auto& ev : m_engine.drainEvents())
        m_pendingEvents.push(std::move(ev));

    // 2. Advance running animation.
    if (m_animating) {
        m_animTimer += dt;
        // For move: smoothly interpolate world position.
        if (m_currentEvent.type == CombatEvent::Type::UnitMoved) {
            float t = std::min(m_animTimer / m_animDuration, 1.0f);
            t = t * t * (3.f - 2.f * t);
            m_anim.worldPos = m_animFrom + t * (m_animTo - m_animFrom);
        }
        if (m_animTimer >= m_animDuration) {
            m_animating   = false;
            m_anim.active = false;
        }
        return;  // still animating — don't start a new event or let AI act
    }

    // 3. Start next pending event (one per tick for sequential playback).
    if (!m_pendingEvents.empty()) {
        startEventAnimation(m_pendingEvents.front());
        m_pendingEvents.pop();
        return;
    }

    // 4. Queue empty and game is over — nothing more to drive.
    if (m_engine.isOver()) return;

    // 5. AI turn: enemy always; friendly when auto-battle is active.
    //    Drain immediately into m_pendingEvents to close the race window
    //    (game loop order: processEvents → update → render).
    bool aiShouldAct = !m_engine.currentTurn().isPlayer
                    || m_autoBattle;
    if (aiShouldAct) {
        CombatAI::takeTurn(m_engine);
        for (auto& ev : m_engine.drainEvents())
            m_pendingEvents.push(std::move(ev));
    }
}

// ── startEventAnimation ───────────────────────────────────────────────────────

static const char* unitName(const CombatEngine& eng, bool isPlayer, int idx) {
    const CombatArmy& army = isPlayer ? eng.playerArmy() : eng.enemyArmy();
    if (idx < 0 || idx >= static_cast<int>(army.stacks.size())) return "?";
    const auto& s = army.stacks[idx];
    return s.type ? s.type->name.c_str() : "?";
}

void CombatState::startEventAnimation(const CombatEvent& ev) {
    m_currentEvent = ev;
    m_animating    = true;
    m_animTimer    = 0.f;

    // ── Combat log entry ─────────────────────────────────────────────────────
    char buf[128];
    static constexpr glm::vec4 COL_PLAYER  = {0.6f, 1.0f, 0.65f, 1.0f};  // green
    static constexpr glm::vec4 COL_ENEMY   = {1.0f, 0.5f, 0.4f,  1.0f};  // red
    static constexpr glm::vec4 COL_DAMAGE  = {1.0f, 0.85f, 0.3f, 1.0f};  // yellow
    static constexpr glm::vec4 COL_DEATH   = {0.7f, 0.7f,  0.7f, 1.0f};  // grey
    static constexpr glm::vec4 COL_VICTORY = {1.0f, 0.95f, 0.4f, 1.0f};  // bright gold
    static constexpr glm::vec4 COL_DEFEAT  = {0.9f, 0.2f,  0.2f, 1.0f};  // bright red

    switch (ev.type) {
        case CombatEvent::Type::UnitMoved: {
            const char* side = ev.isPlayer ? "[P]" : "[E]";
            std::snprintf(buf, sizeof(buf), "%s %s moves",
                          side, unitName(m_engine, ev.isPlayer, ev.stackIndex));
            pushLog(buf, ev.isPlayer ? COL_PLAYER : COL_ENEMY);
            break;
        }
        case CombatEvent::Type::UnitAttacked: {
            const char* atk = unitName(m_engine, ev.isPlayer, ev.stackIndex);
            const char* def = unitName(m_engine, ev.targetIsPlayer, ev.targetIndex);
            std::snprintf(buf, sizeof(buf), "%s attacks %s", atk, def);
            pushLog(buf, ev.isPlayer ? COL_PLAYER : COL_ENEMY);
            break;
        }
        case CombatEvent::Type::UnitDamaged: {
            const char* name = unitName(m_engine, ev.isPlayer, ev.stackIndex);
            const auto& army = ev.isPlayer ? m_engine.playerArmy() : m_engine.enemyArmy();
            int remaining = (ev.stackIndex >= 0 && ev.stackIndex < static_cast<int>(army.stacks.size()))
                            ? army.stacks[ev.stackIndex].count : 0;
            std::snprintf(buf, sizeof(buf), "  %s: -%d hp (%d left)",
                          name, ev.damage, remaining);
            pushLog(buf, COL_DAMAGE);
            break;
        }
        case CombatEvent::Type::UnitDied: {
            const char* name = unitName(m_engine, ev.isPlayer, ev.stackIndex);
            std::snprintf(buf, sizeof(buf), "  %s wiped out!", name);
            pushLog(buf, COL_DEATH);
            break;
        }
        case CombatEvent::Type::UnitDefended: {
            const char* side = ev.isPlayer ? "[P]" : "[E]";
            std::snprintf(buf, sizeof(buf), "%s %s defends",
                          side, unitName(m_engine, ev.isPlayer, ev.stackIndex));
            pushLog(buf, ev.isPlayer ? COL_PLAYER : COL_ENEMY);
            break;
        }
        case CombatEvent::Type::BattleEnded:
            switch (ev.result) {
                case CombatResult::PlayerWon:
                    pushLog("*** VICTORY! ***", COL_VICTORY); break;
                case CombatResult::EnemyWon:
                    pushLog("*** DEFEAT! ***", COL_DEFEAT); break;
                case CombatResult::Retreated:
                    pushLog("*** RETREATED ***", COL_DEATH); break;
                default: break;
            }
            pushLog("Press any key to continue", COL_DEATH);
            break;
        default: break;
    }

    switch (ev.type) {
        case CombatEvent::Type::UnitMoved: {
            m_animDuration    = MOVE_ANIM_DURATION;
            m_anim.active     = true;
            m_anim.isPlayer   = ev.isPlayer;
            m_anim.stackIndex = ev.stackIndex;
            float fx, fz, tx, tz;
            ev.from.toWorld(HEX_SIZE, fx, fz);
            ev.to.toWorld(HEX_SIZE, tx, tz);
            m_animFrom      = { fx, 0.f, fz };
            m_animTo        = { tx, 0.f, tz };
            m_anim.worldPos = m_animFrom;
            break;
        }
        case CombatEvent::Type::UnitAttacked:
            m_animDuration = ATTACK_ANIM_DURATION;
            m_anim.active  = false;
            break;
        case CombatEvent::Type::UnitDamaged:
            m_animDuration = DAMAGE_ANIM_DURATION;
            m_anim.active  = false;
            break;
        case CombatEvent::Type::UnitDied:
            m_animDuration = DEATH_ANIM_DURATION;
            m_anim.active  = false;
            break;
        default:
            m_animDuration = FLASH_ANIM_DURATION;
            m_anim.active  = false;
            break;
    }
}

void CombatState::update(float dt) {
    // Deferred dismiss — safe to pop here because StateMachine sets m_processing=true
    // during update(), so pop() enqueues rather than destroying the object mid-call.
    if (m_wantsDismiss) {
        Application::get().popState();
        return;
    }

    tickAnimation(dt);

    // Camera pan (normalised so diagonal isn't faster)
    float dx = 0.f, dz = 0.f;
    if (m_panLeft)  dx -= 1.f;
    if (m_panRight) dx += 1.f;
    if (m_panUp)    dz -= 1.f;
    if (m_panDown)  dz += 1.f;
    if (dx != 0.f || dz != 0.f) {
        float len = std::sqrt(dx*dx + dz*dz);
        m_cam.pan(dx / len * CAM_SPEED * dt,
                  dz / len * CAM_SPEED * dt);
    }
}

void CombatState::render() {
    glClearColor(0.05f, 0.04f, 0.06f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    auto& app = Application::get();
    m_cam.resize(app.width(), app.height());
    m_renderer.render(app.width(), app.height(),
                      m_engine, m_cam, m_hoveredHex, m_hasHovered, m_anim);

    renderLog(app.width(), app.height());
}

bool CombatState::handleEvent(void* sdlEvent) {
    const SDL_Event* e = static_cast<SDL_Event*>(sdlEvent);

    // ── Battle-over dismissal ─────────────────────────────────────────────────
    // Wait until all queued animations finish before accepting a dismiss input.
    if (m_engine.isOver() && m_pendingEvents.empty() && !m_animating) {
        if (e->type == SDL_KEYDOWN ||
            (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT)) {
            m_wantsDismiss = true;
            return true;
        }
        return false;
    }

    // ── Key down ──────────────────────────────────────────────────────────────
    if (e->type == SDL_KEYDOWN) {
        switch (e->key.keysym.sym) {
            // Camera pan
            case SDLK_UP:    case SDLK_w: m_panUp    = true; return true;
            case SDLK_DOWN:  case SDLK_s: m_panDown  = true; return true;
            case SDLK_LEFT:  case SDLK_a: m_panLeft  = true; return true;
            case SDLK_RIGHT: case SDLK_d: m_panRight = true; return true;

            // Toggle AI control of friendly units (auto-battle).
            case SDLK_TAB:
                m_autoBattle = !m_autoBattle;
                std::cout << "[Combat] Auto-battle: "
                          << (m_autoBattle ? "ON" : "OFF") << "\n";
                return true;

            // Combat actions — only valid on the player's turn, when idle.
            case SDLK_RETURN: case SDLK_KP_ENTER:
                if (!m_animating && m_pendingEvents.empty()
                        && m_engine.currentTurn().isPlayer)
                    m_engine.doAttack(0);
                return true;
            case SDLK_SPACE:
                if (!m_animating && m_pendingEvents.empty()
                        && m_engine.currentTurn().isPlayer)
                    m_engine.doDefend();
                return true;
            // ESC always retreats and queues immediate dismiss.
            case SDLK_ESCAPE:
                m_engine.doRetreat();
                m_wantsDismiss = true;
                return true;
            default: break;
        }
    }

    // ── Key up ────────────────────────────────────────────────────────────────
    if (e->type == SDL_KEYUP) {
        switch (e->key.keysym.sym) {
            case SDLK_UP:    case SDLK_w: m_panUp    = false; return true;
            case SDLK_DOWN:  case SDLK_s: m_panDown  = false; return true;
            case SDLK_LEFT:  case SDLK_a: m_panLeft  = false; return true;
            case SDLK_RIGHT: case SDLK_d: m_panRight = false; return true;
            default: break;
        }
    }

    // ── Mouse move → hex hover ────────────────────────────────────────────────
    if (e->type == SDL_MOUSEMOTION) {
        glm::vec2 world = m_cam.screenToWorld(
            static_cast<float>(e->motion.x),
            static_cast<float>(e->motion.y));
        HexCoord hc = HexCoord::fromWorld(world.x, world.y, HEX_SIZE);
        if (CombatMap::inBounds(hc)) {
            m_hoveredHex = hc;
            m_hasHovered = true;
        } else {
            m_hasHovered = false;
        }
        return false;  // don't consume — let AdventureState below still see it
    }

    // ── Left click → move or attack (player's turn only, when idle) ──────────
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        if (m_animating || !m_pendingEvents.empty()
                || !m_engine.currentTurn().isPlayer) return true;

        glm::vec2 world = m_cam.screenToWorld(
            static_cast<float>(e->button.x),
            static_cast<float>(e->button.y));
        HexCoord clicked = HexCoord::fromWorld(world.x, world.y, HEX_SIZE);

        if (!CombatMap::inBounds(clicked)) return true;

        // Try attack first (click on an attackable enemy hex).
        // Engine produces events; tickAnimation() will consume them next update().
        if (m_engine.doAttackAt(clicked)) return true;

        // Try move (click on a reachable empty hex).
        // Engine produces UnitMoved event; tickAnimation() animates it.
        if (m_engine.canMoveTo(clicked))
            m_engine.doMove(clicked);

        return true;
    }

    return false;
}
