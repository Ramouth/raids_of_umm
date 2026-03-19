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
    if (m_polyVao) glDeleteVertexArrays(1, &m_polyVao);
    if (m_polyVbo) glDeleteBuffers(1, &m_polyVbo);
    if (m_vao)     glDeleteVertexArrays(1, &m_vao);
    if (m_vbo)     glDeleteBuffers(1, &m_vbo);
    if (m_texVao)  glDeleteVertexArrays(1, &m_texVao);
    if (m_texVbo)  glDeleteBuffers(1, &m_texVbo);
    if (m_textVao) glDeleteVertexArrays(1, &m_textVao);
    if (m_textVbo) glDeleteBuffers(1, &m_textVbo);
    if (m_textIbo) glDeleteBuffers(1, &m_textIbo);
}

void HUDRenderer::init() {
    m_shader = Shader("assets/shaders/ui.vert", "assets/shaders/ui.frag");

    // ── Rect VAO (4 verts, GL_TRIANGLE_FAN, position only) ───────────────────
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(HUDVertex) * 4, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(HUDVertex), nullptr);
    glBindVertexArray(0);

    // ── Textured rect VAO (4 verts, GL_TRIANGLE_FAN, position + UV) ──────────
    struct TexVert { float x, y, u, v; };
    glGenVertexArrays(1, &m_texVao);
    glGenBuffers(1, &m_texVbo);
    glBindVertexArray(m_texVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_texVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(TexVert) * 4, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(TexVert),
                          reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TexVert),
                          reinterpret_cast<void*>(sizeof(float) * 2));
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

    // ── Polygon VAO (up to 32 verts, GL_TRIANGLE_FAN, 2-float positions) ─────
    glGenVertexArrays(1, &m_polyVao);
    glGenBuffers(1, &m_polyVbo);
    glBindVertexArray(m_polyVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_polyVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(HUDVertex) * 32, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(HUDVertex), nullptr);
    glBindVertexArray(0);
}

// ── begin ────────────────────────────────────────────────────────────────────

void HUDRenderer::begin(int sw, int sh) {
    m_sw = sw;
    m_sh = sh;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    m_shader.bind();
    m_shader.setVec2("u_ScreenSize", {(float)sw, (float)sh});
    m_shader.setInt("u_UseTexture", 0);
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

// ── drawTexturedRect ─────────────────────────────────────────────────────────
// Draws a textured quad in pixel space. The texture is assumed to be stored
// row-0-at-top (as produced by loadTexturePNG), so UV v=0 → top of image.

void HUDRenderer::drawTexturedRect(float x, float y, float w, float h, GLuint texId) {
    struct TexVert { float x, y, u, v; };
    // v=0 at top-left, v=1 at bottom-right (matches row-0-at-top GL upload)
    const std::array<TexVert, 4> verts = {{
        {x,     y,     0.0f, 0.0f},
        {x + w, y,     1.0f, 0.0f},
        {x + w, y + h, 1.0f, 1.0f},
        {x,     y + h, 0.0f, 1.0f},
    }};

    glBindVertexArray(m_texVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_texVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts.data());

    m_shader.setInt("u_UseTexture", 1);
    m_shader.setVec4("u_Color", {1.0f, 1.0f, 1.0f, 1.0f});  // white: no tint
    m_shader.setInt("u_Texture", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texId);

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
    m_shader.setInt("u_UseTexture", 0);
}

void HUDRenderer::drawTexturedRectUV(float x, float y, float w, float h,
                                      GLuint texId,
                                      float u0, float v0, float u1, float v1) {
    struct TexVert { float x, y, u, v; };
    const std::array<TexVert, 4> verts = {{
        {x,     y,     u0, v0},
        {x + w, y,     u1, v0},
        {x + w, y + h, u1, v1},
        {x,     y + h, u0, v1},
    }};
    glBindVertexArray(m_texVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_texVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts.data());
    m_shader.setInt("u_UseTexture", 1);
    m_shader.setVec4("u_Color", {1.0f, 1.0f, 1.0f, 1.0f});
    m_shader.setInt("u_Texture", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texId);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
    m_shader.setInt("u_UseTexture", 0);
}

// ── drawLine ─────────────────────────────────────────────────────────────────
// Draws a thick line as a rotated quad.

void HUDRenderer::drawLine(float x0, float y0, float x1, float y1,
                            float lineW, const glm::vec4& color) {
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.001f) return;
    float nx = -dy / len * (lineW * 0.5f);
    float ny =  dx / len * (lineW * 0.5f);

    std::array<HUDVertex, 4> v = {{
        {x0 + nx, y0 + ny},
        {x1 + nx, y1 + ny},
        {x1 - nx, y1 - ny},
        {x0 - nx, y0 - ny},
    }};
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v.data());
    m_shader.setVec4("u_Color", color);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

