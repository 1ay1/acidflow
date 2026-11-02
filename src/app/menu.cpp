// Right-click menus: what you can do with the knob, step or drum under the
// mouse, each with its key so the menu also teaches the shortcut.
#include "app.hpp"

#include <algorithm>

namespace {
std::string step_label(int i) { return "step " + std::to_string(i + 1); }
}  // namespace

void App::open_menu_at(int col, int row) {
    const HitZone z = hit_test(col, row);
    g_menu = {};
    g_menu.x = col - 1;
    g_menu.y = row - 1;
    auto add = [&](std::string label, std::string key, std::function<void(App&)> fn,
                   bool checked = false) {
        g_menu.items.push_back({std::move(label), std::move(key), std::move(fn), checked});
    };
    switch (z.kind) {
        case HitZone::Knob: {
            const int i = z.idx;
            g_focus = Section::Knobs;
            g_knob_sel = i;
            g_menu.title = g_knob_table[i].label;
            if (i == K_WAVE) {
                add("Saw", "w", [](App& a) { a.g_wave = 0; }, g_wave == 0);
                add("Square", "w", [](App& a) { a.g_wave = 1; }, g_wave == 1);
            } else {
                add("Reset to default", "0", [i](App& a) { a.reset_knob(i); });
                add("Bind to a MIDI controller", "l", [i](App& a) {
                    a.g_focus = Section::Knobs; a.g_knob_sel = i; a.arm_midi_learn(i); });
            }
            break;
        }
        case HitZone::FxKnob: {
            const int i = z.idx;
            g_focus = Section::FX;
            g_fx_sel = i;
            g_menu.title = kFxLongName[i];
            if (i == FX_DTIME) {
                for (int d = 0; d < 4; ++d)
                    add(std::string("Delay ") + kDlyDivNames[d], "", [d](App& a) { a.g_delay_div = d; },
                        g_delay_div == d);
            } else {
                add("Reset to default", "0", [i](App& a) { a.reset_fx(i); });
                add("Bind to a MIDI controller", "l", [i](App& a) {
                    a.g_focus = Section::FX; a.g_fx_sel = i; a.arm_midi_learn(kMidiFxBase + i); });
            }
            break;
        }
        case HitZone::Step: {
            const int i = z.idx;
            g_focus = Section::Sequencer;
            g_step_sel = i;
            const auto& s = g_steps[static_cast<size_t>(i)];
            g_menu.title = step_label(i) + (s.rest ? ": rest" : ": " + midi_note_name(s.note));
            auto edit = [i](const char* what, auto fn) {
                return [i, what, fn](App& a) {
                    a.push_undo(what);
                    fn(a.g_steps[static_cast<size_t>(i)]);
                };
            };
            add("Accent", "a", edit("accent", [](StepData& t) { t.accent = !t.accent; t.rest = false; }),
                !s.rest && s.accent);
            add("Slide into next", "s", edit("slide", [](StepData& t) { t.slide = !t.slide; t.rest = false; }),
                !s.rest && s.slide);
            add(s.rest ? "Play this step" : "Make it a rest", "x",
                edit("rest", [](StepData& t) { t.rest = !t.rest; }));
            add("Octave up", ">", edit("octave", [](StepData& t) { t.note = std::min(96, t.note + 12); t.rest = false; }));
            add("Octave down", "<", edit("octave", [](StepData& t) { t.note = std::max(12, t.note - 12); t.rest = false; }));
            add("Chance: always", "v", edit("chance", [](StepData& t) { t.prob = 100; }), s.prob == 100);
            add("Chance: half the time", "v", edit("chance", [](StepData& t) { t.prob = 50; }), s.prob == 50);
            add("Roll (ratchet x2)", "j", edit("ratchet", [](StepData& t) { t.ratchet = t.ratchet == 2 ? 1 : 2; }),
                s.ratchet == 2);
            if (s.lock_mask) add("Clear knob locks", "L", edit("clear locks", [](StepData& t) { t.lock_mask = 0; }));
            add("Clear step", "x", edit("clear step", [](StepData& t) { t = StepData{}; t.rest = true; }));
            break;
        }
        case HitZone::DrumCell: {
            const int v = z.idx2;
            g_focus = Section::Drums;
            g_drum_voice_sel = v;
            if (z.idx >= 0) g_drum_step_sel = z.idx;
            static constexpr const char* kNames[] = {
                "Kick", "Snare", "Closed hat", "Open hat", "Clap", "Low tom", "High tom",
                "Rimshot", "Cowbell", "Shaker", "Tambourine", "Conga", "Mid tom",
                "Crash", "Ride", "Bongo"};
            g_menu.title = kNames[v];
            auto fill = [v](int every, int offset, const char* what) {
                return [v, every, offset, what](App& a) {
                    a.push_undo(what);
                    for (int i = 0; i < 16; ++i)
                        a.g_drums[static_cast<size_t>(v)][static_cast<size_t>(i)] = (i % every) == offset;
                };
            };
            add("Every beat", "", fill(4, 0, "drum fill"));
            add("Off-beats", "", fill(4, 2, "drum fill"));
            add("Every 8th", "", fill(2, 0, "drum fill"));
            add("Backbeat (2 and 4)", "", fill(8, 4, "drum fill"));
            add(g_drum_voice_mute[static_cast<size_t>(v)] ? "Unmute" : "Mute", "s",
                [v](App& a) { auto& m = a.g_drum_voice_mute[static_cast<size_t>(v)]; m = !m; },
                g_drum_voice_mute[static_cast<size_t>(v)]);
            add("Clear row", "c", [v](App& a) {
                a.push_undo("clear drum row");
                for (auto& c : a.g_drums[static_cast<size_t>(v)]) c = false; });
            break;
        }
        default:
            return;
    }
    g_menu.open = !g_menu.items.empty();
    g_menu.sel  = 0;
    g_hover_text.clear();
}

