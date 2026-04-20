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
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
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

    // Delay: tempo-synced tape-style. Division picks the beat subdivision
    // (1/16 / 1/16d / 1/8 / 1/8d). Time is derived from g_seq_bpm so the
    // repeats lock to the sequencer even as BPM changes.
    std::atomic<float> delay_mix     {0.0f};   // 0..1 wet send (0 = bypass)
    std::atomic<float> delay_feedback{0.35f};  // 0..1 — clamped < 1 to stay stable
    std::atomic<int>   delay_division{2};      // 0=1/16, 1=1/16d, 2=1/8, 3=1/8d

    // Overdrive: pre-filter soft-clip, sits between the oscillator and the
    // ladder input. Sweeps from transparent to "303 into a Big Muff".
    std::atomic<float> od_amt        {0.0f};

    // Plate reverb: Schroeder-style (4 combs + 2 allpasses). Size scales the
    // comb feedback (= decay time); damp rolls off the HF inside the comb
    // feedback path so longer tails darken like a real plate.
    std::atomic<float> rev_mix       {0.0f};
    std::atomic<float> rev_size      {0.55f};
    std::atomic<float> rev_damp      {0.35f};
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

// ── Delay line ──────────────────────────────────────────────────────────────
// Mono tape-style delay in the master bus. Ring buffer sized for the slowest
// tempo we allow (20 BPM @ 1/8 dotted = 0.9 s, well under 2 s). Fractional
// read via linear interpolation so BPM changes don't click. A one-pole LPF
// inside the feedback path gives each repeat its characteristic tape darken
// and keeps runaway feedback from screeching into self-oscillation.
constexpr int kDelayBufSize = 96000;       // ~2 s at 48 kHz
float g_delay_buf[kDelayBufSize] = {};
int   g_delay_w = 0;
float g_delay_fb_lp = 0.0f;                // one-pole LPF state inside FB path
float g_delay_time_sm = 0.0f;              // smoothed delay time (samples)

// ── Plate reverb (Schroeder / Freeverb-lite) ────────────────────────────────
// 4 parallel comb filters feed 2 serial allpass filters. Comb delays are the
// original Freeverb prime-ish values scaled to the runtime sample rate so the
// density stays constant across 44.1/48 kHz. A one-pole LPF inside each comb
// feedback path gives the "darkens as it decays" plate/room behaviour.
// Totals ~7 KB of buffer state — nothing.
constexpr int kRevCombs    = 4;
constexpr int kRevApass    = 2;
constexpr int kRevCombMax  = 2048;         // worst-case samples at 48 kHz
constexpr int kRevApassMax = 768;
// Freeverb-style comb delays (samples @ 44.1 kHz). Scaled at set_sample_rate.
constexpr int kRevCombBase[kRevCombs] = {1116, 1277, 1422, 1557};
constexpr int kRevApassBase[kRevApass] = {556, 441};
float g_rev_comb_buf[kRevCombs][kRevCombMax]   = {};
float g_rev_apass_buf[kRevApass][kRevApassMax] = {};
int   g_rev_comb_w[kRevCombs]    = {};
int   g_rev_apass_w[kRevApass]   = {};
int   g_rev_comb_len[kRevCombs]  = {1116, 1277, 1422, 1557};
int   g_rev_apass_len[kRevApass] = {556, 441};
float g_rev_comb_lp[kRevCombs]   = {};      // one-pole LPF state per comb

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

// ── Live recorder ───────────────────────────────────────────────────────────
// Captures the raw post-FX output (the same y_out the backend writes to the
// device) into a preallocated heap buffer while `g_rec_on` is true. The UI
// allocates the buffer up-front, flips the flag on, and later flips it off +
// writes a WAV. We never allocate on the audio thread, and we only free on
// the UI side after the flag has been off for >1 block. `g_rec_overflow`
// latches if the buffer fills mid-take so the UI can warn the user.
float*                g_rec_buf      = nullptr;
std::atomic<uint32_t> g_rec_cap      {0};
std::atomic<uint32_t> g_rec_w        {0};
std::atomic<bool>     g_rec_on       {false};
std::atomic<bool>     g_rec_overflow {false};

// ── Audio-thread sequencer ──────────────────────────────────────────────────
// Live step scheduling is done on the audio thread against a sample-accurate
// clock. The UI used to drive it from the 30 FPS render callback, which
// quantised every note-on to ~33 ms and made the live version audibly looser
// than the offline WAV bounce. Now both paths share this scheduler, so live
// matches export to the sample.
//
// UI-shared atomics: pattern (packed per slot), length, bpm, swing, playing
// flag, plus published current step + within-step phase for beat-sync
// animation.
//
// Step encoding (single u32 per slot — single atomic store, no tearing):
//   bits  0-7   midi (0..127)
//   bits  8-10  flags  (rest=1, accent=2, slide=4)
//   bits 11-17  prob   (0..100, 7 bits)
//   bits 18-19  ratchet-1 (0..3 → 1..4 sub-triggers)
//
// Audio-thread locals (g_seq_clock etc.) are only touched inside render().
constexpr int kSeqMaxSteps = 16;
std::atomic<uint32_t> g_seq_steps[kSeqMaxSteps]  {};
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

// Ratchet sub-triggers: when a step has ratchet > 1 we re-fire the same
// (midi, accent) at regular sub-intervals across the step's duration. First
// hit fires on the step boundary; the remainder are counted here and
// commit when the step clock crosses `g_seq_rat_next`. Ratchets always
// retrigger envelopes (no slide) — otherwise the repeats are inaudible
// during the glide.
int     g_seq_rat_left = 0;             // remaining sub-triggers for current step
double  g_seq_rat_next = 0.0;           // next sub-trigger's clock sample
double  g_seq_rat_inc  = 0.0;           // samples between sub-triggers
int     g_seq_rat_midi = 0;
bool    g_seq_rat_acc  = false;

// Probability RNG — xorshift32, deterministic enough for "rolls the dice
// once per step" without pulling libc. Seeded from a non-zero constant so
// fresh engines agree on the first few rolls (useful for offline bounces
// where reproducibility matters).
uint32_t g_seq_rng = 0x9E3779B9u;

// Per-step parameter locks (Elektron-style p-locks). `mask` bits:
//   1=cutoff, 2=res, 4=env, 8=accent. When a bit is set, the matching
// `g_seq_lock_*` value replaces the global knob for the duration of that
// step. Values are normalised 0..1 (knob-native). Single atomic per
// parameter per step; UI writes each setter as one store, audio reads
// relaxed per sub-block.
std::atomic<uint32_t> g_seq_lock_mask[kSeqMaxSteps] {};
std::atomic<float>    g_seq_lock_cut[kSeqMaxSteps]  {};
std::atomic<float>    g_seq_lock_res[kSeqMaxSteps]  {};
std::atomic<float>    g_seq_lock_env[kSeqMaxSteps]  {};
std::atomic<float>    g_seq_lock_acc[kSeqMaxSteps]  {};
inline uint32_t seq_rand_u32() {
    uint32_t s = g_seq_rng;
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    g_seq_rng = s;
    return s;
}

// ── Drum machine (Phase 4.3) ────────────────────────────────────────────────
// Nine synthesized percussion voices — BD / SD / CH / OH / CL / LT / HT / RS /
// CB — each with its own 16-step bitmask lane. Triggers fire on step
// boundaries alongside the bass voice. Voices render in parallel per sample
// and sum into the master bus BEFORE the delay so they hit the same FX the
// bass does.
//
// Voices 0..8 are the legacy kit (kept at same indices so v4 slot files and
// v3/v4 pattern.txt still load). 9..11 extend with shaker/tambourine/conga
// (the texture layer). 12..15 add a mid tom plus two cymbals and a bongo so
// the kit covers the full drum-family triangle (kicks/toms/perc, snares/claps,
// hats/cymbals):
//   SH — continuous 16th-note shuffle (bandpassed noise, short tail)
//   TB — metallic jangle that sits on offbeats (noise + high bandpass)
//   CG — pitched mid-perc that fills between kick and snare
//   MT — mid tom (between LT and HT) so tom fills have a 3-step ladder
//   CY — crash cymbal (long, noisy, bar-top accent on step 1 / 13)
//   RD — ride cymbal (medium, pings on top, drives techno/house at 8ths)
//   BG — bongo (higher-pitched than CG, fast decay, latin/jam fills)
constexpr int kDrumVoices = 16;
constexpr int kDrumBD = 0, kDrumSD = 1, kDrumCH = 2, kDrumOH = 3, kDrumCL = 4;
constexpr int kDrumLT = 5, kDrumHT = 6, kDrumRS = 7, kDrumCB = 8;
constexpr int kDrumSH = 9, kDrumTB = 10, kDrumCG = 11;
constexpr int kDrumMT = 12, kDrumCY = 13, kDrumRD = 14, kDrumBG = 15;

std::atomic<uint32_t> g_seq_drum_mask[kDrumVoices] {};
// Per-voice baseline gain — picked so a four-on-the-floor BD + off-beat CH
// sits roughly at the bass voice's level with DRUM knob at 1.0. Toms sit a
// touch under the kick so layered hits feel like accent rather than two-kick
// mud; rimshot is low by default (it's a transient, easy to over-weight); CB
// is low because a metallic square stacks hard into the mix. SH/TB stay
// quiet so they colour the groove without muddying, CG sits between toms and
// rim.
std::atomic<float> g_seq_drum_gain[kDrumVoices] {
    {0.95f}, {0.70f}, {0.35f}, {0.40f}, {0.60f},
    {0.80f}, {0.75f}, {0.55f}, {0.45f},
    {0.30f}, {0.35f}, {0.65f},
    // MT sits between the two existing toms (LT 0.80 / HT 0.75). CY is a
    // once-per-bar accent so the baseline leans loud; RD sits under the CH
    // level so a steady ride doesn't ride over the kick; BG sits a touch above
    // CG because bongos historically ride on top of the conga.
    {0.78f}, {0.50f}, {0.32f}, {0.55f}
};
// Master drum-bus send. UI knob; 0 = drums silent, 1 = nominal.
std::atomic<float> g_seq_drum_master {1.0f};

