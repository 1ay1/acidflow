#pragma once
// Multi-row magnitude response chart. Each column is a vertical bar made of
// partial block chars (▁..█) whose height encodes |H(f)| in dB. The cutoff
// column is drawn in accent red so you can see it "walk" as CUTOFF is tweaked.
// A log-frequency axis is drawn in the last row with 100 / 1k / 10k labels.
//
// The chart is a ComponentElement — it receives its allocated (w, h) after
// layout, so it fills whatever vertical space the row gives it.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include <maya/maya.hpp>

#include "../audio.hpp"
#include "theme.hpp"

namespace tb303 {

using namespace maya;

[[nodiscard]] inline Element build_filter_response(float cutoff01, float res01) {
    float fc       = 80.0f + cutoff01 * 6000.0f;
    float live_fc  = acid_live_fc();         // post-envelope / post-accent

    char fc_buf[16];
    if (fc >= 1000.0f) std::snprintf(fc_buf, sizeof(fc_buf), "%.2f kHz", fc / 1000.0f);
    else               std::snprintf(fc_buf, sizeof(fc_buf), "%.0f Hz", fc);
    char live_buf[16];
    if (live_fc >= 1000.0f) std::snprintf(live_buf, sizeof(live_buf), "%.2f kHz", live_fc / 1000.0f);
    else                    std::snprintf(live_buf, sizeof(live_buf), "%.0f Hz", live_fc);

    // Caption shows both the static CUTOFF knob position (fc) and the
    // instantaneous modulated frequency (live). When idle they coincide;
    // during a note the live value sweeps up and the gap visualises envmod.
    auto caption = Element{TextElement{
        .content = "fc " + std::string(fc_buf) +
                   "  \xe2\x86\x92 " + std::string(live_buf) +     // "→"
                   "   Q " +
                   std::to_string(static_cast<int>(std::round(res01 * 100))) + "%",
        .style   = Style{}.with_fg(clr_muted()),
        .wrap    = TextWrap::NoWrap,
    }};

    auto chart = dsl::component([cutoff01, res01, live_fc](int w, int h) -> Element {
        // 8-level vertical block chars (empty..full). blocks[0] is a space so
        // the background in empty cells renders as pure background colour.
        static constexpr const char* blocks[] = {
            " ",
            "\xe2\x96\x81", "\xe2\x96\x82", "\xe2\x96\x83", "\xe2\x96\x84",
            "\xe2\x96\x85", "\xe2\x96\x86", "\xe2\x96\x87", "\xe2\x96\x88",
        };

        int axis_rows = 1;                          // bottom row = axis labels
        int chart_h   = std::max(1, h - axis_rows);
        int chart_w   = std::max(4, w);

        float f_lo = 40.0f, f_hi = 12000.0f;
        float fc = 80.0f + cutoff01 * 6000.0f;
        float peak_gain = 1.0f + res01 * 3.5f;
        float damp      = 0.9f - res01 * 0.7f;

        // ── Live FFT overlay ────────────────────────────────────────────────
        // Pull a magnitude spectrum from the engine and project each bin onto
        // the chart's log-frequency axis. We store the dB-normalised peak per
        // column so the overlay reads as "the sound the filter is making right
        // now" sitting on top of "the shape the filter allows". When playing,
        // they should roughly coincide under the cutoff and duck above it —
        // which is exactly the resonance bump made visible.
        constexpr int kFftBins = 1024;
        static std::vector<float> fft_mags(kFftBins, 0.0f);
        int got_bins = acid_fft_bins(fft_mags.data(), kFftBins);
        int sr       = acid_sample_rate();
        std::vector<float> fft_col(static_cast<size_t>(chart_w), 0.0f);
        if (got_bins > 0 && sr > 0) {
            const float bin_hz = static_cast<float>(sr) / 2048.0f;
            const float log_hi_lo = std::log(f_hi / f_lo);
            for (int i = 1; i < got_bins; ++i) {          // skip DC
                float fbin = static_cast<float>(i) * bin_hz;
                if (fbin < f_lo || fbin > f_hi) continue;
                float t = std::log(fbin / f_lo) / log_hi_lo;
                int   c = static_cast<int>(std::round(t * (chart_w - 1)));
                if (c < 0 || c >= chart_w) continue;
                float m = fft_mags[static_cast<size_t>(i)];
                if (m > fft_col[static_cast<size_t>(c)])
                    fft_col[static_cast<size_t>(c)] = m;
            }
        }
        // Normalise to dB on the same −40..+3 scale the filter curve uses,
        // so the FFT line lives in the same vertical space as the fill and
        // can't float off the chart.
        std::vector<float> fft_norm(static_cast<size_t>(chart_w), 0.0f);
        for (int c = 0; c < chart_w; ++c) {
            float m = fft_col[static_cast<size_t>(c)];
            if (m < 1e-5f) { fft_norm[static_cast<size_t>(c)] = 0.0f; continue; }
            float db = 20.0f * std::log10(m);
            fft_norm[static_cast<size_t>(c)] = std::clamp((db + 40.0f) / 43.0f, 0.0f, 1.0f);
        }
        // Map each column to the row its FFT peak sits in. −1 = below chart.
        std::vector<int> fft_row(static_cast<size_t>(chart_w), -1);
        for (int c = 0; c < chart_w; ++c) {
            float v = fft_norm[static_cast<size_t>(c)];
            if (v < 0.02f) continue;                      // below noise floor
            int row = static_cast<int>(std::round(v * (chart_h - 1)));
            fft_row[static_cast<size_t>(c)] = std::clamp(row, 0, chart_h - 1);
        }

        // magnitude per column on a log frequency sweep
        std::vector<float> raw(static_cast<size_t>(chart_w));
        float mx = 0.0f;
        for (int i = 0; i < chart_w; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(chart_w - 1);
            float f = f_lo * std::pow(f_hi / f_lo, t);
            float r = f / fc;
            float denom = std::sqrt((1.0f - r * r) * (1.0f - r * r)
                                  + damp * damp * r * r);
            float m = 1.0f / denom;
            float peak = peak_gain * std::exp(
                -std::pow((std::log(f) - std::log(fc)) / 0.25f, 2.0f));
            m += peak * 0.25f;
            raw[static_cast<size_t>(i)] = m;
            mx = std::max(mx, m);
        }
        if (mx < 1e-3f) mx = 1.0f;

        // convert to normalised 0..1 height on a -40..+3 dB axis
        std::vector<float> norm(static_cast<size_t>(chart_w));
        for (int i = 0; i < chart_w; ++i) {
            float db = 20.0f * std::log10(std::max(raw[static_cast<size_t>(i)] / mx, 1e-3f));
            norm[static_cast<size_t>(i)] = std::clamp((db + 40.0f) / 43.0f, 0.0f, 1.0f);
        }

        // column index of the cutoff frequency (for highlighted bar)
        float fc_t  = std::log(fc / f_lo) / std::log(f_hi / f_lo);
        int   fc_col = std::clamp(
            static_cast<int>(std::round(fc_t * (chart_w - 1))), 0, chart_w - 1);

        // Live effective cutoff — where the filter is RIGHT NOW after env and
        // accent modulation. Usually above fc during a note (envelope opens
        // it up), equal to fc at rest. We clamp the engine's reported value
        // into the display range so off-scale transients still pin to the
        // edge instead of vanishing.
        float live_clamped = std::clamp(live_fc, f_lo, f_hi);
        float live_t       = std::log(live_clamped / f_lo) / std::log(f_hi / f_lo);
        int   live_col     = std::clamp(
            static_cast<int>(std::round(live_t * (chart_w - 1))), 0, chart_w - 1);
        bool  show_live    = std::fabs(live_col - fc_col) > 0;

        // Horizontal dB grid lines. The Y axis maps -40..+3 dB onto norm
        // 0..1, so -24 dB is norm=(−24+40)/43 ≈ 0.372 and -12 dB is ≈ 0.651.
        // We draw a dim dashed horizontal through empty cells at those rows.
        const int grid_row_24 = static_cast<int>(std::round(0.372f * (chart_h - 1)));
        const int grid_row_12 = static_cast<int>(std::round(0.651f * (chart_h - 1)));

        // Pre-compute a heat-gradient palette: deep red-orange at the base
        // through orange, amber, to bright yellow at the top. Each bar is
        // coloured by the row it's drawn in, not by the column — so the whole
        // graph glows like embers: hotter as the magnitude climbs.
        std::vector<Color> row_col(static_cast<size_t>(chart_h));
        for (int row = 0; row < chart_h; ++row) {
            // t = 0 at bottom row, 1 at top row
            float t = chart_h <= 1 ? 1.0f
                    : static_cast<float>(row) / static_cast<float>(chart_h - 1);
            float hue = 12.0f + t * 38.0f;                 // 12° red → 50° yellow
            float sat = 0.95f - t * 0.10f;                 // slight desat at top
            float lgt = 0.38f + t * 0.28f;                 // brighter near top
            row_col[static_cast<size_t>(row)] = Color::hsl(hue, sat, lgt);
        }

        std::vector<Element> rows;
        rows.reserve(static_cast<size_t>(chart_h + axis_rows));

        // chart rows, top → bottom. Each cell = one of 9 block levels.
        for (int row = chart_h - 1; row >= 0; --row) {
            std::string txt;
            std::vector<StyledRun> runs;
            for (int c = 0; c < chart_w; ++c) {
                float fill = norm[static_cast<size_t>(c)]
                           * static_cast<float>(chart_h) * 8.0f;
                float row_start = static_cast<float>(row) * 8.0f;
                int   b = static_cast<int>(std::clamp(fill - row_start, 0.0f, 8.0f));

                const char* glyph;
                Color       fg;
                Style       st;

                // FFT marker sits on top of this row iff the spectrum peak for
                // the column lands here. We consult it after computing `b` so
                // we can decide whether the FFT should replace an otherwise
                // empty cell or a low-fill cell.
                const int fft_here = fft_row[static_cast<size_t>(c)];
                const bool fft_hit = (fft_here == row);

                if (b > 0) {
                    // filled cell: heat-gradient colour + highlight the cutoff bar
                    glyph = blocks[b];
                    if (show_live && c == live_col) {
                        // Brightest marker in the scene — the actual current
                        // filter corner. This is the thing that moves.
                        fg = Color::rgb(255, 240, 120);
                        st = Style{}.with_fg(fg).with_bold();
                    } else if (c == fc_col) {
                        fg = clr_hot();
                        st = Style{}.with_fg(fg).with_bold();
                    } else {
                        fg = row_col[static_cast<size_t>(row)];
                        st = Style{}.with_fg(fg).with_bold();
                    }
                } else if (fft_hit) {
                    // Spectrum peak in an otherwise empty cell. Draw it as a
                    // pale cyan dotted marker — distinct hue from the filter's
                    // warm palette so the two layers are legible at once.
                    glyph = "\xe2\x97\xa6";               // ◦ small circle
                    fg    = Color::rgb(130, 210, 230);    // pale cyan
                    st    = Style{}.with_fg(fg);
                } else if (show_live && c == live_col) {
                    // solid vertical marker above the chart for the live fc —
                    // brighter and thicker than the static fc ghost line so it
                    // reads as "this is where the envelope pushed it".
                    glyph = "\xe2\x94\x83";               // ┃ heavy vertical
                    fg    = Color::rgb(255, 240, 120);
                    st    = Style{}.with_fg(fg).with_bold();
                } else if (c == fc_col) {
                    // empty cell above the cutoff bar — draw a red dashed ghost
                    // line so the eye traces fc right to the top of the chart.
                    glyph = "\xe2\x94\x8a";               // ┊ light dashed
                    fg    = clr_hot();
                    st    = Style{}.with_fg(fg);
                } else if (row == grid_row_12 || row == grid_row_24) {
                    // dB reference line — tiny inline label at the far left
                    // ("-12" / "-24") followed by a sparse dashed horizontal.
                    // Gives readers a scale cue for the resonance bump.
                    const char* label = (row == grid_row_12) ? "-12" : "-24";
                    if (c < 3) {
                        runs.push_back(StyledRun{txt.size(), 1,
                            Style{}.with_fg(clr_muted())});
                        txt += label[c];
                        continue;
                    } else if (c % 2 == 0) {
                        glyph = "\xe2\x94\x80";           // ─ light horizontal
                        fg    = clr_grid();
                    } else {
                        glyph = " ";
                        fg    = clr_grid();
                    }
                    st = Style{}.with_fg(fg);
                } else {
                    // truly empty — blank, grid-dim dot every 8 cols for rhythm
                    if (c % 12 == 0 && row % 2 == 0) {
                        glyph = "\xc2\xb7";               // ·
                        fg    = clr_grid();
                        st    = Style{}.with_fg(fg);
                    } else {
                        glyph = " ";
                        fg    = clr_grid();
                        st    = Style{}.with_fg(fg);
                    }
                }

                std::string_view gv{glyph};
                runs.push_back(StyledRun{txt.size(), gv.size(), st});
                txt += glyph;
            }
            rows.push_back(Element{TextElement{
                .content = std::move(txt),
                .style   = {},
                .wrap    = TextWrap::NoWrap,
                .runs    = std::move(runs),
            }});
        }

        // frequency axis labels (100 / 1k / 10k), plus a red tick under fc
        std::string axis(static_cast<size_t>(chart_w), ' ');
        auto mark = [&](float f, const char* lbl) {
            float t = std::log(f / f_lo) / std::log(f_hi / f_lo);
            int   c = static_cast<int>(std::round(t * (chart_w - 1)));
            int   len = static_cast<int>(std::string_view(lbl).size());
            int   start = std::max(0, c - len / 2);
            for (int i = 0; i < len && start + i < chart_w; ++i) {
                axis[static_cast<size_t>(start + i)] = lbl[i];
            }
        };
        mark(100.0f,   "100");
        mark(1000.0f,  "1k");
        mark(10000.0f, "10k");

        std::vector<StyledRun> axis_runs;
        axis_runs.push_back(StyledRun{0, axis.size(),
            Style{}.with_fg(clr_muted())});
        rows.push_back(Element{TextElement{
            .content = std::move(axis),
            .style   = {},
            .wrap    = TextWrap::NoWrap,
            .runs    = std::move(axis_runs),
        }});

        return (dsl::v(std::move(rows)) | dsl::gap(0)).build();
    }).grow(1).min_height(Dimension::fixed(5));

    return dsl::vstack()
        .border(BorderStyle::Round)
        .border_color(clr_panel_hi())
        .border_text(" FILTER RESPONSE ", BorderTextPos::Top)
        .align_self(Align::Stretch)
        .padding(0, 1, 0, 1)(Element{std::move(chart)}, std::move(caption));
}

} // namespace tb303
