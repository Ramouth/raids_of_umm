#include "HexRenderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <stdexcept>
#include <array>

// Flat-top hex: vertices at angles 0, 60, 120, 180, 240, 300 degrees
static constexpr float PI = 3.14159265358979f;

struct HexVertex {
    float x, y, z;    // position
    float nx, ny, nz; // normal
    float u, v;        // texcoord
};

// Build a unit flat-top hexagon on the XZ plane (Y=0 is floor).
// Returns 7 vertices: [0] = centre, [1..6] = outer ring CW
static std::array<HexVertex, 7> buildHexVerts() {
    std::array<HexVertex, 7> v{};
    // Centre vertex
    v[0] = { 0, 0, 0,  0, 1, 0,  0.5f, 0.5f };
    for (int i = 0; i < 6; ++i) {
        float angle = PI / 3.0f * i; // flat-top: 0° = right (+X)
        float cx = std::cos(angle);
        float cz = std::sin(angle);
        float u  = 0.5f + 0.5f * cx;
        float vv = 0.5f + 0.5f * cz;
        v[i + 1] = { cx, 0, cz,  0, 1, 0,  u, vv };
    }
    return v;
}

// 6 triangles: each is (centre, vert_i, vert_{i+1 mod 6})
static std::array<unsigned short, 18> buildHexIndices() {
    std::array<unsigned short, 18> idx{};
    for (int i = 0; i < 6; ++i) {
        idx[i * 3 + 0] = 0;
        // Swap outer two vertices so the top face winds CCW when viewed from +Y,
        // matching GL_CCW front-face convention with back-face culling enabled.
        idx[i * 3 + 1] = static_cast<unsigned short>((i + 1) % 6 + 1);
        idx[i * 3 + 2] = static_cast<unsigned short>(i + 1);
    }
    return idx;
}

// 6 line-loop vertices for the outline (just the outer ring)
static std::array<float, 18> buildHexOutlineVerts() {
    std::array<float, 18> v{};
    for (int i = 0; i < 6; ++i) {
        float angle = PI / 3.0f * i;
        v[i * 3 + 0] = std::cos(angle);
        v[i * 3 + 1] = 0.002f; // tiny Y offset to avoid z-fighting
        v[i * 3 + 2] = std::sin(angle);
    }
    return v;
}

HexRenderer::~HexRenderer() {
    if (m_ibo)     glDeleteBuffers(1, &m_ibo);
    if (m_vbo)     glDeleteBuffers(1, &m_vbo);
    if (m_vao)     glDeleteVertexArrays(1, &m_vao);
    if (m_lineVbo) glDeleteBuffers(1, &m_lineVbo);
    if (m_lineVao) glDeleteVertexArrays(1, &m_lineVao);
    if (m_whiteTex) glDeleteTextures(1, &m_whiteTex);
}

void HexRenderer::init() {
    m_shader        = Shader("assets/shaders/hex.vert",     "assets/shaders/hex.frag");
    m_outlineShader = Shader("assets/shaders/outline.vert", "assets/shaders/outline.frag");
    buildMesh();
    buildWhiteTexture();
}

void HexRenderer::buildMesh() {
    auto verts   = buildHexVerts();
    auto indices = buildHexIndices();

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ibo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices.data(), GL_STATIC_DRAW);

    // position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(HexVertex),
                          reinterpret_cast<void*>(offsetof(HexVertex, x)));
    // normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(HexVertex),
                          reinterpret_cast<void*>(offsetof(HexVertex, nx)));
    // texcoord
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(HexVertex),
                          reinterpret_cast<void*>(offsetof(HexVertex, u)));

    glBindVertexArray(0);

    // Outline VAO (just position, XYZ, line-loop)
    auto lineVerts = buildHexOutlineVerts();
    glGenVertexArrays(1, &m_lineVao);
    glGenBuffers(1, &m_lineVbo);
    glBindVertexArray(m_lineVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_lineVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(lineVerts), lineVerts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glBindVertexArray(0);
}

void HexRenderer::buildWhiteTexture() {
    unsigned char white[4] = { 255, 255, 255, 255 };
    glGenTextures(1, &m_whiteTex);
    glBindTexture(GL_TEXTURE_2D, m_whiteTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void HexRenderer::beginFrame(const glm::mat4& viewProj,
                              float worldHexSize,
                              const glm::vec3& sunDir,
                              const glm::vec3& sunColor,
                              const glm::vec3& ambientColor) {
    m_viewProj      = viewProj;
    m_worldHexSize  = worldHexSize;
    m_begun         = true;

    m_shader.bind();
    m_shader.setVec3("u_SunDir",      glm::normalize(sunDir));
    m_shader.setVec3("u_SunColor",    sunColor);
    m_shader.setVec3("u_AmbientColor",ambientColor);
    m_shader.setInt ("u_Texture", 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_whiteTex);
    glBindVertexArray(m_vao);
}

void HexRenderer::drawTile(const HexCoord& coord,
                            const glm::vec3& color,
                            float visualScale,
                            float height) {
    // Always use the world hex size for positioning so tokens land on the correct tile
    float wx, wz;
    coord.toWorld(m_worldHexSize, wx, wz);

    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(wx, height, wz));
    model           = glm::scale(model, glm::vec3(visualScale));
    glm::mat4 mvp   = m_viewProj * model;
    // Tiles are always flat on XZ — normal is always up; identity normal matrix is correct.
    glm::mat3 norm  = glm::mat3(1.0f);

    m_shader.setMat4 ("u_MVP",          mvp);
    m_shader.setMat4 ("u_Model",        model);
    m_shader.setMat3 ("u_NormalMatrix", norm);
    m_shader.setVec3 ("u_TileColor",    color);
    m_shader.setFloat("u_Height",       0.0f);

    glDrawElements(GL_TRIANGLES, 18, GL_UNSIGNED_SHORT, nullptr);
}

void HexRenderer::drawOutline(const HexCoord& coord,
                               const glm::vec3& color,
                               float scale) {
    float wx, wz;
    coord.toWorld(m_worldHexSize, wx, wz); // position uses world size, not visual scale

    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(wx, 0.002f, wz));
    model           = glm::scale(model, glm::vec3(scale));
    glm::mat4 mvp   = m_viewProj * model;

    m_outlineShader.bind();
    m_outlineShader.setMat4("u_MVP",   mvp);
    m_outlineShader.setVec3("u_Color", color);

    glBindVertexArray(m_lineVao);
    glDrawArrays(GL_LINE_LOOP, 0, 6);

    // Restore tile shader + texture for subsequent drawTile calls
    m_shader.bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_whiteTex);
    glBindVertexArray(m_vao);
}

void HexRenderer::endFrame() {
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    m_shader.unbind();
    m_begun = false;
}
