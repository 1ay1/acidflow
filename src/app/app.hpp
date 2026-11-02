#pragma once
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
#include "midi_core.hpp"
#include "widgets.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>
#include <maya/host/run.hpp>

using namespace maya;
using namespace maya::dsl;
using tb303::Section;
using tb303::StepData;


// Per-slot bar count for song mode. Each slot (1..9) holds for N bars (N
// pattern wraps) before the chain advances. Defaults to 4 bars — one
// reasonable musical phrase per slot — so manual chains sound like sections
// rather than flash-cuts. The auto-arranger (Shift+A) overwrites per-slot
// values to shape a full song: intro 32 → build 32 → peak 64 → outro 16 etc.
// Session-only; not persisted across restarts.
inline constexpr int kDefaultSongBars = 4;

// Drum machine (Phase 4.3). Flat bool grid — nine voices (rows) × 16 steps
// (cols). Pushed into the engine each tick via acid_seq_set_drum_lane; the
// bool representation is kept here because it's easier to toggle than a
// packed mask, and the pack-to-uint16 happens right before each audio push.
inline constexpr int kDrumVoices = 16;
inline constexpr const char* kDrumLabels[kDrumVoices] = {
    "BD","SD","CH","OH","CL","LT","HT","RS","CB","SH","TB","CG",
    "MT","CY","RD","BG"
};
// Default per-voice gains — match the engine's initial g_seq_drum_gain values.
inline constexpr float kDrumDefaultGain[kDrumVoices] = {
    0.95f, 0.70f, 0.35f, 0.40f, 0.60f,
    0.80f, 0.75f, 0.55f, 0.45f,
    0.30f, 0.35f, 0.65f,
    0.78f, 0.50f, 0.32f, 0.55f
};
inline constexpr float kToastDur = 1.5f;
inline constexpr float kHelpBarChipDur = 1.2f;

// ─────────────────────────────────────────────────────────────────────────────
// Undo / redo — destructive operations (randomize, mutate, preset/slot load,
// clear row / kit / step) snapshot the full musical state before running,
// and `u` pops the last snapshot back. `U` redoes. Keeps the "load random,
// tweak, escape hatch" workflow smooth: if a roll of `R` wrecks a bassline
// you were halfway into, one keypress gets it back.
// ─────────────────────────────────────────────────────────────────────────────
struct Snapshot {
    // Pattern.
    std::array<StepData, 16> steps;
    int pattern_length;
    int preset_index;
    // Drums.
    std::array<std::array<bool, 16>, kDrumVoices> drums;
    std::array<bool, kDrumVoices>                 drum_voice_mute;
    float                                         drum_master;
    // Synth knobs.
    float tune, cutoff, resonance, env_mod, decay, accent, drive, volume;
    int   wave;
    // Transport.
    float bpm, swing;
    // FX.
    float od_amt, delay_mix, delay_fb, rev_mix, rev_size, rev_damp;
    int   delay_div;
};

inline constexpr size_t kUndoMax = 32;

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

inline const char* const kDlyDivNames[4] = {"1/16", "1/16d", "1/8", "1/8d"};

// ── MIDI learn ──────────────────────────────────────────────────────────────────
// Param ids: knobs are 0..K_COUNT-1, FX are kMidiFxBase + idx. CC values are
// applied on the UI tick so knob state stays owned here.
inline constexpr int kMidiFxBase = 16;

// ─────────────────────────────────────────────────────────────────────────────
// Presets
// ─────────────────────────────────────────────────────────────────────────────

struct Preset {
    const char*               name;
    std::array<StepData, 16>  steps;
};

extern const std::array<Preset, 16> g_presets;

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
enum PatArch    { PA_PEDAL, PA_DRIVING, PA_MELODIC, PA_DUB, PA_STUTTER, PA_CHROM, PA_COUNT };
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

extern const VibeSpec kVibes[static_cast<int>(Vibe::V_COUNT)];

