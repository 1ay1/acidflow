// Keyboard and mouse handling.
#include "actions.hpp"
#include "app.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// Input handling
// ─────────────────────────────────────────────────────────────────────────────
void App::adjust_knob(int idx, float delta) {
    if (idx == K_WAVE) {
        // wave is special-cased; treat up/down as toggle
        if (delta > 0) g_wave = 1 - g_wave;
        else           g_wave = 1 - g_wave;
        return;
    }
    float* v = g_knob_table[idx].value;
    *v = std::clamp(*v + delta, 0.0f, 1.0f);
}


void App::reset_knob(int idx) {
    if (idx == K_WAVE) { g_wave = 0; return; }
    *g_knob_table[idx].value = g_knob_table[idx].default_v;
}

// FX knob adjustment. FX_DTIME is a 4-way enum (1/16..1/8d) so ↑/↓ step
// through the divisions rather than adjusting a continuous value.
void App::adjust_fx(int idx, float delta) {
    if (idx == FX_DTIME) {
        int d = g_delay_div + (delta > 0 ? +1 : -1);
        g_delay_div = std::clamp(d, 0, 3);
        return;
    }
    float* v = g_fx_table[idx].value;
    *v = std::clamp(*v + delta, 0.0f, 1.0f);
}


void App::reset_fx(int idx) {
    if (idx == FX_DTIME) { g_delay_div = 2; return; }
    *g_fx_table[idx].value = g_fx_table[idx].default_v;
}

// Cycle focus in the order the panels read on screen: synth voice, FX (top
// row), transport, drums (middle), sequencer (bottom).
void App::cycle_focus(int dir) {
    static constexpr Section kOrder[] = {Section::Knobs, Section::FX, Section::Transport,
                                         Section::Drums, Section::Sequencer};
    constexpr int n = 5;
    int cur = 0;
    while (cur < n && kOrder[cur] != g_focus) ++cur;
    cur = ((cur + dir) % n + n) % n;
    g_focus = kOrder[cur];
}

// Jam-mode piano layout — classic tracker / FL "piano keys" mapping across
// the lower two QWERTY rows. Lower-case letters only; Shift is reserved for
// section cycling. Returns semitones above the jam base octave, or -1 if the
// key isn't part of the piano.
int App::jam_key_semi(char c) {
    switch (c) {
        // Lower octave (z..m) — white on the bottom row, black on the row above
        case 'z': return 0;   case 's': return 1;
        case 'x': return 2;   case 'd': return 3;
        case 'c': return 4;
        case 'v': return 5;   case 'g': return 6;
        case 'b': return 7;   case 'h': return 8;
        case 'n': return 9;   case 'j': return 10;
        case 'm': return 11;
        // Upper octave (q..u) — white on the top row, black on the numbers row
        case 'q': return 12;  case '2': return 13;
        case 'w': return 14;  case '3': return 15;
        case 'e': return 16;
        case 'r': return 17;  case '5': return 18;
        case 't': return 19;  case '6': return 20;
        case 'y': return 21;  case '7': return 22;
        case 'u': return 23;
        default:  return -1;
    }
}

// Pretty-print a MIDI note as "C3", "F#4", etc. — shown in the jam HUD so the
// user can read the last-played pitch at a glance.
std::string App::midi_note_name(int midi) const {
    static constexpr const char* n[12] = {
        "C","C#","D","D#","E","F","F#","G","G#","A","A#","B",
    };
    char buf[8];
    int pc = ((midi % 12) + 12) % 12;
    int oct = (midi / 12) - 1;      // MIDI 0 = C-1, MIDI 60 = C4
    std::snprintf(buf, sizeof(buf), "%s%d", n[pc], oct);
    return buf;
}

// map a typed letter (c, d, e, f, g, a, b) to a MIDI semitone (0=C .. 11=B)
int App::letter_to_semitone(char c) {
    switch (c) {
        case 'c': return 0;
        case 'd': return 2;
        case 'e': return 4;
        case 'f': return 5;
        case 'g': return 7;
        case 'a': return 9;
        case 'b': return 11;
        default:  return -1;
    }
}

// Resolve a mouse position to a target via maya's hit registry. The widgets
// tag their boxes with `| hit(...)`, so this always matches the painted frame.
// SGR coordinates are 1-based; the registry is 0-based.
HitZone App::hit_test(int col, int row) {
    const int x = col - 1;
    const int y = row - 1;
    const auto id = maya::hit_test(x, y);
    if (!id) return {};
    const int idx = static_cast<int>(maya::hit_index(*id));
    // x offset inside the tagged box, for rows that pick a step by column.
    auto rel_x = [&]() -> int {
        auto r = maya::hit_rect(*id);
        return r ? x - r->x : -1;
    };
    switch (maya::hit_kind(*id)) {
        case tb303::kHitTitle:       return {HitZone::Title, 0, 0};
        case tb303::kHitHelpBar:     return {HitZone::HelpBar, 0, 0};
        case tb303::kHitHelpOverlay: return {HitZone::HelpOverlay, 0, 0};
        case tb303::kHitKnob:        return {HitZone::Knob, idx, 0};
        case tb303::kHitFx:          return {HitZone::FxKnob, idx, 0};
        case tb303::kHitTransport:   return {HitZone::TransportArea, 0, 0};
        case tb303::kHitScope:       return {HitZone::ScopeArea, 0, 0};
        case tb303::kHitSeqRow: {
            int rel = rel_x() - tb303::kSeqCellX0;
            if (rel < 0) return {};
            int step = rel / tb303::kSeqCellW;
            if (step >= 16) return {};
            return {HitZone::Step, step, 0};
        }
        case tb303::kHitDrumRow: {
            int rel = rel_x();
            if (rel < 0) return {};
            // Cells adapt to the panel width (3, 2 or 1 columns); the tagged
            // row is exactly label + 16 cells, so its width gives the size.
            const auto r = maya::hit_rect(*id);
            const int cw = r ? std::clamp((r->w - tb303::kDrumCellX0) / 16, 1, 3)
                             : tb303::kDrumCellW;
            // The label ("BD: ") selects the whole voice via step = -1.
            int step = rel < tb303::kDrumCellX0 ? -1 : (rel - tb303::kDrumCellX0) / cw;
            if (step >= 16) return {};
            return {HitZone::DrumCell, step, idx};
        }
        default: return {};
    }
}

