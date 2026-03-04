#pragma once
#include "StateMachine.h"
#include "ResourceManager.h"
#include <string>
#include <cstdint>

// Forward declarations to avoid including SDL2 in headers
struct SDL_Window;
typedef void* SDL_GLContext;

/*
 * Application — owns the SDL2 window, OpenGL context, and main loop.
 *
 * Usage:
 *   Application app("Raids of Umm'Natur", 1280, 720);
 *   app.pushState(std::make_unique<MainMenuState>());
 *   app.run();  // blocks until window is closed
 */
class Application {
public:
    Application(const std::string& title, int width, int height);
    ~Application();

    // Non-copyable
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // State management (delegates to internal StateMachine)
    void pushState(std::unique_ptr<GameState> state);
    void popState();
    void replaceState(std::unique_ptr<GameState> state);

    // Enter main loop — returns when the stack is empty or quit is requested
    void run();

    // Request clean shutdown (processes pending events, then exits)
    void quit() { m_running = false; }

    // Accessors
    int width()  const { return m_width; }
    int height() const { return m_height; }
    float aspectRatio() const;
    SDL_Window* window() const { return m_window; }

    // Game data — loaded once at startup, stable for the application lifetime.
    ResourceManager&       resources()       { return m_resources; }
    const ResourceManager& resources() const { return m_resources; }

    // Singleton-style global access for convenience
    static Application& get();

private:
    void initSDL(const std::string& title, int w, int h);
    void initGL();
    void initResources();
    void shutdown();

    void processEvents();

    SDL_Window*   m_window  = nullptr;
    SDL_GLContext m_context = nullptr;
    StateMachine    m_states;
    ResourceManager m_resources;
    bool            m_running = false;
    int             m_width;
    int             m_height;

    static Application* s_instance;
};
