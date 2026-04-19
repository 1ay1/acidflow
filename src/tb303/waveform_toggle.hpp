#pragma once
// Vertical SAW/SQR selector sized to match the Knob column (WIDTH=8, same
// number of rows) so the eighth column of the synth strip aligns with the
// other seven. Selected option is shown bright + bold; the other is dimmed.

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <maya/maya.hpp>

#include "knob.hpp"
#include "theme.hpp"

namespace tb303 {

using namespace maya;

class WaveformToggle {
public:
    int         index   = 0;   // 0 = saw, 1 = square
    bool        focused = false;
    Color       group_col = clr_accent();
    std::string tech_symbol = "OSC";   // matches Knob's schematic rule row

    [[nodiscard]] Element build() const {
        constexpr int WIDTH = Knob::WIDTH;

        Color label_col  = focused ? clr_accent() : clr_text();
        Color sel_col    = focused ? clr_accent() : group_col;
        Color unsel_col  = clr_dim();
        Color value_col  = focused ? clr_accent() : clr_text();
        Color under_col  = focused ? clr_accent() : clr_dim();

        auto centered = [](std::string_view core, int w) -> std::pair<std::string, size_t> {
            int cells = static_cast<int>(core.size());
            int left = std::max(0, (w - cells) / 2);
            int right = std::max(0, w - cells - left);
            std::string out(static_cast<size_t>(left), ' ');
            size_t off = out.size();
            out.append(core);
            out.append(static_cast<size_t>(right), ' ');
            return {std::move(out), off};
        };

        std::vector<Element> rows;
        rows.reserve(8);

        // Label row (centered "WAVE")
        {
            auto [txt, off] = centered(" WAVE", WIDTH);
            std::vector<StyledRun> runs;
            runs.push_back(StyledRun{off + 1, 4,
                Style{}.with_fg(label_col).with_bold()});
            rows.push_back(Element{TextElement{
                .content = std::move(txt),
                .style   = {},
                .wrap    = TextWrap::NoWrap,
                .runs    = std::move(runs),
            }});
        }

        // 5 "meter" rows. Options occupy rows 1 (SAW) and 3 (SQR); other rows
        // are blanks. This matches the Knob's 5-row body height.
        auto emit_option_row = [&](int row_idx, const char* glyph, const char* name, bool selected) {
            // Layout:  "  ◉ SAW  "  → 1 space + glyph + space + name + space
            // Core: glyph(1) + space(1) + name(3) = 5 cells; centered in 8.
            std::string core;
            core += " ";
            size_t g_off = 1;
            core += glyph;  // 3 bytes, 1 display col
            core += " ";
            size_t n_off = core.size();
            core += name;
            core += " ";
            (void)g_off;

            auto [txt, off] = centered(core, WIDTH);
            std::vector<StyledRun> runs;
            Color c = selected ? sel_col : unsel_col;
            // glyph run: core position 1, length 3 bytes
            runs.push_back(StyledRun{off + 1, 3,
                Style{}.with_fg(c).with_bold()});
            // name run: byte offset in `core` = 1 + 3 + 1 = 5; length = strlen(name)
            runs.push_back(StyledRun{off + 5, std::string_view(name).size(),
                Style{}.with_fg(c).with_bold()});
            rows.push_back(Element{TextElement{
                .content = std::move(txt),
                .style   = {},
                .wrap    = TextWrap::NoWrap,
                .runs    = std::move(runs),
            }});
            (void)row_idx;
        };

        auto blank_row = [&]() {
            rows.push_back(Element{TextElement{
                .content = std::string(WIDTH, ' '),
                .style   = {},
                .wrap    = TextWrap::NoWrap,
            }});
        };

        // Body: blank, SAW, blank, SQR, blank (5 rows total to match Knob).
        blank_row();
        emit_option_row(1, index == 0 ? "\xe2\x97\x89" : "\xe2\x97\x8b",
                        "SAW", index == 0);
        blank_row();
        emit_option_row(3, index == 1 ? "\xe2\x97\x89" : "\xe2\x97\x8b",
                        "SQR", index == 1);
        blank_row();

        // Tech-labeled rule row — mirrors the Knob class's schematic rail.
        {
            std::string txt;
            std::vector<StyledRun> runs;
            Color sym_col = focused ? clr_accent() : group_col;

            if (tech_symbol.empty()) {
                size_t off = txt.size();
                for (int i = 0; i < WIDTH; ++i) txt += "\xe2\x94\x80"; // ─
                runs.push_back(StyledRun{off, txt.size() - off,
                    Style{}.with_fg(under_col)});
            } else {
                int sym_cols = display_cols(tech_symbol);
                int dashes   = std::max(0, WIDTH - sym_cols - 2);
                int left     = dashes / 2;
                int right    = dashes - left;

                size_t ld_off = txt.size();
                for (int i = 0; i < left; ++i) txt += "\xe2\x94\x80";  // ─
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

        // Value text
        {
            std::string val = index == 0 ? "saw" : "square";
            auto [txt, off] = centered(val, WIDTH);
            std::vector<StyledRun> runs;
            runs.push_back(StyledRun{off, val.size(),
                Style{}.with_fg(value_col).with_bold()});
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
