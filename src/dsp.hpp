#pragma once
// DSP building blocks for the 303 voice and the master bus. Header-only and
// free of engine state so each block can be tested on its own.

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace acid::dsp {

inline constexpr float kPi = 3.14159265358979323846f;

// ── Saturation with first-order ADAA ─────────────────────────────────────────
// Antiderivative anti-aliasing (Parker et al. 2016): instead of f(x[n]) we
// output (F(x[n]) - F(x[n-1])) / (x[n] - x[n-1]), where F is the antiderivative
// of f. That is f averaged over the segment between samples, which kills most
// of the aliasing tanh makes when driven hard. Costs one log/cosh per sample.

// log(cosh(x)) without overflow for large |x|. Double: the ADAA quotient
// below subtracts two nearly equal values, and float loses ~3 digits there.
inline double logcosh(double x) {
    const double a = std::fabs(x);
    return a > 18.0 ? a - 0.6931471805599453 : std::log(std::cosh(x));
}

struct TanhADAA {
    double x1 = 0.0;   // previous input
    double F1 = 0.0;   // F(previous input)

    float process(float xf) {
        const double x = xf;
        const double F = logcosh(x);
        const double d = x - x1;
        double y;
        // Ill-conditioned when the step is tiny: use f at the midpoint.
        if (std::fabs(d) < 1e-7) y = std::tanh(0.5 * (x + x1));
        else                     y = (F - F1) / d;
        x1 = x;
        F1 = F;
        return static_cast<float>(y);
    }
    void reset() { x1 = 0.0; F1 = 0.0; }
};

// ── 303 diode-ladder VCF ─────────────────────────────────────────────────────
// The 303 filter is a diode ladder, not a Moog transistor ladder. Neighbouring
// stages are coupled through their shared diodes, so it isn't four equal
// one-poles in a row. Linear model (normalised to ω = 1):
//
//   y1' = u  - y1 + y2 - y1        (first stage: no lower neighbour)
//   y2' = y1 - 2y2 + y3
//   y3' = y2 - 2y3 + y4
//   y4' = y3 - 2y4
//
// Its characteristic polynomial is s^4 + 7s^3 + 15s^2 + 10s + 1, which gives a
// gentle ~12 dB/oct knee that steepens to 24 dB/oct, and self-oscillation at
// k ≈ 18.4. A tanh on the summing node (u = tanh(x - k·y4)) adds the drive.
//
// Discretised with TPT integrators. The linear part is solved exactly by
// back-substitution (tri-diagonal), so there is no unit delay in the feedback,
// and the tanh is solved with Newton.
class DiodeLadder {
  public:
    // k at which the linear filter self-oscillates, and the frequency (as a
    // multiple of the stage frequency) it oscillates at: √(10/7).
    static constexpr float kSelfOsc = 18.39f;
    static constexpr float kResRatio = 1.1952286f;

    void reset() { s_[0] = s_[1] = s_[2] = s_[3] = 0.0f; y_ = 0.0f; }

    // g = tan(π·fc/fs) at the rate this runs at; k = feedback.
    float process(float x, float g, float k) {
        const float a  = g;
        const float i2 = 1.0f / (1.0f + 2.0f * a);
        const float G  = a * i2;
        // y4 = G·y3 + d4, then fold each stage into the one above it.
        const float d4   = s_[3] * i2;
        const float den3 = 1.0f / (1.0f - G * G);
        const float e3   = G * den3,  f3 = (G * d4 + s_[2] * i2) * den3;
        const float den2 = 1.0f / (1.0f - G * e3);
        const float e2   = G * den2,  f2 = (G * f3 + s_[1] * i2) * den2;
        const float i1   = 1.0f / (1.0f + a);
        const float G1   = a * i1;
        const float den1 = 1.0f / (1.0f - G1 * e2);
        const float e1   = G1 * den1, f1 = (G1 * f2 + s_[0] * i1) * den1;

        // y4 as an affine function of the input node u: y4 = A·u + B.
        const float A = G * e3 * e2 * e1;
        const float B = G * (e3 * (e2 * f1 + f2) + f3) + d4;

        // u = tanh(x - k(A·u + B)). Seed from the linear solution, then Newton.
        float u = std::clamp((x - k * B) / (1.0f + k * A), -3.0f, 3.0f);
        for (int it = 0; it < 3; ++it) {
            const float t  = std::tanh(x - k * (A * u + B));
            const float dr = 1.0f + k * A * (1.0f - t * t);
            u -= (u - t) / dr;
        }

        const float y1 = e1 * u  + f1;
        const float y2 = e2 * y1 + f2;
        const float y3 = e3 * y2 + f3;
        const float y4 = G  * y3 + d4;
        s_[0] = 2.0f * y1 - s_[0];
        s_[1] = 2.0f * y2 - s_[1];
        s_[2] = 2.0f * y3 - s_[2];
        s_[3] = 2.0f * y4 - s_[3];
        y_ = y4;
        return y4;
    }

    float last() const { return y_; }

  private:
    float s_[4] = {};
    float y_    = 0.0f;
};

// ── 2× oversampling: half-band polyphase FIR ─────────────────────────────────
// 59-tap Kaiser (β = 8) half-band: 0.002 dB ripple to 0.205·fs2, −80 dB from
// 0.295·fs2. Every other tap is zero, so each phase is 15 symmetric pairs.
struct HalfBand {
    static constexpr int kPairs = 15;
    static constexpr int kHist  = 32;       // ≥ 2·kPairs, power of two
    // Non-zero taps from the outside in; the center tap is 0.5.
    static constexpr float kH[kPairs] = {
         0.0000256716f, -0.0001261095f,  0.0003527452f, -0.0007870779f,
         0.0015373517f, -0.0027419757f,  0.0045754671f, -0.0072618277f,
         0.0111061506f, -0.0165700799f,  0.0244605157f, -0.0364567395f,
         0.0569177764f, -0.1019296190f,  0.3168977509f,
    };
};

// Upsample by 2: one input → two outputs at the doubled rate. Group delay is
// kPairs input samples.
struct Upsampler2x {
    float z[HalfBand::kHist] = {};
    int   p = 0;

    void process(float x, float& even, float& odd) {
        constexpr int M = HalfBand::kHist - 1;
        p = (p + M) & M;
        z[p] = x;
        auto at = [&](int i) { return z[(p + i) & M]; };
        constexpr int P = HalfBand::kPairs;
        // Even phase: the delayed center. Odd phase: the symmetric pair sum.
        float acc = 0.0f;
        for (int t = 0; t < P; ++t)
            acc += HalfBand::kH[t] * (at(t) + at(2 * P - 1 - t));
        even = at(P);
        odd  = 2.0f * acc;
    }
    void reset() { for (auto& v : z) v = 0.0f; p = 0; }
};

// Downsample by 2: two inputs at the doubled rate → one output.
struct Downsampler2x {
    float ze[HalfBand::kHist] = {};
    float zo[HalfBand::kHist] = {};
    int   p = 0;

    float process(float even, float odd) {
        constexpr int M = HalfBand::kHist - 1;
        constexpr int P = HalfBand::kPairs;
        p = (p + M) & M;
        ze[p] = even;
        zo[p] = odd;
        auto o = [&](int i) { return zo[(p + i) & M]; };
        float acc = 0.0f;
        for (int t = 0; t < P; ++t)
            acc += HalfBand::kH[t] * (o(t) + o(2 * P - 1 - t));
        // Odd samples sit at ±1, ±3, ... around the center, which is the even
        // sample P-1 back.
        return acc + 0.5f * ze[(p + P - 1) & M];
    }
    void reset() { for (auto& v : ze) v = 0.0f; for (auto& v : zo) v = 0.0f; p = 0; }
};

// ── Dattorro plate reverb ────────────────────────────────────────────────────
// Jon Dattorro, "Effect Design Part 1" (JAES 1997). Input diffusion (4
// allpasses) feeds a figure-eight tank of two modulated allpass + delay
// branches; stereo comes from tapping the tank at seven fixed points per
// side. Delay lengths are the paper's (at 29761 Hz), scaled to the rate.
class DattorroPlate {
  public:
    void set_sample_rate(int sr);
    void reset();
    // decay 0..1 → tank feedback, damp 0..1 → tank HF loss.
    void process(float in, float decay, float damp, float& out_l, float& out_r);

  private:
    struct Line {
        float* buf = nullptr;
        int    len = 0;
        int    w   = 0;
        float  read(int d) const {
            int i = w - d;
            while (i < 0) i += len;
            return buf[i];
        }
        float  read_frac(float d) const {
            int   di = static_cast<int>(d);
            float fr = d - static_cast<float>(di);
            return read(di) * (1.0f - fr) + read(di + 1) * fr;
        }
        void   write(float v) { buf[w] = v; if (++w >= len) w = 0; }
    };
    // Allpass built on a line: y = -g·x + d; line ← x + g·y
    static float allpass(Line& l, int d, float g, float x) {
        const float dl = l.read(d);
        const float v  = x + g * dl;
        l.write(v);
        return dl - g * v;
    }

    static constexpr int kMaxLen = 8192;   // ≥ longest line at 96 kHz
    float mem_[12][kMaxLen] = {};
    Line  in_ap_[4];
    Line  tank_ap1_[2], tank_dl1_[2], tank_ap2_[2], tank_dl2_[2];
    Line  predelay_;
    int   len_in_[4]{}, len_ap1_[2]{}, len_dl1_[2]{}, len_ap2_[2]{}, len_dl2_[2]{};
    int   taps_[14]{};
    float bw_lp_ = 0.0f;
    float damp_lp_[2] = {};
    float lfo_ph_ = 0.0f, lfo_inc_ = 0.0f, mod_depth_ = 0.0f;
    float scale_ = 1.0f;
};

inline void DattorroPlate::set_sample_rate(int sr) {
    scale_ = static_cast<float>(sr) / 29761.0f;
    auto sc = [&](int n) { return std::clamp(static_cast<int>(std::lround(n * scale_)), 2, kMaxLen - 64); };
    const int in_l[4]  = {142, 107, 379, 277};
    const int ap1[2]   = {672, 908};
    const int dl1[2]   = {4453, 4217};
    const int ap2[2]   = {1800, 2656};
    const int dl2[2]   = {3720, 3163};
    for (int i = 0; i < 4; ++i) len_in_[i] = sc(in_l[i]);
    for (int i = 0; i < 2; ++i) {
        len_ap1_[i] = sc(ap1[i]);
        len_dl1_[i] = sc(dl1[i]);
        len_ap2_[i] = sc(ap2[i]);
        len_dl2_[i] = sc(dl2[i]);
    }
    const int t[14] = {266, 2974, 1913, 1996, 1990, 187, 1066,
                       353, 3627, 1228, 2673, 2111, 335, 121};
    for (int i = 0; i < 14; ++i) taps_[i] = sc(t[i]);
    lfo_inc_   = 1.0f / static_cast<float>(sr);   // ~1 Hz excursion
    mod_depth_ = 8.0f * scale_;
    // Wire lines to memory. Line lengths include headroom for modulation.
    int m = 0;
    auto bind = [&](Line& l, int len) { l.buf = mem_[m++]; l.len = std::min(len + 32, kMaxLen); l.w = 0; };
    for (int i = 0; i < 4; ++i) bind(in_ap_[i], len_in_[i]);
    for (int i = 0; i < 2; ++i) {
        bind(tank_ap1_[i], len_ap1_[i] + static_cast<int>(mod_depth_) + 2);
        bind(tank_dl1_[i], len_dl1_[i]);
        bind(tank_ap2_[i], len_ap2_[i]);
        bind(tank_dl2_[i], len_dl2_[i]);
    }
    reset();
}

inline void DattorroPlate::reset() {
    for (auto& row : mem_) for (auto& v : row) v = 0.0f;
    bw_lp_ = 0.0f;
    damp_lp_[0] = damp_lp_[1] = 0.0f;
    lfo_ph_ = 0.0f;
}

inline void DattorroPlate::process(float in, float decay, float damp,
                                   float& out_l, float& out_r) {
    // Input bandwidth (one-pole LP, bw = 0.9995 in the paper).
    bw_lp_ += 0.9995f * (in - bw_lp_);
    float x = bw_lp_;
    // Input diffusion.
    x = allpass(in_ap_[0], len_in_[0], 0.750f, x);
    x = allpass(in_ap_[1], len_in_[1], 0.750f, x);
    x = allpass(in_ap_[2], len_in_[2], 0.625f, x);
    x = allpass(in_ap_[3], len_in_[3], 0.625f, x);

    // SIZE → tank decay, fitted so RT60 sweeps evenly (exponentially) from
    // about 0.8 s to 5 s. A linear map spent the top third of the knob on
    // 10–60 s tails that never stop ringing.
    const float sz   = std::clamp(decay, 0.0f, 1.0f);
    const float dec  = ((-0.3993f * sz + 0.4896f) * sz + 0.4775f) * sz + 0.1874f;
    const float dmp  = 0.0005f + 0.7f * std::clamp(damp, 0.0f, 1.0f);
    const float dd2  = std::clamp(dec + 0.15f, 0.25f, 0.50f);

    lfo_ph_ += lfo_inc_;
    if (lfo_ph_ >= 1.0f) lfo_ph_ -= 1.0f;
    const float mod = std::sin(2.0f * kPi * lfo_ph_) * mod_depth_;

    // Figure-eight: each branch is fed by the other branch's tail.
    const float tail0 = tank_dl2_[1].read(len_dl2_[1]);
    const float tail1 = tank_dl2_[0].read(len_dl2_[0]);
    for (int b = 0; b < 2; ++b) {
        float v = x + dec * (b == 0 ? tail0 : tail1);
        // Modulated decay-diffusion allpass (g = -0.7 in the paper).
        {
            Line& l = tank_ap1_[b];
            const float d  = static_cast<float>(len_ap1_[b]) + (b == 0 ? mod : -mod);
            const float dl = l.read_frac(d);
            const float w  = v - 0.70f * dl;
            l.write(w);
            v = dl + 0.70f * w;
        }
        tank_dl1_[b].write(v);
        v = tank_dl1_[b].read(len_dl1_[b]);
        damp_lp_[b] += (1.0f - dmp) * (v - damp_lp_[b]);
        v = damp_lp_[b] * dec;
        v = allpass(tank_ap2_[b], len_ap2_[b], dd2, v);
        tank_dl2_[b].write(v);
    }

    // Output taps (Dattorro table 2), left from one side, right from the other.
    const float l =
          tank_dl1_[1].read(taps_[0]) + tank_dl1_[1].read(taps_[1])
        - tank_ap2_[1].read(taps_[2]) + tank_dl2_[1].read(taps_[3])
        - tank_dl1_[0].read(taps_[4]) - tank_ap2_[0].read(taps_[5])
        - tank_dl2_[0].read(taps_[6]);
    const float r =
          tank_dl1_[0].read(taps_[7]) + tank_dl1_[0].read(taps_[8])
        - tank_ap2_[0].read(taps_[9]) + tank_dl2_[0].read(taps_[10])
        - tank_dl1_[1].read(taps_[11]) - tank_ap2_[1].read(taps_[12])
        - tank_dl2_[1].read(taps_[13]);
    out_l = 0.6f * l;
    out_r = 0.6f * r;
}

// ── Tape delay ───────────────────────────────────────────────────────────────
// Stereo ping-pong with tape character in the feedback loop: ADAA tanh
// saturation, a band-pass (HP ~120 Hz, LP falling with feedback) and slow
// wow + faster flutter on the read head. Time is set in samples by the host.
class TapeDelay {
  public:
    static constexpr int kLen = 1 << 17;   // 131072 samples ≈ 2.7 s at 48 kHz

    void reset() {
        for (auto& v : l_) v = 0.0f;
        for (auto& v : r_) v = 0.0f;
        w_ = 0; time_sm_ = 0.0f; lp_[0] = lp_[1] = 0.0f; hp_[0] = hp_[1] = 0.0f;
        hpx_[0] = hpx_[1] = 0.0f; wow_ = 0.0f; flut_ = 0.0f;
        sat_[0].reset(); sat_[1].reset();
    }

    // time_tgt in samples. Input is mono (the bus), output is stereo wet.
    // The first repeat lands on the left, the next on the right, and so on.
    void process(float in, float time_tgt, float fb, float sr,
                 float& wet_l, float& wet_r) {
        time_sm_ += time_coef_ * (time_tgt - time_sm_);
        // Wow ~0.6 Hz, flutter ~7 Hz, about ±0.15% / ±0.03% of the time.
        wow_  += 0.6f / sr; if (wow_  >= 1.0f) wow_  -= 1.0f;
        flut_ += 7.1f / sr; if (flut_ >= 1.0f) flut_ -= 1.0f;
        const float mod = 1.0f + 0.0015f * std::sin(2.0f * kPi * wow_)
                               + 0.0003f * std::sin(2.0f * kPi * flut_);
        const float d = std::clamp(time_sm_ * mod, 4.0f, static_cast<float>(kLen - 8));

        const float tl = read(l_, d);
        const float tr = read(r_, d);

        // Feedback conditioning per channel.
        auto cond = [&](int c, float v) {
            // HP ~120 Hz keeps the repeats from piling up mud.
            const float hp = v - hpx_[c] + hp_a_ * hp_[c];
            hpx_[c] = v; hp_[c] = hp;
            // LP darkens each pass; darker as feedback rises.
            lp_[c] += lp_coef_ * (hp - lp_[c]);
            // Gentle tape saturation. Unity-ish below ~0.3, compresses above.
            return sat_[c].process(lp_[c] * 1.4f) * 0.714f;
        };
        const float fl = cond(0, tl);
        const float fr = cond(1, tr);
        // Ping-pong: input enters left; each channel feeds the other.
        write(l_, in + fb * fr);
        write(r_, fb * fl);
        if (++w_ >= kLen) w_ = 0;
        wet_l = tl;
        wet_r = tr;
    }

    void set_coefs(float sr, float fb) {
        time_coef_ = 1.0f - std::exp(-1.0f / (0.060f * sr));
        hp_a_      = std::exp(-2.0f * kPi * 120.0f / sr);
        const float lp_hz = 4200.0f - 1800.0f * std::clamp(fb, 0.0f, 1.0f);
        lp_coef_   = 1.0f - std::exp(-2.0f * kPi * lp_hz / sr);
    }

  private:
    float read(const float* b, float d) const {
        float rp = static_cast<float>(w_) - d;
        while (rp < 0.0f) rp += static_cast<float>(kLen);
        const int   i0 = static_cast<int>(rp);
        const float fr = rp - static_cast<float>(i0);
        // Cubic (Hermite) interpolation: smoother than linear under wow.
        const float xm1 = b[(i0 - 1) & (kLen - 1)];
        const float x0  = b[i0 & (kLen - 1)];
        const float x1  = b[(i0 + 1) & (kLen - 1)];
        const float x2  = b[(i0 + 2) & (kLen - 1)];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * fr + c2) * fr + c1) * fr + x0;
    }
    void write(float* b, float v) { b[w_] = v; }

    float    l_[kLen] = {};
    float    r_[kLen] = {};
    int      w_ = 0;
    float    time_sm_ = 0.0f, time_coef_ = 0.001f;
    float    lp_[2] = {}, lp_coef_ = 0.3f;
    float    hp_[2] = {}, hpx_[2] = {}, hp_a_ = 0.98f;
    float    wow_ = 0.0f, flut_ = 0.0f;
    TanhADAA sat_[2];
};

