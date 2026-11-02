// Engine-level tests: the full voice + sequencer + FX through the C API.
#include "audio.hpp"
#include "engine.hpp"
#include "sig.hpp"
#include "t.hpp"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {

int step(int midi, int flags, int ratchet = 1) {
    return (midi & 0xFF) | ((flags & 7) << 8) | (100 << 11) | ((ratchet - 1) << 18);
}

// A busy acid line used by most tests.
const int kLine[16] = {
    step(36, 2), step(36, 0), step(48, 4), step(36, 0),
    step(39, 2), step(36, 1), step(43, 4), step(46, 0),
    step(36, 2, 2), step(36, 0), step(48, 2), step(51, 4),
    step(36, 0), step(34, 1), step(36, 2), step(41, 4),
};

void setup(float cutoff = 0.3f, float res = 0.85f, float drive = 0.35f) {
    acid_set_cutoff(cutoff);   acid_set_resonance(res);
    acid_set_env_mod(0.7f);    acid_set_decay(0.35f);
    acid_set_accent_amt(0.7f); acid_set_drive(drive);
    acid_set_volume(0.7f);     acid_set_tuning_semi(0.0f);
    acid_set_waveform(0);
    acid_set_delay_mix(0.3f);  acid_set_delay_feedback(0.45f);
    acid_set_delay_division(3);
    acid_set_od_amt(0.25f);
    acid_set_rev_mix(0.25f);   acid_set_rev_size(0.6f); acid_set_rev_damp(0.4f);
    acid_seq_set_drum_lane(0, 0x1111);
    acid_seq_set_drum_lane(2, 0x4444);
    acid_seq_set_drum_lane(1, 0x1010);
    acid_seq_set_drum_master(0.8f);
    for (int i = 0; i < 16; ++i)
        acid_seq_set_step(i, kLine[i] & 0xFF, (kLine[i] >> 8) & 7, 100, ((kLine[i] >> 18) & 3) + 1);
    acid_seq_set_pattern_length(16);
    acid_seq_set_bpm(128.0f);
    acid_seq_set_swing(0.56f);
}

// Render `seconds` of stereo from a fresh engine with the standard setup.
std::vector<float> render(float seconds, int sr = 44100, float cutoff = 0.3f,
                          float res = 0.85f, float drive = 0.35f) {
    acid::Engine e(sr);
    acid::Engine::Bind b(e);
    setup(cutoff, res, drive);
    acid_seq_play();
    std::vector<float> out(size_t(seconds * sr) * 2);
    for (size_t i = 0; i < out.size() / 2; i += 256)
        e.render(&out[2 * i], int(std::min<size_t>(256, out.size() / 2 - i)));
    return out;
}

std::vector<float> mono(const std::vector<float>& lr) {
    std::vector<float> m(lr.size() / 2);
    for (size_t i = 0; i < m.size(); ++i) m[i] = 0.5f * (lr[2 * i] + lr[2 * i + 1]);
    return m;
}

}  // namespace

TEST("renders finite, non-silent, below full scale") {
    auto x = render(4.0f);
    float peak = 0;
    bool finite = true;
    for (float v : x) { finite = finite && std::isfinite(v); peak = std::max(peak, std::fabs(v)); }
    CHECK(finite);
    CHECK(peak > 0.1f);
    CHECK_LT(peak, 1.0);
    const double level = sig::db(sig::rms(x.data(), x.size()));
    std::printf("      level %.1f dBFS, peak %.2f\n", level, peak);
    CHECK(level > -24.0 && level < -6.0);
}

TEST("deterministic: two engines produce identical output") {
    auto a = render(2.0f), b = render(2.0f);
    REQUIRE(a.size() == b.size());
    size_t diff = 0;
    for (size_t i = 0; i < a.size(); ++i) diff += a[i] != b[i];
    CHECK(diff == 0);
}

TEST("Bind isolates engines from the live instance") {
    // Knob changes made through a Bind must not reach the default instance.
    acid_set_cutoff(0.11f);
    {
        acid::Engine e;
        acid::Engine::Bind b(e);
        acid_set_cutoff(0.99f);
    }
    // The default instance renders silence when idle; set it playing a note
    // and compare its filter against an engine with cutoff 0.11.
    acid::Engine ref;
    {
        acid::Engine::Bind b(ref);
        acid_set_cutoff(0.11f);
    }
    std::vector<float> live(2 * 4096), mine(2 * 4096);
    acid_note_on(110.0f, 0, 0, 0.1f);
    { acid::Engine::Bind b(ref); acid_note_on(110.0f, 0, 0, 0.1f); }
    acid::render(live.data(), 4096);
    ref.render(mine.data(), 4096);
    size_t diff = 0;
    for (size_t i = 0; i < live.size(); ++i) diff += live[i] != mine[i];
    CHECK(diff == 0);
    acid_note_off();
}

