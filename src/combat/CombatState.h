#pragma once
#include "core/StateMachine.h"
#include "render/SpriteRenderer.h"

class CombatState final : public GameState {
public:
    CombatState();

    void onEnter() override;
    void onExit()  override;
    void update(float dt) override;
    void render()  override;
    bool handleEvent(void* sdlEvent) override;

private:
    void renderCombatUI();

    SpriteRenderer m_spriteRenderer;
    bool m_initialized = false;
};
