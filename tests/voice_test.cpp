// Sound-correctness tests: the 303 voice measured against what the hardware
// does (Whittle's sequencer timing and envelope notes, Stinchcombe's filter
// analysis, Open303's measured constants). Each test states the reference.
#include "audio.hpp"
#include "dsp.hpp"
#include "engine.hpp"
#include "sig.hpp"
#include "t.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

constexpr int kSR = 48000;

// A clean voice: no FX, no drums, no drive, mid filter.
void dry_voice(float cutoff = 0.5f, float res = 0.0f, float env = 0.0f, float decay = 0.5f) {
    acid_set_cutoff(cutoff); acid_set_resonance(res); acid_set_env_mod(env);
    acid_set_decay(decay); acid_set_accent_amt(0.7f); acid_set_drive(0.0f);
    acid_set_volume(0.6f); acid_set_tuning_semi(0.0f); acid_set_waveform(0);
    acid_set_delay_mix(0); acid_set_rev_mix(0); acid_set_od_amt(0);
    acid_seq_set_drum_master(0.0f);
    for (int v = 0; v < 16; ++v) acid_seq_set_drum_lane(v, 0);
}

std::vector<float> mono_render(acid::Engine& e, int frames) {
    std::vector<float> lr(static_cast<size_t>(frames) * 2), m(static_cast<size_t>(frames));
    for (int i = 0; i < frames; i += 256) e.render(&lr[static_cast<size_t>(2 * i)], std::min(256, frames - i));
    for (int i = 0; i < frames; ++i) m[static_cast<size_t>(i)] = 0.5f * (lr[2 * static_cast<size_t>(i)] + lr[2 * static_cast<size_t>(i) + 1]);
    return m;
}

// RMS envelope in 1 ms windows.
std::vector<double> env_ms(const std::vector<float>& x) {
    const size_t w = kSR / 1000;
    std::vector<double> e;
    for (size_t i = 0; i + w <= x.size(); i += w) e.push_back(sig::rms(&x[i], w));
    return e;
}


}  // namespace

// ── Sequencer gate timing ────────────────────────────────────────────────────
// Whittle (303-slide.html): each 16th is 6 clock pulses; the gate is high for
// 3.5 and low for 2.5. So a non-slid note is silent (VCA released) for the
// last ~40% of its step.
TEST("gate: a normal step releases ~58% of the way through") {
    acid::Engine e(kSR);
    acid::Engine::Bind b(e);
    dry_voice();
    for (int i = 0; i < 16; ++i) acid_seq_set_step(i, 36, 0, 100, 1);   // all notes, no slide
    acid_seq_set_pattern_length(16);
    acid_seq_set_bpm(120.0f);
    acid_seq_set_swing(0.5f);
    acid_seq_play();
    auto x = mono_render(e, kSR * 2);
    auto en = env_ms(x);
    // One 16th at 120 BPM = 125 ms. Look at steps 4..11 (settled).
    double on = 0, off = 0;
    for (int s = 4; s < 12; ++s) {
        const int t0 = s * 125;
        for (int ms = 10; ms < 60; ++ms) on += en[static_cast<size_t>(t0 + ms)];     // inside the gate
        for (int ms = 95; ms < 120; ++ms) off += en[static_cast<size_t>(t0 + ms)];   // after release
    }
    on /= 8 * 50; off /= 8 * 25;
    std::printf("      gated level %.4f, released level %.4f (%.1f dB)\n", on, off, sig::db(off / on));
    CHECK_LT(sig::db(off / on), -20.0);   // the VCA is closed in the gap
}