// Per-voice audio-thread synth state. Plain floats (no atomics — only the
// render thread touches these). All the state is a few floats per voice so
// the whole drum machine is < 100 B, negligible cache-wise.
struct DrumBD {
    float env     = 0.0f;    // amp envelope (decays ~0.20 s)
    float penv    = 0.0f;    // pitch envelope (sweeps ~0.050 s)
    float click   = 0.0f;    // attack transient (~2 ms)
    float phase   = 0.0f;    // sine phase
};
struct DrumSD {
    float env        = 0.0f;    // amp envelope
    float body_env   = 0.0f;    // body tone decay (~35 ms)
    float body_phase1 = 0.0f;   // low body sine (~180 Hz)
    float body_phase2 = 0.0f;   // high body sine (~330 Hz)
    float noise_env   = 0.0f;   // noise component decay (~120 ms)
    float bp_x1 = 0.0f, bp_y1 = 0.0f;   // snare bandpass state 1
    float bp_x2 = 0.0f, bp_y2 = 0.0f;   // bandpass state 2
    uint32_t rng     = 0xc3a5c85cu;
};
// Metallic hat oscillator — 6 detuned squares at inharmonic ratios, the
// classic 808 recipe. Each voice carries its own phase bank so CH and OH
// stay out of sync even when both fire on the same step.
struct DrumHat {
    float env    = 0.0f;
    float bp_x   = 0.0f, bp_y = 0.0f;      // bandpass (resonant ~8 kHz)
    float hp_x   = 0.0f, hp_y = 0.0f;      // output HPF removes low mud
    float phase[6] = {0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f};  // square phases
    uint32_t rng;
};
struct DrumCL {
    float env         = 0.0f;       // per-burst fast env (~7 ms)
    float tail_env    = 0.0f;       // long tail (~280 ms)
    int   bursts_left = 0;
    float burst_clock = 0.0f;       // seconds until next retrigger
    float bp_x        = 0.0f;       // bandpass ~1.3 kHz for hand-clap body
    float bp_y        = 0.0f;
    uint32_t rng      = 0x52a43d26u;
};
// Tom (LT / HT) — pitched sine with small pitch envelope. Shares structure
// with the kick but with a longer amp decay and narrower pitch sweep.
struct DrumTom {
    float env     = 0.0f;
    float penv    = 0.0f;
    float phase   = 0.0f;
    float base_hz = 80.0f;      // set per-instance (LT = 80, HT = 180)
    float peak_hz = 110.0f;     //                  (LT = 110, HT = 230)
    float env_tc  = 0.30f;      //                  (LT = 0.30s, HT = 0.22s)
    float penv_tc = 0.07f;
};
// Rimshot — sharp click: two sines + a short noise burst through a high
// bandpass. All the energy lives in 1.5–2.5 kHz.
struct DrumRS {
    float env         = 0.0f;
    float sine1_phase = 0.0f;
    float sine2_phase = 0.0f;
    float bp_x        = 0.0f;
    float bp_y        = 0.0f;
    uint32_t rng      = 0x85ebca6bu;
};
// 808 cowbell — two detuned squares at 540 + 800 Hz through a bandpass.
// Short-medium decay (~0.18s) gives the classic "clonk". Phases seeded apart
// so the first cycle has the right interference pattern.
struct DrumCB {
    float env     = 0.0f;
    float phase1  = 0.0f;
    float phase2  = 0.35f;
    float bp_x    = 0.0f;
    float bp_y    = 0.0f;
};
// Shaker — short pink-ish noise burst with a quick attack curve + gentle
// bandpass near 4 kHz. Very short decay (~60 ms) makes a 16th-note shaker
// pattern feel driving without smearing.
struct DrumSH {
    float env    = 0.0f;
    float bp_x   = 0.0f, bp_y = 0.0f;
    float hp_x   = 0.0f, hp_y = 0.0f;
    uint32_t rng = 0x9e3779b9u;
};
// Tambourine — noise through two bandpasses (one around 4 kHz for body, one
// around 8 kHz for jingle), a touch longer than the shaker (~140 ms) so the
// sizzle sits on top of the groove rather than vanishing instantly.
struct DrumTB {
    float env    = 0.0f;
    float bp1_x  = 0.0f, bp1_y = 0.0f;   // body ~4 kHz
    float bp2_x  = 0.0f, bp2_y = 0.0f;   // jingle ~8 kHz
    uint32_t rng = 0x7f4a7c15u;
};
// Conga — pitched sine with a brief pitch envelope and tiny click transient.
// Sits above the toms (base 260 Hz, peak 340 Hz) so it reads as a mid-perc
// "tuk" rather than a drum, ideal for latin/techno fills.
struct DrumCG {
    float env     = 0.0f;
    float penv    = 0.0f;
    float phase   = 0.0f;
};
// Cymbal — longer cousin of the 808 hat. Same 6-square metallic stack but a
// wider bandpass and a longer tail, voiced per-instance for crash vs ride.
//   CY (crash): ~0.80s decay, open bandpass, heavy noise component.
//   RD (ride):  ~0.45s decay, tighter bandpass + a sine "ping" on attack.
struct DrumCym {
    float env      = 0.0f;
    float bp_x     = 0.0f, bp_y  = 0.0f;
    float hp_x     = 0.0f, hp_y  = 0.0f;
    float phase[6] = {0.0f, 0.13f, 0.27f, 0.41f, 0.55f, 0.69f};
    float ping     = 0.0f;        // attack sine envelope (ride only)
    float ping_ph  = 0.0f;
    // Per-instance recipe. CY uses 6 inharmonic squares (same as the hat but
    // octave-down so the lot sits in 400-3k), RD uses 6 above the hat so the
    // ping is bright. Exposed so the render lambda can read them.
    float ping_hz  = 0.0f;        // 0 = disabled (crash); RD sets ~420 Hz
    uint32_t rng   = 0x13578bdfu;
};
// Bongo — pitched sine like the conga but tuned higher (380→470 Hz) and with
// a faster decay (~90 ms). Sits above the conga so a bongo/conga pair reads
// as two separate pitches.
struct DrumBongo {
    float env     = 0.0f;
    float penv    = 0.0f;
    float phase   = 0.0f;
};

DrumBD g_bd;
DrumSD g_sd;
DrumHat g_ch{ .rng = 0x1b873593u };
DrumHat g_oh{ .rng = 0xcc9e2d51u };
DrumCL g_cl;
DrumTom g_lt{ .base_hz = 80.0f,  .peak_hz = 110.0f, .env_tc = 0.30f, .penv_tc = 0.080f };
DrumTom g_ht{ .base_hz = 180.0f, .peak_hz = 230.0f, .env_tc = 0.22f, .penv_tc = 0.060f };
DrumRS  g_rs;
DrumCB  g_cb;
DrumSH  g_sh;
DrumTB  g_tb;
DrumCG  g_cg;
// New: mid tom fills the octave gap between LT (80 Hz) and HT (180 Hz) so the
// three toms read as a proper low→high ladder when a tom fill walks up.
DrumTom g_mt{ .base_hz = 128.0f, .peak_hz = 160.0f, .env_tc = 0.26f, .penv_tc = 0.070f };
DrumCym g_cy{ .ping_hz = 0.0f,    .rng = 0x9d2c5680u };
DrumCym g_rd{ .ping_hz = 420.0f,  .rng = 0xefc60000u };
DrumBongo g_bg;

inline void drum_trig_bd() {
    g_bd.env = 1.0f; g_bd.penv = 1.0f; g_bd.click = 1.0f; g_bd.phase = 0.0f;
}
inline void drum_trig_sd() {
    g_sd.env = 1.0f;
    g_sd.body_env = 1.0f;
    g_sd.noise_env = 1.0f;
    g_sd.body_phase1 = 0.0f;
    g_sd.body_phase2 = 0.0f;
}
inline void drum_trig_ch() { g_ch.env = 1.0f; }
inline void drum_trig_oh() { g_oh.env = 1.0f; }
inline void drum_trig_cl() {
    // Classic 808 clap: 3 fast noise bursts spaced ~12 ms apart, plus a
    // longer dense-noise tail. First burst fires now; the other 2 fire as
    // the burst_clock counts down in the render loop.
    g_cl.env         = 1.0f;
    g_cl.tail_env    = 1.0f;
    g_cl.bursts_left = 2;
    g_cl.burst_clock = 0.012f;
}
inline void drum_trig_tom(DrumTom& t) {
    t.env = 1.0f; t.penv = 1.0f; t.phase = 0.0f;
}
inline void drum_trig_rs() {
    g_rs.env = 1.0f;
    g_rs.sine1_phase = 0.0f;
    g_rs.sine2_phase = 0.25f;
}
inline void drum_trig_cb() {
    g_cb.env = 1.0f;
    // Don't reset phases — seeded offset gives the signature cowbell interference.
}
inline void drum_trig_sh() { g_sh.env = 1.0f; }
inline void drum_trig_tb() { g_tb.env = 1.0f; }
inline void drum_trig_cg() {
    g_cg.env = 1.0f; g_cg.penv = 1.0f; g_cg.phase = 0.0f;
}
inline void drum_trig_cym(DrumCym& c) {
    c.env = 1.0f;
    if (c.ping_hz > 0.0f) { c.ping = 1.0f; c.ping_ph = 0.0f; }
}
inline void drum_trig_bg() {
    g_bg.env = 1.0f; g_bg.penv = 1.0f; g_bg.phase = 0.0f;
}
inline void drum_fire(int voice) {
    switch (voice) {
        case kDrumBD: drum_trig_bd(); break;
        case kDrumSD: drum_trig_sd(); break;
        case kDrumCH: drum_trig_ch(); break;
        case kDrumOH: drum_trig_oh(); break;
        case kDrumCL: drum_trig_cl(); break;
        case kDrumLT: drum_trig_tom(g_lt); break;
        case kDrumHT: drum_trig_tom(g_ht); break;
        case kDrumRS: drum_trig_rs(); break;
        case kDrumCB: drum_trig_cb(); break;
        case kDrumSH: drum_trig_sh(); break;
        case kDrumTB: drum_trig_tb(); break;
        case kDrumCG: drum_trig_cg(); break;
        case kDrumMT: drum_trig_tom(g_mt); break;
        case kDrumCY: drum_trig_cym(g_cy); break;
        case kDrumRD: drum_trig_cym(g_rd); break;
        case kDrumBG: drum_trig_bg(); break;
    }
}

