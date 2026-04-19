#pragma once
// Vertical knob — one column of an 8-knob horizontal strip that mirrors the
// 303's signal chain left-to-right (OSC → VCF → EG → VCA). Eight Knobs
// composed with `dsl::h` form the full row, with a flat schematic rail
// overhead grouping the parameters by their block:
//
//    ──── OSC ────   ───────── VCF ─────────   ── EG ──   ──── VCA ────
//     TUNE   WAVE    CUTOFF  RES   ENVMOD       DECAY      ACCENT  VOL
//     ▕  ▏   ◉SAW    ▕  ▏    ▕██▏  ▕  ▏         ▕  ▏       ▕██▏   ▕██▏
//     ▕  ▏   ○SQR    ▕██▏    ▕██▏  ▕  ▏         ▕  ▏       ▕██▏   ▕██▏
//     ▕██▏           ▕██▏    ▕  ▏  ▕██▏         ▕  ▏       ▕  ▏   ▕  ▏
//     ▕  ▏           ▕██▏    ▕  ▏  ▕  ▏         ▕██▏       ▕  ▏   ▕  ▏
//     ▕  ▏           ▕  ▏    ▕  ▏  ▕  ▏         ▕  ▏       ▕  ▏   ▕  ▏
//    ── f₀ ──  ── ∿ ──   ── fc ── ── Q ── ── →fc ──   ── τ ──   ─ C₁₃ ─  ── VCA ─
//    +0.0 st  saw       141 Hz   75%    2.6 oct       399 ms    65%       65%
//
// Why vertical? The key-to-UI mapping matches reality: ←→ moves horizontally
// along the strip (select), ↑↓ drives the column meter (adjust). On the
// previous 2-col vertical grid ↑↓ had to mean "change value" — which felt
// wrong because the UI screamed "navigate vertically."
//
// Width per column: 8 cells. Meter: 5 rows × 8 sub-levels = 40 levels of
// value resolution, which exceeds the 20 steps a 5% knob increment can hit.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include <maya/maya.hpp>

#include "theme.hpp"

namespace tb303 {

using namespace maya;

class Knob {
public:
    std::string label;
    float       value = 0.0f;           // normalised 0..1
    float       default_value = -1.0f;  // 0..1, or <0 to disable default tick
    std::string value_text;             // pre-formatted (e.g. "35%", "+2.0 st")
    std::string tech_symbol;            // schematic symbol in the rule row (fc, Q, τ…)
    bool        focused = false;
    bool        modified = false;       // • dot before label when off-default
    Color       accent = clr_accent();
    Color       group_col = clr_accent();

    static constexpr int WIDTH  = 8;
    static constexpr int METER_H = 5;   // rows of meter body
    // Meter body layout: 2 pad | left rail | 2 bar cells | right rail | 2 pad
    //                   = 2   + 1          + 2           + 1          + 2 = 8
    // Rails are always drawn; bar cells fill bottom-up with partial-block
    // glyphs (▁..█) giving 5×8 = 40 levels of vertical resolution.
    static constexpr int BAR_BODY_W = 2;

