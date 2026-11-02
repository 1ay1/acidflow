// Unit tests for the DSP blocks in src/dsp.hpp.
#include "dsp.hpp"
#include "sig.hpp"
#include "t.hpp"

#include <vector>

using namespace acid::dsp;

TEST("halfband round trip is flat to 18 kHz at 44.1k") {
    for (double f : {100.0, 1000.0, 5000.0, 12000.0, 18000.0}) {
        Upsampler2x up;
        Downsampler2x dn;
        std::vector<float> in, out;
        for (int i = 0; i < 16384; ++i) {
            const float x = float(std::sin(2 * M_PI * f / 44100.0 * i));
            float e, o;
            up.process(x, e, o);
            in.push_back(x);
            out.push_back(dn.process(e, o));
        }
        const double g = sig::rms(out.data() + 1024, 15000) / sig::rms(in.data() + 1024, 15000);
        CHECK_NEAR(sig::db(g), 0.0, 0.05);
    }
}

TEST("upsampler rejects images by 80 dB") {
    Upsampler2x up;
    std::vector<float> y;
    for (int i = 0; i < 32768; ++i) {
        float e, o;
        up.process(float(std::sin(2 * M_PI * 15000.0 / 44100.0 * i)), e, o);
        y.push_back(e);
        y.push_back(o);
    }
    auto m = sig::spectrum(y);
    const double bin = 88200.0 / (2.0 * double(m.size()));
    const double sigp = m[size_t(15000 / bin + 0.5)];
    const double img  = m[size_t((44100 - 15000) / bin + 0.5)];
    CHECK_LT(sig::db(img / sigp), -80.0);
}

TEST("diode ladder: stable below self-osc, rings above") {
    const float fs = 88200, fc = 1000, g = std::tan(kPi * fc / fs);
    auto tail = [&](float frac) {
        DiodeLadder f;
        float peak = 0;
        for (int i = 0; i < 88200; ++i) {
            const float y = f.process(i == 0 ? 0.01f : 0.0f, g, frac * DiodeLadder::kSelfOsc);
            if (i > 70000) peak = std::max(peak, std::fabs(y));
        }
        return peak;
    };
    CHECK_LT(tail(0.95f), 1e-6);
    CHECK(tail(1.08f) > 1e-3);
}

TEST("diode ladder: self-osc pitch matches kResRatio") {
    // Stage frequency fc / kResRatio should put the oscillation at fc.
    const float fs = 88200, fc = 800;
    const float g = std::tan(kPi * fc / DiodeLadder::kResRatio / fs);
    DiodeLadder f;
    std::vector<float> y;
    for (int i = 0; i < 88200 * 2; ++i) {
        const float v = f.process(i == 0 ? 0.01f : 0.0f, g, 1.05f * DiodeLadder::kSelfOsc);
        if (i >= 88200) y.push_back(v);
    }
    const double hz = sig::peak_hz(sig::spectrum(y), fs);
    CHECK_NEAR(hz, fc, fc * 0.03);
}

TEST("diode ladder: unity DC gain with no feedback") {
    DiodeLadder f;
    float y = 0;
    for (int i = 0; i < 20000; ++i) y = f.process(0.1f, 0.05f, 0.0f);
    CHECK_NEAR(y, std::tanh(0.1f), 1e-4);
}

TEST("diode ladder: never blows up at max feedback and hot input") {
    DiodeLadder f;
    bool finite = true;
    float peak = 0;
    for (int i = 0; i < 200000; ++i) {
        const float x = 4.0f * float(std::sin(i * 0.013));
        const float y = f.process(x, 0.4f, 1.3f * DiodeLadder::kSelfOsc);
        finite = finite && std::isfinite(y);
        peak = std::max(peak, std::fabs(y));
    }
    CHECK(finite);
    CHECK_LT(peak, 4.0);
}

