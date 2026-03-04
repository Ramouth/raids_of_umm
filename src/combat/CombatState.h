#pragma once
#include "core/StateMachine.h"
#include "CombatEngine.h"
#include "CombatRenderer.h"
#include "CombatMap.h"
#include "render/Camera2D.h"
#include "hex/HexCoord.h"
#include <glm/glm.hpp>

/*
 * CombatState — thin coordinator for the combat screen.
 *
 * Owns:
 *   CombatEngine   — pure turn logic
 *   CombatRenderer — GL rendering
 *   Camera2D       — isometric combat viewport
 *
 * Nothing game-logic-related lives here — it belongs in CombatEngine.
 * Nothing render-related lives here — it belongs in CombatRenderer.
 *
 * Controls:
 *   Arrow keys / WASD   — pan camera
 *   Mouse move          — hover hex highlight
 *   Enter               — attack first enemy stack (stub)
 *   Space               — defend (pass turn)
 *   Escape              — retreat
 */
class CombatState final : public GameState {
public:
    explicit CombatState(CombatArmy player, CombatArmy enemy);

    void onEnter() override;
    void onExit()  override;
    void update(float dt) override;
    void render()  override;
    bool handleEvent(void* sdlEvent) override;

private:
    CombatEngine   m_engine;
    CombatRenderer m_renderer;
    Camera2D       m_cam;

    HexCoord m_hoveredHex;
    bool     m_hasHovered = false;

    // Camera pan key state
    bool m_panUp    = false;
    bool m_panDown  = false;
    bool m_panLeft  = false;
    bool m_panRight = false;

    // Movement animation
    UnitAnimState m_anim;
    glm::vec3     m_animFrom      = { 0.f, 0.f, 0.f };
    glm::vec3     m_animTo        = { 0.f, 0.f, 0.f };
    float         m_animProgress  = 0.f;  // 0..1

    static constexpr float CAM_SPEED          = 8.0f;
    static constexpr float HEX_SIZE           = 1.0f;
    static constexpr float MOVE_ANIM_DURATION = 0.30f;
};
