#pragma once
// Piano-roll-inspired 16-step view, drawn as a bordered grid. Each step is
// a 3-col cell wrapped by │ walls with ┌┬┐/└┴┘ top/bottom rules; selected
// and playing columns highlight by swapping their walls+rules into accent
// (orange) or hot (red). The grid structure + coloured border does all the
// work of indicating which column is active — no separate cursor row or
// underline/chip needed.
//
//        ┌───┬───┬───┬───┬───┬───┬───┬───┬ …
//    STEP│ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │ 8 │ …
//    PITCH  ▃ │ · │ ▄ │ ▂ │ ▃ │ ▄ │ ▂ │ · │ …
//    FLAG │ ◉ │ ○ │ ● │ ● │ ● │ ● │◉╱ │ ○ │ …
//    NOTE │ C2│ · │ C3│D#2│ C2│ C3│D#2│ · │ …
//        └───┴───┴───┴───┴───┴───┴───┴───┴ …
//
// - PITCH sparkline (▁..█) visualises the melodic contour at a glance.
// - FLAG: ○ rest / ● note / ◉ accented + ╱ slide overlay in cyan.
// - NOTE: exact pitch reference (e.g. "D#2") or "·" for rest.
// - Selected column: all 4 walls + content shift to accent orange.
// - Playing column:  all 4 walls + content shift to hot red.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <maya/maya.hpp>

#include "step_data.hpp"
#include "theme.hpp"

namespace tb303 {

using namespace maya;

[[nodiscard]] inline Element build_sequencer(
    const std::array<StepData, 16>& steps,
    int pattern_length,
    int selected,
    int playing)
{
    // Pitch range for the sparkline — only notes actually in the pattern
    // (rests don't contribute). If everything is identical or the pattern
    // is empty, we fall back to a safe range so division works.
    int pitch_min = 127, pitch_max = 0;
    for (int i = 0; i < pattern_length; ++i) {
        const auto& s = steps[static_cast<size_t>(i)];
        if (!s.rest) {
            pitch_min = std::min(pitch_min, s.note);
            pitch_max = std::max(pitch_max, s.note);
        }
    }
    if (pitch_max <= pitch_min) pitch_max = pitch_min + 1;

    constexpr int kLabelW = 7;           // "STEP   "

    // Per-cell highlight colour (for inner content and matching walls).
    auto cell_colour = [&](int i, Color base) -> Color {
        if (i == selected) return clr_accent();
        if (i == playing)  return clr_hot();
        return base;
    };
    // A vertical bar or horizontal rule belongs visually to BOTH adjacent
    // cells; if either side is selected/playing we light it in that colour
    // so the frame around an active cell is continuous on every edge.
    auto frame_colour = [&](int left_col, int right_col) -> Color {
        bool sel = (left_col == selected) || (right_col == selected);
        bool ply = (left_col == playing)  || (right_col == playing);
        if (sel) return clr_accent();
        if (ply) return clr_hot();
        return clr_dim();
    };

    // ── Horizontal rule row (top or bottom of the grid) ──────────────────
    // `left`/`right` are the end caps (┌/┐, └/┘), `mid` the junctions
    // (┬/┴), `h` the horizontal segment (─). Each cell occupies 3 copies
    // of h, and each junction follows — the last cell emits `right` instead.
    auto build_border = [&](const char* left, const char* mid,
                            const char* right, const char* h) -> Element {
        std::string txt(kLabelW, ' ');
        std::vector<StyledRun> runs;

        size_t off = txt.size();
        txt += left;
        runs.push_back(StyledRun{off, std::strlen(left),
            Style{}.with_fg(frame_colour(-1, 0)).with_bold()});

        for (int i = 0; i < 16; ++i) {
            off = txt.size();
            txt += h; txt += h; txt += h;
            runs.push_back(StyledRun{off, 3 * std::strlen(h),
                Style{}.with_fg(cell_colour(i, clr_dim())).with_bold()});

            const char* sep = (i == 15) ? right : mid;
            off = txt.size();
            txt += sep;
            int rcol = (i == 15) ? -1 : i + 1;
            runs.push_back(StyledRun{off, std::strlen(sep),
                Style{}.with_fg(frame_colour(i, rcol)).with_bold()});
        }
        return Element{TextElement{
            .content = std::move(txt),
            .style   = {},
            .wrap    = TextWrap::NoWrap,
            .runs    = std::move(runs),
        }};
    };

    // ── Data row — lane label + │-separated 3-col cells ──────────────────
    // cell_fn fills a 3-col cell plus optional local StyledRuns (offsets
    // relative to the cell start); this helper shifts them into the row's
    // absolute coordinates, prepends the lane label, and draws a │ on each
    // side of every cell (coloured by the adjacent cells' state).
    auto build_row = [&](const char* label, auto cell_fn) -> Element {
        std::string txt;
        std::vector<StyledRun> runs;

        std::string lbl = label;
        runs.push_back(StyledRun{0, lbl.size(),
            Style{}.with_fg(clr_muted()).with_bold()});
        txt += lbl;
        while (static_cast<int>(txt.size()) < kLabelW) txt += " ";

        auto emit_bar = [&](int lcol, int rcol) {
            size_t off = txt.size();
            txt += "\xe2\x94\x82"; // │
            runs.push_back(StyledRun{off, 3,
                Style{}.with_fg(frame_colour(lcol, rcol)).with_bold()});
        };
        emit_bar(-1, 0);

        for (int i = 0; i < 16; ++i) {
            std::string cell;
            std::vector<StyledRun> cell_runs;
            cell_fn(i, cell, cell_runs);
            for (auto& r : cell_runs) r.byte_offset += txt.size();
            runs.insert(runs.end(), cell_runs.begin(), cell_runs.end());
            txt += cell;
            emit_bar(i, (i == 15) ? -1 : i + 1);
        }
        return Element{TextElement{
            .content = std::move(txt),
            .style   = {},
            .wrap    = TextWrap::NoWrap,
            .runs    = std::move(runs),
        }};
    };

    // ── Top rule ──────────────────────────────────────────────────────────
    auto top = build_border(
        "\xe2\x94\x8c",   // ┌
        "\xe2\x94\xac",   // ┬
        "\xe2\x94\x90",   // ┐
        "\xe2\x94\x80");  // ─

    // ── STEP row ──────────────────────────────────────────────────────────
    auto nums = build_row("STEP", [&](int i, std::string& cell, std::vector<StyledRun>& rr) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%2d ", i + 1); // " 1 ", "14 "
        cell = buf;
        Color base = (i % 4 == 0) ? clr_text() : clr_muted();
        rr.push_back(StyledRun{0, cell.size(),
            Style{}.with_fg(cell_colour(i, base)).with_bold()});
    });

