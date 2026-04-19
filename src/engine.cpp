// TB-303-style synth DSP. Platform-agnostic: renders mono float32 at the rate
// set via `set_sample_rate()` into a caller-provided buffer.
//
// Signal chain (single voice):
//   PolyBLEP osc (saw/sq) ─► 4-pole ladder filter ─► VCA ─► tanh ─► HPF ─► out
//
// Roland TB-303 specifics that actually matter for the sound — informed by
// Stinchcombe's circuit analysis, Zavalishin's "Art of VA Filter Design",
// and the Robin Whittle / firstpr.com.au reverse engineering of the real
// hardware:
//
//   * Filter: 4-pole (24 dB/oct) with tanh in the feedback path, resonance
//     k mapped 0..18 (diode ladder self-oscillates ~k=17 vs. Moog's k=4).
//     Passband gain is NOT compensated — the volume drop at high resonance
//     is a signature 303 trait; compensating kills the character.
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
//   * Slide: exponential RC on the pitch CV, τ ≈ 88 ms (≈ R=400kΩ, C=0.22µF).
//     Constant-time — settles in ~4τ regardless of interval.
//   * VCA saturation: gentle tanh post-filter + 16 Hz HPF (matches the
//     output-coupling cap). Gives accents their "bite" without becoming a
//     distortion pedal.
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

