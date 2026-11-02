#pragma once
// A tiny test harness: CHECK records failures and the test keeps going,
// REQUIRE stops the test. Each test is a function registered with TEST(name).

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace t {

struct Case { const char* name; void (*fn)(); };
inline std::vector<Case>& registry() { static std::vector<Case> r; return r; }
inline int& failures() { static int f = 0; return f; }
struct Reg { Reg(const char* n, void (*f)()) { registry().push_back({n, f}); } };
struct Stop {};

inline void fail(const char* file, int line, const std::string& what) {
    std::printf("    %s:%d: %s\n", file, line, what.c_str());
    ++failures();
}

inline int run(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int ran = 0, bad = 0;
    for (auto& c : registry()) {
        if (filter && std::string(c.name).find(filter) == std::string::npos) continue;
        const int before = failures();
        try { c.fn(); } catch (Stop&) {}
        const bool ok = failures() == before;
        std::printf("%s %s\n", ok ? "  ok  " : "  FAIL", c.name);
        ++ran;
        if (!ok) ++bad;
    }
    std::printf("%d/%d passed\n", ran - bad, ran);
    return bad ? 1 : 0;
}

}  // namespace t

#define T_CAT2(a, b) a##b
#define T_CAT(a, b) T_CAT2(a, b)
#define TEST(name)                                                    \
    static void T_CAT(t_fn_, __LINE__)();                             \
    static ::t::Reg T_CAT(t_reg_, __LINE__){name, &T_CAT(t_fn_, __LINE__)}; \
    static void T_CAT(t_fn_, __LINE__)()

#define CHECK(cond) \
    do { if (!(cond)) ::t::fail(__FILE__, __LINE__, "CHECK(" #cond ")"); } while (0)
#define REQUIRE(cond) \
    do { if (!(cond)) { ::t::fail(__FILE__, __LINE__, "REQUIRE(" #cond ")"); throw ::t::Stop{}; } } while (0)
// Approximate compare with the values printed on failure.
#define CHECK_NEAR(a, b, tol)                                                     \
    do {                                                                          \
        const double _a = (a), _b = (b);                                          \
        if (!(std::fabs(_a - _b) <= (tol))) {                                     \
            char _m[256];                                                         \
            std::snprintf(_m, sizeof _m, "CHECK_NEAR(" #a ", " #b ") %g vs %g (tol %g)", _a, _b, double(tol)); \
            ::t::fail(__FILE__, __LINE__, _m);                                    \
        }                                                                         \
    } while (0)
#define CHECK_LT(a, b)                                                            \
    do {                                                                          \
        const double _a = (a), _b = (b);                                          \
        if (!(_a < _b)) {                                                         \
            char _m[256];                                                         \
            std::snprintf(_m, sizeof _m, "CHECK_LT(" #a ", " #b ") %g !< %g", _a, _b); \
            ::t::fail(__FILE__, __LINE__, _m);                                    \
        }                                                                         \
    } while (0)

#define TEST_MAIN() int main(int argc, char** argv) { return ::t::run(argc, argv); }