TEST("stereo: drums are panned, bass is centered") {
    // Bass only: L == R.
    {
        acid::Engine e;
        acid::Engine::Bind b(e);
        setup();
        acid_set_delay_mix(0); acid_set_rev_mix(0);
        acid_seq_set_drum_master(0.0f);
        acid_seq_play();
        std::vector<float> x(2 * 44100);
        e.render(x.data(), 44100);
        double side = 0, mid = 0;
        for (size_t i = 0; i < x.size(); i += 2) { side += std::fabs(x[i] - x[i + 1]); mid += std::fabs(x[i] + x[i + 1]); }
        CHECK_LT(side / mid, 1e-6);
    }
    // With hats on: sides differ.
    auto x = render(2.0f);
    double side = 0, mid = 0;
    for (size_t i = 0; i < x.size(); i += 2) { side += std::fabs(x[i] - x[i + 1]); mid += std::fabs(x[i] + x[i + 1]); }
    CHECK(side / mid > 0.02);
}

TEST("full resonance peaks at the cutoff knob's frequency") {
    // With RES at max and no envelope, the strongest partial within an
    // octave of the cutoff should sit on it (within a small tolerance:
    // harmonics of the note quantise where the peak can land).
    for (float cut : {0.45f, 0.55f, 0.65f}) {
        acid::Engine e;
        acid::Engine::Bind b(e);
        setup(cut, 1.0f, 0.0f);
        acid_set_env_mod(0.0f);
        acid_set_delay_mix(0); acid_set_rev_mix(0); acid_set_od_amt(0);
        acid_seq_set_drum_master(0.0f);
        acid_set_volume(0.5f);
        acid_note_on(27.5f, 0, 0, 0.1f);   // low A: dense harmonics
        std::vector<float> x(2 * 44100 * 2);
        e.render(x.data(), 44100 * 2);
        auto m = mono(x);
        std::vector<float> tail(m.begin() + 44100, m.end());
        auto s = sig::spectrum(tail);
        const double want = acid::cutoff_knob_hz(cut);
        const double bin  = 44100.0 / (2.0 * double(s.size()));
        size_t best = size_t(want / 1.41 / bin);
        for (size_t i = best; i < size_t(want * 1.41 / bin); ++i) if (s[i] > s[best]) best = i;
        const double hz = double(best) * bin;
        std::printf("      cutoff %.2f: knob %.0f Hz, peak %.0f Hz\n", cut, want, hz);
        CHECK_NEAR(std::log2(hz / want), 0.0, 0.15);
        acid_note_off();
    }
}

TEST("stems: mix equals the master of synth + drums") {
    namespace fs = std::filesystem;
    const auto dir = fs::temp_directory_path() / "acidflow_stems_test";
    fs::create_directories(dir);
    const auto mix = (dir / "mix.wav").string(), syn = (dir / "synth.wav").string(),
               drm = (dir / "drums.wav").string();
    acid::Engine e;
    acid::Engine::Bind b(e);
    setup();
    REQUIRE(acid_render_stems(mix.c_str(), syn.c_str(), drm.c_str(), kLine, 16, 128.0f, 0.56f, 1) == 0);
    for (auto& p : {mix, syn, drm}) {
        REQUIRE(fs::exists(p));
        CHECK(fs::file_size(p) > 44 + 6 * 44100);
    }
    CHECK(fs::file_size(mix) == fs::file_size(syn));
    CHECK(fs::file_size(mix) == fs::file_size(drm));
    fs::remove_all(dir);
}

TEST("golden: band balance of the reference line") {
    // A coarse fingerprint: energy share per band. Loose enough to survive
    // float differences across compilers, tight enough to catch a DSP change.
    auto m = mono(render(4.0f));
    auto s = sig::spectrum(m);
    const double bin = 44100.0 / (2.0 * double(s.size()));
    auto band = [&](double a, double b) {
        double e = 0, t = 0;
        for (size_t i = 1; i < s.size(); ++i) {
            const double p = s[i] * s[i];
            t += p;
            const double f = double(i) * bin;
            if (f >= a && f < b) e += p;
        }
        return 10 * std::log10(e / t);
    };
    const double sub = band(20, 100), low = band(100, 1000), mid = band(1000, 5000),
                 high = band(5000, 10000), air = band(15000, 22050);
    std::printf("      bands: %.1f %.1f %.1f %.1f %.1f dB\n", sub, low, mid, high, air);
    CHECK_NEAR(sub,  -3.1, 1.5);
    CHECK_NEAR(low,  -3.1, 1.5);
    CHECK_NEAR(mid, -24.5, 3.0);
    CHECK_NEAR(high, -23.2, 3.0);
    CHECK_NEAR(air,  -21.4, 4.0);
}

TEST("real-time headroom: renders 10x faster than real time") {
    acid::Engine e;
    acid::Engine::Bind b(e);
    setup();
    acid_seq_play();
    std::vector<float> x(2 * 256);
    const int blocks = 44100 * 10 / 256;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < blocks; ++i) e.render(x.data(), 256);
    const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("      10 s of audio in %.0f ms (%.0fx real time)\n", s * 1000, 10.0 / s);
    CHECK_LT(s, 1.0);
}

TEST_MAIN()
