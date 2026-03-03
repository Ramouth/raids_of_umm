#include "HUDRenderer.h"
#include "Shader.h"
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <cmath>

struct HUDVertex {
    float x, y;
};

HUDRenderer::HUDRenderer() {}

HUDRenderer::~HUDRenderer() {
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
}

void HUDRenderer::init() {
    m_shader = Shader("assets/shaders/ui.vert", "assets/shaders/ui.frag");
    
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(HUDVertex) * 4, nullptr, GL_DYNAMIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(HUDVertex), nullptr);
    
    glBindVertexArray(0);
}

void HUDRenderer::render(int screenW, int screenH, int day, int movesLeft, int movesMax, int visitedCount,
                        int heroQ, int heroR, bool infiniteMoves) {
    (void)heroQ;
    (void)heroR;
    (void)visitedCount;
    
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    m_shader.bind();
    m_shader.setVec2("u_ScreenSize", {static_cast<float>(screenW), static_cast<float>(screenH)});
    m_shader.setInt("u_UseTexture", 0);
    
    // Scale factor based on screen height
    float scale = screenH / 600.0f;
    
    float hudX = 10 * scale;
    float hudY = screenH - (110 * scale) - 10 * scale;
    float hudW = 350 * scale;
    float hudH = 100 * scale;
    
    // HUD background panel
    drawRect(hudX, hudY, hudW, hudH, {0.0f, 0.0f, 0.0f, 0.7f});
    drawRect(hudX, hudY, hudW, 3 * scale, {0.5f, 0.4f, 0.2f, 1.0f}); // Gold border top
    drawRect(hudX, hudY + hudH - 3 * scale, hudW, 3 * scale, {0.5f, 0.4f, 0.2f, 1.0f}); // Gold border bottom
    
    // ===== TOP ROW: Day + Resources =====
    float colX = hudX + 10 * scale;
    float colY = hudY + 75 * scale;
    float colW = 50 * scale;
    float colH = 22 * scale;
    
    // DAY
    drawRect(colX, colY, colW, colH, {0.3f, 0.3f, 0.35f, 1.0f});
    // Day value bar
    float dayFill = std::min(day, 10) / 10.0f;
    drawRect(colX + 2 * scale, colY + 2 * scale, (colW - 4 * scale) * dayFill, colH - 4 * scale, {0.2f, 0.8f, 1.0f, 1.0f});
    
    // Resources (Gold, Mercury, Sulfur, Crystal, Gem) - placeholder values
    float resW = 55 * scale;
    for (int i = 0; i < 5; i++) {
        float rx = colX + (colW + 5 * scale) + i * (resW + 3 * scale);
        drawRect(rx, colY, resW, colH, {0.2f, 0.15f, 0.1f, 1.0f});
    }
    
    // ===== MIDDLE ROW: Hero portrait, Moves, Luck, Morale =====
    float rowY = hudY + 48 * scale;
    float rowH = 25 * scale;
    
    // Hero portrait box
    drawRect(colX, rowY, 35 * scale, rowH, {0.3f, 0.4f, 0.5f, 1.0f});
    
    // MOVES bar
    float movesX = colX + 40 * scale;
    float movesW = 180 * scale;
    drawRect(movesX, rowY, movesW, rowH, {0.2f, 0.15f, 0.1f, 1.0f});
    float moveFill = movesMax > 0 ? static_cast<float>(movesLeft) / movesMax : 0.0f;
    drawRect(movesX + 2 * scale, rowY + 2 * scale, (movesW - 4 * scale) * moveFill, rowH - 4 * scale, {1.0f, 0.85f, 0.2f, 1.0f});
    
    // LUCK indicator
    float luckX = movesX + movesW + 8 * scale;
    drawRect(luckX, rowY, 25 * scale, rowH, {0.2f, 0.15f, 0.1f, 1.0f});
    drawRect(luckX + 3 * scale, rowY + 3 * scale, 19 * scale * 0.5f, rowH - 6 * scale, {0.2f, 0.8f, 0.3f, 1.0f});
    
    // MORALE indicator
    float moraleX = luckX + 28 * scale;
    drawRect(moraleX, rowY, 25 * scale, rowH, {0.2f, 0.15f, 0.1f, 1.0f});
    drawRect(moraleX + 3 * scale, rowY + 3 * scale, 19 * scale * 0.5f, rowH - 6 * scale, {0.8f, 0.3f, 0.3f, 1.0f});
    
    // Infinite moves indicator
    if (infiniteMoves) {
        float infX = moraleX + 30 * scale;
        drawRect(infX, rowY, 30 * scale, rowH, {1.0f, 0.0f, 0.0f, 0.8f});
    }
    
    // ===== BOTTOM ROW: Actions =====
    float actionY = hudY + 8 * scale;
    float actionH = 32 * scale;
    
    // Actions background
    drawRect(colX, actionY, hudW - 20 * scale, actionH, {0.15f, 0.12f, 0.1f, 1.0f});
    
    // Action slots (6 boxes)
    float slotW = 45 * scale;
    float slotGap = 5 * scale;
    for (int i = 0; i < 6; i++) {
        float sx = colX + 8 * scale + i * (slotW + slotGap);
        drawRect(sx, actionY + 4 * scale, slotW, actionH - 8 * scale, {0.25f, 0.2f, 0.15f, 0.8f});
    }
    
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void HUDRenderer::drawRect(float x, float y, float w, float h, const glm::vec4& color) {
    std::array<HUDVertex, 4> verts = {{
        {x,     y},
        {x + w, y},
        {x + w, y + h},
        {x,     y + h}
    }};
    
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts.data());
    
    m_shader.setVec4("u_Color", color);
    
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}
