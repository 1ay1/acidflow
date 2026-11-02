#pragma once
// The theme picker card. Each row shows the theme's name and a swatch of its
// own colours (resolved from that theme, not the live one), so you can see a
// theme before you move onto it.

#include <algorithm>
#include <string>
#include <vector>

#include <maya/maya.hpp>

#include "hit_ids.hpp"
#include "palette.hpp"
#include "theme.hpp"

namespace tb303 {

using namespace maya;

[[nodiscard]] inline Element build_theme_picker(const std::string& query,
                                                const std::vector<int>& matches,
                                                int sel, int max_rows, int live_idx) {
    const auto& all = all_themes();
    std::vector<Element> rows;
    rows.push_back(Element{TextElement{
        .content = "\xe2\x8c\x95 " + query + "\xe2\x96\x8f",
        .style   = Style{}.with_fg(clr_text()).with_bold()}});
    rows.push_back(Element{TextElement{
        .content = query.empty()
            ? std::to_string(all.size()) + " themes \xc2\xb7 type to search"
            : std::to_string(matches.size()) + " match" + (matches.size() == 1 ? "" : "es"),
        .style   = Style{}.with_fg(clr_dim())}});

    const int n = static_cast<int>(matches.size());
    const int cap = std::max(1, max_rows);
    const int first = std::clamp(sel - cap / 2, 0, std::max(0, n - cap));
    for (int r = first; r < std::min(n, first + cap); ++r) {
        const int idx = matches[static_cast<size_t>(r)];
        const ThemeChoice& tc = all[static_cast<size_t>(idx)];
        const Theme& t = *tc.theme;
        const bool on = r == sel;
        const bool fav = idx < kThemeCount;
        // Swatch: background, then the roles acidflow leans on most.
        std::string sw;
        std::vector<StyledRun> runs;
        auto chip = [&](LitColor fg, LitColor bg) {
            const size_t off = sw.size();
            sw += "\xe2\x96\x88";   // █
            Style st = Style{}.with_fg(Color{fg});
            if (bg.has_channels()) st = st.with_bg(Color{bg});
            runs.push_back(StyledRun{off, 3, st});
        };
        const LitColor bg = t.resolve(t.background);
        for (Color c : {t.background, t.primary, t.error, t.info, t.success, t.warning, t.text})
            chip(t.resolve(c), bg);
        std::string name = std::string(on ? "\xe2\x96\xb8 " : "  ") + tc.name;
        if (idx == live_idx && !on) name += "  \xe2\x80\xa2";   // • what was live
        rows.push_back((dsl::h(
            dsl::text(std::move(name)) | dsl::fgc(on ? clr_accent() : clr_text())
                | (on ? dsl::Bold : dsl::Bold) | dsl::grow(1),
            dsl::text(fav ? "\xe2\x98\x85 " : "  ") | dsl::fgc(clr_dim()),
            Element{TextElement{.content = std::move(sw), .style = {},
                                .wrap = TextWrap::NoWrap, .runs = std::move(runs)}}
        ) | (on ? dsl::bgc(clr_panel()) : dsl::bgc(card_bg()))
          | dsl::hit(hit_id(kHitThemeRow, static_cast<std::uint32_t>(r)))).build());
    }
    if (n == 0)
        rows.push_back(dsl::text("  no theme matches") | dsl::fgc(clr_muted()));
    rows.push_back(dsl::text("\xe2\x86\x91\xe2\x86\x93 preview \xc2\xb7 Enter keep \xc2\xb7 Esc undo \xc2\xb7 \xe2\x98\x85 = favourite")
                   | dsl::fgc(clr_dim()));

    return (dsl::vstack()
        .border(BorderStyle::Round)
        .border_color(clr_accent())
        .border_text(" THEME ", BorderTextPos::Top)
        .bg(card_bg())
        .padding(0, 1, 0, 1)
        .width(Dimension::fixed(48))(std::move(rows))
        | dsl::hit(hit_id(kHitThemeCard))).build();
}

} // namespace tb303
