#pragma once
#include <functional>
#include <unordered_map>
#include <vector>
#include <typeindex>
#include <any>

/*
 * EventBus — lightweight typed publish/subscribe.
 *
 * Usage:
 *   struct DamageEvent { int amount; int targetId; };
 *
 *   auto token = EventBus::get().subscribe<DamageEvent>([](const DamageEvent& e){
 *       std::cout << "Damage: " << e.amount << '\n';
 *   });
 *
 *   EventBus::get().emit<DamageEvent>({42, 1});
 *
 *   EventBus::get().unsubscribe(token);  // optional cleanup
 */

class EventBus {
public:
    using Token = size_t;

    static EventBus& get() {
        static EventBus instance;
        return instance;
    }

    template<typename Event>
    Token subscribe(std::function<void(const Event&)> callback) {
        auto key = std::type_index(typeid(Event));
        Token tok = m_nextToken++;
        m_listeners[key].push_back({ tok, [cb = std::move(callback)](const std::any& ev) {
            cb(std::any_cast<const Event&>(ev));
        }});
        return tok;
    }

    void unsubscribe(Token tok) {
        for (auto& [key, vec] : m_listeners) {
            vec.erase(std::remove_if(vec.begin(), vec.end(),
                [tok](const Listener& l){ return l.token == tok; }),
                vec.end());
        }
    }

    template<typename Event>
    void emit(const Event& event) {
        auto key = std::type_index(typeid(Event));
        auto it = m_listeners.find(key);
        if (it == m_listeners.end()) return;
        std::any wrapped = event;
        for (auto& listener : it->second) {
            listener.handler(wrapped);
        }
    }

private:
    EventBus() = default;
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    struct Listener {
        Token token;
        std::function<void(const std::any&)> handler;
    };

    std::unordered_map<std::type_index, std::vector<Listener>> m_listeners;
    Token m_nextToken = 1;
};
