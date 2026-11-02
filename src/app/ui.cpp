// Discoverability and onboarding: the command palette, help search, the
// welcome card, session save/restore, and the actions the palette runs.
#include "actions.hpp"
#include "app.hpp"

#include <algorithm>
#include <fstream>

// ── Palette ─────────────────────────────────────────────────────────────────

// Everything the palette can run, best match first. With no query: the
// focused panel's actions, then the global ones, in table order.
std::vector<const Action*> App::palette_matches() const {
    std::vector<std::pair<int, const Action*>> scored;
    const Scope here = scope_of(g_focus);
    int order = 0;
    for (const Action* a : actions_for(here)) {
        ++order;
        // Key-only entries (arrows, typing a note) still show when searched
        // for, so the palette teaches the key even when it can't press it.
        if (!a->run && g_palette_query.empty()) continue;
        const int s = fuzzy_score(g_palette_query, *a);
        if (s < 0) continue;
        scored.push_back({s * 100 - order - (a->run ? 0 : 20), a});
    }
    // Actions from other panels, when the query names them.
    if (!g_palette_query.empty()) {
        for (const auto& a : actions()) {
            if (a.scope == here || a.scope == Scope::Global) continue;
            if (a.run && !a.palette_anywhere) continue;
            const int s = fuzzy_score(g_palette_query, a);
            if (s >= 0) scored.push_back({s * 100 - 50 - (a.run ? 0 : 20), &a});
        }
    }
    std::stable_sort(scored.begin(), scored.end(),
                     [](auto& x, auto& y) { return x.first > y.first; });
    std::vector<const Action*> out;
    for (auto& [s, a] : scored) out.push_back(a);
    return out;
}

void App::open_palette() {
    g_palette_open  = true;
    g_palette_query.clear();
    g_palette_sel   = 0;
    g_first_run     = false;
}

void App::run_action(const Action& a) {
    // Panel actions run on that panel, so focus goes there first.
    switch (a.scope) {
        case Scope::Knobs:     g_focus = Section::Knobs;     break;
        case Scope::FX:        g_focus = Section::FX;        break;
        case Scope::Sequencer: g_focus = Section::Sequencer; break;
        case Scope::Drums:     g_focus = Section::Drums;     break;
        case Scope::Transport: g_focus = Section::Transport; break;
        case Scope::Global:    break;
    }
    if (!a.run) {
        // A key-only action: we've moved focus to it; say which key to press.
        set_toast(std::string(a.name) + ":  press " + a.keys);
        return;
    }
    a.run(*this);
}

namespace {
// Printable text from a key event (for typing into a search box).
bool typed_char(const Event& ev, std::string& into) {
    const KeyEvent* k = as_key(ev);
    if (!k) return false;
    const auto* ck = std::get_if<CharKey>(&k->key);
    if (!ck || k->mods.ctrl || k->mods.alt) return false;
    const char32_t c = ck->codepoint;
    if (c < 0x20 || c > 0x7E) return false;
    into.push_back(static_cast<char>(c));
    return true;
}
}  // namespace

// Keys while the palette is open. Returns true when it used the key.
bool App::handle_palette_key(const Event& ev) {
    if (!g_palette_open) return false;
    const auto matches = palette_matches();
    const int n = static_cast<int>(matches.size());
    if (key(ev, SpecialKey::Escape) || ctrl(ev, 'k') || (key(ev, ':') && g_palette_query.empty())) {
        g_palette_open = false;
        return true;
    }
    if (key(ev, SpecialKey::Up) || ctrl(ev, 'p')) {
        if (n) g_palette_sel = (g_palette_sel - 1 + n) % n;
        return true;
    }
    if (key(ev, SpecialKey::Down) || ctrl(ev, 'n') || key(ev, SpecialKey::Tab)) {
        if (n) g_palette_sel = (g_palette_sel + 1) % n;
        return true;
    }
    if (key(ev, SpecialKey::Enter)) {
        if (n) {
            const Action* a = matches[static_cast<size_t>(std::clamp(g_palette_sel, 0, n - 1))];
            g_palette_open = false;
            run_action(*a);
        }
        return true;
    }
    if (key(ev, SpecialKey::Backspace)) {
        if (!g_palette_query.empty()) g_palette_query.pop_back();
        g_palette_sel = 0;
        return true;
    }
    if (typed_char(ev, g_palette_query)) { g_palette_sel = 0; return true; }
    return true;   // modal: nothing leaks to the workspace underneath
}

