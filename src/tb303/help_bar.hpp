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

[[nodiscard]] inline Element build_help_bar(Section sec, bool help_open) {
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
        case Section::Sequencer:
            items.push_back({"\xe2\x86\x90\xe2\x86\x92", "step"});
            items.push_back({"\xe2\x86\x91\xe2\x86\x93", "pitch"});
            items.push_back({"<  >",  "octave"});
            items.push_back({"a",    "accent"});
            items.push_back({"s",    "slide"});
            items.push_back({"m",    "mute"});
            items.push_back({"x",    "clear"});
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
    items.push_back({"e",     "wav"});
    items.push_back({"?",     help_open ? "close" : "help"});
    items.push_back({"q",     "quit"});

    std::vector<Element> row;
    bool first = true;
    for (auto& it : items) {
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

    return (dsl::h(std::move(row)) | dsl::padding(0, 1, 0, 1)).build();
}

} // namespace tb303
