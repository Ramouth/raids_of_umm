#include "Camera2D.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <algorithm>
#include <cmath>

Camera2D::Camera2D(int vpW, int vpH)
    : m_vpW(vpW), m_vpH(vpH) {}

// ── Mutation ──────────────────────────────────────────────────────────────────

void Camera2D::pan(float dx, float dz) {
    m_pos.x += dx;
    m_pos.y += dz;
}

void Camera2D::setPosition(glm::vec2 worldPos) {
    m_pos = worldPos;
}

void Camera2D::setZoom(float zoom) {
    m_zoom = std::clamp(zoom, MIN_ZOOM, MAX_ZOOM);
}

void Camera2D::adjustZoom(float delta) {
    setZoom(m_zoom + delta);
}

void Camera2D::resize(int vpW, int vpH) {
    m_vpW = vpW;
    m_vpH = vpH;
}

// ── Matrices ──────────────────────────────────────────────────────────────────

glm::mat4 Camera2D::viewMatrix() const {
    // Angled overhead view: eye is raised and slightly behind the centre.
    // This gives the classic strategy-game isometric feel without a true
    // isometric projection.
    float eyeX = m_pos.x;
    float eyeZ = m_pos.y + m_zoom * 0.5f;
    float eyeY = m_zoom * 1.8f;
    return glm::lookAt(
        glm::vec3(eyeX, eyeY, eyeZ),           // eye
        glm::vec3(m_pos.x, 0.0f, m_pos.y),     // centre of interest (world XZ)
        glm::vec3(0.0f, 0.0f, -1.0f)           // up
    );
}

glm::mat4 Camera2D::projMatrix() const {
    float aspect = static_cast<float>(m_vpW) / static_cast<float>(m_vpH);
    float hw = m_zoom * aspect;
    float hh = m_zoom;
    return glm::ortho(-hw, hw, -hh, hh, 0.1f, 200.0f);
}

glm::mat4 Camera2D::viewProjMatrix() const {
    return projMatrix() * viewMatrix();
}

// ── Picking ───────────────────────────────────────────────────────────────────

glm::vec2 Camera2D::screenToWorld(int px, int py) const {
    // Convert pixel to NDC.
    float ndcX =  (static_cast<float>(px) / static_cast<float>(m_vpW)) * 2.0f - 1.0f;
    float ndcY = -(static_cast<float>(py) / static_cast<float>(m_vpH)) * 2.0f + 1.0f;

    // Unproject two points on the near/far planes to get a world-space ray.
    glm::mat4 invVP = glm::inverse(viewProjMatrix());
    glm::vec4 near4 = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 far4  = invVP * glm::vec4(ndcX, ndcY,  1.0f, 1.0f);
    glm::vec3 rayO  = glm::vec3(near4) / near4.w;
    glm::vec3 rayD  = glm::normalize(glm::vec3(far4) / far4.w - rayO);

    // Intersect ray with the Y=0 world plane.
    if (std::abs(rayD.y) < 1e-6f)
        return { rayO.x, rayO.z };   // ray parallel to plane — return ray origin

    float t   = -rayO.y / rayD.y;
    glm::vec3 hit = rayO + rayD * t;
    return { hit.x, hit.z };
}
