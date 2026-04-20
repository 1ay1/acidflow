#pragma once
// TR-style drum grid — 5 rows (BD/SD/CH/OH/CL) × 16 steps. Lit cells show
// hits; the currently-playing column flashes hot, the selected cell is
// bordered in accent. Rows shade by voice family (kick/snare/hat/clap) so a
// glance across the grid reads as a pattern, not a bitfield.

#include <array>
#include <cstdio>

#include <maya/maya.hpp>

#include "theme.hpp"

namespace tb303 {

using namespace maya;

[[nodiscard]] inline Element build_drums(
    const std::array<std::array<bool, 16>, 5>& grid,
    int voice_sel,
    int step_sel,
    int playing_step,
    float master_gain,
    bool focused)
{
    static constexpr const char* kLabels[5] = {"BD","SD","CH","OH","CL"};
    // Voice-family colour cue. Kick in hot (thump), snare in accent (crack),
    // hats in slide (shimmer), clap in accent_d so the row reads by timbre.
    auto row_colour = [](int v) -> Color {
        switch (v) {
            case 0: return clr_hot();
            case 1: return clr_accent();
            case 2: return clr_slide();
            case 3: return clr_slide();
            case 4: return clr_accent_d();
            default: return clr_text();
        }
    };

    std::vector<Element> rows;
    rows.reserve(6);

    // Header: step numbers. "1 . . . 5 . . . 9 . . . 13. . ." pattern that the
    // sequencer uses — just 1s at beat boundaries (steps 1, 5, 9, 13) so the
    // four-beat grid is obvious even before any cells are lit.
    {
        std::vector<StyledRun> runs;
        std::string txt = "    ";             // 4-char label gutter
        runs.push_back(StyledRun{0, 4, Style{}.with_fg(clr_dim())});
        for (int i = 0; i < 16; ++i) {
            size_t s = txt.size();
            char b[4];
            if (i % 4 == 0) std::snprintf(b, sizeof(b), "%2d ", i + 1);
            else            std::snprintf(b, sizeof(b), " . ");
            txt += b;
            Style st = Style{}.with_fg(
                (i == step_sel && focused) ? clr_accent() :
                (i == playing_step)        ? clr_hot()    :
                (i % 4 == 0)               ? clr_muted()  : clr_dim());
            runs.push_back(StyledRun{s, 3, st});
        }
        rows.push_back(Element{TextElement{
            .content = std::move(txt),
            .style   = {},
            .wrap    = TextWrap::NoWrap,
            .runs    = std::move(runs),
        }});
    }

    // One row per voice. Label on the left, 16 cells right of it.
    for (int v = 0; v < 5; ++v) {
        std::vector<StyledRun> runs;
        std::string txt;

        // "BD: " label, highlighted if this row is selected.
        bool row_focused = focused && (v == voice_sel);
        std::string lab;
        lab += kLabels[v];
        lab += ":";
        while (lab.size() < 4) lab += " ";
        runs.push_back(StyledRun{0, lab.size(),
            row_focused
                ? Style{}.with_fg(clr_accent()).with_bold()
                : Style{}.with_fg(clr_muted()).with_bold()});
        txt = std::move(lab);

        Color base = row_colour(v);
        for (int i = 0; i < 16; ++i) {
            bool cell = grid[static_cast<size_t>(v)][static_cast<size_t>(i)];
            bool is_play = (i == playing_step);
            bool is_sel  = (focused && v == voice_sel && i == step_sel);

            const char* glyph;
            Color fg;
            if (cell && is_play)      { glyph = " \xe2\x96\x88 "; fg = clr_hot(); }       // █
            else if (cell)            { glyph = " \xe2\x96\x88 "; fg = base; }            // █
            else if (is_play)         { glyph = " \xe2\x94\x82 "; fg = clr_hot(); }       // │
            else if (i % 4 == 0)      { glyph = " \xc2\xb7 ";     fg = clr_grid(); }      // ·
            else                      { glyph = " \xc2\xb7 ";     fg = clr_dim(); }

            size_t s = txt.size();
            txt += glyph;
            Style st = Style{}.with_fg(fg);
            if (cell) st = st.with_bold();
            runs.push_back(StyledRun{s, txt.size() - s, st});

            // Selection brackets around the current cell — rendered by flipping
            // the surrounding spaces to accent. We approximate with a right-
            // bracket overlay: swap the leading " " of next cell. Cheap hack:
            // just brighten the glyph itself to accent when selected and cell is off.
            if (is_sel) {
                runs.back().style = Style{}
                    .with_fg(clr_accent())
                    .with_bold();
            }
        }

        rows.push_back(Element{TextElement{
            .content = std::move(txt),
            .style   = {},
            .wrap    = TextWrap::NoWrap,
            .runs    = std::move(runs),
        }});
    }

    // Master gain readout + `[  ]` hint so the user knows the bus exists.
    {
        char b[64];
        std::snprintf(b, sizeof(b), "    bus %3d%%  [ / ] adjust",
                      static_cast<int>(std::round(master_gain * 100.0f)));
        rows.push_back(Element{TextElement{
            .content = b,
            .style   = Style{}.with_fg(clr_muted()),
            .wrap    = TextWrap::NoWrap,
        }});
    }

    // Center-align each row within the enclosing panel. The rows themselves
    // are NoWrap text of a fixed width, so align_items(Center) on the vstack
    // pins them to the panel's mid-column rather than the left edge.
    return (dsl::v(std::move(rows))
            | dsl::gap(0)
            | dsl::align(Align::Center)).build();
}

} // namespace tb303
