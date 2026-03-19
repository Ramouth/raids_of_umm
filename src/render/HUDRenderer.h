#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "Shader.h"

class HUDRenderer {
public:
    HUDRenderer();
    ~HUDRenderer();

    void init();
    // Must be called once per frame before any drawRect/drawText calls.
    // Binds the shader, sets screen size, disables depth test.
    void begin(int screenW, int screenH);

    void render(int screenW, int screenH,
                int day, int month, int weekOfMonth,
                int movesLeft, int movesMax,
                int visitedCount, int heroQ, int heroR,
                bool infiniteMoves = false, int gold = 0, int crystal = 0);
    void drawRect(float x, float y, float w, float h, const glm::vec4& color);
    void drawTexturedRect(float x, float y, float w, float h, GLuint texId);
    // UV-cropped variant: u0/v0 = top-left UV, u1/v1 = bottom-right UV (0..1 range).
    void drawTexturedRectUV(float x, float y, float w, float h, GLuint texId,
                            float u0, float v0, float u1, float v1);
    void drawText(float x, float y, float scale, const char* text, const glm::vec4& color);

    // Hex helpers — flat-top hex, rx/ry = half-width/half-height of bounding box.
    void drawFilledHex(float cx, float cy, float rx, float ry, const glm::vec4& color);
    void drawHexOutline(float cx, float cy, float rx, float ry, float lineW, const glm::vec4& color);
    void drawLine(float x0, float y0, float x1, float y1, float lineW, const glm::vec4& color);

    Shader m_shader;

    // Rect rendering  (4 verts, GL_TRIANGLE_FAN, 2-float positions)
    GLuint m_vao = 0;
    GLuint m_vbo = 0;

    // Textured rect (4 verts, GL_TRIANGLE_FAN, position+UV)
    GLuint m_texVao = 0;
    GLuint m_texVbo = 0;

    // Text rendering  (stb_easy_font quads → indexed triangles)
    GLuint m_textVao = 0;
    GLuint m_textVbo = 0;
    GLuint m_textIbo = 0;

    // Polygon rendering (up to 32 verts, GL_TRIANGLE_FAN, 2-float positions)
    GLuint m_polyVao = 0;
    GLuint m_polyVbo = 0;

    // Current screen size, set at the start of render() for use in drawText()
    int m_sw = 0;
    int m_sh = 0;
};
