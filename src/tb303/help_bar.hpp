#pragma once
// The bottom line. Normally: the few most useful keys for the focused panel,
// then ": commands  ? help" pinned at the right so the way to everything else
// is always on screen. When the mouse hovers something, it shows what that
// thing is instead.

#include <algorithm>
#include <string>
#include <vector>

#include <maya/maya.hpp>

#include "../app/actions.hpp"
#include "section.hpp"
#include "theme.hpp"

namespace tb303 {

using namespace maya;

namespace detail {
// Display columns of a UTF-8 string (our glyphs are all 1 cell wide).
inline int u8cols(std::string_view s) {
    int n = 0;
    for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++n;
    return n;
}
}  // namespace detail

[[nodiscard]] inline Element build_help_bar(Section focus, const std::string& hover,
                                            const std::string& mode_note, int avail_w) {
    struct Chip { std::string k, v; };
    std::vector<Chip> chips;
    if (!mode_note.empty()) chips.push_back({"", mode_note});
    if (hover.empty()) {
        // Priority 1 first, then 2 if there's room. Global keys are listed in
        // the welcome card and help, so only Space/Tab make the bar.
        for (int pri : {1, 2})
            for (const Action* a : actions_for(scope_of(focus))) {
                if (a->hint != pri) continue;
                if (a->scope == Scope::Global && std::string_view(a->id) != "play"
                    && std::string_view(a->id) != "focus.next") continue;
                chips.push_back({a->keys, a->name});
            }
    }
    const Chip tail_cmd{":", "commands"}, tail_help{"?", "help"};

    auto width_of = [](const Chip& c) {
        return (c.k.empty() ? 0 : detail::u8cols(c.k) + 1) + detail::u8cols(c.v);
    };
    const int sep = 4;   // "  ·  " minus one
    int budget = std::max(0, avail_w - 2 - width_of(tail_cmd) - width_of(tail_help) - 2 * sep - 2);

    std::string txt;
    std::vector<StyledRun> runs;
    auto put = [&](const Chip& c, bool first) {
        if (!first) {
            size_t o = txt.size(); txt += "  \xc2\xb7  ";
            runs.push_back({o, txt.size() - o, Style{}.with_fg(clr_dim())});
        }
        if (!c.k.empty()) {
            size_t o = txt.size(); txt += c.k;
            runs.push_back({o, txt.size() - o, Style{}.with_fg(clr_accent()).with_bold()});
            txt += " ";
        }
        size_t o = txt.size(); txt += c.v;
        runs.push_back({o, txt.size() - o, Style{}.with_fg(clr_muted())});
    };

    if (!hover.empty()) {
        // Hover text, trimmed to fit.
        std::string h = hover;
        while (detail::u8cols(h) > budget && !h.empty()) h.pop_back();
        size_t o = txt.size(); txt += h;
        runs.push_back({o, txt.size() - o, Style{}.with_fg(clr_text())});
    } else {
        bool first = true;
        for (const auto& c : chips) {
            const int w = width_of(c) + (first ? 0 : sep + 1);
            if (w > budget) break;   // drop the rest rather than cut a chip in half
            put(c, first);
            budget -= w;
            first = false;
        }
    }

    // Right-pinned tail.
    std::string tail;
    std::vector<StyledRun> truns;
    auto tput = [&](const Chip& c) {
        if (!tail.empty()) { size_t o = tail.size(); tail += "   "; truns.push_back({o, 3, Style{}}); }
        size_t o = tail.size(); tail += c.k;
        truns.push_back({o, tail.size() - o, Style{}.with_fg(clr_accent()).with_bold()});
        tail += " ";
        o = tail.size(); tail += c.v;
        truns.push_back({o, tail.size() - o, Style{}.with_fg(clr_muted())});
    };
    tput(tail_cmd);
    tput(tail_help);

    return dsl::hstack()
        .padding(0, 1, 0, 1)
        .justify(Justify::SpaceBetween)
        .min_width(Dimension::fixed(0))
        .height(Dimension::fixed(1))
        .min_height(Dimension::fixed(1))
        .shrink(0)
        .overflow(Overflow::Hidden)(
            Element{TextElement{.content = std::move(txt), .style = {},
                                .wrap = TextWrap::NoWrap, .runs = std::move(runs)}},
            Element{TextElement{.content = std::move(tail), .style = {},
                                .wrap = TextWrap::NoWrap, .runs = std::move(truns)}});
}

} // namespace tb303
