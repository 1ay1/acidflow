// Engine plumbing: pushing knob/pattern state to the engine, the UI tick, MIDI learn.
#include "app.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// Audio plumbing
// ─────────────────────────────────────────────────────────────────────────────
float App::midi_to_hz(int note) {
    return 440.0f * std::pow(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f);
}

// Push current knob + sequencer state into the audio engine every frame.
// Cheap (atomics), and keeps everything in perfect sync with the UI with no
// special plumbing. The sequencer itself lives on the audio thread now —
// this just keeps it fed with up-to-date pattern/transport data.
void App::push_params() {
    acid_set_cutoff(g_cutoff);
    acid_set_resonance(g_resonance);
    acid_set_env_mod(g_env_mod);
    acid_set_decay(g_decay);
    acid_set_accent_amt(g_accent);
    acid_set_drive(g_drive);
    acid_set_volume(g_synth_mute ? 0.0f : g_volume);
    acid_set_tuning_semi((g_tune - 0.5f) * 24.0f);
    acid_set_waveform(g_wave);
    acid_set_delay_mix(g_delay_mix);
    acid_set_delay_feedback(g_delay_fb);
    acid_set_delay_division(g_delay_div);
    acid_set_od_amt(g_od_amt);
    acid_set_rev_mix(g_rev_mix);
    acid_set_rev_size(g_rev_size);
    acid_set_rev_damp(g_rev_damp);

    for (int i = 0; i < 16; ++i) {
        const auto& s = g_steps[static_cast<size_t>(i)];
        int flags = 0;
        if (s.rest)   flags |= 1;
        if (s.accent) flags |= 2;
        if (s.slide)  flags |= 4;
        acid_seq_set_step(i, s.note, flags, s.prob, s.ratchet);
        acid_seq_set_step_locks(i, s.lock_mask,
                                s.lock_cutoff, s.lock_res,
                                s.lock_env,    s.lock_accent);
    }
    acid_seq_set_pattern_length(g_pattern_length);
    if (acid_link_enabled()) {
        // Link owns the tempo. A local BPM edit goes to the session; otherwise
        // follow whatever the session says.
        if (g_bpm != g_link_bpm_sent) { acid_link_set_tempo(g_bpm); g_link_bpm_sent = g_bpm; }
        else { g_bpm = acid_current_bpm(); g_link_bpm_sent = g_bpm; }
    } else {
        acid_seq_set_bpm(g_bpm);
    }
    acid_seq_set_swing(g_swing);

    // Drum lanes: pack each row's 16 bool cells into a uint16 mask, then push
    // once per voice. Done every tick so the user's toggles reach the audio
    // thread with UI-tick latency — plenty fast for step-grid editing.
    for (int v = 0; v < kDrumVoices; ++v) {
        int mask = 0;
        for (int i = 0; i < 16; ++i) {
            if (g_drums[static_cast<size_t>(v)][static_cast<size_t>(i)]) {
                mask |= (1 << i);
            }
        }
        acid_seq_set_drum_lane(v, mask);
        acid_seq_set_drum_gain(v, g_drum_voice_mute[static_cast<size_t>(v)]
                                  ? 0.0f : kDrumDefaultGain[v]);
    }
    acid_seq_set_drum_master(g_drums_mute ? 0.0f : g_drum_master);
}