TEST("2x oversampled ADAA tanh cuts aliasing of a hard-driven 5 kHz sine") {
    // The engine runs its saturators at 2× with ADAA. Compare that against a
    // plain tanh at 1× on the same input.
    const double f0 = 4987.0;   // off-grid so aliases don't land on harmonics
    auto measure = [&](bool better) {
        TanhADAA s;
        Upsampler2x up;
        Downsampler2x dn;
        std::vector<float> y;
        for (int i = 0; i < 65536; ++i) {
            const float x = 6.0f * float(std::sin(2 * M_PI * f0 / 44100.0 * i));
            if (!better) { y.push_back(std::tanh(x)); continue; }
            float e, o;
            up.process(x, e, o);
            e = s.process(e);
            o = s.process(o);
            y.push_back(dn.process(e, o));
        }
        return sig::inharmonic_db(sig::spectrum(y), 44100.0, f0);
    };
    const double naive = measure(false), better = measure(true);
    std::printf("      aliasing: naive %.1f dB, 2x+ADAA %.1f dB\n", naive, better);
    CHECK_LT(better, naive - 15.0);
}

TEST("tanh ADAA equals tanh averaged over each step") {
    // ADAA output is the mean of tanh over [x[n-1], x[n]]; check it against a
    // numeric average, which is what it should be exactly.
    TanhADAA s;
    double err = 0, prev = 0;
    for (int i = 0; i < 20000; ++i) {
        const double x = 3.0 * std::sin(2 * M_PI * 50.0 / 44100.0 * i);
        const float y = s.process(float(x));
        double avg = 0;
        for (int k = 0; k < 64; ++k) avg += std::tanh(prev + (x - prev) * (k + 0.5) / 64.0);
        avg /= 64.0;
        if (i > 0) err = std::max(err, std::fabs(double(y) - avg));
        prev = double(float(x));
    }
    CHECK_LT(err, 1e-4);
}

TEST("plate reverb: decays, stays finite, is stereo") {
    static DattorroPlate p;
    p.set_sample_rate(48000);
    std::vector<float> l, r;
    for (int i = 0; i < 48000 * 6; ++i) {
        float ol, orr;
        p.process(i == 0 ? 1.0f : 0.0f, 0.6f, 0.3f, ol, orr);
        l.push_back(ol);
        r.push_back(orr);
    }
    const double early = sig::rms(l.data() + 4800, 24000);
    const double late  = sig::rms(l.data() + 48000 * 5, 24000);
    CHECK(early > 1e-4);
    CHECK_LT(late, early * 0.05);
    // Decorrelated sides.
    double num = 0, dl = 0, dr = 0;
    for (size_t i = 4800; i < 48000; ++i) { num += l[i] * r[i]; dl += l[i] * l[i]; dr += r[i] * r[i]; }
    CHECK_LT(std::fabs(num / std::sqrt(dl * dr)), 0.5);
}

TEST("plate reverb: bounded at max size") {
    static DattorroPlate p;
    p.set_sample_rate(44100);
    float peak = 0;
    for (int i = 0; i < 44100 * 20; ++i) {
        float ol, orr;
        p.process(float(std::sin(i * 0.05)) * (i < 44100 ? 1.0f : 0.0f), 1.0f, 0.0f, ol, orr);
        peak = std::max({peak, std::fabs(ol), std::fabs(orr)});
    }
    CHECK(std::isfinite(peak));
    CHECK_LT(peak, 4.0);
}

TEST("tape delay: first repeat on the left, then the right") {
    static TapeDelay d;
    d.reset();
    const float sr = 44100, t = 4410;
    d.set_coefs(sr, 0.5f);
    // Let the smoothed delay time settle before the impulse.
    for (int i = 0; i < 44100; ++i) { float a, b; d.process(0.0f, t, 0.5f, sr, a, b); }
    int first_l = -1, first_r = -1;
    for (int i = 0; i < 20000; ++i) {
        float a, b;
        d.process(i == 0 ? 1.0f : 0.0f, t, 0.5f, sr, a, b);
        if (first_l < 0 && std::fabs(a) > 0.05f) first_l = i;
        if (first_r < 0 && std::fabs(b) > 0.05f) first_r = i;
    }
    REQUIRE(first_l > 0);
    REQUIRE(first_r > 0);
    CHECK_NEAR(first_l, t, 40);
    CHECK_NEAR(first_r, 2 * t, 80);
}

TEST("limiter holds -1 dBFS") {
    Limiter lim;
    lim.set_sample_rate(44100);
    float peak = 0;
    for (int i = 0; i < 44100; ++i) {
        float l = 3.0f * float(std::sin(i * 0.02)), r = -2.0f * float(std::sin(i * 0.03));
        lim.process(l, r);
        if (i > 256) peak = std::max({peak, std::fabs(l), std::fabs(r)});
    }
    CHECK_LT(peak, 0.95);
}

TEST_MAIN()
