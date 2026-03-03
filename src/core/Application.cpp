#include "Application.h"
#include <glad/glad.h>
#include <SDL2/SDL.h>
#include <stdexcept>
#include <string>
#include <iostream>
#include <chrono>

Application* Application::s_instance = nullptr;

Application::Application(const std::string& title, int width, int height)
    : m_width(width), m_height(height)
{
    if (s_instance) {
        throw std::runtime_error("Only one Application instance is allowed.");
    }
    s_instance = this;
    initSDL(title, width, height);
    initGL();
}

Application::~Application() {
    shutdown();
    s_instance = nullptr;
}

Application& Application::get() {
    if (!s_instance) throw std::runtime_error("Application not created.");
    return *s_instance;
}

void Application::initSDL(const std::string& title, int w, int h) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }

    // OpenGL 3.3 Core
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);

    m_window = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        w, h,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!m_window) {
        throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    }

    m_context = SDL_GL_CreateContext(m_window);
    if (!m_context) {
        throw std::runtime_error(std::string("SDL_GL_CreateContext failed: ") + SDL_GetError());
    }

    SDL_GL_SetSwapInterval(1); // vsync
}

void Application::initGL() {
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        throw std::runtime_error("gladLoadGLLoader failed — could not load OpenGL functions.");
    }

    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* version  = glGetString(GL_VERSION);
    std::cout << "[GL] Renderer : " << (renderer ? (const char*)renderer : "unknown") << '\n';
    std::cout << "[GL] Version  : " << (version  ? (const char*)version  : "unknown") << '\n';

    glViewport(0, 0, m_width, m_height);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // Dark void — makes the hex-map boundary clearly visible against the background.
    glClearColor(0.04f, 0.04f, 0.06f, 1.0f);
}

void Application::shutdown() {
    m_states.clear();
    if (m_context) {
        SDL_GL_DeleteContext(m_context);
        m_context = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    SDL_Quit();
}

void Application::pushState(std::unique_ptr<GameState> state) {
    m_states.push(std::move(state));
}

void Application::popState() {
    m_states.pop();
}

void Application::replaceState(std::unique_ptr<GameState> state) {
    m_states.replace(std::move(state));
}

float Application::aspectRatio() const {
    return static_cast<float>(m_width) / static_cast<float>(m_height);
}

void Application::processEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            m_running = false;
            return;
        }
        if (event.type == SDL_WINDOWEVENT &&
            event.window.event == SDL_WINDOWEVENT_RESIZED) {
            m_width  = event.window.data1;
            m_height = event.window.data2;
            glViewport(0, 0, m_width, m_height);
        }
        // Propagate to top state (cast to void* to match forward-declared signature)
        if (auto* top = m_states.top()) {
            top->handleEvent(static_cast<void*>(&event));
        }
    }
}

void Application::run() {
    m_running = true;

    using Clock = std::chrono::steady_clock;
    auto lastTime = Clock::now();

    while (m_running && !m_states.empty()) {
        auto now = Clock::now();
        float dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;
        // Clamp dt to avoid spiral-of-death on debug pauses
        if (dt > 0.1f) dt = 0.1f;

        processEvents();

        m_states.update(dt);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        m_states.render();

        SDL_GL_SwapWindow(m_window);
    }
}
