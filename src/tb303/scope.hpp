#pragma once
// Live oscilloscope + peak meter. A ComponentElement that polls the audio
// engine's scope ring buffer every frame (30 Hz) and draws a centred
// waveform using 2-row Braille cells, so each terminal row resolves two
// sample "dots" of vertical detail.
//
//   ┌────────────────────────────────┐
//   │ ⠠⢀⡠⠔⠒⠉⠒⠢⣀⣠⠔⠊⠉⠒⠢⡀⠤⠒⠉⠑⠤⣀⡠⠒⠉⠒⠢⡀⢠⠒⠊ │   ← waveform
//   │ ▄▄▄▁▁▁▁▁                         │   ← peak meter (left-anchored)
//   └────────────────────────────────┘
//
// The scope does NOT change the rest of the layout — it's self-contained and
// can be dropped anywhere a rectangular region fits.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include <maya/maya.hpp>

#include "../audio.hpp"
#include "theme.hpp"

namespace tb303 {

using namespace maya;

// Braille cell bit layout (Unicode U+2800..U+28FF). Each Braille cell encodes
// a 2x4 dot matrix; we only use the 2-column, 4-row form, so the per-column
// bits we care about are:
//   col 0:  bit 0 (top) · bit 1 · bit 2 · bit 6 (bottom)
//   col 1:  bit 3 (top) · bit 4 · bit 5 · bit 7 (bottom)
inline constexpr uint8_t kBrailleColBits[2][4] = {
    {0x01, 0x02, 0x04, 0x40},
    {0x08, 0x10, 0x20, 0x80},
};

[[nodiscard]] inline Element build_scope() {
    auto scope = dsl::component([](int w, int h) -> Element {
        int chart_h = std::max(2, h - 1);          // reserve 1 row for meter
        int chart_w = std::max(8, w);

        // Pull 2*chart_w samples (Braille doubles horizontal resolution).
        int want = chart_w * 2;
        std::vector<float> samples(static_cast<size_t>(want), 0.0f);
        int got = acid_scope_tail(samples.data(), want);
        if (got < want) {
            // Left-pad with silence so the waveform hugs the right side when
            // the ring hasn't filled yet.
            std::vector<float> tmp(static_cast<size_t>(want), 0.0f);
            for (int i = 0; i < got; ++i)
                tmp[static_cast<size_t>(want - got + i)] = samples[static_cast<size_t>(i)];
            samples = std::move(tmp);
        }

        // Map each sample to a dot row in the Braille grid. Each terminal row
        // is 4 dot-rows tall, so total dot-rows = chart_h * 4.
        int dot_rows = chart_h * 4;
        auto sample_to_row = [&](float s) -> int {
            float norm = std::clamp(s, -1.0f, 1.0f);    // -1..1
            float t    = 0.5f - 0.5f * norm;            //  1..0 (top is small)
            int   r    = static_cast<int>(std::round(t * (dot_rows - 1)));
            return std::clamp(r, 0, dot_rows - 1);
        };

        // Build chart_h rows of Braille text with colour gradient by row.
        // Grid colour ramps red/orange for rows closer to peak amplitude.
        std::vector<Element> rows;
        rows.reserve(static_cast<size_t>(chart_h + 1));

        for (int row = 0; row < chart_h; ++row) {
            std::string txt;
            std::vector<StyledRun> runs;
            int row_dot_top = row * 4;               // inclusive
            // Per-cell colour: hotter in the middle rows (where loud signals
            // live), dimmer near the edges. Looks a little like an analog CRT
            // with phosphor persistence.
            float ct = 1.0f - std::fabs((row + 0.5f) / chart_h * 2.0f - 1.0f);
            Color base = Color::hsl(20.0f + ct * 18.0f,    // red → amber
                                    0.85f,
                                    0.35f + ct * 0.25f);

            for (int col = 0; col < chart_w; ++col) {
                // two samples per cell: col*2 and col*2+1
                int s_a = sample_to_row(samples[static_cast<size_t>(col * 2)]);
                int s_b = sample_to_row(samples[static_cast<size_t>(col * 2 + 1)]);

                uint32_t cp = 0x2800;
                // For each of the two columns, if the dot row falls within
                // this terminal row's 4-dot-row window, light the dot. We
                // also light every dot between the two consecutive samples
                // in a column ("stem" fill) so a fast-moving waveform doesn't
                // look like a dashed line.
                auto light = [&](int col_idx, int s_prev, int s_cur) {
                    int dmin = std::min(s_prev, s_cur);
                    int dmax = std::max(s_prev, s_cur);
                    for (int d = dmin; d <= dmax; ++d) {
                        int local = d - row_dot_top;
                        if (local >= 0 && local < 4) {
                            cp |= kBrailleColBits[col_idx][local];
                        }
                    }
                };
                // stem for col 0 uses previous cell's last sample if possible
                int prev_b = (col == 0)
                           ? s_a
                           : sample_to_row(samples[static_cast<size_t>(col * 2 - 1)]);
                light(0, prev_b, s_a);
                light(1, s_a,    s_b);

                // encode cp (U+2800..U+28FF) as UTF-8 (3 bytes)
                char buf[4] = {
                    static_cast<char>(0xE0 | (cp >> 12)),
                    static_cast<char>(0x80 | ((cp >> 6) & 0x3F)),
                    static_cast<char>(0x80 | (cp & 0x3F)),
                    0,
                };
                size_t off = txt.size();
                txt += buf;
                // Dim the cell when its Braille code is empty (0x2800) so the
                // background doesn't look speckled.
                bool empty = (cp == 0x2800);
                Color fg = empty ? clr_grid() : base;
                runs.push_back(StyledRun{off, 3,
                    Style{}.with_fg(fg).with_bold()});
            }
            rows.push_back(Element{TextElement{
                .content = std::move(txt),
                .style   = {},
                .wrap    = TextWrap::NoWrap,
                .runs    = std::move(runs),
            }});
        }

        // ── Peak meter row (bottom) ────────────────────────────────────────
        // Left-anchored bar with 8-level blocks (▁..█) showing the decaying
        // peak envelope. Turns red once past 0.9 so the user knows when the
        // tanh saturator is biting.
        float peak = std::clamp(acid_output_peak(), 0.0f, 1.0f);
        int   lit  = static_cast<int>(std::round(peak * chart_w));
        lit = std::clamp(lit, 0, chart_w);

        std::string mtxt;
        std::vector<StyledRun> mruns;
        for (int c = 0; c < chart_w; ++c) {
            const char* glyph;
            Color fg;
            if (c < lit) {
                glyph = "\xe2\x96\x88";                        // █
                float ct = static_cast<float>(c) / std::max(1, chart_w - 1);
                if (peak > 0.9f && c > static_cast<int>(chart_w * 0.8f))
                    fg = clr_hot();
                else
                    fg = Color::hsl(35.0f - ct * 20.0f, 0.9f, 0.45f + ct * 0.10f);
            } else {
                glyph = "\xe2\x96\x81";                        // ▁
                fg = clr_grid();
            }
            size_t off = mtxt.size();
            mtxt += glyph;
            mruns.push_back(StyledRun{off, 3,
                Style{}.with_fg(fg).with_bold()});
        }
        rows.push_back(Element{TextElement{
            .content = std::move(mtxt),
            .style   = {},
            .wrap    = TextWrap::NoWrap,
            .runs    = std::move(mruns),
        }});

        return (dsl::v(std::move(rows)) | dsl::gap(0)).build();
    }).grow(1).min_height(Dimension::fixed(4));

    return dsl::vstack()
        .border(BorderStyle::Round)
        .border_color(clr_panel_hi())
        .border_text(" SCOPE ", BorderTextPos::Top)
        .align_self(Align::Stretch)
        .padding(0, 1, 0, 1)(Element{std::move(scope)});
}

} // namespace tb303