// ─────────────────────────────────────────────────────────────────────────────
// Auto-arrangement — one-press (Shift+A) generates a ~10 minute song as a
// chain of 9 slots. Picks a Vibe so every slot shares the same key/tempo/
// palette, then derives section-specific transforms (intro / build / peak /
// break / drop / outro) from that base. Writes each variation to its slot
// file, sets per-slot bar counts, and boots into song mode so playback
// cycles through the sections end-to-end. Pair with `W` before pressing
// Shift+A to capture the whole arrangement to a timestamped take.
// ─────────────────────────────────────────────────────────────────────────────
enum ArrSection {
    SEC_INTRO,      // drums-only, no bass, low cutoff
    SEC_BASS_IN,    // sparse bass on downbeats, minimal drums
    SEC_BUILD,      // mutated bass, rising resonance, opening filter
    SEC_PEAK_A,     // full mix, bright filter, mild drive
    SEC_VARIATION,  // peak-like with 3 mutations on the bass
    SEC_BREAK,      // drums drop to kick-only, echoed bass
    SEC_DROP,       // full mix + crash on 1
    SEC_PEAK_B,     // another mutated peak with open hats
    SEC_OUTRO       // sparse, fading, drums off
};

struct ArrSlot {
    ArrSection role;
    int        bars;
};

// ─────────────────────────────────────────────────────────────────────────────
// Live recording — tap the running audio thread's output into
// ~/.config/acidflow/live_<timestamp>.wav. Unlike the offline bounce
// (export_wav) this captures knob tweaks, jammed notes, MIDI-in hits, and
// pattern edits as they happen. Toggle with capital `W`; max 10 minutes per
// take. Each take carries a timestamp in the filename so takes accumulate
// instead of overwriting, and a companion .txt file is dropped next to the
// wav holding the session state at stop-time (same format as slot files) so
// the take can be reloaded later to keep tweaking from where it ended.
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr int kLiveRecordMaxSeconds = 600;

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

// ─────────────────────────────────────────────────────────────────────────────
// App — all of acidflow's UI state and behaviour. One instance lives in the
// jaal Model; on(), update() and view() reach it there.
// ─────────────────────────────────────────────────────────────────────────────
struct App {
    std::mt19937 rng_{std::random_device{}()};
    tb303::ScopeState       g_scope;
    tb303::SpectrumSnapshot g_spectrum;

    // ─────────────────────────────────────────────────────────────────────────────
    // State
    // ─────────────────────────────────────────────────────────────────────────────
    // Knob and pattern state lives here and is pushed to the engine every tick.
    // The `g_` prefix is historical (these were file globals); they're members.

    // Knobs — normalised 0..1 except k_tuning which is 0..1 mapped to ±12 semis
    // and k_wave which is an enum.
    //
    // Defaults roughly match a "classic acid" 303 knob photo: cutoff a little
    // under noon (~120 Hz so the envelope has room to scream up), resonance
    // three-quarters-right for Q without full self-osc, env-mod around noon,
    // decay toward the short end (~0.4 s sweep — quick squelch, not pads), and
    // a moderate accent so two stacked accents still have headroom.
    float g_tune      = 0.50f;   // 0.5 → 0 semitones
    float g_cutoff    = 0.40f;
    float g_resonance = 0.75f;
    float g_env_mod   = 0.65f;
    float g_decay     = 0.30f;
    float g_accent    = 0.65f;
    float g_drive     = 0.20f;   // output tanh pre-gain
    float g_volume    = 0.65f;
    int   g_wave      = 0;       // 0 = saw, 1 = square

    // Sequencer
    std::array<StepData, 16> g_steps{};
    int   g_pattern_length = 16;
    int   g_current_step   = -1;     // -1 while stopped
    int   g_preset_index   = 0;
    double g_step_clock    = 0.0;    // accumulated seconds within current step

    // Transport — 122 BPM lands between the canonical acid tempos: Phuture
    // "Acid Tracks" at 120, Josh Wink "Higher State" at 124, Adonis "No Way
    // Back" at 122. 128+ pushes into acid techno territory (Beltram, Hawtin).
    bool  g_playing = false;
    float g_bpm     = 122.0f;
    // Swing: 0.50 = straight 16ths, 0.62 ≈ classic MPC swing, 0.66 = hard
    // shuffle. Even-indexed steps (0,2,4…) play on the grid; odd-indexed steps
    // are delayed by (swing − 0.5) × one-pair duration so the pair still sums
    // to two 16ths. Applies uniformly to all steps so every pattern grooves.
    float g_swing   = 0.50f;

