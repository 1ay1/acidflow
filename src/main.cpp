// TB-303 Acid Bass Simulator — TUI build on top of the maya framework.
//
// Keyboard-only, terminal-native acid synth with:
//   * 8 knobs (tuning, cutoff, resonance, env-mod, decay, accent, volume, wave)
//   * 16-step sequencer with per-step note / accent / slide / rest
//   * classic acid preset patterns
//   * live filter-response curve driven by cutoff + resonance knobs
//
// UX choices (see README / widgets.hpp for context):
//   * Tab-switched sections. Each section has its own arrow-key semantics.
//   * Focus is always visually obvious (bold label + orange border + bright glyph).
//   * The help bar at the bottom changes per-section so the user never has
//     to remember out-of-context keys.
//   * `?` brings up a full keyboard reference.

#include "audio.hpp"
#include "widgets.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <maya/maya.hpp>

using namespace maya;
using namespace maya::dsl;
using tb303::Section;
using tb303::StepData;

// ─────────────────────────────────────────────────────────────────────────────
// Global model
// ─────────────────────────────────────────────────────────────────────────────
// We use file-scope globals (as in maya's own music.cpp example) because the
// simple run(event_fn, render_fn) API expects plain closures. For a synth
// this also matches the reality that audio params really are global state.

// Knobs — normalised 0..1 except k_tuning which is 0..1 mapped to ±12 semis
// and k_wave which is an enum.
//
// Defaults roughly match a "classic acid" 303 knob photo: cutoff a little
// under noon (~120 Hz so the envelope has room to scream up), resonance
// three-quarters-right for Q without full self-osc, env-mod around noon,
// decay toward the short end (~0.4 s sweep — quick squelch, not pads), and
// a moderate accent so two stacked accents still have headroom.
static float g_tune      = 0.50f;   // 0.5 → 0 semitones
static float g_cutoff    = 0.40f;
static float g_resonance = 0.75f;
static float g_env_mod   = 0.65f;
static float g_decay     = 0.30f;
static float g_accent    = 0.65f;
static float g_volume    = 0.65f;
static int   g_wave      = 0;       // 0 = saw, 1 = square

// Sequencer
static std::array<StepData, 16> g_steps{};
static int   g_pattern_length = 16;
static int   g_current_step   = -1;     // -1 while stopped
static int   g_preset_index   = 0;
static double g_step_clock    = 0.0;    // accumulated seconds within current step

// Transport — 122 BPM lands between the canonical acid tempos: Phuture
// "Acid Tracks" at 120, Josh Wink "Higher State" at 124, Adonis "No Way
// Back" at 122. 128+ pushes into acid techno territory (Beltram, Hawtin).
static bool  g_playing = false;
static float g_bpm     = 122.0f;

// UI focus
static Section g_focus      = Section::Sequencer;
static int     g_knob_sel   = 2;   // K_CUTOFF — the signature 303 knob
static int     g_step_sel   = 0;
static bool    g_help_open  = false;

// ─────────────────────────────────────────────────────────────────────────────
// Knob descriptor table — makes per-knob behaviour data-driven
// ─────────────────────────────────────────────────────────────────────────────

// Order here is the on-screen column order AND the "signal-flow" left-to-
// right ordering: OSC (TUNE+WAVE) → VCF (CUTOFF+RES+ENVMOD) → EG (DECAY) →
// VCA (ACCENT+VOL). Reading the strip is reading the block diagram.
enum KnobIdx {
    K_TUNE = 0, K_WAVE, K_CUTOFF, K_RES, K_ENV, K_DECAY, K_ACCENT, K_VOL,
    K_COUNT
};

struct KnobDesc {
    const char* label;
    float*      value;           // nullptr for K_WAVE (special-cased)
    float       default_v;
    const char* tech;            // engineering symbol under the meter
    const char* caption;         // one-line description shown when focused
};