    // ── PITCH sparkline row ──────────────────────────────────────────────
    static constexpr const char* spark[9] = {
        " ",
        "\xe2\x96\x81", "\xe2\x96\x82", "\xe2\x96\x83", "\xe2\x96\x84",
        "\xe2\x96\x85", "\xe2\x96\x86", "\xe2\x96\x87", "\xe2\x96\x88",
    };
    auto sparkline = build_row("PITCH", [&](int i, std::string& cell, std::vector<StyledRun>& rr) {
        const auto& s = steps[static_cast<size_t>(i)];
        bool in_pat = i < pattern_length;
        if (!in_pat || s.rest) {
            cell = " \xc2\xb7 ";
            rr.push_back(StyledRun{0, cell.size(),
                Style{}.with_fg(cell_colour(i, clr_grid()))});
            return;
        }
        float t = static_cast<float>(s.note - pitch_min)
                / static_cast<float>(pitch_max - pitch_min);
        int level = static_cast<int>(std::round(t * 7.0f)) + 1;
        level = std::clamp(level, 1, 8);
        cell = std::string(" ") + spark[level] + " ";
        Color base = s.accent ? clr_hot() : clr_accent_d();
        rr.push_back(StyledRun{1, 3,
            Style{}.with_fg(cell_colour(i, base)).with_bold()});
    });

    // ── FLAG row (accent/rest LED + slide glyph) ──────────────────────────
    auto leds = build_row("FLAG", [&](int i, std::string& cell, std::vector<StyledRun>& rr) {
        const auto& s = steps[static_cast<size_t>(i)];
        bool in_pat = i < pattern_length;
        if (!in_pat || s.rest) {
            cell = " \xe2\x97\x8b ";  // ○
            rr.push_back(StyledRun{0, cell.size(),
                Style{}.with_fg(cell_colour(i, clr_dim())).with_bold()});
            return;
        }
        const char* led = s.accent ? "\xe2\x97\x89"   // ◉
                                   : "\xe2\x97\x8f";  // ●
        Color led_base = s.accent ? clr_hot() : clr_accent();
        cell = " ";
        cell += led;
        if (s.slide) cell += "\xe2\x95\xb1";          // ╱
        else         cell += " ";
        rr.push_back(StyledRun{1, 3,
            Style{}.with_fg(cell_colour(i, led_base)).with_bold()});
        if (s.slide) {
            rr.push_back(StyledRun{4, 3,
                Style{}.with_fg(cell_colour(i, clr_slide())).with_bold()});
        }
    });

    // ── NOTE row (C2 / D#2 / ·) ───────────────────────────────────────────
    auto notes = build_row("NOTE", [&](int i, std::string& cell, std::vector<StyledRun>& rr) {
        const auto& s = steps[static_cast<size_t>(i)];
        bool in_pat = i < pattern_length;
        if (!in_pat || s.rest) {
            cell = " \xc2\xb7 ";
            rr.push_back(StyledRun{0, cell.size(),
                Style{}.with_fg(cell_colour(i, clr_dim()))});
            return;
        }
        int semi = ((s.note % 12) + 12) % 12;
        int oct  = s.note / 12 - 1;
        std::string name = NOTE_NAMES[static_cast<size_t>(semi)];
        if (name.size() == 2 && name[1] == ' ') name = name.substr(0, 1);
        cell = name + std::to_string(oct);      // "C2" or "D#2"
        while (cell.size() < 3) cell = " " + cell;
        rr.push_back(StyledRun{0, cell.size(),
            Style{}.with_fg(cell_colour(i, clr_text())).with_bold()});
    });

    // ── Bottom rule ───────────────────────────────────────────────────────
    auto bottom = build_border(
        "\xe2\x94\x94",   // └
        "\xe2\x94\xb4",   // ┴
        "\xe2\x94\x98",   // ┘
        "\xe2\x94\x80");  // ─

    // align_items(Center) on the vstack itself centers each row on the
    // cross-axis (horizontal), so the whole block floats to the middle of
    // the SEQUENCER panel on wide terminals and stays intrinsic-width on
    // narrow ones.
    return dsl::vstack()
        .gap(0)
        .align_items(Align::Center)(
            std::move(top),
            std::move(nums),
            std::move(sparkline),
            std::move(leds),
            std::move(notes),
            std::move(bottom)
        );
}

} // namespace tb303