// Keys while help is open: type to search, Esc clears the search or closes.
bool App::handle_help_key(const Event& ev) {
    if (!g_help_open) return false;
    if (key(ev, SpecialKey::Escape)) {
        if (!g_help_query.empty()) { g_help_query.clear(); g_help_ov_scroll = 0; }
        else g_help_open = false;
        return true;
    }
    if (key(ev, '?') && g_help_query.empty()) { g_help_open = false; return true; }
    if (key(ev, SpecialKey::Up))       { g_help_ov_scroll = std::max(0, g_help_ov_scroll - 1); return true; }
    if (key(ev, SpecialKey::Down))     { ++g_help_ov_scroll; return true; }
    if (key(ev, SpecialKey::PageUp))   { g_help_ov_scroll = std::max(0, g_help_ov_scroll - 10); return true; }
    if (key(ev, SpecialKey::PageDown)) { g_help_ov_scroll += 10; return true; }
    // Tab flips between the panels' help pages without leaving help.
    if (key(ev, SpecialKey::Tab) || key(ev, SpecialKey::BackTab)) {
        int i = static_cast<int>(g_help_from) + (key(ev, SpecialKey::BackTab) ? 4 : 1);
        g_help_from = static_cast<Section>(i % 5);
        g_help_ov_scroll = 0;
        return true;
    }
    if (key(ev, SpecialKey::Backspace)) {
        if (!g_help_query.empty()) g_help_query.pop_back();
        g_help_ov_scroll = 0;
        return true;
    }
    if (typed_char(ev, g_help_query)) { g_help_ov_scroll = 0; return true; }
    return true;
}

// ── Actions the palette runs ────────────────────────────────────────────────

void App::randomize_focused() {
    switch (g_focus) {
        case Section::Knobs:     push_undo("randomize knobs");   randomize_knobs();     break;
        case Section::FX:        push_undo("randomize FX");      randomize_fx();        break;
        case Section::Sequencer: push_undo("randomize pattern"); randomize_pattern();   break;
        case Section::Drums:     push_undo("randomize drums");   randomize_drums();     break;
        case Section::Transport: push_undo("randomize tempo");   randomize_transport(); break;
    }
    // The randomizer named what it rolled; add the undo hint to that.
    toast_undoable(g_toast);
}

void App::toggle_jam() {
    if (g_jam_mode) {
        g_jam_mode = false;
        acid_note_off();
        g_jam_gate_s = 0.0f;
        g_jam_last_midi = -1;
        set_toast("jam off");
        return;
    }
    g_jam_mode = true;
    g_jam_gate_s = 0.0f;
    g_jam_last_midi = -1;
    set_toast("\xe2\x99\xac jam: play z\xe2\x80\x93m / q\xe2\x80\x93u  \xc2\xb7  \xe2\x86\x90\xe2\x86\x92 octave  \xc2\xb7  k or Esc to stop");
}

void App::toggle_synth_mute() {
    g_synth_mute = !g_synth_mute;
    set_toast(g_synth_mute ? "bass muted  \xc2\xb7  m again to unmute" : "bass live");
}
void App::toggle_drums_mute() {
    g_drums_mute = !g_drums_mute;
    set_toast(g_drums_mute ? "drums muted  \xc2\xb7  m again to unmute" : "drums live");
}
void App::toggle_all_mute() {
    const bool any_live = !g_synth_mute || !g_drums_mute;
    g_synth_mute = any_live;
    g_drums_mute = any_live;
    set_toast(any_live ? "all muted  \xc2\xb7  m again to unmute" : "all live");
}

// ── Session ─────────────────────────────────────────────────────────────────
// session.txt is a normal slot file (pattern, drums, knobs, FX), so it uses
// the same reader as slots and takes. prefs.txt holds the rest.

void App::save_session() {
    save_slot_to(config_dir() / "session.txt");
    std::ofstream f(config_dir() / "prefs.txt");
    if (g_theme_chosen) f << "theme " << tb303::g_theme_idx << "\n";
    f << "focus " << static_cast<int>(g_focus) << "\n"
      << "welcomed " << (g_first_run ? 0 : 1) << "\n"
      << "undo_hints " << g_undo_hints_left << "\n";
}

void App::load_session() {
    // NO_COLOR (no-color.org): start in the greyscale theme. A theme the user
    // picked and saved still wins.
    if (const char* nc = std::getenv("NO_COLOR"); nc && *nc) tb303::set_theme(tb303::kThemeCount - 1);
    std::error_code ec;
    const auto sess = config_dir() / "session.txt";
    if (std::filesystem::exists(sess, ec)) {
        load_slot_from_path(sess, "session", /*quiet=*/true);
        g_preset_index = -1;
    }
    std::ifstream f(config_dir() / "prefs.txt");
    std::string k;
    int v = 0;
    while (f >> k >> v) {
        if (k == "theme") { tb303::set_theme(v); g_theme_chosen = true; }
        else if (k == "focus" && v >= 0 && v < 5) g_focus = static_cast<Section>(v);
        else if (k == "welcomed") g_first_run = v == 0;
        else if (k == "undo_hints") g_undo_hints_left = std::clamp(v, 0, 4);
    }
}
