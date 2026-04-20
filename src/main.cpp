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
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
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
static float g_drive     = 0.20f;   // output tanh pre-gain
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
// Swing: 0.50 = straight 16ths, 0.62 ≈ classic MPC swing, 0.66 = hard
// shuffle. Even-indexed steps (0,2,4…) play on the grid; odd-indexed steps
// are delayed by (swing − 0.5) × one-pair duration so the pair still sums
// to two 16ths. Applies uniformly to all steps so every pattern grooves.
static float g_swing   = 0.50f;

// Song mode (Phase 4.1). When enabled, the transport auto-advances through
// saved pattern slots on each pattern wrap — classic hardware "chain" mode.
// g_song_slot is the slot currently loaded (1..9, 0 = none). Wrap detection
// piggy-backs on g_current_step going backwards between ticks, so no extra
// audio-thread coupling is needed.
static bool g_song_mode = false;
static int  g_song_slot = 0;

// Jam mode (Phase 4.2). When enabled, the sequencer is silenced and the
// computer keyboard becomes a two-octave piano (tracker layout). Terminals
// don't deliver key-release events, so each press retriggers a short gate
// (g_jam_gate_s) that tick() closes with acid_note_off when it expires.
// g_jam_last_midi drives the "PLAYING: C3" display; -1 means silent.
static bool  g_jam_mode      = false;
static int   g_jam_octave    = 3;      // C3 = MIDI 36 — classic bass starting octave
static bool  g_jam_accent    = false;  // toggled with '
static bool  g_jam_slide     = false;  // toggled with ;
static float g_jam_gate_s    = 0.0f;
static int   g_jam_last_midi = -1;

// FX rack. Separate from the core voice knobs because the signal chain
// splits clearly at the VCA output: everything before is "synth voice",
// everything after is "effects". Order = (od pre-filter) → delay → reverb
// so the FX row is read left-to-right as signal flow.
static float g_od_amt     = 0.0f;          // pre-filter overdrive
static float g_delay_mix  = 0.0f;          // 0 = bypass
static float g_delay_fb   = 0.35f;
static int   g_delay_div  = 2;             // 0=1/16, 1=1/16d, 2=1/8, 3=1/8d
static float g_rev_mix    = 0.0f;
static float g_rev_size   = 0.55f;
static float g_rev_damp   = 0.35f;

// MIDI I/O (Phase 5). Out = forward note events + drum hits + clock to the
// MIDI backend port. In sync = slave our BPM + transport to incoming MIDI
// clock / start / stop. Note-in (controller → bass voice) is always on when
// the backend is up; these flags only gate the tempo-sync and event-output
// paths so a user can have a keyboard plugged in without accidentally
// broadcasting clock to everything downstream.
static bool g_midi_out  = false;
static bool g_midi_sync = false;

// Drum machine (Phase 4.3). Flat bool grid — nine voices (rows) × 16 steps
// (cols). Pushed into the engine each tick via acid_seq_set_drum_lane; the
// bool representation is kept here because it's easier to toggle than a
// packed mask, and the pack-to-uint16 happens right before each audio push.
static constexpr int kDrumVoices = 16;
static constexpr const char* kDrumLabels[kDrumVoices] = {
    "BD","SD","CH","OH","CL","LT","HT","RS","CB","SH","TB","CG",
    "MT","CY","RD","BG"
};
static std::array<std::array<bool, 16>, kDrumVoices> g_drums{};
static int   g_drum_voice_sel = 0;     // which row (voice) is being edited
static int   g_drum_step_sel  = 0;     // which step in that row
static float g_drum_master    = 1.0f;  // drum-bus master send (0 = silent)

// Per-section mute toggles — `m` in any non-sequencer section flips the
// relevant flag. Sequencer keeps `m` for its per-step rest toggle (each step
// is its own "channel"). Mutes are applied right before pushing volumes to
// the engine so the rest of the state stays intact.
static bool  g_synth_mute  = false;    // silences the 303 voice (knobs/FX/seq)
static bool  g_drums_mute  = false;    // silences the drum bus

// UI focus
static Section g_focus      = Section::Sequencer;
static int     g_knob_sel   = 2;   // K_CUTOFF — the signature 303 knob
static int     g_fx_sel     = 0;   // currently-focused FX knob
static int     g_step_sel   = 0;
static bool    g_help_open  = false;

// Beat-sync animation state. g_step_phase ramps 0..1 within the current step
// (wraps each trigger) so the UI can fade flashes smoothly. g_toast is a
// transient banner — whenever an action fires (randomize / save / load /
// export) we set g_toast + g_toast_t and the title bar shows it, fading out
// over ~1.5s.
static float       g_step_phase = 0.0f;
static std::string g_toast;
static float       g_toast_t    = 0.0f;    // seconds remaining
static constexpr float kToastDur = 1.5f;

// Mouse support — captured terminal size (updated each frame via the root
// component wrapper and by ResizeEvent) plus click-drag state for knobs. We
// don't know widget screen positions from the framework, so hit-testing is
// done from the known static row heights + centred widget widths below.
static int   g_term_w = 0;
static int   g_term_h = 0;
static int   g_knob_drag_idx = -1;   // -1 when not dragging
static int   g_knob_drag_y0  = 0;
static float g_knob_drag_v0  = 0.0f;

// Scroll offsets for areas that may overflow the visible region:
//   help_bar   — horizontal scroll (units of items), bumped by wheel on row H-1
//   help_ov    — vertical scroll (line offset), bumped by wheel inside the
//                help overlay modal
static int   g_help_bar_scroll = 0;
// Seconds accumulator for the help bar's marquee animation. Every
// `kHelpBarChipDur` seconds the bar advances by one chip; the builder only
// applies the rotation when the full chip set is wider than the terminal.
static float g_help_bar_auto_t   = 0.0f;
static constexpr float kHelpBarChipDur = 1.2f;
static int   g_help_ov_scroll  = 0;

static void set_toast(std::string s) {
    g_toast   = std::move(s);
    g_toast_t = kToastDur;
}

// ─────────────────────────────────────────────────────────────────────────────
// Knob descriptor table — makes per-knob behaviour data-driven
// ─────────────────────────────────────────────────────────────────────────────

// Order here is the on-screen column order AND the "signal-flow" left-to-
// right ordering: OSC (TUNE+WAVE) → VCF (CUTOFF+RES+ENVMOD) → EG (DECAY) →
// VCA (ACCENT+VOL). Reading the strip is reading the block diagram.
enum KnobIdx {
    K_TUNE = 0, K_WAVE, K_CUTOFF, K_RES, K_ENV, K_DECAY, K_ACCENT, K_DRIVE, K_VOL,
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
    {"DRIVE",  &g_drive,      0.20f, "\xe2\x88\xab",   // ∫ (integrator / saturator glyph)
        "output stage saturation  [clean \xe2\x86\x92 pedal-grit]"},
    {"VOL",    &g_volume,     0.65f, "VCA",
        "master output level"},
};

// ─────────────────────────────────────────────────────────────────────────────
// FX rack descriptor table (Phase 1.1 = delay only; more effects land here)
// ─────────────────────────────────────────────────────────────────────────────
enum FxIdx {
    FX_OD = 0,
    FX_DMIX, FX_DFB, FX_DTIME,
    FX_RMIX, FX_RSIZE, FX_RDAMP,
    FX_COUNT
};

struct FxDesc {
    const char* label;
    float*      value;           // nullptr for FX_DTIME (discrete)
    float       default_v;
    const char* tech;
    const char* caption;
};

static FxDesc g_fx_table[FX_COUNT] = {
    {"O-DRIVE",  &g_od_amt,     0.0f,  "OD",
        "pre-filter overdrive  [0 = clean, 1 = '303 into fuzz pedal']"},
    {"DLY MIX",  &g_delay_mix,  0.0f,  "w/d",
        "delay wet/dry send  [0 = bypass, 1 = full echo tail]"},
    {"DLY FB",   &g_delay_fb,   0.35f, "fb",
        "delay feedback  [0 = single tap, 1 = self-oscillating regen]"},
    {"DLY TIME", nullptr,       0.0f,  "\xe2\x99\xaa",  // ♪
        "delay time  [tempo-synced: 1/16, 1/16d, 1/8, 1/8d]"},
    {"REV MIX",  &g_rev_mix,    0.0f,  "w/d",
        "plate reverb send  [0 = bypass, 1 = full plate ambience]"},
    {"REV SIZE", &g_rev_size,   0.55f, "RT",
        "plate size / decay  [room \xe2\x86\x92 hall \xe2\x86\x92 cathedral]"},
    {"REV DAMP", &g_rev_damp,   0.35f, "HF",
        "plate HF damping  [bright plate \xe2\x86\x92 dark dub chamber]"},
};

static const char* kDlyDivNames[4] = {"1/16", "1/16d", "1/8", "1/8d"};