// A slid step holds its gate through into the next step: no release between.
TEST("gate: slide holds the gate across the step boundary") {
    acid::Engine e(kSR);
    acid::Engine::Bind b(e);
    dry_voice();
    for (int i = 0; i < 16; ++i) acid_seq_set_step(i, 36 + (i % 2) * 7, 4, 100, 1);   // every step slides
    acid_seq_set_pattern_length(16);
    acid_seq_set_bpm(120.0f);
    acid_seq_set_swing(0.5f);
    acid_seq_play();
    // 20 ms windows: wide enough to average a 65 Hz cycle, so a trough in
    // the waveform isn't mistaken for the VCA closing.
    auto x = mono_render(e, kSR * 2);
    auto win = [&](int ms) { return sig::rms(&x[static_cast<size_t>(ms) * kSR / 1000], kSR / 50); };
    double gap = 1e9, held = 0;
    for (int s = 4; s < 12; ++s) {
        gap = std::min(gap, win(s * 125 + 100));   // where an unslid step is silent
        held += win(s * 125 + 30);
    }
    held /= 8;
    std::printf("      min level in the gap %.4f vs mid-step %.4f\n", gap, held);
    CHECK(gap > 0.5 * held);
}

// ── Slide ───────────────────────────────────────────────────────────────────
// Whittle: the slide is an RC on the pitch CV, reaching the target in about
// 60 ms. Open303 uses a 60 ms slide time. Measured here as the time to cover
// 90% of the pitch distance.
TEST("slide: pitch glides with an exponential, ~60 ms to 90%") {
    acid::Engine e(kSR);
    acid::Engine::Bind b(e);
    dry_voice(0.9f);
    acid_note_on(110.0f, 0, 0, 0.0f);
    mono_render(e, kSR / 4);
    acid_note_on(220.0f, 0, 1, 0.0f);    // slide up an octave
    auto x = mono_render(e, kSR / 4);
    // Instantaneous pitch from zero crossings.
    std::vector<std::pair<double, double>> f;   // (time ms, hz)
    int last = -1;
    for (size_t i = 1; i < x.size(); ++i)
        if (x[i - 1] < 0 && x[i] >= 0) {
            if (last >= 0) f.push_back({1000.0 * i / kSR, double(kSR) / double(static_cast<int>(i) - last)});
            last = static_cast<int>(i);
        }
    double t90 = -1;
    for (auto [t, hz] : f) if (std::log2(hz / 110.0) >= 0.9) { t90 = t; break; }
    std::printf("      90%% of the octave reached at %.0f ms\n", t90);
    CHECK(t90 > 35 && t90 < 110);
}

// ── Oscillator ──────────────────────────────────────────────────────────────
// Alias energy (anything not on a harmonic) of the raw saw and square at a
// high note, filter wide open. Measured below 15 kHz, where it's audible.
TEST("osc: aliasing at a high note stays under -65 dB") {
    for (int wf : {0, 1}) {
        acid::Engine e(kSR);
        acid::Engine::Bind b(e);
        dry_voice(1.0f);
        acid_set_waveform(wf);
        const double f0 = 1318.5;   // E6, near the top of the 303's range
        acid_note_on(static_cast<float>(f0), 0, 0, 0.0f);
        mono_render(e, kSR / 4);
        auto x = mono_render(e, 1 << 16);
        auto s = sig::spectrum(x, /*blackman-harris=*/true);   // Hann's leakage floor is ~-58 dB
        const double bin = double(kSR) / (2.0 * double(s.size()));
        double tot = 0, inh = 0;
        for (size_t i = 1; i < s.size(); ++i) {
            const double f = double(i) * bin;
            if (f > 15000) break;
            const double p = s[i] * s[i];
            tot += p;
            const double h = std::round(f / f0);
            if (h < 1 || std::fabs(f - h * f0) > 6 * bin) inh += p;
        }
        const double db = 10 * std::log10(inh / tot);
        std::printf("      %s: alias energy %.1f dB\n", wf ? "square" : "saw", db);
        CHECK_LT(db, -65.0);
    }
}