namespace acid {
namespace {

// ── Parameters (UI-thread writes, audio-thread reads per block) ─────────────
struct Params {
    std::atomic<float> cutoff     {0.35f};  // 0..1 → 13..5000 Hz, log
    std::atomic<float> resonance  {0.78f};  // 0..1 → k = 0..18
    std::atomic<float> env_mod    {0.70f};  // 0..1 → 0..5 octaves of sweep
    std::atomic<float> decay      {0.40f};  // 0..1 → 0.2..2 s (MEG)
    std::atomic<float> accent_amt {0.55f};  // 0..1
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

// Oscillator
float g_phase = 0.0f;

// 4-pole ladder (Huovilainen-style with tanh in global feedback)
float g_fs[4] = {0.0f, 0.0f, 0.0f, 0.0f};
float g_fb    = 0.0f;          // feedback with 1-sample smoothing

// Envelopes (all exponential)
float g_meg     = 0.0f;        // filter envelope (0..1, triggered on note-on)
float g_veg     = 0.0f;        // amp envelope (0..1)
float g_acc_env = 0.0f;        // persistent accent cap voltage (0..1)

// Latched state of current held note
bool  g_gate          = false;
bool  g_note_accented = false;

// Output-stage DC blocker
float g_hpf_xprev = 0.0f;
float g_hpf_yprev = 0.0f;

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

}  // namespace

void set_sample_rate(int sr) { g_sample_rate = sr > 0 ? sr : kDefaultSampleRate; }
int  sample_rate()           { return g_sample_rate; }

void render(float* out, int frames) {
    const float sr    = static_cast<float>(g_sample_rate);
    const float dt_s  = 1.0f / sr;

    // ── Commit any pending note command ─────────────────────────────────────
    uint64_t seq = g_cmd_seq.load(std::memory_order_acquire);
    if (seq != g_last_seq) {
        g_last_seq      = seq;
        uint32_t flags  = g_cmd_flags.load(std::memory_order_relaxed);
        bool on         = (flags & 0b001);
        bool accent     = (flags & 0b010);
        bool slide      = (flags & 0b100);
        float freq      = g_cmd_freq.load(std::memory_order_relaxed);

        if (on) {
            g_target_freq = freq * std::pow(2.0f, g_params.tuning.load() / 12.0f);
            // Slide semantics (stock 303): the previous note's gate is still
            // held when the new note fires, so envelopes DON'T retrigger —
            // they continue decaying from wherever they were, and only the
            // pitch CV glides to the new target. Without this the "rising
            // squelch" on slide chains gets interrupted on every step.
            const bool is_slide = slide && g_current_freq >= 10.0f;
            if (!is_slide) {
                g_current_freq = g_target_freq;
                g_phase        = 0.0f;
                g_meg          = 1.0f;
                g_veg          = 1.0f;
                g_gate         = true;
            }
            g_note_accented = accent;

            // Accent cap: add to existing charge (does not fully discharge
            // between fast consecutive accents → rising squelch). One hit at
            // full accent-amt should leave plenty of headroom so that two or
            // three stacked accents keep climbing — 0.45 was measured off the
            // real C13 voltage step in Stinchcombe's notes.
            if (accent) {
                float acc = g_params.accent_amt.load();
                g_acc_env = std::min(1.0f, g_acc_env + 0.45f * acc);
            }
        } else {
            // Note-off: gate down, amp envelope will decay fast below.
            g_gate = false;
        }
    }

    // ── Block-rate parameter snapshot ───────────────────────────────────────
    const float fc_knob  = g_params.cutoff.load();
    const float res_knob = g_params.resonance.load();
    const float envmod_k = g_params.env_mod.load();
    const float decay_k  = g_params.decay.load();
    const float acc_amt  = g_params.accent_amt.load();
    const int   wf       = g_params.waveform.load();
    const float master   = g_params.master_vol.load();

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

    // Base cutoff in Hz
    const float fc_base  = cutoff_knob_to_hz(fc_knob);

    // Precomputed duty cycle (doesn't need to track micro pitch changes)
    const float duty     = square_duty_for_hz(g_current_freq);

    // DC-blocker coefficient: one-pole HPF at ~16 Hz.
    const float hpf_a    = std::exp(-2.0f * static_cast<float>(M_PI) * 16.0f / sr);

    // ── Sample loop ─────────────────────────────────────────────────────────
    for (int i = 0; i < frames; i++) {
        // Slide: exponential approach to target pitch (single-pole LPF)
        g_current_freq += slide_coef * (g_target_freq - g_current_freq);

        // Oscillator
        const float phase_inc = g_current_freq / sr;
        g_phase += phase_inc;
        if (g_phase >= 1.0f) g_phase -= 1.0f;

        float osc;
        if (wf == 0) {
            // Band-limited saw: naive ramp minus PolyBLEP at the wrap.
            float naive = 2.0f * g_phase - 1.0f;
            osc = naive - poly_blep(g_phase, phase_inc);
        } else {
            // Band-limited square with pitch-dependent duty.
            osc = (g_phase < duty) ? 1.0f : -1.0f;
            osc += poly_blep(g_phase, phase_inc);                // rising edge
            float tf = g_phase - duty;
            if (tf < 0.0f) tf += 1.0f;
            osc -= poly_blep(tf, phase_inc);                     // falling edge
        }

        // Envelope updates (exponential decay — multiply by pre-computed factor)
        g_meg     *= meg_dec;
        g_veg     *= veg_dec;
        g_acc_env *= acc_dec;

        // Exponential cutoff modulation (V/oct). MEG rides the ENV MOD knob;
        // accent is on its own fixed-range CV path (resonance-coupled).
        float oct_shift = env_oct * g_meg + acc_oct_max * acc_coup * g_acc_env;
        float fc_hz     = fc_base * std::exp2(oct_shift);
        // Hard-ceiling at ~10 kHz: even a real 303 doesn't meaningfully go
        // higher, and letting resonance track up to Nyquist just aliases into
        // a pure whine.
        fc_hz = std::clamp(fc_hz, 20.0f, std::min(10000.0f, 0.45f * sr));

        // 4-pole ladder: cascade of 1-pole LPFs with global tanh feedback.
        // `g` is the impulse-invariant 1-pole LPF coefficient — matches the
        // analog LPF exactly at low fc and stays stable up to Nyquist.
        const float g  = 1.0f - std::exp(-2.0f * static_cast<float>(M_PI) * fc_hz / sr);
        float u        = std::tanh(osc - k_res * g_fb);
        g_fs[0] += g * (u       - g_fs[0]);
        g_fs[1] += g * (g_fs[0] - g_fs[1]);
        g_fs[2] += g * (g_fs[1] - g_fs[2]);
        g_fs[3] += g * (g_fs[2] - g_fs[3]);
        // Light 1-sample smoothing on the feedback path. Heavier averaging
        // kills the resonance bite; 0.8/0.2 is just enough to calm aliasing
        // at self-oscillation without damping the squelch.
        g_fb = 0.8f * g_fs[3] + 0.2f * g_fb;

        // VCA: VEG × (accent boost on accented notes) × master
        float amp = g_veg;
        if (g_note_accented) amp *= (1.0f + acc_amt);
        amp *= master;

        float y = g_fs[3] * amp;

        // Post-VCA soft clip — the real BA662 VCA starts compressing before
        // the rail; tanh with modest drive captures the "bite" on accents.
        y = std::tanh(y * 1.3f);

        // 16 Hz single-pole HPF (matches the output-coupling cap, also
        // removes any DC drift from the filter at low cutoffs).
        float y_hpf = y - g_hpf_xprev + hpf_a * g_hpf_yprev;
        g_hpf_xprev = y;
        g_hpf_yprev = y_hpf;

        out[i] = y_hpf;
    }
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

}  // extern "C"
