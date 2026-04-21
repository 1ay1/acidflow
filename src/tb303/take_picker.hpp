#pragma once
// Modal list of live-recording takes. The user pressed `Y`; we scanned the
// config dir for live_*.txt slot files; this widget shows them one per row
// so the user can toggle through with Up/Down and hit Enter to load.

#include <string>
#include <vector>

#include <maya/maya.hpp>

#include "theme.hpp"

namespace tb303 {

using namespace maya;

[[nodiscard]] inline Element build_take_picker(
    const std::vector<std::string>& names,  // display names (file stem)
    int selected)
{
    auto heading = [](const char* s) {
        return Element{TextElement{
            .content = s,
            .style   = Style{}.with_fg(clr_accent()).with_bold(),
        }};
    };

    std::vector<Element> children;
    children.push_back(heading("LOAD LIVE TAKE"));
    children.push_back(Element{TextElement{.content = ""}});

    if (names.empty()) {
        children.push_back(Element{TextElement{
            .content = "no takes yet — press W during playback to record one",
            .style   = Style{}.with_fg(clr_muted()),
        }});
    } else {
        for (size_t i = 0; i < names.size(); ++i) {
            bool sel = (static_cast<int>(i) == selected);
            std::string line;
            line += sel ? "\xe2\x96\xb6 " : "  ";  // ▶
            line += names[i];

            Style st = sel
                ? Style{}.with_fg(clr_accent()).with_bold()
                : Style{}.with_fg(clr_text());
            children.push_back(Element{TextElement{
                .content = std::move(line),
                .style   = st,
                .wrap    = TextWrap::NoWrap,
            }});
        }
    }

    children.push_back(Element{TextElement{.content = ""}});
    children.push_back(Element{TextElement{
        .content = "\xe2\x86\x91/\xe2\x86\x93 navigate   Enter load   Esc cancel",
        .style   = Style{}.with_fg(clr_muted()),
    }});

    return dsl::vstack()
        .border(BorderStyle::Double)
        .border_color(clr_accent())
        .border_text(" TAKES ", BorderTextPos::Top)
        .align_self(Align::Stretch)
        .grow(1.0f)
        .overflow(Overflow::Hidden)
        .padding(1, 3, 1, 3)(std::move(children));
}

} // namespace tb303
