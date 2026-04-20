#pragma once
// Short, context-aware help line. Different content per section so the user
// always knows the active keys.

#include <string>
#include <vector>

#include <maya/maya.hpp>

#include "section.hpp"
#include "theme.hpp"

namespace tb303 {

using namespace maya;

[[nodiscard]] inline Element build_help_bar(Section sec, bool help_open,
                                            int scroll_offset = 0) {
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
            break;
        case Section::FX:
            items.push_back({"\xe2\x86\x90\xe2\x86\x92", "select"});
            items.push_back({"\xe2\x86\x91\xe2\x86\x93", "value"});
            items.push_back({"[  ]", "coarse"});
            items.push_back({"0",    "reset"});
            break;
        case Section::Sequencer:
            items.push_back({"\xe2\x86\x90\xe2\x86\x92", "step"});
            items.push_back({"\xe2\x86\x91\xe2\x86\x93", "pitch"});
            items.push_back({"<  >",  "octave"});
            items.push_back({"a",    "accent"});
            items.push_back({"s",    "slide"});
            items.push_back({"m",    "mute"});
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
            items.push_back({"c/C",   "clear"});
            break;
        case Section::Transport:
            items.push_back({"\xe2\x86\x91\xe2\x86\x93", "preset"}); // ↑↓
            items.push_back({"[  ]",  "bpm"});
            items.push_back({"{  }",  "length"});
            items.push_back({"-  =",  "swing"});
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

    // Horizontal scroll — skip `scroll_offset` leading items so the wheel on
    // this row can reveal chips that would otherwise be clipped past the right
    // edge on narrow terminals. Clamp so there's always at least one item left.
    int skip = std::clamp(scroll_offset, 0, static_cast<int>(items.size()) - 1);

    std::vector<Element> row;
    bool first = true;
    for (size_t i = static_cast<size_t>(skip); i < items.size(); ++i) {
        auto& it = items[i];
        if (!first) {
            row.push_back(Element{TextElement{
                .content = "  \xc2\xb7  ",   // "  ·  "
                .style   = Style{}.with_fg(clr_dim()),
                .wrap    = TextWrap::NoWrap,
            }});
        }
        first = false;
        row.push_back(Element{TextElement{
            .content = it.keys,
            .style   = Style{}.with_fg(clr_accent()).with_bold(),
            .wrap    = TextWrap::NoWrap,
        }});
        row.push_back(Element{TextElement{
            .content = std::string(" ") + it.desc,
            .style   = Style{}.with_fg(clr_muted()),
            .wrap    = TextWrap::NoWrap,
        }});
    }

    // When scroll is active, a leading chevron reminds the user there's more
    // content to the left. The whole row is set to clip at the panel edge so
    // overflowing chips don't break the layout.
    if (skip > 0) {
        row.insert(row.begin(), Element{TextElement{
            .content = "\xe2\x97\x82 ",   // ◂
            .style   = Style{}.with_fg(clr_accent()),
            .wrap    = TextWrap::NoWrap,
        }});
    }

    return dsl::hstack()
        .padding(0, 1, 0, 1)
        .min_width(Dimension::fixed(0))
        .overflow(Overflow::Hidden)(std::move(row));
}

} // namespace tb303