// ── MIDI event ring (Phase 5) ───────────────────────────────────────────────
// Audio thread pushes note-on/off messages here whenever the sequencer fires
// a bass note or a drum hit. The MIDI backend thread drains the ring and
// forwards the messages out of its ALSA sequencer port. SPSC, power-of-two
// size so the mask is a cheap AND; drops on overrun rather than blocking the
// audio thread (overruns require >64 pending events which only happens if the
// MIDI thread has stalled).
constexpr int kMidiRingSize = 256;
struct MidiEvt {
    uint8_t type;     // 0=none, 1=note_on, 2=note_off
    uint8_t channel;  // 0..15
    uint8_t data1;    // note number
    uint8_t data2;    // velocity
};
MidiEvt              g_midi_ring[kMidiRingSize] {};
std::atomic<uint32_t> g_midi_w {0};
std::atomic<uint32_t> g_midi_r {0};
// When false, midi_push is a no-op — keeps the ring cold for users who never
// enable MIDI so the audio thread does zero extra work.
std::atomic<bool>    g_midi_out_enabled {false};
// MIDI channels for bass / drums. Bass on 1 (index 0), drums on 10 (index 9
// — GM convention). Most DAWs and hardware expect this layout.
constexpr uint8_t    kMidiBassChan = 0;
constexpr uint8_t    kMidiDrumChan = 9;
// GM drum mapping for our 9 voices. BD=35 (acoustic bass drum), SD=38 (snare),
// CH=42 (closed hat), OH=46 (open hat), CL=39 (hand clap), LT=41 (low floor
// tom), HT=48 (hi mid tom), RS=37 (side stick / rim), CB=56 (cowbell). Stock
// enough that a plugged-in drum module or a GM softsynth will just play the
// right sound without remapping.
// GM note mapping per voice. Extras: SH=70 (maracas), TB=54 (tambourine),
// CG=63 (high conga). v6 kit adds MT=47 (low-mid tom), CY=49 (crash cymbal 1),
// RD=51 (ride cymbal 1), BG=60 (hi bongo).
constexpr uint8_t    kDrumMidiNote[16] = {
    35, 38, 42, 46, 39, 41, 48, 37, 56, 70, 54, 63,
    47, 49, 51, 60
};
// Track the currently-gated bass note so the audio thread can emit a matching
// note-off before the next note-on (otherwise external synths stack voices).
uint8_t              g_midi_last_bass_note = 0;

inline void midi_push(uint8_t type, uint8_t ch, uint8_t d1, uint8_t d2) {
    if (!g_midi_out_enabled.load(std::memory_order_relaxed)) return;
    uint32_t w = g_midi_w.load(std::memory_order_relaxed);
    uint32_t r = g_midi_r.load(std::memory_order_acquire);
    if ((w - r) >= static_cast<uint32_t>(kMidiRingSize)) return;
    g_midi_ring[w & (kMidiRingSize - 1)] = { type, ch, d1, d2 };
    g_midi_w.store(w + 1, std::memory_order_release);
}

// Clock-in state (external-tempo sync). When slaved, the backend thread calls
// acid_midi_feed_clock() once per incoming 0xF8 pulse; we average the
// inter-pulse interval across 24 pulses (= one beat) and set g_seq_bpm.
std::atomic<bool>  g_midi_sync_in  {false};
// BPM-averaging ring: 24 recent inter-pulse deltas in seconds. Running sum
// kept for cheap running mean.
constexpr int      kMidiClockWindow = 24;
float              g_midi_clock_delta[kMidiClockWindow] {};
int                g_midi_clock_i      = 0;
int                g_midi_clock_filled = 0;
double             g_midi_clock_sum    = 0.0;

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

