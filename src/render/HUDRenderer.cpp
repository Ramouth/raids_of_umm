#include "HUDRenderer.h"
#include "Shader.h"
#include "stb_easy_font.h"
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <vector>

struct HUDVertex {
    float x, y;
};

HUDRenderer::HUDRenderer() {}

HUDRenderer::~HUDRenderer() {
    if (m_vao)     glDeleteVertexArrays(1, &m_vao);
    if (m_vbo)     glDeleteBuffers(1, &m_vbo);
    if (m_textVao) glDeleteVertexArrays(1, &m_textVao);
    if (m_textVbo) glDeleteBuffers(1, &m_textVbo);
    if (m_textIbo) glDeleteBuffers(1, &m_textIbo);
}

void HUDRenderer::init() {
    m_shader = Shader("assets/shaders/ui.vert", "assets/shaders/ui.frag");

    // ── Rect VAO (4 verts, GL_TRIANGLE_FAN) ──────────────────────────────────
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(HUDVertex) * 4, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(HUDVertex), nullptr);
    glBindVertexArray(0);

    // ── Text VAO (stb_easy_font quads → pre-built index buffer) ─────────────
    //   stb vertex layout: float x, y, z, uint8 r,g,b,a  — 16 bytes per vert
    //   We only read x,y (location 0, 2 floats, stride 16).
    constexpr int MAX_TEXT_QUADS = 2048;

    glGenVertexArrays(1, &m_textVao);
    glGenBuffers(1, &m_textVbo);
    glGenBuffers(1, &m_textIbo);

    glBindVertexArray(m_textVao);

    glBindBuffer(GL_ARRAY_BUFFER, m_textVbo);
    glBufferData(GL_ARRAY_BUFFER, MAX_TEXT_QUADS * 4 * 16, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, nullptr); // stride 16, x,y at offset 0

    // Build index buffer: two triangles per quad
    std::vector<unsigned int> idx;
    idx.reserve(MAX_TEXT_QUADS * 6);
    for (int i = 0; i < MAX_TEXT_QUADS; ++i) {
        unsigned int b = static_cast<unsigned int>(i) * 4;
        idx.push_back(b+0); idx.push_back(b+1); idx.push_back(b+2);
        idx.push_back(b+0); idx.push_back(b+2); idx.push_back(b+3);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_textIbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(idx.size() * sizeof(unsigned int)),
                 idx.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
}

// ── drawRect ─────────────────────────────────────────────────────────────────

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

// ── drawText ─────────────────────────────────────────────────────────────────
//   x, y  — pixel position of text top-left (y-down)
//   scale — multiply font pixel size (stb chars are ~6px wide, 9px tall at scale=1)

