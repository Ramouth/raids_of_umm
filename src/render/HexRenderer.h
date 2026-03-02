#pragma once
#include "Shader.h"
#include "hex/HexCoord.h"
#include <glad/glad.h>
#include <glm/glm.hpp>

/*
 * HexRenderer — draws flat-top hex tiles using OpenGL 3.3.
 *
 * All tiles share one VAO (unit-radius hex mesh). Each tile is drawn with a
 * per-draw-call model matrix and colour — simple enough for hundreds of tiles.
 *
 * Usage per frame:
 *   renderer.beginFrame(viewProj, sunDir, sunColor, ambientColor);
 *   renderer.drawTile(coord, color, hexSize, heightOffset);
 *   renderer.endFrame();
 */
class HexRenderer {
public:
    HexRenderer() = default;
    ~HexRenderer();

    HexRenderer(const HexRenderer&) = delete;
    HexRenderer& operator=(const HexRenderer&) = delete;

    // Call once after GL context is active
    void init();

    // worldHexSize: the spacing between tile centres (same value used for all terrain tiles).
    //               Stored internally so tokens can be placed at the correct world position
    //               regardless of their visual scale.
    void beginFrame(const glm::mat4& viewProj,
                    float worldHexSize           = 1.0f,
                    const glm::vec3& sunDir      = glm::vec3(0.4f, 1.0f, 0.3f),
                    const glm::vec3& sunColor    = glm::vec3(1.0f, 0.92f, 0.70f),
                    const glm::vec3& ambientColor= glm::vec3(0.35f, 0.30f, 0.22f));

    // Draw a hex shape centred on the axial coordinate.
    // visualScale: size of the drawn hex (< worldHexSize → token, == worldHexSize → terrain tile).
    // height:      Y offset in world units.
    void drawTile(const HexCoord& coord,
                  const glm::vec3& color,
                  float visualScale = 1.0f,
                  float height      = 0.0f);

    // Draw a wireframe outline.  scale multiplies the unit hex outline.
    void drawOutline(const HexCoord& coord, const glm::vec3& color, float scale = 1.0f);

    void endFrame();

private:
    void buildMesh();
    void buildWhiteTexture();

    Shader  m_shader;
    Shader  m_outlineShader;

    GLuint  m_vao = 0;
    GLuint  m_vbo = 0;
    GLuint  m_ibo = 0;
    GLuint  m_lineVao = 0;
    GLuint  m_lineVbo = 0;
    GLuint  m_whiteTex = 0;

    glm::mat4 m_viewProj{1.0f};
    float     m_worldHexSize = 1.0f; // used for toWorld() positioning
    bool      m_begun = false;
};
