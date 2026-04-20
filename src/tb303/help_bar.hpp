#pragma once
// Short, context-aware help line. Different content per section so the user
// always knows the active keys. When the full chip list is wider than the
// terminal, we rotate items over time (marquee) so every chip is eventually
// visible without the user having to scroll manually.

#include <algorithm>
#include <string>
#include <vector>

#include <maya/maya.hpp>

#include "section.hpp"
#include "theme.hpp"

namespace tb303 {

using namespace maya;

namespace detail {

// Count display columns of a UTF-8 string. Close enough for our label set
// (mostly ASCII plus a handful of single-cell arrows and bullets): treat every
// non-continuation byte as one cell.
inline int u8cols(const char* s) {
    int n = 0;
    for (; *s; ++s) if ((static_cast<unsigned char>(*s) & 0xC0) != 0x80) ++n;
    return n;
}

} // namespace detail

// `avail_w` is the terminal width; when chip width exceeds it we wrap around
// so the bar marquees through the full set. `auto_scroll` rotates items over
// time (one unit = one chip), `user_scroll` is the mouse-wheel offset. The
// two sum so wheeling nudges without stopping the auto animation.
[[nodiscard]] inline Element build_help_bar(Section sec, bool help_open,
                                            int user_scroll = 0,
                                            int avail_w = 0,
                                            int auto_scroll = 0) {
    struct K { const char* keys; const char* desc; };
    std::vector<K> items;
    items.push_back({"Tab",  "section"});
    switch (sec) {
        case Section::Knobs:
            items.push_back({"\xe2\x86\x90\xe2\x86\x92", "select"}); // ←→
            items.push_back({"\xe2\x86\x91\xe2\x86\x93", "value"});  // ↑↓
            items.push_back({"[  ]", "coarse"});
            items.push_back({"0",    "reset"});
            items.push_back({"w",    "wave"});
            items.push_back({"m",    "mute"});
            break;
        case Section::FX:
            items.push_back({"\xe2\x86\x90\xe2\x86\x92", "select"});
            items.push_back({"\xe2\x86\x91\xe2\x86\x93", "value"});
            items.push_back({"[  ]", "coarse"});
            items.push_back({"0",    "reset"});
            items.push_back({"m",    "mute"});
            break;
        case Section::Sequencer:
            items.push_back({"\xe2\x86\x90\xe2\x86\x92", "step"});
            items.push_back({"\xe2\x86\x91\xe2\x86\x93", "pitch"});
            items.push_back({"<  >",  "octave"});
            items.push_back({"a",    "accent"});
            items.push_back({"s",    "slide"});
            items.push_back({"m",    "rest"});
            items.push_back({"v",    "prob"});
            items.push_back({"j",    "ratchet"});
            items.push_back({"F/G/H/J", "plock"});
            items.push_back({"L",    "unlock"});
            items.push_back({"x",    "clear"});
            break;
        case Section::Drums:
            items.push_back({"\xe2\x86\x90\xe2\x86\x92", "step"});
            items.push_back({"\xe2\x86\x91\xe2\x86\x93", "voice"});
            items.push_back({"space/x", "toggle"});
            items.push_back({"1..0",  "quick"});
            items.push_back({"[  ]",  "bus"});
            items.push_back({"m",     "mute"});
            items.push_back({"c/C",   "clear"});
            break;
        case Section::Transport:
            items.push_back({"\xe2\x86\x91\xe2\x86\x93", "preset"}); // ↑↓
            items.push_back({"[  ]",  "bpm"});
            items.push_back({"{  }",  "length"});
            items.push_back({"-  =",  "swing"});
            items.push_back({"m",     "all mute"});
            break;
    }
    items.push_back({"space", "play"});
    items.push_back({"r/R",   "rand/all"});
    items.push_back({"M",     "mutate"});
    items.push_back({"T",     "theme"});
    items.push_back({"e/E",   "wav/mid"});
    items.push_back({"p/P",   "text"});
    items.push_back({"n",     "song"});
    items.push_back({"k",     "jam"});
    items.push_back({"I/O",   "midi"});
    items.push_back({"?",     help_open ? "close" : "help"});
    items.push_back({"q",     "quit"});

    const int sep_cols = 5;           // "  ·  "
    const int pad_cols = 2;           // left+right padding on the bar
    int total_cols = pad_cols;
    for (size_t i = 0; i < items.size(); ++i) {
        total_cols += detail::u8cols(items[i].keys);
        total_cols += 1 + detail::u8cols(items[i].desc);   // " desc"
        if (i + 1 < items.size()) total_cols += sep_cols;
    }

    bool overflow = (avail_w > 0) && (total_cols > avail_w);
    int n = static_cast<int>(items.size());

    // Combined offset: auto marquee advances over time; wheel nudges by hand.
    // When content fits, both are forced to 0 — nothing to scroll past.
    int combined = overflow ? (auto_scroll + user_scroll) : 0;
    int skip = ((combined % n) + n) % n;

    std::vector<Element> row;
    // Wrap each text span in an hstack with shrink(0) so yoga can't collapse
    // the intrinsic width of a label when the full bar overflows. Without this
    // every chip gets proportionally squeezed ("space" → "spa", "play" → "pl"
    // etc.) instead of entire trailing chips being clipped off the right edge.
    auto push_text = [&](std::string content, Style st) {
        Element t = Element{TextElement{
            .content = std::move(content),
            .style   = st,
            .wrap    = TextWrap::NoWrap,
        }};
        row.push_back(dsl::hstack().shrink(0)(std::move(t)));
    };
    auto push_item = [&](const K& it, bool with_sep) {
        if (with_sep) {
            push_text("  \xc2\xb7  ", Style{}.with_fg(clr_dim()));   // "  ·  "
        }
        push_text(it.keys, Style{}.with_fg(clr_accent()).with_bold());
        push_text(std::string(" ") + it.desc, Style{}.with_fg(clr_muted()));
    };

    // When overflowing, emit items as a wrap-around ring starting at `skip`
    // so the animation loops smoothly (the last chip flows back to the first).
    // When not overflowing, emit the full list in declaration order.
    if (overflow) {
        for (int i = 0; i < n; ++i) {
            const K& it = items[static_cast<size_t>((skip + i) % n)];
            push_item(it, i > 0);
        }
    } else {
        for (int i = 0; i < n; ++i) {
            push_item(items[static_cast<size_t>(i)], i > 0);
        }
    }

    return dsl::hstack()
        .padding(0, 1, 0, 1)
        .min_width(Dimension::fixed(0))
        .overflow(Overflow::Hidden)(std::move(row));
}

} // namespace tb303