// The VCO runs freely; a new note must not reset its phase (that is a click
// on every note). Check for the step discontinuity a reset would cause.
TEST("osc: no phase reset click on retrigger") {
    acid::Engine e(kSR);
    acid::Engine::Bind b(e);
    dry_voice(1.0f);
    acid_note_on(55.0f, 0, 0, 0.0f);
    mono_render(e, kSR / 10 + 37);           // stop mid-cycle
    acid_note_on(55.0f, 0, 0, 0.0f);
    auto x = mono_render(e, 64);
    // A reset jumps the saw to -1 on the first sample.
    std::vector<float> pre(1);
    double jump = std::fabs(x[1] - x[0]);
    std::printf("      first-sample step %.3f\n", jump);
    CHECK_LT(jump, 0.3);
}

// The 303 square is shaped from the saw by a transistor, not an ideal
// comparator: its harmonics fall faster than an ideal square's 1/n.
// Open303 models it as tanh(36.9 dB * saw + offset).
TEST("osc: square is shaped (upper harmonics below an ideal square's)") {
    acid::Engine e(kSR);
    acid::Engine::Bind b(e);
    dry_voice(1.0f);
    acid_set_waveform(1);
    acid_note_on(110.0f, 0, 0, 0.0f);
    mono_render(e, kSR / 4);
    auto x = mono_render(e, 1 << 15);
    auto s = sig::spectrum(x);
    const double bin = double(kSR) / (2.0 * double(s.size()));
    auto h = [&](int k) { return s[static_cast<size_t>(std::lround(110.0 * k / bin))]; };
    const double r1 = h(1), r15 = h(15);
    std::printf("      15th harmonic %.1f dB under the fundamental (ideal square ~ -23.5)\n", sig::db(r15 / r1));
    CHECK_LT(sig::db(r15 / r1), -26.0);
}

// ── Filter ──────────────────────────────────────────────────────────────────
// Stinchcombe: the 303's feedback coupling weakens the resonance at low
// cutoff. His measured transfer function puts the full-RES peak at 100 Hz
// ~13 dB under the peak at 3.5 kHz; check we're in that neighbourhood.
TEST("filter: resonance weakens at low cutoff (feedback highpass)") {
    // Small-signal sine response at the peak vs the passband, at near-full
    // RES, measured the same way the fit to Stinchcombe's curve was made.
    auto gain = [](float f, float fc, float k) {
        acid::dsp::DiodeLadder fl;
        acid::dsp::FeedbackHP fb;
        const float fs = 2.0f * kSR;
        fb.set(fs);
        const float g = std::tan(acid::dsp::kPi * fc / acid::dsp::DiodeLadder::kResRatio / fs);
        const int n = static_cast<int>(fs);
        double si = 0, so = 0;
        for (int i = 0; i < n; ++i) {
            const float x = 0.001f * std::sin(2 * acid::dsp::kPi * f * static_cast<float>(i) / fs);
            const float y = fl.process_fb(x, g, k, fb);
            if (i > n / 2) { si += double(x) * x; so += double(y) * y; }
        }
        return 10 * std::log10(so / si);
    };
    auto peak_db = [&](float fc) {
        const float k = 0.97f * acid::dsp::DiodeLadder::kSelfOsc;
        double best = -999;
        for (float m = 0.75f; m <= 1.3f; m += 0.05f) best = std::max(best, gain(fc * m, fc, k));
        return best - gain(fc / 6, fc, 0.0f);
    };
    const double lo = peak_db(100.0f), hi = peak_db(3500.0f);
    std::printf("      resonant peak over passband: 100 Hz %.1f dB, 3.5 kHz %.1f dB (%.1f dB apart)\n",
                lo, hi, hi - lo);
    CHECK(hi - lo > 8.0 && hi - lo < 18.0);
}

