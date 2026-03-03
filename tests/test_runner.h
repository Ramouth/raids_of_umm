#pragma once
#include <iostream>
#include <string>
#include <functional>
#include <vector>

// ── Minimal test runner ───────────────────────────────────────────────────────
//
// Usage:
//   SUITE("MyModule") {
//       CHECK(1 + 1 == 2);
//       CHECK_EQ(someFunc(), 42);
//       CHECK_STR(obj.name(), "hello");
//   }
//
// Then in main():
//   return TestRunner::get().run();
//

// inline: exactly one definition across all translation units (C++17)
inline int g_passed = 0;
inline int g_failed = 0;
inline std::string g_currentSuite;

#define CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "  FAIL  " << #expr \
                      << "  (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            ++g_failed; \
        } else { \
            std::cout << "  ok    " << #expr << "\n"; \
            ++g_passed; \
        } \
    } while(0)

#define CHECK_EQ(a, b) \
    do { \
        auto _a = (a); auto _b = (b); \
        if (!(_a == _b)) { \
            std::cerr << "  FAIL  " << #a << " == " << #b \
                      << "  (got " << _a << " vs " << _b << ")" \
                      << "  (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            ++g_failed; \
        } else { \
            std::cout << "  ok    " << #a << " == " << _b << "\n"; \
            ++g_passed; \
        } \
    } while(0)

#define CHECK_NEAR(a, b, eps) \
    do { \
        auto _a = (a); auto _b = (b); auto _e = (eps); \
        auto _diff = _a - _b; if (_diff < 0) _diff = -_diff; \
        if (_diff > _e) { \
            std::cerr << "  FAIL  " << #a << " ~= " << #b \
                      << "  (got " << _a << " vs " << _b << ")" \
                      << "  (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            ++g_failed; \
        } else { \
            std::cout << "  ok    " << #a << " ~= " << _b << "\n"; \
            ++g_passed; \
        } \
    } while(0)

// Force __LINE__ to expand before concatenation
#define SUITE_CONCAT_(a, b) a##b
#define SUITE_CONCAT(a, b)  SUITE_CONCAT_(a, b)

#define SUITE(name) \
    static void SUITE_CONCAT(_suite_, __LINE__)(); \
    static bool SUITE_CONCAT(_reg_,   __LINE__) = \
        (TestRunner::get().add(name, SUITE_CONCAT(_suite_, __LINE__)), true); \
    static void SUITE_CONCAT(_suite_, __LINE__)()

class TestRunner {
public:
    static TestRunner& get() { static TestRunner t; return t; }

    void add(const std::string& name, std::function<void()> fn) {
        m_suites.push_back({name, fn});
    }

    int run() {
        for (auto& [name, fn] : m_suites) {
            std::cout << "\n=== " << name << " ===\n";
            g_currentSuite = name;
            fn();
        }
        int total = g_passed + g_failed;
        std::cout << "\n────────────────────────────────────────\n";
        if (g_failed == 0)
            std::cout << "PASSED: " << g_passed << " / " << total << "\n";
        else
            std::cout << "FAILED: " << g_failed << " / " << total
                      << "  (" << g_passed << " passed)\n";
        return g_failed > 0 ? 1 : 0;
    }

private:
    std::vector<std::pair<std::string, std::function<void()>>> m_suites;
};
