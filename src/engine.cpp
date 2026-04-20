// TB-303-style synth DSP. Platform-agnostic: renders mono float32 at the rate
// set via `set_sample_rate()` into a caller-provided buffer.
//
// Signal chain (single voice):
//   PolyBLEP osc (saw/sq, drift ±3¢)
//     └─► [2×] ZDF/TPT 4-pole ladder (per-stage tanh, fb tanh)
//         └─► VCA (VEG × accent × master)
//             └─► tanh saturator
//                 └─► 16 Hz HPF
//                     └─► out
//
// Roland TB-303 specifics that actually matter for the sound — informed by
// Stinchcombe's circuit analysis, Zavalishin's "Art of VA Filter Design",
// and the Robin Whittle / firstpr.com.au reverse engineering of the real
// hardware:
//
//   * Filter: 4-pole (24 dB/oct) ZDF trapezoidal ladder with tanh in the
//     global feedback path **and** on each stage output (models the diode
//     junctions). Resonance k maps 0..12 — a diode ladder self-oscillates
//     around k≈10–12 vs. Moog's k=4. Passband gain is NOT compensated; the
//     volume drop at high resonance is a signature 303 trait.
//   * 2× oversampling around the whole nonlinear section cuts the high-Q
//     aliasing that otherwise grates on accented notes.
//   * Cutoff range: 13 Hz – 5 kHz (log knob law). Envelope modulates cutoff
//     **exponentially** (V/oct), not linearly in Hz — this is what makes the
//     filter "scream" on accents instead of plodding up in Hz.
//   * MEG (VCF envelope): exponential decay, 0.2–2 s via the Decay knob.
//     On an accented note the Decay pot is bypassed in the real circuit —
//     we replicate by forcing τ=200 ms on accented notes.
//   * Accent envelope: separate "persistent capacitor" (C13 in the schematic,
//     ~200 ms time constant). It does NOT fully discharge between notes, so
//     back-to-back accents stack — this is the classic rising acid squelch.
//     Its contribution to cutoff is cross-coupled with resonance (at low res
//     the accent is attenuated, per the stock circuit).
//   * VEG (VCA envelope): fixed ~3 s decay — no user control on real 303.
//     A 3 ms linear attack ramp sits on top of the exponential so hard gates
//     don't click.
//   * Slide: exponential RC on the pitch CV, τ ≈ 88 ms (≈ R=400kΩ, C=0.22µF).
//     Constant-time — settles in ~4τ regardless of interval.
//   * VCO drift: slow filtered noise, ±3 cents. Stops the synth sounding
//     computer-perfect on long sustained notes.
//   * VCA saturation: gentle tanh post-filter + 16 Hz HPF (matches the
//     output-coupling cap). Gives accents their "bite" without becoming a
//     distortion pedal.
//   * Parameter smoothing: cutoff / resonance / volume are one-pole smoothed
//     at 15 ms so discrete (keyboard) knob steps don't audibly zipper.
//
// Command delivery (UI → audio) is lock-free SPSC: the UI bumps a seq counter
// after staging freq/flags into relaxed atomics; the audio thread commits on
// its next render by observing the seq change with acquire ordering.