    // Song mode (Phase 4.1). When enabled, the transport auto-advances through
    // saved pattern slots on each pattern wrap — classic hardware "chain" mode.
    // g_song_slot is the slot currently loaded (1..9, 0 = none). Wrap detection
    // piggy-backs on g_current_step going backwards between ticks, so no extra
    // audio-thread coupling is needed.
    bool g_song_mode = false;
    int  g_song_slot = 0;
    std::array<int, 10> g_song_bars{
    kDefaultSongBars, kDefaultSongBars, kDefaultSongBars, kDefaultSongBars,
    kDefaultSongBars, kDefaultSongBars, kDefaultSongBars, kDefaultSongBars,
    kDefaultSongBars, kDefaultSongBars
    };
    int g_song_bars_left = 0;            // decrements each pattern wrap

    // Knob automation — arm a knob in DAW-style "write mode" while playing. Each
    // UI tick, the current global-knob value is written into the current step's
    // p-lock slot and the lock bit is set. Only cutoff/res/env/accent have
    // p-lock slots in the engine, so those are the only automatable knobs. To
    // clear a recorded sweep: press `L` in the sequencer (all-locks clear) on
    // the steps you want to unlock, or re-arm and walk through with the knob
    // held at the value you want.
    //   Index: 0=CUTOFF 1=RES 2=ENV 3=ACCENT.
    std::array<bool, 4> g_auto_arm{};

    // Jam mode (Phase 4.2). When enabled, the sequencer is silenced and the
    // computer keyboard becomes a two-octave piano (tracker layout). Terminals
    // don't deliver key-release events, so each press retriggers a short gate
    // (g_jam_gate_s) that tick() closes with acid_note_off when it expires.
    // g_jam_last_midi drives the "PLAYING: C3" display; -1 means silent.
    bool  g_jam_mode      = false;
    int   g_jam_octave    = 3;      // C3 = MIDI 36 — classic bass starting octave
    bool  g_jam_accent    = false;  // toggled with '
    bool  g_jam_slide     = false;  // toggled with ;
    float g_jam_gate_s    = 0.0f;
    int   g_jam_last_midi = -1;

    // FX rack. Separate from the core voice knobs because the signal chain
    // splits clearly at the VCA output: everything before is "synth voice",
    // everything after is "effects". Order = (od pre-filter) → delay → reverb
    // so the FX row is read left-to-right as signal flow.
    float g_od_amt     = 0.0f;          // pre-filter overdrive
    float g_delay_mix  = 0.0f;          // 0 = bypass
    float g_delay_fb   = 0.35f;
    int   g_delay_div  = 2;             // 0=1/16, 1=1/16d, 2=1/8, 3=1/8d
    float g_rev_mix    = 0.0f;
    float g_rev_size   = 0.55f;
    float g_rev_damp   = 0.35f;

    // MIDI I/O (Phase 5). Out = forward note events + drum hits + clock to the
    // MIDI backend port. In sync = slave our BPM + transport to incoming MIDI
    // clock / start / stop. Note-in (controller → bass voice) is always on when
    // the backend is up; these flags only gate the tempo-sync and event-output
    // paths so a user can have a keyboard plugged in without accidentally
    // broadcasting clock to everything downstream.
    bool g_midi_out  = false;
    bool g_midi_sync = false;
    std::array<std::array<bool, 16>, kDrumVoices> g_drums{};
    int   g_drum_voice_sel = 0;     // which row (voice) is being edited
    int   g_drum_step_sel  = 0;     // which step in that row
    float g_drum_master    = 1.0f;  // drum-bus master send (0 = silent)
    std::array<bool, kDrumVoices> g_drum_voice_mute{};  // per-voice mute (s key)

    // Per-section mute toggles — `m` in any non-sequencer section flips the
    // relevant flag. Sequencer keeps `m` for its per-step rest toggle (each step
    // is its own "channel"). Mutes are applied right before pushing volumes to
    // the engine so the rest of the state stays intact.
    bool  g_synth_mute  = false;    // silences the 303 voice (knobs/FX/seq)
    bool  g_drums_mute  = false;    // silences the drum bus

