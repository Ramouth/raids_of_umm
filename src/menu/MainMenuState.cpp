#include "MainMenuState.h"
#include "adventure/AdventureState.h"
#include "worldbuilder/WorldBuilderState.h"
#include "world/WorldMap.h"
#include "core/Application.h"
#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <filesystem>
#include <iostream>

// ── Geometry helper ──────────────────────────────────────────────────────────

void MainMenuState::buttonRect(int i, int sw, int sh,
                                float& x, float& y, float& w, float& h) const {
    const float scale = sh / 600.0f;
    w = 220.f * scale;
    h =  44.f * scale;
    x = (sw - w) / 2.f;
    // Stack buttons centered vertically, 4 buttons with spacing
    const float totalH = 4 * h + 3 * 12.f * scale;
    const float startY = (sh - totalH) / 2.f + sh * 0.08f;  // slightly below center
    y = startY + i * (h + 12.f * scale);
}

// ── Lifecycle ────────────────────────────────────────────────────────────────

void MainMenuState::onEnter() {
    m_hud.init();
    buildButtons();
    std::cout << "[Menu] Main menu entered.\n";
}

void MainMenuState::onExit() {}

void MainMenuState::onResume() {
    // Re-check whether Continue should be enabled (player may have saved while playing).
    buildButtons();
}

void MainMenuState::buildButtons() {
    bool hasSave = std::filesystem::exists("data/saves/session.json");
    m_buttons = {
        { "New Game",     true     },
        { "Continue",     hasSave  },
        { "World Builder",true     },
        { "Quit",         true     },
    };
    m_hovered = -1;
}

// ── Input ────────────────────────────────────────────────────────────────────

void MainMenuState::activate(int idx) {
    switch (idx) {
        case 0: {  // New Game
            WorldMap map;
            std::string path = "data/maps/default.json";
            if (std::filesystem::exists(path)) {
                if (auto err = map.loadJson(path)) {
                    std::cerr << "[Menu] Map load error: " << *err
                              << " — falling back to procedural\n";
                    Application::get().pushState(std::make_unique<AdventureState>());
                } else {
                    Application::get().pushState(
                        std::make_unique<AdventureState>(std::move(map), path));
                }
            } else {
                std::cout << "[Menu] No default map found, generating procedural map.\n";
                Application::get().pushState(std::make_unique<AdventureState>());
            }
            break;
        }
        case 1:  // Continue
            Application::get().pushState(
                std::make_unique<AdventureState>(AdventureState::LoadSave{}));
            break;
        case 2:  // World Builder
            Application::get().pushState(std::make_unique<WorldBuilderState>());
            break;
        case 3:  // Quit
            Application::get().quit();
            break;
        default: break;
    }
}

bool MainMenuState::handleEvent(void* sdlEvent) {
    SDL_Event* e = static_cast<SDL_Event*>(sdlEvent);
    int sw = Application::get().width();
    int sh = Application::get().height();

    if (e->type == SDL_KEYDOWN && !e->key.repeat) {
        switch (e->key.keysym.sym) {
            case SDLK_RETURN:
            case SDLK_SPACE:
                if (m_hovered >= 0 && m_buttons[m_hovered].enabled)
                    activate(m_hovered);
                return true;
            case SDLK_UP:
                m_hovered = (m_hovered <= 0)
                    ? static_cast<int>(m_buttons.size()) - 1
                    : m_hovered - 1;
                return true;
            case SDLK_DOWN:
                m_hovered = (m_hovered + 1) % static_cast<int>(m_buttons.size());
                return true;
            case SDLK_1: activate(0); return true;
            case SDLK_2: if (m_buttons[1].enabled) activate(1); return true;
            case SDLK_3: activate(2); return true;
            case SDLK_4: activate(3); return true;
            default: break;
        }
    }

    if (e->type == SDL_MOUSEMOTION) {
        float mx = static_cast<float>(e->motion.x);
        float my = static_cast<float>(e->motion.y);
        m_hovered = -1;
        for (int i = 0; i < static_cast<int>(m_buttons.size()); ++i) {
            float bx, by, bw, bh;
            buttonRect(i, sw, sh, bx, by, bw, bh);
            if (mx >= bx && mx <= bx + bw && my >= by && my <= by + bh) {
                if (m_buttons[i].enabled) m_hovered = i;
                break;
            }
        }
    }

    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        float mx = static_cast<float>(e->button.x);
        float my = static_cast<float>(e->button.y);
        for (int i = 0; i < static_cast<int>(m_buttons.size()); ++i) {
            float bx, by, bw, bh;
            buttonRect(i, sw, sh, bx, by, bw, bh);
            if (mx >= bx && mx <= bx + bw && my >= by && my <= by + bh) {
                if (m_buttons[i].enabled) activate(i);
                return true;
            }
        }
    }

    return false;
}