static std::string format_fx(int idx) {
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

// Forward-declares — the save/export helpers need to reach into the audio
// transport below, but we want to declare the pattern I/O up here where the
// rest of the pattern management lives.
static void start_playback();
static void stop_playback();

// ─────────────────────────────────────────────────────────────────────────────
// Vibe coordination — the top-level "roll everything" button picks one
// overarching vibe and routes all five subsystems (knobs, FX, pattern, drums,
// transport) to archetypes that go together. A random acid-house vibe should
// land on 128 BPM with four-on-the-floor drums, a classic-squelch patch, and
// slapback delay — not a 90 BPM dub patch with a techno drum pattern.
//
// Each randomize_X below accepts an optional archetype hint; individual
// per-section dice rolls pass no hint (pure random), randomize_everything()
// picks a Vibe from kVibes and forwards the matching hint to each helper so
// the whole kit stays internally coherent.
// ─────────────────────────────────────────────────────────────────────────────

// File-scope archetype enums so the Vibe table can index them by int. Each
// matches the same ordering used inside the individual randomize_X helpers.
enum KnobsArch  { KA_CLASSIC, KA_SQUELCH, KA_DRIVING, KA_DUBBY, KA_COUNT };
enum PatArch    { PA_PEDAL,   PA_DRIVING, PA_MELODIC, PA_DUB,   PA_COUNT };
enum FxArch     { FA_DRY,     FA_SLAP,    FA_DUB,     FA_CAVERN, FA_DIRTY, FA_COUNT };
// Drum genres — must mirror the `Groove` enum inside randomize_drums().
enum DrumArch   {
    DA_ACID, DA_CHICAGO, DA_TECHNO, DA_MINIMAL, DA_ELECTRO,
    DA_BREAKS, DA_DUB, DA_LATIN, DA_JAM, DA_COUNT
};

enum class Vibe {
    ACID_HOUSE,      // Chicago '87 — classic squelch over a 4/4 kick at 128 BPM
    DEEP_HOUSE,      // warmer, lazier — 122 BPM, dub-heavy delay, clap on 2+4
    TECHNO,          // Detroit driving — 132 BPM, 16th hats, open filter
    HARDFLOOR,       // acid trance — 138 BPM, self-osc, layered melodic patterns
    ELECTRO,         // 808 electro — 125 BPM, syncopated kick + snare on 2/4
    MINIMAL,         // sparse — 128 BPM, dry FX, minimal drums
    DUB_TECHNO,      // 118 BPM, long reverb, half-time feel
    BREAKS,          // breakbeat — 135 BPM, syncopated, tom + rim fills
    LATIN_HOUSE,     // 124 BPM, conga/bongo led, classic knobs
    AMBIENT_ACID,    // 100 BPM, long decay, pedal notes, cavernous FX
    V_COUNT
};

struct VibeSpec {
    const char* name;
    int         knobs_arch;     // KA_*
    int         pattern_arch;   // PA_*
    int         fx_arch;        // FA_*
    int         drums_arch;     // DA_*
    float       bpm_center;     // target BPM (jitter ± bpm_spread)
    float       bpm_spread;
    float       swing_prob;     // chance of picking a shuffled swing
    bool        square_bias;    // true = favour square wave for this vibe
};

// One row per Vibe. Order matches the Vibe enum 1:1 so we can index by int.
static const VibeSpec kVibes[static_cast<int>(Vibe::V_COUNT)] = {
    {"acid house",    KA_CLASSIC, PA_PEDAL,   FA_SLAP,    DA_ACID,    128.0f, 2.0f, 0.30f, false},
    {"deep house",    KA_CLASSIC, PA_MELODIC, FA_DUB,     DA_CHICAGO, 122.0f, 2.5f, 0.55f, false},
    {"techno",        KA_DRIVING, PA_DRIVING, FA_SLAP,    DA_TECHNO,  132.0f, 2.5f, 0.15f, false},
    {"hardfloor",     KA_SQUELCH, PA_MELODIC, FA_DIRTY,   DA_TECHNO,  138.0f, 2.0f, 0.10f, false},
    {"electro",       KA_CLASSIC, PA_DRIVING, FA_SLAP,    DA_ELECTRO, 125.0f, 2.0f, 0.20f, true },
    {"minimal",       KA_DUBBY,   PA_PEDAL,   FA_DRY,     DA_MINIMAL, 128.0f, 1.5f, 0.25f, false},
    {"dub techno",    KA_DUBBY,   PA_DUB,     FA_CAVERN,  DA_DUB,     118.0f, 2.5f, 0.50f, false},
    {"breakbeat",     KA_DRIVING, PA_MELODIC, FA_DIRTY,   DA_BREAKS,  135.0f, 3.0f, 0.20f, false},
    {"latin house",   KA_CLASSIC, PA_PEDAL,   FA_SLAP,    DA_LATIN,   124.0f, 2.0f, 0.55f, false},
    {"ambient acid",  KA_DUBBY,   PA_DUB,     FA_CAVERN,  DA_MINIMAL, 100.0f, 4.0f, 0.20f, false},
};

// ─────────────────────────────────────────────────────────────────────────────
// Knob randomizer — a dice roll over the *correlated* acid parameter space.
// We pick one of four character archetypes (classic / squelchy / driving /
// dubby), each pre-baked with sensible inter-knob relationships, then jitter
// within that archetype. This consistently produces playable patches:
//   * high Q pairs with low-mid cutoff (so the envelope has room to scream)
//   * long decay pairs with lower env mod (slow evolving sweeps)
//   * short decay pairs with heavy env mod + high accent (snappy squelch)
// A naive rand(0..1) across every knob usually lands on muddy / inaudible
// patches — the archetype pass keeps each roll musical.
//
// `arch_hint` forces a specific archetype (used by randomize_everything to
// match the picked Vibe); default -1 leaves the helper to roll its own dice.
// ─────────────────────────────────────────────────────────────────────────────
static void randomize_knobs(int arch_hint = -1, bool square_bias = false,
                            bool silent = false) {
    static std::mt19937 rng{std::random_device{}()};
    auto u = [&](float lo, float hi) {
        return lo + (hi - lo) * std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
    };
    auto chance = [&](float p) {
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng) < p;
    };

    int a = (arch_hint >= 0)
        ? std::clamp(arch_hint, 0, KA_COUNT - 1)
        : std::uniform_int_distribution<int>(0, KA_COUNT - 1)(rng);

    // Tuning: always near centre — detuning kills "in-key" playback. ±1 st.
    g_tune = u(0.48f, 0.52f);

    switch (a) {
        case KA_CLASSIC:
            // Mid cutoff, strong Q, moderate sweep, medium decay — the
            // universal acid starting point.
            g_cutoff    = u(0.30f, 0.50f);
            g_resonance = u(0.65f, 0.85f);
            g_env_mod   = u(0.50f, 0.75f);
            g_decay     = u(0.25f, 0.50f);
            g_accent    = u(0.50f, 0.75f);
            break;
        case KA_SQUELCH:
            // Self-osc territory: very high Q, low cutoff (max headroom for
            // the MEG to sweep), tight decay and heavy accent.
            g_cutoff    = u(0.15f, 0.35f);
            g_resonance = u(0.80f, 0.95f);
            g_env_mod   = u(0.70f, 0.95f);
            g_decay     = u(0.10f, 0.30f);
            g_accent    = u(0.70f, 0.90f);
            break;
        case KA_DRIVING:
            // Open filter, less envelope motion, medium resonance — the
            // Beltram "Energy Flash" engine sound.
            g_cutoff    = u(0.50f, 0.75f);
            g_resonance = u(0.55f, 0.75f);
            g_env_mod   = u(0.30f, 0.55f);
            g_decay     = u(0.20f, 0.45f);
            g_accent    = u(0.55f, 0.80f);
            break;
        case KA_DUBBY:
            // Low cutoff, long decay, moderate Q — Plastikman / dub techno
            // territory. Sustained filter motion over bars.
            g_cutoff    = u(0.10f, 0.30f);
            g_resonance = u(0.55f, 0.80f);
            g_env_mod   = u(0.40f, 0.70f);
            g_decay     = u(0.55f, 0.90f);
            g_accent    = u(0.35f, 0.60f);
            break;
    }

    g_volume = u(0.55f, 0.70f);           // keep output in a safe range
    // Square is classic but ~30% of rolls — saw is canonical 303. A vibe hint
    // can flip the bias (electro prefers square).
    g_wave   = chance(square_bias ? 0.70f : 0.30f) ? 1 : 0;

    if (!silent) {
        static const char* kNames[KA_COUNT] = {"classic", "squelch", "driving", "dubby"};
        char buf[32];
        std::snprintf(buf, sizeof(buf), "\xe2\x86\xbb knobs: %s", kNames[a]);
        set_toast(buf);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Pattern randomizer (musical, not purely random)
// ─────────────────────────────────────────────────────────────────────────────
// Picks one of four groove archetypes (pedal / driving / melodic / dub),
// each of which parameterises note density, octave-jump frequency, slide
// probability, and accent weighting differently. All pitches are drawn from
// a minor-pentatonic/Dorian superset — the canonical 303 vocabulary
// (Voodoo Ray, Energy Flash, Acperience etc.). A few hand-picked musical
// constraints keep every roll playable:
//
//   * downbeats (steps 0, 4, 8, 12) favour the tonic so the groove lands
//   * at least one accent per bar — flat patterns sound dead
//   * slides are only kept when the previous step played (you can't glide
//     into a note from a rest)
//   * slides resolve DOWN to the tonic on the next downbeat, which is the
//     signature 303 "squelch back home" motion
//
// This is strictly better than a flat per-step dice roll, which produces
// listless noise roughly 80% of the time.
static void randomize_pattern(int arch_hint = -1, bool silent = false) {
    static std::mt19937 rng{std::random_device{}()};
    auto U = [&]() {
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
    };
    auto chance = [&](float p) { return U() < p; };

    static constexpr int kTonic = 36;                      // C2
    // Minor pentatonic + b7 + octave + octave+5. Each entry is the semitone
    // offset from the tonic. Selection is weight-biased: root/5 heavy,
    // upper notes sparse. Indices: 0=root, 1=♭3, 2=4, 3=5, 4=♭7, 5=oct,
    // 6=oct+5.
    static constexpr int kScale[]   = {0, 3, 5, 7, 10, 12, 15};
    static constexpr int kWeight[]  = {7, 2, 3, 4,  2,  4,  1};  // totals 23
    static constexpr int kWeightSum = 7+2+3+4+2+4+1;

    auto pick_scale_tone = [&]() -> int {
        int r = std::uniform_int_distribution<int>(0, kWeightSum - 1)(rng);
        int acc = 0;
        for (size_t i = 0; i < std::size(kScale); ++i) {
            acc += kWeight[i];
            if (r < acc) return kScale[i];
        }
        return kScale[0];
    };

    int gr = (arch_hint >= 0)
        ? std::clamp(arch_hint, 0, PA_COUNT - 1)
        : std::uniform_int_distribution<int>(0, PA_COUNT - 1)(rng);

    // Per-groove shape parameters.
    //   rest_rate  : probability an off-beat becomes a rest (sparseness)
    //   jump_rate  : probability a non-root note lands (colour-tone density)
    //   slide_rate : base slide probability per non-rest note
    //   accent_off : off-beat accent probability
    float rest_rate = 0.25f, jump_rate = 0.35f, slide_rate = 0.25f, accent_off = 0.15f;
    switch (gr) {
        case PA_PEDAL:   rest_rate = 0.20f; jump_rate = 0.20f; slide_rate = 0.25f; accent_off = 0.10f; break;
        case PA_DRIVING: rest_rate = 0.05f; jump_rate = 0.30f; slide_rate = 0.20f; accent_off = 0.25f; break;
        case PA_MELODIC: rest_rate = 0.15f; jump_rate = 0.60f; slide_rate = 0.35f; accent_off = 0.20f; break;
        case PA_DUB:     rest_rate = 0.55f; jump_rate = 0.30f; slide_rate = 0.40f; accent_off = 0.15f; break;
    }

    // ── Pass 1: lay down notes + rests ──────────────────────────────────────
    bool has_accent = false;
    for (int i = 0; i < g_pattern_length; ++i) {
        auto& s = g_steps[static_cast<size_t>(i)];
        s = StepData{};
        bool downbeat = (i % 4 == 0);

        if (!downbeat && chance(rest_rate)) {
            s.rest = true;
            continue;
        }

        int pitch;
        if (downbeat && chance(0.80f)) {
            pitch = kTonic;
        } else if (chance(jump_rate)) {
            pitch = kTonic + pick_scale_tone();
        } else {
            pitch = kTonic;
        }
        s.note   = pitch;
        s.rest   = false;
        s.accent = downbeat ? chance(0.70f) : chance(accent_off);
        if (s.accent) has_accent = true;
    }

    // ── Pass 2: slides (second pass so we know neighbours are real notes) ──
    for (int i = 1; i < g_pattern_length; ++i) {
        auto& s = g_steps[static_cast<size_t>(i)];
        if (s.rest) continue;
        const auto& prev = g_steps[static_cast<size_t>(i - 1)];
        if (prev.rest) continue;
        // More likely to slide INTO upper notes than back down to root —
        // that's the "pull up" direction the 303 envelope emphasises.
        float p = slide_rate;
        if (s.note > prev.note) p *= 1.5f;
        if (chance(std::min(p, 0.9f))) s.slide = true;
    }

    // ── Pass 3: musical guarantees ─────────────────────────────────────────
    // If somehow the whole bar ended up flat, force an accent on beat 1.
    if (!has_accent) {
        g_steps[0].accent = true;
        g_steps[0].rest   = false;
        if (g_steps[0].note < kTonic) g_steps[0].note = kTonic;
    }
    // Ensure beat 1 always plays — a pattern that starts on a rest reads as
    // confusing when looped.
    if (g_steps[0].rest) {
        g_steps[0].rest = false;
        g_steps[0].note = kTonic;
    }

    // Clear trailing steps when pattern_length < 16.
    for (int i = g_pattern_length; i < 16; ++i) {
        g_steps[static_cast<size_t>(i)] = StepData{};
        g_steps[static_cast<size_t>(i)].rest = true;
    }

    g_preset_index = -1;                   // mark as "custom, not a preset"
    g_step_sel     = 0;

    if (!silent) {
        static const char* kNames[PA_COUNT] = {"pedal", "driving", "melodic", "dub"};
        char buf[32];
        std::snprintf(buf, sizeof(buf), "\xe2\x86\xbb pattern: %s", kNames[gr]);
        set_toast(buf);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// FX randomizer — five named archetypes, matched to how the 303 actually gets
// processed in a rack: DRY (clean), SLAP (close-mic echo), DUB (feedback-
// soaked delay), CAVERN (plate-reverb wash), DIRTY (overdrive-forward). We
// pick archetype-friendly ranges so each roll is usable instead of mud.
// ─────────────────────────────────────────────────────────────────────────────
static void randomize_fx(int arch_hint = -1, bool silent = false) {
    static std::mt19937 rng{std::random_device{}()};
    auto u = [&](float lo, float hi) {
        return lo + (hi - lo) * std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
    };

    int a = (arch_hint >= 0)
        ? std::clamp(arch_hint, 0, FA_COUNT - 1)
        : std::uniform_int_distribution<int>(0, FA_COUNT - 1)(rng);

    switch (a) {
        case FA_DRY:
            g_od_amt    = u(0.00f, 0.10f);
            g_delay_mix = u(0.00f, 0.10f);
            g_delay_fb  = u(0.20f, 0.35f);
            g_delay_div = 2;                // 1/8 — neutral tempo sync
            g_rev_mix   = u(0.00f, 0.10f);
            g_rev_size  = u(0.40f, 0.55f);
            g_rev_damp  = u(0.35f, 0.55f);
            break;
        case FA_SLAP:
            g_od_amt    = u(0.05f, 0.25f);
            g_delay_mix = u(0.20f, 0.40f);  // audible but never drowning
            g_delay_fb  = u(0.15f, 0.30f);  // short tail, not regenerative
            g_delay_div = std::uniform_int_distribution<int>(0, 1)(rng);  // 1/16 or 1/16d
            g_rev_mix   = u(0.05f, 0.15f);
            g_rev_size  = u(0.35f, 0.55f);
            g_rev_damp  = u(0.30f, 0.50f);
            break;
        case FA_DUB:
            g_od_amt    = u(0.10f, 0.30f);
            g_delay_mix = u(0.35f, 0.55f);  // hefty wet
            g_delay_fb  = u(0.55f, 0.78f);  // regenerative but not self-osc
            g_delay_div = std::uniform_int_distribution<int>(2, 3)(rng);  // 1/8 or 1/8d
            g_rev_mix   = u(0.15f, 0.30f);
            g_rev_size  = u(0.55f, 0.75f);
            g_rev_damp  = u(0.40f, 0.65f);
            break;
        case FA_CAVERN:
            g_od_amt    = u(0.00f, 0.15f);
            g_delay_mix = u(0.00f, 0.20f);
            g_delay_fb  = u(0.20f, 0.40f);
            g_delay_div = std::uniform_int_distribution<int>(1, 3)(rng);
            g_rev_mix   = u(0.45f, 0.70f);  // lush plate
            g_rev_size  = u(0.70f, 0.95f);  // big room
            g_rev_damp  = u(0.20f, 0.45f);  // bright, not muffled
            break;
        case FA_DIRTY:
            g_od_amt    = u(0.55f, 0.85f);  // the "pedal grit" setting
            g_delay_mix = u(0.10f, 0.30f);
            g_delay_fb  = u(0.25f, 0.50f);
            g_delay_div = std::uniform_int_distribution<int>(0, 3)(rng);
            g_rev_mix   = u(0.05f, 0.25f);
            g_rev_size  = u(0.40f, 0.65f);
            g_rev_damp  = u(0.35f, 0.60f);
            break;
    }

    if (!silent) {
        static const char* kNames[FA_COUNT] = {"dry", "slap", "dub", "cavern", "dirty"};
        char buf[32];
        std::snprintf(buf, sizeof(buf), "\xe2\x86\xbb fx: %s", kNames[a]);
        set_toast(buf);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Drum randomizer — genre-grammar engine over the 16-voice kit. Instead of
// dropping a fixed bitmap per genre, each genre declares:
//   - a kick "backbone" (deterministic kick pattern, the genre's fingerprint)
//   - a backbeat rule (snare / clap / rim placement)
//   - a hat/cymbal driver (CH, OH, RD, or sparse)
//   - a perc palette (shaker / tambourine / cowbell / conga / bongo weights)
//   - a crash accent flag (bar-top CY on step 0 or 12)
//   - a tom-fill chance + tom-walk preference (LT→MT→HT ladder on last beat)
//   - a density multiplier so "more" = busier, "less" = sparser
//
// Then a few universal polish passes run on top (ghost notes, accent-layer
// crash, weighted hat variation, slight 1/32 ratchet via retrigger) so every
// roll reads like a composed groove rather than random noise.
//
// Voice indices: 0=BD 1=SD 2=CH 3=OH 4=CL 5=LT 6=HT 7=RS 8=CB 9=SH 10=TB
// 11=CG 12=MT 13=CY 14=RD 15=BG.
// ─────────────────────────────────────────────────────────────────────────────
static void randomize_drums(int arch_hint = -1, bool silent = false) {
    static std::mt19937 rng{std::random_device{}()};
    auto u01 = [&] {
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
    };
    auto chance = [&](float p) { return u01() < std::clamp(p, 0.0f, 1.0f); };
    auto pick   = [&](int lo, int hi) {
        return std::uniform_int_distribution<int>(lo, hi)(rng);
    };

    // Clear the kit — callers may already have a dense pattern they expect to
    // be replaced, not OR'd into. A `hit()` helper hides the clumsy array
    // indexing everywhere downstream.
    for (auto& row : g_drums) for (auto& c : row) c = false;
    auto hit = [&](int voice, int step) {
        if (voice < 0 || voice >= kDrumVoices) return;
        if (step  < 0 || step  >= 16)          return;
        g_drums[static_cast<size_t>(voice)][static_cast<size_t>(step)] = true;
    };
    auto unhit = [&](int voice, int step) {
        if (voice < 0 || voice >= kDrumVoices) return;
        if (step  < 0 || step  >= 16)          return;
        g_drums[static_cast<size_t>(voice)][static_cast<size_t>(step)] = false;
    };
    auto has = [&](int voice, int step) {
        if (voice < 0 || voice >= kDrumVoices) return false;
        if (step  < 0 || step  >= 16)          return false;
        return g_drums[static_cast<size_t>(voice)][static_cast<size_t>(step)];
    };

    // Must match the order of DA_* (file-scope) so the Vibe table's
    // drums_arch int indexes into this switch correctly.
    enum Groove {
        ACID, CHICAGO, TECHNO, MINIMAL, ELECTRO,
        BREAKS, DUB, LATIN, JAM, G_COUNT
    };
    static_assert(G_COUNT == DA_COUNT, "drum genre count mismatch");
    Groove gen = (arch_hint >= 0)
        ? static_cast<Groove>(std::clamp(arch_hint, 0, G_COUNT - 1))
        : static_cast<Groove>(pick(0, G_COUNT - 1));

    // Density roll — a per-call "how busy should this be" factor. Clusters
    // around 0.75 (musical default) with a tail toward either side so the same
    // genre can produce sparse OR dense grooves from successive rolls.
    float density = std::clamp(0.75f + (u01() - 0.5f) * 0.55f, 0.45f, 1.05f);

    // Hat driver — one of the three cymbal voices leads the top end:
    //   CH = closed hat on offbeats (house / techno default)
    //   OH = open hat offbeats (lazier, dub / deep house)
    //   RD = ride cymbal on 8ths (driving, jazz-leaning, ambient techno)
    // Skipping entirely ("none") is also a valid option for ultra-sparse.
    enum Hat { HAT_CH, HAT_OH, HAT_RD, HAT_NONE };
    auto chance_hat = [&](float ch, float oh, float rd) -> Hat {
        float r = u01();
        if (r < ch) return HAT_CH;
        if (r < ch + oh) return HAT_OH;
        if (r < ch + oh + rd) return HAT_RD;
        return HAT_NONE;
    };

    // Universal helper: lay down a 16th-note run on a given voice with a
    // base hit probability. Odd-step bias lets callers ask for offbeats only
    // ("odd_only") or full 16ths ("step=1").
    auto sprinkle = [&](int voice, float p, int step_mod, int offset) {
        for (int i = offset; i < 16; i += step_mod) if (chance(p)) hit(voice, i);
    };

    // Tom walk — three-tom ladder LT→MT→HT, landing across the last N steps.
    // Used as a fill at the end of bar 2 (steps 13/14/15) when the genre's
    // fill-chance rolls through.
    auto tom_walk = [&](int start_step) {
        static const int voices[3] = { 5 /*LT*/, 12 /*MT*/, 6 /*HT*/ };
        int s = std::clamp(start_step, 0, 13);
        for (int i = 0; i < 3; ++i) hit(voices[i], s + i);
    };

    // ── Genre dispatch ──────────────────────────────────────────────────────
    switch (gen) {
        case ACID: {
            // Chicago-acid skeleton: kick 4-on-the-floor, clap on 2/4, no
            // snare. The 303 should carry the groove — drums stay sparse.
            hit(0, 0); hit(0, 4); hit(0, 8); hit(0, 12);
            hit(4, 4); hit(4, 12);
            Hat h = chance_hat(0.60f, 0.10f, 0.15f);
            if (h == HAT_CH)
                for (int i = 2; i < 16; i += 4) if (chance(0.85f * density)) hit(2, i);
            else if (h == HAT_OH)
                for (int i = 2; i < 16; i += 4) if (chance(0.70f * density)) hit(3, i);
            else if (h == HAT_RD)
                for (int i = 0; i < 16; i += 2) if (chance(0.65f * density)) hit(14, i);
            if (chance(0.35f * density)) hit(8, 10);                   // lonely cowbell
            if (chance(0.35f * density))                               // rim ghosts
                for (int i : { 3, 11 }) if (chance(0.6f)) hit(7, i);
            if (chance(0.25f * density)) hit(13, 0);                   // crash on 1
            break;
        }
        case CHICAGO: {
            // Deep Chicago / early UK house: kick 4-on-floor, clap on 2+4,
            // an open hat on the 'and' of 4, soft shaker underneath.
            for (int i = 0; i < 16; i += 4) hit(0, i);
            hit(4, 4); hit(4, 12);
            sprinkle(2, 0.95f * density, 4, 2);                         // CH offbeats
            if (chance(0.55f)) hit(3, 14);                              // OH on the "and of 4"
            if (chance(0.60f * density))
                sprinkle(9, 0.75f, 2, 1);                               // shaker
            if (chance(0.35f * density)) sprinkle(11, 0.4f, 4, 3);      // conga on "and"
            if (chance(0.25f)) hit(10, 12);                             // tambourine on 4
            break;
        }
        case TECHNO: {
            // Driving 4/4 techno: kick on every quarter, snappy 16ths on
            // hats, synced cowbell syncopation, optional ride under the hat.
            for (int i = 0; i < 16; i += 4) hit(0, i);
            if (chance(0.45f)) hit(1, 4);                               // snare on 2
            if (chance(0.75f)) hit(4, 12);                              // clap on 4
            sprinkle(2, 0.55f * density, 2, 1);                         // 16th CH
            if (chance(0.55f * density))
                sprinkle(14, 0.55f, 2, 0);                              // ride on 8ths
            if (chance(0.45f * density)) {                              // cb on "and"
                hit(8, 6); hit(8, 14);
            }
            if (chance(0.70f * density)) sprinkle(9, 0.95f, 1, 0);      // 16th shaker drive
            if (chance(0.35f)) hit(13, 12);                             // crash on bar-top
            if (chance(0.20f)) tom_walk(13);                            // tom fill on last beat
            break;
        }
        case MINIMAL: {
            // Sparse. One kick per bar-half, one clap, maybe a rim tick.
            hit(0, 0); hit(0, 8);
            if (chance(0.55f)) hit(4, 12); else hit(1, 12);
            if (chance(0.50f * density))
                for (int i : { 2, 10 }) if (chance(0.55f)) hit(2, i);
            if (chance(0.35f * density)) hit(3, 14);                    // "and of 4" OH
            if (chance(0.30f * density))
                sprinkle(9, 0.35f, 4, 2);                               // whisper shaker
            if (chance(0.18f)) hit(8, 7);                               // single cowbell
            break;
        }
        case ELECTRO: {
            // Electro / 808: syncopated kick 1+11, snare on 4+12, no 4/4
            // kick. Heavy on the CB and CH. 808 clap layered with snare.
            hit(0, 0); hit(0, 11);
            if (chance(0.50f)) hit(0, 7);                               // syncopated extra
            hit(1, 4); hit(1, 12);
            if (chance(0.65f)) { hit(4, 4); hit(4, 12); }               // clap layer
            sprinkle(2, 0.65f * density, 2, 1);                         // CH offbeats
            if (chance(0.40f)) { hit(3, 6); hit(3, 14); }
            if (chance(0.75f))                                          // cb on 2 and 4 "ands"
                for (int i : { 2, 6, 10, 14 }) if (chance(0.55f)) hit(8, i);
            if (chance(0.45f * density)) sprinkle(9, 0.7f, 2, 1);       // shaker
            if (chance(0.25f)) hit(13, 0);                              // crash on 1
            break;
        }
        case BREAKS: {
            // Breakbeat: broken kick, snare on 5+13, busy hats, tom+rim fills.
            hit(0, 0); hit(0, 10);
            if (chance(0.55f)) hit(0, 6);
            if (chance(0.40f)) hit(0, 14);
            hit(1, 4); hit(1, 12);
            if (chance(0.55f)) hit(7, 3);                               // rim ghost
            if (chance(0.55f)) hit(7, 11);
            sprinkle(2, 0.60f * density, 1, 0);
            if (chance(0.45f)) hit(3, 7);
            if (chance(0.55f)) hit(3, 15);
            if (chance(0.60f)) tom_walk(13);
            if (chance(0.35f)) { hit(5, 2); hit(12, 9); }               // sprinkle toms
            if (chance(0.35f)) sprinkle(11, 0.45f, 2, 1);               // conga ghost
            if (chance(0.20f)) hit(13, 12);                             // crash on last
            break;
        }
        case DUB: {
            // Half-time feel: single kick + long OH, claps echoed, delay-heavy
            // arrangement friendly. Leave lots of empty space.
            hit(0, 0);
            if (chance(0.60f)) hit(0, 8);                               // optional 2nd kick
            hit(4, 8);                                                  // clap on "3"
            if (chance(0.50f)) hit(3, 4);                               // OH on 2
            if (chance(0.50f)) hit(3, 12);                              // OH on 4
            if (chance(0.40f * density)) sprinkle(2, 0.45f, 4, 2);      // sparse CH
            if (chance(0.40f * density)) sprinkle(11, 0.40f, 4, 3);     // ghost conga
            if (chance(0.55f * density))
                for (int i = 2; i < 16; i += 4) if (chance(0.5f)) hit(9, i);
            if (chance(0.30f)) hit(13, 0);                              // crash on 1
            break;
        }
        case LATIN: {
            // Latin-house / afro-house: kick 4/4, heavy conga+bongo grooves,
            // cowbell clave, shaker 16ths, light clap. The perc carries more
            // weight than the backbeat.
            for (int i = 0; i < 16; i += 4) hit(0, i);
            if (chance(0.70f)) hit(4, 12);                              // clap on 4
            if (chance(0.60f)) hit(8, 6);                               // clave-ish CB
            if (chance(0.60f)) hit(8, 10);
            // Rumba-ish conga pattern: 0,3,6,10,13 is a common 3-2 clave cell
            static const int conga_a[] = { 3, 6, 10, 13 };
            for (int s : conga_a) if (chance(0.75f * density)) hit(11, s);
            // Bongo on the "ands" — higher-pitched, so it rides on top.
            static const int bongo_a[] = { 2, 5, 9, 14 };
            for (int s : bongo_a) if (chance(0.65f * density)) hit(15, s);
            if (chance(0.80f)) sprinkle(9, 0.90f, 1, 0);                // driving shaker
            if (chance(0.35f)) sprinkle(10, 0.55f, 4, 2);               // tambourine
            if (chance(0.20f)) hit(13, 0);
            break;
        }
        case JAM: {
            // Open jam — everyone gets a shot. Kicks half 4/4, half syncopated.
            // Clap + snare both make appearances. Used for practice loops.
            bool four_on_floor = chance(0.55f);
            if (four_on_floor) for (int i = 0; i < 16; i += 4) hit(0, i);
            else { hit(0, 0); hit(0, 7); hit(0, 10); if (chance(0.5f)) hit(0, 14); }
            if (chance(0.70f)) { hit(1, 4); hit(1, 12); }
            if (chance(0.55f)) { hit(4, 4); hit(4, 12); }
            Hat h = chance_hat(0.35f, 0.20f, 0.30f);
            if (h == HAT_CH) sprinkle(2, 0.65f * density, 2, 1);
            if (h == HAT_OH) { hit(3, 6); hit(3, 14); if (chance(0.5f)) hit(3, 2); }
            if (h == HAT_RD) sprinkle(14, 0.60f * density, 2, 0);
            if (chance(0.45f)) sprinkle(11, 0.50f * density, 2, 1);     // conga
            if (chance(0.45f)) sprinkle(15, 0.45f * density, 4, 2);     // bongo
            if (chance(0.40f)) sprinkle(9, 0.60f * density, 2, 1);      // shaker
            if (chance(0.35f)) sprinkle(10, 0.40f * density, 4, 0);     // tambourine
            if (chance(0.30f)) hit(8, 6);                               // cowbell
            if (chance(0.40f)) tom_walk(13);
            if (chance(0.30f)) hit(13, 0);
            break;
        }
        default: break;
    }

    // ── Universal polish ───────────────────────────────────────────────────
    // These passes add life on top of any genre-specific backbone without
    // overriding it — so an ACID roll still feels like acid, just nudged.
    //
    // Ghost-rim: quiet rim clicks scattered on empty 16ths add micro-timing.
    if (chance(0.55f * density)) {
        for (int i = 1; i < 16; i += 2) {
            if (!has(7, i) && !has(1, i) && chance(0.08f * density)) hit(7, i);
        }
    }
    // Shimmer pass: a subtle tambourine doubles the clap/snare on 4 if empty.
    if (chance(0.30f * density) && !has(10, 12) && (has(4, 12) || has(1, 12))) {
        hit(10, 12);
    }
    // Final-beat accent: cymbals often hit on step 12 with the snare/clap for
    // a musical lift into the next bar. Only fires if nothing already there.
    if (chance(0.20f * density) && !has(13, 12) && !has(14, 12)) {
        if (chance(0.5f)) hit(13, 12); else hit(14, 12);
    }
    // Constraint: if CH and RD both land on the same step, kill the CH hit
    // there — they compete in the top-end and muddy each other.
    for (int i = 0; i < 16; ++i) {
        if (has(2, i) && has(14, i)) unhit(2, i);
    }

    if (!silent) {
        static const char* kNames[G_COUNT] = {
            "acid", "chicago", "techno", "minimal", "electro",
            "breaks", "dub", "latin", "jam"
        };
        char buf[48];
        std::snprintf(buf, sizeof(buf), "\xe2\x86\xbb drums: %s", kNames[gen]);
        set_toast(buf);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Transport randomizer — tempo / swing / length. Stays inside the musical
// envelope of actual acid records (115-140 BPM, straight or lightly shuffled,
// 12/16-step patterns) rather than the full knob ranges, so a roll here
// doesn't send the groove to 60 BPM or 7-step weirdness.
// ─────────────────────────────────────────────────────────────────────────────
static void randomize_transport(float bpm_center = 0.0f,
                                float bpm_spread = 0.0f,
                                float swing_prob = -1.0f,
                                bool  silent     = false) {
    static std::mt19937 rng{std::random_device{}()};
    auto u = [&](float lo, float hi) {
        return lo + (hi - lo) * std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
    };

    if (bpm_center > 0.0f) {
        // Vibe-coordinated tempo — jitter around the vibe's center BPM.
        float spread = (bpm_spread > 0.0f) ? bpm_spread : 2.0f;
        g_bpm = bpm_center + u(-spread, spread);
    } else {
        // Stand-alone: cluster around classic scene tempos.
        static constexpr float kBpms[] = {118.0f, 122.0f, 125.0f, 128.0f, 132.0f, 138.0f};
        g_bpm = kBpms[std::uniform_int_distribution<int>(0, 5)(rng)] + u(-1.5f, 1.5f);
    }
    g_bpm = std::clamp(g_bpm, 40.0f, 220.0f);

    // Swing: unless the vibe asks otherwise, 40% chance of a gentle shuffle.
    // Never hard swing here — the `-/=` keys can push it further if the user
    // wants. swing_prob >= 0 overrides the default coin-flip.
    float p_swing = (swing_prob >= 0.0f) ? swing_prob : 0.40f;
    g_swing = (u(0.0f, 1.0f) >= p_swing) ? 0.50f : u(0.54f, 0.62f);

    // Length: bias toward 16 (bar), sometimes 12 (odd meter feel) or 8 (short
    // loop). 4 and 6 are too short to sound like a finished groove.
    int r = std::uniform_int_distribution<int>(0, 9)(rng);
    g_pattern_length = (r < 6) ? 16 : (r < 9) ? 12 : 8;

    if (!silent) {
        char buf[40];
        std::snprintf(buf, sizeof(buf), "\xe2\x86\xbb transport: %.0f bpm",
                      static_cast<double>(g_bpm));
        set_toast(buf);
    }
}

// Randomize everything — pick one Vibe from kVibes and route every subsystem
// (knobs, FX, pattern, drums, transport) to archetypes that belong together.
// Each helper is called in silent mode so their individual toasts don't stomp
// each other, then one combined toast names the vibe.
//
// This is what makes capital-R musical: an acid-house roll produces 128 BPM
// with four-on-the-floor drums, classic-squelch knobs, and slapback delay,
// rather than the mismatched tempo/knobs/drums you'd get from five
// independent dice rolls.
static void randomize_everything() {
    static std::mt19937 rng{std::random_device{}()};
    int vi = std::uniform_int_distribution<int>(
        0, static_cast<int>(Vibe::V_COUNT) - 1)(rng);
    const VibeSpec& v = kVibes[vi];

    randomize_knobs    (v.knobs_arch,   v.square_bias, /*silent=*/true);
    randomize_fx       (v.fx_arch,      /*silent=*/true);
    randomize_pattern  (v.pattern_arch, /*silent=*/true);
    randomize_drums    (v.drums_arch,   /*silent=*/true);
    randomize_transport(v.bpm_center, v.bpm_spread, v.swing_prob,
                        /*silent=*/true);

    char buf[48];
    std::snprintf(buf, sizeof(buf), "\xe2\x86\xbb %s", v.name);
    set_toast(buf);
}

// ─────────────────────────────────────────────────────────────────────────────
// Evolving mutator — one small, weighted-random change per call. Repeated
// presses "walk" the pattern without obliterating it, a vibe somewhere
// between Generative Sequencer and Mutable Instruments' "Marbles". The key
// design goal: after N presses the pattern is recognisably descended from
// where it started, not pure noise.
// ─────────────────────────────────────────────────────────────────────────────
static void mutate_pattern() {
    static std::mt19937 rng{std::random_device{}()};
    auto urnd = [&]() {
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
    };
    auto pick = [&](int lo, int hi) {
        if (hi <= lo) return lo;
        return std::uniform_int_distribution<int>(lo, hi - 1)(rng);
    };

    // Weighted mutation menu. Probabilities sum to 1.0; biggest slice goes to
    // accent/slide toggles since those change "vibe" without changing pitch.
    const float r = urnd();
    const char* label = "mutate";

    if (r < 0.28f) {
        // Toggle accent on a random non-rest step.
        std::vector<int> cands;
        for (int i = 0; i < g_pattern_length; ++i)
            if (!g_steps[static_cast<size_t>(i)].rest) cands.push_back(i);
        if (!cands.empty()) {
            int i = cands[static_cast<size_t>(pick(0, static_cast<int>(cands.size())))];
            g_steps[static_cast<size_t>(i)].accent ^= true;
            label = "mutate: accent";
        }
    } else if (r < 0.52f) {
        // Toggle slide on a random non-rest step (only where next step plays).
        std::vector<int> cands;
        for (int i = 0; i < g_pattern_length; ++i) {
            if (g_steps[static_cast<size_t>(i)].rest) continue;
            int ni = (i + 1) % g_pattern_length;
            if (!g_steps[static_cast<size_t>(ni)].rest) cands.push_back(i);
        }
        if (!cands.empty()) {
            int i = cands[static_cast<size_t>(pick(0, static_cast<int>(cands.size())))];
            g_steps[static_cast<size_t>(i)].slide ^= true;
            label = "mutate: slide";
        }
    } else if (r < 0.72f) {
        // Nudge a step's pitch by ±1 or ±2 semitones, clamped into 303 range.
        std::vector<int> cands;
        for (int i = 0; i < g_pattern_length; ++i)
            if (!g_steps[static_cast<size_t>(i)].rest) cands.push_back(i);
        if (!cands.empty()) {
            int i = cands[static_cast<size_t>(pick(0, static_cast<int>(cands.size())))];
            static constexpr int steps[] = {-2, -1, 1, 2};
            int d = steps[pick(0, 4)];
            auto& s = g_steps[static_cast<size_t>(i)];
            s.note = std::clamp(s.note + d, 24, 84);
            label = "mutate: pitch";
        }
    } else if (r < 0.84f) {
        // Octave jump on a random note step — ±12 semitones, reversible next
        // call by picking the opposite direction.
        std::vector<int> cands;
        for (int i = 0; i < g_pattern_length; ++i)
            if (!g_steps[static_cast<size_t>(i)].rest) cands.push_back(i);
        if (!cands.empty()) {
            int i = cands[static_cast<size_t>(pick(0, static_cast<int>(cands.size())))];
            int d = (urnd() < 0.5f) ? -12 : 12;
            auto& s = g_steps[static_cast<size_t>(i)];
            int nxt = s.note + d;
            if (nxt >= 24 && nxt <= 84) s.note = nxt;
            label = "mutate: octave";
        }
    } else if (r < 0.94f) {
        // Toggle rest on a random step — but NEVER on beat 1 (that's the
        // anchor that makes a pattern feel coherent when looped).
        int i = pick(1, g_pattern_length);
        auto& s = g_steps[static_cast<size_t>(i)];
        if (s.rest) {
            s.rest = false;
            // Bias a new note toward the tonic or its fifth for musicality.
            static constexpr int nudges[] = {0, 0, 3, 5, 7, 12};
            int base = g_steps[0].rest ? 36 : g_steps[0].note;
            s.note = std::clamp(base + nudges[pick(0, 6)], 24, 84);
        } else {
            s.rest = true;
            s.accent = false;
            s.slide  = false;
        }
        label = "mutate: rest";
    } else {
        // Rotate pattern by one step (left or right). Cheapest way to make the
        // groove feel "different" without actually touching the content.
        bool right = urnd() < 0.5f;
        std::array<StepData, 16> tmp = g_steps;
        int n = g_pattern_length;
        for (int i = 0; i < n; ++i) {
            int src = right ? (i - 1 + n) % n : (i + 1) % n;
            g_steps[static_cast<size_t>(i)] = tmp[static_cast<size_t>(src)];
        }
        label = right ? "mutate: rotate \xe2\x86\x92" : "mutate: rotate \xe2\x86\x90";
    }

    g_preset_index = -1;
    set_toast(label);
}

// ─────────────────────────────────────────────────────────────────────────────
// Pattern persistence — save/load current 16-step pattern as JSON-ish text
// under ~/.config/acidflow/. One slot per keyboard digit 1..9; slot 0 is
// "last", used for the quick-load round-trip.
// ─────────────────────────────────────────────────────────────────────────────

static std::filesystem::path config_dir() {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    const char* home = std::getenv("HOME");
    std::filesystem::path base = xdg ? xdg : (home ? std::string(home) + "/.config" : std::string("."));
    return base / "acidflow";
}

static std::filesystem::path pattern_path(int slot) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "pattern_%d.txt", slot);
    return config_dir() / buf;
}

// Minimal human-readable format — one step per line, fields space-separated:
//   note rest accent slide
// Header line carries pattern_length + bpm so we restore full state.
static bool save_pattern_slot(int slot) {
    std::error_code ec;
    std::filesystem::create_directories(config_dir(), ec);
    if (ec) return false;

    std::ofstream f(pattern_path(slot));
    if (!f) return false;

    // v8: adds knobs + fx lines after the drum block so v4-v7 readers stop
    // at EOF cleanly. v7 had kDrumVoices (16) drum rows; v8 keeps that and
    // appends swing/wave/tune/knobs and od/delay/reverb FX params.
    f << "# acidflow pattern v8\n";
    f << g_pattern_length << " " << g_bpm << "\n";
    // Per-step line (v3+):
    //   note rest accent slide prob ratchet lockmask lockC lockR lockE lockA
    // Older readers (v1/v2) stop at field 4/6 and silently default the
    // trailing fields.
    for (int i = 0; i < 16; ++i) {
        const auto& s = g_steps[static_cast<size_t>(i)];
        f << s.note << " " << (s.rest ? 1 : 0) << " "
          << (s.accent ? 1 : 0) << " " << (s.slide ? 1 : 0) << " "
          << s.prob << " " << s.ratchet << " "
          << static_cast<int>(s.lock_mask) << " "
          << s.lock_cutoff << " " << s.lock_res << " "
          << s.lock_env    << " " << s.lock_accent << "\n";
    }
    // v4 drum block — one line per voice, 16 ints (0/1) + final drum master.
    // Kept as a trailing section so v3 readers just stop at EOF.
    f << "drums " << g_drum_master << "\n";
    for (int v = 0; v < kDrumVoices; ++v) {
        for (int i = 0; i < 16; ++i) {
            f << (g_drums[static_cast<size_t>(v)][static_cast<size_t>(i)] ? 1 : 0);
            if (i < 15) f << " ";
        }
        f << "\n";
    }
    // v8 knobs + FX block — appended after drums so v4-v7 readers stop at EOF
    // cleanly. The wave field is an int; all others are floats on [0..1] except
    // delay_div (0..3) which is also an int.
    f << "knobs " << g_swing << " " << g_wave << " " << g_tune
      << " " << g_cutoff    << " " << g_resonance << " " << g_env_mod
      << " " << g_decay     << " " << g_accent    << " " << g_drive
      << " " << g_volume    << "\n";
    f << "fx " << g_od_amt   << " " << g_delay_mix << " " << g_delay_fb
      << " " << g_delay_div  << " " << g_rev_mix   << " " << g_rev_size
      << " " << g_rev_damp   << "\n";
    bool ok = static_cast<bool>(f);
    if (ok) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "\xe2\x9c\x93 saved slot %d", slot);
        set_toast(buf);
    }
    return ok;
}

static bool load_pattern_slot(int slot, bool quiet = false) {
    std::ifstream f(pattern_path(slot));
    if (!f) return false;

    std::string header;
    std::getline(f, header);
    // v2 adds trailing prob+ratchet; v3 adds p-lock fields; v4 adds a drum
    // block (5 voices); v5 extends drums to 9; v6 extends to 12 (SH/TB/CG);
    // v7 extends to 16 (MT/CY/RD/BG); v8 adds knobs + fx lines after drums.
    // The reader always consumes up to kDrumVoices rows and bails at EOF,
    // so older files leave newer voices empty.
    bool v8 = header.find("v8") != std::string::npos;
    bool v7 = v8 || header.find("v7") != std::string::npos;
    bool v6 = v7 || header.find("v6") != std::string::npos;
    bool v5 = v6 || header.find("v5") != std::string::npos;
    bool v4 = v5 || header.find("v4") != std::string::npos;
    bool v3 = v4 || header.find("v3") != std::string::npos;
    bool v2 = v3 || header.find("v2") != std::string::npos;
    (void)v7; (void)v6; (void)v5;

    int plen; float bpm;
    if (!(f >> plen >> bpm)) return false;
    g_pattern_length = std::clamp(plen, 4, 16);
    g_bpm = std::clamp(bpm, 40.0f, 220.0f);

    for (int i = 0; i < 16; ++i) {
        int note, rest, acc, sld;
        if (!(f >> note >> rest >> acc >> sld)) break;
        auto& s = g_steps[static_cast<size_t>(i)];
        s.note   = std::clamp(note, 12, 96);
        s.rest   = rest != 0;
        s.accent = acc != 0;
        s.slide  = sld != 0;
        s.prob    = 100;
        s.ratchet = 1;
        s.lock_mask   = 0;
        s.lock_cutoff = 0.0f;
        s.lock_res    = 0.0f;
        s.lock_env    = 0.0f;
        s.lock_accent = 0.0f;
        if (v2) {
            int prob, rat;
            if (f >> prob >> rat) {
                s.prob    = std::clamp(prob, 0, 100);
                s.ratchet = std::clamp(rat,  1,   4);
            }
        }
        if (v3) {
            int   lm; float lc, lr, le, la;
            if (f >> lm >> lc >> lr >> le >> la) {
                s.lock_mask   = static_cast<uint8_t>(lm & 0xF);
                s.lock_cutoff = std::clamp(lc, 0.0f, 1.0f);
                s.lock_res    = std::clamp(lr, 0.0f, 1.0f);
                s.lock_env    = std::clamp(le, 0.0f, 1.0f);
                s.lock_accent = std::clamp(la, 0.0f, 1.0f);
            }
        }
    }
    // v4+: drum block — "drums <master>\n" header, then N rows of 16 ints
    // (v4 = 5 rows, v5 = 9 rows). We always read up to kDrumVoices rows and
    // stop early if the stream dries up, so v4 files still load cleanly with
    // the 4 extra voices empty. Always reset the kit first so older-format
    // loads wipe any drums from a prior session rather than leaking them
    // into a drum-less pattern.
    for (auto& row : g_drums) for (auto& c : row) c = false;
    if (v4) {
        std::string tag; float dm = 1.0f;
        if (f >> tag >> dm && tag == "drums") {
            g_drum_master = std::clamp(dm, 0.0f, 1.5f);
            for (int v = 0; v < kDrumVoices; ++v) {
                bool row_ok = true;
                for (int i = 0; i < 16; ++i) {
                    int x = 0;
                    if (!(f >> x)) { row_ok = false; break; }
                    g_drums[static_cast<size_t>(v)][static_cast<size_t>(i)] = x != 0;
                }
                if (!row_ok) break;      // short file → leave remaining voices empty
            }
        }
    }
    // v8: knobs + FX lines appended after the drum block.
    if (v8) {
        std::string tag;
        if (f >> tag && tag == "knobs") {
            float sw, tune, cut, res, em, dec, acc, drv, vol;
            int   wav;
            if (f >> sw >> wav >> tune >> cut >> res >> em >> dec >> acc >> drv >> vol) {
                g_swing      = std::clamp(sw,   0.50f, 0.75f);
                g_wave       = std::clamp(wav, 0, 1);
                g_tune       = std::clamp(tune, 0.0f, 1.0f);
                g_cutoff     = std::clamp(cut,  0.0f, 1.0f);
                g_resonance  = std::clamp(res,  0.0f, 1.0f);
                g_env_mod    = std::clamp(em,   0.0f, 1.0f);
                g_decay      = std::clamp(dec,  0.0f, 1.0f);
                g_accent     = std::clamp(acc,  0.0f, 1.0f);
                g_drive      = std::clamp(drv,  0.0f, 1.0f);
                g_volume     = std::clamp(vol,  0.0f, 1.0f);
            }
        }
        if (f >> tag && tag == "fx") {
            float od, dmix, dfb, rmix, rsize, rdamp;
            int   ddiv;
            if (f >> od >> dmix >> dfb >> ddiv >> rmix >> rsize >> rdamp) {
                g_od_amt    = std::clamp(od,    0.0f, 1.0f);
                g_delay_mix = std::clamp(dmix,  0.0f, 1.0f);
                g_delay_fb  = std::clamp(dfb,   0.0f, 1.0f);
                g_delay_div = std::clamp(ddiv,  0, 3);
                g_rev_mix   = std::clamp(rmix,  0.0f, 1.0f);
                g_rev_size  = std::clamp(rsize, 0.0f, 1.0f);
                g_rev_damp  = std::clamp(rdamp, 0.0f, 1.0f);
            }
        }
    }
    g_preset_index = -1;
    g_step_sel = 0;
    if (!quiet) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "\xe2\x86\xba loaded slot %d", slot);
        set_toast(buf);
    }
    return true;
}

// Song-mode helper — find the next saved slot after `after` (cyclic, 1..9).
// Returns 0 when nothing is saved at all. When `after` is already saved and is
// the only saved slot, it returns `after` so song mode loops the same pattern.
static int song_next_slot(int after) {
    for (int step = 1; step <= 9; ++step) {
        int s = ((after - 1 + step) % 9) + 1;
        if (std::filesystem::exists(pattern_path(s))) return s;
    }
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// WAV export — offline bounce the current pattern (4 loops) to
// ~/.config/acidflow/bounce.wav via the engine's synchronous render path.
// ─────────────────────────────────────────────────────────────────────────────
static bool export_wav() {
    std::error_code ec;
    std::filesystem::create_directories(config_dir(), ec);
    if (ec) return false;

    auto path = config_dir() / "bounce.wav";
    // Pack each step into the 32-bit layout acid_render_wav → acid_seq_set_step
    // unpacks: midi in the low byte, flags at bit 8 (rest=1|acc=2|slide=4),
    // prob at bit 11 (7 bits), ratchet-1 at bit 18 (2 bits).
    int notes[16];
    for (int i = 0; i < 16; ++i) {
        const auto& s = g_steps[static_cast<size_t>(i)];
        int flags = 0;
        if (s.rest)   flags |= 1;
        if (s.accent) flags |= 2;
        if (s.slide)  flags |= 4;
        int p = std::clamp(s.prob,    0, 100);
        int r = std::clamp(s.ratchet, 1,   4) - 1;
        notes[i] = (s.note & 0xFF)
                 | ((flags & 0x7) << 8)
                 | ((p     & 0x7F) << 11)
                 | ((r     & 0x3)  << 18);
    }
    // Stop live audio so we don't race the engine's global state.
    bool was_playing = g_playing;
    stop_playback();
    acid_stop();

    int rc = acid_render_wav(path.string().c_str(), notes,
                             g_pattern_length, g_bpm, g_swing, /*loops=*/4);

    acid_start();                        // restart the live audio thread
    if (was_playing) start_playback();

    set_toast(rc == 0 ? "\xe2\x9c\x93 wav \xe2\x86\x92 bounce.wav"
                      : "\xe2\x9c\x97 wav failed");
    return rc == 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Live recording — tap the running audio thread's output into
// ~/.config/acidflow/live.wav. Unlike the offline bounce (export_wav) this
// captures knob tweaks, jammed notes, MIDI-in hits, and pattern edits as
// they happen. Toggle with capital `W`; max 10 minutes per take.
// ─────────────────────────────────────────────────────────────────────────────
static constexpr int kLiveRecordMaxSeconds = 600;

static bool toggle_live_record() {
    if (acid_is_recording()) {
        std::error_code ec;
        std::filesystem::create_directories(config_dir(), ec);
        auto path = config_dir() / "live.wav";
        int rc = acid_record_end(path.string().c_str());
        set_toast(rc == 0 ? "\xe2\x9c\x93 live \xe2\x86\x92 live.wav"
                          : "\xe2\x9c\x97 live save failed");
        return rc == 0;
    }
    int rc = acid_record_begin(kLiveRecordMaxSeconds);
    if (rc != 0) {
        set_toast("\xe2\x9c\x97 record begin failed");
        return false;
    }
    set_toast("\xe2\x97\x8f REC  (W again to stop)");
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// MIDI export — write the current pattern as a Standard MIDI File (type 0) to
// ~/.config/acidflow/bounce.mid so the user can drop the groove into any DAW.
// Represents accent via higher velocity and slide as a small note-overlap so
// DAWs render it as legato/portamento. One loop of the pattern is written —
// a MIDI file can be looped natively by any host, unlike a WAV bounce.
// ─────────────────────────────────────────────────────────────────────────────
static bool export_mid() {
    std::error_code ec;
    std::filesystem::create_directories(config_dir(), ec);
    if (ec) { set_toast("\xe2\x9c\x97 mid failed"); return false; }

    auto path = config_dir() / "bounce.mid";
    FILE* f = std::fopen(path.string().c_str(), "wb");
    if (!f) { set_toast("\xe2\x9c\x97 mid failed"); return false; }

    // 480 PPQN is the de-facto SMF standard; any DAW reads it without squinting.
    // At 480 ticks per quarter, a 16th = 120 ticks — swing then splits a pair
    // of 16ths as (240 * ratio, 240 * (1-ratio)) matching the audio-thread math.
    constexpr int kPPQN           = 480;
    constexpr int kSixteenthTicks = kPPQN / 4;
    const int plen = g_pattern_length;

    // Absolute tick at the start of each step (index i in [0, plen]).
    std::vector<int> step_tick(static_cast<size_t>(plen + 1), 0);
    for (int i = 0; i < plen; ++i) {
        bool  even      = (i % 2) == 0;
        float dur_16ths = 2.0f * (even ? g_swing : (1.0f - g_swing));
        int   dur       = static_cast<int>(std::round(dur_16ths * kSixteenthTicks));
        step_tick[static_cast<size_t>(i + 1)] = step_tick[static_cast<size_t>(i)] + dur;
    }

    struct Event { int tick; uint8_t status; uint8_t note; uint8_t vel; };
    std::vector<Event> events;
    events.reserve(static_cast<size_t>(plen * 2));

    for (int i = 0; i < plen; ++i) {
        const auto& s = g_steps[static_cast<size_t>(i)];
        if (s.rest) continue;
        // Probability is a live-performance thing. Exporting a file that
        // plays different notes every time you re-open it would be surprising,
        // so prob is ignored here (always emit the notes the user sees).
        uint8_t vel  = s.accent ? 112 : 80;
        int     midi = std::clamp(s.note, 0, 127);
        int     rat  = std::clamp(s.ratchet, 1, 4);
        int     step_on  = step_tick[static_cast<size_t>(i)];
        int     step_end = step_tick[static_cast<size_t>(i + 1)];
        int     step_len = step_end - step_on;

        for (int k = 0; k < rat; ++k) {
            int on_tick = step_on + step_len * k / rat;
            int off_tick;
            const bool last_sub = (k == rat - 1);
            if (last_sub && s.slide && i + 1 < plen
                && !g_steps[static_cast<size_t>(i + 1)].rest) {
                // 8-tick overlap with the next step's first note → legato.
                off_tick = step_end + 8;
            } else if (last_sub) {
                // 4-tick gap at the step boundary.
                off_tick = step_end - 4;
            } else {
                // Mid-step ratchet: gap-then-retrigger so DAWs hear each
                // sub-hit as a distinct note.
                int sub_end = step_on + step_len * (k + 1) / rat;
                off_tick = sub_end - 2;
            }
            if (off_tick <= on_tick) off_tick = on_tick + 1;
            events.push_back({on_tick,  0x90, static_cast<uint8_t>(midi), vel});
            events.push_back({off_tick, 0x80, static_cast<uint8_t>(midi), 0});
        }
    }

    // Stable sort: note-off before note-on at identical tick so back-to-back
    // same-pitch notes don't leave a hanging gate on the receiver.
    std::stable_sort(events.begin(), events.end(),
        [](const Event& a, const Event& b) {
            if (a.tick != b.tick) return a.tick < b.tick;
            return a.status < b.status;
        });

    // ── Encode the track body as a byte vector, then emit the file chunks ──
    std::vector<uint8_t> track;
    auto emit = [&](std::initializer_list<uint8_t> bs) {
        for (auto b : bs) track.push_back(b);
    };
    auto emit_vlq = [&](int v) {
        uint8_t buf[5];
        int n = 0;
        buf[n++] = static_cast<uint8_t>(v & 0x7F);
        v >>= 7;
        while (v > 0) {
            buf[n++] = static_cast<uint8_t>(0x80 | (v & 0x7F));
            v >>= 7;
        }
        for (int i = n - 1; i >= 0; --i) track.push_back(buf[i]);
    };

    // Tempo meta (FF 51 03 <us per quarter, 3 bytes big-endian>).
    uint32_t us_per_qn = static_cast<uint32_t>(60000000.0f / g_bpm);
    emit_vlq(0);
    emit({0xFF, 0x51, 0x03,
          static_cast<uint8_t>((us_per_qn >> 16) & 0xFF),
          static_cast<uint8_t>((us_per_qn >> 8) & 0xFF),
          static_cast<uint8_t>(us_per_qn & 0xFF)});

    // Time signature 4/4 with 24 MIDI clocks per quarter.
    emit_vlq(0);
    emit({0xFF, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08});

    // Track name ("TB-303").
    emit_vlq(0);
    emit({0xFF, 0x03, 0x06,
          'T', 'B', '-', '3', '0', '3'});

    int last_tick = 0;
    for (const auto& e : events) {
        emit_vlq(e.tick - last_tick);
        last_tick = e.tick;
        track.push_back(e.status);
        track.push_back(e.note);
        track.push_back(e.vel);
    }

    // End of track.
    emit_vlq(0);
    emit({0xFF, 0x2F, 0x00});

    // ── File chunks ────────────────────────────────────────────────────────
    auto write_be32 = [&](uint32_t v) {
        uint8_t b[4] = {
            static_cast<uint8_t>((v >> 24) & 0xFF),
            static_cast<uint8_t>((v >> 16) & 0xFF),
            static_cast<uint8_t>((v >> 8) & 0xFF),
            static_cast<uint8_t>(v & 0xFF),
        };
        std::fwrite(b, 1, 4, f);
    };
    auto write_be16 = [&](uint16_t v) {
        uint8_t b[2] = {
            static_cast<uint8_t>((v >> 8) & 0xFF),
            static_cast<uint8_t>(v & 0xFF),
        };
        std::fwrite(b, 1, 2, f);
    };

    std::fwrite("MThd", 1, 4, f);
    write_be32(6);
    write_be16(0);                 // format 0 = single multi-channel track
    write_be16(1);                 // 1 track
    write_be16(kPPQN);

    std::fwrite("MTrk", 1, 4, f);
    write_be32(static_cast<uint32_t>(track.size()));
    std::fwrite(track.data(), 1, track.size(), f);
    std::fclose(f);

    set_toast("\xe2\x9c\x93 mid \xe2\x86\x92 bounce.mid");
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Pattern text export/import — a tweet-sized base64 code representing the
// entire patch (steps, knobs, FX, tempo, swing) so users can paste grooves
// into chat or commit them to a gist. URL-safe alphabet so the code can sit
// bare in URLs / terminals / markdown without escapes.
//
// Byte 0:   version (1 = legacy, 2 = +prob/ratchet, 3 = +p-locks,
//                    4 = +9 drums, 5 = +3 drum voices: SH/TB/CG,
//                    6 = +4 drum voices: MT/CY/RD/BG)
// Byte 1:   pattern_length
// Bytes 2-3: bpm * 10 as LE u16    (e.g. 1225 = 122.5 bpm)
// Byte 4:   swing u8  (0=0.50, 255=0.75)
// Byte 5:   waveform (0/1)
// Byte 6:   tuning u8 (0..255 maps to the knob's 0..1)
// Bytes 7-13: knobs cutoff/res/env/decay/accent/drive/vol u8
// Bytes 14-20: FX od/dmix/dfb/delay_div/rmix/rsize/rdamp (dfdiv is 0-3)
// Bytes 21-52: 16 steps × (note u8, flags u8) where flags = rest|accent<<1|slide<<2
// Bytes 53-84: 16 steps × (prob u8 [0..100], ratchet u8 [1..4])  — v2+
// Bytes 85-164: 16 steps × (mask u8, lockC u8, lockR u8, lockE u8, lockA u8)
//               — v3+ only. Lock values are the same u8-scaled knob mapping.
// Byte 165: drum_master u8  (0..255 maps to 0..1.5 — scaled-clamp on decode) — v4+
// Bytes 166-.. : N × drum_mask u16 LE  (bit i = hit on step i) — v4+
//                v4 codes carry 9 voices, v5 carries 12, v6 carries
//                kDrumVoices (16).
// Total 165 / 184 / 190 / 198 bytes → ~245 / 254 / 264 base64 chars. Older
// readers silently zero the fields they don't understand. Roundtrips via
// bit-exact encode/decode.
// ─────────────────────────────────────────────────────────────────────────────

static std::string base64_encode(const uint8_t* data, size_t n) {
    static constexpr char kAlpha[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string out;
    out.reserve((n + 2) / 3 * 4);
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < n) v |= static_cast<uint32_t>(data[i + 1]) << 8;
        if (i + 2 < n) v |= static_cast<uint32_t>(data[i + 2]);
        out += kAlpha[(v >> 18) & 0x3F];
        out += kAlpha[(v >> 12) & 0x3F];
        if (i + 1 < n) out += kAlpha[(v >> 6) & 0x3F];
        if (i + 2 < n) out += kAlpha[v & 0x3F];
    }
    return out;
}

static bool base64_decode(const std::string& s, std::vector<uint8_t>& out) {
    int8_t rev[256];
    for (int i = 0; i < 256; ++i) rev[i] = -1;
    static constexpr char kAlpha[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    for (int i = 0; i < 64; ++i) rev[static_cast<uint8_t>(kAlpha[i])] = static_cast<int8_t>(i);
    // Also accept standard base64 so users pasting +// variants still work.
    rev[static_cast<uint8_t>('+')] = 62;
    rev[static_cast<uint8_t>('/')] = 63;

    std::string clean;
    clean.reserve(s.size());
    for (char c : s) {
        unsigned uc = static_cast<unsigned char>(c);
        if (uc <= 0x20 || c == '=') continue;     // skip whitespace + padding
        if (rev[uc] < 0) return false;
        clean += c;
    }

    out.clear();
    out.reserve(clean.size() * 3 / 4);
    for (size_t i = 0; i < clean.size(); i += 4) {
        uint32_t v = 0;
        int take = 0;
        for (int k = 0; k < 4 && i + k < clean.size(); ++k) {
            v = (v << 6) | static_cast<uint32_t>(rev[static_cast<uint8_t>(clean[i + k])]);
            take++;
        }
        v <<= (4 - take) * 6;
        if (take >= 2) out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        if (take >= 3) out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        if (take >= 4) out.push_back(static_cast<uint8_t>(v & 0xFF));
    }
    return true;
}

static std::string pattern_encode_string() {
    // 165 bytes of v3 header + 1 byte drum_master + kDrumVoices × 2 drum masks.
    constexpr size_t kN = 165 + 1 + kDrumVoices * 2;
    uint8_t buf[kN] = {};
    auto u8 = [](float v) -> uint8_t {
        int i = static_cast<int>(std::round(std::clamp(v, 0.0f, 1.0f) * 255.0f));
        return static_cast<uint8_t>(std::clamp(i, 0, 255));
    };
    buf[0] = 6;                                                // version
    buf[1] = static_cast<uint8_t>(std::clamp(g_pattern_length, 1, 16));
    int bpm10 = std::clamp(static_cast<int>(std::round(g_bpm * 10.0f)), 200, 3000);
    buf[2] = static_cast<uint8_t>(bpm10 & 0xFF);
    buf[3] = static_cast<uint8_t>((bpm10 >> 8) & 0xFF);
    float sw_n = std::clamp((g_swing - 0.50f) / 0.25f, 0.0f, 1.0f);
    buf[4]  = static_cast<uint8_t>(std::round(sw_n * 255.0f));
    buf[5]  = static_cast<uint8_t>(g_wave & 1);
    buf[6]  = u8(g_tune);
    buf[7]  = u8(g_cutoff);
    buf[8]  = u8(g_resonance);
    buf[9]  = u8(g_env_mod);
    buf[10] = u8(g_decay);
    buf[11] = u8(g_accent);
    buf[12] = u8(g_drive);
    buf[13] = u8(g_volume);
    buf[14] = u8(g_od_amt);
    buf[15] = u8(g_delay_mix);
    buf[16] = u8(g_delay_fb);
    buf[17] = static_cast<uint8_t>(std::clamp(g_delay_div, 0, 3));
    buf[18] = u8(g_rev_mix);
    buf[19] = u8(g_rev_size);
    buf[20] = u8(g_rev_damp);
    for (int i = 0; i < 16; ++i) {
        const auto& s = g_steps[static_cast<size_t>(i)];
        buf[21 + i * 2]     = static_cast<uint8_t>(std::clamp(s.note, 0, 127));
        uint8_t fl = 0;
        if (s.rest)   fl |= 1;
        if (s.accent) fl |= 2;
        if (s.slide)  fl |= 4;
        buf[21 + i * 2 + 1] = fl;
        buf[53 + i * 2]     = static_cast<uint8_t>(std::clamp(s.prob,    0, 100));
        buf[53 + i * 2 + 1] = static_cast<uint8_t>(std::clamp(s.ratchet, 1,   4));
        const size_t L = 85 + i * 5;
        buf[L + 0] = static_cast<uint8_t>(s.lock_mask & 0xF);
        buf[L + 1] = u8(s.lock_cutoff);
        buf[L + 2] = u8(s.lock_res);
        buf[L + 3] = u8(s.lock_env);
        buf[L + 4] = u8(s.lock_accent);
    }
    // v4 drum block. Master is 0..1.5, scale-encode as u8 where 255 = 1.5.
    {
        int dm = static_cast<int>(std::round(
            std::clamp(g_drum_master, 0.0f, 1.5f) / 1.5f * 255.0f));
        buf[165] = static_cast<uint8_t>(std::clamp(dm, 0, 255));
        for (int v = 0; v < kDrumVoices; ++v) {
            uint16_t mask = 0;
            for (int i = 0; i < 16; ++i) {
                if (g_drums[static_cast<size_t>(v)][static_cast<size_t>(i)])
                    mask |= static_cast<uint16_t>(1u << i);
            }
            buf[166 + v * 2 + 0] = static_cast<uint8_t>(mask & 0xFF);
            buf[166 + v * 2 + 1] = static_cast<uint8_t>((mask >> 8) & 0xFF);
        }
    }
    return base64_encode(buf, kN);
}

static bool pattern_decode_string(const std::string& code) {
    std::vector<uint8_t> buf;
    if (!base64_decode(code, buf)) return false;
    if (buf.size() < 53 || buf[0] < 1 || buf[0] > 6) return false;
    const bool v2 = (buf[0] >= 2) && (buf.size() >= 85);
    const bool v3 = (buf[0] >= 3) && (buf.size() >= 165);
    // Drum block: v4 codes carry 9 voices (184 bytes), v5 carries 12 (190
    // bytes), v6 carries kDrumVoices (16 → 198 bytes). We detect the version
    // from byte 0 and read only as many voices as that version promised so
    // short payloads decode cleanly without reaching past the buffer.
    const int  v_drums  = (buf[0] >= 6) ? kDrumVoices
                        : (buf[0] >= 5) ? 12
                        : 9;
    const bool v4 = (buf[0] >= 4) && (buf.size() >= 165 + 1 + static_cast<size_t>(v_drums) * 2);
    auto f8 = [](uint8_t v) -> float { return static_cast<float>(v) / 255.0f; };

    g_pattern_length = std::clamp(static_cast<int>(buf[1]), 1, 16);
    uint16_t bpm10 = static_cast<uint16_t>(buf[2])
                   | (static_cast<uint16_t>(buf[3]) << 8);
    g_bpm       = std::clamp(static_cast<float>(bpm10) / 10.0f, 40.0f, 300.0f);
    g_swing     = std::clamp(0.50f + f8(buf[4]) * 0.25f, 0.50f, 0.75f);
    g_wave      = buf[5] & 1;
    g_tune      = f8(buf[6]);
    g_cutoff    = f8(buf[7]);
    g_resonance = f8(buf[8]);
    g_env_mod   = f8(buf[9]);
    g_decay     = f8(buf[10]);
    g_accent    = f8(buf[11]);
    g_drive     = f8(buf[12]);
    g_volume    = f8(buf[13]);
    g_od_amt    = f8(buf[14]);
    g_delay_mix = f8(buf[15]);
    g_delay_fb  = f8(buf[16]);
    g_delay_div = std::clamp(static_cast<int>(buf[17]), 0, 3);
    g_rev_mix   = f8(buf[18]);
    g_rev_size  = f8(buf[19]);
    g_rev_damp  = f8(buf[20]);
    for (int i = 0; i < 16; ++i) {
        auto& s = g_steps[static_cast<size_t>(i)];
        s.note   = std::clamp(static_cast<int>(buf[21 + i * 2]), 12, 96);
        uint8_t fl = buf[21 + i * 2 + 1];
        s.rest   = (fl & 1) != 0;
        s.accent = (fl & 2) != 0;
        s.slide  = (fl & 4) != 0;
        if (v2) {
            s.prob    = std::clamp(static_cast<int>(buf[53 + i * 2]),     0, 100);
            s.ratchet = std::clamp(static_cast<int>(buf[53 + i * 2 + 1]), 1,   4);
        } else {
            s.prob    = 100;
            s.ratchet = 1;
        }
        if (v3) {
            const size_t L = 85 + i * 5;
            s.lock_mask   = static_cast<uint8_t>(buf[L + 0] & 0xF);
            s.lock_cutoff = f8(buf[L + 1]);
            s.lock_res    = f8(buf[L + 2]);
            s.lock_env    = f8(buf[L + 3]);
            s.lock_accent = f8(buf[L + 4]);
        } else {
            s.lock_mask   = 0;
            s.lock_cutoff = s.lock_res = s.lock_env = s.lock_accent = 0.0f;
        }
    }
    // Drum block. Always wipe the kit first so older codes (v1-v3, no drums)
    // land in a cleared grid rather than leaking the previous pattern's hits.
    // v4 payloads carry 9 voices; v5 adds SH/TB/CG; v6 adds MT/CY/RD/BG.
    // Unknown (newer) voices stay silent when loading older codes.
    for (auto& row : g_drums) for (auto& c : row) c = false;
    if (v4) {
        g_drum_master = std::clamp(
            static_cast<float>(buf[165]) / 255.0f * 1.5f, 0.0f, 1.5f);
        for (int v = 0; v < v_drums; ++v) {
            uint16_t m = static_cast<uint16_t>(buf[166 + v * 2 + 0])
                       | (static_cast<uint16_t>(buf[166 + v * 2 + 1]) << 8);
            for (int i = 0; i < 16; ++i) {
                g_drums[static_cast<size_t>(v)][static_cast<size_t>(i)]
                    = (m & (1u << i)) != 0;
            }
        }
    } else {
        // v1-v3 codes carry no drums; leave drum_master at its current value
        // (the user's knob state) rather than forcing it to 1.0.
    }
    g_preset_index = -1;
    g_step_sel     = 0;
    return true;
}

static bool export_pattern_text() {
    std::error_code ec;
    std::filesystem::create_directories(config_dir(), ec);
    if (ec) { set_toast("\xe2\x9c\x97 text failed"); return false; }

    auto path = config_dir() / "pattern.txt";
    std::ofstream f(path);
    if (!f) { set_toast("\xe2\x9c\x97 text failed"); return false; }
    f << pattern_encode_string() << '\n';
    f.close();

    set_toast("\xe2\x9c\x93 text \xe2\x86\x92 pattern.txt");
    return true;
}

static bool import_pattern_text() {
    auto path = config_dir() / "pattern.txt";
    std::ifstream f(path);
    if (!f) { set_toast("\xe2\x9c\x97 no pattern.txt"); return false; }

    std::string code, line;
    while (std::getline(f, line)) code += line;
    if (!pattern_decode_string(code)) {
        set_toast("\xe2\x9c\x97 bad pattern code");
        return false;
    }
    set_toast("\xe2\x86\xba text \xe2\x86\x90 pattern.txt");
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Audio plumbing
// ─────────────────────────────────────────────────────────────────────────────

static float midi_to_hz(int note) {
    return 440.0f * std::pow(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f);
}

// Push current knob + sequencer state into the audio engine every frame.
// Cheap (atomics), and keeps everything in perfect sync with the UI with no
// special plumbing. The sequencer itself lives on the audio thread now —
// this just keeps it fed with up-to-date pattern/transport data.
static void push_params() {
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
    acid_seq_set_bpm(g_bpm);
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
    }
    acid_seq_set_drum_master(g_drums_mute ? 0.0f : g_drum_master);
}

static void tick(float dt) {
    push_params();

    // Upgrade maya's default mouse tracking (1000 + 1002 = press/release/drag)
    // to any-event motion (1003) so bare hover deltas are reported. maya's
    // RunConfig only enables click+drag; we opt into full motion tracking once
    // the runtime has finished its own terminal init by writing the DECSET
    // sequence to stdout on the first tick. The teardown ? 1003l runs on exit.
    static bool motion_on = false;
    if (!motion_on) {
        std::fputs("\x1b[?1003h", stdout);
        std::fflush(stdout);
        motion_on = true;
    }

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
    // auto-advance to the next saved slot. Detecting the wrap here avoids any
    // audio-thread coupling — the UI tick already sees the same positions.
    if (g_song_mode && g_playing
        && prev_step >= 0 && g_current_step >= 0
        && g_current_step < prev_step)
    {
        int next = song_next_slot(g_song_slot > 0 ? g_song_slot : 0);
        if (next > 0) {
            if (next != g_song_slot) {
                load_pattern_slot(next, /*quiet=*/true);
                char buf[32];
                std::snprintf(buf, sizeof(buf),
                              "\xe2\x99\xaa song \xe2\x86\x92 slot %d", next);
                set_toast(buf);
            }
            g_song_slot = next;
        }
    }
}

static void start_playback() {
    g_playing = true;
    g_current_step = -1;
    acid_seq_play();
}

static void stop_playback() {
    g_playing = false;
    g_current_step = -1;
    acid_seq_stop();
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

// FX knob adjustment. FX_DTIME is a 4-way enum (1/16..1/8d) so ↑/↓ step
// through the divisions rather than adjusting a continuous value.
static void adjust_fx(int idx, float delta) {
    if (idx == FX_DTIME) {
        int d = g_delay_div + (delta > 0 ? +1 : -1);
        g_delay_div = std::clamp(d, 0, 3);
        return;
    }
    float* v = g_fx_table[idx].value;
    *v = std::clamp(*v + delta, 0.0f, 1.0f);
}

static void reset_fx(int idx) {
    if (idx == FX_DTIME) { g_delay_div = 2; return; }
    *g_fx_table[idx].value = g_fx_table[idx].default_v;
}

// cycle focus forward (or backward)
static void cycle_focus(int dir) {
    // Section count follows Section enum — Knobs, FX, Sequencer, Drums, Transport.
    int n = 5;
    int cur = static_cast<int>(g_focus);
    cur = ((cur + dir) % n + n) % n;
    g_focus = static_cast<Section>(cur);
}

// Jam-mode piano layout — classic tracker / FL "piano keys" mapping across
// the lower two QWERTY rows. Lower-case letters only; Shift is reserved for
// section cycling. Returns semitones above the jam base octave, or -1 if the
// key isn't part of the piano.
static int jam_key_semi(char c) {
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
static std::string midi_note_name(int midi) {
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

// ─────────────────────────────────────────────────────────────────────────────
// Mouse hit-testing & event handling
// ─────────────────────────────────────────────────────────────────────────────
// Layout is fixed-row at top and bottom, flex in the middle. Known heights:
//
//   row 0              title bar
//   rows 1..12         voice_row = SYNTH VOICE (grow 8) | FX RACK (grow 6)
//                       12 rows = 1 border + 1 header + 8 knobs + 1 caption + 1 border
//   rows 13..H-15      middle_row (filter+scope on left, transport on right)
//   rows H-14..H-2     step_row = SEQUENCER (grow 1) | DRUMS (grow 1)
//                       13 rows = max(seq panel 10, drum panel 13) via align Stretch
//                       — drum panel wants 1 border + 1 header + 9 voices +
//                       1 bus caption + 1 border = 13, seq stretches to match.
//   row H-1            help bar
//
// Content widths (what justify-center lines up inside each panel):
//   synth strip  : 9 knobs × 8 + 8 gaps = 80
//   fx strip     : 7 knobs × 8 + 6 gaps = 62
//   sequencer    : 7-col label + 16 × 4-col cells + 1 trailing wall = 72
//   drums        : 4-col label + 16 × 3-col glyphs = 52
//
// Panel widths are reconstructed by mirroring yoga's integer-safe flex
// distribution (floor shares + hand remainder to the first grow-able item).
// This matches yoga's output cell-for-cell instead of eyeballing 50/50 splits.

struct HitZone {
    enum Kind {
        None,
        Title,
        Knob,
        FxKnob,
        Step,
        DrumCell,
        TransportArea,
        ScopeArea,
        HelpBar,
        HelpOverlay,
    };
    Kind kind = None;
    int  idx  = 0;      // knob idx / fx idx / step / drum step
    int  idx2 = 0;      // drum voice (0..4)
};

// Yoga's integer-safe flex split: items get floor(avail * grow/total) each,
// and the remainder (avail - sum(floors)) is handed to grow-able items in
// declaration order. For a 2-child split this is simply: last = floor(second);
// first = avail - last. Using this formula keeps our hit-test grid aligned
// with what yoga actually renders.
static inline int flex_first_share(int avail, int grow_first, int grow_total) {
    int second = (avail * (grow_total - grow_first)) / grow_total;
    return avail - second;
}

static HitZone hit_test(int col, int row) {
    // SGR mouse coordinates are 1-indexed per protocol; our geometry here is
    // 0-indexed (matches maya's internal render positions). Shift once at the
    // boundary so the math below lines up with what the user actually sees.
    col -= 1;
    row -= 1;
    if (g_term_w <= 0 || g_term_h <= 0) return {};
    if (row < 0 || row >= g_term_h || col < 0 || col >= g_term_w) return {};

    if (row == 0) return {HitZone::Title, 0, 0};
    if (row == g_term_h - 1) return {HitZone::HelpBar, 0, 0};

    // Help overlay covers the middle region entirely — report a single zone so
    // the wheel can scroll the modal, even though individual lines aren't
    // interactive.
    if (g_help_open) return {HitZone::HelpOverlay, 0, 0};

    // ── Voice row (rows 1..12): synth 8/14, fx 6/14, gap 1 ────────────────
    if (row >= 1 && row <= 12) {
        int avail    = std::max(0, g_term_w - 1);
        int synth_w  = flex_first_share(avail, 8, 14);
        int fx_x     = synth_w + 1;
        int fx_w     = std::max(0, g_term_w - fx_x);

        // Inside-panel tests only apply in the content band (rows 2..11);
        // rows 1 and 12 are borders. We're lenient about group-header /
        // caption rows — they fall to the nearest knob column so the user
        // doesn't have to thread their click between ruler strips.
        bool in_body = (row >= 2 && row <= 11);

        if (col < synth_w) {
            if (!in_body) return {};
            int interior_x = 1 + 2;                 // border + left pad
            int interior_w = std::max(0, synth_w - 6);
            constexpr int kStripW = 9 * 8 + 8;      // 80
            // justify(Center) clamps free_main to 0 when content > container,
            // so the strip sits at interior_x when the panel is too narrow to
            // center. Mirror that clamp here.
            int strip_start = interior_x + std::max(0, (interior_w - kStripW) / 2);
            int rel = col - strip_start;
            if (rel < 0) return {};
            // Each knob lives in a 9-col slot (8 glyph + 1 gap). The trailing
            // knob has no gap, so the last valid `rel` is kStripW-1. Map gap
            // columns to the knob on their left so clicks between columns
            // don't bounce off.
            int idx = std::min(rel / 9, K_COUNT - 1);
            if (rel >= kStripW) return {};
            if (idx < 0 || idx >= K_COUNT) return {};
            return {HitZone::Knob, idx, 0};
        }
        if (col >= fx_x) {
            if (!in_body) return {};
            int interior_x = fx_x + 1 + 2;
            int interior_w = std::max(0, fx_w - 6);
            constexpr int kFxStripW = FX_COUNT * 8 + (FX_COUNT - 1); // 62
            int strip_start = interior_x + std::max(0, (interior_w - kFxStripW) / 2);
            int rel = col - strip_start;
            if (rel < 0 || rel >= kFxStripW) return {};
            int idx = std::min(rel / 9, FX_COUNT - 1);
            if (idx < 0 || idx >= FX_COUNT) return {};
            return {HitZone::FxKnob, idx, 0};
        }
        return {};
    }

    // ── Step row (bottom): sequencer fills the full width, 10 rows tall. Data
    // rows (STEP/PITCH/FLAG/PROB/LOCK/NOTE) live at step_top + 2 .. step_top + 7.
    int step_top = g_term_h - 11;
    int step_bot = g_term_h - 2;
    if (row >= step_top && row <= step_bot) {
        int avail     = std::max(0, g_term_w - 1);
        int interior_x = 1 + 1;                                  // border + pad
        int interior_w = std::max(0, avail - 3);
        constexpr int kSeqW = 7 + 16 * 4 + 1;                     // 72
        int seq_start = interior_x + std::max(0, (interior_w - kSeqW) / 2);
        int rel = col - seq_start;
        if (rel < 8 || rel >= kSeqW) return {};
        int cell_rel = rel - 8;
        int step = cell_rel / 4;
        if (step < 0 || step >= 16) return {};
        if (row < step_top + 2 || row > step_top + 7) return {};
        return {HitZone::Step, step, 0};
    }

    // ── Middle row: left_col (scope + filter) | transport | drums. Split
    // via 3:2:2 flex shares, matching the dsl hstack that builds this row.
    // left_col (3) carries the filter chart so it reads at full width;
    // transport (2) and drums (2) stay equal on the right side.
    int mid_top = 13;
    int mid_bot = step_top - 1;
    if (row >= mid_top && row <= mid_bot) {
        // 3 panels + 2 gaps fill g_term_w exactly, so the panels share
        // (g_term_w - 2) columns. NB: the voice_row hit_test uses
        // `g_term_w - 1` because it has only one gap; we need to subtract
        // one cell per gap, hence -2 here.
        int flex = std::max(0, g_term_w - 2);
        int left_w      = flex_first_share(flex, 3, 7);
        int tail        = flex - left_w;
        int transport_w = flex_first_share(tail, 2, 4);
        int drum_w      = tail - transport_w;
        int transport_x = left_w + 1;
        int drum_x      = transport_x + transport_w + 1;

        if (col >= drum_x && col < drum_x + drum_w) {
            // Drum panel: border(1) + header(1) + kDrumVoices voice rows +
            // bus(1) + border(1). Voice rows live at mid_top + 2 .. + 2+N-1.
            int interior_x = drum_x + 1 + 1;                     // border + pad
            int interior_w = std::max(0, drum_w - 4);
            constexpr int kDrumW = 4 + 16 * 3;                    // 52
            int drum_start = interior_x + std::max(0, (interior_w - kDrumW) / 2);
            int rel = col - drum_start;
            if (rel < 4 || rel >= kDrumW) return {};
            int cell_rel = rel - 4;
            int step = cell_rel / 3;
            if (step < 0 || step >= 16) return {};
            int voice = row - (mid_top + 2);
            if (voice < 0 || voice >= kDrumVoices) return {};
            return {HitZone::DrumCell, step, voice};
        }
        if (col >= transport_x) return {HitZone::TransportArea, 0, 0};
        return {HitZone::ScopeArea, 0, 0};
    }

    return {};
}

static void handle_mouse(const MouseEvent& m) {
    int col = m.x.raw();
    int row = m.y.raw();

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
        if (g_help_open) return;
        if (g_term_w <= 0 || g_term_h <= 0) return;
        int hc = col - 1;       // SGR → 0-indexed
        int hr = row - 1;
        if (hr <= 0 || hr >= g_term_h - 1) return;   // title / help bar rows

        // Voice row: synth panel (8/14) vs FX panel (6/14).
        if (hr >= 1 && hr <= 12) {
            int avail   = std::max(0, g_term_w - 1);
            int synth_w = flex_first_share(avail, 8, 14);
            g_focus = (hc < synth_w) ? Section::Knobs : Section::FX;
            return;
        }
        // Step row: sequencer spans full width at the bottom.
        int step_top = g_term_h - 11;
        int step_bot = g_term_h - 2;
        if (hr >= step_top && hr <= step_bot) {
            g_focus = Section::Sequencer;
            return;
        }
        // Middle band: left_col | transport | drums, 3:2:2 flex split matching
        // the dsl::hstack that builds this row (gap 1 between cells).
        // Uses g_term_w - 2 (one cell per gap) — same correction as hit_test.
        int mid_top = 13;
        int mid_bot = step_top - 1;
        if (hr >= mid_top && hr <= mid_bot) {
            int flex        = std::max(0, g_term_w - 2);
            int left_w      = flex_first_share(flex, 3, 7);
            int tail        = flex - left_w;
            int transport_w = flex_first_share(tail, 2, 4);
            int transport_x = left_w + 1;
            int drum_x      = transport_x + transport_w + 1;
            // Left third has no interactive widgets — leave focus where it
            // was rather than hijacking it to the sequencer (which lives in
            // a totally different row).
            if (hc >= drum_x)            g_focus = Section::Drums;
            else if (hc >= transport_x)  g_focus = Section::Transport;
            return;
        }
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
                g_drum_step_sel  = z.idx;
                g_drum_voice_sel = z.idx2;
                g_drum_master = std::clamp(g_drum_master + dir * 0.05f, 0.0f, 1.5f);
                return;
            case HitZone::TransportArea:
                g_focus = Section::Transport;
                // Scroll-down feels like "next preset" — invert dir so wheel-down
                // advances the list even though our logical "dir" is +1 for up.
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
                g_drum_step_sel  = z.idx;
                g_drum_voice_sel = z.idx2;
                auto& cell = g_drums[static_cast<size_t>(z.idx2)]
                                    [static_cast<size_t>(z.idx)];
                cell = !cell;
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

    // ── Right click ────────────────────────────────────────────────────────
    if (m.kind == MouseEventKind::Press && m.button == MouseButton::Right) {
        switch (z.kind) {
            case HitZone::Knob:
                g_focus = Section::Knobs;
                g_knob_sel = z.idx;
                reset_knob(z.idx);
                return;
            case HitZone::FxKnob:
                g_focus = Section::FX;
                g_fx_sel = z.idx;
                reset_fx(z.idx);
                return;
            case HitZone::Step: {
                g_focus = Section::Sequencer;
                g_step_sel = z.idx;
                auto& s = g_steps[static_cast<size_t>(z.idx)];
                s.rest = !s.rest;
                return;
            }
            case HitZone::DrumCell: {
                // Right-click clears the cell (matches the right-click-to-rest
                // pattern on the sequencer).
                g_focus = Section::Drums;
                g_drum_step_sel  = z.idx;
                g_drum_voice_sel = z.idx2;
                g_drums[static_cast<size_t>(z.idx2)]
                       [static_cast<size_t>(z.idx)] = false;
                return;
            }
            default: return;
        }
    }
}

static void handle_event(const Event& ev) {
    // Resize — keep terminal size in sync for mouse hit-tests.
    int rw = 0, rh = 0;
    if (resized(ev, &rw, &rh)) { g_term_w = rw; g_term_h = rh; return; }
    if (const MouseEvent* m = as_mouse(ev)) { handle_mouse(*m); return; }

    // ── Global keys ──────────────────────────────────────────────────────────
    if (key(ev, 'q') || key(ev, 'Q') || key(ev, SpecialKey::Escape)) {
        if (g_help_open) { g_help_open = false; g_help_ov_scroll = 0; return; }
        // Escape also exits jam mode without quitting — otherwise the user
        // can't get out except by typing `k` (which would trigger a note).
        if (g_jam_mode) {
            g_jam_mode = false;
            acid_note_off();
            g_jam_gate_s = 0.0f;
            g_jam_last_midi = -1;
            set_toast("jam off");
            return;
        }
        maya::quit();
        return;
    }
    if (key(ev, '?')) { g_help_open = !g_help_open; g_help_ov_scroll = 0; return; }
    // While help is open we only accept Esc/? to close — everything else is no-op.
    if (g_help_open) return;

    // ── Jam mode ────────────────────────────────────────────────────────────
    // When active, swallow almost every keystroke and route letters through
    // jam_key_semi. Tab / `k` / Esc still do their usual things so the user
    // always has a way out. Space still plays/pauses the pattern so jam and
    // sequencer can layer (sequencer rides through jam; pattern runs in the
    // background and jam adds a lead on top).
    if (g_jam_mode) {
        if (key(ev, 'k')) {
            g_jam_mode = false;
            acid_note_off();
            g_jam_gate_s = 0.0f;
            g_jam_last_midi = -1;
            set_toast("jam off");
            return;
        }
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
    if (key(ev, 'r')) {
        switch (g_focus) {
            case Section::Knobs:     randomize_knobs();     break;
            case Section::FX:        randomize_fx();        break;
            case Section::Sequencer: randomize_pattern();   break;
            case Section::Drums:     randomize_drums();     break;
            case Section::Transport: randomize_transport(); break;
        }
        return;
    }
    if (key(ev, 'R')) { randomize_everything(); return; }
    if (key(ev, 'M')) { mutate_pattern(); return; }
    if (key(ev, 'T')) {
        tb303::cycle_theme();
        char buf[32];
        std::snprintf(buf, sizeof(buf), "\xe2\x97\x86 theme: %s",
                      tb303::active_theme().name);
        set_toast(buf);
        return;
    }
    if (key(ev, 'e')) { export_wav(); return; }
    if (key(ev, 'E')) { export_mid(); return; }
    if (key(ev, 'W')) { toggle_live_record(); return; }
    if (key(ev, 'p')) { export_pattern_text(); return; }
    if (key(ev, 'P')) { import_pattern_text(); return; }
    // Jam mode toggle — take over the keyboard for live two-octave playing.
    // Pattern playback keeps running under jam, so the user can lay a lead
    // line over an already-running sequence.
    if (key(ev, 'k')) {
        g_jam_mode = true;
        g_jam_gate_s = 0.0f;
        g_jam_last_midi = -1;
        char buf[48];
        std::snprintf(buf, sizeof(buf),
            "\xe2\x99\xac jam mode  \xe2\x86\x90\xe2\x86\x92 octave  k/Esc exit");
        set_toast(buf);
        return;
    }
    // MIDI output toggle — emit notes + drum hits + clock to the ALSA seq
    // port. On the first enable, tell the user how to hook it up; aconnect -lo
    // shows the port and a DAW/softsynth can subscribe.
    if (key(ev, 'O')) {
        g_midi_out = !g_midi_out;
        acid_midi_set_out_enabled(g_midi_out ? 1 : 0);
        set_toast(g_midi_out
            ? "\xe2\x99\xaa MIDI out on  (connect 'acidflow:out' via aconnect)"
            : "MIDI out off");
        return;
    }
    // MIDI clock input sync — follow an external master's tempo + transport.
    // Incoming notes still trigger the bass voice regardless; this switch
    // controls only the tempo-slaving path.
    if (key(ev, 'I')) {
        g_midi_sync = !g_midi_sync;
        acid_midi_set_sync_in(g_midi_sync ? 1 : 0);
        set_toast(g_midi_sync
            ? "\xe2\x99\xaa MIDI sync on  (waiting for clock/start)"
            : "MIDI sync off");
        return;
    }
    // Song mode chain: step through saved slots 1..9 on each pattern wrap.
    // Toggling ON boots straight into the first saved slot so the user hears
    // immediate feedback; toggling OFF leaves the current pattern loaded.
    if (key(ev, 'n')) {
        if (g_song_mode) {
            g_song_mode = false;
            set_toast("song mode off");
        } else {
            int first = song_next_slot(0);
            if (first == 0) {
                set_toast("\xe2\x9c\x97 no saved slots");
            } else {
                g_song_mode = true;
                g_song_slot = first;
                load_pattern_slot(first, /*quiet=*/true);
                char buf[32];
                std::snprintf(buf, sizeof(buf),
                              "\xe2\x99\xaa song mode \xe2\x86\x92 slot %d", first);
                set_toast(buf);
            }
        }
        return;
    }
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
                    load_pattern_slot(slot);
                    // Keep song mode in sync: manual jumps should advance
                    // from the slot the user just picked, not from wherever
                    // the chain was previously.
                    if (g_song_mode) g_song_slot = slot;
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
            if (key(ev, '0'))               { reset_knob(g_knob_sel); return; }
            if (key(ev, 'w'))               { g_wave = 1 - g_wave; return; }
            if (key(ev, 'm')) {
                g_synth_mute = !g_synth_mute;
                set_toast(g_synth_mute ? "synth muted" : "synth live");
                return;
            }
            break;
        }
        case Section::FX: {
            if (key(ev, SpecialKey::Left))  { g_fx_sel = (g_fx_sel - 1 + FX_COUNT) % FX_COUNT; return; }
            if (key(ev, SpecialKey::Right)) { g_fx_sel = (g_fx_sel + 1) % FX_COUNT; return; }
            if (key(ev, SpecialKey::Up))    { adjust_fx(g_fx_sel, +0.05f); return; }
            if (key(ev, SpecialKey::Down))  { adjust_fx(g_fx_sel, -0.05f); return; }
            if (key(ev, ']'))               { adjust_fx(g_fx_sel, +0.10f); return; }
            if (key(ev, '['))               { adjust_fx(g_fx_sel, -0.10f); return; }
            if (key(ev, '0'))               { reset_fx(g_fx_sel); return; }
            if (key(ev, 'm')) {
                g_synth_mute = !g_synth_mute;
                set_toast(g_synth_mute ? "synth muted" : "synth live");
                return;
            }
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
            // `m` for mute (rest toggle) — `r` is the global randomize key
            // now, so per-step rest moves here. Mnemonic: muted step = rest.
            if (key(ev, 'm')) { s.rest   = !s.rest; return; }
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
            if (key(ev, 'x')) { s = StepData{}; s.rest = true; return; }
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
            // `c` clears the focused row; `C` clears the entire kit. Mirrors
            // the sequencer's `x = clear step` ergonomics at the row level.
            if (key(ev, 'c')) {
                for (auto& c : g_drums[static_cast<size_t>(g_drum_voice_sel)]) c = false;
                return;
            }
            if (key(ev, 'C')) {
                for (auto& row : g_drums) for (auto& c : row) c = false;
                set_toast("drums cleared");
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
            if (key(ev, SpecialKey::Up))   { load_preset(g_preset_index - 1); return; }
            if (key(ev, SpecialKey::Down)) { load_preset(g_preset_index + 1); return; }
            if (key(ev, '[')) { g_bpm = std::max(40.0f,  g_bpm - 2.0f); return; }
            if (key(ev, ']')) { g_bpm = std::min(220.0f, g_bpm + 2.0f); return; }
            if (key(ev, '{')) { g_pattern_length = std::max(4, g_pattern_length - 1); return; }
            if (key(ev, '}')) { g_pattern_length = std::min(16, g_pattern_length + 1); return; }
            // Swing: 0.50 straight → 0.75 hard shuffle. Step is 2% so three
            // taps from centre lands on the canonical MPC-56/58/60/62 feels.
            if (key(ev, '-')) { g_swing = std::max(0.50f, g_swing - 0.02f); return; }
            if (key(ev, '=')) { g_swing = std::min(0.75f, g_swing + 0.02f); return; }
            if (key(ev, ',')) { load_preset(g_preset_index - 1); return; }
            if (key(ev, '.')) { load_preset(g_preset_index + 1); return; }
            if (key(ev, 'm')) {
                // Master mute: if either bus is live, silence both; otherwise unmute both.
                bool any_live = !g_synth_mute || !g_drums_mute;
                g_synth_mute = any_live;
                g_drums_mute = any_live;
                set_toast(any_live ? "all muted" : "all live");
                return;
            }
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
            case K_DRIVE:  return Color::rgb(120, 210, 140);
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
                  K_ENV,  K_DECAY, K_ACCENT, K_DRIVE, K_VOL}) {
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
    //   OSC = TUNE(8)   + gap + WAVE(8)                       = 17
    //   VCF = CUTOFF(8) + gap + RES(8)    + gap + ENV(8)      = 26
    //   EG  = DECAY(8)                                        =  8
    //   VCA = ACCENT(8) + gap + DRIVE(8)  + gap + VOL(8)      = 26
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
        build_rail("VCA", 26, group_color_for(K_VOL))
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

        // Wrap the NoWrap caption in a min-width:0 / overflow:hidden box so
        // its intrinsic text length can't inflate the panel's min-content
        // width. Without this, switching to a long caption ("output stage
        // saturation [clean → pedal-grit]") would grow the whole SYNTH VOICE
        // panel and squeeze the FX RACK on the same row.
        auto text_el = Element{maya::TextElement{
            .content = std::move(txt),
            .style   = {},
            .wrap    = maya::TextWrap::NoWrap,
            .runs    = std::move(runs),
        }};
        return dsl::hstack()
            .min_width(Dimension::fixed(0))
            .overflow(Overflow::Hidden)(std::move(text_el));
    };

    // basis(0) + min_width(0) pins the panel's width to its flex-grow share
    // alone. Without these, yoga would use the children's intrinsic widths
    // (caption text length, strip width, etc.) as the panel's minimum content
    // size — so a long focused-knob caption would inflate the SYNTH panel and
    // squeeze the FX panel on the same row. Overflow::Hidden clips any content
    // wider than the allotted slot cleanly.
    const char* synth_title = g_synth_mute
        ? " \xe2\x97\x87 SYNTH VOICE \xc2\xb7 MUTED (m) \xe2\x97\x87 "
        : " \xe2\x97\x87 SYNTH VOICE \xc2\xb7 VCO\xe2\x86\x92VCF\xe2\x86\x92VCA"
          " \xc2\xb7 24 dB/oct LADDER \xe2\x97\x87 ";
    return dsl::vstack()
        .border(BorderStyle::Round)
        .border_color(g_synth_mute ? tb303::clr_dim()
                      : g_focus == Section::Knobs ? tb303::clr_accent() : tb303::clr_panel_hi())
        .border_text(synth_title, BorderTextPos::Top)
        .align_self(Align::Stretch)
        .basis(Dimension::fixed(0))
        .min_width(Dimension::fixed(0))
        .overflow(Overflow::Hidden)
        .padding(0, 2, 0, 2)(
            std::move(group_header),
            std::move(strip),
            build_caption()
        );
}

// Compact FX rack panel — one bordered row below the SYNTH VOICE panel.
// Keeps the same group-colour discipline as the voice strip so related FX
// read together visually. Three groups in signal-flow order:
//   OD (pre-filter, amber) → DELAY (warm orange) → REVERB (cool teal)
static Element build_fx_row() {
    auto group_color_for = [](int i) -> maya::Color {
        using maya::Color;
        switch (i) {
            case FX_OD:    return Color::rgb(230, 180,  90);  // OD       amber
            case FX_DMIX:
            case FX_DFB:
            case FX_DTIME: return Color::rgb(220, 150,  80);  // DELAY    orange
            case FX_RMIX:
            case FX_RSIZE:
            case FX_RDAMP: return Color::rgb( 90, 190, 200);  // REVERB   teal
        }
        return tb303::clr_accent();
    };

    auto build_one = [&](int i) -> Element {
        bool focused = (g_focus == Section::FX) && (g_fx_sel == i);
        maya::Color gc = group_color_for(i);
        tb303::Knob k;
        k.label         = g_fx_table[i].label;
        k.tech_symbol   = g_fx_table[i].tech;
        k.focused       = focused;
        k.group_col     = gc;
        k.value_text    = format_fx(i);
        if (i == FX_DTIME) {
            // Show selected division as a quantised meter height so the
            // knob visually steps through the four positions.
            k.value         = static_cast<float>(g_delay_div) / 3.0f;
            k.default_value = 2.0f / 3.0f;
            k.modified      = (g_delay_div != 2);
        } else {
            k.value         = *g_fx_table[i].value;
            k.default_value = g_fx_table[i].default_v;
            k.modified      = std::abs(*g_fx_table[i].value - g_fx_table[i].default_v) > 0.005f;
        }
        return k.build();
    };

    std::vector<Element> cols;
    cols.reserve(FX_COUNT);
    for (int i = 0; i < FX_COUNT; ++i) cols.push_back(build_one(i));

    auto strip = (dsl::h(std::move(cols)) | dsl::gap(1)
                  | dsl::justify(Justify::Center)).build();

    // Caption row matching the SYNTH panel's style so both read together.
    auto build_caption = [&]() -> Element {
        std::string txt;
        std::vector<StyledRun> runs;
        bool ffoc = (g_focus == Section::FX);

        if (ffoc) {
            size_t off = txt.size();
            txt += "\xe2\x96\xb6 ";
            runs.push_back(StyledRun{off, 3,
                maya::Style{}.with_fg(tb303::clr_accent()).with_bold()});

            size_t lbl_off = txt.size();
            txt += g_fx_table[g_fx_sel].label;
            runs.push_back(StyledRun{lbl_off, std::string_view(g_fx_table[g_fx_sel].label).size(),
                maya::Style{}.with_fg(tb303::clr_accent()).with_bold()});

            txt += "  ";
            size_t cap_off = txt.size();
            txt += g_fx_table[g_fx_sel].caption;
            runs.push_back(StyledRun{cap_off, std::string_view(g_fx_table[g_fx_sel].caption).size(),
                maya::Style{}.with_fg(tb303::clr_text())});
        } else {
            size_t off = txt.size();
            txt += "\xe2\x97\xa6 ";
            runs.push_back(StyledRun{off, 3,
                maya::Style{}.with_fg(tb303::clr_muted())});
            size_t msg_off = txt.size();
            txt += "Tab here for the FX rack  \xc2\xb7  \xe2\x86\x90\xe2\x86\x92 select, \xe2\x86\x91\xe2\x86\x93 turn";
            runs.push_back(StyledRun{msg_off, txt.size() - msg_off,
                maya::Style{}.with_fg(tb303::clr_muted())});
        }

        auto text_el = Element{maya::TextElement{
            .content = std::move(txt),
            .style   = {},
            .wrap    = maya::TextWrap::NoWrap,
            .runs    = std::move(runs),
        }};
        // Same width-pin trick as the SYNTH caption — keeps the FX panel's
        // width independent of which caption happens to be showing.
        return dsl::hstack()
            .min_width(Dimension::fixed(0))
            .overflow(Overflow::Hidden)(std::move(text_el));
    };

    // Group-header rail, mirroring the SYNTH panel. Three groups in signal-
    // flow order: OD (1 col) → DELAY (3 cols) → REVERB (3 cols). Widths
    // match the knob cells beneath them.
    auto build_rail = [](const char* label, int width, maya::Color c) -> Element {
        int label_cols   = static_cast<int>(std::string_view(label).size());
        int dashes_total = std::max(0, width - 2 - label_cols);
        int left         = dashes_total / 2;
        int right        = dashes_total - left;
        std::string txt;
        std::vector<StyledRun> runs;
        size_t l_off = txt.size();
        for (int i = 0; i < left; ++i) txt += "\xe2\x94\x80";
        txt += " ";
        runs.push_back(StyledRun{l_off, txt.size() - l_off,
            maya::Style{}.with_fg(c)});
        size_t lbl_off = txt.size();
        txt += label;
        runs.push_back(StyledRun{lbl_off, static_cast<size_t>(label_cols),
            maya::Style{}.with_fg(c).with_bold()});
        size_t r_off = txt.size();
        txt += " ";
        for (int i = 0; i < right; ++i) txt += "\xe2\x94\x80";
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
        build_rail("OD",    8, group_color_for(FX_OD)),
        build_rail("DELAY", 26, group_color_for(FX_DMIX)),
        build_rail("REVERB",26, group_color_for(FX_RMIX))
    ) | dsl::gap(1) | dsl::justify(Justify::Center)).build();

    // Same basis/min_width pin as the SYNTH panel — keeps this panel's width
    // strictly at its flex-grow share (6/14) regardless of caption length.
    const char* fx_title = g_synth_mute
        ? " \xe2\x97\x87 FX RACK \xc2\xb7 MUTED (m) \xe2\x97\x87 "
        : " \xe2\x97\x87 FX RACK \xc2\xb7 OD\xe2\x86\x92" "DELAY\xe2\x86\x92" "REVERB \xe2\x97\x87 ";
    return dsl::vstack()
        .border(BorderStyle::Round)
        .border_color(g_synth_mute ? tb303::clr_dim()
                      : g_focus == Section::FX ? tb303::clr_accent() : tb303::clr_panel_hi())
        .border_text(fx_title, BorderTextPos::Top)
        .align_self(Align::Stretch)
        .basis(Dimension::fixed(0))
        .min_width(Dimension::fixed(0))
        .overflow(Overflow::Hidden)
        .padding(0, 2, 0, 2)(std::move(group_header), std::move(strip), build_caption());
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
        .swing          = g_swing,
        .pattern_length = g_pattern_length,
        .current_step   = g_current_step,
        .pattern_index  = g_preset_index,
        .presets        = std::move(preset_summaries),
        .steps          = std::span<const StepData>{g_steps.data(), g_steps.size()},
        .focused        = (g_focus == Section::Transport),
        .song_mode      = g_song_mode,
        .song_slot      = g_song_slot,
        .jam_mode       = g_jam_mode,
        .jam_octave     = g_jam_octave,
        .jam_last_midi  = g_jam_last_midi,
        .jam_accent     = g_jam_accent,
        .jam_slide      = g_jam_slide,
        .midi_out       = g_midi_out,
        .midi_sync      = g_midi_sync,
    };

    auto knob_row   = build_knob_row();
    auto fx_row     = build_fx_row();
    auto filter_box = tb303::build_filter_response(g_cutoff, g_resonance);
    auto scope_box  = tb303::build_scope();
    auto transport  = tb303::build_transport(ts);

    // Left column stacks FILTER RESPONSE on top of SCOPE so the two visual
    // analyses share the same width (both read horizontally as log-freq /
    // time). basis(0) + min_width(0) lets middle_row's flex algorithm
    // distribute horizontal space by grow factors alone; without this the
    // filter chart's natural width would claim most of the row.
    auto left_col = dsl::vstack()
        .gap(0)
        .grow(1.0f)
        .basis(Dimension::fixed(0))
        .min_width(Dimension::fixed(0))
        .overflow(Overflow::Hidden)
        (
            std::move(filter_box) | dsl::grow(3),
            std::move(scope_box)  | dsl::grow(2)
        );

    auto drums = tb303::build_drums(g_drums,
                                    g_drum_voice_sel, g_drum_step_sel,
                                    g_playing ? g_current_step : -1,
                                    g_drum_master,
                                    g_focus == Section::Drums);
    auto drum_panel = dsl::vstack()
        .border(BorderStyle::Round)
        .border_color(g_drums_mute ? tb303::clr_dim()
                      : g_focus == Section::Drums ? tb303::clr_accent() : tb303::clr_panel_hi())
        .border_text(g_drums_mute ? " DRUMS \xc2\xb7 MUTED (m) " : " DRUMS ", BorderTextPos::Top)
        .basis(Dimension::fixed(0))
        .min_width(Dimension::fixed(0))
        .overflow(Overflow::Hidden)
        .padding(0, 1, 0, 1)(std::move(drums));

    // Middle row hosts three stacked panels side by side: scope/filter (left),
    // transport (centre), drums (right). voice_row (below) is given shrink(1)
    // so on short terminals the knob row compresses first and middle_row keeps
    // enough height to render the 16-voice drum grid unclipped.
    auto middle_row = (dsl::h(
        std::move(left_col)   | dsl::grow(3),
        std::move(transport)  | dsl::grow(2),
        std::move(drum_panel) | dsl::grow(2)
    ) | dsl::gap(1) | dsl::grow(1)).build();

    auto seq = tb303::build_sequencer(g_steps, g_pattern_length, g_step_sel,
                                      g_playing ? g_current_step : -1);
    auto seq_panel = dsl::vstack()
        .border(BorderStyle::Round)
        .border_color(g_synth_mute ? tb303::clr_dim()
                      : g_focus == Section::Sequencer ? tb303::clr_accent() : tb303::clr_panel_hi())
        .border_text(g_synth_mute ? " SEQUENCER \xc2\xb7 MUTED (m) " : " SEQUENCER ", BorderTextPos::Top)
        .basis(Dimension::fixed(0))
        .min_width(Dimension::fixed(0))
        .overflow(Overflow::Hidden)
        .padding(0, 1, 0, 1)(std::move(seq));

    int help_auto = static_cast<int>(g_help_bar_auto_t / kHelpBarChipDur);
    auto help_bar = tb303::build_help_bar(g_focus, g_help_open,
                                          g_help_bar_scroll, g_term_w, help_auto);

    // Title bar: a compact brand strip with a pulse-colored dot that flips
    // from muted → red while playing, so the play state is legible from a
    // glance even when the transport panel is off-screen. The ◆ icon brightens
    // on the downbeat (step%4==0) and fades back over the step — an ambient
    // cue that's easy to sync a shoulder nod to without looking at the cells.
    bool on_downbeat = g_playing && g_current_step >= 0 && (g_current_step % 4 == 0);
    float pulse_amt = on_downbeat ? (1.0f - g_step_phase) : 0.0f;
    maya::Color brand_col = on_downbeat
        ? maya::Color::rgb(
            static_cast<uint8_t>(255),
            static_cast<uint8_t>(138 + (255 - 138) * pulse_amt),
            static_cast<uint8_t>(40  + (140 - 40)  * pulse_amt))
        : tb303::clr_accent();

    auto brand = Element{TextElement{
        .content = "\xe2\x97\x86 TB-303",                    // ◆
        .style   = Style{}.with_fg(brand_col).with_bold(),
        .wrap    = TextWrap::NoWrap,
    }};
    auto brand_sub = Element{TextElement{
        .content = "  acid bass simulator",
        .style   = Style{}.with_fg(tb303::clr_muted()),
        .wrap    = TextWrap::NoWrap,
    }};

    // Right-side status. When a toast is active it REPLACES the play indicator
    // (single element at the right edge) so there's never both a toast AND a
    // "live" glyph on the header at the same time. When the toast timer hits
    // zero we also clear the string so nothing stale can leak into a later
    // render path.
    Element status_el;
    if (g_toast_t > 0.0f) {
        float t = g_toast_t / kToastDur;
        uint8_t r = static_cast<uint8_t>(120 + (255 - 120) * t);
        uint8_t g = static_cast<uint8_t>(120 + ( 70 - 120) * t);
        uint8_t b = static_cast<uint8_t>(120 + ( 70 - 120) * t);
        status_el = Element{TextElement{
            .content = g_toast,
            .style   = Style{}.with_fg(maya::Color::rgb(r, g, b)).with_bold(),
            .wrap    = TextWrap::NoWrap,
        }};
    } else {
        if (!g_toast.empty()) g_toast.clear();
        // When the user is tracking to live.wav, replace the play glyph with
        // a bright blinking REC + elapsed seconds so the indicator is
        // unmissable during a take. Blink rate is ~1 Hz off the same timer
        // the toast uses — coarse enough to read, not enough to distract.
        if (acid_is_recording()) {
            float sec = acid_record_seconds();
            bool blink_on = (static_cast<int>(sec * 2.0f) & 1) == 0;
            char buf[48];
            std::snprintf(buf, sizeof(buf), "%s REC  %5.1fs",
                          blink_on ? "\xe2\x97\x8f" : " ", sec);
            status_el = Element{TextElement{
                .content = buf,
                .style   = Style{}.with_fg(tb303::clr_hot()).with_bold(),
                .wrap    = TextWrap::NoWrap,
            }};
        } else {
            status_el = Element{TextElement{
                .content = g_playing ? "\xe2\x97\x8f  live" : "\xe2\x97\x8b  idle",
                .style   = Style{}.with_fg(g_playing ? tb303::clr_hot() : tb303::clr_muted())
                                 .with_bold(),
                .wrap    = TextWrap::NoWrap,
            }};
        }
    }

    // Group brand on the left, status on the right, with `justify(SpaceBetween)`
    // doing the separation. Previously this used a manual `dsl::space` spacer
    // between brand_sub and status_el. That worked but left a subtle artefact:
    // as the status text shrank (long toast → short "● live"), the spacer had
    // to grow to compensate, and on some terminals the last columns of the old
    // toast text lingered visually until the next full repaint. Using
    // SpaceBetween puts each group at a fixed edge, so the status swap happens
    // in-place at the right margin and old glyphs are always overwritten.
    auto brand_group = dsl::hstack().gap(0)(
        std::move(brand),
        std::move(brand_sub)
    );
    auto title_bar = dsl::hstack()
        .padding(0, 1, 0, 1)
        .justify(Justify::SpaceBetween)
        .align_items(Align::Center)
        (std::move(brand_group), std::move(status_el));

    // Help replaces the main workspace (keeps title + status bar visible) so
    // the user always has context and knows how to close it.
    if (g_help_open) {
        return (dsl::v(
            std::move(title_bar),
            tb303::build_help_overlay(g_help_ov_scroll) | dsl::grow(1),
            std::move(help_bar)
        ) | dsl::gap(0)).build();
    }

    // Sequencer spans the full width at the bottom — drums moved up next to
    // transport so step_row is a single-panel row. shrink(0) pins its natural
    // height on terminal resize so only middle_row flexes.
    auto step_row = dsl::hstack()
        .gap(0)
        .align_items(Align::End)
        .shrink(0)(
            std::move(seq_panel) | dsl::grow(1)
        );

    // Synth voice + FX rack on one row — the voice panel is ~8 knobs wide, FX
    // is ~6, so together they fill a wide terminal in one glance (signal flow
    // reads left → right: voice → FX chain). Stretch keeps the shorter panel's
    // border flush with the taller one's height. shrink(0) pins this row to
    // its natural height when the terminal is resized — only middle_row flexes.
    auto voice_row = dsl::hstack()
        .gap(1)
        .align_items(Align::Stretch)
        .shrink(1)(
            std::move(knob_row) | dsl::grow(8),
            std::move(fx_row)   | dsl::grow(6)
        );

    return (dsl::v(
        std::move(title_bar),
        std::move(voice_row),
        std::move(middle_row),
        std::move(step_row),
        std::move(help_bar)
    ) | dsl::gap(0)).build();
}

// ─────────────────────────────────────────────────────────────────────────────
// Entry point
// ─────────────────────────────────────────────────────────────────────────────

int main() {
    load_preset(0);
    acid_start();
    acid_midi_start();   // No-op on platforms without a MIDI backend.

    using clock = std::chrono::steady_clock;
    auto last = clock::now();

    maya::run(
        RunConfig{
            .title = "tb-303",
            .fps   = 30,
            .mouse = true,
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
            // Wrap the whole tree in a root ComponentElement so every render
            // pass captures the current terminal (w, h) into globals. We need
            // these for mouse hit-testing — maya doesn't otherwise expose
            // widget positions to user code.
            return Element{dsl::component([](int w, int h) -> Element {
                g_term_w = w;
                g_term_h = h;
                return render_frame();
            }).grow(1)};
        }
    );

    // Pair with the 1003h DECSET in tick() — without this the host shell can
    // inherit all-motion mouse tracking and spray SGR escapes at the prompt.
    std::fputs("\x1b[?1003l", stdout);
    std::fflush(stdout);

    acid_midi_stop();
    acid_stop();
    return 0;
}