// ── Envelopes ───────────────────────────────────────────────────────────────
// Changing gate state mid-block must take effect on that sample: the release
// is identical whatever block size the host renders in, and starts at the
// gate edge (72.9 ms) — the -20 dB point then trails it by the 8 ms release
// RC (8 ms × ln 10 ≈ 18 ms).
TEST("envelope: release is sample-accurate, not block-quantised") {
    std::vector<int> seen;
    for (int block : {64, 256, 1024}) {
        acid::Engine e(kSR);
        acid::Engine::Bind b(e);
        dry_voice();
        acid_seq_set_step(0, 36, 0, 100, 1);
        for (int i = 1; i < 16; ++i) acid_seq_set_step(i, 36, 1, 100, 1);   // rests
        acid_seq_set_pattern_length(16);
        acid_seq_set_bpm(120.0f);
        acid_seq_set_swing(0.5f);
        acid_seq_play();
        std::vector<float> lr(2 * kSR / 2);
        for (int i = 0; i < kSR / 2; i += block) e.render(&lr[2 * static_cast<size_t>(i)], std::min(block, kSR / 2 - i));
        std::vector<float> m(kSR / 2);
        for (int i = 0; i < kSR / 2; ++i) m[static_cast<size_t>(i)] = lr[2 * static_cast<size_t>(i)];
        auto en = env_ms(m);
        // Gate falls at 3.5/6 of a 125 ms step = 72.9 ms. Find where the level
        // first drops 20 dB under its gated value.
        const double ref = en[30];
        int t = -1;
        for (int ms = 40; ms < 125; ++ms) if (en[static_cast<size_t>(ms)] < ref * 0.1) { t = ms; break; }
        std::printf("      block %4d: -20 dB at %d ms\n", block, t);
        CHECK(t >= 86 && t <= 94);
        seen.push_back(t);
    }
    CHECK(seen[0] == seen[1] && seen[1] == seen[2]);
}

// Hardware VEG decay ~1.2 s (Open303: 1230 ms) while the gate is held.
TEST("envelope: held note amp decay is about a second") {
    acid::Engine e(kSR);
    acid::Engine::Bind b(e);
    dry_voice(0.9f);
    acid_note_on(110.0f, 0, 0, 0.0f);
    auto en = env_ms(mono_render(e, kSR * 3));
    const double a = en[50], c = en[1050];
    const double tau = 1.0 / std::log(a / c);   // seconds per neper between 50 ms and 1050 ms
    std::printf("      VEG time constant ~%.2f s\n", tau);
    CHECK(tau > 0.7 && tau < 2.0);
}

// Stacked accents rise but never step: no click on the accent charge.
TEST("accent: no discontinuity when accents stack") {
    acid::Engine e(kSR);
    acid::Engine::Bind b(e);
    dry_voice(0.3f, 0.8f, 0.6f, 0.3f);
    for (int i = 0; i < 16; ++i) acid_seq_set_step(i, 36, 2 | 4, 100, 1);   // accent + slide: env never retriggers
    acid_seq_set_pattern_length(16);
    acid_seq_set_bpm(130.0f);
    acid_seq_play();
    auto x = mono_render(e, kSR);
    // Largest sample-to-sample jump relative to signal level.
    double mx = 0, lvl = sig::rms(x.data() + kSR / 4, kSR / 2);
    for (size_t i = kSR / 4 + 1; i < x.size(); ++i) mx = std::max(mx, double(std::fabs(x[i] - x[i - 1])));
    std::printf("      max step %.3f at rms %.3f\n", mx, lvl);
    CHECK_LT(mx / lvl, 3.5);
}