// ── Hover ───────────────────────────────────────────────────────────────────
// What the thing under the mouse is and how to use it, for the bottom bar.
std::string App::describe_at(int col, int row) const {
    const auto id = maya::hit_test(col - 1, row - 1);
    if (!id) return {};
    const int idx = static_cast<int>(maya::hit_index(*id));
    switch (maya::hit_kind(*id)) {
        case tb303::kHitKnob:
            if (idx >= 0 && idx < K_COUNT)
                return std::string(g_knob_table[idx].label) + " " + format_knob(idx) + "  \xe2\x80\x94  "
                     + g_knob_table[idx].caption + "   \xc2\xb7 drag or scroll to turn, right-click for more";
            break;
        case tb303::kHitFx:
            if (idx >= 0 && idx < FX_COUNT)
                return std::string(kFxLongName[idx]) + " " + format_fx(idx) + "  \xe2\x80\x94  "
                     + g_fx_table[idx].caption + "   \xc2\xb7 scroll to turn, right-click for more";
            break;
        case tb303::kHitSeqRow: {
            const HitZone z = const_cast<App*>(this)->hit_test(col, row);   // reads only
            if (z.kind == HitZone::Step && z.idx >= 0 && z.idx < 16) {
                const auto& s = g_steps[static_cast<size_t>(z.idx)];
                std::string d = "step " + std::to_string(z.idx + 1) + ": ";
                d += s.rest ? std::string("rest") : midi_note_name(s.note);
                if (!s.rest && s.accent) d += ", accent";
                if (!s.rest && s.slide)  d += ", slide";
                if (s.prob < 100) d += ", " + std::to_string(s.prob) + "% chance";
                return d + "   \xc2\xb7 click to select, scroll for pitch, right-click for more";
            }
            break;
        }
        case tb303::kHitDrumRow: {
            static constexpr const char* kNames[] = {
                "kick", "snare", "closed hat", "open hat", "clap", "low tom", "high tom",
                "rimshot", "cowbell", "shaker", "tambourine", "conga", "mid tom",
                "crash", "ride", "bongo"};
            if (idx >= 0 && idx < kDrumVoices)
                return std::string(kNames[idx]) + "   \xc2\xb7 click a cell to add or remove a hit, right-click for patterns";
            break;
        }
        case tb303::kHitTransport:
            return "transport \xe2\x80\x94 tempo, swing and presets   \xc2\xb7 scroll to change preset";
        case tb303::kHitScope:
            return "the filter's response and the waveform you're hearing";
        default: break;
    }
    return {};
}

// Which section a hover over this target should focus, if any.
std::optional<Section> App::hover_section(int col, int row) {
    const auto id = maya::hit_test(col - 1, row - 1);
    if (!id) return std::nullopt;
    switch (maya::hit_kind(*id)) {
        case tb303::kHitKnob:      return Section::Knobs;
        case tb303::kHitFx:        return Section::FX;
        case tb303::kHitSeqRow:    return Section::Sequencer;
        case tb303::kHitDrumRow:   return Section::Drums;
        case tb303::kHitTransport: return Section::Transport;
        case tb303::kHitPanel:
            return static_cast<Section>(maya::hit_index(*id));
        default: return std::nullopt;
    }
}