#include "engine.hpp"
#include "audio.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace acid {
namespace {

// ── Parameters (UI-thread writes, audio-thread reads per block) ─────────────
struct Params {
    std::atomic<float> cutoff     {0.35f};  // 0..1 → 13..5000 Hz, log
    std::atomic<float> resonance  {0.78f};  // 0..1 → k = 0..18
    std::atomic<float> env_mod    {0.70f};  // 0..1 → 0..5 octaves of sweep
    std::atomic<float> decay      {0.40f};  // 0..1 → 0.2..2 s (MEG)
    std::atomic<float> accent_amt {0.55f};  // 0..1
    std::atomic<float> drive      {0.20f};  // 0..1 → tanh pre-gain (post-VCA)
    std::atomic<float> tuning     {0.0f};   // ±12 semitones
    std::atomic<float> master_vol {0.60f};
    std::atomic<int>   waveform   {0};      // 0 = saw, 1 = square
};

Params g_params;
int    g_sample_rate = kDefaultSampleRate;

// ── Note command ring (SPSC lock-free) ──────────────────────────────────────
std::atomic<uint64_t> g_cmd_seq       {0};
std::atomic<float>    g_cmd_freq      {440.0f};
std::atomic<uint32_t> g_cmd_flags     {0};
std::atomic<float>    g_cmd_slide_time{0.0f};
uint64_t g_last_seq = 0;

// ── Audio-thread state (only touched from render()) ─────────────────────────

// Pitch & slide
float g_target_freq  = 440.0f;
float g_current_freq = 440.0f;

// VCO drift — pink-ish noise filtered to ~0.5 Hz, scaled to ±3 cents. Real 303
// oscillators wander a few cents around temperature; without this the synth
// sounds computer-perfect on sustained notes.
uint32_t g_drift_rng  = 0xdeadbeefu;
float    g_drift_lp   = 0.0f;      // filtered noise, ±1
float    g_drift_oct  = 0.0f;      // final octave offset applied to pitch

// Oscillator
float g_phase    = 0.0f;
float g_osc_prev = 0.0f;           // for 2x upsample linear interp

// ZDF/TPT 4-pole ladder state.
//   Each stage is a trapezoidal 1-pole LPF:  y = (s + g·x)/(1+g); s' = 2y - s
//   Feedback is global (tanh on the summing node). Solved by 2-iter fixed
//   point using the previous output as the seed — converges well for the
//   mild nonlinearity in a ladder filter and is massively cheaper than
//   Newton's method. Per-stage tanh saturates each integrator to mimic the
//   diode junctions in the TB-303's stock ladder.
float g_tpt_s[4] = {0.0f, 0.0f, 0.0f, 0.0f};
float g_tpt_y4   = 0.0f;           // last stage-4 output (feedback seed)

// Smoothed parameter targets (kill zipper noise when knob jumps discretely).
float g_fc_sm  = 200.0f;           // smoothed base cutoff in Hz
float g_res_sm = 0.0f;             // smoothed feedback gain k
float g_mvol_sm= 0.0f;             // smoothed master volume

// Envelopes (all exponential)
float g_meg     = 0.0f;            // filter envelope (0..1, triggered on note-on)
float g_veg     = 0.0f;            // amp envelope (0..1)
float g_acc_env = 0.0f;            // persistent accent cap voltage (0..1)

// VCA attack ramp. The real 303 has a ~2 ms RC on the VEG rise, and digital
// simulations that snap from 0→1 instantly click audibly. This phase ramps
// linearly from 0→1 over ~3 ms on each non-slide note-on; we take max(veg,
// atk_phase) so slides (which don't retrigger) are unaffected.
float g_veg_atk = 1.0f;

// Latched state of current held note
bool  g_gate          = false;
bool  g_note_accented = false;

// Output-stage DC blocker
float g_hpf_xprev = 0.0f;
float g_hpf_yprev = 0.0f;

// ── Scope + meter taps ──────────────────────────────────────────────────────
// Lock-free single-producer / single-consumer ring. Audio thread writes into
// g_scope_buf at g_scope_w (relaxed → release bump); UI thread reads the
// most-recent N samples during render. Only the write index needs atomicity;
// the buffer itself is racy under pathological timing but we only draw, not
// process, so a tearing sample is visually invisible.
constexpr int kScopeRingSize = 4096;
float                 g_scope_buf[kScopeRingSize] = {};
std::atomic<uint32_t> g_scope_w{0};

// Peak envelope: max |sample| over a ~100 ms window, one-pole decay for nice
// meter ballistics. UI reads the atomic once per frame.
std::atomic<float> g_peak{0.0f};
float              g_peak_env = 0.0f;

// Current effective filter cutoff (smoothed base × envelope × accent), in Hz.
// Snapshotted once per render block — plenty fast for the 30 Hz UI, and
// keeps audio-side overhead to a single relaxed store per block.
std::atomic<float> g_live_fc{200.0f};

// ── Audio-thread sequencer ──────────────────────────────────────────────────
// Live step scheduling is done on the audio thread against a sample-accurate
// clock. The UI used to drive it from the 30 FPS render callback, which
// quantised every note-on to ~33 ms and made the live version audibly looser
// than the offline WAV bounce. Now both paths share this scheduler, so live
// matches export to the sample.
//
// UI-shared atomics: pattern (packed midi|flags per slot), length, bpm,
// swing, playing flag, plus published current step + within-step phase for
// beat-sync animation.
//
// Audio-thread locals (g_seq_clock etc.) are only touched inside render().
constexpr int kSeqMaxSteps = 16;
std::atomic<uint32_t> g_seq_steps[kSeqMaxSteps]  {};            // bits: flags<<16 | midi
std::atomic<int>      g_seq_pattern_length       {16};
std::atomic<float>    g_seq_bpm                  {122.0f};
std::atomic<float>    g_seq_swing                {0.50f};       // 0.50..0.75
std::atomic<bool>     g_seq_playing              {false};
std::atomic<int>      g_seq_current_step         {-1};
std::atomic<float>    g_seq_step_phase           {0.0f};

double g_seq_clock       = 0.0;         // samples into the current step
int    g_seq_step        = -1;          // playing step (-1 = idle)
bool   g_seq_prev_rest   = true;        // for slide-after-rest suppression
bool   g_seq_was_playing = false;

// ── Helpers ─────────────────────────────────────────────────────────────────

// PolyBLEP residual for a waveform discontinuity at phase wrap.
// `t` is phase in [0,1), `dt` is phase increment per sample.
// For a saw: subtract this at phase=0; for a square: add at the rising edge,
// subtract at the falling edge.
inline float poly_blep(float t, float dt) {
    if (t < dt) {
        float x = t / dt;
        return x + x - x * x - 1.0f;
    } else if (t > 1.0f - dt) {
        float x = (t - 1.0f) / dt;
        return x * x + x + x + 1.0f;
    }
    return 0.0f;
}

// Exponential knob → Hz. 13 Hz at 0, 5 kHz at 1. Matches measured 303 range.
inline float cutoff_knob_to_hz(float knob) {
    constexpr float kMin = 13.0f;
    constexpr float kMax = 5000.0f;
    float k = std::clamp(knob, 0.0f, 1.0f);
    return kMin * std::pow(kMax / kMin, k);
}

// Decay knob → seconds (log): 0.2 s at 0, 2 s at 1.
inline float decay_knob_to_seconds(float knob) {
    float k = std::clamp(knob, 0.0f, 1.0f);
    return 0.2f * std::pow(10.0f, k);
}

// Pitch-dependent duty cycle for the 303's square wave. The real circuit
// produces ~71% duty at the bottom of the range and ~45% at the top — the
// derived-from-saw comparator threshold drifts with frequency.
inline float square_duty_for_hz(float hz) {
    // midi_note = 69 + 12 * log2(hz / 440); clamp to [24..84] — 303's playable
    // range roughly — and interpolate linearly.
    float midi = 69.0f + 12.0f * std::log2(std::max(hz, 20.0f) / 440.0f);
    float n = std::clamp((midi - 24.0f) / 60.0f, 0.0f, 1.0f);
    return 0.71f - 0.26f * n;
}

// Fast xorshift32 → uniform float in [-1, 1]. Used only for drift noise,
// so statistical quality doesn't matter — we just want a cheap per-sample
// random without calling into libc's rand().
inline float rand_bipolar(uint32_t& s) {
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return static_cast<float>(static_cast<int32_t>(s)) * (1.0f / 2147483648.0f);
}

// ZDF/TPT 4-pole ladder filter with tanh in the global feedback path and
// per-stage gentle saturation. Solves the implicit feedback equation by
// seeding with the previous output and doing 2 fixed-point iterations —
// stable up to self-oscillation and indistinguishable from Newton for the
// mild nonlinearity we have here.
//
//   g   = tan(π·fc/sr_eff)    [pre-warped integrator gain]
//   k   = feedback depth (resonance)
//   y4  = LPF( LPF( LPF( LPF( tanh(x − k·y4) ) ) ) )
//
// On return, the per-stage states g_tpt_s[] are advanced by one sample and
// g_tpt_y4 is updated so the next call can seed from a good guess.
inline float ladder_tpt_step(float x, float g, float k) {
    const float G = g / (1.0f + g);        // normalised 1-pole gain per stage

    // Iterative feedback solve. Two passes with the previous output as seed
    // keeps the resonance peak clean even when tanh is clipping hard.
    float y4 = g_tpt_y4;
    for (int it = 0; it < 2; ++it) {
        float u  = std::tanh(x - k * y4);
        float y1 = G * u  + g_tpt_s[0] / (1.0f + g);
        float y2 = G * y1 + g_tpt_s[1] / (1.0f + g);
        float y3 = G * y2 + g_tpt_s[2] / (1.0f + g);
        y4       = G * y3 + g_tpt_s[3] / (1.0f + g);
    }

    // Commit: recompute stage outputs with the converged feedback and step
    // each integrator's state. Gentle tanh after every stage mimics the
    // progressive diode-junction compression of the real ladder — this is
    // what gives high-resonance 303s their creamy (not brittle) squelch.
    float u  = std::tanh(x - k * y4);
    float y1 = G * u  + g_tpt_s[0] / (1.0f + g);
    y1 = std::tanh(y1 * 1.05f) * 0.95f;
    g_tpt_s[0] = 2.0f * y1 - g_tpt_s[0];

    float y2 = G * y1 + g_tpt_s[1] / (1.0f + g);
    y2 = std::tanh(y2 * 1.05f) * 0.95f;
    g_tpt_s[1] = 2.0f * y2 - g_tpt_s[1];

    float y3 = G * y2 + g_tpt_s[2] / (1.0f + g);
    y3 = std::tanh(y3 * 1.05f) * 0.95f;
    g_tpt_s[2] = 2.0f * y3 - g_tpt_s[2];

    y4 = G * y3 + g_tpt_s[3] / (1.0f + g);
    g_tpt_s[3] = 2.0f * y4 - g_tpt_s[3];

    g_tpt_y4 = y4;
    return y4;
}

// Shared note-commit logic used by both the external command ring (for
// compatibility) and the audio-thread sequencer. Mutates audio-thread state
// directly — only safe to call from inside render().
//
// Slide semantics (stock 303): the previous note's gate is still held when
// the new note fires, so envelopes DON'T retrigger — they continue decaying
// from wherever they were, and only the pitch CV glides to the new target.
// Without this the "rising squelch" on slide chains gets interrupted on
// every step.
//
// Accent cap: add to existing charge (does not fully discharge between fast
// consecutive accents → rising squelch). 0.45 was measured off the real C13
// voltage step in Stinchcombe's notes.
inline void note_commit_on(float freq, bool accent, bool slide) {
    g_target_freq = freq * std::pow(2.0f, g_params.tuning.load() / 12.0f);
    const bool is_slide = slide && g_current_freq >= 10.0f;
    if (!is_slide) {
        g_current_freq = g_target_freq;
        g_phase        = 0.0f;
        g_meg          = 1.0f;
        g_veg          = 0.0f;   // start silent — ramp up via g_veg_atk
        g_veg_atk      = 0.0f;   // triggers the 3 ms linear attack
        g_gate         = true;
    }
    g_note_accented = accent;
    if (accent) {
        float acc = g_params.accent_amt.load();
        g_acc_env = std::min(1.0f, g_acc_env + 0.45f * acc);
    }
}

inline void note_commit_off() {
    g_gate = false;
}

// Inner sample-loop worker. All the per-block DSP constants live in the
// captures of the lambda in render(); this function is just the declaration.
// (Implemented inline inside render() as a lambda so it can reference those
// constants cheaply.)

}  // namespace

