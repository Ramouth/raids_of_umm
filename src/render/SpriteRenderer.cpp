#include "SpriteRenderer.h"
#include "Texture.h"
#include <array>
#include <cstddef>

struct SpriteVertex {
    float ox, oy;  // billboard-local offset
    float u,  v;   // texture UV
};

SpriteRenderer::~SpriteRenderer() {
    if (m_ibo) glDeleteBuffers(1, &m_ibo);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_tex) glDeleteTextures(1, &m_tex);
}

void SpriteRenderer::init() {
    m_shader = Shader("assets/shaders/sprite.vert", "assets/shaders/sprite.frag");
    buildQuad();
}

void SpriteRenderer::buildQuad() {
    // Four corners of the billboard quad.
    // a_Offset: x in [-0.5, 0.5], y in [0, 1] (0 = base, 1 = top of sprite).
    // a_TexCoord: V is flipped in the vertex shader to correct PNG row ordering.
    const std::array<SpriteVertex, 4> verts = {{
        { -0.5f, 0.0f,  0.0f, 0.0f },  // bottom-left
        {  0.5f, 0.0f,  1.0f, 0.0f },  // bottom-right
        {  0.5f, 1.0f,  1.0f, 1.0f },  // top-right
        { -0.5f, 1.0f,  0.0f, 1.0f },  // top-left
    }};
    const std::array<unsigned short, 6> idx = {{ 0, 1, 2, 0, 2, 3 }};

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ibo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx.data(), GL_STATIC_DRAW);

    // location 0: a_Offset (vec2)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex),
                          reinterpret_cast<void*>(offsetof(SpriteVertex, ox)));

    // location 1: a_TexCoord (vec2)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex),
                          reinterpret_cast<void*>(offsetof(SpriteVertex, u)));

    glBindVertexArray(0);
}

void SpriteRenderer::loadSprite(const std::string& pngPath) {
    if (m_tex) {
        glDeleteTextures(1, &m_tex);
        m_tex = 0;
    }
    m_tex = loadTexturePNG(pngPath);
}

void SpriteRenderer::draw(const glm::vec3& worldPos, float size,
                           const glm::mat4& view, const glm::mat4& proj) {
    if (!m_tex || !m_shader.valid()) return;

    m_shader.bind();
    m_shader.setVec3 ("u_Center",  worldPos);
    m_shader.setFloat("u_Size",    size);
    m_shader.setMat4 ("u_View",    view);
    m_shader.setMat4 ("u_Proj",    proj);
    m_shader.setInt  ("u_Texture", 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_tex);
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    m_shader.unbind();
}
