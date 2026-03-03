#include "CombatState.h"
#include "core/Application.h"
#include "render/HUDRenderer.h"
#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>
#include <cmath>

struct CombatUnit {
    int hp;
    int maxHp;
    int attack;
    int defense;
    bool isPlayer;
};

class CombatRenderer {
public:
    CombatRenderer() {}
    ~CombatRenderer() {
        if (m_vao) glDeleteVertexArrays(1, &m_vao);
        if (m_vbo) glDeleteBuffers(1, &m_vbo);
    }

    void init() {
        m_shader = Shader("assets/shaders/ui.vert", "assets/shaders/ui.frag");
        
        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 4, nullptr, GL_DYNAMIC_DRAW);
        
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, nullptr);
        
        glBindVertexArray(0);
    }

    void render(int screenW, int screenH, const std::vector<CombatUnit>& playerUnits, const std::vector<CombatUnit>& enemyUnits) {
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        m_shader.bind();
        m_shader.setVec2("u_ScreenSize", {static_cast<float>(screenW), static_cast<float>(screenH)});
        m_shader.setInt("u_UseTexture", 0);
        
        float scale = screenH / 600.0f;
        
        // Combat background
        drawRect(0, 0, screenW, screenH, {0.1f, 0.05f, 0.05f, 1.0f});
        
        // Battle title bar at top
        drawRect(0, 0, screenW, 40 * scale, {0.3f, 0.1f, 0.1f, 1.0f});
        
        // ===== LEFT SIDE: Player units (bottom-aligned) =====
        float playerY = screenH - 50 * scale;
        for (size_t i = 0; i < playerUnits.size(); ++i) {
            float x = 50 * scale + i * 120 * scale;
            drawRect(x, playerY - 80 * scale, 100 * scale, 80 * scale, {0.2f, 0.4f, 0.6f, 1.0f});
            
            // HP bar
            float hpFill = static_cast<float>(playerUnits[i].hp) / playerUnits[i].maxHp;
            drawRect(x + 10 * scale, playerY - 70 * scale, 80 * scale * hpFill, 10 * scale, {0.2f, 0.8f, 0.2f, 1.0f});
        }
        
        // ===== RIGHT SIDE: Enemy units (top-aligned) =====
        float enemyY = 60 * scale;
        for (size_t i = 0; i < enemyUnits.size(); ++i) {
            float x = screenW - 150 * scale - i * 120 * scale;
            drawRect(x, enemyY, 100 * scale, 80 * scale, {0.6f, 0.2f, 0.2f, 1.0f});
            
            // HP bar
            float hpFill = static_cast<float>(enemyUnits[i].hp) / enemyUnits[i].maxHp;
            drawRect(x + 10 * scale, enemyY + 10 * scale, 80 * scale * hpFill, 10 * scale, {0.8f, 0.2f, 0.2f, 1.0f});
        }
        
        // ===== BOTTOM: Action buttons =====
        float btnY = 50 * scale;
        float btnW = 120 * scale;
        float btnH = 35 * scale;
        
        // Attack button
        drawRect(100 * scale, btnY, btnW, btnH, {0.6f, 0.3f, 0.1f, 1.0f});
        
        // Defend button
        drawRect(240 * scale, btnY, btnW, btnH, {0.3f, 0.3f, 0.6f, 1.0f});
        
        // Cast Spell button
        drawRect(380 * scale, btnY, btnW, btnH, {0.3f, 0.5f, 0.3f, 1.0f});
        
        // Retreat button
        drawRect(520 * scale, btnY, btnW, btnH, {0.5f, 0.2f, 0.2f, 1.0f});
        
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
    }

private:
    void drawRect(float x, float y, float w, float h, const glm::vec4& color) {
        float verts[] = { x, y, x + w, y, x + w, y + h, x, y + h };
        
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
        
        m_shader.setVec4("u_Color", color);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }

    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    Shader m_shader;
};

static CombatRenderer* g_combatRenderer = nullptr;
static std::vector<CombatUnit> g_playerUnits;
static std::vector<CombatUnit> g_enemyUnits;

CombatState::CombatState() {}

void CombatState::onEnter() {
    m_initialized = true;
    
    g_combatRenderer = new CombatRenderer();
    g_combatRenderer->init();
    
    // Initialize 3 player units (can be up to 6)
    g_playerUnits = {
        {50, 50, 8, 4, true},
        {40, 40, 6, 3, true},
        {30, 30, 10, 2, true}
    };
    
    // Initialize 3 enemy units
    g_enemyUnits = {
        {45, 45, 7, 3, false},
        {35, 35, 9, 2, false},
        {25, 25, 8, 1, false}
    };
    
    std::cout << "[Combat] Battle started!\n";
}

void CombatState::onExit() {
    delete g_combatRenderer;
    g_combatRenderer = nullptr;
    g_playerUnits.clear();
    g_enemyUnits.clear();
    std::cout << "[Combat] Battle ended\n";
}

void CombatState::update(float dt) {
    (void)dt;
}

void CombatState::render() {
    glClearColor(0.1f, 0.05f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    if (g_combatRenderer) {
        auto& app = Application::get();
        g_combatRenderer->render(app.width(), app.height(), g_playerUnits, g_enemyUnits);
    }
}

bool CombatState::handleEvent(void* sdlEvent) {
    SDL_Event* e = static_cast<SDL_Event*>(sdlEvent);

    if (e->type == SDL_KEYDOWN) {
        if (e->key.keysym.sym == SDLK_ESCAPE || 
            e->key.keysym.sym == SDLK_SPACE ||
            e->key.keysym.sym == SDLK_RETURN) {
            Application::get().popState();
            return true;
        }
    }
    return false;
}
