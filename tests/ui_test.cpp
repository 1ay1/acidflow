// UI tests: drive the real Program with jaal::headless (no terminal, no
// audio device) and assert on App state and on the rendered frame.
#include "app/program.hpp"
#include "t.hpp"

#include <jaal/host/headless.hpp>
#include <maya/print.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>

using namespace std::chrono_literals;
using H = jaal::headless<Acidflow>;

namespace {

// Keep tests away from the user's real config dir.
struct TempHome {
    std::filesystem::path dir;
    TempHome() {
        const auto tag = std::chrono::steady_clock::now().time_since_epoch().count();
        dir = std::filesystem::temp_directory_path() / ("acidflow_ui_test_" + std::to_string(tag));
        std::filesystem::create_directories(dir);
#if defined(_WIN32)
        _putenv_s("APPDATA", dir.string().c_str());
#else
        ::setenv("HOME", dir.c_str(), 1);
        ::setenv("XDG_CONFIG_HOME", dir.c_str(), 1);
#endif
    }
    ~TempHome() { std::error_code ec; std::filesystem::remove_all(dir, ec); }
};
TempHome g_home;

KeyEvent ch(char32_t c) { return KeyEvent{.key = CharKey{c}, .mods = {}}; }
KeyEvent sp(SpecialKey k) { return KeyEvent{.key = k, .mods = {}}; }
void key(H& h, char32_t c) { h.event(Acidflow::Event{ch(c)}); h.kernel().step(h.record()); }
void key(H& h, SpecialKey k) { h.event(Acidflow::Event{sp(k)}); h.kernel().step(h.record()); }

const App& app(const H& h) { return *h.model().app; }

}  // namespace

TEST("boots into the sequencer with preset 0 loaded") {
    H h;
    CHECK(app(h).g_focus == Section::Sequencer);
    CHECK(app(h).g_preset_index == 0);
    CHECK(!app(h).g_playing);
}

TEST("space starts and stops playback") {
    H h;
    key(h, U' ');
    CHECK(app(h).g_playing);
    key(h, U' ');
    CHECK(!app(h).g_playing);
}

TEST("tab cycles focus through every section and back") {
    H h;
    const Section start = app(h).g_focus;
    int steps = 0;
    do { key(h, SpecialKey::Tab); ++steps; } while (app(h).g_focus != start && steps < 10);
    CHECK(app(h).g_focus == start);
    CHECK(steps == 5);
}

TEST("arrow keys move the step cursor and wrap") {
    H h;
    REQUIRE(app(h).g_focus == Section::Sequencer);
    key(h, SpecialKey::Right);
    CHECK(app(h).g_step_sel == 1);
    key(h, SpecialKey::Left);
    key(h, SpecialKey::Left);
    CHECK(app(h).g_step_sel == app(h).g_pattern_length - 1);
}

TEST("undo restores a step edit, redo reapplies it") {
    H h;
    REQUIRE(app(h).g_focus == Section::Sequencer);
    // Find a played step and clear it with x (undoable).
    int i = 0;
    while (i < 16 && app(h).g_steps[static_cast<size_t>(i)].rest) { key(h, SpecialKey::Right); ++i; }
    REQUIRE(i < 16);
    const auto before = app(h).g_steps;
    key(h, U'x');
    CHECK(app(h).g_steps[static_cast<size_t>(i)].rest);
    key(h, U'u');
    CHECK(!app(h).g_steps[static_cast<size_t>(i)].rest);
    CHECK(app(h).g_steps[static_cast<size_t>(i)].note == before[static_cast<size_t>(i)].note);
    key(h, U'U');
    CHECK(app(h).g_steps[static_cast<size_t>(i)].rest);
}

TEST("q quits with exit code 0") {
    H h;
    key(h, U'q');
    CHECK(h.quit());
    CHECK(std::move(h).finish() == 0);
}

TEST("help overlay opens with ? and q closes it without quitting") {
    H h;
    key(h, U'?');
    CHECK(app(h).g_help_open);
    key(h, U'q');
    CHECK(!app(h).g_help_open);
    CHECK(!h.quit());
}

TEST("idle: slow tick, and input wakes the fast tick") {
    H h;
    h.advance(400ms);
    REQUIRE(h.model().idle);
    const auto r0 = h.model().rev;
    key(h, U' ');
    CHECK(!h.model().idle);
    CHECK(h.model().rev != r0);
    key(h, U' ');
}

TEST("view renders the main panels") {
    H h;
    Element e = Acidflow::view(h.model());
    const std::string s = maya::render_to_string(e, 160);
    CHECK(s.find("TB-303") != std::string::npos);
    CHECK(s.find("SYNTH VOICE") != std::string::npos);
    CHECK(s.find("SEQUENCER") != std::string::npos);
    CHECK(s.find("SCOPE") != std::string::npos);
}

TEST_MAIN()
