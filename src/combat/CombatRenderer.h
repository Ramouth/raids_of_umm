#pragma once
#include "CombatEngine.h"
#include "CombatMap.h"
#include "render/HexRenderer.h"
#include "render/SpriteRenderer.h"
#include "render/Camera2D.h"
#include "render/Shader.h"
#include "hex/HexCoord.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

/*
 * UnitAnimState — transient render-only data for smooth unit movement.
 *
 * When active, the animated unit's sprite is drawn at worldPos instead of
 * its logical hex centre.  All other units are drawn at their hex centres.
 */
struct UnitAnimState {
    bool       active    = false;
    bool       isPlayer  = false;   // which army's unit is moving
    int        stackIndex = 0;
    glm::vec3  worldPos  = { 0.f, 0.f, 0.f };  // interpolated world position
};

/*
 * CombatRenderer — GL 3.3 Core rendering for the combat battlefield.
 *
 * Draws:
 *   - 11×5 hex grid with zone tinting (player / neutral / enemy)
 *   - Reachable tile highlights (blue-green)
 *   - Attackable tile highlights (red)
 *   - Active-unit and hover outlines
 *   - Unit sprites billboarded above their hex
 *   - 2D action panel at the bottom of the screen
 *
 * Reads all game state from const CombatEngine& — no game logic here.
 * Camera (view/proj) is owned by CombatState and passed in each frame.
 *
 * Non-copyable (owns GL objects).
 */
class CombatRenderer {
public:
    CombatRenderer()  = default;
    ~CombatRenderer();

    CombatRenderer(const CombatRenderer&)            = delete;
    CombatRenderer& operator=(const CombatRenderer&) = delete;

    // Call once after the GL context is ready (from CombatState::onEnter).
    void init();

    // Draw the full combat screen each frame.
    void render(int screenW, int screenH,
                const CombatEngine& engine,
                const Camera2D& cam,
                HexCoord hoveredHex,
                bool     hasHovered,
                const UnitAnimState& anim);

private:
    // Decide tile colour: zone tint + highlight overlays.
    glm::vec3 tileColor(HexCoord coord,
                        const std::vector<HexCoord>& reachable,
                        const std::vector<HexCoord>& attackable) const;

    // 2D screen-space panel for action buttons.
    void renderActionPanel(int screenW, int screenH);
    void drawRect2D(float x, float y, float w, float h, const glm::vec4& color);

    // Returns the sprite for a unit, falling back to a default if not found.
    SpriteRenderer* spriteFor(const CombatUnit& unit);

    // ── 3D hex + sprite rendering ─────────────────────────────────────────────
    HexRenderer m_hexRenderer;

    // Per-unit-type sprites keyed by unit id or SC id.
    std::unordered_map<std::string, std::unique_ptr<SpriteRenderer>> m_sprites;
    SpriteRenderer m_fallbackPlayerSprite;  // used when unit id has no dedicated sprite
    SpriteRenderer m_fallbackEnemySprite;

    // ── 2D UI overlay ─────────────────────────────────────────────────────────
    GLuint m_uiVao = 0;
    GLuint m_uiVbo = 0;
    Shader m_uiShader;

    bool m_ready = false;

    static constexpr float HEX_SIZE = 1.0f;
};