// ── drawFilledHex / drawHexOutline ───────────────────────────────────────────
// Flat-top hexagon. rx = half-width, ry = half-height of bounding box.
// Vertices at angles 0°, 60°, 120°, 180°, 240°, 300°.

// Bounding-box-filling flat-top hex.
// Vertices sit exactly on the bounding box edges so adjacent cells tile
// without gaps: right/left points at ±rx mid-height, top/bottom flat edges
// span ±rx/2 at ±ry.  The tiling condition: right-vertex of cell (c,r)
// coincides with left-vertex of cell (c+1,r), and top/bottom flat edges of
// adjacent rows share a common y coordinate.
static void hexVerts(float cx, float cy, float rx, float ry,
                     std::array<HUDVertex, 8>& v) {
    float hx = rx * 0.5f;
    v[0] = {cx,      cy};        // centre (for filled triangle fan)
    v[1] = {cx + rx, cy};        // right
    v[2] = {cx + hx, cy + ry};  // bottom-right
    v[3] = {cx - hx, cy + ry};  // bottom-left
    v[4] = {cx - rx, cy};        // left
    v[5] = {cx - hx, cy - ry};  // top-left
    v[6] = {cx + hx, cy - ry};  // top-right
    v[7] = v[1];                 // close the fan
}

void HUDRenderer::drawFilledHex(float cx, float cy, float rx, float ry,
                                  const glm::vec4& color) {
    std::array<HUDVertex, 8> v;
    hexVerts(cx, cy, rx, ry, v);
    glBindVertexArray(m_polyVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_polyVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v.data());
    m_shader.setVec4("u_Color", color);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 8);
}

