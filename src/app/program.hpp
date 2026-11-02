#pragma once
// The jaal Program around App. Header-only so the UI tests can drive it
// with jaal::headless, without a terminal.

#include "app.hpp"

#include <maya/host/run.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <variant>

// The Model owns the App. App holds tables that point into itself, so it
// lives behind a unique_ptr (stable address; the Model stays movable).
// on(), update() and view() all run on jaal's loop thread; the audio and
// MIDI threads only talk to the engine through atomics.
struct Acidflow {
    struct Model {
        std::unique_ptr<App> app = std::make_unique<App>();
        std::chrono::steady_clock::time_point last{};
        bool idle = false;
        // Bumped whenever something visible may have changed; visual_hash
        // reads it so idle ticks with nothing new skip the rebuild.
        std::uint64_t rev = 0;
    };

    struct Tick {};
    struct Key    { KeyEvent ev; };
    struct Mouse  { MouseEvent ev; };
    struct Resize { int w, h; };
    using Msg   = std::variant<Tick, Key, Mouse, Resize>;
    using Cmd   = jaal::Cmd<Msg>;
    using Sub   = jaal::Sub<Msg>;
    using Event = maya::terminal_events;

    static Cmd init(Model& m) {
        m.last = std::chrono::steady_clock::now();
        return {};
    }

    static Cmd update(Model& m, Tick) {
        auto now = std::chrono::steady_clock::now();
        // Clamp so a debugger pause or a slow first frame can't jump timers.
        float dt = std::clamp(std::chrono::duration<float>(now - m.last).count(), 0.0f, 0.1f);
        m.last = now;
        App& a = *m.app;
        a.tick(dt);
        if (a.g_toast_t <= 0.0f && !a.g_toast.empty()) a.g_toast.clear();
        m.idle = a.ui_idle();
        // While idle the frame only changes when the marquee chip turns.
        m.rev = m.idle ? (m.rev & ~0xFFFFull) | a.idle_visual_key() : m.rev + 0x10000;
        return {};
    }
    static Cmd update(Model& m, Key k)    { return input(m, Event{k.ev}); }
    static Cmd update(Model& m, Mouse mo) { return input(m, Event{mo.ev}); }
    static Cmd update(Model& m, Resize r) {
        return input(m, Event{ResizeEvent{Columns{r.w}, Rows{r.h}}});
    }

    static Element view(const Model& m) { return m.app->render_frame(); }
    static std::uint64_t visual_hash(const Model& m) { return m.rev; }

    static std::optional<Msg> on(const Model&, const KeyEvent& k)   { return Key{k}; }
    static std::optional<Msg> on(const Model&, const MouseEvent& m) { return Mouse{m}; }
    static std::optional<Msg> on(const Model&, const ResizeEvent& r) {
        return Resize{r.width.value, r.height.value};
    }
    static std::optional<Msg> on(const Model&, const PasteEvent&) { return std::nullopt; }
    static std::optional<Msg> on(const Model&, const FocusEvent&) { return std::nullopt; }

    // 30 fps while anything moves; a slow tick when idle so the help bar
    // marquee still turns but the process doesn't spin.
    static Sub subscribe(const Model& m) {
        return Sub::every(std::chrono::milliseconds{m.idle ? 250 : 33}, Tick{});
    }
    static bool subs_key(const Model& m) { return m.idle; }

  private:
    static Cmd input(Model& m, const Event& ev) {
        m.app->handle_event(ev);
        // Input wakes the fast tick right away (a knob turn, play, ...).
        m.idle = false;
        m.rev += 0x10000;
        if (m.app->g_quit_requested) return Cmd::quit(0);
        return {};
    }
};

static_assert(maya::Program<Acidflow>);