void App::handle_mouse(const MouseEvent& m) {
    if (handle_menu_mouse(m)) return;
    int col = m.x.raw();
    int row = m.y.raw();

    // Take picker is modal — swallow mouse so clicks don't leak through to
    // the hidden workspace. The user dismisses it with Esc or Enter.
    if (g_takes_open) return;

    // ── Drag continuation ──────────────────────────────────────────────────
    // Once a knob drag starts, we keep tracking motion until the button is
    // released — even if the cursor leaves the original knob column. This is
    // the standard "capture" behaviour users expect from sliders.
    if (g_knob_drag_idx >= 0 && m.kind == MouseEventKind::Move) {
        int   dy    = g_knob_drag_y0 - row;              // up = positive
        float new_v = std::clamp(g_knob_drag_v0 + dy * 0.02f, 0.0f, 1.0f);
        *g_knob_table[g_knob_drag_idx].value = new_v;
        return;
    }
    if (m.kind == MouseEventKind::Release) {
        g_knob_drag_idx = -1;
        return;
    }

    // ── Hover focus ────────────────────────────────────────────────────────
    // Drag the cursor across a panel and the focused section follows — the
    // bottom help bar and key shortcuts then match what's under the pointer.
    // We use broad panel regions (not the widget-precise hit_test zones) so
    // hovering a border, padding cell, or empty gutter still counts; the
    // user's intent on hover is "this panel", not "this exact knob".
    if (m.kind == MouseEventKind::Move) {
        if (g_help_open || g_palette_open) return;
        if (auto s = hover_section(col, row)) g_focus = *s;
        g_hover_text = describe_at(col, row);
        return;
    }

    HitZone z = hit_test(col, row);

    // ── Scroll wheel ───────────────────────────────────────────────────────
    if (m.button == MouseButton::ScrollUp || m.button == MouseButton::ScrollDown) {
        int dir = (m.button == MouseButton::ScrollUp) ? +1 : -1;
        switch (z.kind) {
            case HitZone::Knob:
                g_focus = Section::Knobs;
                g_knob_sel = z.idx;
                if (z.idx == K_WAVE) g_wave = 1 - g_wave;
                else                 adjust_knob(z.idx, dir * 0.05f);
                return;
            case HitZone::FxKnob:
                g_focus = Section::FX;
                g_fx_sel = z.idx;
                adjust_fx(z.idx, dir * 0.05f);
                return;
            case HitZone::Step: {
                g_focus = Section::Sequencer;
                g_step_sel = z.idx;
                auto& s = g_steps[static_cast<size_t>(z.idx)];
                s.note = std::clamp(s.note + dir, 12, 96);
                s.rest = false;
                return;
            }
            case HitZone::DrumCell:
                // Scroll inside the drum grid nudges the drum-bus master send
                // — same shape as the `[`/`]` keybind. Also focuses drums and
                // selects the cell under the cursor so the user immediately
                // sees where they are in the grid.
                g_focus = Section::Drums;
                g_drum_voice_sel = z.idx2;
                if (z.idx >= 0) g_drum_step_sel = z.idx;
                g_drum_master = std::clamp(g_drum_master + dir * 0.05f, 0.0f, 1.5f);
                return;
            case HitZone::TransportArea:
                g_focus = Section::Transport;
                // Scroll-down feels like "next preset" — invert dir so wheel-down
                // advances the list even though our logical "dir" is +1 for up.
                push_undo("preset change");
                load_preset(g_preset_index - dir);
                return;
            case HitZone::ScopeArea:
                // No widget to adjust on the left half of the middle row, but
                // scrolling here is natural for bpm tweaks.
                g_bpm = std::clamp(g_bpm + dir * 1.0f, 40.0f, 220.0f);
                return;
            case HitZone::Title:
                // Scroll on the title bar nudges BPM by 1 — discoverable cherry
                // on top, not essential.
                g_bpm = std::clamp(g_bpm + dir * 1.0f, 40.0f, 220.0f);
                return;
            case HitZone::HelpBar:
                // Horizontal scroll of the context help bar — by item count,
                // so one tick of the wheel advances by one chip. A loose cap
                // (< 60 items total in the widest layout) avoids runaway
                // offsets; the builder also clamps to items.size() - 1.
                g_help_bar_scroll = std::clamp(g_help_bar_scroll - dir, 0, 60);
                return;
            case HitZone::HelpOverlay:
                // Vertical scroll of the help modal body (wheel up = scroll
                // toward top, so content moves down → decrement offset).
                // The overlay currently has ~120 lines of reference content
                // including section headings and blank spacers.
                g_help_ov_scroll = std::clamp(g_help_ov_scroll - dir, 0, 200);
                return;
            default: return;
        }
    }

    // Help overlay absorbs clicks too — no individual hit-targets inside, but
    // we don't want a stray click to leak through to the hidden workspace.
    if (g_help_open) return;

    // ── Left click ─────────────────────────────────────────────────────────
    if (m.kind == MouseEventKind::Press && m.button == MouseButton::Left) {
        switch (z.kind) {
            case HitZone::Title:
                g_playing ? stop_playback() : start_playback();
                return;
            case HitZone::Knob:
                g_focus = Section::Knobs;
                g_knob_sel = z.idx;
                if (z.idx == K_WAVE) {
                    g_wave = 1 - g_wave;
                } else {
                    // Begin vertical drag. While the button is held, every
                    // Move event updates the value relative to this anchor.
                    g_knob_drag_idx = z.idx;
                    g_knob_drag_y0  = row;
                    g_knob_drag_v0  = *g_knob_table[z.idx].value;
                }
                return;
            case HitZone::FxKnob:
                g_focus = Section::FX;
                g_fx_sel = z.idx;
                // Discrete DLY TIME just steps on each click; continuous FX
                // knobs use scroll/arrow keys (no drag for Phase 1.1).
                if (z.idx == FX_DTIME) adjust_fx(FX_DTIME, +0.05f);
                return;
            case HitZone::Step:
                g_focus = Section::Sequencer;
                g_step_sel = z.idx;
                return;
            case HitZone::DrumCell: {
                g_focus = Section::Drums;
                g_drum_voice_sel = z.idx2;
                // step == -1 signals a label-area click (the "BD:" column):
                // select the voice row but don't toggle a step. Gives the
                // expected "click a label to pick that drum" behaviour.
                if (z.idx >= 0) {
                    g_drum_step_sel = z.idx;
                    auto& cell = g_drums[static_cast<size_t>(z.idx2)]
                                        [static_cast<size_t>(z.idx)];
                    cell = !cell;
                }
                return;
            }
            case HitZone::TransportArea:
                g_focus = Section::Transport;
                return;
            case HitZone::ScopeArea:
                // Clicking the filter / scope area focuses the sequencer —
                // there's nothing interactive here, but giving the empty
                // space a focus target prevents dead clicks.
                g_focus = Section::Sequencer;
                return;
            default: return;
        }
    }

    // ── Right click: a menu of what you can do with the thing under the mouse.
    if (m.kind == MouseEventKind::Press && m.button == MouseButton::Right) {
        open_menu_at(col, row);
        return;
    }
}