void set_sample_rate(int sr) {
    g_sample_rate = sr > 0 ? sr : kDefaultSampleRate;
    // Scale Freeverb delays from the reference 44.1 kHz to the runtime SR so
    // the reverb's resonant density sounds the same at 48 kHz.
    const double scale = static_cast<double>(g_sample_rate) / 44100.0;
    for (int i = 0; i < kRevCombs; ++i) {
        int len = static_cast<int>(std::round(kRevCombBase[i] * scale));
        g_rev_comb_len[i] = std::clamp(len, 16, kRevCombMax);
    }
    for (int i = 0; i < kRevApass; ++i) {
        int len = static_cast<int>(std::round(kRevApassBase[i] * scale));
        g_rev_apass_len[i] = std::clamp(len, 16, kRevApassMax);
    }
}
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
        g_seq_rat_left  = 0;
    } else if (!sp_on && g_seq_was_playing) {
        // Falling edge: release the gate and park.
        note_commit_off();
        if (g_midi_last_bass_note) {
            midi_push(2, kMidiBassChan, g_midi_last_bass_note, 64);
            g_midi_last_bass_note = 0;
        }
        g_seq_step = -1;
        g_seq_rat_left = 0;
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
        // Advance sequencer across any step boundary OR ratchet sub-trigger
        // that is already due. Each iteration fires at most one event, so two
        // events on the same sample simply resolve in separate passes.
        if (sp_on) {
            while (true) {
                // Step boundary first — a brand-new step can supersede a
                // pending ratchet from the previous step (its sub-triggers
                // never bleed past the step they were scheduled in).
                double cur_dur = (g_seq_step < 0) ? 0.0 : step_samples(g_seq_step);
                if (g_seq_clock >= cur_dur) {
                    g_seq_clock -= cur_dur;
                    g_seq_step   = (g_seq_step + 1) % plen;
                    uint32_t enc = g_seq_steps[g_seq_step].load(std::memory_order_relaxed);
                    int  midi  = static_cast<int>(enc & 0xFF);
                    int  flg   = static_cast<int>((enc >> 8) & 0x7);
                    int  prob  = static_cast<int>((enc >> 11) & 0x7F);
                    int  rat   = static_cast<int>((enc >> 18) & 0x3) + 1;
                    bool rest  = (flg & 1) != 0;
                    bool acc   = (flg & 2) != 0;
                    bool sld   = (flg & 4) != 0;

                    // Probability roll. 100 always plays; 0 never plays; in
                    // between, xorshift RNG mod 100 beats `prob`.
                    bool rolled_out = false;
                    if (prob < 100) {
                        uint32_t r = seq_rand_u32() % 100u;
                        rolled_out = (static_cast<int>(r) >= prob);
                    }

                    g_seq_rat_left = 0;
                    if (!rest && !rolled_out) {
                        float freq = 440.0f * std::pow(2.0f,
                                     static_cast<float>(midi - 69) / 12.0f);
                        // Slide only when previous step played — can't glide
                        // from a rest.
                        bool slide_now = sld && !g_seq_prev_rest;
                        note_commit_on(freq, acc, slide_now);
                        // MIDI out: end previous note (unless slide — let the
                        // receiver do its own legato handling) and start this
                        // one. Accent → higher velocity (127 vs 96).
                        if (g_midi_last_bass_note && !slide_now) {
                            midi_push(2, kMidiBassChan, g_midi_last_bass_note, 64);
                        }
                        uint8_t mv = static_cast<uint8_t>(
                            std::clamp(midi, 0, 127));
                        midi_push(1, kMidiBassChan, mv, acc ? 127 : 96);
                        g_midi_last_bass_note = mv;
                        if (rat > 1) {
                            g_seq_rat_left = rat - 1;
                            g_seq_rat_inc  = step_samples(g_seq_step)
                                           / static_cast<double>(rat);
                            g_seq_rat_next = g_seq_rat_inc;
                            g_seq_rat_midi = midi;
                            g_seq_rat_acc  = acc;
                        }
                        g_seq_prev_rest = false;
                    } else {
                        // Rest (or rolled-out). Mark so the next step can't
                        // slide into silence.
                        if (g_midi_last_bass_note) {
                            midi_push(2, kMidiBassChan, g_midi_last_bass_note, 64);
                            g_midi_last_bass_note = 0;
                        }
                        g_seq_prev_rest = true;
                    }
                    g_seq_current_step.store(g_seq_step, std::memory_order_relaxed);
                    // Drum lanes: each voice's 16-bit mask picks which of the
                    // 16 steps fire. Drums don't use probability or ratchet —
                    // they're a fixed grid; that's the whole draw of a TR-
                    // style sequencer. (Can layer per-lane prob later if we
                    // miss hardware-style generative drums.)
                    {
                        const uint32_t bit = 1u << g_seq_step;
                        for (int v = 0; v < kDrumVoices; ++v) {
                            uint32_t m = g_seq_drum_mask[v].load(std::memory_order_relaxed);
                            if (m & bit) {
                                drum_fire(v);
                                // MIDI out: short note-on/off pair on ch 10.
                                // Drum hits are one-shots so we emit the off
                                // immediately; receivers treat it as a trigger.
                                midi_push(1, kMidiDrumChan, kDrumMidiNote[v], 112);
                                midi_push(2, kMidiDrumChan, kDrumMidiNote[v], 64);
                            }
                        }
                    }
                    continue;
                }
                // Ratchet sub-trigger boundary — re-fire same pitch. Always
                // retrigger (no slide) so the repeat is audible.
                if (g_seq_rat_left > 0 && g_seq_clock >= g_seq_rat_next) {
                    float freq = 440.0f * std::pow(2.0f,
                                 static_cast<float>(g_seq_rat_midi - 69) / 12.0f);
                    note_commit_on(freq, g_seq_rat_acc, /*slide=*/false);
                    // Ratchet MIDI: retrigger the same note. Emit off/on pair.
                    if (g_midi_last_bass_note) {
                        midi_push(2, kMidiBassChan, g_midi_last_bass_note, 64);
                    }
                    uint8_t mv = static_cast<uint8_t>(
                        std::clamp(g_seq_rat_midi, 0, 127));
                    midi_push(1, kMidiBassChan, mv, g_seq_rat_acc ? 127 : 96);
                    g_midi_last_bass_note = mv;
                    g_seq_rat_left -= 1;
                    g_seq_rat_next += g_seq_rat_inc;
                    continue;
                }
                break;
            }
        }

        // How many frames until the next step boundary or ratchet sub-trigger?
        int n;
        if (sp_on && g_seq_step >= 0) {
            double remaining = step_samples(g_seq_step) - g_seq_clock;
            if (g_seq_rat_left > 0) {
                double rr = g_seq_rat_next - g_seq_clock;
                if (rr > 0.0 && rr < remaining) remaining = rr;
            }
            int fu = (remaining <= 1.0) ? 1 : static_cast<int>(remaining);
            n = std::min(frames_left, fu);
        } else {
            n = frames_left;
        }

        // ── Block-rate parameter snapshot (per sub-block) ───────────────────
    // Per-step parameter locks: if the currently-playing step has a lock
    // bit set, use the step's override instead of the global knob. Since a
    // sub-block never straddles a step boundary, the override stays valid
    // for the whole window.
    uint32_t lk_m = 0;
    if (sp_on && g_seq_step >= 0) {
        lk_m = g_seq_lock_mask[g_seq_step].load(std::memory_order_relaxed);
    }
    const float fc_knob  = (lk_m & 1)
        ? g_seq_lock_cut[g_seq_step].load(std::memory_order_relaxed)
        : g_params.cutoff.load();
    const float res_knob = (lk_m & 2)
        ? g_seq_lock_res[g_seq_step].load(std::memory_order_relaxed)
        : g_params.resonance.load();
    const float envmod_k = (lk_m & 4)
        ? g_seq_lock_env[g_seq_step].load(std::memory_order_relaxed)
        : g_params.env_mod.load();
    const float acc_amt  = (lk_m & 8)
        ? g_seq_lock_acc[g_seq_step].load(std::memory_order_relaxed)
        : g_params.accent_amt.load();
    const float decay_k  = g_params.decay.load();
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

    // ── Overdrive precompute ────────────────────────────────────────────────
    // Pre-filter soft-clip (classic "303 into distortion pedal"). Gain sweeps
    // 1× → 6× with gain-compensated tanh so perceived loudness is roughly
    // constant as OD is dialled up — the knob is TEXTURE, not level.
    const float od_amt  = std::clamp(g_params.od_amt.load(), 0.0f, 1.0f);
    const float od_pre  = 1.0f + od_amt * 5.0f;
    const float od_norm = 1.0f / std::tanh(od_pre);

    // ── Delay precomputes ────────────────────────────────────────────────────
    // Target delay time in samples, synced to the sequencer's BPM. One 16th =
    // 60/BPM/4 s; division multipliers give 1/16, 1/16d, 1/8, 1/8d.
    const float dly_mix = std::clamp(g_params.delay_mix.load(),      0.0f, 1.0f);
    const float dly_fb  = std::clamp(g_params.delay_feedback.load(), 0.0f, 0.92f);
    const int   dly_div = std::clamp(g_params.delay_division.load(), 0, 3);
    static constexpr float kDivMul[4] = {1.0f, 1.5f, 2.0f, 3.0f};  // in 16ths
    const float sixteenth_s   = 60.0f / bpm / 4.0f;
    const float dly_time_tgt  = sixteenth_s * kDivMul[dly_div] * sr;
    // Clamp to buffer size minus 4 for safe linear-interp read across wrap.
    const float dly_time_max  = static_cast<float>(kDelayBufSize - 4);
    const float dly_time_clip = std::min(dly_time_tgt, dly_time_max);
    // Per-sample one-pole smoothing for time changes (BPM or division changes
    // shouldn't click). 60 ms settles faster than most finger turns register.
    const float dly_time_coef = 1.0f - std::exp(-dt_s / 0.060f);
    // Tape-darken LPF inside the feedback path, ~3.5 kHz. Keeps the repeats
    // getting steadily duller, which is what makes feedback feel "tape" not
    // "digital ping-pong".
    const float fb_lp_coef    = 1.0f - std::exp(-2.0f * static_cast<float>(M_PI)
                                               * 3500.0f / sr);

    // ── Reverb precomputes ──────────────────────────────────────────────────
    // Comb feedback g = 0.70 + size * 0.28 → small room (RT60≈0.6s) through
    // big plate (RT60≈4s). Damp LPF coef in [0, 0.5] rolls off high end in the
    // feedback path so long tails go dark instead of ringing.
    const float rev_mix_k  = std::clamp(g_params.rev_mix.load(),  0.0f, 1.0f);
    const float rev_size_k = std::clamp(g_params.rev_size.load(), 0.0f, 1.0f);
    const float rev_damp_k = std::clamp(g_params.rev_damp.load(), 0.0f, 1.0f);
    const float comb_g     = 0.70f + 0.28f * rev_size_k;
    const float damp       = 0.05f + 0.45f * rev_damp_k;
    const float undamp     = 1.0f - damp;
    // 0.5 fixed allpass coefficient is the Freeverb standard.
    constexpr float apass_g = 0.5f;
    // Input attenuation — without this, the dense comb net runs way too hot
    // even before Mix. 0.015 matches Freeverb's fixed input scale.
    constexpr float rev_in_gain = 0.015f;

    // ── Drum precomputes ────────────────────────────────────────────────────
    // Per-voice envelope decay coefficients (exp(-dt/tau)) and the per-voice
    // bus gains loaded once per sub-block. Time constants are tuned by ear:
    //   BD: 200 ms amp / 50 ms pitch sweep / 2 ms click → deep punchy kick
    //   SD: 120 ms noise / 35 ms body → snappy two-tone snare
    //   CH: 25 ms → classic tight 808-style metallic closed hat
    //   OH: 320 ms → brassy open hat
    //   CL: 7 ms per burst / 280 ms tail → 808 clap
    //   LT / HT: 300 / 220 ms amp, 80 / 60 ms pitch → tom sweep
    //   RS: 18 ms → sharp rimshot click
    //   CB: 180 ms → 808 cowbell ring
    const float drum_master = std::clamp(
        g_seq_drum_master.load(std::memory_order_relaxed), 0.0f, 1.5f);
    float drum_gain[kDrumVoices];
    for (int v = 0; v < kDrumVoices; ++v) {
        drum_gain[v] = g_seq_drum_gain[v].load(std::memory_order_relaxed)
                     * drum_master;
    }
    const float bd_env_dec   = std::exp(-dt_s / 0.20f);
    const float bd_penv_dec  = std::exp(-dt_s / 0.050f);
    const float bd_click_dec = std::exp(-dt_s / 0.002f);
    const float sd_env_dec   = std::exp(-dt_s / 0.12f);
    const float sd_body_dec  = std::exp(-dt_s / 0.035f);
    const float sd_noise_dec = std::exp(-dt_s / 0.12f);
    const float ch_env_dec   = std::exp(-dt_s / 0.025f);
    const float oh_env_dec   = std::exp(-dt_s / 0.32f);
    const float cl_env_dec   = std::exp(-dt_s / 0.007f);
    const float cl_tail_dec  = std::exp(-dt_s / 0.28f);
    const float lt_env_dec   = std::exp(-dt_s / g_lt.env_tc);
    const float lt_penv_dec  = std::exp(-dt_s / g_lt.penv_tc);
    const float ht_env_dec   = std::exp(-dt_s / g_ht.env_tc);
    const float ht_penv_dec  = std::exp(-dt_s / g_ht.penv_tc);
    const float rs_env_dec   = std::exp(-dt_s / 0.018f);
    const float cb_env_dec   = std::exp(-dt_s / 0.18f);
    const float sh_env_dec   = std::exp(-dt_s / 0.060f);
    const float tb_env_dec   = std::exp(-dt_s / 0.140f);
    const float cg_env_dec   = std::exp(-dt_s / 0.180f);
    const float cg_penv_dec  = std::exp(-dt_s / 0.040f);
    const float mt_env_dec   = std::exp(-dt_s / g_mt.env_tc);
    const float mt_penv_dec  = std::exp(-dt_s / g_mt.penv_tc);
    const float cy_env_dec   = std::exp(-dt_s / 0.80f);
    const float cy_ping_dec  = std::exp(-dt_s / 0.015f);
    const float rd_env_dec   = std::exp(-dt_s / 0.45f);
    const float rd_ping_dec  = std::exp(-dt_s / 0.040f);
    const float bg_env_dec   = std::exp(-dt_s / 0.090f);
    const float bg_penv_dec  = std::exp(-dt_s / 0.030f);

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

        // Pre-filter overdrive. Tanh with gain-compensation so output level
        // stays close to input level as OD sweeps. OD=0 is near-transparent.
        const float osc_od = std::tanh(osc * od_pre) * od_norm;

        // 2× oversample the filter + saturator. Produce two input sub-samples
        // by linear interpolation between the previous and current osc value,
        // run each through the ZDF ladder, and average for decimation.
        const float osc_mid = 0.5f * (g_osc_prev + osc_od);
        const float y_a     = ladder_tpt_step(osc_mid, g_tpt, g_res_sm);
        const float y_b     = ladder_tpt_step(osc_od,  g_tpt, g_res_sm);
        g_osc_prev          = osc_od;
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

        // ── Drum voice mix (pre-FX) ─────────────────────────────────────────
        // Every voice checks its env > ~0 gate so silent voices cost nothing
        // beyond a compare+branch. Output sums into drum_bus, then folded
        // into y_hpf BEFORE delay/reverb so drums carry the same FX send.
        //
        // The six-phase `kHatPitches` / `kCbPitches` tables are the 808-era
        // inharmonic-square recipes — detuned prime-ish ratios that give the
        // hat its metallic tone and the cowbell its unmistakable clonk.
        static constexpr float kHatPitches[6] = {
            205.0f, 370.0f, 540.0f, 775.0f, 1050.0f, 1400.0f
        };
        float drum_bus = 0.0f;
        if (g_bd.env > 1e-4f) {
            g_bd.env   *= bd_env_dec;
            g_bd.penv  *= bd_penv_dec;
            g_bd.click *= bd_click_dec;
            // Wider pitch sweep than the old recipe — 150→50 Hz is the proper
            // 808 "punch down" range. 100 Hz of excursion over 50 ms gives the
            // classic low-end "whomp".
            float bd_hz = 50.0f + 100.0f * g_bd.penv;
            g_bd.phase += bd_hz / sr;
            if (g_bd.phase >= 1.0f) g_bd.phase -= 1.0f;
            float tone = std::sin(2.0f * static_cast<float>(M_PI) * g_bd.phase);
            // Two attack components: a pitch-envelope-shaped "body click" that
            // fades with the sweep, plus a very short (~2 ms) noise-free
            // transient that gives the kick a drum-stick snap on top.
            float body_click = g_bd.penv * g_bd.penv;
            float attack     = g_bd.click * g_bd.click;
            drum_bus += (tone * g_bd.env
                         + body_click * 0.20f
                         + attack     * 0.35f)
                        * drum_gain[kDrumBD];
        }
        if (g_sd.env > 1e-4f || g_sd.noise_env > 1e-4f) {
            g_sd.env       *= sd_env_dec;
            g_sd.body_env  *= sd_body_dec;
            g_sd.noise_env *= sd_noise_dec;
            // Two-tone body — 180 + 330 Hz — closer to a real snare's dual
            // membrane fundamentals than the old single 220 Hz tone.
            g_sd.body_phase1 += 180.0f / sr;
            g_sd.body_phase2 += 330.0f / sr;
            if (g_sd.body_phase1 >= 1.0f) g_sd.body_phase1 -= 1.0f;
            if (g_sd.body_phase2 >= 1.0f) g_sd.body_phase2 -= 1.0f;
            float body = (std::sin(2.0f * static_cast<float>(M_PI) * g_sd.body_phase1)
                        + std::sin(2.0f * static_cast<float>(M_PI) * g_sd.body_phase2) * 0.6f)
                        * g_sd.body_env;
            // Two cascaded bandpass-ish one-poles give the noise a 1.5k/4k
            // shape — the "crack + sizzle" profile the old single-HPF missed.
            float n = rand_bipolar(g_sd.rng);
            float b1 = n - g_sd.bp_x1 + 0.80f * g_sd.bp_y1;
            g_sd.bp_x1 = n; g_sd.bp_y1 = b1;
            float b2 = b1 - g_sd.bp_x2 + 0.60f * g_sd.bp_y2;
            g_sd.bp_x2 = b1; g_sd.bp_y2 = b2;
            drum_bus += (body * 0.50f + b2 * 0.85f * g_sd.noise_env)
                        * g_sd.env * drum_gain[kDrumSD];
        }
        // Hat oscillator (shared between CH / OH). 6 squares at inharmonic
        // ratios summed, then passed through a resonant bandpass around 8 kHz
        // and an HPF to clear the low-mid mud. Mixed with a little white noise
        // so the attack has some hiss on top of the metallic body.
        auto render_hat = [&](DrumHat& h, float env_dec, float hpf_a) -> float {
            if (!(h.env > 1e-4f)) return 0.0f;
            h.env *= env_dec;
            float metal = 0.0f;
            for (int i = 0; i < 6; ++i) {
                h.phase[i] += kHatPitches[i] / sr;
                if (h.phase[i] >= 1.0f) h.phase[i] -= 1.0f;
                metal += (h.phase[i] < 0.5f) ? 1.0f : -1.0f;
            }
            metal *= (1.0f / 6.0f);
            // Bandpass — simple resonant one-pole pair centered near 8 kHz
            // emphasises the "tss" component while letting the metal tone
            // carry the body.
            float bp = metal - h.bp_x + 0.55f * h.bp_y;
            h.bp_x = metal; h.bp_y = bp;
            // Noise injection — 30% white mixed into the metal to humanise
            // the otherwise-repetitive square stack.
            float n  = rand_bipolar(h.rng);
            float mix = bp * 0.7f + n * 0.3f;
            // Output HPF — rolls off everything below ~500 Hz so the hat
            // doesn't mud the mix when it layers with the kick.
            float out = mix - h.hp_x + hpf_a * h.hp_y;
            h.hp_x = mix; h.hp_y = out;
            return out * h.env;
        };
        drum_bus += render_hat(g_ch, ch_env_dec, 0.97f) * drum_gain[kDrumCH];
        drum_bus += render_hat(g_oh, oh_env_dec, 0.95f) * drum_gain[kDrumOH];
        if (g_cl.env > 1e-4f || g_cl.tail_env > 1e-4f) {
            g_cl.env      *= cl_env_dec;
            g_cl.tail_env *= cl_tail_dec;
            if (g_cl.bursts_left > 0) {
                g_cl.burst_clock -= dt_s;
                if (g_cl.burst_clock <= 0.0f) {
                    g_cl.env = 1.0f;
                    g_cl.bursts_left -= 1;
                    g_cl.burst_clock += 0.012f;
                }
            }
            float n = rand_bipolar(g_cl.rng);
            // Bandpass around ~1.3 kHz gives the tail the cupped-hands body
            // a real clap has. Coefficients picked to emphasise 1k-2k.
            float b = n - g_cl.bp_x + 0.88f * g_cl.bp_y;
            g_cl.bp_x = n; g_cl.bp_y = b;
            float amp = std::max(g_cl.env, g_cl.tail_env * 0.35f);
            drum_bus += b * amp * drum_gain[kDrumCL];
        }
        // Toms — pitched sines with small pitch envelope. Same structure as
        // BD but longer decay and narrower sweep so the pitch stays obvious.
        auto render_tom = [&](DrumTom& t, float env_dec, float penv_dec) -> float {
            if (!(t.env > 1e-4f)) return 0.0f;
            t.env  *= env_dec;
            t.penv *= penv_dec;
            float hz = t.base_hz + (t.peak_hz - t.base_hz) * t.penv;
            t.phase += hz / sr;
            if (t.phase >= 1.0f) t.phase -= 1.0f;
            float s = std::sin(2.0f * static_cast<float>(M_PI) * t.phase);
            // Tiny click adds initial transient without the tom losing its tuned character.
            float click = t.penv * t.penv * 0.18f;
            return (s * t.env + click) * t.env;
        };
        drum_bus += render_tom(g_lt, lt_env_dec, lt_penv_dec) * drum_gain[kDrumLT];
        drum_bus += render_tom(g_ht, ht_env_dec, ht_penv_dec) * drum_gain[kDrumHT];
        if (g_rs.env > 1e-4f) {
            g_rs.env *= rs_env_dec;
            g_rs.sine1_phase += 1650.0f / sr;
            g_rs.sine2_phase += 2200.0f / sr;
            if (g_rs.sine1_phase >= 1.0f) g_rs.sine1_phase -= 1.0f;
            if (g_rs.sine2_phase >= 1.0f) g_rs.sine2_phase -= 1.0f;
            float s1 = std::sin(2.0f * static_cast<float>(M_PI) * g_rs.sine1_phase);
            float s2 = std::sin(2.0f * static_cast<float>(M_PI) * g_rs.sine2_phase);
            float n  = rand_bipolar(g_rs.rng);
            // High bandpass — noise component lives 1.5–2.5 kHz along with the sines.
            float b  = n - g_rs.bp_x + 0.72f * g_rs.bp_y;
            g_rs.bp_x = n; g_rs.bp_y = b;
            float body = s1 * 0.5f + s2 * 0.5f + b * 0.6f;
            drum_bus += body * g_rs.env * drum_gain[kDrumRS];
        }
        if (g_cb.env > 1e-4f) {
            g_cb.env *= cb_env_dec;
            // Two detuned squares — the 540/800 Hz ratio is the signature 808
            // cowbell. Unblepped: these are low enough that a naive square
            // doesn't alias audibly.
            g_cb.phase1 += 540.0f / sr;
            g_cb.phase2 += 800.0f / sr;
            if (g_cb.phase1 >= 1.0f) g_cb.phase1 -= 1.0f;
            if (g_cb.phase2 >= 1.0f) g_cb.phase2 -= 1.0f;
            float sq1 = (g_cb.phase1 < 0.5f) ? 1.0f : -1.0f;
            float sq2 = (g_cb.phase2 < 0.5f) ? 1.0f : -1.0f;
            float m   = (sq1 + sq2) * 0.5f;
            // Bandpass keeps the mid-range body and drops the harsh square edges.
            float b = m - g_cb.bp_x + 0.70f * g_cb.bp_y;
            g_cb.bp_x = m; g_cb.bp_y = b;
            drum_bus += b * g_cb.env * drum_gain[kDrumCB];
        }
        if (g_sh.env > 1e-4f) {
            g_sh.env *= sh_env_dec;
            // Shaker: white noise through a resonant bandpass ~4 kHz then a
            // gentle HPF so low-mid rattle doesn't muddy the kick. The env²
            // shape gives it a fast percussive snap rather than a linear fade.
            float n  = rand_bipolar(g_sh.rng);
            float bp = n - g_sh.bp_x + 0.60f * g_sh.bp_y;
            g_sh.bp_x = n; g_sh.bp_y = bp;
            float out = bp - g_sh.hp_x + 0.93f * g_sh.hp_y;
            g_sh.hp_x = bp; g_sh.hp_y = out;
            drum_bus += out * (g_sh.env * g_sh.env) * drum_gain[kDrumSH];
        }
        if (g_tb.env > 1e-4f) {
            g_tb.env *= tb_env_dec;
            // Tambourine: two parallel bandpasses on white noise give "body"
            // (~4 kHz jingle) and "shimmer" (~8 kHz). Summed roughly equal so
            // the hit reads as bright without being harsh.
            float n   = rand_bipolar(g_tb.rng);
            float b1  = n - g_tb.bp1_x + 0.72f * g_tb.bp1_y;
            g_tb.bp1_x = n; g_tb.bp1_y = b1;
            float b2  = n - g_tb.bp2_x + 0.45f * g_tb.bp2_y;
            g_tb.bp2_x = n; g_tb.bp2_y = b2;
            drum_bus += (b1 * 0.55f + b2 * 0.75f) * g_tb.env * drum_gain[kDrumTB];
        }
        if (g_cg.env > 1e-4f) {
            g_cg.env  *= cg_env_dec;
            g_cg.penv *= cg_penv_dec;
            // Conga: 260→340 Hz pitch sweep gives the hand-slap attack pitch.
            // Click transient (penv²) doubles the initial impact like a real
            // conga's skin strike.
            float hz = 260.0f + 80.0f * g_cg.penv;
            g_cg.phase += hz / sr;
            if (g_cg.phase >= 1.0f) g_cg.phase -= 1.0f;
            float s = std::sin(2.0f * static_cast<float>(M_PI) * g_cg.phase);
            float click = g_cg.penv * g_cg.penv * 0.25f;
            drum_bus += (s * g_cg.env + click) * g_cg.env * drum_gain[kDrumCG];
        }
        drum_bus += render_tom(g_mt, mt_env_dec, mt_penv_dec) * drum_gain[kDrumMT];
        // Cymbals — shared recipe with the hat (6 inharmonic squares + BP),
        // but with longer decays, wider bandpass and, for RD, a sine "ping"
        // on the attack so the bell rings above the noise. Uses the same
        // kHatPitches table as CH/OH so the whole cymbal section shares a
        // consistent metallic signature.
        auto render_cym = [&](DrumCym& c, float env_dec, float ping_dec,
                              float bp_res, float hpf_a) -> float {
            if (!(c.env > 1e-4f) && !(c.ping > 1e-4f)) return 0.0f;
            c.env  *= env_dec;
            c.ping *= ping_dec;
            float metal = 0.0f;
            for (int i = 0; i < 6; ++i) {
                c.phase[i] += kHatPitches[i] / sr;
                if (c.phase[i] >= 1.0f) c.phase[i] -= 1.0f;
                metal += (c.phase[i] < 0.5f) ? 1.0f : -1.0f;
            }
            metal *= (1.0f / 6.0f);
            float bp = metal - c.bp_x + bp_res * c.bp_y;
            c.bp_x = metal; c.bp_y = bp;
            float n  = rand_bipolar(c.rng);
            // Crash leans noisier (0.55 noise vs 0.45 metal) so the hit
            // reads as a splash; ride leans cleaner so the ping stays
            // audible under steady 8th-note playing.
            float noise_mix = (c.ping_hz > 0.0f) ? 0.35f : 0.55f;
            float mix = bp * (1.0f - noise_mix) + n * noise_mix;
            float out = mix - c.hp_x + hpf_a * c.hp_y;
            c.hp_x = mix; c.hp_y = out;
            // Ride ping: short pure sine on the attack, panned under the
            // metallic wash so each ride hit has a pitched "bell" transient.
            float ping_sig = 0.0f;
            if (c.ping_hz > 0.0f) {
                c.ping_ph += c.ping_hz / sr;
                if (c.ping_ph >= 1.0f) c.ping_ph -= 1.0f;
                ping_sig = std::sin(2.0f * static_cast<float>(M_PI) * c.ping_ph)
                         * c.ping * 0.45f;
            }
            return out * c.env + ping_sig;
        };
        // Crash: wide BP (low res), bright HPF → big splash. Ride: tight BP,
        // darker HPF so the ping carries without competing with the noise.
        drum_bus += render_cym(g_cy, cy_env_dec, cy_ping_dec, 0.30f, 0.90f)
                    * drum_gain[kDrumCY];
        drum_bus += render_cym(g_rd, rd_env_dec, rd_ping_dec, 0.55f, 0.85f)
                    * drum_gain[kDrumRD];
        if (g_bg.env > 1e-4f) {
            g_bg.env  *= bg_env_dec;
            g_bg.penv *= bg_penv_dec;
            // Bongo: pitched one octave above the conga so a CG/BG pair
            // reads as two distinct drums. Same sine + click recipe.
            float hz = 380.0f + 90.0f * g_bg.penv;
            g_bg.phase += hz / sr;
            if (g_bg.phase >= 1.0f) g_bg.phase -= 1.0f;
            float s = std::sin(2.0f * static_cast<float>(M_PI) * g_bg.phase);
            float click = g_bg.penv * g_bg.penv * 0.30f;
            drum_bus += (s * g_bg.env + click) * g_bg.env * drum_gain[kDrumBG];
        }
        y_hpf += drum_bus;

        // ── Tempo-synced delay (master bus) ─────────────────────────────────
        // Read at fractional index with linear interp so BPM sweeps glide.
        // Feedback path runs through a one-pole LPF to darken each repeat —
        // the "tape" flavour that makes stacked echoes feel musical rather
        // than digital-harsh. Dry + wet are summed additively (wet scaled by
        // the MIX knob) so MIX=0 is clean bypass.
        g_delay_time_sm += dly_time_coef * (dly_time_clip - g_delay_time_sm);
        float rd_f  = static_cast<float>(g_delay_w) - g_delay_time_sm;
        while (rd_f < 0.0f) rd_f += static_cast<float>(kDelayBufSize);
        int   rd_i  = static_cast<int>(rd_f);
        float frac  = rd_f - static_cast<float>(rd_i);
        int   rd_i2 = (rd_i + 1) % kDelayBufSize;
        float wet   = g_delay_buf[rd_i] * (1.0f - frac)
                    + g_delay_buf[rd_i2] * frac;
        g_delay_fb_lp += fb_lp_coef * (wet - g_delay_fb_lp);
        g_delay_buf[g_delay_w] = y_hpf + dly_fb * g_delay_fb_lp;
        g_delay_w = (g_delay_w + 1) % kDelayBufSize;
        float y_post_dly = y_hpf + dly_mix * wet;

        // ── Plate reverb (master bus, parallel combs → serial allpass) ──────
        // Only pay the cost when the wet path is actually audible. Below
        // 0.3% mix the reverb is below the noise floor of most listeners, so
        // skipping there keeps idle CPU low.
        float y_out = y_post_dly;
        if (rev_mix_k > 0.003f) {
            float rv_in = y_post_dly * rev_in_gain;
            float comb_sum = 0.0f;
            for (int c = 0; c < kRevCombs; ++c) {
                int   len = g_rev_comb_len[c];
                int   w   = g_rev_comb_w[c];
                float s   = g_rev_comb_buf[c][w];
                g_rev_comb_lp[c] = s * undamp + g_rev_comb_lp[c] * damp;
                float fb  = g_rev_comb_lp[c] * comb_g;
                g_rev_comb_buf[c][w] = rv_in + fb;
                g_rev_comb_w[c] = (w + 1) % len;
                comb_sum += s;
            }
            float rv = comb_sum;
            for (int a = 0; a < kRevApass; ++a) {
                int   len = g_rev_apass_len[a];
                int   w   = g_rev_apass_w[a];
                float s   = g_rev_apass_buf[a][w];
                float in  = rv;
                float out_ap = -in + s;
                g_rev_apass_buf[a][w] = in + apass_g * out_ap;
                g_rev_apass_w[a] = (w + 1) % len;
                rv = out_ap;
            }
            y_out = y_post_dly + rev_mix_k * rv;
        }

        out_ptr[i] = y_out;

        // Scope tap: store in ring; peak envelope decays ~100 ms.
        uint32_t w = g_scope_w.load(std::memory_order_relaxed);
        g_scope_buf[w & (kScopeRingSize - 1)] = y_out;
        g_scope_w.store(w + 1, std::memory_order_release);

        // Live-record tap: linear append into the preallocated capture buffer
        // while the flag is on. No allocation, no I/O — just a bounded store.
        if (g_rec_on.load(std::memory_order_acquire)) {
            uint32_t rw  = g_rec_w.load(std::memory_order_relaxed);
            uint32_t cap = g_rec_cap.load(std::memory_order_relaxed);
            if (g_rec_buf && rw < cap) {
                g_rec_buf[rw] = y_out;
                g_rec_w.store(rw + 1, std::memory_order_release);
            } else {
                g_rec_overflow.store(true, std::memory_order_relaxed);
            }
        }

        float abs_y = std::fabs(y_out);
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

void acid_set_delay_mix(float v)      { acid::g_params.delay_mix.store(v); }
void acid_set_delay_feedback(float v) { acid::g_params.delay_feedback.store(v); }
void acid_set_delay_division(int v)   { acid::g_params.delay_division.store(v); }

void acid_set_od_amt(float v)   { acid::g_params.od_amt.store(v); }
void acid_set_rev_mix(float v)  { acid::g_params.rev_mix.store(v); }
void acid_set_rev_size(float v) { acid::g_params.rev_size.store(v); }
void acid_set_rev_damp(float v) { acid::g_params.rev_damp.store(v); }

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

int acid_sample_rate(void) {
    return acid::sample_rate();
}

// Snapshot spectrum. Pulls the most-recent 2048 samples out of the scope ring,
// Hann-windows them, runs a radix-2 Cooley-Tukey FFT, and writes the first
// `n_bins_request` magnitude bins (linearly spaced 0..sr/2) into `out_mags`.
// 2048 gives ~23 Hz bin width at 48 kHz — enough to resolve bass detail while
// staying well under 1 ms of UI-thread work. Buffers are `thread_local static`
// so there's no per-call allocation; the call is UI-only so no race concerns.
int acid_fft_bins(float* out_mags, int n_bins_request) {
    if (!out_mags || n_bins_request <= 0) return 0;
    constexpr int N       = 2048;
    constexpr int N_bins  = N / 2;

    thread_local float                       samples[N] = {};
    thread_local std::complex<float>         x[N];

    int got = acid_scope_tail(samples, N);
    for (int i = got; i < N; ++i) samples[i] = 0.0f;

    // Hann window to kill leakage from the rectangular ring cut.
    const float two_pi_over_Nm1 = 2.0f * static_cast<float>(M_PI)
                                / static_cast<float>(N - 1);
    for (int i = 0; i < N; ++i) {
        float w = 0.5f * (1.0f - std::cos(two_pi_over_Nm1 * i));
        x[i] = { samples[i] * w, 0.0f };
    }

    // Bit-reverse permutation.
    for (int i = 1, j = 0; i < N; ++i) {
        int bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(x[i], x[j]);
    }

    // Cooley-Tukey in-place.
    for (int len = 2; len <= N; len <<= 1) {
        float ang = -2.0f * static_cast<float>(M_PI) / static_cast<float>(len);
        std::complex<float> wlen{ std::cos(ang), std::sin(ang) };
        const int half = len >> 1;
        for (int i = 0; i < N; i += len) {
            std::complex<float> w{1.0f, 0.0f};
            for (int j = 0; j < half; ++j) {
                std::complex<float> u = x[i + j];
                std::complex<float> v = x[i + j + half] * w;
                x[i + j]        = u + v;
                x[i + j + half] = u - v;
                w *= wlen;
            }
        }
    }

    // Magnitude of the first N/2 bins. 2/N normalisation so a full-scale
    // sine at bin k reads back at amplitude 1.0 (Hann coherent gain folded
    // in lightly by using 2/N rather than 2/(0.5*N) = 4/N).
    const int n = std::min(n_bins_request, N_bins);
    const float scale = 2.0f / static_cast<float>(N);
    for (int i = 0; i < n; ++i) {
        out_mags[i] = std::abs(x[i]) * scale;
    }
    return n;
}

// ── Audio-thread sequencer API (called from UI) ─────────────────────────────
// Each step is packed into a single u32 — see the layout comment near
// g_seq_steps. Every write is one atomic store; the audio thread reads each
// slot when it advances to that step so a torn read is not possible.

void acid_seq_set_step_locks(int idx, int mask,
                             float cutoff_v, float res_v,
                             float env_v,    float accent_v) {
    if (idx < 0 || idx >= acid::kSeqMaxSteps) return;
    auto clip = [](float v) { return std::clamp(v, 0.0f, 1.0f); };
    acid::g_seq_lock_cut[idx].store(clip(cutoff_v), std::memory_order_relaxed);
    acid::g_seq_lock_res[idx].store(clip(res_v),    std::memory_order_relaxed);
    acid::g_seq_lock_env[idx].store(clip(env_v),    std::memory_order_relaxed);
    acid::g_seq_lock_acc[idx].store(clip(accent_v), std::memory_order_relaxed);
    // Mask last — audio thread reads mask to decide whether to look at values.
    acid::g_seq_lock_mask[idx].store(static_cast<uint32_t>(mask & 0xF),
                                    std::memory_order_release);
}

void acid_seq_set_step(int idx, int midi, int flags, int prob, int ratchet) {
    if (idx < 0 || idx >= acid::kSeqMaxSteps) return;
    int p = std::clamp(prob,   0, 100);
    int r = std::clamp(ratchet, 1,   4) - 1;
    uint32_t enc = (static_cast<uint32_t>(midi)    & 0xFFu)
                 | ((static_cast<uint32_t>(flags)  & 0x07u) << 8)
                 | ((static_cast<uint32_t>(p)      & 0x7Fu) << 11)
                 | ((static_cast<uint32_t>(r)      & 0x03u) << 18);
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

void acid_seq_set_drum_lane(int voice, int mask_16bit) {
    if (voice < 0 || voice >= acid::kDrumVoices) return;
    acid::g_seq_drum_mask[voice].store(
        static_cast<uint32_t>(mask_16bit) & 0xFFFFu,
        std::memory_order_relaxed);
}

void acid_seq_set_drum_gain(int voice, float v) {
    if (voice < 0 || voice >= acid::kDrumVoices) return;
    acid::g_seq_drum_gain[voice].store(std::clamp(v, 0.0f, 2.0f),
                                       std::memory_order_relaxed);
}

void acid_seq_set_drum_master(float v) {
    acid::g_seq_drum_master.store(std::clamp(v, 0.0f, 1.5f),
                                  std::memory_order_relaxed);
}

// ── MIDI bridge (Phase 5) ───────────────────────────────────────────────────
// These setters/getters let the MIDI backend thread poll the engine's event
// ring and feed incoming clock/note messages back in. Everything except
// acid_midi_consume_event is safe to call from any thread.

void acid_midi_set_out_enabled(int on) {
    acid::g_midi_out_enabled.store(on != 0, std::memory_order_relaxed);
}

int acid_midi_out_enabled(void) {
    return acid::g_midi_out_enabled.load(std::memory_order_relaxed) ? 1 : 0;
}

void acid_midi_set_sync_in(int on) {
    acid::g_midi_sync_in.store(on != 0, std::memory_order_relaxed);
    if (!on) {
        // Clear the averaging window so re-enabling doesn't snap back to an
        // old tempo the user has meanwhile tweaked with [/].
        acid::g_midi_clock_i      = 0;
        acid::g_midi_clock_filled = 0;
        acid::g_midi_clock_sum    = 0.0;
    }
}

int acid_midi_sync_in(void) {
    return acid::g_midi_sync_in.load(std::memory_order_relaxed) ? 1 : 0;
}

// Called by the MIDI thread on every incoming 0xF8 pulse. `now_seconds` is a
// monotonic timestamp (e.g. from CLOCK_MONOTONIC). Averages the last 24
// inter-pulse deltas and writes the derived BPM into g_seq_bpm.
void acid_midi_feed_clock(double now_seconds) {
    if (!acid::g_midi_sync_in.load(std::memory_order_relaxed)) return;
    static double last_t = 0.0;
    if (last_t <= 0.0) { last_t = now_seconds; return; }
    double dt = now_seconds - last_t;
    last_t = now_seconds;
    // Reject absurd gaps (>0.5 s would imply the master stalled — start over).
    if (dt <= 0.0 || dt > 0.5) {
        acid::g_midi_clock_i      = 0;
        acid::g_midi_clock_filled = 0;
        acid::g_midi_clock_sum    = 0.0;
        return;
    }
    int idx = acid::g_midi_clock_i;
    if (acid::g_midi_clock_filled >= acid::kMidiClockWindow) {
        acid::g_midi_clock_sum -= acid::g_midi_clock_delta[idx];
    } else {
        acid::g_midi_clock_filled += 1;
    }
    acid::g_midi_clock_delta[idx] = static_cast<float>(dt);
    acid::g_midi_clock_sum       += dt;
    acid::g_midi_clock_i          = (idx + 1) % acid::kMidiClockWindow;
    if (acid::g_midi_clock_filled >= 4) {
        double mean_dt = acid::g_midi_clock_sum
                      / static_cast<double>(acid::g_midi_clock_filled);
        // dt is seconds per 24 PPQN pulse → beat = 24*dt → bpm = 60/(24*dt).
        double bpm = 60.0 / (24.0 * mean_dt);
        acid::g_seq_bpm.store(std::clamp(static_cast<float>(bpm), 20.0f, 300.0f),
                              std::memory_order_relaxed);
    }
}

// Received 0xFA (start) or 0xFC (stop) from the master. 0xFB (continue) is
// treated as start. The engine starts/stops its internal sequencer; if clock
// sync is off these calls are silently ignored so a stray master message
// can't steamroll manual playback.
void acid_midi_feed_start_stop(int start) {
    if (!acid::g_midi_sync_in.load(std::memory_order_relaxed)) return;
    acid::g_seq_playing.store(start != 0, std::memory_order_release);
}

// Drain the MIDI out event ring. Caller supplies a buffer of `max_evts` 4-byte
// events; returns the number written. Event layout matches acid::MidiEvt.
int acid_midi_consume_events(unsigned char* out, int max_evts) {
    if (!out || max_evts <= 0) return 0;
    uint32_t w = acid::g_midi_w.load(std::memory_order_acquire);
    uint32_t r = acid::g_midi_r.load(std::memory_order_relaxed);
    int written = 0;
    while (r != w && written < max_evts) {
        const auto& e = acid::g_midi_ring[r & (acid::kMidiRingSize - 1)];
        out[written * 4 + 0] = e.type;
        out[written * 4 + 1] = e.channel;
        out[written * 4 + 2] = e.data1;
        out[written * 4 + 3] = e.data2;
        ++written;
        ++r;
    }
    acid::g_midi_r.store(r, std::memory_order_release);
    return written;
}

// Incoming note from a MIDI controller. The backend converts MIDI note →
// frequency and calls this; we pipe through the normal note_on path so the
// synth behaves exactly like jam mode. `step_sec` is the slide ramp time when
// slide=1; 0 when not sliding.
void acid_midi_note_on_ext(int midi_note, int velocity) {
    if (midi_note < 0 || midi_note > 127) return;
    float freq = 440.0f * std::pow(2.0f, static_cast<float>(midi_note - 69) / 12.0f);
    acid_note_on(freq, velocity >= 100 ? 1 : 0, /*slide=*/0, /*step_sec=*/0.18f);
}
void acid_midi_note_off_ext(int /*midi_note*/) {
    acid_note_off();
}

// Current BPM the engine is acting on — clock-out pacing derives from this.
float acid_current_bpm(void) {
    return acid::g_seq_bpm.load(std::memory_order_relaxed);
}
int acid_is_playing(void) {
    return acid::g_seq_playing.load(std::memory_order_relaxed) ? 1 : 0;
}

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
    // Fresh delay state for the bounce — otherwise a previous live session
    // leaks its repeats into the first few hundred ms of the render.
    std::memset(acid::g_delay_buf, 0, sizeof(acid::g_delay_buf));
    acid::g_delay_w = 0;
    acid::g_delay_fb_lp = 0.0f;
    acid::g_delay_time_sm = 0.0f;
    // Reverb buffers also need wiping — old tails would bleed into bar 1.
    std::memset(acid::g_rev_comb_buf,  0, sizeof(acid::g_rev_comb_buf));
    std::memset(acid::g_rev_apass_buf, 0, sizeof(acid::g_rev_apass_buf));
    for (int i = 0; i < acid::kRevCombs;  ++i) { acid::g_rev_comb_w[i]  = 0; acid::g_rev_comb_lp[i] = 0.0f; }
    for (int i = 0; i < acid::kRevApass;  ++i) { acid::g_rev_apass_w[i] = 0; }
    acid::g_last_seq        = acid::g_cmd_seq.load(std::memory_order_acquire);
    acid::g_seq_was_playing = false;
    acid::g_seq_rat_left    = 0;
    // Reset the probability RNG so back-to-back bounces of the same pattern
    // produce identical WAVs — matters when users iterate on a groove.
    acid::g_seq_rng         = 0x9E3779B9u;
    // Drum voices — envelopes and phases back to idle so no leakage from a
    // previous bounce. Masks are preserved (the drum pattern bounces too).
    acid::g_bd = acid::DrumBD{};
    acid::g_sd = acid::DrumSD{};
    acid::g_ch = acid::DrumHat{ .rng = 0x1b873593u };
    acid::g_oh = acid::DrumHat{ .rng = 0xcc9e2d51u };
    acid::g_cl = acid::DrumCL{};
    acid::g_lt = acid::DrumTom{ .base_hz = 80.0f,  .peak_hz = 110.0f, .env_tc = 0.30f, .penv_tc = 0.080f };
    acid::g_ht = acid::DrumTom{ .base_hz = 180.0f, .peak_hz = 230.0f, .env_tc = 0.22f, .penv_tc = 0.060f };
    acid::g_rs = acid::DrumRS{};
    acid::g_cb = acid::DrumCB{};
    acid::g_sh = acid::DrumSH{};
    acid::g_tb = acid::DrumTB{};
    acid::g_cg = acid::DrumCG{};
    acid::g_mt = acid::DrumTom{
        .base_hz = 128.0f, .peak_hz = 160.0f, .env_tc = 0.26f, .penv_tc = 0.070f };
    acid::g_cy = acid::DrumCym{ .ping_hz = 0.0f,   .rng = 0x9d2c5680u };
    acid::g_rd = acid::DrumCym{ .ping_hz = 420.0f, .rng = 0xefc60000u };
    acid::g_bg = acid::DrumBongo{};

    // Load the pattern into the audio-thread sequencer. `notes[i]` carries
    // the same 32-bit layout the audio thread reads back — midi in the low
    // byte, flags at bit 8, prob at bit 11, ratchet-1 at bit 18.
    for (int i = 0; i < pattern_length; ++i) {
        int enc  = notes[i];
        int midi = enc & 0xFF;
        int flg  = (enc >> 8)  & 0x7;
        int prob = (enc >> 11) & 0x7F;
        int rat  = ((enc >> 18) & 0x3) + 1;
        acid_seq_set_step(i, midi, flg, prob, rat);
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

// ── Live recorder ───────────────────────────────────────────────────────────
// Unlike acid_render_wav (which synthesises offline), this captures the
// audio-thread output in real time so every knob tweak, jammed note, and
// MIDI-input hit winds up on tape. Allocation is UI-side, freeing is UI-side,
// and the audio thread only appends to a preallocated buffer while a flag is
// set — so there's no allocation or blocking on the render path.
int acid_record_begin(int max_seconds) {
    if (acid::g_rec_on.load(std::memory_order_acquire)) return -1;
    if (max_seconds <= 0) max_seconds = 60;
    // Clamp so a careless call can't OOM — 1 hour @ 48 kHz = ~690 MB raw.
    if (max_seconds > 3600) max_seconds = 3600;

    const int      sr  = acid::sample_rate();
    const uint32_t cap = static_cast<uint32_t>(sr) *
                         static_cast<uint32_t>(max_seconds);
    float* buf = nullptr;
    try { buf = new float[cap]; }
    catch (...) { return -1; }

    acid::g_rec_buf = buf;
    acid::g_rec_cap.store(cap, std::memory_order_relaxed);
    acid::g_rec_w.store(0,   std::memory_order_relaxed);
    acid::g_rec_overflow.store(false, std::memory_order_relaxed);
    // Release: everything above must be visible before the audio thread
    // sees the flag flip.
    acid::g_rec_on.store(true, std::memory_order_release);
    return 0;
}

int acid_record_end(const char* path) {
    if (!path) return -1;
    if (!acid::g_rec_on.load(std::memory_order_acquire) && !acid::g_rec_buf)
        return -1;

    // Flip the flag off first so the audio thread stops appending.
    acid::g_rec_on.store(false, std::memory_order_release);
    // Give the audio thread one render block to finish any in-flight write.
    // kBufFrames @ 44.1 kHz ≈ 5.8 ms; sleep 20 ms for plenty of margin.
    // (Using a simple busy-ish spin via a C++11 sleep keeps the header light.)
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    uint32_t frames = acid::g_rec_w.load(std::memory_order_acquire);
    float*   buf    = acid::g_rec_buf;
    acid::g_rec_buf = nullptr;
    acid::g_rec_cap.store(0, std::memory_order_relaxed);

    if (!buf) return -1;
    if (frames == 0) { delete[] buf; return -1; }

    FILE* f = std::fopen(path, "wb");
    if (!f) { delete[] buf; return -1; }

    const int sr = acid::sample_rate();
    auto write_u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto write_u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    uint32_t data_bytes = frames * 2u;
    std::fwrite("RIFF", 1, 4, f);
    write_u32(36u + data_bytes);
    std::fwrite("WAVEfmt ", 1, 8, f);
    write_u32(16);
    write_u16(1);
    write_u16(1);
    write_u32(static_cast<uint32_t>(sr));
    write_u32(static_cast<uint32_t>(sr * 2));
    write_u16(2);
    write_u16(16);
    std::fwrite("data", 1, 4, f);
    write_u32(data_bytes);

    constexpr uint32_t kChunk = 4096;
    std::vector<int16_t> out16(kChunk);
    uint32_t written = 0;
    while (written < frames) {
        uint32_t n = std::min(kChunk, frames - written);
        for (uint32_t i = 0; i < n; ++i) {
            float s = std::clamp(buf[written + i], -1.0f, 1.0f);
            out16[i] = static_cast<int16_t>(s * 32767.0f);
        }
        std::fwrite(out16.data(), 2, n, f);
        written += n;
    }
    std::fclose(f);
    delete[] buf;
    return 0;
}

int acid_is_recording(void) {
    return acid::g_rec_on.load(std::memory_order_acquire) ? 1 : 0;
}

float acid_record_seconds(void) {
    if (!acid::g_rec_on.load(std::memory_order_acquire)) return 0.0f;
    uint32_t w = acid::g_rec_w.load(std::memory_order_relaxed);
    int sr = acid::sample_rate();
    if (sr <= 0) return 0.0f;
    return static_cast<float>(w) / static_cast<float>(sr);
}

}  // extern "C"
