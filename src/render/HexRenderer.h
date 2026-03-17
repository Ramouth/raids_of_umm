#pragma once
#include "Shader.h"
#include "hex/HexCoord.h"
#include "world/MapTile.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>

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
                    const glm::vec3& ambientColor= glm::vec3(0.35f, 0.30f, 0.22f),
                    const glm::vec3& cameraPos   = glm::vec3(0.0f, 0.0f, 0.0f));

    // Load terrain textures from assetRoot/textures/terrain/.
    // Tries <name>1.png, <name>2.png ... up to MAX_TERRAIN_VARIANTS per terrain.
    // Falls back to <name>.png for variant 0 if no numbered files exist.
    void loadTerrainTextures(const std::string& assetRoot = "assets");

    // Returns the GL texture ID for (terrain, variant), clamped to loaded range.
    // Returns 0 if no texture was loaded for this terrain.
    GLuint terrainTex(Terrain t, int variant = 0) const;

    // Road overlay texture (assets/textures/terrain/road.png). Returns 0 if not loaded.
    GLuint roadTex() const { return m_roadTex; }

    // How many variants were loaded for this terrain (0 = none, falls back to colour).
    int terrainVariantCount(Terrain t) const;

    // Draw a hex shape centred on the axial coordinate.
    // visualScale: size of the drawn hex (< worldHexSize → token, == worldHexSize → terrain tile).
    // height:      Y offset in world units.
    // texId:       GL texture to bind for this tile; 0 = use the white placeholder.
    void drawTile(const HexCoord& coord,
                  const glm::vec3& color,
                  float visualScale          = 1.0f,
                  float height               = 0.0f,
                  GLuint texId               = 0,
                  const glm::vec2& xzOffset  = {0.0f, 0.0f});

    // Draw a wireframe outline.  scale multiplies the unit hex outline.
    // height should match the tile's terrainHeight() so the outline sits on the tile face.
    void drawOutline(const HexCoord& coord, const glm::vec3& color,
                     float scale = 1.0f, float height = 0.0f,
                     const glm::vec2& xzOffset = {0.0f, 0.0f});

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
    GLuint  m_roadTex  = 0;
    GLuint  m_terrainTex[TERRAIN_COUNT][MAX_TERRAIN_VARIANTS] = {};
    int     m_variantCount[TERRAIN_COUNT] = {};

    glm::mat4 m_viewProj{1.0f};
    float     m_worldHexSize = 1.0f; // used for toWorld() positioning
    bool      m_begun = false;
};