// MIDI learn on a knob (0..K_COUNT-1) or FX (kMidiFxBase + i). Arming the
// same one again cancels.
void App::arm_midi_learn(int param) {
    auto& cc = acid::midi::cc();
    if (cc.learn_param.load() == param) {
        cc.learn_param.store(-1);
        set_toast("MIDI learn cancelled");
        return;
    }
    cc.last_learned_cc.store(-1);
    cc.learn_param.store(param);
    g_midi_learn_wait = param;
    set_toast("\xe2\x99\xaa MIDI learn: move a controller\xe2\x80\xa6  (l again cancels)");
}


void App::handle_event(const Event& ev) {
    // Resize — keep terminal size in sync for mouse hit-tests.
    int rw = 0, rh = 0;
    if (resized(ev, &rw, &rh)) { g_term_w = rw; g_term_h = rh; return; }
    if (const MouseEvent* m = as_mouse(ev)) { handle_mouse(*m); return; }

    // ── Take picker modal ────────────────────────────────────────────────────
    // Handled before any other keys so Up/Down/Enter/Esc are scoped to the
    // modal while it is open — the workspace underneath sees nothing.
    if (g_takes_open) {
        if (key(ev, SpecialKey::Escape) || key(ev, 'q') || key(ev, 'Q')) {
            g_takes_open = false;
            return;
        }
        if (key(ev, SpecialKey::Up)) {
            if (!g_takes_entries.empty())
                g_takes_sel = (g_takes_sel - 1 + static_cast<int>(g_takes_entries.size()))
                            % static_cast<int>(g_takes_entries.size());
            return;
        }
        if (key(ev, SpecialKey::Down)) {
            if (!g_takes_entries.empty())
                g_takes_sel = (g_takes_sel + 1) % static_cast<int>(g_takes_entries.size());
            return;
        }
        if (key(ev, SpecialKey::Enter)) {
            if (!g_takes_entries.empty()) {
                push_undo("take load");
                auto& p = g_takes_entries[static_cast<size_t>(g_takes_sel)];
                std::string stem = p.stem().string();           // "live_<ts>"
                std::string lbl  = "take " + stem;
                load_slot_from_path(p, lbl, /*quiet=*/false);
            }
            g_takes_open = false;
            return;
        }
        return;                                                  // swallow all other keys
    }

    // ── Global keys ──────────────────────────────────────────────────────────────────────────
    // Overlays own the keyboard while open.
    if (handle_menu_key(ev)) return;
    if (handle_palette_key(ev)) return;
    if (handle_help_key(ev)) return;
    if (as_key(ev)) g_hover_text.clear();

    if (key(ev, ':') || ctrl(ev, 'k')) { open_palette(); return; }
    if (key(ev, '?')) {
        g_help_open = true;
        g_help_from = g_focus;
        g_help_query.clear();
        g_help_ov_scroll = 0;
        g_first_run = false;
        return;
    }
    // The welcome card: Enter or Space starts playing, anything else just
    // dismisses it and does its usual job.
    if (g_first_run) {
        g_first_run = false;
        if (key(ev, SpecialKey::Enter)) { start_playback(); return; }
    }

    if (key(ev, 'q') || key(ev, 'Q') || key(ev, SpecialKey::Escape)) {
        // Escape also exits jam mode without quitting — otherwise the user
        // can't get out except by typing `k` (which would trigger a note).
        if (g_jam_mode) {
            toggle_jam();
            return;
        }
        // Esc alone never quits: it's the "get me out of here" key in every
        // other app, so it shouldn't end the session. q quits.
        if (key(ev, SpecialKey::Escape)) {
            set_toast("q to quit  \xc2\xb7  : for commands  \xc2\xb7  ? for help");
            return;
        }
        g_quit_requested = true;
        return;
    }
    if (key(ev, 'Y')) { open_take_picker(); return; }

    // ── Jam mode ────────────────────────────────────────────────────────────
    // When active, swallow almost every keystroke and route letters through
    // jam_key_semi. Tab / `k` / Esc still do their usual things so the user
    // always has a way out. Space still plays/pauses the pattern so jam and
    // sequencer can layer (sequencer rides through jam; pattern runs in the
    // background and jam adds a lead on top).
    if (g_jam_mode) {
        if (key(ev, 'k')) { toggle_jam(); return; }
        if (key(ev, SpecialKey::Tab) || key(ev, SpecialKey::BackTab)) {
            cycle_focus(key(ev, SpecialKey::BackTab) ? -1 : +1);
            return;
        }
        if (key(ev, ' ')) { g_playing ? stop_playback() : start_playback(); return; }
        if (key(ev, SpecialKey::Left))  { g_jam_octave = std::max(0, g_jam_octave - 1); return; }
        if (key(ev, SpecialKey::Right)) { g_jam_octave = std::min(7, g_jam_octave + 1); return; }
        if (key(ev, '\'')) { g_jam_accent = !g_jam_accent; return; }
        if (key(ev, ';'))  { g_jam_slide  = !g_jam_slide;  return; }
        // Piano keys — build the MIDI note and fire.
        const KeyEvent* ke = as_key(ev);
        if (ke) {
            auto* ck = std::get_if<CharKey>(&ke->key);
            if (ck && ke->mods.none()) {
                int semi = jam_key_semi(static_cast<char>(ck->codepoint));
                if (semi >= 0) {
                    int midi = std::clamp(12 * g_jam_octave + semi, 12, 108);
                    float freq = 440.0f * std::pow(2.0f,
                                    static_cast<float>(midi - 69) / 12.0f);
                    float step_sec = 60.0f / std::max(20.0f, g_bpm) / 4.0f;
                    acid_note_on(freq,
                                 g_jam_accent ? 1 : 0,
                                 g_jam_slide  ? 1 : 0,
                                 step_sec);
                    g_jam_gate_s   = 0.18f;       // ~180 ms sustain before auto-off
                    g_jam_last_midi = midi;
                    return;
                }
            }
        }
        // Anything else is swallowed so the user can't accidentally randomize
        // the pattern by stabbing at the keyboard mid-jam.
        return;
    }

    if (key(ev, ' ')) { g_playing ? stop_playback() : start_playback(); return; }
    if (key(ev, SpecialKey::Tab))     { cycle_focus(+1); return; }
    if (key(ev, SpecialKey::BackTab)) { cycle_focus(-1); return; }

    // ── Global pattern / I/O actions ────────────────────────────────────────
    // Randomization: lowercase `r` dice-rolls the currently-focused section
    // only ("randomize what I'm looking at"); uppercase `R` dice-rolls
    // everything (knobs + pattern) for a full fresh patch + groove combo.
    // The sequencer's previous per-step rest toggle moved to `m` (mute).
    if (key(ev, 'r')) { randomize_focused(); return; }
    if (key(ev, 'R')) { push_undo("randomize everything"); randomize_everything();
                        toast_undoable("\xe2\x9a\x84 everything randomized"); return; }
    if (key(ev, 'M')) { push_undo("evolve"); mutate_pattern(); return; }
    // Shift+A — one-press auto-arrangement. Overwrites slots 1..9 (with .bak
    // rotation) and boots into song mode so a ~10-minute coherent track
    // plays end-to-end. The intermediate slots are real save files so the
    // user can tweak any section after the fact and keep listening.
    if (key(ev, 'A')) { auto_arrange(); return; }
    if (key(ev, 'u')) { do_undo(); return; }
    if (key(ev, 'U')) { do_redo(); return; }
    if (key(ev, 'T')) { next_theme(); return; }
    if (key(ev, 'e')) { export_wav(); return; }
    if (key(ev, 'E')) { export_mid(); return; }
    if (key(ev, 'W')) { toggle_live_record(); return; }
    if (key(ev, 'p')) { export_pattern_text(); return; }
    if (key(ev, 'P')) { push_undo("import pattern"); import_pattern_text(); return; }
    // Jam mode toggle — take over the keyboard for live two-octave playing.
    // Pattern playback keeps running under jam, so the user can lay a lead
    // line over an already-running sequence.
    if (key(ev, 'k')) { toggle_jam(); return; }
    // MIDI output toggle — emit notes + drum hits + clock to the ALSA seq
    // port. On the first enable, tell the user how to hook it up; aconnect -lo
    // shows the port and a DAW/softsynth can subscribe.
    if (key(ev, 'O')) { toggle_midi_out(); return; }
    // MIDI clock input sync — follow an external master's tempo + transport.
    // Incoming notes still trigger the bass voice regardless; this switch
    // controls only the tempo-slaving path.
    if (key(ev, 'I')) { toggle_midi_sync(); return; }
    // MIDI learn: `l` arms the focused knob or FX; the next CC binds to it.
    // `l` again while armed cancels.
    if (key(ev, 'l') && (g_focus == Section::Knobs || g_focus == Section::FX)) {
        arm_midi_learn(g_focus == Section::Knobs ? g_knob_sel : kMidiFxBase + g_fx_sel);
        return;
    }
    // Song mode chain: step through saved slots 1..9 on each pattern wrap.
    // Toggling ON boots straight into the first saved slot so the user hears
    // immediate feedback; toggling OFF leaves the current pattern loaded.
    if (key(ev, 'n')) { toggle_song_mode(); return; }
    // Save slots: Shift+digit saves, digit alone loads. Slot 0 is a bottom-of-
    // the-stack auto-slot (no binding); users address slots 1..9.
    {
        const KeyEvent* k = as_key(ev);
        if (k) {
            auto* ck = std::get_if<CharKey>(&k->key);
            if (ck) {
                char c = static_cast<char>(ck->codepoint);
                // Shift+1..9 = save; characters '!'…'(' arrive as the US
                // keyboard's shifted digit pair, so accept either form.
                static constexpr const char* kShiftedDigits = ")!@#$%^&*(";
                for (int i = 1; i <= 9; ++i) {
                    if (c == kShiftedDigits[i]) { save_pattern_slot(i); return; }
                }
                // Only handle plain digits when the sequencer isn't focused
                // (those keys don't mean anything to sequencer editing anyway,
                // but keeping the guard makes the intent explicit).
                if (c >= '1' && c <= '9' && k->mods.none()
                    && g_focus != Section::Drums) {
                    int slot = c - '0';
                    // Only a real load becomes an undo step; an empty slot just
                    // says how to fill it.
                    std::error_code ec;
                    if (std::filesystem::exists(pattern_path(slot), ec)) push_undo("slot load");
                    if (!load_pattern_slot(slot)) return;
                    // Keep song mode in sync: manual jumps should advance
                    // from the slot the user just picked, not from wherever
                    // the chain was previously, and reset the bar counter
                    // so the new slot gets its full hold duration.
                    if (g_song_mode) {
                        g_song_slot = slot;
                        int bars = g_song_bars[static_cast<size_t>(slot)];
                        if (bars <= 0) bars = kDefaultSongBars;
                        g_song_bars_left = bars;
                    }
                    return;
                }
            }
        }
    }

    // ── Section-specific keys ────────────────────────────────────────────────
    switch (g_focus) {
        case Section::Knobs: {
            if (key(ev, SpecialKey::Left))  { g_knob_sel = (g_knob_sel - 1 + K_COUNT) % K_COUNT; return; }
            if (key(ev, SpecialKey::Right)) { g_knob_sel = (g_knob_sel + 1) % K_COUNT; return; }
            if (key(ev, SpecialKey::Up))    { adjust_knob(g_knob_sel, +0.05f); return; }
            if (key(ev, SpecialKey::Down))  { adjust_knob(g_knob_sel, -0.05f); return; }
            if (key(ev, ']'))               { adjust_knob(g_knob_sel, +0.10f); return; }
            if (key(ev, '['))               { adjust_knob(g_knob_sel, -0.10f); return; }
            if (key(ev, '0'))               { reset_knob(g_knob_sel);
                                              set_toast(std::string(g_knob_table[g_knob_sel].label) + " reset to " + format_knob(g_knob_sel)); return; }
            if (key(ev, 'w'))               { g_wave = 1 - g_wave; return; }
            // `.` toggles write-mode automation on the currently-focused
            // knob. Only cutoff / res / env / accent have p-lock slots in
            // the engine; other knobs emit a warning toast instead.
            if (key(ev, '.')) {
                int ai = -1;
                const char* lbl = nullptr;
                switch (g_knob_sel) {
                    case K_CUTOFF: ai = 0; lbl = "CUTOFF"; break;
                    case K_RES:    ai = 1; lbl = "RES";    break;
                    case K_ENV:    ai = 2; lbl = "ENVMOD"; break;
                    case K_ACCENT: ai = 3; lbl = "ACCENT"; break;
                    default: break;
                }
                if (ai < 0) {
                    set_toast("\xe2\x9c\x97 automation: cutoff/res/env/accent only");
                    return;
                }
                g_auto_arm[static_cast<size_t>(ai)] =
                    !g_auto_arm[static_cast<size_t>(ai)];
                char buf[48];
                std::snprintf(buf, sizeof(buf), "%s %s auto",
                    lbl,
                    g_auto_arm[static_cast<size_t>(ai)]
                        ? "\xe2\x97\x8f rec" : "\xe2\x97\x8b off");
                set_toast(buf);
                return;
            }
            if (key(ev, 'm')) { toggle_synth_mute(); return; }
            break;
        }
        case Section::FX: {
            if (key(ev, SpecialKey::Left))  { g_fx_sel = (g_fx_sel - 1 + FX_COUNT) % FX_COUNT; return; }
            if (key(ev, SpecialKey::Right)) { g_fx_sel = (g_fx_sel + 1) % FX_COUNT; return; }
            if (key(ev, SpecialKey::Up))    { adjust_fx(g_fx_sel, +0.05f); return; }
            if (key(ev, SpecialKey::Down))  { adjust_fx(g_fx_sel, -0.05f); return; }
            if (key(ev, ']'))               { adjust_fx(g_fx_sel, +0.10f); return; }
            if (key(ev, '['))               { adjust_fx(g_fx_sel, -0.10f); return; }
            if (key(ev, '0'))               { reset_fx(g_fx_sel);
                                              set_toast(std::string(kFxLongName[g_fx_sel]) + " reset to " + format_fx(g_fx_sel)); return; }
            if (key(ev, 'm')) { toggle_synth_mute(); return; }
            break;
        }
        case Section::Sequencer: {
            auto& s = g_steps[static_cast<size_t>(g_step_sel)];
            if (key(ev, SpecialKey::Left))  { g_step_sel = (g_step_sel - 1 + 16) % 16; return; }
            if (key(ev, SpecialKey::Right)) { g_step_sel = (g_step_sel + 1) % 16; return; }
            if (key(ev, SpecialKey::Up))    { s.note = std::min(s.note + 1, 96); s.rest = false; return; }
            if (key(ev, SpecialKey::Down))  { s.note = std::max(s.note - 1, 12); s.rest = false; return; }
            if (key(ev, '<') || key(ev, ',')) { s.note = std::max(s.note - 12, 12); return; }
            if (key(ev, '>') || key(ev, '.')) { s.note = std::min(s.note + 12, 96); return; }
            if (key(ev, 'a')) { s.accent = !s.accent; s.rest = false;
                                set_toast(std::string("step ") + std::to_string(g_step_sel + 1) + (s.accent ? " accented" : " accent off")); return; }
            if (key(ev, 's')) { s.slide  = !s.slide;  s.rest = false;
                                set_toast(std::string("step ") + std::to_string(g_step_sel + 1) + (s.slide ? " slides into the next" : " slide off")); return; }
            // `m` mutates the sequence in place — one small weighted change
            // per press so the new line sits musically next to the old one
            // (accent flip / pitch nudge / rotate / etc). push_undo() before
            // the mutation so `u` restores the previous state; hammering `m`
            // "walks" the pattern and `u` rewinds step-by-step up to 32
            // presses (kUndoMax). Rest-toggle on a step is still available
            // via `x` (which wipes the step to an explicit rest).
            if (key(ev, 'm')) { push_undo("evolve"); mutate_pattern(); return; }
            // Probability cycles 100→75→50→25→100; ratchet cycles 1→2→3→4→1.
            // Keeps the "always/half/quarter" musical feel without an extra
            // continuous-value knob row.
            if (key(ev, 'v')) {
                static constexpr int cyc[] = {100, 75, 50, 25};
                int cur = s.prob;
                int idx = 0;
                for (int k = 0; k < 4; ++k) if (cyc[k] == cur) idx = k;
                s.prob = cyc[(idx + 1) % 4];
                return;
            }
            if (key(ev, 'j')) {
                s.ratchet = (s.ratchet % 4) + 1;
                return;
            }
            // Per-step parameter locks ("p-locks", Elektron-style). Capitals
            // so they don't collide with the existing lowercase shortcuts.
            // First press snapshots the current global knob as the lock;
            // second press clears the lock. To edit the locked value, adjust
            // the global knob and toggle twice (off, on) — cheap UX that
            // avoids inventing a modal editing mode.
            auto toggle_lock = [&](uint8_t bit, float& store, float snap) {
                if (s.lock_mask & bit) {
                    s.lock_mask &= ~bit;
                } else {
                    s.lock_mask |= bit;
                    store = snap;
                }
                s.rest = false;
            };
            if (key(ev, 'F')) { toggle_lock(tb303::LOCK_CUTOFF, s.lock_cutoff, g_cutoff);    return; }
            if (key(ev, 'G')) { toggle_lock(tb303::LOCK_RES,    s.lock_res,    g_resonance); return; }
            if (key(ev, 'H')) { toggle_lock(tb303::LOCK_ENV,    s.lock_env,    g_env_mod);   return; }
            if (key(ev, 'J')) { toggle_lock(tb303::LOCK_ACCENT, s.lock_accent, g_accent);    return; }
            // Clear every lock on the current step. `x` already resets the
            // whole step; `L` is the lighter-touch option that preserves the
            // note + flags but wipes p-locks.
            if (key(ev, 'L')) {
                push_undo("clear locks");
                s.lock_mask = 0;
                return;
            }
            // letter notes — keep octave of current step
            {
                const KeyEvent* k = as_key(ev);
                if (k) {
                    auto* ck = std::get_if<CharKey>(&k->key);
                    if (ck && k->mods.none()) {
                        int semi = letter_to_semitone(static_cast<char>(ck->codepoint));
                        if (semi >= 0) {
                            int octave = s.note / 12;
                            s.note = octave * 12 + semi;
                            s.rest = false;
                            return;
                        }
                    }
                }
            }
            // `.` also means "clear" when the step is already at an octave boundary
            // (conflict with octave-up) — use `x` as the unambiguous clear key.
            if (key(ev, 'x')) { push_undo("clear step"); s = StepData{}; s.rest = true; toast_undoable("step cleared"); return; }
            break;
        }
        case Section::Drums: {
            // Drum grid: ←/→ step cursor, ↑/↓ voice cursor, space/x toggle,
            // [ / ] adjust drum bus send, `c` clears the current row.
            if (key(ev, SpecialKey::Left))  { g_drum_step_sel  = (g_drum_step_sel  - 1 + 16) % 16; return; }
            if (key(ev, SpecialKey::Right)) { g_drum_step_sel  = (g_drum_step_sel  + 1) % 16; return; }
            if (key(ev, SpecialKey::Up))    { g_drum_voice_sel = (g_drum_voice_sel - 1 + kDrumVoices) % kDrumVoices; return; }
            if (key(ev, SpecialKey::Down))  { g_drum_voice_sel = (g_drum_voice_sel + 1) % kDrumVoices; return; }
            if (key(ev, ' ') || key(ev, 'x')) {
                auto& cell = g_drums[static_cast<size_t>(g_drum_voice_sel)]
                                   [static_cast<size_t>(g_drum_step_sel)];
                cell = !cell;
                return;
            }
            if (key(ev, '[')) { g_drum_master = std::max(0.0f, g_drum_master - 0.05f); return; }
            if (key(ev, ']')) { g_drum_master = std::min(1.5f, g_drum_master + 0.05f); return; }
            if (key(ev, 'm')) {
                g_drums_mute = !g_drums_mute;
                set_toast(g_drums_mute ? "drums muted" : "drums live");
                return;
            }
            if (key(ev, 's')) {
                bool& mute = g_drum_voice_mute[static_cast<size_t>(g_drum_voice_sel)];
                mute = !mute;
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%s %s",
                    kDrumLabels[g_drum_voice_sel], mute ? "muted" : "live");
                set_toast(buf);
                return;
            }
            // `c` clears the focused row; `C` clears the entire kit. Mirrors
            // the sequencer's `x = clear step` ergonomics at the row level.
            if (key(ev, 'c')) {
                push_undo("clear drum row");
                for (auto& c : g_drums[static_cast<size_t>(g_drum_voice_sel)]) c = false;
                return;
            }
            if (key(ev, 'C')) {
                push_undo("clear drums");
                for (auto& row : g_drums) for (auto& c : row) c = false;
                toast_undoable("drums cleared");
                return;
            }
            // Number-row 1..0 = quick-toggle steps 1..10 on the current voice.
            // Makes it fast to punch in a four-on-the-floor BD without arrow-
            // keying across the grid.
            {
                const KeyEvent* k = as_key(ev);
                if (k) {
                    auto* ck = std::get_if<CharKey>(&k->key);
                    if (ck && k->mods.none()) {
                        char cc = static_cast<char>(ck->codepoint);
                        int idx = -1;
                        if (cc >= '1' && cc <= '9') idx = cc - '1';
                        else if (cc == '0')         idx = 9;
                        if (idx >= 0) {
                            auto& cell = g_drums[static_cast<size_t>(g_drum_voice_sel)]
                                               [static_cast<size_t>(idx)];
                            cell = !cell;
                            g_drum_step_sel = idx;
                            return;
                        }
                    }
                }
            }
            break;
        }
        case Section::Transport: {
            if (key(ev, SpecialKey::Up))   { push_undo("preset change"); load_preset(g_preset_index - 1); return; }
            if (key(ev, SpecialKey::Down)) { push_undo("preset change"); load_preset(g_preset_index + 1); return; }
            if (key(ev, '[')) { nudge_bpm(-2.0f); return; }
            if (key(ev, ']')) { nudge_bpm(+2.0f); return; }
            if (key(ev, '{')) { g_pattern_length = std::max(4, g_pattern_length - 1); return; }
            if (key(ev, '}')) { g_pattern_length = std::min(16, g_pattern_length + 1); return; }
            // Swing: 0.50 straight → 0.75 hard shuffle. Step is 2% so three
            // taps from centre lands on the canonical MPC-56/58/60/62 feels.
            if (key(ev, '-')) { nudge_swing(-0.02f); return; }
            if (key(ev, '=')) { nudge_swing(+0.02f); return; }
            if (key(ev, ',')) { push_undo("preset change"); load_preset(g_preset_index - 1); return; }
            if (key(ev, '.')) { push_undo("preset change"); load_preset(g_preset_index + 1); return; }
            // ( / ) — shrink/grow how long the current song-mode slot
            // holds before the chain advances. Cycle: 1 → 2 → 4 → 8 → 16
            // → 32 → 64 (bars). No-op when song mode is off or no slot
            // has been picked yet (the toast nudges the user there).
            if (key(ev, '(') || key(ev, ')')) {
                if (g_song_slot <= 0) {
                    set_toast("bars: turn on song mode (n) first");
                    return;
                }
                static constexpr int kBarCycle[] = {1, 2, 4, 8, 16, 32, 64};
                constexpr int kBarCycleN =
                    static_cast<int>(sizeof(kBarCycle) / sizeof(kBarCycle[0]));
                int cur = g_song_bars[static_cast<size_t>(g_song_slot)];
                int idx = 0;
                for (int i = 0; i < kBarCycleN; ++i)
                    if (kBarCycle[i] == cur) { idx = i; break; }
                idx = std::clamp(
                    idx + (key(ev, ')') ? +1 : -1), 0, kBarCycleN - 1);
                int bars = kBarCycle[idx];
                g_song_bars[static_cast<size_t>(g_song_slot)] = bars;
                // Reset the live counter so the change takes effect on the
                // next bar boundary instead of stretching the current hold.
                g_song_bars_left = std::min(g_song_bars_left, bars);
                if (g_song_bars_left <= 0) g_song_bars_left = bars;
                char buf[48];
                std::snprintf(buf, sizeof(buf),
                    "slot %d  \xc2\xb7 %d bars", g_song_slot, bars);
                set_toast(buf);
                return;
            }
            if (key(ev, 'm')) { toggle_all_mute(); return; }
            break;
        }
    }
}

