#pragma once
// Piano-roll-inspired 16-step view. Instead of 16 cramped cells each with
// their own border, the pattern is rendered as a stack of aligned rows where
// each *row* conveys one facet of the step, and each *column* corresponds
// to one step. A leading column of dim lane labels says what each row is:
//
//                ╷       ╷       ╷           ← beat separators (dim)
//                     ▾  ▼                       ← selection / playhead cursors
//    STEP      1  2  3  4   5  6  7  8    …
//    PITCH     ▃  ·  ▄  ▂   ▃  ▄  ▂  ·    …
//    FLAG      ◉  ○  ●  ●   ●  ●  ◉╱ ○    …
//    NOTE      C2 ·  C3 D#2 C2 C3 D#2 ·   …
//
// - CURSOR row shows the column heads: ▾ for the user-selected step (accent
//   orange) and ▼ for the playhead when playing (hot red). The row renders
//   blank when neither is on that column, so the eye only catches what matters.
// - PITCH sparkline (▁..█) visualises the melodic contour at a glance.
// - FLAG: ○ rest / ● note / ◉ accented + ╱ slide overlay in cyan.
// - NOTE: exact pitch reference (e.g. "D#2") or "·" for rest.
// - Beat separators are rendered as thin, dim ╷ strokes to reinforce the 4/4
//   grouping without pulling focus from the step glyphs.
// - Selected column: all content switches to accent orange.
// - Playing column:  all content switches to hot red.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
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
    // Pitch range for the sparkline — we only count notes that are actually
    // in the pattern (rests don't contribute). If everything is identical or
    // the pattern is empty, we fall back to a safe range so division works.
    int pitch_min = 127, pitch_max = 0;
    for (int i = 0; i < pattern_length; ++i) {
        const auto& s = steps[static_cast<size_t>(i)];
        if (!s.rest) {
            pitch_min = std::min(pitch_min, s.note);
            pitch_max = std::max(pitch_max, s.note);
        }
    }
    if (pitch_max <= pitch_min) pitch_max = pitch_min + 1;

    // Build one horizontal row by calling `cell_fn` for each step. cell_fn
    // fills a 3-column cell plus optional local StyledRuns (offsets relative
    // to the cell start); this helper shifts them into the row's absolute
    // coordinates, prepends a 7-col lane label, and handles the 4/4 beat
    // separator (a single dim ╷ mark in the gap between beats).
    constexpr int kLabelW = 7;           // "STEP   "
    auto build_row = [&](const char* label, auto cell_fn) -> Element {
        std::string txt;
        std::vector<StyledRun> runs;

        // Lane label — soft grey, bold, padded to kLabelW cols
        std::string lbl = label;
        runs.push_back(StyledRun{0, lbl.size(),
            Style{}.with_fg(clr_muted()).with_bold()});
        txt += lbl;
        while (static_cast<int>(txt.size()) < kLabelW) txt += " ";

        for (int i = 0; i < 16; ++i) {
            if (i > 0 && i % 4 == 0) {
                // beat gap — thin dim tick + extra breathing space
                txt += " ";
                size_t off = txt.size();
                txt += "\xe2\x95\xb7"; // ╷  (lightweight top-half vertical)
                runs.push_back(StyledRun{off, 3, Style{}.with_fg(clr_panel())});
                txt += " ";
            } else if (i > 0) {
                txt += " ";
            }
            std::string cell;
            std::vector<StyledRun> cell_runs;
            cell_fn(i, cell, cell_runs);
            for (auto& r : cell_runs) r.byte_offset += txt.size();
            runs.insert(runs.end(), cell_runs.begin(), cell_runs.end());
            txt += cell;
        }
        return Element{TextElement{
            .content = std::move(txt),
            .style   = {},
            .wrap    = TextWrap::NoWrap,
            .runs    = std::move(runs),
        }};
    };

    // Resolve the "accent orange / hot red / regular" colour for a column.
    auto col_colour = [&](int i, Color base) -> Color {
        if (i == selected) return clr_accent();
        if (i == playing)  return clr_hot();
        return base;
    };

    // ── Row 0: cursor heads (▾ selected, ▼ playhead) ──────────────────────
    auto cursors = build_row("", [&](int i, std::string& cell, std::vector<StyledRun>& rr) {
        if (i == selected) {
            cell = " \xe2\x96\xbe ";  // ▾
            rr.push_back(StyledRun{0, cell.size(),
                Style{}.with_fg(clr_accent()).with_bold()});
        } else if (i == playing) {
            cell = " \xe2\x96\xbc ";  // ▼
            rr.push_back(StyledRun{0, cell.size(),
                Style{}.with_fg(clr_hot()).with_bold()});
        } else {
            cell = "   ";
        }
    });

    // ── Row 1: step numbers ───────────────────────────────────────────────
    auto nums = build_row("STEP", [&](int i, std::string& cell, std::vector<StyledRun>& rr) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%-2d ", i + 1); // e.g. "1  ", "14 "
        cell = buf;
        Color base = (i % 4 == 0) ? clr_text() : clr_muted();
        rr.push_back(StyledRun{0, cell.size(),
            Style{}.with_fg(col_colour(i, base)).with_bold()});
    });

    // ── Row 2: pitch sparkline ────────────────────────────────────────────
    // Relative pitch mapped to ▁..█. Rests are left blank. Accented steps
    // are drawn in hot red so they leap off the sparkline even without the
    // accent-row support.
    static constexpr const char* spark[9] = {
        " ",
        "\xe2\x96\x81", "\xe2\x96\x82", "\xe2\x96\x83", "\xe2\x96\x84",
        "\xe2\x96\x85", "\xe2\x96\x86", "\xe2\x96\x87", "\xe2\x96\x88",
    };
    auto sparkline = build_row("PITCH", [&](int i, std::string& cell, std::vector<StyledRun>& rr) {
        const auto& s = steps[static_cast<size_t>(i)];
        bool in_pat = i < pattern_length;
        if (!in_pat || s.rest) {
            cell = " \xc2\xb7 ";  // · for empty column (still takes 3 cols)
            rr.push_back(StyledRun{0, cell.size(),
                Style{}.with_fg(clr_grid())});
            return;
        }
        float t = static_cast<float>(s.note - pitch_min)
                / static_cast<float>(pitch_max - pitch_min);
        int level = static_cast<int>(std::round(t * 7.0f)) + 1;
        level = std::clamp(level, 1, 8);
        cell = std::string(" ") + spark[level] + " ";
        Color base = s.accent ? clr_hot() : clr_accent_d();
        rr.push_back(StyledRun{1, 3,
            Style{}.with_fg(col_colour(i, base)).with_bold()});
    });

    // ── Row 3: accent/rest LED + slide glyph ──────────────────────────────
    auto leds = build_row("FLAG", [&](int i, std::string& cell, std::vector<StyledRun>& rr) {
        const auto& s = steps[static_cast<size_t>(i)];
        bool in_pat = i < pattern_length;
        bool is_ply = (i == playing);
        bool is_sel = (i == selected);

        if (!in_pat || s.rest) {
            cell = " \xe2\x97\x8b ";  // ○
            Color c = (is_sel || is_ply) ? col_colour(i, clr_muted()) : clr_dim();
            rr.push_back(StyledRun{0, cell.size(),
                Style{}.with_fg(c).with_bold()});
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
            Style{}.with_fg(col_colour(i, led_base)).with_bold()});
        if (s.slide) {
            rr.push_back(StyledRun{4, 3,
                Style{}.with_fg(col_colour(i, clr_slide())).with_bold()});
        }
    });

    // ── Row 4: note name ──────────────────────────────────────────────────
    auto notes = build_row("NOTE", [&](int i, std::string& cell, std::vector<StyledRun>& rr) {
        const auto& s = steps[static_cast<size_t>(i)];
        bool in_pat = i < pattern_length;
        if (!in_pat || s.rest) {
            cell = " \xc2\xb7 ";  // · centered in 3-col cell
            rr.push_back(StyledRun{0, cell.size(),
                Style{}.with_fg(clr_dim())});
            return;
        }
        int semi = ((s.note % 12) + 12) % 12;
        int oct  = s.note / 12 - 1;
        std::string name = NOTE_NAMES[static_cast<size_t>(semi)];
        if (name.size() == 2 && name[1] == ' ') name = name.substr(0, 1);
        cell = name + std::to_string(oct);
        while (cell.size() < 3) cell += " ";
        rr.push_back(StyledRun{0, cell.size(),
            Style{}.with_fg(col_colour(i, clr_text())).with_bold()});
    });

    // align_items(Center) on the vstack itself centers each row on the
    // cross-axis (horizontal), so the whole 5-row block floats to the middle
    // of the SEQUENCER panel on wide terminals and stays intrinsic-width
    // (overflow even on both sides) on narrow ones. Must use the vstack()
    // builder directly — `dsl::v(...) | dsl::align(...)` wraps in a default
    // Row box where align_items would mean *vertical* centering instead.
    return dsl::vstack()
        .gap(0)
        .align_items(Align::Center)(
            std::move(cursors),
            std::move(nums),
            std::move(sparkline),
            std::move(leds),
            std::move(notes)
        );
}

} // namespace tb303
