#include "StateMachine.h"
#include <stdexcept>

void StateMachine::push(std::unique_ptr<GameState> state) {
    if (m_processing) {
        m_pending.push_back({ Op::Push, std::move(state) });
        return;
    }
    state->onEnter();
    m_stack.push_back(std::move(state));
}

void StateMachine::pop() {
    if (m_processing) {
        m_pending.push_back({ Op::Pop, nullptr });
        return;
    }
    if (m_stack.empty()) return;
    m_stack.back()->onExit();
    m_stack.pop_back();
}

void StateMachine::replace(std::unique_ptr<GameState> state) {
    if (m_processing) {
        m_pending.push_back({ Op::Replace, std::move(state) });
        return;
    }
    if (!m_stack.empty()) {
        m_stack.back()->onExit();
        m_stack.pop_back();
    }
    state->onEnter();
    m_stack.push_back(std::move(state));
}

void StateMachine::clear() {
    if (m_processing) {
        m_pending.push_back({ Op::Clear, nullptr });
        return;
    }
    while (!m_stack.empty()) {
        m_stack.back()->onExit();
        m_stack.pop_back();
    }
}

GameState* StateMachine::top() const {
    return m_stack.empty() ? nullptr : m_stack.back().get();
}

void StateMachine::update(float dt) {
    applyPending();
    if (!m_stack.empty()) {
        m_processing = true;
        m_stack.back()->update(dt);
        m_processing = false;
    }
    applyPending();
}

void StateMachine::render() {
    if (!m_stack.empty())
        m_stack.back()->render();
}

void StateMachine::applyPending() {
    for (auto& p : m_pending) {
        switch (p.op) {
            case Op::Push:
                p.state->onEnter();
                m_stack.push_back(std::move(p.state));
                break;
            case Op::Pop:
                if (!m_stack.empty()) {
                    m_stack.back()->onExit();
                    m_stack.pop_back();
                }
                if (!m_stack.empty())
                    m_stack.back()->onResume();
                break;
            case Op::Replace:
                if (!m_stack.empty()) {
                    m_stack.back()->onExit();
                    m_stack.pop_back();
                }
                p.state->onEnter();
                m_stack.push_back(std::move(p.state));
                break;
            case Op::Clear:
                while (!m_stack.empty()) {
                    m_stack.back()->onExit();
                    m_stack.pop_back();
                }
                break;
        }
    }
    m_pending.clear();
}
