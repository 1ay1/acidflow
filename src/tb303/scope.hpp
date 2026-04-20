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

        // ── Phosphor persistence buffer ─────────────────────────────────────
        // One byte per Braille dot (chart_w*2 columns × chart_h*4 rows). Each
        // frame we decay every dot by a fixed step and stamp the current
        // trace at 255. When rendering, a cell's glyph ORs together every dot
        // whose brightness is non-zero, so old traces persist as "ghosts"
        // behind the live scan — the TUI equivalent of an analog CRT's
        // phosphor afterglow. State-change on resize is handled by comparing
        // the cached dimensions and re-allocating when they differ.
        static std::vector<uint8_t> persist;
        static int persist_w = 0, persist_rows = 0;
        int dot_cols = chart_w * 2;
        int dot_rows = chart_h * 4;
        if (persist_w != dot_cols || persist_rows != dot_rows) {
            persist.assign(static_cast<size_t>(dot_cols * dot_rows), 0);
            persist_w    = dot_cols;
            persist_rows = dot_rows;
        }
        // Decay: linear subtract so the tail has a crisp edge (~5 frames to
        // vanish). Exponential decay would mush endlessly at low values.
        for (auto& v : persist) v = (v >= 56) ? static_cast<uint8_t>(v - 56) : 0;

        // Pull 2*chart_w samples (Braille doubles horizontal resolution).
        int want = chart_w * 2;

        // ── Zero-crossing trigger ───────────────────────────────────────────
        // A free-running trailing-window scope "swims" because each frame
        // picks up a different phase of the waveform. To lock the display we
        // grab a larger buffer and hunt for the most-recent rising zero
        // crossing that still has `want` samples after it — that's the left
        // edge of our displayed window. This is how real oscilloscopes stay
        // stable: trigger on edge, display N samples starting from it.
        //
        // We search backwards (newest-first) so the display shows the freshest
        // locked frame rather than an older one. If no trigger is found
        // (silence, or a waveform that stays on one side of zero), we fall
        // back to the trailing window — at which point the scope will simply
        // show whatever's there with no alignment, which is fine.
        int pull_n = std::min(want * 3, 4096);
        std::vector<float> pulled(static_cast<size_t>(pull_n), 0.0f);
        int got = acid_scope_tail(pulled.data(), pull_n);

        int trigger = -1;
        const float hysteresis = 0.01f;   // ignore sub-noise-floor crossings
        int search_hi = got - want;       // need `want` samples after trigger
        for (int i = search_hi; i > 0; --i) {
            if (pulled[static_cast<size_t>(i)] >= hysteresis &&
                pulled[static_cast<size_t>(i - 1)] < -hysteresis) {
                trigger = i;
                break;
            }
        }

        std::vector<float> samples(static_cast<size_t>(want), 0.0f);
        if (trigger >= 0) {
            for (int i = 0; i < want; ++i)
                samples[static_cast<size_t>(i)] =
                    pulled[static_cast<size_t>(trigger + i)];
        } else {
            // Fallback: trailing window, right-aligned so partial rings still
            // read naturally (scope "fills in from the right" during startup).
            int take = std::min(want, got);
            int src_off = got - take;
            int dst_off = want - take;
            for (int i = 0; i < take; ++i)
                samples[static_cast<size_t>(dst_off + i)] =
                    pulled[static_cast<size_t>(src_off + i)];
        }

        // Map each sample to a dot row in the Braille grid. Each terminal row
        // is 4 dot-rows tall, so total dot-rows = chart_h * 4.
        auto sample_to_row = [&](float s) -> int {
            float norm = std::clamp(s, -1.0f, 1.0f);    // -1..1
            float t    = 0.5f - 0.5f * norm;            //  1..0 (top is small)
            int   r    = static_cast<int>(std::round(t * (dot_rows - 1)));
            return std::clamp(r, 0, dot_rows - 1);
        };
        // Dot row of the zero line (centre). A dim dotted line at this row
        // draws the 0-V reference.
        const int zero_dot = dot_rows / 2;

        // Stamp the current trace into the phosphor buffer at full brightness.
        // Fill every dot between consecutive samples (stem fill) so a fast
        // waveform still reads as a solid line rather than a dashed one.
        auto stamp_range = [&](int col, int r_a, int r_b) {
            int lo = std::min(r_a, r_b);
            int hi = std::max(r_a, r_b);
            for (int r = lo; r <= hi; ++r) {
                persist[static_cast<size_t>(r * dot_cols + col)] = 255;
            }
        };
        for (int col = 0; col < chart_w; ++col) {
            int s_a = sample_to_row(samples[static_cast<size_t>(col * 2)]);
            int s_b = sample_to_row(samples[static_cast<size_t>(col * 2 + 1)]);
            int prev_b = (col == 0)
                ? s_a
                : sample_to_row(samples[static_cast<size_t>(col * 2 - 1)]);
            stamp_range(col * 2,     prev_b, s_a);
            stamp_range(col * 2 + 1, s_a,    s_b);
        }

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
            // Ghost colour: same hue as base but much darker — the faded
            // afterglow of past frames. Stands out from `clr_grid()` so the
            // eye reads the trail as "part of the scope" rather than grid.
            Color ghost = Color::hsl(20.0f + ct * 18.0f,
                                     0.60f,
                                     0.16f + ct * 0.08f);

            for (int col = 0; col < chart_w; ++col) {
                uint32_t cp       = 0x2800;
                bool     has_live = false;   // any dot lit at full brightness
                bool     has_any  = false;   // any dot lit at all
                // Walk the 4×2 dot window for this cell. A dot counts if its
                // phosphor value is above zero; ≥250 flags it as "live" (this
                // frame) so we can colour it hot, otherwise it's ghost.
                for (int ci = 0; ci < 2; ++ci) {
                    int dc = col * 2 + ci;
                    for (int dri = 0; dri < 4; ++dri) {
                        int dr = row_dot_top + dri;
                        uint8_t v = persist[static_cast<size_t>(dr * dot_cols + dc)];
                        if (v == 0) continue;
                        cp |= kBrailleColBits[ci][dri];
                        has_any = true;
                        if (v >= 250) has_live = true;
                    }
                }

                // If still empty AND the zero line passes through this cell's
                // dot-row window, sprinkle a dim zero-reference mark — dot at
                // col 0 on even cells, col 1 on odd cells. Reads as a dashed
                // horizontal without adding any new colour.
                bool was_empty = (cp == 0x2800);
                int  zero_local = zero_dot - row_dot_top;
                if (was_empty && zero_local >= 0 && zero_local < 4) {
                    int zero_col = (col % 2);
                    cp |= kBrailleColBits[zero_col][zero_local];
                }

                // encode cp (U+2800..U+28FF) as UTF-8 (3 bytes)
                char buf[4] = {
                    static_cast<char>(0xE0 | (cp >> 12)),
                    static_cast<char>(0x80 | ((cp >> 6) & 0x3F)),
                    static_cast<char>(0x80 | (cp & 0x3F)),
                    0,
                };
                size_t off = txt.size();
                txt += buf;
                // Hot colour on live dots; faded hue on ghost-only dots; the
                // grid dim for zero-line-only cells with no scope signal.
                Color fg = has_live ? base
                         : has_any  ? ghost
                                    : clr_grid();
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
