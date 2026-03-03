#pragma once
#include <glm/glm.hpp>

/*
 * Camera2D — orthographic overhead camera for hex-map views.
 *
 * Despite the name, the projection is genuinely 3D (perspective lookAt +
 * orthographic clip) to give the slight isometric angle seen on the map.
 * "2D" refers to the game-world plane being navigated (the XZ plane, Y=0).
 *
 * Reusable by any state that shows a hex map:
 *   AdventureState, WorldBuilderState, CombatState (with its own instance).
 *
 * Thread-safety: none — call only from the render thread.
 */
class Camera2D {
public:
    Camera2D() = default;
    Camera2D(int vpW, int vpH);

    // ── Mutation ─────────────────────────────────────────────────────────────

    // Pan the camera centre by (dx, dz) in world units.
    void pan(float dx, float dz);

    // Set camera centre to an absolute world position.
    void setPosition(glm::vec2 worldPos);

    // Zoom level = half-height of visible world area.
    // Clamped to [MIN_ZOOM, MAX_ZOOM].
    void setZoom(float zoom);

    // Adjust zoom by a delta (positive = zoom in, negative = zoom out).
    void adjustZoom(float delta);

    // Update viewport dimensions (call on SDL_WINDOWEVENT_RESIZED).
    void resize(int vpW, int vpH);

    // ── Query ─────────────────────────────────────────────────────────────────

    glm::vec2 position() const { return m_pos; }
    float     zoom()     const { return m_zoom; }
    int       vpWidth()  const { return m_vpW; }
    int       vpHeight() const { return m_vpH; }

    // ── Matrices ──────────────────────────────────────────────────────────────

    glm::mat4 viewMatrix()     const;
    glm::mat4 projMatrix()     const;
    glm::mat4 viewProjMatrix() const;

    // ── Picking ───────────────────────────────────────────────────────────────

    // Convert a screen pixel to a world XZ position (Y=0 plane).
    // px, py are in window pixels (top-left origin, as SDL gives them).
    glm::vec2 screenToWorld(int px, int py) const;

    // ── Constants ─────────────────────────────────────────────────────────────

    static constexpr float MIN_ZOOM  = 3.0f;
    static constexpr float MAX_ZOOM  = 40.0f;
    static constexpr float ZOOM_STEP = 1.5f;  // default scroll increment

private:
    glm::vec2 m_pos  { 0.0f, 0.0f };
    float     m_zoom { 8.0f };
    int       m_vpW  { 1280 };
    int       m_vpH  { 720  };
};