// ── Update ───────────────────────────────────────────────────────────────────

void MainMenuState::update(float /*dt*/) {}

// ── Render ───────────────────────────────────────────────────────────────────

void MainMenuState::render() {
    int sw = Application::get().width();
    int sh = Application::get().height();

    glClearColor(0.06f, 0.04f, 0.02f, 1.0f);  // deep desert night
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_hud.begin(sw, sh);

    // ── Title ─────────────────────────────────────────────────────────────────
    const float scale = sh / 600.0f;

    // Title background banner
    float bannerH = 70.f * scale;
    float bannerY = sh * 0.10f;
    m_hud.drawRect(0, bannerY, static_cast<float>(sw), bannerH,
                   { 0.25f, 0.18f, 0.04f, 0.90f });

    // Title text — two lines for legibility at different sizes
    float titleScale = 3.0f * scale;
    const char* title1 = "RAIDS OF";
    const char* title2 = "UMM'NATUR";
    // Approximate centering (stb_easy_font is ~8px wide per char at scale=1)
    float charW = 8.f * titleScale;
    float t1x = (sw - charW * 8)  / 2.f;
    float t2x = (sw - charW * 9)  / 2.f;
    m_hud.drawText(t1x, bannerY + 8.f * scale,  titleScale,
                   title1, { 0.95f, 0.80f, 0.30f, 1.0f });
    m_hud.drawText(t2x, bannerY + 38.f * scale, titleScale,
                   title2, { 0.95f, 0.80f, 0.30f, 1.0f });

    // ── Buttons ───────────────────────────────────────────────────────────────
    for (int i = 0; i < static_cast<int>(m_buttons.size()); ++i) {
        float bx, by, bw, bh;
        buttonRect(i, sw, sh, bx, by, bw, bh);

        bool  hov  = (i == m_hovered);
        bool  enab = m_buttons[i].enabled;
        glm::vec4 bg = enab
            ? (hov ? glm::vec4{0.75f, 0.60f, 0.15f, 1.0f}   // gold hover
                   : glm::vec4{0.35f, 0.28f, 0.08f, 1.0f})  // dark gold
            : glm::vec4{0.18f, 0.18f, 0.18f, 0.7f};          // greyed

        // Shadow
        m_hud.drawRect(bx + 2, by + 2, bw, bh, { 0.f, 0.f, 0.f, 0.5f });
        // Button face
        m_hud.drawRect(bx, by, bw, bh, bg);
        // Border
        float brd = 2.f * scale;
        glm::vec4 borderCol = hov ? glm::vec4{1.f, 0.9f, 0.4f, 1.f}
                                  : glm::vec4{0.6f, 0.5f, 0.1f, 1.f};
        m_hud.drawRect(bx,         by,         bw, brd,  borderCol);
        m_hud.drawRect(bx,         by+bh-brd,  bw, brd,  borderCol);
        m_hud.drawRect(bx,         by,         brd, bh,  borderCol);
        m_hud.drawRect(bx+bw-brd,  by,         brd, bh,  borderCol);

        // Label
        float textScale = 1.8f * scale;
        float charPx = 8.f * textScale;
        float tx = bx + (bw - charPx * m_buttons[i].label.size()) / 2.f;
        float ty = by + (bh - 8.f * textScale) / 2.f;
        glm::vec4 textCol = enab ? glm::vec4{1.f, 0.95f, 0.75f, 1.f}
                                 : glm::vec4{0.5f, 0.5f, 0.5f, 1.f};
        m_hud.drawText(tx, ty, textScale, m_buttons[i].label.c_str(), textCol);

        // Key hint (1/2/3/4)
        float hintScale = 1.2f * scale;
        char hint[3] = { static_cast<char>('1' + i), '\0', '\0' };
        m_hud.drawText(bx + 6.f * scale, ty + 2.f * scale,
                       hintScale, hint,
                       enab ? glm::vec4{0.8f, 0.8f, 0.4f, 0.9f}
                            : glm::vec4{0.4f, 0.4f, 0.4f, 0.6f});
    }

    // ── Hint bar ──────────────────────────────────────────────────────────────
    float hintScale = 1.0f * scale;
    m_hud.drawText(8.f * scale, sh - 20.f * scale, hintScale,
                   "Arrow keys / mouse to navigate  |  Enter or click to select",
                   { 0.6f, 0.6f, 0.5f, 0.8f });
}
