#pragma once
#include "core/StateMachine.h"
#include "render/SpriteRenderer.h"

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
    bool m_initialized = false;
};
