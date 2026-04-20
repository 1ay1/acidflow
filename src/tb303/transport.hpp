#pragma once
// Transport bar: status line, playhead sweep, pattern preview, preset list.
// The preset list is a full browser: one row per preset with a 16-glyph mini
// pitch contour so you can read patterns by shape without loading them.

#include <algorithm>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <maya/maya.hpp>

#include "step_data.hpp"
#include "theme.hpp"

namespace tb303 {

using namespace maya;

// Per-preset snapshot — just enough data to render a name + miniature pattern
// contour in the preset list. Kept as std::string + vector<StepData> rather
// than views so TransportState can be safely passed by value.
struct PresetSummary {
    std::string           name;
    std::vector<StepData> steps;
};

struct TransportState {
    bool                        playing;
    float                       bpm;
    float                       swing;            // 0.50 = straight, 0.62 ≈ MPC
    int                         pattern_length;
    int                         current_step;     // -1 when stopped
    int                         pattern_index;
    std::vector<PresetSummary>  presets;          // all presets, full data
    std::span<const StepData>   steps;            // currently-loaded 16 steps
    bool                        focused;
    bool                        song_mode = false; // Phase 4.1: chain saved slots
    int                         song_slot = 0;     // active slot (1..9, 0 = none)
    bool                        jam_mode  = false; // Phase 4.2: live keyboard
    int                         jam_octave = 3;
    int                         jam_last_midi = -1; // -1 if nothing held
    bool                        jam_accent = false;
    bool                        jam_slide  = false;
    bool                        midi_out  = false; // Phase 5: forward notes + clock
    bool                        midi_sync = false; // Phase 5: slaved to ext clock
};

// Render a step pattern as a 16-character-wide block-character contour. Each
// step becomes one block glyph (▁..█) whose height is the note's pitch
// relative to the pattern's min/max, or an underscore for a rest. Used both
// in the preset list (per-row miniature) and anywhere else we want a compact
// melodic fingerprint.
[[nodiscard]] inline std::string build_mini_contour(std::span<const StepData> steps) {
    static constexpr const char* blocks[] = {
        "_",                                           // rest
        "\xe2\x96\x81", "\xe2\x96\x82", "\xe2\x96\x83",
        "\xe2\x96\x84", "\xe2\x96\x85", "\xe2\x96\x86",
        "\xe2\x96\x87", "\xe2\x96\x88",
    };
    int lo = 1000, hi = -1000;
    for (const auto& s : steps) {
        if (s.rest) continue;
        lo = std::min(lo, s.note);
        hi = std::max(hi, s.note);
    }
    if (lo > hi)       { lo = 36; hi = 48; }
    if (hi - lo < 6)   { hi = lo + 6; }

    std::string out;
    out.reserve(steps.size() * 3);
    for (const auto& s : steps) {
        if (s.rest) { out += blocks[0]; continue; }
        float n = static_cast<float>(s.note - lo)
                / static_cast<float>(hi - lo);
        int b = std::clamp(
            static_cast<int>(std::round(n * 7.0f)) + 1, 1, 8);
        out += blocks[b];
    }
    return out;
}

// Build a single TextElement with multiple StyledRuns. Tiny helper — makes the
// transport rows legible without a wall of vector-push-back boilerplate.
namespace detail {
struct RichText {
    std::string            txt;
    std::vector<StyledRun> runs;
    void push(std::string_view s, Style st) {
        runs.push_back(StyledRun{txt.size(), s.size(), st});
        txt.append(s);
    }
    [[nodiscard]] Element build() const {
        return Element{TextElement{
            .content = txt,
            .style   = {},
            .wrap    = TextWrap::NoWrap,
            .runs    = runs,
        }};
    }
};
} // namespace detail

[[nodiscard]] inline Element build_transport(const TransportState& t) {
    using detail::RichText;
    Color border_col = t.focused ? clr_accent() : clr_panel_hi();

    // ── Row 1: status line ───────────────────────────────────────────────────
    // ▶ LIVE / ■ STOPPED, bold colour; then BPM + STEPS with dim labels and
    // bright values so the numbers pop out first when you scan.
    RichText status;
    {
        const char* icon = t.playing ? "\xe2\x96\xb6 " : "\xe2\x96\xa0 ";
        status.push(icon,
            Style{}.with_fg(t.playing ? clr_ok() : clr_muted()).with_bold());
        status.push(t.playing ? "LIVE   " : "IDLE   ",
            Style{}.with_fg(t.playing ? clr_ok() : clr_muted()).with_bold());

        // ♩ pulses red on the beat (every 4 steps), dim otherwise
        bool on_beat = t.playing && t.current_step >= 0
                    && (t.current_step % 4) == 0;
        status.push("\xe2\x99\xa9 ",
            Style{}.with_fg(on_beat ? clr_hot() : clr_accent()).with_bold());

        char bpm_buf[16];
        std::snprintf(bpm_buf, sizeof(bpm_buf), "%.0f", t.bpm);
        status.push(bpm_buf, Style{}.with_fg(clr_text()).with_bold());
        status.push(" BPM   ", Style{}.with_fg(clr_muted()));

        char len_buf[8];
        std::snprintf(len_buf, sizeof(len_buf), "%d", t.pattern_length);
        status.push("\xe2\x96\xae ", Style{}.with_fg(clr_accent()));      // ▮
        status.push(len_buf,         Style{}.with_fg(clr_text()).with_bold());
        status.push(" STEPS   ",     Style{}.with_fg(clr_muted()));

        // Swing — "∿ 50%" straight, brightens to hot red as shuffle increases.
        // The glyph pulses from muted → accent → hot as the knob passes the
        // subtle (~55%) and classic-MPC (~62%) thresholds.
        int swing_pct = static_cast<int>(std::round(t.swing * 100.0f));
        Color sw_col;
        if (swing_pct <= 50)       sw_col = clr_muted();
        else if (swing_pct < 58)   sw_col = clr_accent();
        else                       sw_col = clr_hot();
        status.push("\xe2\x88\xbf ", Style{}.with_fg(sw_col).with_bold());     // ∿
        char sw_buf[8];
        std::snprintf(sw_buf, sizeof(sw_buf), "%d%%", swing_pct);
        status.push(sw_buf,  Style{}.with_fg(clr_text()).with_bold());
        status.push(" SWING", Style{}.with_fg(clr_muted()));

        // Song-mode badge — only drawn when active so the status line stays
        // minimal in the common case. "♪ SONG 3" with the slot number in the
        // hot colour so you can tell at a glance where the chain has advanced.
        if (t.song_mode) {
            status.push("   \xe2\x99\xaa ",                                // ♪
                Style{}.with_fg(clr_hot()).with_bold());
            status.push("SONG ",
                Style{}.with_fg(clr_accent()).with_bold());
            char ss[8];
            std::snprintf(ss, sizeof(ss), "%d", t.song_slot);
            status.push(ss, Style{}.with_fg(clr_hot()).with_bold());
        }

        // MIDI badges. Two compact flags — "◇ MIDI OUT" when we're forwarding
        // events and "◇ MIDI SYNC" when we're slaved to an external clock. Use
        // the slide colour so they read as "external signal" visually, distinct
        // from the hot/accent palette of SONG and JAM.
        if (t.midi_out) {
            status.push("   \xe2\x97\x86 ",                                // ◆
                Style{}.with_fg(clr_slide()).with_bold());
            status.push("MIDI OUT",
                Style{}.with_fg(clr_slide()).with_bold());
        }
        if (t.midi_sync) {
            status.push("   \xe2\x97\x86 ",                                // ◆
                Style{}.with_fg(clr_slide()).with_bold());
            status.push("MIDI SYNC",
                Style{}.with_fg(clr_slide()).with_bold());
        }

        // Jam-mode badge — "♬ JAM Oct3 ♩A C4" with the current octave and the
        // last played note, plus tiny accent/slide flags so the user knows
        // which voicing-modifiers will apply to the next keypress.
        if (t.jam_mode) {
            status.push("   \xe2\x99\xac ",                                // ♬
                Style{}.with_fg(clr_hot()).with_bold());
            status.push("JAM ",
                Style{}.with_fg(clr_accent()).with_bold());
            char ob[12];
            std::snprintf(ob, sizeof(ob), "Oct%d ", t.jam_octave);
            status.push(ob, Style{}.with_fg(clr_text()).with_bold());
            if (t.jam_accent) {
                status.push("A ", Style{}.with_fg(clr_hot()).with_bold());
            }
            if (t.jam_slide) {
                status.push("S ", Style{}.with_fg(clr_slide()).with_bold());
            }
            if (t.jam_last_midi >= 0) {
                static constexpr const char* n[12] = {
                    "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
                int pc = ((t.jam_last_midi % 12) + 12) % 12;
                int oct = (t.jam_last_midi / 12) - 1;
                char nb[12];
                std::snprintf(nb, sizeof(nb), "\xe2\x99\xa9%s%d", n[pc], oct);
                status.push(nb, Style{}.with_fg(clr_hot()).with_bold());
            }
        }
    }

    // ── Row 2: playhead progress bar ─────────────────────────────────────────
    // A horizontal strip with one cell per step (up to pattern_length). Cells
    // to the left of the playhead are lit, the playhead itself is red, rest
    // are dim. Great for seeing the cursor sweep at a glance.
    int  cstep      = t.current_step;
    int  patlen     = t.pattern_length;
    bool playing    = t.playing;
    auto playhead_el = dsl::component(
        [cstep, patlen, playing](int w, int /*h*/) -> Element {
            int text_w = 8;                          // "  N/N "
            int bar_w  = std::clamp(w - text_w, 8, patlen * 3);
            RichText rt;

            // one cell per step, resampled to fit bar_w
            for (int i = 0; i < bar_w; ++i) {
                int step = (i * patlen) / bar_w;
                bool is_current = playing && step == cstep;
                bool played     = playing && step < cstep;

                const char* glyph;
                Color       fg;
                if (is_current)  { glyph = "\xe2\x96\xb0"; fg = clr_hot(); }     // ▰
                else if (played) { glyph = "\xe2\x96\xb0"; fg = clr_accent(); } // ▰
                else             { glyph = "\xe2\x96\xb1"; fg = clr_dim(); }    // ▱
                rt.push(glyph, Style{}.with_fg(fg).with_bold());
            }

            char cnt[16];
            if (playing)
                std::snprintf(cnt, sizeof(cnt), "  %d/%d", cstep + 1, patlen);
            else
                std::snprintf(cnt, sizeof(cnt), "  -/%d", patlen);
            rt.push(cnt,
                Style{}.with_fg(playing ? clr_hot() : clr_muted()).with_bold());

            return rt.build();
        }).grow(1);

    // ── Row 3: pattern preview (pitch contour + accent/slide markers) ───────
    // Mini piano-roll. Each step is one column; height of the block bar
    // encodes the pitch relative to the pattern's range. Rest steps are a
    // single dim dot. The currently-playing step is drawn in red, accents
    // get a red ● under the bar, slides get a cyan / glyph.
    auto pattern_preview = [&]() -> Element {
        using detail::RichText;

        // find pitch range across non-rest steps in the active pattern length
        int lo =  1000, hi = -1000;
        for (int i = 0; i < t.pattern_length
                      && i < static_cast<int>(t.steps.size()); ++i) {
            const auto& s = t.steps[static_cast<size_t>(i)];
            if (s.rest) continue;
            lo = std::min(lo, s.note);
            hi = std::max(hi, s.note);
        }
        if (lo > hi) { lo = 36; hi = 48; }        // all-rest fallback
        if (hi - lo < 6) hi = lo + 6;             // keep a minimum range

        constexpr int ROWS   = 4;                  // pitch rows
        constexpr int CELL_W = 3;                  // step block is 3 cells wide
        static constexpr const char* blocks[] = {
            " ",
            "\xe2\x96\x81", "\xe2\x96\x82", "\xe2\x96\x83", "\xe2\x96\x84",
            "\xe2\x96\x85", "\xe2\x96\x86", "\xe2\x96\x87", "\xe2\x96\x88",
        };

        std::vector<Element> out;
        out.reserve(static_cast<size_t>(ROWS + 1));

        // four stacked block-chart rows, top → bottom
        for (int row = ROWS - 1; row >= 0; --row) {
            RichText rt;
            for (int i = 0; i < t.pattern_length; ++i) {
                const auto& s = t.steps[static_cast<size_t>(i)];
                bool is_cur = t.playing && i == t.current_step;

                int b;
                if (s.rest) {
                    b = 0;
                } else {
                    float norm = static_cast<float>(s.note - lo)
                               / static_cast<float>(hi - lo);
                    float fill = std::clamp(norm, 0.0f, 1.0f)
                               * static_cast<float>(ROWS) * 8.0f;
                    float row_start = static_cast<float>(row) * 8.0f;
                    b = static_cast<int>(std::clamp(fill - row_start, 0.0f, 8.0f));
                }
                const char* glyph = blocks[b];

                Color fg;
                if (is_cur) {
                    fg = clr_hot();
                } else if (s.rest) {
                    fg = clr_grid();
                } else if (s.slide) {
                    fg = clr_slide();
                } else if (s.accent) {
                    fg = clr_accent();
                } else {
                    fg = clr_panel_hi();
                }

                // repeat the block 3 times to give each step visual width
                for (int k = 0; k < CELL_W; ++k) {
                    rt.push(glyph, Style{}.with_fg(fg).with_bold());
                }
            }
            out.push_back(rt.build());
        }

        // thin separator row with per-step marker glyphs (a/s/·)
        RichText marks;
        for (int i = 0; i < t.pattern_length; ++i) {
            const auto& s = t.steps[static_cast<size_t>(i)];
            bool is_cur = t.playing && i == t.current_step;

            const char* g = "   ";
            Style st = Style{}.with_fg(clr_dim());
            if (s.rest) {
                g  = " \xc2\xb7 ";                                // ·
                st = Style{}.with_fg(clr_dim());
            } else if (s.slide && s.accent) {
                g  = "\xe2\x97\x8f/ ";                            // ●/
                st = Style{}.with_fg(clr_slide()).with_bold();
            } else if (s.slide) {
                g  = " / ";
                st = Style{}.with_fg(clr_slide()).with_bold();
            } else if (s.accent) {
                g  = " \xe2\x97\x8f ";                            // ●
                st = Style{}.with_fg(clr_hot()).with_bold();
            } else {
                g  = " \xe2\x97\xa6 ";                            // ◦
                st = Style{}.with_fg(clr_panel_hi());
            }
            if (is_cur) st = Style{}.with_fg(clr_hot()).with_bold();
            marks.push(g, st);
        }
        out.push_back(marks.build());

        return (dsl::v(std::move(out)) | dsl::gap(0)).build();
    }();

    // ── Row 4: full-length preset list with mini-contour previews ────────────
    // Every preset gets its own row, showing:
    //   * a row marker (▸ on the current, ▪ on the others)
    //   * the zero-padded index (01..16)
    //   * the preset name, padded so the mini-contour column aligns
    //   * a 16-character pitch-contour fingerprint of the pattern — each step
    //     becomes one block-char, coloured by its flags (rest=dim, accent=hot,
    //     slide=cyan, plain=panel-hi). The current preset's contour is lit
    //     with the same heat-gradient palette as the filter graph so the
    //     "selected" row visually glows.
    //
    // Scrolls: when the allocated height can't fit every preset, we clamp a
    // scroll window around `pattern_index` so the current preset is always
    // visible, and reserve one header row for ▲/▼ "more above/below" arrows.
    // Captured by value because ComponentElement::render runs after we've
    // already returned from build_transport.
    int preset_count = static_cast<int>(t.presets.size());
    Element preset_list = Element{ComponentElement{
        .render = [presets    = t.presets,
                   pattern_ix = t.pattern_index,
                   focused    = t.focused]
            (int /*w*/, int h) -> Element
        {
            int n = static_cast<int>(presets.size());
            int cv = pattern_ix;

            // pad names to the longest for alignment of the contour column
            size_t name_w = 4;
            for (const auto& p : presets)
                name_w = std::max(name_w, p.name.size());

            // Header (1 row) + dotted rule (1 row) consume 2 lines before any
            // preset row. If the caller gave us less than 3 we still render
            // at least one preset so the list isn't silent.
            int visible = std::max(1, h - 2);
            int first = 0;
            if (n > visible) {
                // Keep the current row centred; clamp at the ends so we don't
                // scroll past the first/last preset.
                first = std::clamp(cv - visible / 2, 0, n - visible);
            }
            int last = std::min(first + visible, n);
            bool more_above = first > 0;
            bool more_below = last < n;

            std::vector<Element> rows;
            rows.reserve(static_cast<size_t>(visible + 2));

            // ── header row ───────────────────────────────────────────────────
            RichText hdr;
            hdr.push("PRESETS  ", Style{}.with_fg(clr_muted()).with_bold());
            // ▲/▼ arrows flag hidden rows above/below — same cells always, just
            // swap to a dim dot when nothing is clipped so the header width
            // stays constant.
            hdr.push(more_above ? "\xe2\x96\xb2" : "\xc2\xb7",             // ▲ / ·
                Style{}.with_fg(more_above ? clr_accent() : clr_dim()).with_bold());
            hdr.push(more_below ? "\xe2\x96\xbc" : "\xc2\xb7",             // ▼ / ·
                Style{}.with_fg(more_below ? clr_accent() : clr_dim()).with_bold());
            hdr.push("  ", Style{});
            hdr.push("\xe2\x86\x91\xe2\x86\x93 browse",                    // ↑↓ browse
                Style{}.with_fg(focused ? clr_accent() : clr_dim()));
            char cnt[16];
            std::snprintf(cnt, sizeof(cnt), "   %d/%d", cv + 1, n);
            hdr.push(cnt, Style{}.with_fg(clr_muted()));
            rows.push_back(hdr.build());

            // subtle dotted rule under the header
            {
                std::string rule;
                for (size_t k = 0; k < name_w + 24; ++k) {
                    rule += (k % 2 == 0) ? "\xc2\xb7" : " ";               // ·
                }
                rows.push_back(Element{TextElement{
                    .content = std::move(rule),
                    .style   = Style{}.with_fg(clr_grid()),
                    .wrap    = TextWrap::NoWrap,
                }});
            }

            // ── visible slice of preset rows ─────────────────────────────────
            for (int i = first; i < last; ++i) {
                bool is_cur = (i == cv);
                const auto& p = presets[static_cast<size_t>(i)];

                RichText r;

                // marker + index
                r.push(is_cur ? " \xe2\x96\xb8 " : "   ",                  // ▸
                    Style{}.with_fg(is_cur ? clr_accent() : clr_dim()).with_bold());
                char idx[8];
                std::snprintf(idx, sizeof(idx), "%02d  ", i + 1);
                r.push(idx,
                    is_cur ? Style{}.with_fg(clr_muted()).with_bold()
                           : Style{}.with_fg(clr_dim()));

                // name, padded to the longest name so contours line up
                std::string name = p.name;
                if (name.size() < name_w) name.append(name_w - name.size(), ' ');
                name += "  ";
                r.push(name,
                    is_cur ? Style{}.with_fg(clr_accent()).with_bold()
                           : Style{}.with_fg(clr_muted()));

                // mini pitch-contour — colour each cell based on its flags
                int lo = 1000, hi = -1000;
                for (const auto& s : p.steps) {
                    if (s.rest) continue;
                    lo = std::min(lo, s.note);
                    hi = std::max(hi, s.note);
                }
                if (lo > hi)     { lo = 36; hi = 48; }
                if (hi - lo < 6) { hi = lo + 6; }

                static constexpr const char* blocks[] = {
                    "_",
                    "\xe2\x96\x81", "\xe2\x96\x82", "\xe2\x96\x83",
                    "\xe2\x96\x84", "\xe2\x96\x85", "\xe2\x96\x86",
                    "\xe2\x96\x87", "\xe2\x96\x88",
                };
                for (const auto& s : p.steps) {
                    const char* glyph;
                    int b;
                    if (s.rest) { glyph = "_"; b = 0; }
                    else {
                        float nrm = static_cast<float>(s.note - lo)
                                  / static_cast<float>(hi - lo);
                        b = std::clamp(
                            static_cast<int>(std::round(nrm * 7.0f)) + 1, 1, 8);
                        glyph = blocks[b];
                    }

                    Color fg;
                    if (is_cur) {
                        if (s.rest)         fg = clr_dim();
                        else if (s.slide)   fg = clr_slide();
                        else if (s.accent)  fg = clr_hot();
                        else                fg = clr_accent();
                    } else {
                        if (s.rest)         fg = clr_grid();
                        else if (s.slide)   fg = Color::rgb(70, 120, 160);
                        else if (s.accent)  fg = Color::rgb(150, 80, 60);
                        else                fg = clr_panel_hi();
                    }
                    Style st = is_cur
                        ? Style{}.with_fg(fg).with_bold()
                        : Style{}.with_fg(fg);
                    r.push(glyph, st);
                }

                rows.push_back(r.build());
            }

            return (dsl::v(std::move(rows)) | dsl::gap(0)).build();
        },
        // Report our *ideal* height (all presets + header + rule) so the
        // layout engine allocates enough vertical space when there's room.
        // When the parent can't afford the full height, flex-shrink reduces
        // our allocation and the render callback scrolls within it.
        .measure = [preset_count](int max_width) -> Size {
            return {Columns{max_width}, Rows{preset_count + 2}};
        },
        .layout = FlexStyle{.grow = 1.0f, .shrink = 1.0f},
    }};

    auto body = dsl::vstack().gap(0)(
        status.build(),
        Element{std::move(playhead_el)},
        Element{TextElement{.content = ""}},
        std::move(pattern_preview),
        Element{TextElement{.content = ""}},
        std::move(preset_list)
    );

    return dsl::vstack()
        .border(BorderStyle::Round)
        .border_color(border_col)
        .border_text(" TRANSPORT ", BorderTextPos::Top)
        .basis(Dimension::fixed(0))
        .min_width(Dimension::fixed(0))
        .overflow(Overflow::Hidden)
        .padding(1, 2, 1, 2)(std::move(body));
}

} // namespace tb303
