#pragma once
// Oscilloscope panel with phosphor persistence, drawn with pixels (two per
// cell, upper-half block). The app keeps the last few triggered traces and
// advances them in update; the view rasterises them at whatever size the
// panel has, so the scope fills its slot at any terminal height.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include <maya/maya.hpp>
#include <maya/element/pixels.hpp>

#include "../audio.hpp"
#include "theme.hpp"

namespace tb303 {

using namespace maya;

struct ScopeState {
    static constexpr int kSamples = 512;   // samples per trace (time axis)
    static constexpr int kHistory = 6;     // traces kept for the afterglow
    std::array<std::vector<float>, kHistory> traces{};
    int   newest = -1;                     // index of the latest trace, -1 = none
    float peak   = 0.0f;
    float gain   = 1.0f;                   // display gain, follows the signal level

    // Grab the latest audio, trigger on a rising zero crossing, push it as
    // the newest trace. Called once per UI tick.
    void advance() {
        float pulled[4096];
        const int got = acid_scope_tail(pulled, std::min(kSamples * 3, 4096));
        peak = std::clamp(acid_output_peak(), 0.0f, 1.0f);
        if (got < kSamples) return;
        int start = got - kSamples;
        for (int i = got - kSamples; i > 0; --i)
            if (pulled[i] >= 0.01f && pulled[i - 1] < -0.01f) { start = i; break; }
        newest = (newest + 1) % kHistory;
        traces[static_cast<size_t>(newest)].assign(pulled + start, pulled + start + kSamples);
        // Auto-gain: fit the trace to ~85% of the height. Fast to shrink,
        // slow to grow, capped so silence doesn't blow noise up.
        float mx = 0.0f;
        for (int i = 0; i < kSamples; ++i) mx = std::max(mx, std::fabs(pulled[start + i]));
        const float want = std::clamp(0.85f / std::max(mx, 1e-3f), 1.0f, 8.0f);
        gain += (want < gain ? 0.5f : 0.05f) * (want - gain);
    }
};

namespace scope_detail {

// The scope's bed: the theme's surface, darkened so the trace stands out.
inline Rgb dark_bed() {
    const Rgb s = rgb_of(clr_grid(), Rgb{36, 36, 48});
    return Rgb{static_cast<uint8_t>(s.r / 3), static_cast<uint8_t>(s.g / 3), static_cast<uint8_t>(s.b / 3)};
}

// Draw `img` (w × ph pixels): background, zero line, then traces oldest to
// newest so the live one is on top.
inline void rasterise(Image& img, const std::vector<std::vector<float>>& traces, float gain) {
    const int w = img.width(), ph = img.height();
    const Rgb bg = dark_bed();
    const Rgb zero{static_cast<uint8_t>(bg.r + 30), static_cast<uint8_t>(bg.g + 26),
                   static_cast<uint8_t>(bg.b + 24)};
    for (int y = 0; y < ph; ++y)
        for (int x = 0; x < w; ++x) img(x, y) = bg;
    const int mid = ph / 2;
    for (int x = 0; x < w; x += 4) img(x, mid) = zero;

    auto row_of = [&](float s) {
        const float t = 0.5f - 0.5f * std::clamp(s * gain, -1.0f, 1.0f);
        return std::clamp(static_cast<int>(std::lround(t * (ph - 1))), 0, ph - 1);
    };
    // Trace colours from the theme: primary at the edges, warning (hotter)
    // through the middle, so it glows like a phosphor.
    const Rgb edge = rgb_of(clr_accent(), Rgb{255, 138, 40});
    const Rgb core = rgb_of(Color::slot(ThemeSlot::Warning), Rgb{255, 200, 80});
    const int n = static_cast<int>(traces.size());
    for (int age = n - 1; age >= 0; --age) {
        const auto& tr = traces[static_cast<size_t>(n - 1 - age)];
        if (tr.empty()) continue;
        // Live trace is bright amber; older ones fade toward the bed.
        const float a = age == 0 ? 1.0f : 0.45f * std::pow(0.6f, static_cast<float>(age - 1));
        const int ns = static_cast<int>(tr.size());
        int prev = row_of(tr[0]);
        for (int x = 0; x < w; ++x) {
            const int i = std::min(ns - 1, x * ns / std::max(1, w));
            const int r = row_of(tr[static_cast<size_t>(i)]);
            // Fill between neighbouring columns so steep edges stay solid.
            for (int y = std::min(prev, r); y <= std::max(prev, r); ++y) {
                const float ct = 1.0f - std::fabs((y + 0.5f) / ph * 2.0f - 1.0f);
                const Rgb old = img(x, y);
                auto lerp = [ct](uint8_t e, uint8_t c) { return e + (static_cast<float>(c) - e) * ct * 0.5f; };
                const float rr = lerp(edge.r, core.r) * a, gg = lerp(edge.g, core.g) * a * a,
                            bb = lerp(edge.b, core.b) * a * a;
                auto ch = [](uint8_t o, float v) {
                    return static_cast<uint8_t>(static_cast<int>(std::max<float>(o, v)) & 0xF8);
                };
                img(x, y) = Rgb{ch(old.r, rr), ch(old.g, gg), ch(old.b, bb)};
            }
            prev = r;
        }
    }
}

}  // namespace scope_detail

[[nodiscard]] inline Element build_scope(const ScopeState& st) {
    // Traces oldest → newest, copied so the element outlives this model.
    std::vector<std::vector<float>> traces;
    if (st.newest >= 0)
        for (int k = 1; k <= ScopeState::kHistory; ++k)
            traces.push_back(st.traces[static_cast<size_t>((st.newest + k) % ScopeState::kHistory)]);

    // Rasterised at paint time at the panel's real size (w × 2h pixels), so
    // the trace stays one pixel thick and fills the panel at any height.
    // With no signal it says how to get one, instead of showing a flat line.
    const bool silent = st.peak < 1e-3f;
    auto trace = detail::paint([traces = std::move(traces), gain = st.gain, silent](Canvas& c, int x, int y, int w, int h) {
        if (w <= 0 || h <= 0) return;
        Image img(w, h * 2);
        scope_detail::rasterise(img, traces, silent ? 0.0f : gain);
        detail::paint_pixels(c, img, x, y, w, h);
        if (silent && h >= 3) {
            const std::u32string msg = U"silent \u00b7 press Space to play";
            const int mx = x + std::max(0, (w - static_cast<int>(msg.size())) / 2);
            const int my = y + h / 2 - 1;
            auto& pool = *c.style_pool();
            const auto bed = scope_detail::dark_bed();
            const auto sid = pool.intern(Style{}.with_fg(theme::live().resolve(clr_muted()))
                                             .with_bg(Color::rgb(bed.r, bed.g, bed.b)));
            for (int i = 0; i < static_cast<int>(msg.size()) && mx + i < x + w; ++i)
                c.set(mx + i, my, msg[static_cast<size_t>(i)], sid);
        }
    }, /*min_w=*/0, /*min_h=*/2);

    // Peak meter: exactly one cell row under the trace.
    auto meter = dsl::fill([peak = st.peak](int w, int) -> Element {
        const int lit = std::clamp(static_cast<int>(std::lround(peak * w)), 0, w);
        std::string txt;
        std::vector<StyledRun> runs;
        for (int c = 0; c < w; ++c) {
            const size_t off = txt.size();
            Color fg;
            if (c < lit) {
                txt += "\xe2\x96\x88";   // █
                const float ct = static_cast<float>(c) / std::max(1, w - 1);
                fg = (peak > 0.9f && c > static_cast<int>(w * 0.8f))
                   ? clr_hot() : mix(clr_accent(), clr_hot(), ct * 0.6f);
            } else {
                txt += "\xe2\x96\x81";   // ▁
                fg = clr_grid();
            }
            runs.push_back(StyledRun{off, 3, Style{}.with_fg(fg).with_bold()});
        }
        return Element{TextElement{.content = std::move(txt), .style = {},
                                   .wrap = TextWrap::NoWrap, .runs = std::move(runs)}};
    }, /*min_w=*/0, /*min_h=*/1);

    return dsl::vstack()
        .border(BorderStyle::Round)
        .border_color(clr_panel_hi())
        .border_text(" SCOPE ", BorderTextPos::Top)
        .align_self(Align::Stretch)
        .padding(0, 1, 0, 1)(
            std::move(trace) | dsl::grow(1),
            std::move(meter) | dsl::height(1) | dsl::grow(0));
}

} // namespace tb303
