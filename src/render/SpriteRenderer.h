#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include "Shader.h"

/*
 * SpriteRenderer — draws a single textured billboard sprite in world space.
 *
 * The quad is oriented using the view matrix so it always faces the camera
 * regardless of pan/zoom. Alpha cutout is handled in the shader.
 *
 * Usage:
 *   renderer.init();
 *   renderer.loadSprite("assets/textures/units/rider_lance.png");
 *   // per frame:
 *   glEnable(GL_BLEND);
 *   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
 *   renderer.draw(worldPos, hexSize, view, proj);
 *   glDisable(GL_BLEND);
 */
class SpriteRenderer {
public:
    SpriteRenderer()  = default;
    ~SpriteRenderer();

    SpriteRenderer(const SpriteRenderer&)            = delete;
    SpriteRenderer& operator=(const SpriteRenderer&) = delete;

    void init();
    void loadSprite(const std::string& pngPath);

    // worldPos — base (bottom-centre) of the sprite in world space
    // size     — width and height in world units (square)
    void draw(const glm::vec3& worldPos, float size,
              const glm::mat4& view, const glm::mat4& proj);

    // Non-square variant: separate world-space width and height
    void draw(const glm::vec3& worldPos, float worldWidth, float worldHeight,
              const glm::mat4& view, const glm::mat4& proj);

    // Animation: select a frame from a horizontal spritesheet before draw().
    // Defaults are frame=0, numFrames=1 (full texture = single static sprite).
    void setFrame(int frame, int numFrames) {
        m_frame     = frame;
        m_numFrames = numFrames;
    }

private:
    void buildQuad();

    Shader m_shader;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ibo = 0;
    GLuint m_tex = 0;

    int m_frame     = 0;
    int m_numFrames = 1;
};