    // UI focus
    Section g_focus      = Section::Sequencer;
    int     g_knob_sel   = 2;   // K_CUTOFF — the signature 303 knob
    int     g_fx_sel     = 0;   // currently-focused FX knob
    int     g_step_sel   = 0;
    bool    g_help_open  = false;

    // Take picker modal — `Y` opens it, Up/Down move the cursor, Enter loads the
    // selected live_<timestamp>.txt into the live pattern state, Esc cancels.
    // Populated by refresh_takes() at open time; sorted newest-first so the take
    // the user just recorded is at the top.
    bool                              g_takes_open = false;
    int                               g_takes_sel  = 0;
    std::vector<std::filesystem::path> g_takes_entries;

    // Beat-sync animation state. g_step_phase ramps 0..1 within the current step
    // (wraps each trigger) so the UI can fade flashes smoothly. g_toast is a
    // transient banner — whenever an action fires (randomize / save / load /
    // export) we set g_toast + g_toast_t and the title bar shows it, fading out
    // over ~1.5s.
    float       g_step_phase = 0.0f;
    std::string g_toast;
    float       g_toast_t    = 0.0f;    // seconds remaining

    // Terminal size, from ResizeEvent (the help bar marquee needs the width).
    // Knob drag state: the value and row the drag started at.
    int   g_term_w = 0;
    int   g_knob_drag_idx = -1;   // -1 when not dragging
    int   g_knob_drag_y0  = 0;
    float g_knob_drag_v0  = 0.0f;

    // Set by the quit keys; the program's update turns it into Cmd::quit.
    bool  g_quit_requested = false;

    // Scroll offsets for areas that may overflow the visible region:
    //   help_bar   — horizontal scroll (units of items), bumped by wheel on row H-1
    //   help_ov    — vertical scroll (line offset), bumped by wheel inside the
    //                help overlay modal
    int   g_help_bar_scroll = 0;
    // Seconds accumulator for the help bar's marquee animation. Every
    // `kHelpBarChipDur` seconds the bar advances by one chip; the builder only
    // applies the rotation when the full chip set is wider than the terminal.
    float g_help_bar_auto_t   = 0.0f;
    int   g_help_ov_scroll  = 0;

    void set_toast(std::string s);
    std::deque<Snapshot> g_undo_past;
    std::deque<Snapshot> g_undo_future;

    Snapshot capture_snapshot();

    void apply_snapshot(const Snapshot& s);

    // Push the current state onto the undo stack before running a destructive
    // action. Clears the redo stack — a new branch of history invalidates any
    // pending redos, matching what users expect from text editors.
    void push_undo();

    void do_undo();

    void do_redo();

