#pragma once
#include <vector>
#include <memory>

/*
 * Stack-based Finite State Machine.
 *
 * States are owned by the FSM. Pushing places a new state on top; the previous
 * state is paused (onExit is NOT called — only onSuspend conceptually).
 * Popping resumes the state below. Replacing swaps the top.
 *
 * Lifecycle per frame:
 *   handleEvent(SDL_Event&)  — top state gets events first
 *   update(dt)               — only top state updates
 *   render()                 — all states render bottom-to-top (transparent stack)
 */

class GameState {
public:
    virtual ~GameState() = default;
    virtual void onEnter() {}
    virtual void onExit()  {}
    virtual void update(float dt) = 0;
    virtual void render() = 0;

    // Return true to consume the event (block states below from seeing it).
    // SDL_Event is forward-declared as void* to avoid pulling in SDL2 headers here.
    virtual bool handleEvent(void* sdlEvent) { (void)sdlEvent; return false; }
};

class StateMachine {
public:
    StateMachine() = default;
    ~StateMachine() = default;

    // Non-copyable
    StateMachine(const StateMachine&) = delete;
    StateMachine& operator=(const StateMachine&) = delete;

    void push(std::unique_ptr<GameState> state);
    void pop();
    void replace(std::unique_ptr<GameState> state);
    void clear();

    GameState* top() const;
    bool empty() const { return m_stack.empty(); }
    size_t size() const { return m_stack.size(); }

    // Call each frame
    void update(float dt);
    void render();

private:
    std::vector<std::unique_ptr<GameState>> m_stack;

    // Deferred operations — applied at start of next update to avoid
    // modifying the stack while iterating
    enum class Op { Push, Pop, Replace, Clear };
    struct Pending {
        Op op;
        std::unique_ptr<GameState> state; // used by Push/Replace
    };
    std::vector<Pending> m_pending;
    bool m_processing = false;

    void applyPending();
};
