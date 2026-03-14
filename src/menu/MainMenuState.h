#pragma once
#include "core/StateMachine.h"
#include "render/HUDRenderer.h"
#include <string>
#include <vector>

/*
 * MainMenuState — default entry point for the game.
 *
 * Four actions:
 *   New Game     : loads data/maps/default.json (or procedural if missing)
 *   Continue     : loads data/saves/session.json into a new AdventureState
 *   World Builder: pushes WorldBuilderState
 *   Quit         : Application::get().quit()
 *
 * "Continue" is greyed out when no save file exists.
 */
class MainMenuState final : public GameState {
public:
    void onEnter()  override;
    void onExit()   override;
    void onResume() override;
    void update(float dt) override;
    void render()   override;
    bool handleEvent(void* sdlEvent) override;

private:
    struct Button {
        std::string label;
        bool        enabled = true;
    };

    std::vector<Button> m_buttons;
    int                 m_hovered = -1;
    HUDRenderer         m_hud;

    // Called in onEnter (and onResume) so "Continue" greyed state is fresh.
    void buildButtons();

    // Shared geometry: screen-space rect for button at index i.
    void buttonRect(int i, int sw, int sh,
                    float& x, float& y, float& w, float& h) const;

    void activate(int idx);
};