void App::run_menu_item(int i) {
    if (i < 0 || i >= static_cast<int>(g_menu.items.size())) return;
    auto item = g_menu.items[static_cast<size_t>(i)];
    g_menu.open = false;
    item.run(*this);
    set_toast(g_menu.title + ": " + item.label
              + (item.key.empty() ? "" : "   (key " + item.key + ")"));
}

bool App::handle_menu_key(const Event& ev) {
    if (!g_menu.open) return false;
    const int n = static_cast<int>(g_menu.items.size());
    if (key(ev, SpecialKey::Escape) || key(ev, 'q')) { g_menu.open = false; return true; }
    if (key(ev, SpecialKey::Up))   { g_menu.sel = (g_menu.sel - 1 + n) % n; return true; }
    if (key(ev, SpecialKey::Down) || key(ev, SpecialKey::Tab)) { g_menu.sel = (g_menu.sel + 1) % n; return true; }
    if (key(ev, SpecialKey::Enter) || key(ev, ' ')) { run_menu_item(g_menu.sel); return true; }
    return true;   // modal
}

// Mouse while a menu is open: hover highlights, click runs, click outside
// closes. Returns true when the menu took the event.
bool App::handle_menu_mouse(const MouseEvent& m) {
    if (!g_menu.open) return false;
    const int x = m.x.raw() - 1, y = m.y.raw() - 1;
    int hit = -1;
    if (const auto id = maya::hit_test(x, y); id && maya::hit_kind(*id) == tb303::kHitMenuItem)
        hit = static_cast<int>(maya::hit_index(*id));
    if (m.kind == MouseEventKind::Move) {
        if (hit >= 0) g_menu.sel = hit;
        return true;
    }
    if (m.kind == MouseEventKind::Press) {
        if (hit >= 0 && (m.button == MouseButton::Left || m.button == MouseButton::Right)) run_menu_item(hit);
        else g_menu.open = false;   // click outside closes; nothing underneath fires
        return true;
    }
    return true;
}
