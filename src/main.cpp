/*
 * Raids of Umm'Natur — Entry point
 */
#include "core/Application.h"
#include "menu/MainMenuState.h"
#include <iostream>
#include <stdexcept>

int main(int /*argc*/, char* /*argv*/[]) {
    try {
        Application app("Raids of Umm'Natur", 1280, 720);
        app.pushState(std::make_unique<MainMenuState>());
        app.run();
        std::cout << "[Main] Clean exit.\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[Fatal] " << ex.what() << '\n';
        return 1;
    }
}
