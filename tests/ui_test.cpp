// UI tests: drive the real Program with jaal::headless (no terminal, no
// audio device) and assert on App state and on the rendered frame.
#include "app/program.hpp"
#include "app/actions.hpp"
#include "t.hpp"

#include <jaal/host/headless.hpp>
#include <maya/print.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <maya/render/renderer.hpp>

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

// Each test starts as a returning user (welcome already seen) with no saved
// session, so tests don't see each other's state. `fresh` = first-ever run.
void clean_config(bool fresh = false) {
    std::error_code ec;
    const auto dir = g_home.dir / "acidflow";
    std::filesystem::remove_all(dir, ec);
    if (!fresh) {
        std::filesystem::create_directories(dir, ec);
        std::ofstream(dir / "prefs.txt") << "welcomed 1\n";
    }
}

// Render the view at a terminal size the way the app does: the app learns
// the size from a ResizeEvent first.
std::string screen(H& h, int w, int rows) {
    h.event(Acidflow::Event{ResizeEvent{Columns{w}, Rows{rows}}});
    h.kernel().step(h.record());
    StylePool pool;
    Canvas canvas(w, rows, &pool);
    render_tree(Acidflow::view(h.model()), canvas, pool, theme::native);
    std::string out;
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < w; ++x) {
            const char32_t c = canvas.get(x, y).character;
            out += (c >= 0x20 && c < 0x7F) ? static_cast<char>(c) : ' ';
        }
        out += '\n';
    }
    return out;
}
bool has(const std::string& s, const char* what) { return s.find(what) != std::string::npos; }
std::string lower_copy(std::string s) {
    for (auto& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

struct Clean { Clean(bool fresh = false) { clean_config(fresh); } };

}  // namespace

TEST("boots into the sequencer with preset 0 loaded") {
    Clean c;
    H h;
    CHECK(app(h).g_focus == Section::Sequencer);
    CHECK(app(h).g_preset_index == 0);
    CHECK(!app(h).g_playing);
}

TEST("space starts and stops playback") {
    Clean c;
    H h;
    key(h, U' ');
    CHECK(app(h).g_playing);
    key(h, U' ');
    CHECK(!app(h).g_playing);
}

TEST("tab cycles focus through every section and back") {
    Clean c;
    H h;
    const Section start = app(h).g_focus;
    int steps = 0;
    do { key(h, SpecialKey::Tab); ++steps; } while (app(h).g_focus != start && steps < 10);
    CHECK(app(h).g_focus == start);
    CHECK(steps == 5);
}

TEST("arrow keys move the step cursor and wrap") {
    Clean c;
    H h;
    REQUIRE(app(h).g_focus == Section::Sequencer);
    key(h, SpecialKey::Right);
    CHECK(app(h).g_step_sel == 1);
    key(h, SpecialKey::Left);
    key(h, SpecialKey::Left);
    CHECK(app(h).g_step_sel == app(h).g_pattern_length - 1);
}

TEST("undo restores a step edit, redo reapplies it") {
    Clean c;
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
    Clean c;
    H h;
    key(h, U'q');
    CHECK(h.quit());
    CHECK(std::move(h).finish() == 0);
}

TEST("help opens on the focused panel; Esc closes it without quitting") {
    Clean c;
    H h;
    key(h, U'?');
    CHECK(app(h).g_help_open);
    CHECK(app(h).g_help_from == Section::Sequencer);
    // Typing searches instead of acting on the workspace.
    key(h, U'q');
    CHECK(app(h).g_help_open);
    CHECK(app(h).g_help_query == "q");
    CHECK(!h.quit());
    key(h, SpecialKey::Escape);     // clears the search
    CHECK(app(h).g_help_open);
    CHECK(app(h).g_help_query.empty());
    key(h, SpecialKey::Escape);     // closes help
    CHECK(!app(h).g_help_open);
    CHECK(!h.quit());
}

TEST("help search finds actions and words, and says when nothing matches") {
    Clean c;
    H h;
    key(h, U'?');
    for (char32_t ch2 : std::u32string(U"slide")) key(h, ch2);
    std::string s = screen(h, 140, 50);
    CHECK(has(s, "Slide"));
    CHECK(has(s, "Glide from this note"));
    CHECK(!has(s, "Pick a knob"));          // unrelated actions are filtered out
    for (int i = 0; i < 5; ++i) key(h, SpecialKey::Backspace);
    for (char32_t ch2 : std::u32string(U"zzzq")) key(h, ch2);
    s = screen(h, 140, 50);
    CHECK(has(s, "Nothing matches"));
}

TEST("command palette finds and runs an action") {
    Clean c;
    H h;
    const float bpm = app(h).g_bpm;
    key(h, U':');
    REQUIRE(app(h).g_palette_open);
    for (char32_t ch2 : std::u32string(U"tempo up")) key(h, ch2);
    std::string s = screen(h, 140, 50);
    CHECK(has(s, "COMMANDS"));
    CHECK(has(s, "Tempo up"));
    key(h, SpecialKey::Enter);
    CHECK(!app(h).g_palette_open);
    CHECK(app(h).g_bpm == bpm + 2.0f);
    CHECK(app(h).g_focus == Section::Transport);   // went to where tempo lives
}

TEST("palette: key-only actions teach their key, Esc closes") {
    Clean c;
    H h;
    key(h, U':');
    for (char32_t ch2 : std::u32string(U"accent")) key(h, ch2);
    key(h, SpecialKey::Enter);
    CHECK(!app(h).g_palette_open);
    CHECK(has(app(h).g_toast, "press a"));
    key(h, U':');
    key(h, SpecialKey::Escape);
    CHECK(!app(h).g_palette_open);
    CHECK(!h.quit());
}

TEST("first run shows the welcome card; Enter starts playing") {
    Clean c(/*fresh=*/true);
    H h;
    REQUIRE(app(h).g_first_run);
    CHECK(has(screen(h, 120, 40), "welcome to acidflow"));
    key(h, SpecialKey::Enter);
    CHECK(!app(h).g_first_run);
    CHECK(app(h).g_playing);
    CHECK(!has(screen(h, 120, 40), "welcome to acidflow"));
    key(h, U' ');
}

TEST("session: knobs and pattern come back after quitting") {
    Clean c;
    float cutoff = 0;
    {
        H h;
        auto& a = const_cast<App&>(app(h));
        a.g_cutoff = 0.123f;
        a.g_steps[3].note = 50;
        a.g_steps[3].rest = false;
        cutoff = a.g_cutoff;
    }   // ~App saves the session
    H h2;
    CHECK_NEAR(app(h2).g_cutoff, cutoff, 1e-3);
    CHECK(app(h2).g_steps[3].note == 50);
    CHECK(!app(h2).g_steps[3].rest);
}

TEST("undo toast names what it undid") {
    Clean c;
    H h;
    key(h, U'r');                   // randomize the sequencer
    CHECK(has(app(h).g_toast, "u to undo"));
    key(h, U'u');
    CHECK(has(app(h).g_toast, "undid randomize pattern"));
}

// The table that help and the palette are built from has to match the key
// handler. For each single-character key listed, pressing it in the panel it
// belongs to must change something visible (state, toast or an overlay).
TEST("every single-key action listed in help does something") {
    // Keys that need setup or a device, checked elsewhere or by hand.
    const std::string skip = "qkWYEepPAOIKl.";
    int checked = 0;
    for (const Action& a : actions()) {
        std::string k = a.keys;
        if (k.size() != 1 || skip.find(k[0]) != std::string::npos) continue;
        Clean c;
        H h;
        auto& ap = const_cast<App&>(app(h));
        if (a.scope != Scope::Global) {
            switch (a.scope) {
                case Scope::Knobs:     ap.g_focus = Section::Knobs; break;
                case Scope::FX:        ap.g_focus = Section::FX; break;
                case Scope::Sequencer: ap.g_focus = Section::Sequencer; break;
                case Scope::Drums:     ap.g_focus = Section::Drums; break;
                case Scope::Transport: ap.g_focus = Section::Transport; break;
                default: break;
            }
        }
        // A played step under the cursor, so step edits have something to do.
        ap.g_steps[0].rest = false;
        ap.g_knob_sel = K_CUTOFF;
        const std::string before = screen(h, 160, 50);
        ap.g_toast.clear();
        key(h, static_cast<char32_t>(k[0]));
        const std::string after = screen(h, 160, 50);
        const bool did = before != after || !ap.g_toast.empty() || ap.g_help_open || ap.g_palette_open;
        if (!did) std::printf("      key '%s' (%s, %s) did nothing\n", a.keys, a.name, scope_name(a.scope));
        CHECK(did);
        ++checked;
    }
    std::printf("      %d keys checked\n", checked);
    CHECK(checked > 15);
}

// An overlay floats over the workspace; it must not shrink it.
TEST("palette and welcome card keep the full screen height") {
    auto last_row_with = [](const std::string& s, const char* what) {
        int row = 0, best = -1;
        size_t pos = 0;
        while (pos < s.size()) {
            const size_t nl = s.find('\n', pos);
            if (s.substr(pos, nl - pos).find(what) != std::string::npos) best = row;
            ++row;
            pos = nl + 1;
        }
        return best;
    };
    for (int rows : {24, 40, 60}) {
        Clean c;
        H h;
        key(h, U':');
        CHECK(last_row_with(screen(h, 120, rows), ": commands") == rows - 1);
    }
    for (int rows : {24, 60}) {
        Clean c(/*fresh=*/true);
        H h;
        const std::string s = screen(h, 120, rows);
        CHECK(has(s, "welcome to acidflow"));
        CHECK(last_row_with(s, ": commands") == rows - 1);
    }
}

// Right-click a step: a menu opens with what you can do there; choosing an
// item does it, Esc closes without doing anything.
TEST("right-click menu on a step edits that step") {
    Clean c;
    H h;
    auto& ap = const_cast<App&>(app(h));
    screen(h, 120, 40);                       // paint once so hit targets exist
    ap.open_menu_at(0, 0);                    // nothing there: no menu
    CHECK(!ap.g_menu.open);
    // Find step 5 on screen through the hit registry.
    int sx = -1, sy = -1;
    for (int y = 0; y < 40 && sx < 0; ++y)
        for (int x = 0; x < 120; ++x) {
            const HitZone z = ap.hit_test(x + 1, y + 1);
            if (z.kind == HitZone::Step && z.idx == 4) { sx = x + 1; sy = y + 1; break; }
        }
    REQUIRE(sx > 0);
    ap.g_steps[4].slide = false;
    ap.open_menu_at(sx, sy);
    REQUIRE(ap.g_menu.open);
    CHECK(has(ap.g_menu.title, "step 5"));
    CHECK(has(screen(h, 120, 40), "Slide into next"));
    // Pick "Slide into next" with the keyboard.
    int i = 0;
    while (i < static_cast<int>(ap.g_menu.items.size()) && ap.g_menu.items[static_cast<size_t>(i)].label != "Slide into next") ++i;
    REQUIRE(i < static_cast<int>(ap.g_menu.items.size()));
    while (ap.g_menu.sel != i) key(h, SpecialKey::Down);
    key(h, SpecialKey::Enter);
    CHECK(!ap.g_menu.open);
    CHECK(ap.g_steps[4].slide);
    CHECK(has(ap.g_toast, "(key s)"));       // the menu teaches the key
    key(h, U'u');                             // and it's undoable
    CHECK(!ap.g_steps[4].slide);
    // Esc closes without changing anything.
    ap.open_menu_at(sx, sy);
    key(h, SpecialKey::Escape);
    CHECK(!ap.g_menu.open);
    CHECK(!h.quit());
}

TEST("right-click menu: clicking an item runs it, clicking outside closes") {
    Clean c;
    H h;
    auto& ap = const_cast<App&>(app(h));
    screen(h, 120, 40);
    int sx = -1, sy = -1;
    for (int y = 0; y < 40 && sx < 0; ++y)
        for (int x = 0; x < 120; ++x) {
            const HitZone z = ap.hit_test(x + 1, y + 1);
            if (z.kind == HitZone::Step && z.idx == 4) { sx = x + 1; sy = y + 1; break; }
        }
    REQUIRE(sx > 0);
    ap.g_steps[4].accent = false;
    auto mouse = [&](MouseButton b, MouseEventKind k, int x, int y) {
        h.event(Acidflow::Event{MouseEvent{.button = b, .kind = k, .x = Columns{x}, .y = Rows{y}, .mods = {}}});
        h.kernel().step(h.record());
    };
    mouse(MouseButton::Right, MouseEventKind::Press, sx, sy);
    mouse(MouseButton::Right, MouseEventKind::Release, sx, sy);
    REQUIRE(ap.g_menu.open);
    screen(h, 120, 40);                       // paint the menu so its rows are targets
    // Find the "Accent" row on screen and click it.
    int ax = -1, ay = -1;
    for (int y = 0; y < 40 && ax < 0; ++y)
        for (int x = 0; x < 120; ++x)
            if (auto id = maya::hit_test(x, y); id && maya::hit_kind(*id) == tb303::kHitMenuItem
                && ap.g_menu.items[maya::hit_index(*id)].label == "Accent") { ax = x + 1; ay = y + 1; break; }
    REQUIRE(ax > 0);
    mouse(MouseButton::Left, MouseEventKind::Press, ax, ay);
    CHECK(!ap.g_menu.open);
    CHECK(ap.g_steps[4].accent);
    // Open again and click far away: closes, nothing underneath fires.
    mouse(MouseButton::Right, MouseEventKind::Press, sx, sy);
    REQUIRE(ap.g_menu.open);
    const bool playing = ap.g_playing;
    mouse(MouseButton::Left, MouseEventKind::Press, 2, 2);
    CHECK(!ap.g_menu.open);
    CHECK(ap.g_playing == playing);
}

// The scope's auto-gain should make a playing pattern fill most of the
// panel, without clipping most of the time. Notes are gated now, so only
// the ticks that caught sound count (the gaps are silence, as on a 303).
TEST("scope trace uses most of the height during playback") {
    acid::Engine e(44100);
    acid::Engine::Bind b(e);
    acid_set_cutoff(0.3f); acid_set_resonance(0.85f); acid_set_env_mod(0.7f);
    acid_set_decay(0.35f); acid_set_volume(0.65f);
    const int notes[16] = {36, 36, 48, 36, 39, 36, 43, 46, 36, 36, 48, 51, 36, 34, 36, 41};
    for (int i = 0; i < 16; ++i) acid_seq_set_step(i, notes[i], i % 4 == 0 ? 2 : 0, 100, 1);
    acid_seq_set_pattern_length(16);
    acid_seq_set_bpm(122.0f);
    acid_seq_play();
    tb303::ScopeState st;
    std::vector<float> buf(2 * 1470);           // one 30 Hz UI tick of audio
    for (int t = 0; t < 60; ++t) { e.render(buf.data(), 1470); st.advance(); }   // settle
    double used = 0;
    int clipped = 0, n = 0;
    for (int t = 0; t < 200; ++t) {
        e.render(buf.data(), 1470);
        st.advance();
        float mx = 0;
        for (float v : st.traces[static_cast<size_t>(st.newest)]) mx = std::max(mx, std::fabs(v));
        if (mx < 0.05f) continue;               // between notes
        ++n;
        used += std::min(1.0f, mx * st.gain);
        clipped += mx * st.gain > 1.0f;
    }
    std::printf("      mean span %.0f%%, clipped %d/%d\n", 100 * used / n, clipped, n);
    // Notes vary a lot in level (accents, filter sweeps), so the mean sits
    // well under the 85% target; what matters is that loud notes fill it
    // and almost none clip.
    CHECK(used / n > 0.5);
    CHECK(clipped < n / 20);
}

// Themes are maya themes: T installs the next one in maya, every slot colour
// follows it, and the choice comes back by name next launch.
TEST("T switches maya's live theme and it is restored by name") {
    Clean c;
    {
        H h;
        REQUIRE(tb303::g_theme_name == "acid");
        const auto acid = theme::live().resolve(tb303::clr_accent());
        // T opens the picker; moving previews, Enter keeps.
        key(h, U'T');
        REQUIRE(app(h).g_theme_picker);
        CHECK(has(screen(h, 120, 40), "THEME"));
        key(h, SpecialKey::Down);
        CHECK(tb303::g_theme_name == "terminal");
        CHECK(theme::live() == theme::native);
        key(h, SpecialKey::Down);
        CHECK(tb303::g_theme_name == "gruvbox dark");
        CHECK(theme::live() == theme::gruvbox_dark);
        CHECK(!(theme::live().resolve(tb303::clr_accent()) == acid));
        key(h, SpecialKey::Enter);
        CHECK(!app(h).g_theme_picker);
        CHECK(has(app(h).g_toast, "gruvbox dark"));
    }   // quit saves prefs
    tb303::set_theme(0);
    H h2;
    CHECK(tb303::g_theme_name == "gruvbox dark");
    CHECK(theme::live() == theme::gruvbox_dark);
    tb303::set_theme(0);
}

TEST("theme picker: Esc puts back the old theme; search reaches every maya scheme") {
    Clean c;
    H h;
    REQUIRE(tb303::g_theme_name == "acid");
    key(h, U'T');
    key(h, SpecialKey::Down);
    key(h, SpecialKey::Down);
    CHECK(tb303::g_theme_name == "gruvbox dark");
    key(h, SpecialKey::Escape);                 // cancel
    CHECK(!app(h).g_theme_picker);
    CHECK(tb303::g_theme_name == "acid");
    CHECK(theme::live() == tb303::kAcidTheme);
    // Every maya scheme is in the list.
    CHECK(tb303::all_themes().size() >= std::size(theme::schemes));
    // Search previews the first match; first Esc clears the search only.
    key(h, U'T');
    for (char32_t ch2 : std::u32string(U"kanagawa")) key(h, ch2);
    CHECK(has(lower_copy(tb303::g_theme_name), "kanagawa"));
    CHECK(has(screen(h, 120, 40), "Kanagawa"));
    key(h, SpecialKey::Escape);
    CHECK(app(h).g_theme_picker);
    CHECK(app(h).g_theme_query.empty());
    key(h, SpecialKey::Escape);
    CHECK(tb303::g_theme_name == "acid");
    CHECK(!h.quit());
}

// The layout at common terminal sizes: every panel is there, labels aren't
// cut, and the way to help is on screen.
TEST("layout holds at 80x24, 100x30, 120x40 and 160x50") {
    struct Sz { int w, h; };
    for (Sz z : {Sz{80, 24}, Sz{100, 30}, Sz{120, 40}, Sz{160, 50}}) {
        Clean c;
        H h;
        const std::string s = screen(h, z.w, z.h);
        const bool compact = z.h < kCompactRows;
        std::printf("      %dx%d %s\n", z.w, z.h, compact ? "(compact)" : "");
        CHECK(has(s, "TB-303"));
        CHECK(has(s, "SEQUENCER"));
        CHECK(has(s, "TRANSPORT"));
        CHECK(has(s, "DRUMS"));
        CHECK(has(s, "BD:"));                 // drum names never clip
        CHECK(has(s, "SD:"));
        CHECK(has(s, ": commands"));          // the way to everything
        CHECK(has(s, "? help"));
        CHECK(has(s, "Acid Tracks"));         // a preset is visible
        if (compact) {
            CHECK(has(s, "CUT"));
            CHECK(has(s, "141 Hz"));
        } else {
            CHECK(has(s, "CUTOFF"));
            CHECK(has(s, "SCOPE"));
            CHECK(has(s, "FDBK"));            // FX labels fit their knobs
            CHECK(has(s, "DAMP"));
        }
    }
}

TEST("idle: slow tick, and input wakes the fast tick") {
    Clean c;
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
    Clean c;
    H h;
    Element e = Acidflow::view(h.model());
    const std::string s = maya::render_to_string(e, 160);
    CHECK(s.find("TB-303") != std::string::npos);
    CHECK(s.find("SYNTH VOICE") != std::string::npos);
    CHECK(s.find("SEQUENCER") != std::string::npos);
    CHECK(s.find("SCOPE") != std::string::npos);
}

TEST_MAIN()
