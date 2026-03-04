#include "CastleState.h"
#include "core/Application.h"
#include "render/HUDRenderer.h"
#include "render/Texture.h"
#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <iostream>
#include <cstdio>

CastleState::CastleState(std::string castleName)
    : m_castleName(std::move(castleName)) {}

void CastleState::onEnter() {
    m_spriteRenderer.init();
    m_spriteRenderer.loadSprite("assets/textures/objects/castle.png");
    m_interiorTex = loadTexturePNG("assets/textures/buildings/castle_interior.png");
    m_hud.init();
    m_initialized = true;
    std::cout << "[Castle] Entered " << m_castleName << "\n";
}

void CastleState::onExit() {
    if (m_interiorTex) { glDeleteTextures(1, &m_interiorTex); m_interiorTex = 0; }
    std::cout << "[Castle] Exited\n";
}

void CastleState::update(float dt) {
    (void)dt;
}

void CastleState::render() {
    auto& app = Application::get();
    int w = app.width();
    int h = app.height();

    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float scale = h / 600.0f;

    float panelW = 260 * scale;
    float panelH = h - 40 * scale;
    float panelX = 20 * scale;
    float panelY = 20 * scale;

    float castleAreaX = panelX + panelW + 15 * scale;
    float castleAreaW = w - castleAreaX - 20 * scale;
    float castleAreaH = h - 40 * scale;

    // All 2D drawing — set up HUD shader once
    m_hud.begin(w, h);

    // --- Left panel ---
    m_hud.drawRect(panelX, panelY, panelW, panelH, {0.1f, 0.06f, 0.03f, 0.92f});
    m_hud.drawRect(panelX, panelY, panelW, 5 * scale, {0.65f, 0.45f, 0.2f, 1.0f});
    m_hud.drawRect(panelX, panelY + panelH - 5 * scale, panelW, 5 * scale, {0.65f, 0.45f, 0.2f, 1.0f});

    float titleY = panelY + panelH - 28 * scale;
    m_hud.drawText(panelX + 10 * scale, titleY, scale * 2.0f, m_castleName.c_str(), {1.0f, 0.9f, 0.5f, 1.0f});

    float resY = titleY - 35 * scale;
    char goldText[32];
    std::snprintf(goldText, sizeof(goldText), "Gold: 5000");
    m_hud.drawText(panelX + 10 * scale, resY, scale * 1.3f, goldText, {1.0f, 0.85f, 0.2f, 1.0f});

    char woodText[32];
    std::snprintf(woodText, sizeof(woodText), "Wood: 5");
    m_hud.drawText(panelX + 10 * scale, resY - 20 * scale, scale * 1.3f, woodText, {0.6f, 0.8f, 0.4f, 1.0f});

    char oreText[32];
    std::snprintf(oreText, sizeof(oreText), "Ore: 3");
    m_hud.drawText(panelX + 100 * scale, resY - 20 * scale, scale * 1.3f, oreText, {0.7f, 0.7f, 0.7f, 1.0f});

    float buildingY = resY - 60 * scale;
    m_hud.drawRect(panelX + 10 * scale, buildingY + 15 * scale, panelW - 20 * scale, 2 * scale, {0.5f, 0.4f, 0.2f, 1.0f});

    const char* buildings[] = {
        "Town Hall",
        "Fort",
        "Blacksmith",
        "Marketplace",
        "Resource Silo",
        "Tavern",
        "Warehouse"
    };

    float by = buildingY - 5 * scale;
    for (const char* bld : buildings) {
        m_hud.drawRect(panelX + 12 * scale, by, 10 * scale, 10 * scale, {0.25f, 0.55f, 0.25f, 1.0f});
        m_hud.drawText(panelX + 28 * scale, by + 1 * scale, scale * 1.1f, bld, {0.85f, 0.85f, 0.75f, 1.0f});
        by -= 20 * scale;
    }

    float exitBtnW = 100 * scale;
    float exitBtnH = 30 * scale;
    float exitBtnX = panelX + (panelW - exitBtnW) / 2;
    float exitBtnY = panelY + 10 * scale;

    m_hud.drawRect(exitBtnX, exitBtnY, exitBtnW, exitBtnH, {0.55f, 0.2f, 0.08f, 1.0f});
    m_hud.drawRect(exitBtnX, exitBtnY, exitBtnW, 3 * scale, {0.75f, 0.5f, 0.25f, 1.0f});
    m_hud.drawRect(exitBtnX, exitBtnY + exitBtnH - 3 * scale, exitBtnW, 3 * scale, {0.35f, 0.1f, 0.02f, 1.0f});
    m_hud.drawText(exitBtnX + (exitBtnW - 50 * scale) / 2, exitBtnY + 8 * scale, scale * 1.2f, "EXIT", {1.0f, 0.95f, 0.75f, 1.0f});

    // --- Right castle area ---
    // Draw interior image filling the entire right panel
    if (m_interiorTex)
        m_hud.drawTexturedRect(castleAreaX, panelY, castleAreaW, castleAreaH, m_interiorTex);
    else
        m_hud.drawRect(castleAreaX, panelY, castleAreaW, castleAreaH, {0.05f, 0.08f, 0.12f, 1.0f});

    // Borders on top of interior
    m_hud.drawRect(castleAreaX, panelY, castleAreaW, 4 * scale, {0.45f, 0.35f, 0.2f, 1.0f});
    m_hud.drawRect(castleAreaX, panelY + castleAreaH - 4 * scale, castleAreaW, 4 * scale, {0.45f, 0.35f, 0.2f, 1.0f});

    float infoX = castleAreaX + 15 * scale;
    float infoY = panelY + castleAreaH - 35 * scale;
    m_hud.drawText(infoX, infoY, scale * 1.1f, "Visiting hero:", {0.7f, 0.75f, 0.85f, 1.0f});
    m_hud.drawText(infoX, infoY - 18 * scale, scale * 1.2f, "No hero stationed", {0.5f, 0.5f, 0.55f, 1.0f});

    glDisable(GL_BLEND);
}

bool CastleState::handleEvent(void* sdlEvent) {
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
