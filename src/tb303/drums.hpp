#pragma once
// TR-style drum grid — 16 rows (BD/SD/CH/OH/CL/LT/HT/RS/CB/SH/TB/CG/MT/CY/
// RD/BG) × 16 steps. Lit cells show hits; the currently-playing column
// flashes hot, the selected cell is bordered in accent. Rows shade by voice
// family so a glance across the grid reads as a pattern, not a bitfield.

#include <array>
#include <cstdio>

#include <maya/maya.hpp>

#include "theme.hpp"

namespace tb303 {

using namespace maya;

inline constexpr int kDrumGridVoices = 16;

[[nodiscard]] inline Element build_drums(
    const std::array<std::array<bool, 16>, kDrumGridVoices>& grid,
    const std::array<bool, kDrumGridVoices>& voice_mute,
    int voice_sel,
    int step_sel,
    int playing_step,
    float master_gain,
    bool focused)
{
    static constexpr const char* kLabels[kDrumGridVoices] = {
        "BD","SD","CH","OH","CL","LT","HT","RS","CB","SH","TB","CG",
        "MT","CY","RD","BG"
    };
    // Voice-family colour cue. Kick/toms/conga/bongo in hot (thumpy pitched),
    // snare/rim in accent (cracky transients), hats/shaker/tambourine/cymbals
    // in slide (shimmery noise), clap/cowbell in accent_d.
    auto row_colour = [](int v) -> Color {
        switch (v) {
            case 0:  return clr_hot();        // BD
            case 1:  return clr_accent();     // SD
            case 2:  return clr_slide();      // CH
            case 3:  return clr_slide();      // OH
            case 4:  return clr_accent_d();   // CL
            case 5:  return clr_hot();        // LT
            case 6:  return clr_hot();        // HT
            case 7:  return clr_accent();     // RS
            case 8:  return clr_accent_d();   // CB
            case 9:  return clr_slide();      // SH
            case 10: return clr_slide();      // TB
            case 11: return clr_hot();        // CG
            case 12: return clr_hot();        // MT
            case 13: return clr_slide();      // CY
            case 14: return clr_slide();      // RD
            case 15: return clr_hot();        // BG
            default: return clr_text();
        }
    };

    std::vector<Element> rows;
    rows.reserve(static_cast<size_t>(kDrumGridVoices) + 2);

    // Header: step numbers. "1 . . . 5 . . . 9 . . . 13. . ." pattern — just
    // 1s at beat boundaries so the four-beat grid is obvious before any cells
    // are lit.
    {
        std::vector<StyledRun> runs;
        std::string txt = "    ";
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

    for (int v = 0; v < kDrumGridVoices; ++v) {
        std::vector<StyledRun> runs;
        std::string txt;

        bool row_focused = focused && (v == voice_sel);
        bool muted = voice_mute[static_cast<size_t>(v)];
        std::string lab;
        lab += kLabels[v];
        lab += muted ? "M" : ":";
        while (lab.size() < 4) lab += " ";
        Color lab_col = muted       ? clr_dim()
                      : row_focused ? clr_accent()
                                    : clr_muted();
        runs.push_back(StyledRun{0, lab.size(),
            Style{}.with_fg(lab_col).with_bold()});
        txt = std::move(lab);

        Color base = muted ? clr_dim() : row_colour(v);
        for (int i = 0; i < 16; ++i) {
            bool cell = grid[static_cast<size_t>(v)][static_cast<size_t>(i)];
            bool is_play = (i == playing_step);
            bool is_sel  = (focused && v == voice_sel && i == step_sel);

            const char* glyph;
            Color fg;
            if (muted && cell)        { glyph = " \xe2\x96\x88 "; fg = clr_dim(); }
            else if (cell && is_play) { glyph = " \xe2\x96\x88 "; fg = clr_hot(); }
            else if (cell)            { glyph = " \xe2\x96\x88 "; fg = base; }
            else if (is_play)         { glyph = " \xe2\x94\x82 "; fg = muted ? clr_dim() : clr_hot(); }
            else if (i % 4 == 0)      { glyph = " \xc2\xb7 ";     fg = clr_grid(); }
            else                      { glyph = " \xc2\xb7 ";     fg = clr_dim(); }

            size_t s = txt.size();
            txt += glyph;
            Style st = Style{}.with_fg(fg);
            if (cell) st = st.with_bold();
            runs.push_back(StyledRun{s, txt.size() - s, st});

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

    return (dsl::v(std::move(rows))
            | dsl::gap(0)
            | dsl::align(Align::Center)).build();
}

} // namespace tb303