void set_sample_rate(int sr) { g_sample_rate = sr > 0 ? sr : kDefaultSampleRate; }
int  sample_rate()           { return g_sample_rate; }

void render(float* out, int frames) {
    const float sr    = static_cast<float>(g_sample_rate);
    const float dt_s  = 1.0f / sr;

    // ── External note command ring (legacy path; used by nothing now, but
    // kept working for offline render tools / external drivers) ─────────────
    uint64_t seq = g_cmd_seq.load(std::memory_order_acquire);
    if (seq != g_last_seq) {
        g_last_seq      = seq;
        uint32_t flags  = g_cmd_flags.load(std::memory_order_relaxed);
        bool on         = (flags & 0b001);
        bool accent     = (flags & 0b010);
        bool slide      = (flags & 0b100);
        float freq      = g_cmd_freq.load(std::memory_order_relaxed);
        if (on) note_commit_on(freq, accent, slide);
        else    note_commit_off();
    }

    // ── Sequencer snapshot + play/stop transition ──────────────────────────
    const bool  sp_on = g_seq_playing.load(std::memory_order_acquire);
    const int   plen  = std::clamp(g_seq_pattern_length.load(std::memory_order_relaxed),
                                   1, kSeqMaxSteps);
    const float bpm   = std::max(20.0f, g_seq_bpm.load(std::memory_order_relaxed));
    const float swin  = std::clamp(g_seq_swing.load(std::memory_order_relaxed), 0.50f, 0.75f);
    const double base_samples = 60.0 / static_cast<double>(bpm) / 4.0
                              * static_cast<double>(sr);
    auto step_samples = [&](int si) -> double {
        // Even steps stretch, odd steps compress — pairs always sum to 2 × base,
        // so the bar length stays locked to the BPM regardless of swing.
        bool even = (si % 2) == 0;
        return base_samples * 2.0 * (even ? swin : (1.0 - swin));
    };

    if (sp_on && !g_seq_was_playing) {
        // Rising edge: step -1 is a sentinel whose duration is 0 so the very
        // next pass through the advance loop bumps us to step 0 and fires the
        // first note. Without this clock-at-zero trick, a large sentinel
        // would chain-advance through every step in one shot.
        g_seq_step      = -1;
        g_seq_clock     = 0.0;
        g_seq_prev_rest = true;
    } else if (!sp_on && g_seq_was_playing) {
        // Falling edge: release the gate and park.
        note_commit_off();
        g_seq_step = -1;
        g_seq_current_step.store(-1, std::memory_order_relaxed);
        g_seq_step_phase.store(0.0f,  std::memory_order_relaxed);
    }
    g_seq_was_playing = sp_on;

    // ── Sub-block loop ─────────────────────────────────────────────────────
    // Slice the audio block at step boundaries so note_on fires on the exact
    // sample, matching the offline WAV bounce. Block constants (env times,
    // filter coeffs, …) are recomputed once per sub-block — the sequencer
    // may toggle accent mid-block and those constants depend on it.
    int   frames_left = frames;
    float* out_ptr    = out;
    while (frames_left > 0) {
        // Advance sequencer across any boundaries that are already due.
        if (sp_on) {
            while (true) {
                // Pre-start (step==-1) has zero "duration" so the first
                // iteration immediately advances to step 0 and fires its note.
                double cur_dur = (g_seq_step < 0) ? 0.0 : step_samples(g_seq_step);
                if (g_seq_clock < cur_dur) break;
                g_seq_clock -= cur_dur;
                g_seq_step   = (g_seq_step + 1) % plen;
                uint32_t enc = g_seq_steps[g_seq_step].load(std::memory_order_relaxed);
                int  midi  = static_cast<int>(enc & 0xFFFF);
                int  flg   = static_cast<int>((enc >> 16) & 0xFF);
                bool rest  = (flg & 1) != 0;
                bool acc   = (flg & 2) != 0;
                bool sld   = (flg & 4) != 0;
                if (!rest) {
                    // Mirror live tick()'s rule: a slide only counts when the
                    // previous step actually played — you can't glide from a
                    // rest.
                    float freq = 440.0f * std::pow(2.0f,
                                 static_cast<float>(midi - 69) / 12.0f);
                    note_commit_on(freq, acc, sld && !g_seq_prev_rest);
                }
                g_seq_prev_rest = rest;
                g_seq_current_step.store(g_seq_step, std::memory_order_relaxed);
            }
        }

        // How many frames until the next step boundary?
        int n;
        if (sp_on && g_seq_step >= 0) {
            double remaining = step_samples(g_seq_step) - g_seq_clock;
            int fu = (remaining <= 1.0) ? 1 : static_cast<int>(remaining);
            n = std::min(frames_left, fu);
        } else {
            n = frames_left;
        }

        // ── Block-rate parameter snapshot (per sub-block) ───────────────────
    const float fc_knob  = g_params.cutoff.load();
    const float res_knob = g_params.resonance.load();
    const float envmod_k = g_params.env_mod.load();
    const float decay_k  = g_params.decay.load();
    const float acc_amt  = g_params.accent_amt.load();
    const float drive_k  = g_params.drive.load();
    const int   wf       = g_params.waveform.load();
    const float master   = g_params.master_vol.load();

    // Output-stage drive. 0 = barely-there tanh (1.3× pre-gain, historical
    // 303-into-mixer level), 1 = "303 into a fuzzpedal" (≈ 6× pre-gain with
    // post-normalise). The pre-gain gets compensated on the way out so the
    // overall loudness stays roughly matched as the user sweeps drive — this
    // turns the knob into a TEXTURE knob, not a volume knob.
    const float drive_pre  = 1.3f + drive_k * 4.7f;      // 1.3..6.0
    const float drive_norm = 1.0f / std::tanh(drive_pre);

    // MEG time constant: accent bypasses the Decay pot and forces ~200 ms.
    const float meg_tau  = g_note_accented ? 0.2f : decay_knob_to_seconds(decay_k);
    const float meg_dec  = std::exp(-dt_s / meg_tau);

    // VEG: hold while gated, snap-off after release. ~3 s when held matches
    // the fixed VEG RC in the real unit (we don't expose this knob).
    const float veg_tau  = g_gate ? 3.0f : 0.008f;
    const float veg_dec  = std::exp(-dt_s / veg_tau);

    // Accent cap discharge: τ ≈ 200 ms (C13 = 1 µF / R stock values).
    const float acc_dec  = std::exp(-dt_s / 0.20f);

    // Slide: exponential RC on pitch CV, τ = 88 ms.
    const float slide_coef = 1.0f - std::exp(-dt_s / 0.088f);

    // Resonance: map knob to feedback gain. Diode ladder with tanh feedback
    // self-oscillates around k≈10–12; 12 lets the top of the knob hit full
    // self-osc without spending most of the range above it (which reads as
    // pure whistle rather than 303 squelch).
    const float k_res    = res_knob * 12.0f;

    // Filter envelope modulation. 4 octaves from MEG is about right — the
    // real ENV MOD pot on a 303 sweeps roughly 0–4 oct at max (Whittle's
    // measurements). Beyond that, the filter just screams.
    const float env_oct  = envmod_k * 4.0f;

    // Accent contribution is on its own CV path — the accent cap feeds the
    // filter summing node directly, bypassing the ENV MOD attenuator. Fixed
    // max range (~2 octaves), cross-coupled with resonance (at low Q the
    // accent is largely shunted in the stock schematic).
    const float acc_oct_max = 2.0f;
    const float acc_coup    = 0.5f + 0.5f * res_knob;

    // Base cutoff target (in Hz) — smoothed per-sample to kill zipper noise on
    // discrete knob jumps. 15 ms time constant is short enough to feel
    // instant under the finger but long enough to smear a ≥5% knob step into
    // inaudibility.
    const float fc_base_target  = cutoff_knob_to_hz(fc_knob);
    const float smooth_coef     = 1.0f - std::exp(-dt_s / 0.015f);

    // Precomputed duty cycle (doesn't need to track micro pitch changes). For
    // the DC-center trick we subtract the duty's mean from the square output,
    // so resonance feedback doesn't carry a DC bias that would shift pitch at
    // high Q.
    const float duty            = square_duty_for_hz(g_current_freq);
    const float square_dc       = 2.0f * duty - 1.0f;

    // DC-blocker coefficient: one-pole HPF at ~16 Hz.
    const float hpf_a           = std::exp(-2.0f * static_cast<float>(M_PI) * 16.0f / sr);

    // VCA attack ramp coefficient: linear 3 ms rise from 0 to 1 on note-on.
    const float atk_step        = dt_s / 0.003f;

    // Drift LFO: pink-ish noise (1-pole LPF'd at ~0.4 Hz) scaled to ±0.003
    // octaves = ±3 cents. Applied as a V/oct offset on pitch.
    const float drift_coef      = 1.0f - std::exp(-dt_s / 0.4f);
    const float drift_depth_oct = 0.003f;

    // 2x oversampling rate for the filter + saturator section. Running the
    // nonlinearities at 2·sr and decimating by simple 2-tap averaging halves
    // the aliasing you'd otherwise hear as "grit" on high-Q squelches.
    const float sr2 = sr * 2.0f;

    // ── Sample loop (inner — runs for `n` frames of this sub-block) ─────
    for (int i = 0; i < n; i++) {
        // Slide: exponential approach to target pitch (single-pole LPF)
        g_current_freq += slide_coef * (g_target_freq - g_current_freq);

        // Analog drift: generate a filtered random walk, scale to ±3 cents,
        // apply as a V/oct offset. Freq is recomputed from this offset below.
        float dn = rand_bipolar(g_drift_rng);
        g_drift_lp  += drift_coef * (dn - g_drift_lp);
        g_drift_oct  = g_drift_lp * drift_depth_oct;
        const float freq_with_drift = g_current_freq * std::exp2(g_drift_oct);

        // Oscillator
        const float phase_inc = freq_with_drift / sr;
        g_phase += phase_inc;
        if (g_phase >= 1.0f) g_phase -= 1.0f;

        float osc;
        if (wf == 0) {
            // Band-limited saw: naive ramp minus PolyBLEP at the wrap.
            float naive = 2.0f * g_phase - 1.0f;
            osc = naive - poly_blep(g_phase, phase_inc);
        } else {
            // Band-limited square with pitch-dependent duty. DC-centered.
            osc = (g_phase < duty) ? 1.0f : -1.0f;
            osc += poly_blep(g_phase, phase_inc);                // rising edge
            float tf = g_phase - duty;
            if (tf < 0.0f) tf += 1.0f;
            osc -= poly_blep(tf, phase_inc);                     // falling edge
            osc -= square_dc;
        }

        // Parameter smoothing (per-sample, one-pole).
        g_fc_sm   += smooth_coef * (fc_base_target - g_fc_sm);
        g_res_sm  += smooth_coef * (k_res          - g_res_sm);
        g_mvol_sm += smooth_coef * (master         - g_mvol_sm);

        // Envelope updates (exponential decay — multiply by pre-computed factor)
        g_meg     *= meg_dec;
        g_veg     *= veg_dec;
        g_acc_env *= acc_dec;

        // Fast linear attack ramp for the VCA. Takes max(veg, atk) so an
        // existing tail (from a slide or stacked notes) is never pulled down.
        if (g_veg_atk < 1.0f) {
            g_veg_atk = std::min(1.0f, g_veg_atk + atk_step);
            if (g_veg_atk > g_veg) g_veg = g_veg_atk;
        }

        // Exponential cutoff modulation (V/oct). MEG rides the ENV MOD knob;
        // accent is on its own fixed-range CV path (resonance-coupled).
        float oct_shift = env_oct * g_meg + acc_oct_max * acc_coup * g_acc_env;
        float fc_hz     = g_fc_sm * std::exp2(oct_shift);
        fc_hz = std::clamp(fc_hz, 20.0f, std::min(10000.0f, 0.45f * sr));

        // Pre-warped TPT integrator gain at the 2× rate. `tan` blows up as we
        // approach Nyquist of the oversampled rate (sr2 / 2 = sr), so the
        // clamp above already keeps us in the safe zone.
        const float g_tpt = std::tan(static_cast<float>(M_PI) * fc_hz / sr2);

        // 2× oversample the filter + saturator. Produce two input sub-samples
        // by linear interpolation between the previous and current osc value,
        // run each through the ZDF ladder, and average for decimation.
        const float osc_mid = 0.5f * (g_osc_prev + osc);
        const float y_a     = ladder_tpt_step(osc_mid, g_tpt, g_res_sm);
        const float y_b     = ladder_tpt_step(osc,     g_tpt, g_res_sm);
        g_osc_prev          = osc;
        const float y_filt  = 0.5f * (y_a + y_b);

        // VCA: VEG × (accent boost on accented notes) × smoothed master.
        float amp = g_veg;
        if (g_note_accented) amp *= (1.0f + acc_amt);
        amp *= g_mvol_sm;

        float y = y_filt * amp;

        // Post-VCA soft clip — the real BA662 VCA starts compressing before
        // the rail; tanh with modest drive captures the "bite" on accents.
        // The DRIVE knob scales the pre-gain and we divide by tanh(pre) so
        // perceived loudness is roughly constant as drive sweeps.
        y = std::tanh(y * drive_pre) * drive_norm;

        // 16 Hz single-pole HPF (matches the output-coupling cap, also
        // removes any DC drift from the filter at low cutoffs).
        float y_hpf = y - g_hpf_xprev + hpf_a * g_hpf_yprev;
        g_hpf_xprev = y;
        g_hpf_yprev = y_hpf;

        out_ptr[i] = y_hpf;

        // Scope tap: store in ring; peak envelope decays ~100 ms.
        uint32_t w = g_scope_w.load(std::memory_order_relaxed);
        g_scope_buf[w & (kScopeRingSize - 1)] = y_hpf;
        g_scope_w.store(w + 1, std::memory_order_release);

        float abs_y = std::fabs(y_hpf);
        if (abs_y > g_peak_env) g_peak_env = abs_y;
        else                    g_peak_env *= 0.9995f;   // ~100 ms decay @ 44.1kHz

        // Publish the most-recent effective cutoff once per sample — cheap
        // (relaxed atomic store) and lets the UI show the envelope "scream".
        g_live_fc.store(fc_hz, std::memory_order_relaxed);
    }

    // Advance sequencer clock, output cursor, remaining frames.
    g_seq_clock += static_cast<double>(n);
    out_ptr     += n;
    frames_left -= n;

    // Publish within-step phase for UI beat-sync fades.
    if (sp_on && g_seq_step >= 0) {
        double denom = step_samples(g_seq_step);
        float  ph    = (denom > 0.0) ? static_cast<float>(g_seq_clock / denom) : 0.0f;
        g_seq_step_phase.store(std::clamp(ph, 0.0f, 1.0f),
                               std::memory_order_relaxed);
    }
    }  // end sub-block while loop

    g_peak.store(g_peak_env, std::memory_order_relaxed);
}

}  // namespace acid

