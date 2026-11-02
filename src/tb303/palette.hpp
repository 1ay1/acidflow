#pragma once
// The command palette (: or Ctrl-K) and the first-run welcome card.

#include <algorithm>
#include <string>
#include <vector>

#include <maya/maya.hpp>

#include "../app/actions.hpp"
#include "theme.hpp"

namespace tb303 {

using namespace maya;

// An opaque backing for floating cards, a little darker than the panels so
// they read as on top.
inline Color card_bg() {
    const Color g = clr_grid();
    if (g.kind() != Color::Kind::Rgb) return Color::rgb(16, 16, 20);
    return Color::rgb(static_cast<uint8_t>(g.raw_r() / 2), static_cast<uint8_t>(g.raw_g() / 2),
                      static_cast<uint8_t>(g.raw_b() / 2));
}

[[nodiscard]] inline Element build_palette(const std::string& query,
                                           const std::vector<const Action*>& matches,
                                           int sel, int max_rows) {
    std::vector<Element> rows;
    rows.push_back(Element{TextElement{
        .content = ": " + query + "\xe2\x96\x8f",
        .style   = Style{}.with_fg(clr_text()).with_bold()}});
    rows.push_back(Element{TextElement{
        .content = query.empty() ? "type what you want to do \xe2\x80\x94 e.g. tempo, random, save, slide"
                                 : std::to_string(matches.size()) + " match" + (matches.size() == 1 ? "" : "es"),
        .style   = Style{}.with_fg(clr_dim())}});

    const int n = static_cast<int>(matches.size());
    const int cap = std::max(1, max_rows);
    const int first = std::clamp(sel - cap / 2, 0, std::max(0, n - cap));
    for (int i = first; i < std::min(n, first + cap); ++i) {
        const Action& a = *matches[static_cast<size_t>(i)];
        const bool on = i == sel;
        const Color fg = on ? clr_accent() : clr_text();
        std::string where = a.scope == Scope::Global ? "" : std::string(scope_name(a.scope));
        // Key-only entries can't be run from here; the key column says how.
        const std::string keys = a.run ? std::string(a.keys) : std::string("key ") + a.keys;
        rows.push_back((dsl::h(
            dsl::text(on ? "\xe2\x96\xb8 " : "  ") | dsl::fgc(clr_accent()),
            dsl::text(a.name) | dsl::Bold | dsl::fgc(fg) | dsl::grow(1),
            dsl::text(where) | dsl::fgc(clr_dim()) | dsl::width(13),
            dsl::text(keys) | dsl::fgc(on ? clr_accent() : clr_muted()) | dsl::width(14)
        )).build());
        if (on) rows.push_back(dsl::text(std::string("    ") + a.help) | dsl::fgc(clr_muted()));
    }
    if (n == 0)
        rows.push_back(dsl::text("  no action matches \xe2\x80\x94 ? opens help, which explains everything")
                       | dsl::fgc(clr_muted()));
    rows.push_back(dsl::text("\xe2\x86\x91\xe2\x86\x93 choose  \xc2\xb7  Enter run  \xc2\xb7  Esc close")
                   | dsl::fgc(clr_dim()));

    return dsl::vstack()
        .border(BorderStyle::Round)
        .border_color(clr_accent())
        .border_text(" COMMANDS ", BorderTextPos::Top)
        .bg(card_bg())
        .padding(0, 1, 0, 1)
        .width(Dimension::fixed(72))(std::move(rows));
}

[[nodiscard]] inline Element build_welcome() {
    auto line = [](const char* k, const char* what) {
        return (dsl::h(
            dsl::text(k) | dsl::Bold | dsl::fgc(clr_accent()) | dsl::width(10),
            dsl::text(what) | dsl::fgc(clr_text())
        )).build();
    };
    return dsl::vstack()
        .border(BorderStyle::Round)
        .border_color(clr_accent())
        .border_text(" welcome to acidflow ", BorderTextPos::Top)
        .bg(card_bg())
        .padding(1, 3, 1, 3)
        .width(Dimension::fixed(56))(
            dsl::text("A TB-303 bassline machine. Here's all you need:") | dsl::fgc(clr_muted()),
            dsl::text(""),
            line("Space", "play / stop"),
            line("Tab", "move between panels (the orange box)"),
            line("arrows", "change what's in the box"),
            line(":", "search every action"),
            line("?", "help for where you are"),
            line("u", "undo anything"),
            dsl::text(""),
            dsl::text("Press Enter to start the beat.") | dsl::Bold | dsl::fgc(clr_ok()));
}

} // namespace tb303
