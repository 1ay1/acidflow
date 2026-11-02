// App state: undo/redo, presets, construction.
#include "app.hpp"


void App::set_toast(std::string s) {
    g_toast   = std::move(s);
    g_toast_t = kToastDur;
}


Snapshot App::capture_snapshot() {
    Snapshot s;
    s.steps            = g_steps;
    s.pattern_length   = g_pattern_length;
    s.preset_index     = g_preset_index;
    s.drums            = g_drums;
    s.drum_voice_mute  = g_drum_voice_mute;
    s.drum_master      = g_drum_master;
    s.tune = g_tune;     s.cutoff = g_cutoff; s.resonance = g_resonance;
    s.env_mod = g_env_mod; s.decay = g_decay; s.accent = g_accent;
    s.drive = g_drive;   s.volume = g_volume; s.wave = g_wave;
    s.bpm = g_bpm;       s.swing = g_swing;
    s.od_amt = g_od_amt; s.delay_mix = g_delay_mix; s.delay_fb = g_delay_fb;
    s.rev_mix = g_rev_mix; s.rev_size = g_rev_size; s.rev_damp = g_rev_damp;
    s.delay_div = g_delay_div;
    return s;
}


void App::apply_snapshot(const Snapshot& s) {
    g_steps           = s.steps;
    g_pattern_length  = s.pattern_length;
    g_preset_index    = s.preset_index;
    g_drums           = s.drums;
    g_drum_voice_mute = s.drum_voice_mute;
    g_drum_master     = s.drum_master;
    g_tune = s.tune;     g_cutoff = s.cutoff; g_resonance = s.resonance;
    g_env_mod = s.env_mod; g_decay = s.decay; g_accent = s.accent;
    g_drive = s.drive;   g_volume = s.volume; g_wave = s.wave;
    g_bpm = s.bpm;       g_swing = s.swing;
    g_od_amt = s.od_amt; g_delay_mix = s.delay_mix; g_delay_fb = s.delay_fb;
    g_rev_mix = s.rev_mix; g_rev_size = s.rev_size; g_rev_damp = s.rev_damp;
    g_delay_div = s.delay_div;
}

// Push the current state onto the undo stack before running a destructive
// action. Clears the redo stack. The label is what the undo toast names.
void App::push_undo(const char* what) {
    Snapshot s = capture_snapshot();
    s.label = what ? what : "change";
    g_undo_past.push_back(std::move(s));
    if (g_undo_past.size() > kUndoMax) g_undo_past.pop_front();
    g_undo_future.clear();
    g_last_undo_label = g_undo_past.back().label;
}

// Toast for a change that can be undone: say what happened, then how to
// take it back. Teaches `u` the first few times without nagging forever.
void App::toast_undoable(const std::string& what) {
    if (g_undo_hints_left > 0) {
        --g_undo_hints_left;
        set_toast(what + "  \xc2\xb7  u to undo");
    } else {
        set_toast(what);
    }
}


void App::do_undo() {
    if (g_undo_past.empty()) { set_toast("nothing to undo"); return; }
    Snapshot now = capture_snapshot();
    now.label = g_undo_past.back().label;
    g_undo_future.push_back(std::move(now));
    if (g_undo_future.size() > kUndoMax) g_undo_future.pop_front();
    const std::string what = g_undo_past.back().label;
    apply_snapshot(g_undo_past.back());
    g_undo_past.pop_back();
    set_toast("\xe2\x86\xb6 undid " + what + "  \xc2\xb7  U to redo");
}


void App::do_redo() {
    if (g_undo_future.empty()) { set_toast("nothing to redo"); return; }
    Snapshot now = capture_snapshot();
    now.label = g_undo_future.back().label;
    g_undo_past.push_back(std::move(now));
    if (g_undo_past.size() > kUndoMax) g_undo_past.pop_front();
    const std::string what = g_undo_future.back().label;
    apply_snapshot(g_undo_future.back());
    g_undo_future.pop_back();
    set_toast("\xe2\x86\xb7 redid " + what);
}


std::string App::format_fx(int idx) const {
    char buf[32];
    switch (idx) {
        case FX_OD:
        case FX_DMIX:
        case FX_DFB:
        case FX_RMIX:
        case FX_RSIZE:
        case FX_RDAMP:
            std::snprintf(buf, sizeof(buf), "%d%%",
                static_cast<int>(std::round(*g_fx_table[idx].value * 100.0f)));
            break;
        case FX_DTIME:
            std::snprintf(buf, sizeof(buf), "%s", kDlyDivNames[g_delay_div]);
            break;
        default:
            buf[0] = 0;
    }
    return buf;
}

// format a knob's current value for display under the rail
std::string App::format_knob(int idx) const {
    char buf[32];
    switch (idx) {
        case K_TUNE: {
            float semis = (g_tune - 0.5f) * 24.0f;  // ±12 semis
            std::snprintf(buf, sizeof(buf), "%+.1f st", semis);
            break;
        }
        case K_CUTOFF: {
            // Matches engine's 13 Hz → 5 kHz log curve (measured TB-303 range).
            float fc = 13.0f * std::pow(5000.0f / 13.0f,
                                        std::clamp(g_cutoff, 0.0f, 1.0f));
            if (fc >= 1000.0f)
                std::snprintf(buf, sizeof(buf), "%.2f kHz", fc / 1000.0f);
            else
                std::snprintf(buf, sizeof(buf), "%.0f Hz", fc);
            break;
        }
        case K_ENV: {
            // Show in octaves of filter sweep — matches engine's env_oct
            // scale (envmod * 4). Much more meaningful than "65%" for a
            // musician thinking in octaves.
            float oct = std::clamp(g_env_mod, 0.0f, 1.0f) * 4.0f;
            std::snprintf(buf, sizeof(buf), "%.1f oct", oct);
            break;
        }
        case K_DECAY: {
            // Show actual filter-env length — engine maps via 0.2 * 10^k.
            // Flip to ms under one second; keeps the readout compact.
            float sec = 0.2f * std::pow(10.0f, std::clamp(g_decay, 0.0f, 1.0f));
            if (sec < 1.0f) std::snprintf(buf, sizeof(buf), "%.0f ms", sec * 1000.0f);
            else            std::snprintf(buf, sizeof(buf), "%.2f s",  sec);
            break;
        }
        case K_RES:
        case K_ACCENT:
        case K_DRIVE:
        case K_VOL:
            std::snprintf(buf, sizeof(buf), "%d%%",
                static_cast<int>(std::round(*g_knob_table[idx].value * 100.0f)));
            break;
        case K_WAVE:
            std::snprintf(buf, sizeof(buf), "%s", g_wave == 0 ? "saw" : "square");
            break;
        default:
            buf[0] = 0;
    }
    return buf;
}


void App::load_preset(int idx) {
    idx = ((idx % static_cast<int>(g_presets.size())) + static_cast<int>(g_presets.size()))
        % static_cast<int>(g_presets.size());
    g_preset_index = idx;
    g_steps        = g_presets[static_cast<size_t>(idx)].steps;
    g_step_sel     = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Entry point
// ─────────────────────────────────────────────────────────────────────────────
// True when nothing on screen is moving, so the tick can slow right down.
bool App::ui_idle() const {
    return !g_playing && g_toast_t <= 0.0f && g_jam_gate_s <= 0.0f
        && !acid_is_recording() && g_knob_drag_idx < 0
        && acid_output_peak() < 1e-4f;
}