// Equal-power pan: p in [-1, 1].
inline void pan_gains(float p, float& gl, float& gr) {
    const float a = (std::clamp(p, -1.0f, 1.0f) + 1.0f) * 0.25f * kPi;
    gl = std::cos(a);
    gr = std::sin(a);
}

// ── Master limiter ────────────────────────────────────────────────────────────
// Stereo-linked look-ahead peak limiter. A 1.5 ms delay lets the gain come
// down before the peak arrives, so the ceiling holds without clipping; release
// is ~120 ms. Transparent below the ceiling.
class Limiter {
  public:
    static constexpr int kLook = 128;   // ≥ 1.5 ms at 48 kHz, power of two

    void set_sample_rate(float sr) {
        look_ = std::clamp(static_cast<int>(0.0015f * sr), 1, kLook - 1);
        rel_  = std::exp(-1.0f / (0.120f * sr));
        att_  = std::exp(-1.0f / (0.0005f * sr));
    }
    void reset() {
        for (auto& v : dl_) v = 0.0f;
        for (auto& v : dr_) v = 0.0f;
        w_ = 0; env_ = 0.0f; gain_ = 1.0f;
    }

    void process(float& l, float& r) {
        dl_[w_] = l; dr_[w_] = r;
        const int rd = (w_ - look_) & (kLook - 1);
        w_ = (w_ + 1) & (kLook - 1);
        // Peak hold over the look-ahead window via a fast-attack follower.
        const float pk = std::max(std::fabs(l), std::fabs(r));
        env_ = pk > env_ ? pk : env_ * rel_ + pk * (1.0f - rel_);
        const float tgt = env_ > kCeil ? kCeil / env_ : 1.0f;
        gain_ = tgt < gain_ ? tgt + att_ * (gain_ - tgt) : tgt + rel_ * (gain_ - tgt);
        l = soft(dl_[rd] * gain_);
        r = soft(dr_[rd] * gain_);
    }

    float gain() const { return gain_; }

  private:
    static constexpr float kCeil = 0.89f;   // −1 dBFS
    // Last-resort knee for anything that slips past: linear to 0.89, then
    // smoothly to 1.0.
    static float soft(float x) {
        const float a = std::fabs(x);
        if (a <= kCeil) return x;
        const float over = (a - kCeil) / (1.0f - kCeil);
        const float y = kCeil + (1.0f - kCeil) * std::tanh(over);
        return x < 0.0f ? -y : y;
    }
    float dl_[kLook] = {}, dr_[kLook] = {};
    int   w_ = 0, look_ = 64;
    float env_ = 0.0f, gain_ = 1.0f, rel_ = 0.9998f, att_ = 0.95f;
};

}  // namespace acid::dsp
