#pragma once
#include "core/StateMachine.h"
#include "render/SpriteRenderer.h"
#include "render/HUDRenderer.h"
#include "render/Texture.h"
#include <glad/glad.h>

class CastleState final : public GameState {
public:
    explicit CastleState(std::string castleName);

    void onEnter() override;
    void onExit()  override;
    void update(float dt) override;
    void render()  override;
    bool handleEvent(void* sdlEvent) override;

private:
    std::string m_castleName;
    SpriteRenderer m_spriteRenderer;
    HUDRenderer m_hud;
    GLuint m_interiorTex = 0;
    bool m_initialized = false;
};
