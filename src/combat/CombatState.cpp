#include "CombatState.h"
#include "core/Application.h"
#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <iostream>
#include <cmath>

CombatState::CombatState(CombatArmy player, CombatArmy enemy)
    : m_engine(std::move(player), std::move(enemy))
{}

void CombatState::onEnter() {
    m_renderer.init();

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
}

void CombatState::update(float dt) {
    // Deferred dismiss — safe to pop here because StateMachine sets m_processing=true
    // during update(), so pop() enqueues rather than destroying the object mid-call.
    if (m_wantsDismiss) {
        Application::get().popState();
        return;
    }

    // Battle over — wait for player to press any key / click to dismiss.
    if (m_engine.isOver()) return;

    // Simple enemy AI: auto-advance non-player turns.
    // Finds the first living player stack and attacks it.
    if (!m_anim.active && !m_engine.currentTurn().isPlayer) {
        const auto& targets = m_engine.playerArmy().stacks;
        for (int i = 0; i < static_cast<int>(targets.size()); ++i) {
            if (!targets[i].isDead()) { m_engine.doAttack(i); break; }
        }
        return;
    }

    // Advance move animation
    if (m_anim.active) {
        m_animProgress += dt / MOVE_ANIM_DURATION;
        if (m_animProgress >= 1.0f) {
            m_animProgress = 1.0f;
            m_anim.active  = false;
        }
        float t = m_animProgress;
        // Smooth-step interpolation
        t = t * t * (3.f - 2.f * t);
        m_anim.worldPos = m_animFrom + t * (m_animTo - m_animFrom);
    }

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
}

bool CombatState::handleEvent(void* sdlEvent) {
    const SDL_Event* e = static_cast<SDL_Event*>(sdlEvent);

    // ── Battle-over dismissal ─────────────────────────────────────────────────
    // When the battle is decided, any key press or left-click queues a dismiss.
    // We never call popState() directly here — see m_wantsDismiss comment in .h.
    if (m_engine.isOver()) {
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

            // Combat actions — only valid on the player's turn.
            // Enemy turns are auto-advanced in update() instead.
            case SDLK_RETURN: case SDLK_KP_ENTER:
                if (!m_anim.active && m_engine.currentTurn().isPlayer)
                    m_engine.doAttack(0);
                return true;
            case SDLK_SPACE:
                if (!m_anim.active && m_engine.currentTurn().isPlayer)
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

    // ── Left click → move or attack (player's turn only) ─────────────────────
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        if (m_anim.active || !m_engine.currentTurn().isPlayer) return true;

        glm::vec2 world = m_cam.screenToWorld(
            static_cast<float>(e->button.x),
            static_cast<float>(e->button.y));
        HexCoord clicked = HexCoord::fromWorld(world.x, world.y, HEX_SIZE);

        if (!CombatMap::inBounds(clicked)) return true;

        // Try attack first (click on an attackable enemy hex)
        if (m_engine.doAttackAt(clicked)) return true;

        // Try move (click on a reachable empty hex)
        if (m_engine.canMoveTo(clicked)) {
            // Identify which stack is moving for animation
            const TurnSlot& slot = m_engine.currentTurn();
            m_anim.isPlayer   = slot.isPlayer;
            m_anim.stackIndex = slot.stackIndex;

            // World positions for interpolation
            const CombatUnit& actor = m_engine.activeUnit();
            float fx, fz, tx, tz;
            actor.pos.toWorld(HEX_SIZE, fx, fz);
            clicked.toWorld(HEX_SIZE, tx, tz);
            m_animFrom     = { fx, 0.f, fz };
            m_animTo       = { tx, 0.f, tz };
            m_animProgress = 0.f;
            m_anim.worldPos = m_animFrom;
            m_anim.active  = true;

            m_engine.doMove(clicked);  // updates logical pos + advances turn
        }
        return true;
    }

    return false;
}
