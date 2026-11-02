#pragma once
// Oscilloscope panel with phosphor persistence, drawn as a pixel image
// (maya's pixels(): two pixels per cell, upper-half block). The persistence
// buffer is owned by the app and advanced in update; the view only reads it.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <maya/maya.hpp>
#include <maya/element/pixels.hpp>

#include "../audio.hpp"
#include "theme.hpp"

namespace tb303 {

using namespace maya;

// Phosphor buffer: one brightness byte per pixel. Sized to the scope's slot
// at a fixed resolution and stretched by pixels() (nearest neighbour).
struct ScopeState {
    static constexpr int kW = 192;   // pixels across
    static constexpr int kH = 48;    // pixels tall (24 cells of half blocks)
    std::vector<uint8_t> phos = std::vector<uint8_t>(kW * kH, 0);
    float peak = 0.0f;

    // Pull the latest audio, trigger on a rising zero crossing, decay the old
    // trace and stamp the new one. Called once per UI tick.
    void advance() {
        for (auto& v : phos) v = v >= 40 ? static_cast<uint8_t>(v - 40) : 0;

        const int want = kW;
        float pulled[4096];
        const int pull_n = std::min(want * 3, 4096);
        const int got = acid_scope_tail(pulled, pull_n);
        // Most recent rising crossing that still has `want` samples after it.
        int trigger = -1;
        for (int i = got - want; i > 0; --i)
            if (pulled[i] >= 0.01f && pulled[i - 1] < -0.01f) { trigger = i; break; }
        const int start = trigger >= 0 ? trigger : std::max(0, got - want);

        auto row_of = [](float s) {
            const float t = 0.5f - 0.5f * std::clamp(s, -1.0f, 1.0f);
            return std::clamp(static_cast<int>(std::lround(t * (kH - 1))), 0, kH - 1);
        };
        int prev = row_of(start < got ? pulled[start] : 0.0f);
        for (int x = 0; x < want && start + x < got; ++x) {
            const int r = row_of(pulled[start + x]);
            // Fill between consecutive samples so steep edges stay solid.
            for (int y = std::min(prev, r); y <= std::max(prev, r); ++y)
                phos[static_cast<size_t>(y * kW + x)] = 255;
            prev = r;
        }
        peak = std::clamp(acid_output_peak(), 0.0f, 1.0f);
    }
};

[[nodiscard]] inline Element build_scope(const ScopeState& st) {
    // Paint the phosphor buffer into an image: hot amber for this frame's
    // trace, a dim afterglow for older ones, a faint zero line.
    auto img = std::make_shared<Image>(ScopeState::kW, ScopeState::kH);
    // A dark bed tinted toward the theme's grid colour.
    const Color gc = clr_grid();
    const Rgb   bg = gc.kind() == Color::Kind::Rgb
        ? Rgb{static_cast<uint8_t>(gc.raw_r() / 3), static_cast<uint8_t>(gc.raw_g() / 3),
              static_cast<uint8_t>(gc.raw_b() / 3)}
        : Rgb{12, 12, 16};
    const Rgb zero{static_cast<uint8_t>(bg.r + 30), static_cast<uint8_t>(bg.g + 26),
                   static_cast<uint8_t>(bg.b + 24)};
    for (int y = 0; y < ScopeState::kH; ++y) {
        const float ct = 1.0f - std::fabs((y + 0.5f) / ScopeState::kH * 2.0f - 1.0f);
        for (int x = 0; x < ScopeState::kW; ++x) {
            const uint8_t v = st.phos[static_cast<size_t>(y * ScopeState::kW + x)];
            Rgb px = bg;
            if (y == ScopeState::kH / 2 && (x & 3) == 0) px = zero;
            if (v > 0) {
                // 255 = live: bright amber, hotter in the middle rows. Below
                // that the trail fades toward the background.
                const float a = v / 255.0f;
                const float r = (190 + 65 * ct) * a, g = (70 + 80 * ct) * a * a, b = 18 * a;
                px = Rgb{static_cast<uint8_t>(std::max<float>(bg.r, r)) , static_cast<uint8_t>(std::max<float>(bg.g, g)),
                         static_cast<uint8_t>(std::max<float>(bg.b, b))};
                px.r &= 0xF8; px.g &= 0xF8; px.b &= 0xF8;   // keep the style cache small
            }
            (*img)(x, y) = px;
        }
    }

    // Peak meter: one cell row of blocks under the trace.
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
                   ? clr_hot() : Color::hsl(35.0f - ct * 20.0f, 0.9f, 0.45f + ct * 0.10f);
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
            pixels(std::shared_ptr<const Image>(std::move(img))) | dsl::grow(1),
            std::move(meter));
}

} // namespace tb303