// ── Extern C bridge (parameter setters + note triggers) ─────────────────────
// `acid_start` / `acid_stop` live in the platform-specific backend; everything
// else just pokes atomics, so we define it once here.
extern "C" {

void acid_set_cutoff(float v)      { acid::g_params.cutoff.store(v); }
void acid_set_resonance(float v)   { acid::g_params.resonance.store(v); }
void acid_set_env_mod(float v)     { acid::g_params.env_mod.store(v); }
void acid_set_decay(float v)       { acid::g_params.decay.store(v); }
void acid_set_accent_amt(float v)  { acid::g_params.accent_amt.store(v); }
void acid_set_drive(float v)       { acid::g_params.drive.store(v); }
void acid_set_volume(float v)      { acid::g_params.master_vol.store(v); }
void acid_set_tuning_semi(float v) { acid::g_params.tuning.store(v); }
void acid_set_waveform(int v)      { acid::g_params.waveform.store(v); }

void acid_note_on(float freq, int accent, int slide, float step_sec) {
    acid::g_cmd_freq.store(freq, std::memory_order_relaxed);
    uint32_t flags = 0b001;
    if (accent) flags |= 0b010;
    if (slide)  flags |= 0b100;
    acid::g_cmd_flags.store(flags, std::memory_order_relaxed);
    acid::g_cmd_slide_time.store(slide ? step_sec * 0.6f : 0.0f,
                                 std::memory_order_relaxed);
    acid::g_cmd_seq.fetch_add(1, std::memory_order_release);
}

void acid_note_off() {
    acid::g_cmd_flags.store(0, std::memory_order_relaxed);
    acid::g_cmd_seq.fetch_add(1, std::memory_order_release);
}

int acid_scope_tail(float* dst, int n) {
    if (!dst || n <= 0) return 0;
    int avail = std::min(n, acid::kScopeRingSize);
    uint32_t w = acid::g_scope_w.load(std::memory_order_acquire);
    // Copy the most-recent `avail` samples ending at w-1. Done with two
    // memcpys straddling the ring's wrap point — avoids per-sample work.
    uint32_t start = w - static_cast<uint32_t>(avail);
    for (int i = 0; i < avail; ++i) {
        dst[i] = acid::g_scope_buf[(start + i) & (acid::kScopeRingSize - 1)];
    }
    return avail;
}

float acid_output_peak(void) {
    return acid::g_peak.load(std::memory_order_relaxed);
}

float acid_live_fc(void) {
    return acid::g_live_fc.load(std::memory_order_relaxed);
}

// ── Audio-thread sequencer API (called from UI) ─────────────────────────────
// Each step is packed as (flags<<16 | midi) where flags bits are
//   {rest=1, accent=2, slide=4}. Every write is a single atomic store; the
// audio thread reads each slot when it advances to that step so a torn read
// is not possible (slot state is self-contained per step).

void acid_seq_set_step(int idx, int midi, int flags) {
    if (idx < 0 || idx >= acid::kSeqMaxSteps) return;
    uint32_t enc = (static_cast<uint32_t>(flags & 0xFF) << 16)
                 | (static_cast<uint32_t>(midi) & 0xFFFFu);
    acid::g_seq_steps[idx].store(enc, std::memory_order_relaxed);
}

void acid_seq_set_pattern_length(int n) {
    acid::g_seq_pattern_length.store(std::clamp(n, 1, acid::kSeqMaxSteps),
                                     std::memory_order_relaxed);
}

void acid_seq_set_bpm(float bpm) {
    acid::g_seq_bpm.store(std::clamp(bpm, 20.0f, 300.0f),
                          std::memory_order_relaxed);
}

void acid_seq_set_swing(float s) {
    acid::g_seq_swing.store(std::clamp(s, 0.50f, 0.75f),
                            std::memory_order_relaxed);
}

void acid_seq_play(void)  { acid::g_seq_playing.store(true,  std::memory_order_release); }
void acid_seq_stop(void)  { acid::g_seq_playing.store(false, std::memory_order_release); }

int   acid_seq_current_step(void) {
    return acid::g_seq_current_step.load(std::memory_order_relaxed);
}

float acid_seq_step_phase(void) {
    return acid::g_seq_step_phase.load(std::memory_order_relaxed);
}

// ── Offline WAV render ──────────────────────────────────────────────────────
// Drives the same in-engine sequencer as live playback — just runs render()
// synchronously on the calling thread. Caller must have torn down the audio
// backend (main.cpp does acid_stop() → render → acid_start()).
int acid_render_wav(const char* path,
                    const int*  notes,
                    int         pattern_length,
                    float       bpm,
                    float       swing,
                    int         loops) {
    if (!path || !notes || pattern_length <= 0 || loops <= 0 || bpm <= 0.0f)
        return -1;
    pattern_length = std::min(pattern_length, acid::kSeqMaxSteps);
    swing = std::clamp(swing, 0.50f, 0.75f);

    FILE* f = std::fopen(path, "wb");
    if (!f) return -1;

    const int sr          = acid::sample_rate();
    const int total_steps = pattern_length * loops;

    // Compute total frame count using the same swing formula the audio thread
    // uses, so the WAV header matches exactly what we write.
    const double base_samples = 60.0 / static_cast<double>(bpm) / 4.0
                              * static_cast<double>(sr);
    double pattern_samples = 0.0;
    for (int si = 0; si < total_steps; ++si) {
        bool even = (si % 2) == 0;
        pattern_samples += base_samples * 2.0 *
                           (even ? static_cast<double>(swing)
                                 : 1.0 - static_cast<double>(swing));
    }
    const int pattern_frames = static_cast<int>(pattern_samples);
    // 350 ms release tail so the last note's VEG decays to silence instead
    // of cutting off mid-note (which would click on playback).
    const int tail_frames    = static_cast<int>(0.35 * sr);
    const int total_frames   = pattern_frames + tail_frames;

    // ── WAV header (44-byte RIFF/PCM, 16-bit mono) ─────────────────────────
    auto write_u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto write_u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    uint32_t data_bytes = static_cast<uint32_t>(total_frames) * 2u;
    std::fwrite("RIFF", 1, 4, f);
    write_u32(36u + data_bytes);
    std::fwrite("WAVEfmt ", 1, 8, f);
    write_u32(16);                              // PCM fmt chunk size
    write_u16(1);                               // PCM
    write_u16(1);                               // mono
    write_u32(static_cast<uint32_t>(sr));
    write_u32(static_cast<uint32_t>(sr * 2));   // byte rate
    write_u16(2);                               // block align
    write_u16(16);                              // bits per sample
    std::fwrite("data", 1, 4, f);
    write_u32(data_bytes);

    // Reset engine state so every bounce starts from silence.
    acid::g_phase    = 0.0f;
    acid::g_osc_prev = 0.0f;
    acid::g_tpt_s[0] = acid::g_tpt_s[1] = acid::g_tpt_s[2] = acid::g_tpt_s[3] = 0.0f;
    acid::g_tpt_y4   = 0.0f;
    acid::g_meg = acid::g_veg = acid::g_acc_env = 0.0f;
    acid::g_veg_atk  = 1.0f;
    acid::g_fc_sm    = acid::cutoff_knob_to_hz(acid::g_params.cutoff.load());
    acid::g_res_sm   = acid::g_params.resonance.load() * 12.0f;
    acid::g_mvol_sm  = acid::g_params.master_vol.load();
    acid::g_drift_lp = 0.0f;
    acid::g_gate = false; acid::g_note_accented = false;
    acid::g_hpf_xprev = acid::g_hpf_yprev = 0.0f;
    acid::g_current_freq = acid::g_target_freq = 100.0f;
    acid::g_last_seq       = acid::g_cmd_seq.load(std::memory_order_acquire);
    acid::g_seq_was_playing = false;

    // Load the pattern into the audio-thread sequencer.
    for (int i = 0; i < pattern_length; ++i) {
        acid_seq_set_step(i, notes[i] & 0xFFFF, (notes[i] >> 16) & 0xFF);
    }
    acid_seq_set_pattern_length(pattern_length);
    acid_seq_set_bpm(bpm);
    acid_seq_set_swing(swing);
    acid_seq_play();

    // Render the main pattern — stop the seq exactly at the pattern boundary.
    // Loops are handled by just rendering enough frames; the sequencer wraps
    // on its own when it hits pattern_length.
    constexpr int kBlock = 256;
    std::vector<float>   buf(kBlock);
    std::vector<int16_t> out16(kBlock);

    int written = 0;
    while (written < pattern_frames) {
        int n = std::min(kBlock, pattern_frames - written);
        acid::render(buf.data(), n);
        for (int i = 0; i < n; ++i) {
            float s = std::clamp(buf[i], -1.0f, 1.0f);
            out16[static_cast<size_t>(i)] = static_cast<int16_t>(s * 32767.0f);
        }
        std::fwrite(out16.data(), 2, static_cast<size_t>(n), f);
        written += n;
    }

    // Release tail — stop the sequencer (triggers note_off on the next
    // render call) and drain the envelopes.
    acid_seq_stop();
    while (written < total_frames) {
        int n = std::min(kBlock, total_frames - written);
        acid::render(buf.data(), n);
        for (int i = 0; i < n; ++i) {
            float s = std::clamp(buf[i], -1.0f, 1.0f);
            out16[static_cast<size_t>(i)] = static_cast<int16_t>(s * 32767.0f);
        }
        std::fwrite(out16.data(), 2, static_cast<size_t>(n), f);
        written += n;
    }

    std::fclose(f);
    return 0;
}

}  // extern "C"