    // Captions are written in present-tense "what this knob does" form, mixing a
    // short noun phrase with a bracketed sound-engineering cue so a newcomer can
    // learn the 303 vocabulary just by scanning between knobs. `tech` is the
    // schematic-style symbol that appears under each meter — f₀/fc/Q/τ etc.
    KnobDesc g_knob_table[K_COUNT] = {
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

    FxDesc g_fx_table[FX_COUNT] = {
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
    int           g_midi_learn_wait = -1;   // param we're waiting on, for the toast
    std::array<uint32_t, acid::midi::kMaxParams> g_midi_cc_seen{};

    std::string format_fx(int idx) const;

    // format a knob's current value for display under the rail
    std::string format_knob(int idx) const;

    void load_preset(int idx);

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
    void randomize_knobs(int arch_hint = -1, bool square_bias = false,
                            bool silent = false);

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
    void randomize_pattern(int arch_hint = -1, bool silent = false);

    // ─────────────────────────────────────────────────────────────────────────────
    // FX randomizer — five named archetypes, matched to how the 303 actually gets
    // processed in a rack: DRY (clean), SLAP (close-mic echo), DUB (feedback-
    // soaked delay), CAVERN (plate-reverb wash), DIRTY (overdrive-forward). We
    // pick archetype-friendly ranges so each roll is usable instead of mud.
    // ─────────────────────────────────────────────────────────────────────────────
    void randomize_fx(int arch_hint = -1, bool silent = false);

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
    void randomize_drums(int arch_hint = -1, bool silent = false);

    // ─────────────────────────────────────────────────────────────────────────────
    // Transport randomizer — tempo / swing / length. Stays inside the musical
    // envelope of actual acid records (115-140 BPM, straight or lightly shuffled,
    // 12/16-step patterns) rather than the full knob ranges, so a roll here
    // doesn't send the groove to 60 BPM or 7-step weirdness.
    // ─────────────────────────────────────────────────────────────────────────────
    void randomize_transport(float bpm_center = 0.0f,
                                float bpm_spread = 0.0f,
                                float swing_prob = -1.0f,
                                bool  silent     = false);

    // Randomize everything — pick one Vibe from kVibes and route every subsystem
    // (knobs, FX, pattern, drums, transport) to archetypes that belong together.
    // Each helper is called in silent mode so their individual toasts don't stomp
    // each other, then one combined toast names the vibe.
    //
    // This is what makes capital-R musical: an acid-house roll produces 128 BPM
    // with four-on-the-floor drums, classic-squelch knobs, and slapback delay,
    // rather than the mismatched tempo/knobs/drums you'd get from five
    // independent dice rolls.
    void randomize_everything();

    // ─────────────────────────────────────────────────────────────────────────────
    // Evolving mutator — one small, weighted-random change per call. Repeated
    // presses "walk" the pattern without obliterating it, a vibe somewhere
    // between Generative Sequencer and Mutable Instruments' "Marbles". The key
    // design goal: after N presses the pattern is recognisably descended from
    // where it started, not pure noise.
    // ─────────────────────────────────────────────────────────────────────────────
    void mutate_pattern();

    // ─────────────────────────────────────────────────────────────────────────────
    // Pattern persistence — save/load current 16-step pattern as JSON-ish text
    // under ~/.config/acidflow/. One slot per keyboard digit 1..9; slot 0 is
    // "last", used for the quick-load round-trip.
    // ─────────────────────────────────────────────────────────────────────────────

    std::filesystem::path config_dir();

    // YYYYMMDD_HHMMSS in local time — a filename-safe timestamp so takes don't
    // collide and sort chronologically when the user lists the directory.
    std::string timestamp_now();

    std::filesystem::path pattern_path(int slot);

    // Minimal human-readable format — one step per line, fields space-separated:
    //   note rest accent slide
    // Header line carries pattern_length + bpm so we restore full state.
    // Writes the current state to `path` in the v8 slot format. No toast, no
    // backup — just the raw write, so callers can reuse this for takes and
    // autosaves without surprise side effects.
    bool save_slot_to(const std::filesystem::path& path);

    bool save_pattern_slot(int slot);

    // Core slot-file reader. Shared between numbered-slot loading (digit keys)
    // and take picker loading (live_<timestamp>.txt files alongside wavs).
    // `label` appears in the "loaded X" toast when quiet is false — e.g. "slot 3"
    // or "take 20260421_153900".
    bool load_slot_from_path(const std::filesystem::path& path,
                                const std::string& label,
                                bool quiet);

    // Numbered-slot wrapper around `load_slot_from_path`. Used by digit keys and
    // song mode; the take picker calls `load_slot_from_path` directly with the
    // live-recording .txt path.
    bool load_pattern_slot(int slot, bool quiet = false);

    // Scan the config dir for live_*.txt take files and sort newest-first so the
    // most recent recording sits at the top of the picker. Silent on I/O errors —
    // we treat a missing / unreadable dir as "no takes".
    void refresh_takes();

    void open_take_picker();

    // Song-mode helper — find the next saved slot after `after` (cyclic, 1..9).
    // Returns 0 when nothing is saved at all. When `after` is already saved and is
    // the only saved slot, it returns `after` so song mode loops the same pattern.
    int song_next_slot(int after);

    void auto_arrange();

    // ─────────────────────────────────────────────────────────────────────────────
    // WAV export — offline bounce the current pattern (4 loops) to
    // ~/.config/acidflow/bounce.wav via the engine's synchronous render path.
    // ─────────────────────────────────────────────────────────────────────────────
    bool export_wav();

    // Set when recording begins — reused on stop so the paired wav + slot file
    // share the same timestamp.
    std::string g_live_rec_stem;

    bool toggle_live_record();

    // ─────────────────────────────────────────────────────────────────────────────
    // MIDI export — write the current pattern as a Standard MIDI File (type 0) to
    // ~/.config/acidflow/bounce.mid so the user can drop the groove into any DAW.
    // Represents accent via higher velocity and slide as a small note-overlap so
    // DAWs render it as legato/portamento. One loop of the pattern is written —
    // a MIDI file can be looped natively by any host, unlike a WAV bounce.
    // ─────────────────────────────────────────────────────────────────────────────
    bool export_mid();

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

    std::string base64_encode(const uint8_t* data, size_t n);

    bool base64_decode(const std::string& s, std::vector<uint8_t>& out);

    std::string pattern_encode_string();

    bool pattern_decode_string(const std::string& code);

    bool export_pattern_text();

    bool import_pattern_text();

    // ─────────────────────────────────────────────────────────────────────────────
    // Audio plumbing
    // ─────────────────────────────────────────────────────────────────────────────

    float midi_to_hz(int note);

    // Push current knob + sequencer state into the audio engine every frame.
    // Cheap (atomics), and keeps everything in perfect sync with the UI with no
    // special plumbing. The sequencer itself lives on the audio thread now —
    // this just keeps it fed with up-to-date pattern/transport data.
    void push_params();

    void tick(float dt);

    void start_playback();

    void stop_playback();

    // ── MIDI learn / CC ──────────────────────────────────────────────────────────────

    void apply_midi_cc(int param, float v01);

    std::string midi_param_name(int param);

    // Called from tick(): apply new CC values and report a finished learn.
    void poll_midi_cc();

    // One "cc param" pair per line in ~/.config/acidflow/midi_map.txt.
    void save_midi_map();

    void load_midi_map();

    // ─────────────────────────────────────────────────────────────────────────────
    // Input handling
    // ─────────────────────────────────────────────────────────────────────────────

    void adjust_knob(int idx, float delta);

    void reset_knob(int idx);

    // FX knob adjustment. FX_DTIME is a 4-way enum (1/16..1/8d) so ↑/↓ step
    // through the divisions rather than adjusting a continuous value.
    void adjust_fx(int idx, float delta);

    void reset_fx(int idx);

    // cycle focus forward (or backward)
    void cycle_focus(int dir);

    // Jam-mode piano layout — classic tracker / FL "piano keys" mapping across
    // the lower two QWERTY rows. Lower-case letters only; Shift is reserved for
    // section cycling. Returns semitones above the jam base octave, or -1 if the
    // key isn't part of the piano.
    int jam_key_semi(char c);

    // Pretty-print a MIDI note as "C3", "F#4", etc. — shown in the jam HUD so the
    // user can read the last-played pitch at a glance.
    std::string midi_note_name(int midi);

    // map a typed letter (c, d, e, f, g, a, b) to a MIDI semitone (0=C .. 11=B)
    int letter_to_semitone(char c);

    // Resolve a mouse position to a target via maya's hit registry. The widgets
    // tag their boxes with `| hit(...)`, so this always matches the painted frame.
    // SGR coordinates are 1-based; the registry is 0-based.
    HitZone hit_test(int col, int row);

    // Which section a hover over this target should focus, if any.
    std::optional<Section> hover_section(int col, int row);

    void handle_mouse(const MouseEvent& m);

    void handle_event(const Event& ev);

    // ─────────────────────────────────────────────────────────────────────────────
    // Render
    // ─────────────────────────────────────────────────────────────────────────────

    Element build_knob_row() const;

    // Compact FX rack panel — one bordered row below the SYNTH VOICE panel.
    // Keeps the same group-colour discipline as the voice strip so related FX
    // read together visually. Three groups in signal-flow order:
    //   OD (pre-filter, amber) → DELAY (warm orange) → REVERB (cool teal)
    Element build_fx_row() const;

    Element render_frame() const;

    // ─────────────────────────────────────────────────────────────────────────────
    // Entry point
    // ─────────────────────────────────────────────────────────────────────────────

    // True when nothing on screen is moving, so the tick can slow right down.
    bool ui_idle() const;
    // While idle, the only moving part is the help-bar marquee chip.
    std::uint64_t idle_visual_key() const {
        return static_cast<std::uint64_t>(g_help_bar_auto_t / kHelpBarChipDur) & 0xFFFF;
    }

    App() { load_preset(0); load_midi_map(); }
    App(const App&)            = delete;
    App& operator=(const App&) = delete;

};
