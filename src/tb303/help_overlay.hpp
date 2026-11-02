#pragma once
// Help screen. Built from the action table, so every key it lists is a key
// that works. Opens on the panel you were in; type to search everything;
// Tab flips between panels. A short "words" section explains the 303 jargon
// the knobs use.

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

#include <maya/maya.hpp>

#include "../app/actions.hpp"
#include "theme.hpp"

namespace tb303 {

using namespace maya;

struct HelpTerm { const char* word; const char* meaning; };

// Plain-English glossary for the words on the panels.
inline constexpr HelpTerm kHelpTerms[] = {
    {"CUTOFF", "How bright the sound is. Low = dark and round, high = buzzy."},
    {"RES",    "Resonance. Adds a whistle at the cutoff. Turn it up for the acid squelch."},
    {"ENVMOD", "How far each note sweeps the cutoff up. More = more 'wow'."},
    {"DECAY",  "How fast that sweep falls back. Short = plucky, long = vowel-like."},
    {"ACCENT", "How much harder accented steps hit. Accents also sweep the filter more."},
    {"Slide",  "The note glides into the next one instead of re-starting. Key to the 303 sound."},
    {"Chance", "A step's probability of playing each time round."},
    {"Ratchet","Play a step 2-4 times quickly, like a drum roll."},
    {"P-lock", "A knob value saved on one step only, so one note can sound different."},
    {"Swing",  "Every second 16th note is pushed late, for groove."},
};

namespace help_detail {

inline std::string lower(std::string_view s) {
    std::string o(s);
    for (auto& c : o) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return o;
}
inline bool contains(std::string_view hay, const std::string& q) {
    return lower(hay).find(q) != std::string::npos;
}

}  // namespace help_detail

// `from` is the panel help was opened from; `query` is the search text.
[[nodiscard]] inline Element build_help_overlay(Scope from, const std::string& query,
                                                int scroll_offset) {
    using help_detail::contains;
    const std::string q = help_detail::lower(query);

    auto heading = [](std::string s, Color c) {
        return Element{TextElement{.content = std::move(s),
                                   .style = Style{}.with_fg(c).with_bold()}};
    };
    auto blank = [] { return Element{TextElement{.content = ""}}; };
    // Pad to a fixed column count (our glyphs are all one cell wide).
    auto pad = [](std::string s, int w) {
        int cols = 0;
        for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++cols;
        if (cols < w) s.append(static_cast<size_t>(w - cols), ' ');
        return s;
    };
    // keys | name | description, as one row of runs so the columns line up
    // and the description wraps under itself.
    auto row = [&](const Action& a) {
        const std::string k = pad(std::string("  ") + a.keys, 16);
        const std::string n = pad(a.name, 28);
        return (dsl::h(
            dsl::text(k) | dsl::Bold | dsl::fgc(clr_accent()) | dsl::shrink(0),
            dsl::text(n) | dsl::Bold | dsl::fgc(clr_text())   | dsl::shrink(0),
            dsl::text(a.help) | dsl::fgc(clr_muted()) | dsl::grow(1) | dsl::shrink(1)
        )).build();
    };

    std::vector<Element> body;

    // Search box line.
    {
        std::string line = query.empty()
            ? "  \xe2\x8c\x95 type to search \xc2\xb7 Tab: other panels \xc2\xb7 \xe2\x86\x91\xe2\x86\x93 scroll \xc2\xb7 Esc: close"
            : "  \xe2\x8c\x95 " + query + "\xe2\x96\x8f   (Esc clears)";
        body.push_back(Element{TextElement{
            .content = std::move(line),
            .style = Style{}.with_fg(query.empty() ? clr_muted() : clr_text())}});
        body.push_back(blank());
    }

    int shown = 0;
    auto section = [&](Scope s, bool first) {
        std::vector<const Action*> rows;
        for (const auto& a : actions())
            if (a.scope == s && (q.empty() || contains(a.name, q) || contains(a.help, q) || contains(a.keys, q)))
                rows.push_back(&a);
        if (rows.empty()) return;
        std::string title = scope_name(s);
        if (first && q.empty()) title = "\xe2\x96\xb8 " + title + "  (where you are)";
        body.push_back(heading(std::move(title), first ? clr_accent() : clr_text()));
        for (const Action* a : rows) { body.push_back(row(*a)); ++shown; }
        body.push_back(blank());
    };

    // The panel you came from first, then everywhere-keys, then the rest.
    section(from, true);
    if (from != Scope::Global) section(Scope::Global, false);
    for (Scope s : {Scope::Knobs, Scope::FX, Scope::Sequencer, Scope::Drums, Scope::Transport})
        if (s != from) section(s, false);

    // Glossary.
    std::vector<const HelpTerm*> terms;
    for (const auto& t : kHelpTerms)
        if (q.empty() || contains(t.word, q) || contains(t.meaning, q)) terms.push_back(&t);
    if (!terms.empty()) {
        body.push_back(heading("What the words mean", clr_text()));
        for (const HelpTerm* t : terms) {
            body.push_back((dsl::h(
                dsl::text(pad(std::string("  ") + t->word, 16)) | dsl::Bold | dsl::fgc(clr_slide()) | dsl::shrink(0),
                dsl::text(t->meaning) | dsl::fgc(clr_muted()) | dsl::grow(1) | dsl::shrink(1)
            )).build());
            ++shown;
        }
        body.push_back(blank());
    }

    if (q.empty()) {
        body.push_back(heading("Mouse", clr_text()));
        body.push_back(dsl::text("  Click a step or drum cell to edit it \xc2\xb7 drag a knob up/down \xc2\xb7 "
                                 "scroll to fine-tune \xc2\xb7 right-click to reset or clear \xc2\xb7 "
                                 "hover anything to see what it does") | dsl::fgc(clr_muted()));
        body.push_back(blank());
        body.push_back(dsl::text("  Your session is saved when you quit. Files live in ~/.config/acidflow/.")
                       | dsl::fgc(clr_dim()));
    } else if (shown == 0) {
        body.push_back(dsl::text("  Nothing matches \"" + query + "\". Try a word like tempo, slide, save or random.")
                       | dsl::fgc(clr_muted()));
    }

    const int skip = std::clamp(scroll_offset, 0, std::max(0, static_cast<int>(body.size()) - 3));
    std::vector<Element> visible(body.begin() + skip, body.end());

    return dsl::vstack()
        .border(BorderStyle::Round)
        .border_color(clr_accent())
        .border_text(" HELP ", BorderTextPos::Top)
        .align_self(Align::Stretch)
        .grow(1.0f)
        .overflow(Overflow::Hidden)
        .padding(1, 2, 0, 2)(std::move(visible));
}

} // namespace tb303
