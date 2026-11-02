#pragma once
// Signal analysis helpers for the DSP tests.

#include <cmath>
#include <complex>
#include <vector>

namespace sig {

inline double rms(const float* x, size_t n) {
    double s = 0;
    for (size_t i = 0; i < n; ++i) s += double(x[i]) * x[i];
    return n ? std::sqrt(s / double(n)) : 0.0;
}
inline double db(double v) { return 20.0 * std::log10(v + 1e-30); }

// Magnitude spectrum of x (Hann-windowed, length rounded down to a power of 2).
// `bh` = 4-term Blackman-Harris instead: ~-92 dB sidelobes, for measuring
// aliasing far below what a Hann window's leakage lets you see.
inline std::vector<double> spectrum(const std::vector<float>& x, bool bh = false) {
    size_t n = 1;
    while (n * 2 <= x.size()) n *= 2;
    std::vector<std::complex<double>> a(n);
    for (size_t i = 0; i < n; ++i) {
        const double t = 2 * M_PI * double(i) / double(n);
        const double w = bh ? 0.35875 - 0.48829 * std::cos(t) + 0.14128 * std::cos(2 * t) - 0.01168 * std::cos(3 * t)
                            : 0.5 - 0.5 * std::cos(t);
        a[i] = x[i] * w;
    }
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const auto w = std::polar(1.0, -2 * M_PI / double(len));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> wn = 1;
            for (size_t k = 0; k < len / 2; ++k, wn *= w) {
                auto u = a[i + k], v = a[i + k + len / 2] * wn;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
            }
        }
    }
    std::vector<double> m(n / 2);
    for (size_t i = 0; i < n / 2; ++i) m[i] = std::abs(a[i]);
    return m;
}

// Frequency of the strongest bin, with parabolic interpolation.
inline double peak_hz(const std::vector<double>& m, double sr, double lo_hz = 20) {
    const double bin = sr / (2.0 * double(m.size()));
    size_t best = size_t(lo_hz / bin) + 1;
    for (size_t i = best; i + 1 < m.size(); ++i) if (m[i] > m[best]) best = i;
    const double a = m[best - 1], b = m[best], c = m[best + 1];
    const double p = 0.5 * (a - c) / (a - 2 * b + c);
    return (double(best) + p) * bin;
}

// Fraction of energy (in dB) that is NOT near harmonics of f0. With a sine
// through a nonlinearity this measures aliasing.
inline double inharmonic_db(const std::vector<double>& m, double sr, double f0) {
    const double bin = sr / (2.0 * double(m.size()));
    double tot = 0, inh = 0;
    for (size_t i = 1; i < m.size(); ++i) {
        const double p = m[i] * m[i];
        tot += p;
        const double f = double(i) * bin;
        const double h = std::round(f / f0);
        if (h < 1 || std::fabs(f - h * f0) > 4 * bin) inh += p;
    }
    return 10 * std::log10(inh / tot + 1e-30);
}

}  // namespace sig