void HUDRenderer::drawHexOutline(float cx, float cy, float rx, float ry,
                                   float lineW, const glm::vec4& color) {
    std::array<HUDVertex, 8> v;
    hexVerts(cx, cy, rx, ry, v);
    // v[1]..v[6] are the 6 corners
    for (int i = 1; i <= 6; ++i) {
        int j = (i % 6) + 1;
        drawLine(v[i].x, v[i].y, v[j].x, v[j].y, lineW, color);
    }
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
                          int day, int month, int weekOfMonth,
                          int movesLeft, int movesMax,
                          int visitedCount, int heroQ, int heroR,
                          bool infiniteMoves, int gold, int crystal) {
    m_sw = screenW;
    m_sh = screenH;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);          // 2D quads wind CW in NDC after y-flip
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_shader.bind();
    m_shader.setVec2("u_ScreenSize", {(float)screenW, (float)screenH});
    m_shader.setInt("u_UseTexture", 0);

    float sc = screenH / 600.0f;

    float hudX = 10 * sc;
    float hudY = screenH - (120 * sc) - 10 * sc;  // slightly taller for week pips
    float hudW = 350 * sc;
    float hudH = 110 * sc;

    // ── Background panel ─────────────────────────────────────────────────────
    drawRect(hudX, hudY, hudW, hudH, {0.0f, 0.0f, 0.0f, 0.7f});
    drawRect(hudX, hudY, hudW, 3 * sc, {0.5f, 0.4f, 0.2f, 1.0f});
    drawRect(hudX, hudY + hudH - 3 * sc, hudW, 3 * sc, {0.5f, 0.4f, 0.2f, 1.0f});

    // ── Moves bar ────────────────────────────────────────────────────────────
    float movesBarX = hudX + 12 * sc;
    float movesBarY = hudY + 48 * sc;
    float movesBarW = hudW - 24 * sc;
    float movesBarH = 20 * sc;

    drawRect(movesBarX, movesBarY, movesBarW, movesBarH, {0.15f, 0.12f, 0.10f, 1.0f});
    float fill = movesMax > 0 ? static_cast<float>(movesLeft) / movesMax : 0.0f;
    glm::vec4 barColor = (movesLeft > movesMax / 2)
        ? glm::vec4{0.2f, 0.85f, 0.3f, 1.0f}
        : (movesLeft > 0)
            ? glm::vec4{1.0f, 0.75f, 0.1f, 1.0f}
            : glm::vec4{0.8f, 0.2f, 0.2f, 1.0f};
    if (infiniteMoves) barColor = {0.3f, 0.6f, 1.0f, 1.0f};
    drawRect(movesBarX + 2 * sc, movesBarY + 2 * sc,
             (movesBarW - 4 * sc) * (infiniteMoves ? 1.0f : fill),
             movesBarH - 4 * sc, barColor);

    // ── Week-of-month pips (4 squares) ───────────────────────────────────────
    //   Row above the day pips. Filled squares = weeks elapsed in the month.
    float weekPipY  = hudY + 76 * sc;
    float weekPipSz = 10 * sc;
    float weekPipGap = 4 * sc;
    for (int i = 0; i < 4; ++i) {
        float px = hudX + 12 * sc + i * (weekPipSz + weekPipGap);
        glm::vec4 col = (i < weekOfMonth)
            ? glm::vec4{0.6f, 0.45f, 0.1f, 1.0f}   // filled — warm amber
            : glm::vec4{0.20f, 0.18f, 0.14f, 1.0f}; // empty — dark
        drawRect(px, weekPipY, weekPipSz, weekPipSz, col);
    }

    // ── Day-of-week pips (7 squares) ─────────────────────────────────────────
    float dayPipY  = hudY + 90 * sc;
    float dayPipSz = 14 * sc;
    float dayPipGap = 4 * sc;
    int dayInWeek = ((day - 1) % 7) + 1;
    for (int i = 0; i < 7; ++i) {
        float px = hudX + 12 * sc + i * (dayPipSz + dayPipGap);
        glm::vec4 col = (i < dayInWeek)
            ? glm::vec4{0.9f, 0.7f, 0.1f, 1.0f}    // filled — gold
            : glm::vec4{0.25f, 0.22f, 0.18f, 1.0f}; // empty
        drawRect(px, dayPipY, dayPipSz, dayPipSz, col);
    }

    // ── Text strip above panel (3 rows) ──────────────────────────────────────
    float ts  = sc * 1.5f;
    float tx  = hudX + 4 * sc;
    float ty1 = hudY - 44 * sc;   // Month · Week · Day
    float ty2 = hudY - 28 * sc;   // Moves
    float ty3 = hudY - 12 * sc;   // Position / Visited / Gold

    char line1[80], line2[64], line3[64], line4[80];

    // "MONTH 2  WEEK 3  DAY 5"
    std::snprintf(line1, sizeof(line1), "MONTH %d   WEEK %d   DAY %d",
                  month, weekOfMonth, dayInWeek);

    // "MOVES 3/8" or "MOVES INF"
    if (infiniteMoves)
        std::snprintf(line2, sizeof(line2), "MOVES INF");
    else
        std::snprintf(line2, sizeof(line2), "MOVES %d/%d", movesLeft, movesMax);

    // "POS (q,r)  VISITED n"
    std::snprintf(line3, sizeof(line3), "POS (%d,%d)   VISITED %d",
                  heroQ, heroR, visitedCount);

    // "GOLD n  CRYSTAL n"
    std::snprintf(line4, sizeof(line4), "GOLD %d   CRYSTAL %d", gold, crystal);

    // Text background band — tall enough for four rows
    float bandH = 66 * sc;
    float ty4   = hudY - 24 * sc;
    drawRect(hudX, hudY - bandH - 2 * sc, hudW, bandH, {0.0f, 0.0f, 0.0f, 0.55f});

    glm::vec4 textCol = {1.0f, 0.95f, 0.7f, 1.0f};
    glm::vec4 warnCol = {1.0f, 0.35f, 0.2f, 1.0f};
    glm::vec4 resCol  = {0.75f, 0.85f, 0.95f, 1.0f};
    drawText(tx, ty1, ts, line1, textCol);
    drawText(tx, ty2, ts, line2, infiniteMoves ? warnCol : textCol);
    drawText(tx, ty3, ts, line3, resCol);
    drawText(tx, ty4, ts, line4, {0.95f, 0.82f, 0.30f, 1.0f});

    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}
