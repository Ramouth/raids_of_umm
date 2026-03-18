#include "HexRenderer.h"
#include "render/Texture.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <array>
#include <string>
#include <vector>

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
    if (m_roadTex)  glDeleteTextures(1, &m_roadTex);
    for (auto& row : m_terrainTex)
        for (GLuint& t : row)
            if (t) { glDeleteTextures(1, &t); t = 0; }
    for (GLuint& t : m_grassSandEdgeTex)
        if (t) { glDeleteTextures(1, &t); t = 0; }
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

void HexRenderer::loadTerrainTextures(const std::string& assetRoot) {
    namespace fs = std::filesystem;

    for (int i = 0; i < TERRAIN_COUNT; ++i) {
        Terrain t = static_cast<Terrain>(i);
        std::string name(terrainName(t));
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        // Normalise to filesystem-safe name: spaces and hyphens → underscores.
        for (char& c : name) if (c == ' ' || c == '-') c = '_';

        m_variantCount[i] = 0;

        // Each terrain type lives in its own subdirectory:
        //   assets/textures/terrain/<name>/
        // All .png files found there are loaded as variants, sorted alphabetically
        // for deterministic ordering. Drop a file in the folder to add a variant.
        fs::path dir = fs::path(assetRoot) / "textures" / "terrain" / name;

        if (fs::is_directory(dir)) {
            std::vector<fs::path> pngs;
            for (const auto& entry : fs::directory_iterator(dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".png")
                    pngs.push_back(entry.path());
            }
            std::sort(pngs.begin(), pngs.end());

            for (const auto& p : pngs) {
                if (m_variantCount[i] >= MAX_TERRAIN_VARIANTS) break;
                GLuint id = loadTexturePNG(p.string());
                if (id)
                    m_terrainTex[i][m_variantCount[i]++] = id;
            }
        }
    }

    // Road overlay texture — single file, not terrain-type-based.
    fs::path roadPath = fs::path(assetRoot) / "textures" / "terrain" / "road.png";
    if (fs::is_regular_file(roadPath))
        m_roadTex = loadTexturePNG(roadPath.string());
}

GLuint HexRenderer::terrainTex(Terrain t, int variant) const {
    int ti = static_cast<int>(t);
    int vc = m_variantCount[ti];
    if (vc == 0) return 0;
    int v = (variant >= 0 && variant < vc) ? variant : 0;
    return m_terrainTex[ti][v];
}

int HexRenderer::terrainVariantCount(Terrain t) const {
    return m_variantCount[static_cast<int>(t)];
}

void HexRenderer::loadEdgeTiles(const std::string& assetRoot) {
    namespace fs = std::filesystem;
    // File names per direction: 0=E 1=NE 2=NW 3=W 4=SW 5=SE
    static const char* const FILENAMES[EDGE_DIR_COUNT] = {
        "gs_left_right.png",   // 0: E  — grass left, sand right
        "gs_edge_ne.png",      // 1: NE — grass lower-left, sand upper-right
        "gs_edge_nw.png",      // 2: NW — grass lower-right, sand upper-left
        "gs_edge_w.png",       // 3: W  — grass right, sand left
        "gs_diag_topright.png",// 4: SW — grass upper-right, sand lower-left
        "gs_diag_topleft.png", // 5: SE — grass upper-left, sand lower-right
    };
    fs::path dir = fs::path(assetRoot) / "textures" / "terrain" / "grass_sand_edge";
    for (int i = 0; i < EDGE_DIR_COUNT; ++i) {
        fs::path p = dir / FILENAMES[i];
        if (fs::is_regular_file(p))
            m_grassSandEdgeTex[i] = loadTexturePNG(p.string());
    }
}

GLuint HexRenderer::grassSandEdgeTex(int dir) const {
    if (dir < 0 || dir >= EDGE_DIR_COUNT) return 0;
    return m_grassSandEdgeTex[dir];
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
                              const glm::vec3& ambientColor,
                              const glm::vec3& cameraPos) {
    m_viewProj      = viewProj;
    m_worldHexSize  = worldHexSize;
    m_begun         = true;

    m_shader.bind();
    m_shader.setVec3("u_SunDir",      glm::normalize(sunDir));
    m_shader.setVec3("u_SunColor",    sunColor);
    m_shader.setVec3("u_AmbientColor",ambientColor);
    m_shader.setInt ("u_Texture",  0);
    m_shader.setInt ("u_TintMode", 0);
    m_shader.setInt ("u_SoftEdge", 0);
    m_shader.setFloat("u_FogDensity", 0.015f);
    m_shader.setVec3("u_FogColor",    {0.76f, 0.69f, 0.50f});
    m_shader.setVec3("u_CameraPos",   cameraPos);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_whiteTex);
    glBindVertexArray(m_vao);
}

void HexRenderer::drawTile(const HexCoord& coord,
                            const glm::vec3& color,
                            float visualScale,
                            float height,
                            GLuint texId,
                            const glm::vec2& xzOffset,
                            int tintMode,
                            bool softEdge) {
    // Always use the world hex size for positioning so tokens land on the correct tile
    float wx, wz;
    coord.toWorld(m_worldHexSize, wx, wz);
    wx += xzOffset.x;
    wz += xzOffset.y;

    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(wx, height, wz));
    model           = glm::scale(model, glm::vec3(visualScale));
    glm::mat4 mvp   = m_viewProj * model;
    // Tiles are always flat on XZ — normal is always up; identity normal matrix is correct.
    glm::mat3 norm  = glm::mat3(1.0f);

    if (texId != 0)
        glBindTexture(GL_TEXTURE_2D, texId);

    m_shader.setMat4("u_MVP",          mvp);
    m_shader.setMat4("u_Model",        model);
    m_shader.setMat3("u_NormalMatrix", norm);
    m_shader.setVec3("u_TileColor",    color);
    m_shader.setFloat("u_Height",      0.0f);
    m_shader.setInt  ("u_Textured",    texId != 0 ? 1 : 0);
    m_shader.setInt  ("u_TintMode",    tintMode);
    m_shader.setInt  ("u_SoftEdge",    softEdge ? 1 : 0);

    glDrawElements(GL_TRIANGLES, 18, GL_UNSIGNED_SHORT, nullptr);

    if (texId != 0)
        glBindTexture(GL_TEXTURE_2D, m_whiteTex);
}

void HexRenderer::drawOutline(const HexCoord& coord,
                               const glm::vec3& color,
                               float scale,
                               float height,
                               const glm::vec2& xzOffset) {
    float wx, wz;
    coord.toWorld(m_worldHexSize, wx, wz); // position uses world size, not visual scale
    wx += xzOffset.x;
    wz += xzOffset.y;

    // Slightly higher offset to avoid z-fighting with tiles; raised to match the tile's height.
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(wx, height + 0.01f, wz));
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