    [[nodiscard]] Element build() const {
        Color label_col  = focused ? accent   : clr_text();
        Color bar_col    = focused ? accent   : group_col;
        Color rail_col   = focused ? clr_panel_hi() : clr_dim();
        Color value_col  = focused ? accent   : clr_text();
        Color unit_col   = focused ? accent   : clr_muted();
        Color under_col  = focused ? accent   : clr_dim();

        float v = std::clamp(value, 0.0f, 1.0f);
        int   fill_units = static_cast<int>(std::round(v * METER_H * 8.0f));

        std::vector<Element> rows;
        rows.reserve(static_cast<size_t>(METER_H + 3));

        // ── Label row (centered within WIDTH cells) ─────────────────────────
        {
            // Compose "•LABEL" if modified, else " LABEL". Always 1+len chars
            // of content; pad to WIDTH by centering around that block.
            std::string core;
            bool has_dot = modified;
            int core_cells = static_cast<int>(label.size()) + 1;
            int left = std::max(0, (WIDTH - core_cells) / 2);
            int right = std::max(0, WIDTH - core_cells - left);

            std::string txt(static_cast<size_t>(left), ' ');
            std::vector<StyledRun> runs;
            // modified dot (1 display col, 3 bytes)
            size_t dot_off = txt.size();
            if (has_dot) {
                txt += "\xe2\x80\xa2";   // •
                runs.push_back(StyledRun{dot_off, 3,
                    Style{}.with_fg(focused ? accent : group_col).with_bold()});
            } else {
                txt += " ";
            }
            size_t lbl_off = txt.size();
            txt += label;
            runs.push_back(StyledRun{lbl_off, label.size(),
                Style{}.with_fg(label_col).with_bold()});
            txt.append(static_cast<size_t>(right), ' ');

            rows.push_back(Element{TextElement{
                .content = std::move(txt),
                .style   = {},
                .wrap    = TextWrap::NoWrap,
                .runs    = std::move(runs),
            }});
        }

        // ── Meter rows (top → bottom), fills from bottom up ─────────────────
        // Each body row is:   "  ▕ ██ ▏  "
        //                      pad rail bar rail pad   (2 + 1 + 2 + 1 + 2 = 8)
        // The rails are drawn every row (always visible) so the meter body
        // reads as an LCD bar graph rather than a single floating column.
        // `▕` (right-eighth) sits on the right edge of the left-rail cell,
        // `▏` (left-eighth) sits on the left edge of the right-rail cell —
        // they hug the bar tightly without consuming bar real estate.
        static constexpr const char* LEVELS[9] = {
            " ",
            "\xe2\x96\x81", "\xe2\x96\x82", "\xe2\x96\x83", "\xe2\x96\x84",
            "\xe2\x96\x85", "\xe2\x96\x86", "\xe2\x96\x87", "\xe2\x96\x88",
        };
        static constexpr const char* RAIL_L = "\xe2\x96\x95"; // ▕
        static constexpr const char* RAIL_R = "\xe2\x96\x8f"; // ▏

        for (int row = METER_H - 1; row >= 0; --row) {
            int cell_fill = std::clamp(fill_units - row * 8, 0, 8);

            std::string txt;
            std::vector<StyledRun> runs;

            // Left padding
            txt.append(2, ' ');

            // Left rail
            size_t lr_off = txt.size();
            txt += RAIL_L;
            runs.push_back(StyledRun{lr_off, 3, Style{}.with_fg(rail_col)});

            // 2-cell bar body
            if (cell_fill == 0) {
                txt.append(BAR_BODY_W, ' ');
            } else {
                size_t bar_off = txt.size();
                for (int i = 0; i < BAR_BODY_W; ++i) txt += LEVELS[cell_fill];
                runs.push_back(StyledRun{bar_off, txt.size() - bar_off,
                    Style{}.with_fg(bar_col).with_bold()});
            }

            // Right rail
            size_t rr_off = txt.size();
            txt += RAIL_R;
            runs.push_back(StyledRun{rr_off, 3, Style{}.with_fg(rail_col)});

            // Right padding
            txt.append(2, ' ');

            rows.push_back(Element{TextElement{
                .content = std::move(txt),
                .style   = {},
                .wrap    = TextWrap::NoWrap,
                .runs    = std::move(runs),
            }});
        }

        // ── Tech-labeled rule row ───────────────────────────────────────────
        // Replaces the plain underline with a schematic-style labeled rail:
        // `── fc ──` / `─── Q ──` / `── C₁₃ ─`. The label pops in group_col
        // (or accent when focused); dashes use under_col so the row still
        // reads as a visual separator. Falls back to a plain rule when no
        // tech_symbol is set.
        {
            std::string txt;
            std::vector<StyledRun> runs;
            Color sym_col = focused ? accent : group_col;

            if (tech_symbol.empty()) {
                size_t off = txt.size();
                for (int i = 0; i < WIDTH; ++i) txt += "\xe2\x94\x80"; // ─
                runs.push_back(StyledRun{off, txt.size() - off,
                    Style{}.with_fg(under_col)});
            } else {
                int sym_cols = display_cols(tech_symbol);
                int dashes   = std::max(0, WIDTH - sym_cols - 2); // 2 spaces
                int left     = dashes / 2;
                int right    = dashes - left;

                size_t ld_off = txt.size();
                for (int i = 0; i < left; ++i) txt += "\xe2\x94\x80"; // ─
                txt += " ";
                runs.push_back(StyledRun{ld_off, txt.size() - ld_off,
                    Style{}.with_fg(under_col)});

                size_t sym_off = txt.size();
                txt += tech_symbol;
                runs.push_back(StyledRun{sym_off, tech_symbol.size(),
                    Style{}.with_fg(sym_col).with_bold()});

                size_t rd_off = txt.size();
                txt += " ";
                for (int i = 0; i < right; ++i) txt += "\xe2\x94\x80"; // ─
                runs.push_back(StyledRun{rd_off, txt.size() - rd_off,
                    Style{}.with_fg(under_col)});
            }

            rows.push_back(Element{TextElement{
                .content = std::move(txt),
                .style   = {},
                .wrap    = TextWrap::NoWrap,
                .runs    = std::move(runs),
            }});
        }

        // ── Value text (centered within WIDTH cells) ────────────────────────
        // Split into "numeric prefix" + "unit suffix" so the numbers read
        // bold and bright while the unit stays muted — classic DAW readout.
        // `format_knob` always returns ASCII, so byte-count == column-count.
        {
            size_t sep = 0;
            while (sep < value_text.size()) {
                char c = value_text[sep];
                if ((c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.') ++sep;
                else break;
            }

            int vt_cells = static_cast<int>(value_text.size());
            int left = std::max(0, (WIDTH - vt_cells) / 2);
            int right = std::max(0, WIDTH - vt_cells - left);
            std::string txt(static_cast<size_t>(left), ' ');
            std::vector<StyledRun> runs;

            if (sep > 0) {
                size_t n_off = txt.size();
                txt.append(value_text, 0, sep);
                runs.push_back(StyledRun{n_off, sep,
                    Style{}.with_fg(value_col).with_bold()});
            }
            if (sep < value_text.size()) {
                size_t u_off = txt.size();
                txt.append(value_text, sep);
                runs.push_back(StyledRun{u_off, value_text.size() - sep,
                    Style{}.with_fg(unit_col)});
            }
            txt.append(static_cast<size_t>(right), ' ');
            rows.push_back(Element{TextElement{
                .content = std::move(txt),
                .style   = {},
                .wrap    = TextWrap::NoWrap,
                .runs    = std::move(runs),
            }});
        }

        return (dsl::v(std::move(rows)) | dsl::gap(0)).build();
    }
};

} // namespace tb303