void HUDRenderer::drawText(float x, float y, float scale,
                            const char* text, const glm::vec4& color) {
    // stb_easy_font_print expects a mutable char* (it doesn't actually mutate)
    static char buf[1 << 15]; // 32 KB — handles ~120 characters comfortably
    int quads = stb_easy_font_print(0, 0,
                                    const_cast<char*>(text),
                                    nullptr, buf, sizeof(buf));
    if (quads <= 0) return;

    // Apply scale and pixel offset to every vertex position in-place.
    // stb layout: float x (0), float y (4), float z (8), uint8×4 (12) — 16 bytes/vert.
    for (int v = 0, n = quads * 4; v < n; ++v) {
        float* p = reinterpret_cast<float*>(buf + v * 16);
        p[0] = p[0] * scale + x;
        p[1] = p[1] * scale + y;
    }

    glBindVertexArray(m_textVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_textVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, quads * 4 * 16, buf);

    m_shader.setVec2("u_ScreenSize", {(float)m_sw, (float)m_sh});
    m_shader.setVec4("u_Color", color);
    m_shader.setInt("u_UseTexture", 0);

    glDrawElements(GL_TRIANGLES, quads * 6, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

// ── render ────────────────────────────────────────────────────────────────────

void HUDRenderer::render(int screenW, int screenH,
                          int day, int movesLeft, int movesMax,
                          int visitedCount, int heroQ, int heroR,
                          bool infiniteMoves) {
    m_sw = screenW;
    m_sh = screenH;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);          // 2D quads wind CW in NDC after y-flip
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_shader.bind();
    m_shader.setVec2("u_ScreenSize", {(float)screenW, (float)screenH});
    m_shader.setInt("u_UseTexture", 0);

    float scale = screenH / 600.0f;

    float hudX = 10 * scale;
    float hudY = screenH - (110 * scale) - 10 * scale;
    float hudW = 350 * scale;
    float hudH = 100 * scale;

    // ── Background panel ─────────────────────────────────────────────────────
    drawRect(hudX, hudY, hudW, hudH, {0.0f, 0.0f, 0.0f, 0.7f});
    drawRect(hudX, hudY, hudW, 3 * scale, {0.5f, 0.4f, 0.2f, 1.0f});
    drawRect(hudX, hudY + hudH - 3 * scale, hudW, 3 * scale, {0.5f, 0.4f, 0.2f, 1.0f});

    // ── Moves bar (middle of panel) ───────────────────────────────────────────
    float movesBarX = hudX + 12 * scale;
    float movesBarY = hudY + 42 * scale;
    float movesBarW = hudW - 24 * scale;
    float movesBarH = 20 * scale;

    drawRect(movesBarX, movesBarY, movesBarW, movesBarH, {0.15f, 0.12f, 0.10f, 1.0f});
    float fill = movesMax > 0 ? static_cast<float>(movesLeft) / movesMax : 0.0f;
    glm::vec4 barColor = (movesLeft > movesMax / 2)
        ? glm::vec4{0.2f, 0.85f, 0.3f, 1.0f}
        : (movesLeft > 0)
            ? glm::vec4{1.0f, 0.75f, 0.1f, 1.0f}
            : glm::vec4{0.8f, 0.2f, 0.2f, 1.0f};
    if (infiniteMoves) barColor = {0.3f, 0.6f, 1.0f, 1.0f};
    drawRect(movesBarX + 2 * scale, movesBarY + 2 * scale,
             (movesBarW - 4 * scale) * (infiniteMoves ? 1.0f : fill),
             movesBarH - 4 * scale, barColor);

    // ── Day pip row (top of panel) ────────────────────────────────────────────
    //   Seven squares, filled for days 1–7 (current week).
    float pipY  = hudY + 72 * scale;
    float pipSz = 14 * scale;
    float pipGap = 4 * scale;
    int dayInWeek = ((day - 1) % 7) + 1;
    for (int i = 0; i < 7; ++i) {
        float px = hudX + 12 * scale + i * (pipSz + pipGap);
        glm::vec4 pipCol = (i < dayInWeek)
            ? glm::vec4{0.9f, 0.7f, 0.1f, 1.0f}
            : glm::vec4{0.25f, 0.22f, 0.18f, 1.0f};
        drawRect(px, pipY, pipSz, pipSz, pipCol);
    }

    // ── Text info strip above the panel ──────────────────────────────────────
    //   Two lines: row 1 = day/moves, row 2 = position/visited/mode
    //   textScale: stb chars are ~9px tall; at textScale=scale*1.5 → ~14px on a 600px screen.
    float ts  = scale * 1.5f;
    float tx  = hudX + 4 * scale;
    float ty1 = hudY - 32 * scale;  // first line above panel
    float ty2 = hudY - 16 * scale;  // second line above panel

    // Build line 1: "DAY 5   WEEK 1   MOVES 3/8"
    char line1[64];
    int week = ((day - 1) / 7) + 1;
    if (infiniteMoves)
        std::snprintf(line1, sizeof(line1), "DAY %d   WEEK %d   MOVES INF", day, week);
    else
        std::snprintf(line1, sizeof(line1), "DAY %d   WEEK %d   MOVES %d/%d",
                      day, week, movesLeft, movesMax);

    // Build line 2: "POS (q,r)   VISITED n"
    char line2[64];
    std::snprintf(line2, sizeof(line2), "POS (%d, %d)   VISITED %d",
                  heroQ, heroR, visitedCount);

    // Text background band
    float bandH = 36 * scale;
    drawRect(hudX, hudY - bandH - 2 * scale, hudW, bandH, {0.0f, 0.0f, 0.0f, 0.55f});

    // Draw the two text lines
    glm::vec4 textCol  = {1.0f, 0.95f, 0.7f, 1.0f};  // warm white
    glm::vec4 warnCol  = {1.0f, 0.35f, 0.2f, 1.0f};  // orange-red for INF

    drawText(tx, ty1, ts, line1, infiniteMoves ? warnCol : textCol);
    drawText(tx, ty2, ts, line2, {0.75f, 0.85f, 0.95f, 1.0f});

    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}
