#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "Shader.h"

class HUDRenderer {
public:
    HUDRenderer();
    ~HUDRenderer();

    void init();
    void render(int screenW, int screenH, int day, int movesLeft, int movesMax, int visitedCount, 
                int heroQ, int heroR, bool infiniteMoves = false);

private:
    void drawRect(float x, float y, float w, float h, const glm::vec4& color);

    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    Shader m_shader;
};
