#include "CastleState.h"
#include "core/Application.h"
#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

CastleState::CastleState(std::string castleName)
    : m_castleName(std::move(castleName)) {}

void CastleState::onEnter() {
    m_spriteRenderer.init();
    m_spriteRenderer.loadSprite("assets/textures/buildings/castle.png");
    m_initialized = true;
    std::cout << "[Castle] Entered " << m_castleName << "\n";
}

void CastleState::onExit() {
    std::cout << "[Castle] Exited\n";
}

void CastleState::update(float dt) {
    (void)dt;
}

void CastleState::render() {
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 0.0f, 5.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    glm::mat4 proj = glm::ortho(-2.0f, 2.0f, -1.5f, 1.5f, 0.1f, 10.0f);

    m_spriteRenderer.draw(glm::vec3(0.0f, 0.0f, 0.0f), 3.0f, view, proj);

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
