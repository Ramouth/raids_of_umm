/*
 * Raids of Umm'Natur — Entry point
 */
#include "core/Application.h"
#include "worldbuilder/WorldBuilderState.h"
#include <iostream>
#include <stdexcept>

int main(int /*argc*/, char* /*argv*/[]) {
    try {
        Application app("Raids of Umm'Natur — World Builder", 1280, 720);
        app.pushState(std::make_unique<WorldBuilderState>());
        app.run();
        std::cout << "[Main] Clean exit.\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[Fatal] " << ex.what() << '\n';
        return 1;
    }
}
