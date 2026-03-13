#pragma once
#include "core/StateMachine.h"
#include "hero/Hero.h"
#include "render/HUDRenderer.h"
#include <glad/glad.h>

/*
 * PartyState — P key
 *
 * Read-only overview of the hero's full party:
 *   Left panel  — army unit stacks (type, count, key stats)
 *   Right panel — Special Characters with level, XP bar, and unlocked abilities
 *
 * ESC or EXIT button dismisses.
 */
class PartyState final : public GameState {
public:
    explicit PartyState(const Hero& hero);

    void onEnter()  override;
    void onExit()   override;
    void update(float dt) override;
    void render()   override;
    bool handleEvent(void* sdlEvent) override;

private:
    const Hero& m_hero;
    HUDRenderer m_hud;
};