// ── Actions shared by keys and the command palette ──────────────────────────
void App::next_theme() {
    tb303::cycle_theme();
    g_theme_chosen = true;
    set_toast("\xe2\x97\x86 theme: " + tb303::g_theme_name
              + "  \xc2\xb7  T for the next one");
}

void App::toggle_midi_out() {
    g_midi_out = !g_midi_out;
    acid_midi_set_out_enabled(g_midi_out ? 1 : 0);
    set_toast(g_midi_out
        ? "\xe2\x99\xaa MIDI out on  (connect 'acidflow:out' via aconnect)"
        : "MIDI out off");
}

void App::toggle_midi_sync() {
    g_midi_sync = !g_midi_sync;
    acid_midi_set_sync_in(g_midi_sync ? 1 : 0);
    set_toast(g_midi_sync
        ? "\xe2\x99\xaa MIDI sync on  (waiting for clock/start)"
        : "MIDI sync off");
}

void App::toggle_song_mode() {
    if (g_song_mode) {
        g_song_mode = false;
        set_toast("song mode off");
    } else {
        int first = song_next_slot(0);
        if (first == 0) {
            set_toast("no saved slots for song mode  \xc2\xb7  Shift+1\xe2\x80\xa6" "9 saves, A writes a song");
        } else {
            g_song_mode = true;
            g_song_slot = first;
            int bars = g_song_bars[static_cast<size_t>(first)];
            if (bars <= 0) bars = kDefaultSongBars;
            g_song_bars_left = bars;
            load_pattern_slot(first, /*quiet=*/true);
            char buf[48];
            std::snprintf(buf, sizeof(buf),
                          "\xe2\x99\xaa song \xe2\x86\x92 slot %d  \xc2\xb7 %d bars",
                          first, bars);
            set_toast(buf);
        }
    }
}