// Captions are written in present-tense "what this knob does" form, mixing a
// short noun phrase with a bracketed sound-engineering cue so a newcomer can
// learn the 303 vocabulary just by scanning between knobs. `tech` is the
// schematic-style symbol that appears under each meter — f₀/fc/Q/τ etc.
static KnobDesc g_knob_table[K_COUNT] = {
    {"TUNE",   &g_tune,       0.50f, "f\xe2\x82\x80",
        "oscillator pitch offset  [\xc2\xb1" "12 semitones]"},
    {"WAVE",   nullptr,       0.0f,  "\xe2\x88\xbf",   // ∿
        "oscillator shape  [saw = classic, square = reedy]"},
    {"CUTOFF", &g_cutoff,     0.40f, "fc",
        "filter frequency  [13 Hz \xe2\x80\x93 5 kHz, log]"},
    {"RES",    &g_resonance,  0.75f, "Q",
        "filter resonance  [squelch \xe2\x86\x92 self-osc at \xe2\x89\x88" "90%]"},
    {"ENVMOD", &g_env_mod,    0.65f, "\xe2\x86\x92""fc",
        "envelope \xe2\x86\x92 cutoff sweep range  [0\xe2\x80\x93" "4 oct]"},
    {"DECAY",  &g_decay,      0.30f, "\xcf\x84",  // τ
        "filter envelope length  [0.2 \xe2\x80\x93 2 s, bypassed on accent]"},
    {"ACCENT", &g_accent,     0.65f, "C\xe2\x82\x81\xe2\x82\x83",  // C₁₃
        "accent emphasis  [cap stacks across consecutive accents]"},
    {"VOL",    &g_volume,     0.65f, "VCA",
        "master output level"},
};

