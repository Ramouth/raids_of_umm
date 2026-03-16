#include "CombatRenderer.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <algorithm>

// ── Helpers ───────────────────────────────────────────────────────────────────

static bool hexIn(const std::vector<HexCoord>& list, HexCoord c) {
    for (const auto& h : list)
        if (h == c) return true;
    return false;
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

CombatRenderer::~CombatRenderer() {
    if (m_uiVao) glDeleteVertexArrays(1, &m_uiVao);
    if (m_uiVbo) glDeleteBuffers(1, &m_uiVbo);
}

void CombatRenderer::init() {
    // ── 3D hex grid ───────────────────────────────────────────────────────────
    m_hexRenderer.init();
    m_hexRenderer.loadTerrainTextures("assets");  // loads sand.png if present

    // ── Unit sprites — one SpriteRenderer per unit/SC id ─────────────────────
    // Fallbacks for any unit without a dedicated sprite.
    m_fallbackPlayerSprite.init();
    m_fallbackPlayerSprite.loadSprite("assets/textures/units/armoured_warrior.png");
    m_fallbackEnemySprite.init();
    m_fallbackEnemySprite.loadSprite("assets/textures/units/enemy_scout.png");

    // All known unit sprites. Missing files are silent no-ops (SpriteRenderer::draw
    // returns immediately when m_tex == 0, falling back to tinted hex outline).
    static const std::pair<const char*, const char*> kUnitSprites[] = {
        // Units from units.json
        { "mummy",            "assets/textures/units/mummy.png"           },
        { "djinn",            "assets/textures/units/djinn.png"           },
        { "ancient_guardian", "assets/textures/units/ancient_guardian.png"},
        { "pharaoh_lich",     "assets/textures/units/pharaoh_lich.png"    },
        { "skeleton_warrior", "assets/textures/units/armoured_warrior.png"},
        { "desert_archer",    "assets/textures/units/rider_archer.png"    },
        { "sand_scorpion",    "assets/textures/units/enemy_scout.png"     },
        // SC portraits
        { "ushari",           "assets/textures/units/ushari.png"          },
        { "sekhara",          "assets/textures/units/sekhara.png"         },
        { "khet",             "assets/textures/units/khet.png"            },
        // Extra sprites present in assets
        { "armoured_warrior", "assets/textures/units/armoured_warrior.png"},
        { "kharim",           "assets/textures/units/kharim.png"          },
        { "rider_archer",     "assets/textures/units/rider_archer.png"    },
        { "rider_banner",     "assets/textures/units/rider_banner.png"    },
        { "rider_knight",     "assets/textures/units/rider_knight.png"    },
        { "rider_lance",      "assets/textures/units/rider_lance.png"     },
    };
    for (const auto& [id, path] : kUnitSprites) {
        auto sr = std::make_unique<SpriteRenderer>();
        sr->init();
        sr->loadSprite(path);
        m_sprites[id] = std::move(sr);
    }

    // ── 2D UI overlay ─────────────────────────────────────────────────────────
    m_uiShader = Shader("assets/shaders/ui.vert", "assets/shaders/ui.frag");

    glGenVertexArrays(1, &m_uiVao);
    glGenBuffers(1, &m_uiVbo);
    glBindVertexArray(m_uiVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_uiVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 8, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, nullptr);
    glBindVertexArray(0);

    m_ready = true;
}

// ── Main render ───────────────────────────────────────────────────────────────

void CombatRenderer::render(int screenW, int screenH,
                             const CombatEngine& engine,
                             const Camera2D& cam,
                             HexCoord hoveredHex,
                             bool hasHovered,
                             const UnitAnimState& anim) {
    if (!m_ready) return;

    // Pre-compute highlight sets once per frame
    const auto reachable  = engine.reachableTiles();
    const auto attackable = engine.attackableTiles();

    const glm::mat4 viewProj = cam.viewProjMatrix();
    const glm::mat4 view     = cam.viewMatrix();
    const glm::mat4 proj     = cam.projMatrix();

    // ── 3D pass: hex grid ─────────────────────────────────────────────────────
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    // Warm desert-sun lighting
    m_hexRenderer.beginFrame(
        viewProj,
        HEX_SIZE,
        glm::vec3(0.4f, 1.0f, 0.3f),    // sun direction
        glm::vec3(1.0f, 0.90f, 0.72f),  // sun colour
        glm::vec3(0.35f, 0.30f, 0.20f), // ambient
        glm::vec3(cam.position().x, 0.0f, cam.position().y)
    );

    GLuint sandTex = m_hexRenderer.terrainTex(Terrain::Sand);

    for (HexCoord coord : CombatMap::allHexes()) {
        glm::vec3 color = tileColor(coord, reachable, attackable);
        m_hexRenderer.drawTile(coord, color, HEX_SIZE, 0.0f, sandTex);
    }

    m_hexRenderer.endFrame();

    // ── 3D pass: outlines ─────────────────────────────────────────────────────

    // Active-unit hex — yellow
    if (!engine.isOver()) {
        m_hexRenderer.drawOutline(engine.activeUnit().pos,
                                  { 0.95f, 0.85f, 0.1f }, HEX_SIZE);
    }

    // Hovered hex — white
    if (hasHovered && CombatMap::inBounds(hoveredHex))
        m_hexRenderer.drawOutline(hoveredHex, { 0.9f, 0.9f, 0.9f }, HEX_SIZE);

    // ── 3D pass: unit sprites ─────────────────────────────────────────────────
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (int i = 0; i < static_cast<int>(engine.playerArmy().stacks.size()); ++i) {
        const auto& unit = engine.playerArmy().stacks[i];
        if (unit.isDead()) continue;
        glm::vec3 pos;
        if (anim.active && anim.isPlayer && anim.stackIndex == i) {
            pos = anim.worldPos;
        } else {
            float wx, wz;
            unit.pos.toWorld(HEX_SIZE, wx, wz);
            pos = { wx, 0.0f, wz };
        }
        spriteFor(unit)->draw(pos, HEX_SIZE * 0.85f, view, proj);
    }
    for (int i = 0; i < static_cast<int>(engine.enemyArmy().stacks.size()); ++i) {
        const auto& unit = engine.enemyArmy().stacks[i];
        if (unit.isDead()) continue;
        glm::vec3 pos;
        if (anim.active && !anim.isPlayer && anim.stackIndex == i) {
            pos = anim.worldPos;
        } else {
            float wx, wz;
            unit.pos.toWorld(HEX_SIZE, wx, wz);
            pos = { wx, 0.0f, wz };
        }
        spriteFor(unit)->draw(pos, HEX_SIZE * 0.85f, view, proj);
    }

    // ── 2D pass: action panel ─────────────────────────────────────────────────
    glDisable(GL_DEPTH_TEST);
    renderActionPanel(screenW, screenH);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

// ── Sprite lookup ─────────────────────────────────────────────────────────────

SpriteRenderer* CombatRenderer::spriteFor(const CombatUnit& unit) {
    // SC portrait takes priority
    if (unit.isSpecialCharacter && !unit.scId.empty()) {
        auto it = m_sprites.find(unit.scId);
        if (it != m_sprites.end()) return it->second.get();
    }
    // Regular unit type sprite
    if (unit.type) {
        auto it = m_sprites.find(unit.type->id);
        if (it != m_sprites.end()) return it->second.get();
    }
    return unit.isPlayer ? &m_fallbackPlayerSprite : &m_fallbackEnemySprite;
}

// ── Tile colour logic ─────────────────────────────────────────────────────────

glm::vec3 CombatRenderer::tileColor(HexCoord coord,
                                     const std::vector<HexCoord>& reachable,
                                     const std::vector<HexCoord>& attackable) const {
    // Highlight sets take priority
    if (hexIn(attackable, coord)) return { 0.72f, 0.30f, 0.22f };  // red — attack
    if (hexIn(reachable,  coord)) return { 0.28f, 0.55f, 0.52f };  // teal — move

    // Zone tints
    int col, row;
    CombatMap::fromHex(coord, col, row);
    (void)row;

    if (col <= 1)                    return { 0.42f, 0.52f, 0.70f };  // player zone
    if (col >= CombatMap::GRID_W-2)  return { 0.70f, 0.42f, 0.42f };  // enemy zone
    return { 0.76f, 0.62f, 0.38f };                                    // neutral sand
}

// ── 2D action panel ───────────────────────────────────────────────────────────

void CombatRenderer::renderActionPanel(int screenW, int screenH) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_uiShader.bind();
    m_uiShader.setVec2("u_ScreenSize", { static_cast<float>(screenW),
                                         static_cast<float>(screenH) });
    m_uiShader.setInt("u_UseTexture", 0);

    const float scale  = screenH / 600.0f;
    const float panelH = 60.0f * scale;
    const float panelY = screenH - panelH;

    // Panel background
    drawRect2D(0, panelY, static_cast<float>(screenW), panelH,
               { 0.08f, 0.06f, 0.04f, 0.92f });

    // Buttons:  A=Attack   Space=Defend   Spell(stub)   ESC=Retreat
    const float btnW = 110.0f * scale;
    const float btnH = 36.0f * scale;
    const float btnY = panelY + (panelH - btnH) * 0.5f;
    const float gap  = 12.0f * scale;

    drawRect2D(gap,                     btnY, btnW, btnH, { 0.65f, 0.30f, 0.08f, 1.0f }); // Attack
    drawRect2D(gap + (btnW+gap),        btnY, btnW, btnH, { 0.28f, 0.32f, 0.65f, 1.0f }); // Defend
    drawRect2D(gap + (btnW+gap)*2,      btnY, btnW, btnH, { 0.28f, 0.52f, 0.28f, 1.0f }); // Spell
    drawRect2D(gap + (btnW+gap)*3,      btnY, btnW, btnH, { 0.55f, 0.18f, 0.18f, 1.0f }); // Retreat

    glDisable(GL_BLEND);
}

void CombatRenderer::drawRect2D(float x, float y, float w, float h,
                                 const glm::vec4& color) {
    float verts[] = { x, y,  x+w, y,  x+w, y+h,  x, y+h };

    glBindVertexArray(m_uiVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_uiVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

    m_uiShader.setVec4("u_Color", color);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}
