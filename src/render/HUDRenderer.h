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
    void drawText(float x, float y, float scale, const char* text, const glm::vec4& color);

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

    // Current screen size, set at the start of render() for use in drawText()
    int m_sw = 0;
    int m_sh = 0;
};