// format a knob's current value for display under the rail
static std::string format_knob(int idx) {
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

// ─────────────────────────────────────────────────────────────────────────────
// Presets
// ─────────────────────────────────────────────────────────────────────────────

struct Preset {
    const char*               name;
    std::array<StepData, 16>  steps;
};

// Helpers for compact preset construction. Notes are MIDI numbers.
//   NN = rest, otherwise pitch; flag chars: 'a' accent, 's' slide.
static constexpr int _ = -1;  // rest sentinel

static StepData sd(int note, const char* flags = "") {
    StepData s;
    if (note < 0) {
        s.rest = true;
    } else {
        s.rest   = false;
        s.note   = note;
        for (const char* p = flags; *p; ++p) {
            if (*p == 'a') s.accent = true;
            if (*p == 's') s.slide  = true;
        }
    }
    return s;
}

// Acid patterns inspired by iconic TB-303 tracks. Precise sequencer data for
// these records isn't publicly documented — these are community-consensus
// reconstructions (Attack Mag tutorials, KVR / Gearspace mega-threads, Robin
// Whittle's notes at firstpr.com.au). They're "plausible representations"
// that evoke each track rather than forensic reproductions.
//
// MIDI notes used below: E1=28, G1=31, A1=33, B1=35, C2=36, D2=38, Eb2=39,
// E2=40, F2=41, G2=43, A2=45, C3=48, Eb3=51.
static std::array<Preset, 16> g_presets = {{
    // 1. Phuture — "Acid Tracks" (1987). The founding acid record. Pedal
    //    point on C with two octave-ups on accented slides. The song's real
    //    identity is Pierre's live cutoff sweep — the notes are almost
    //    incidental.
    {"Acid Tracks", {{
        sd(36, "a"), sd(36),      sd(36),      sd(36, "as"),
        sd(48),      sd(36),      sd(36, "a"), sd(36),
        sd(36),      sd(36, "s"), sd(48, "a"), sd(36),
        sd(36),      sd(36, "a"), sd(39, "s"), sd(36),
    }}},
    // 2. Josh Wink — "Higher State of Consciousness". Pattern is deliberately
    //    simple; the song is the slow rising cutoff across many bars.
    {"Higher State", {{
        sd(36, "a"), sd(36),      sd(48, "s"), sd(36),
        sd(36, "a"), sd(36),      sd(39, "s"), sd(36),
        sd(36, "a"), sd(36),      sd(48, "s"), sd(36),
        sd(36, "a"), sd(43),      sd(39, "s"), sd(36),
    }}},
    // 3. Hardfloor — "Acperience 1" style. A-minor, heavy accents, octave-
    //    plus slides back to root produce the signature Hardfloor scream.
    {"Acperience", {{
        sd(33, "a"), sd(_),       sd(45, "s"), sd(33),
        sd(33, "a"), sd(36),      sd(40, "s"), sd(33),
        sd(33, "a"), sd(_),       sd(45, "s"), sd(43),
        sd(40, "a"), sd(38),      sd(36, "s"), sd(33),
    }}},
    // 4. Adonis — "No Way Back". Early Chicago acid. Call-and-response
    //    between the C root and upper Eb/F — one of the first 303 lines
    //    with a real riff shape rather than pure pedal tone.
    {"No Way Back", {{
        sd(36, "a"), sd(_),       sd(36),      sd(39, "s"),
        sd(36),      sd(_),       sd(36, "a"), sd(31),
        sd(36),      sd(_),       sd(36),      sd(39, "s"),
        sd(41, "a"), sd(39),      sd(36),      sd(31),
    }}},
    // 5. A Guy Called Gerald — "Voodoo Ray" (actually an MC-202, but the
    //    line translates). A minor pentatonic, open filter, "sung" rather
    //    than squelchy — recommend lowering RES when loading this one.
    {"Voodoo Ray", {{
        sd(33),      sd(_),       sd(45, "a"), sd(_),
        sd(40, "s"), sd(_),       sd(33),      sd(_),
        sd(33),      sd(_),       sd(45, "a"), sd(_),
        sd(43, "s"), sd(40),      sd(33),      sd(_),
    }}},
    // 6. Joey Beltram — "Energy Flash" style. Relentless 16ths on the E
    //    root — a rave engine, not a melody.
    {"Energy Flash", {{
        sd(28, "a"), sd(28),      sd(28),      sd(28, "a"),
        sd(28),      sd(28, "s"), sd(35),      sd(28),
        sd(28, "a"), sd(28),      sd(28),      sd(31, "s"),
        sd(28, "a"), sd(28),      sd(28),      sd(28),
    }}},
    // 7. Plastikman (Richie Hawtin). Sparse, high-resonance, long-decay
    //    minimalism — space and filter movement carry the groove.
    {"Plastikman", {{
        sd(36, "a"), sd(_),       sd(_),       sd(36),
        sd(_),       sd(36, "s"), sd(_),       sd(39, "a"),
        sd(_),       sd(36),      sd(_),       sd(_),
        sd(36, "a"), sd(_),       sd(39, "s"), sd(_),
    }}},
    // 8. Hoover / dub-techno — extremely sparse, long decay, subsonic.
    //    Filter movement over many bars is the performance.
    {"Hoover Dub", {{
        sd(36, "a"), sd(_),       sd(_),       sd(_),
        sd(_),       sd(_),       sd(_),       sd(_),
        sd(36, "a"), sd(_),       sd(_),       sd(31, "s"),
        sd(_),       sd(_),       sd(_),       sd(_),
    }}},
    // 9. Squelch — heavy-resonance showcase, single-octave jumps to drive
    //    the accent envelope stack.
    {"Squelch", {{
        sd(36, "a"), sd(_),       sd(48, "s"), sd(_),
        sd(36),      sd(_),       sd(48, "a"), sd(_),
        sd(36, "a"), sd(_),       sd(48, "s"), sd(_),
        sd(36),      sd(_),       sd(48, "a"), sd(_),
    }}},
    // 10. Detroit — rolling Underground Resistance bassline, A minor.
    {"Detroit", {{
        sd(33, "a"), sd(_),       sd(33),      sd(_),
        sd(40, "s"), sd(_),       sd(33, "a"), sd(_),
        sd(33),      sd(36, "s"), sd(_),       sd(40),
        sd(33, "a"), sd(_),       sd(31),      sd(33),
    }}},
    // 11. Dub 303 — half-time feel, extremely spacey.
    {"Dub 303", {{
        sd(24, "a"), sd(_),       sd(_),       sd(_),
        sd(_),       sd(_),       sd(36, "s"), sd(_),
        sd(24, "a"), sd(_),       sd(_),       sd(_),
        sd(36),      sd(_),       sd(31, "s"), sd(_),
    }}},
    // 12. Liquid — wall-to-wall slides. With the slide-doesn't-retrigger
    //    envelope fix, this one should now do the "continuously flowing"
    //    acid thing rather than retriggering every step.
    {"Liquid", {{
        sd(36, "a"), sd(43, "s"), sd(48, "s"), sd(43, "s"),
        sd(36),      sd(41, "s"), sd(48, "s"), sd(41, "s"),
        sd(36, "a"), sd(43, "s"), sd(48, "s"), sd(55, "s"),
        sd(48, "s"), sd(43, "s"), sd(41, "s"), sd(36),
    }}},
    // 13. Humanoid / Stakker — busy FSOL-era, octave hops every step.
    {"Humanoid", {{
        sd(36, "a"), sd(41, "s"), sd(48),      sd(41),
        sd(36),      sd(41, "s"), sd(48, "a"), sd(43),
        sd(36, "a"), sd(41, "s"), sd(51),      sd(48),
        sd(43),      sd(41),      sd(48, "s"), sd(36),
    }}},
    // 14. Melodic — longer phrased line, pseudo-arpeggio through a minor.
    {"Melodic", {{
        sd(36, "a"), sd(39),      sd(43),      sd(48, "s"),
        sd(46),      sd(43),      sd(39),      sd(36),
        sd(36, "a"), sd(43, "s"), sd(48),      sd(51),
        sd(48),      sd(43),      sd(39, "s"), sd(36),
    }}},
    // 15. Bassline — UK bassline swing, syncopated.
    {"Bassline", {{
        sd(36, "a"), sd(_),       sd(36),      sd(48, "a"),
        sd(_),       sd(36, "s"), sd(43),      sd(36),
        sd(36, "a"), sd(_),       sd(38),      sd(43, "s"),
        sd(_),       sd(36),      sd(48, "a"), sd(_),
    }}},
    // 16. Blank slate for the user to sketch on.
    {"Empty", {{
        sd(_), sd(_), sd(_), sd(_),  sd(_), sd(_), sd(_), sd(_),
        sd(_), sd(_), sd(_), sd(_),  sd(_), sd(_), sd(_), sd(_),
    }}},
}};

static void load_preset(int idx) {
    idx = ((idx % static_cast<int>(g_presets.size())) + static_cast<int>(g_presets.size()))
        % static_cast<int>(g_presets.size());
    g_preset_index = idx;
    g_steps        = g_presets[static_cast<size_t>(idx)].steps;
    g_step_sel     = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Audio plumbing
// ─────────────────────────────────────────────────────────────────────────────

static float midi_to_hz(int note) {
    return 440.0f * std::pow(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f);
}

// Push current knob values into the audio engine every frame. Cheap (atomics),
// and keeps parameters in perfect sync with the UI with no special plumbing.
static void push_params() {
    acid_set_cutoff(g_cutoff);
    acid_set_resonance(g_resonance);
    acid_set_env_mod(g_env_mod);
    acid_set_decay(g_decay);
    acid_set_accent_amt(g_accent);
    acid_set_volume(g_volume);
    acid_set_tuning_semi((g_tune - 0.5f) * 24.0f);
    acid_set_waveform(g_wave);
}

static void tick(float dt) {
    push_params();

    if (!g_playing) return;

    // 16th-note step duration from BPM
    float step_sec = 60.0f / std::max(g_bpm, 20.0f) / 4.0f;
    g_step_clock += dt;

    if (g_step_clock >= step_sec) {
        g_step_clock -= step_sec;
        g_current_step = (g_current_step + 1) % g_pattern_length;

        const auto& s = g_steps[static_cast<size_t>(g_current_step)];
        if (!s.rest) {
            // slide = "glide into this note from the previous one". Needs a
            // real held note to glide from, so only honour it when the prev
            // step actually played.
            int  prev = (g_current_step - 1 + g_pattern_length) % g_pattern_length;
            bool do_slide = s.slide && !g_steps[static_cast<size_t>(prev)].rest;
            acid_note_on(midi_to_hz(s.note), s.accent ? 1 : 0,
                         do_slide ? 1 : 0, step_sec);
        }
    }
}

static void start_playback() {
    g_playing = true;
    g_current_step = -1;
    g_step_clock = 1e9;  // force immediate advance on the very next tick
}

static void stop_playback() {
    g_playing = false;
    g_current_step = -1;
    acid_note_off();
}

// ─────────────────────────────────────────────────────────────────────────────
// Input handling
// ─────────────────────────────────────────────────────────────────────────────

static void adjust_knob(int idx, float delta) {
    if (idx == K_WAVE) {
        // wave is special-cased; treat up/down as toggle
        if (delta > 0) g_wave = 1 - g_wave;
        else           g_wave = 1 - g_wave;
        return;
    }
    float* v = g_knob_table[idx].value;
    *v = std::clamp(*v + delta, 0.0f, 1.0f);
}

static void reset_knob(int idx) {
    if (idx == K_WAVE) { g_wave = 0; return; }
    *g_knob_table[idx].value = g_knob_table[idx].default_v;
}

// cycle focus forward (or backward)
static void cycle_focus(int dir) {
    int n = 3;
    int cur = static_cast<int>(g_focus);
    cur = ((cur + dir) % n + n) % n;
    g_focus = static_cast<Section>(cur);
}

// map a typed letter (c, d, e, f, g, a, b) to a MIDI semitone (0=C .. 11=B)
static int letter_to_semitone(char c) {
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

static void handle_event(const Event& ev) {
    // ── Global keys ──────────────────────────────────────────────────────────
    if (key(ev, 'q') || key(ev, SpecialKey::Escape)) {
        if (g_help_open) { g_help_open = false; return; }
        maya::quit();
        return;
    }
    if (key(ev, '?')) { g_help_open = !g_help_open; return; }
    // While help is open we only accept Esc/? to close — everything else is no-op.
    if (g_help_open) return;

    if (key(ev, ' ')) { g_playing ? stop_playback() : start_playback(); return; }
    if (key(ev, SpecialKey::Tab))     { cycle_focus(+1); return; }
    if (key(ev, SpecialKey::BackTab)) { cycle_focus(-1); return; }

    // ── Section-specific keys ────────────────────────────────────────────────
    switch (g_focus) {
        case Section::Knobs: {
            if (key(ev, SpecialKey::Left))  { g_knob_sel = (g_knob_sel - 1 + K_COUNT) % K_COUNT; return; }
            if (key(ev, SpecialKey::Right)) { g_knob_sel = (g_knob_sel + 1) % K_COUNT; return; }
            if (key(ev, SpecialKey::Up))    { adjust_knob(g_knob_sel, +0.05f); return; }
            if (key(ev, SpecialKey::Down))  { adjust_knob(g_knob_sel, -0.05f); return; }
            if (key(ev, ']'))               { adjust_knob(g_knob_sel, +0.10f); return; }
            if (key(ev, '['))               { adjust_knob(g_knob_sel, -0.10f); return; }
            if (key(ev, '0'))               { reset_knob(g_knob_sel); return; }
            if (key(ev, 'w'))               { g_wave = 1 - g_wave; return; }
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
            if (key(ev, 'a')) { s.accent = !s.accent; s.rest = false; return; }
            if (key(ev, 's')) { s.slide  = !s.slide;  s.rest = false; return; }
            if (key(ev, 'r')) { s.rest   = !s.rest; return; }
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
            if (key(ev, 'x')) { s = StepData{}; s.rest = true; return; }
            break;
        }
        case Section::Transport: {
            if (key(ev, SpecialKey::Up))   { load_preset(g_preset_index - 1); return; }
            if (key(ev, SpecialKey::Down)) { load_preset(g_preset_index + 1); return; }
            if (key(ev, '[')) { g_bpm = std::max(40.0f,  g_bpm - 2.0f); return; }
            if (key(ev, ']')) { g_bpm = std::min(220.0f, g_bpm + 2.0f); return; }
            if (key(ev, '{')) { g_pattern_length = std::max(4, g_pattern_length - 1); return; }
            if (key(ev, '}')) { g_pattern_length = std::min(16, g_pattern_length + 1); return; }
            if (key(ev, ',')) { load_preset(g_preset_index - 1); return; }
            if (key(ev, '.')) { load_preset(g_preset_index + 1); return; }
            break;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Render
// ─────────────────────────────────────────────────────────────────────────────

static Element build_knob_row() {
    // TB-303 signal-flow groups. Each knob gets a group colour so adjacent
    // knobs visually bind into their functional block — OSC/FILTER/ENV/AMP —
    // without needing a horizontal divider between them.
    //   OSC     (pitch + wave)        : violet
    //   FILTER  (cutoff, Q, env mod)  : cyan
    //   ENV     (decay)               : amber
    //   AMP     (accent, volume)      : green
    auto group_color_for = [](int i) -> maya::Color {
        using maya::Color;
        switch (i) {
            case K_TUNE:   return Color::rgb(180, 130, 230); // OSC    violet
            case K_WAVE:   return Color::rgb(180, 130, 230);
            case K_CUTOFF: return Color::rgb( 90, 200, 220); // FILTER cyan
            case K_RES:    return Color::rgb( 90, 200, 220);
            case K_ENV:    return Color::rgb( 90, 200, 220);
            case K_DECAY:  return Color::rgb(230, 180,  90); // ENV    amber
            case K_ACCENT: return Color::rgb(120, 210, 140); // AMP    green
            case K_VOL:    return Color::rgb(120, 210, 140);
        }
        return tb303::clr_accent();
    };

    auto build_one = [&](int i) -> Element {
        bool focused = (g_focus == Section::Knobs) && (g_knob_sel == i);
        maya::Color gc = group_color_for(i);
        if (i == K_WAVE) {
            tb303::WaveformToggle w;
            w.index       = g_wave;
            w.focused     = focused;
            w.group_col   = gc;
            w.tech_symbol = g_knob_table[i].tech;
            return w.build();
        }
        tb303::Knob k;
        k.label         = g_knob_table[i].label;
        k.value         = *g_knob_table[i].value;
        k.default_value = g_knob_table[i].default_v;
        // 0.005 threshold keeps UI rounding from tripping "modified" — a
        // single ↑ tap moves by 0.05, well above the noise floor.
        k.modified      = std::abs(*g_knob_table[i].value - g_knob_table[i].default_v) > 0.005f;
        k.value_text    = format_knob(i);
        k.tech_symbol   = g_knob_table[i].tech;
        k.focused       = focused;
        k.group_col     = gc;
        return k.build();
    };

    // Build the 8-column strip in signal-flow order: OSC (tune, wave) →
    // VCF (cutoff, res, env) → EG (decay) → VCA (accent, vol). Knob::WIDTH
    // is 8 with a 1-col gap between columns → the strip is 71 cells wide.
    // justify(Center) lets the hstack expand to the panel width and centers
    // the 8 knobs in it, so the strip stays centered on wide terminals and
    // collapses neatly (even overflow both sides) on narrow ones.
    std::vector<Element> cols;
    cols.reserve(K_COUNT);
    for (int i : {K_TUNE, K_WAVE, K_CUTOFF, K_RES,
                  K_ENV,  K_DECAY, K_ACCENT, K_VOL}) {
        cols.push_back(build_one(i));
    }
    auto strip = (dsl::h(std::move(cols)) | dsl::gap(1)
                  | dsl::justify(Justify::Center)).build();

    // ── Schematic group header ─────────────────────────────────────────────
    // One flat rail per functional block (OSC / VCF / EG / VCA). Each rail
    // is its own Element with a fixed width that exactly spans the knob
    // columns beneath it, and rails are composed into an hstack with gap(1)
    // — mirroring the knob strip's structure cell-for-cell, so both rows
    // centre to identical positions under justify(Center).
    //   OSC = TUNE(8)   + gap + WAVE(8)                 = 17
    //   VCF = CUTOFF(8) + gap + RES(8) + gap + ENV(8)   = 26
    //   EG  = DECAY(8)                                  =  8
    //   VCA = ACCENT(8) + gap + VOL(8)                  = 17
    auto build_rail = [](const char* label, int width, maya::Color c) -> Element {
        int label_cols   = static_cast<int>(std::string_view(label).size());
        int dashes_total = std::max(0, width - 2 /*spaces*/ - label_cols);
        int left         = dashes_total / 2;
        int right        = dashes_total - left;

        std::string            txt;
        std::vector<StyledRun> runs;

        size_t l_off = txt.size();
        for (int i = 0; i < left; ++i) txt += "\xe2\x94\x80"; // ─
        txt += " ";
        runs.push_back(StyledRun{l_off, txt.size() - l_off,
            maya::Style{}.with_fg(c)});

        size_t lbl_off = txt.size();
        txt += label;
        runs.push_back(StyledRun{lbl_off, static_cast<size_t>(label_cols),
            maya::Style{}.with_fg(c).with_bold()});

        size_t r_off = txt.size();
        txt += " ";
        for (int i = 0; i < right; ++i) txt += "\xe2\x94\x80"; // ─
        runs.push_back(StyledRun{r_off, txt.size() - r_off,
            maya::Style{}.with_fg(c)});

        return Element{maya::TextElement{
            .content = std::move(txt),
            .style   = {},
            .wrap    = maya::TextWrap::NoWrap,
            .runs    = std::move(runs),
        }};
    };

    auto group_header = (dsl::h(
        build_rail("OSC", 17, group_color_for(K_TUNE)),
        build_rail("VCF", 26, group_color_for(K_CUTOFF)),
        build_rail("EG",   8, group_color_for(K_DECAY)),
        build_rail("VCA", 17, group_color_for(K_VOL))
    ) | dsl::gap(1) | dsl::justify(Justify::Center)).build();

    // Caption row: shows the focused knob's description inline with the
    // actual keybinds ("↑↓ ±5% · [ ] ±10% · 0 reset"). Appears whenever the
    // synth-voice section is focused; otherwise a quiet hint keeps the
    // panel height stable.
    auto build_caption = [&]() -> Element {
        std::string txt;
        std::vector<StyledRun> runs;
        bool kfoc = (g_focus == Section::Knobs);

        if (kfoc) {
            size_t off = txt.size();
            txt += "\xe2\x96\xb6 ";  // ▶
            runs.push_back(StyledRun{off, 3,
                maya::Style{}.with_fg(tb303::clr_accent()).with_bold()});

            size_t lbl_off = txt.size();
            txt += g_knob_table[g_knob_sel].label;
            runs.push_back(StyledRun{lbl_off, std::string_view(g_knob_table[g_knob_sel].label).size(),
                maya::Style{}.with_fg(tb303::clr_accent()).with_bold()});

            txt += "  ";
            size_t cap_off = txt.size();
            txt += g_knob_table[g_knob_sel].caption;
            runs.push_back(StyledRun{cap_off, std::string_view(g_knob_table[g_knob_sel].caption).size(),
                maya::Style{}.with_fg(tb303::clr_text())});

            txt += "   \xc2\xb7   ";
            size_t h_off = txt.size();
            txt += "\xe2\x86\x91\xe2\x86\x93 turn  \xc2\xb7  \xe2\x86\x90\xe2\x86\x92 select"
                   "  \xc2\xb7  [ ] \xc2\xb1" "10%  \xc2\xb7  0 reset";
            runs.push_back(StyledRun{h_off, txt.size() - h_off,
                maya::Style{}.with_fg(tb303::clr_muted())});
        } else {
            size_t off = txt.size();
            txt += "\xe2\x97\xa6 ";  // ◦
            runs.push_back(StyledRun{off, 3,
                maya::Style{}.with_fg(tb303::clr_muted())});
            size_t msg_off = txt.size();
            txt += "Tab here to shape the voice  \xc2\xb7  \xe2\x86\x90\xe2\x86\x92 select knob, \xe2\x86\x91\xe2\x86\x93 turn it";
            runs.push_back(StyledRun{msg_off, txt.size() - msg_off,
                maya::Style{}.with_fg(tb303::clr_muted())});
        }

        return Element{maya::TextElement{
            .content = std::move(txt),
            .style   = {},
            .wrap    = maya::TextWrap::NoWrap,
            .runs    = std::move(runs),
        }};
    };

    return dsl::vstack()
        .border(BorderStyle::Round)
        .border_color(g_focus == Section::Knobs ? tb303::clr_accent() : tb303::clr_panel_hi())
        .border_text(
            " \xe2\x97\x87 SYNTH VOICE \xc2\xb7 VCO\xe2\x86\x92VCF\xe2\x86\x92VCA"
            " \xc2\xb7 24 dB/oct LADDER \xe2\x97\x87 ",   // ◇ SYNTH VOICE · VCO→VCF→VCA · 24 dB/oct LADDER ◇
            BorderTextPos::Top)
        .padding(0, 2, 0, 2)(
            std::move(group_header),
            std::move(strip),
            build_caption()
        );
}

static Element render_frame() {
    // Transport state bundle
    std::vector<tb303::PresetSummary> preset_summaries;
    preset_summaries.reserve(g_presets.size());
    for (const auto& p : g_presets) {
        preset_summaries.push_back(tb303::PresetSummary{
            .name  = p.name,
            .steps = std::vector<StepData>(p.steps.begin(), p.steps.end()),
        });
    }

    tb303::TransportState ts{
        .playing        = g_playing,
        .bpm            = g_bpm,
        .pattern_length = g_pattern_length,
        .current_step   = g_current_step,
        .pattern_index  = g_preset_index,
        .presets        = std::move(preset_summaries),
        .steps          = std::span<const StepData>{g_steps.data(), g_steps.size()},
        .focused        = (g_focus == Section::Transport),
    };

    auto knob_row   = build_knob_row();
    auto filter_box = tb303::build_filter_response(g_cutoff, g_resonance);
    auto transport  = tb303::build_transport(ts);

    auto middle_row = (dsl::h(
        std::move(filter_box) | dsl::grow(2),
        std::move(transport)  | dsl::grow(3)
    ) | dsl::gap(1) | dsl::grow(1)).build();

    auto seq = tb303::build_sequencer(g_steps, g_pattern_length, g_step_sel,
                                      g_playing ? g_current_step : -1);
    auto seq_panel = dsl::vstack()
        .border(BorderStyle::Round)
        .border_color(g_focus == Section::Sequencer ? tb303::clr_accent() : tb303::clr_panel_hi())
        .border_text(" SEQUENCER ", BorderTextPos::Top)
        .padding(0, 1, 0, 1)(std::move(seq));

    auto help_bar = tb303::build_help_bar(g_focus, g_help_open);

    // Title bar: a compact brand strip with a pulse-colored dot that flips
    // from muted → red while playing, so the play state is legible from a
    // glance even when the transport panel is off-screen.
    auto brand = Element{TextElement{
        .content = "\xe2\x97\x86 TB-303",                    // ◆
        .style   = Style{}.with_fg(tb303::clr_accent()).with_bold(),
        .wrap    = TextWrap::NoWrap,
    }};
    auto brand_sub = Element{TextElement{
        .content = "  acid bass simulator",
        .style   = Style{}.with_fg(tb303::clr_muted()),
        .wrap    = TextWrap::NoWrap,
    }};
    auto pulse = Element{TextElement{
        .content = g_playing ? "\xe2\x97\x8f  live" : "\xe2\x97\x8b  idle",  // ● / ○
        .style   = Style{}.with_fg(g_playing ? tb303::clr_hot() : tb303::clr_muted())
                         .with_bold(),
        .wrap    = TextWrap::NoWrap,
    }};
    auto title_bar = (dsl::h(
        std::move(brand), std::move(brand_sub), dsl::space, std::move(pulse)
    ) | dsl::padding(0, 1, 0, 1)).build();

    // Help replaces the main workspace (keeps title + status bar visible) so
    // the user always has context and knows how to close it.
    if (g_help_open) {
        return (dsl::v(
            std::move(title_bar),
            tb303::build_help_overlay() | dsl::grow(1),
            std::move(help_bar)
        ) | dsl::gap(0)).build();
    }

    return (dsl::v(
        std::move(title_bar),
        std::move(knob_row),
        std::move(middle_row),
        std::move(seq_panel),
        std::move(help_bar)
    ) | dsl::gap(0)).build();
}

// ─────────────────────────────────────────────────────────────────────────────
// Entry point
// ─────────────────────────────────────────────────────────────────────────────

int main() {
    load_preset(0);
    acid_start();

    using clock = std::chrono::steady_clock;
    auto last = clock::now();

    maya::run(
        RunConfig{
            .title = "tb-303",
            .fps   = 30,
            .mouse = false,
            .mode  = Mode::Fullscreen,
        },
        [](const Event& ev) { handle_event(ev); },
        [&] {
            auto now = clock::now();
            float dt = std::chrono::duration<float>(now - last).count();
            last = now;
            // clamp — protects against debugger pauses or initial frame
            dt = std::clamp(dt, 0.0f, 0.1f);
            tick(dt);
            return render_frame();
        }
    );

    acid_stop();
    return 0;
}