// Whittle's accent sweep: with RES up, consecutive accents charge the sweep
// cap higher each time, so the filter peak climbs note after note.
std::vector<float> accent_peaks(float res) {
    acid::Engine e(kSR);
    acid::Engine::Bind b(e);
    dry_voice(0.25f, res, 0.3f, 0.3f);
    acid_set_accent_amt(1.0f);
    for (int i = 0; i < 16; ++i) acid_seq_set_step(i, 36, i < 4 ? 2 : 1, 100, 1);   // 4 accents, then rests
    acid_seq_set_pattern_length(16);
    acid_seq_set_bpm(130.0f);
    acid_seq_set_swing(0.5f);
    acid_seq_play();
    std::vector<float> peaks;
    const int step = static_cast<int>(kSR * 60.0 / 130.0 / 4.0);
    std::vector<float> lr(2 * static_cast<size_t>(step));
    for (int s = 0; s < 4; ++s) {
        float pk = 0;
        for (int i = 0; i < step; i += 64) {
            e.render(&lr[2 * static_cast<size_t>(i)], std::min(64, step - i));
            pk = std::max(pk, acid_live_fc());
        }
        peaks.push_back(pk);
    }
    std::printf("      RES %.2f, peak cutoff per accent: %.0f %.0f %.0f %.0f Hz\n", res,
                peaks[0], peaks[1], peaks[2], peaks[3]);
    return peaks;
}

TEST("accent: consecutive accents push the cutoff higher each time") {
    auto p = accent_peaks(0.85f);
    // It climbs, then levels off as the cap nears full charge.
    CHECK(p[1] > p[0] * 1.2f);
    CHECK(p[2] > p[1] * 1.1f);
    CHECK(p[3] >= p[2] * 0.97f);
}

// At RES fully down the sweep is the raw MEG pulse: no climb.
TEST("accent: with RES at zero, repeated accents don't climb") {
    auto p = accent_peaks(0.0f);
    CHECK_NEAR(p[3] / p[0], 1.0, 0.05);
}

// ── Sample-rate independence ────────────────────────────────────────────────
// The same pattern at 44.1, 48 and 96 kHz should sound the same: band
// balance within 1 dB.
TEST("sample rate: 44.1 / 48 / 96 kHz render the same spectrum") {
    auto bands = [](int sr) {
        acid::Engine e(sr);
        acid::Engine::Bind b(e);
        dry_voice(0.35f, 0.8f, 0.6f, 0.4f);
        const int notes[8] = {36, 48, 36, 39, 36, 43, 36, 46};
        for (int i = 0; i < 16; ++i) acid_seq_set_step(i, notes[i % 8], i % 4 == 0 ? 2 : 0, 100, 1);
        acid_seq_set_pattern_length(16);
        acid_seq_set_bpm(126.0f);
        acid_seq_play();
        std::vector<float> lr(2 * static_cast<size_t>(sr) * 2), m(static_cast<size_t>(sr) * 2);
        for (int i = 0; i < sr * 2; i += 256) e.render(&lr[2 * static_cast<size_t>(i)], std::min(256, sr * 2 - i));
        for (int i = 0; i < sr * 2; ++i) m[static_cast<size_t>(i)] = lr[2 * static_cast<size_t>(i)];
        auto s = sig::spectrum(m);
        const double bin = double(sr) / (2.0 * double(s.size()));
        std::vector<double> out;
        for (auto [lo, hi] : {std::pair{40.0, 200.0}, {200.0, 1000.0}, {1000.0, 4000.0}, {4000.0, 12000.0}}) {
            double e2 = 0, t = 0;
            for (size_t i = 1; i < s.size(); ++i) {
                const double f = double(i) * bin, p = s[i] * s[i];
                if (f < 20000) t += p;
                if (f >= lo && f < hi) e2 += p;
            }
            out.push_back(10 * std::log10(e2 / t));
        }
        return out;
    };
    auto a = bands(44100), b48 = bands(48000), c = bands(96000);
    for (size_t i = 0; i < a.size(); ++i) {
        std::printf("      band %zu: %.1f / %.1f / %.1f dB\n", i, a[i], b48[i], c[i]);
        CHECK_NEAR(a[i], c[i], 1.5);
        CHECK_NEAR(b48[i], c[i], 1.5);
    }
}

TEST_MAIN()