void App::tick(float dt) {
    poll_midi_cc();
    push_params();
    // Visualiser state is advanced here so view() only reads it.
    g_scope.advance();
    g_spectrum.advance();

    // Toast fade runs whether or not playback is active.
    if (g_toast_t > 0.0f) g_toast_t = std::max(0.0f, g_toast_t - dt);

    // Help-bar marquee clock. Advances whether or not playback is active so
    // the bar always keeps rotating when narrow terminals clip it.
    g_help_bar_auto_t += dt;

    // Jam-mode gate: terminals don't deliver key-up events, so every live
    // keypress arms a short sustain window and we send the note-off when it
    // expires. This is what makes repeated taps actually retrigger the
    // envelope instead of piling up on a held gate.
    if (g_jam_mode && g_jam_gate_s > 0.0f) {
        g_jam_gate_s -= dt;
        if (g_jam_gate_s <= 0.0f) {
            g_jam_gate_s = 0.0f;
            acid_note_off();
            g_jam_last_midi = -1;
        }
    }

    // Pull the sequencer's published position so the UI (highlighted step,
    // beat-sync fades) stays in lockstep with what's actually playing.
    int prev_step = g_current_step;
    g_current_step = g_playing ? acid_seq_current_step() : -1;
    g_step_phase   = g_playing ? acid_seq_step_phase()   : 0.0f;

    // Song mode: on pattern wrap (step index went backwards while playing),
    // decrement the bar counter. When the counter hits zero, advance to the
    // next saved slot and reload its bar count. Detecting the wrap here
    // avoids any audio-thread coupling — the UI tick already sees the same
    // positions.
    if (g_song_mode && g_playing
        && prev_step >= 0 && g_current_step >= 0
        && g_current_step < prev_step)
    {
        --g_song_bars_left;
        if (g_song_bars_left <= 0) {
            int next = song_next_slot(g_song_slot > 0 ? g_song_slot : 0);
            if (next > 0) {
                if (next != g_song_slot) {
                    load_pattern_slot(next, /*quiet=*/true);
                }
                g_song_slot = next;
                int bars = g_song_bars[static_cast<size_t>(next)];
                if (bars <= 0) bars = kDefaultSongBars;
                g_song_bars_left = bars;
                char buf[48];
                std::snprintf(buf, sizeof(buf),
                              "\xe2\x99\xaa song \xe2\x86\x92 slot %d  \xc2\xb7 %d bars",
                              next, bars);
                set_toast(buf);
            }
        }
    }

    // Knob automation: while armed + playing, write the current global-knob
    // value into the current step's p-lock slot (and set the lock bit) each
    // tick. Continuous "write mode" — releasing the knob freezes the sweep
    // into the pattern so the next loop plays it back without the user
    // touching anything. Only runs inside the active pattern length so the
    // trailing rest-padded steps (17..16) stay untouched.
    if (g_playing && g_current_step >= 0 && g_current_step < g_pattern_length) {
        auto& s = g_steps[static_cast<size_t>(g_current_step)];
        if (g_auto_arm[0]) {
            s.lock_cutoff = g_cutoff;
            s.lock_mask  |= tb303::LOCK_CUTOFF;
        }
        if (g_auto_arm[1]) {
            s.lock_res    = g_resonance;
            s.lock_mask  |= tb303::LOCK_RES;
        }
        if (g_auto_arm[2]) {
            s.lock_env    = g_env_mod;
            s.lock_mask  |= tb303::LOCK_ENV;
        }
        if (g_auto_arm[3]) {
            s.lock_accent = g_accent;
            s.lock_mask  |= tb303::LOCK_ACCENT;
        }
    }
}


void App::start_playback() {
    g_playing = true;
    g_current_step = -1;
    acid_seq_play();
}


void App::stop_playback() {
    g_playing = false;
    g_current_step = -1;
    acid_seq_stop();
}

// ── MIDI learn / CC ──────────────────────────────────────────────────────────────
void App::apply_midi_cc(int param, float v01) {
    if (param >= 0 && param < K_COUNT) {
        if (param == K_WAVE) { g_wave = v01 >= 0.5f ? 1 : 0; return; }
        *g_knob_table[param].value = v01;
        return;
    }
    const int fx = param - kMidiFxBase;
    if (fx < 0 || fx >= FX_COUNT) return;
    if (fx == FX_DTIME) { g_delay_div = std::clamp(static_cast<int>(v01 * 4.0f), 0, 3); return; }
    *g_fx_table[fx].value = v01;
}


std::string App::midi_param_name(int param) {
    if (param >= 0 && param < K_COUNT) return g_knob_table[param].label;
    const int fx = param - kMidiFxBase;
    if (fx >= 0 && fx < FX_COUNT) return g_fx_table[fx].label;
    return "?";
}

// Called from tick(): apply new CC values and report a finished learn.
void App::poll_midi_cc() {
    auto& cc = acid::midi::cc();
    for (int p = 0; p < acid::midi::kMaxParams; ++p) {
        const uint32_t s = cc.seq[static_cast<size_t>(p)].load(std::memory_order_acquire);
        if (s == g_midi_cc_seen[static_cast<size_t>(p)]) continue;
        g_midi_cc_seen[static_cast<size_t>(p)] = s;
        apply_midi_cc(p, cc.value[static_cast<size_t>(p)].load() / 127.0f);
    }
    if (g_midi_learn_wait >= 0 && cc.learn_param.load() < 0) {
        const int got = cc.last_learned_cc.load();
        if (got >= 0) {
            set_toast("\xe2\x99\xaa CC " + std::to_string(got) + " \xe2\x86\x92 "
                      + midi_param_name(g_midi_learn_wait));
            save_midi_map();
        }
        g_midi_learn_wait = -1;
    }
}

// One "cc param" pair per line in ~/.config/acidflow/midi_map.txt.
void App::save_midi_map() {
    std::error_code ec;
    std::filesystem::create_directories(config_dir(), ec);
    std::ofstream f(config_dir() / "midi_map.txt");
    auto& cc = acid::midi::cc();
    for (int c = 0; c < acid::midi::kNumCC; ++c) {
        const int p = cc.cc_to_param[static_cast<size_t>(c)].load();
        if (p >= 0) f << c << ' ' << p << '\n';
    }
}


void App::load_midi_map() {
    std::ifstream f(config_dir() / "midi_map.txt");
    auto& cc = acid::midi::cc();
    int c = 0, p = 0;
    while (f >> c >> p)
        if (c >= 0 && c < acid::midi::kNumCC && p >= 0 && p < acid::midi::kMaxParams)
            cc.cc_to_param[static_cast<size_t>(c)].store(static_cast<int8_t>(p));
}
