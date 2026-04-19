#pragma once
// Full-screen-ish modal shown when user presses ?. Mouse-free and scannable.

#include <vector>

#include <maya/maya.hpp>

#include "theme.hpp"

namespace tb303 {

using namespace maya;

[[nodiscard]] inline Element build_help_overlay() {
    auto heading = [](const char* s) {
        return Element{TextElement{
            .content = s,
            .style   = Style{}.with_fg(clr_accent()).with_bold(),
        }};
    };
    auto row = [](const char* k, const char* d) {
        return (dsl::h(
            dsl::text(k) | dsl::Bold | dsl::fgc(clr_text()) | dsl::width(14),
            dsl::text(d) | dsl::fgc(clr_muted())
        )).build();
    };

    std::vector<Element> children;
    children.push_back(heading("KEYBOARD REFERENCE"));
    children.push_back(Element{TextElement{.content = ""}});

    children.push_back(heading("Global"));
    children.push_back(row("Tab / S-Tab",  "cycle focus between sections"));
    children.push_back(row("Space",        "play / pause"));
    children.push_back(row("r",            "randomize current section (knobs OR pattern)"));
    children.push_back(row("R",            "randomize everything (knobs AND pattern)"));
    children.push_back(row("e",            "export pattern as WAV (4 loops)"));
    children.push_back(row("!..(  (S-1..9)","save pattern to slot 1..9"));
    children.push_back(row("1..9",         "load pattern from slot 1..9"));
    children.push_back(row("?",            "toggle this help"));
    children.push_back(row("q / Esc",      "quit"));
    children.push_back(Element{TextElement{.content = ""}});

    children.push_back(heading("Knobs"));
    children.push_back(row("\xe2\x86\x90 / \xe2\x86\x92",  "select knob"));
    children.push_back(row("\xe2\x86\x91 / \xe2\x86\x93",  "adjust by 5%"));
    children.push_back(row("[ / ]",        "coarse adjust by 10%"));
    children.push_back(row("0",            "reset to default"));
    children.push_back(row("w",            "toggle waveform (saw / square)"));
    children.push_back(Element{TextElement{.content = ""}});

    children.push_back(heading("Sequencer"));
    children.push_back(row("\xe2\x86\x90 / \xe2\x86\x92",  "select step"));
    children.push_back(row("\xe2\x86\x91 / \xe2\x86\x93",  "transpose semitone"));
    children.push_back(row("< / >",        "octave down / up"));
    children.push_back(row("a",            "toggle accent on step"));
    children.push_back(row("s",            "toggle slide into next step"));
    children.push_back(row("m",            "toggle mute/rest (note on / off)"));
    children.push_back(row("x",            "clear step (note + flags)"));
    children.push_back(row("c d e f g a b","set root pitch at current octave"));
    children.push_back(Element{TextElement{.content = ""}});

    children.push_back(heading("Transport"));
    children.push_back(row("\xe2\x86\x91 / \xe2\x86\x93",  "previous / next preset"));
    children.push_back(row(", / .",        "previous / next preset (alt)"));
    children.push_back(row("[ / ]",        "bpm -/+ 2"));
    children.push_back(row("{ / }",        "pattern length"));

    return dsl::vstack()
        .border(BorderStyle::Double)
        .border_color(clr_accent())
        .border_text(" HELP — press ? or Esc to close ", BorderTextPos::Top)
        .align_self(Align::Stretch)      // fill cross-axis (width) of parent
        .grow(1.0f)                      // fill remaining vertical space
        .padding(1, 3, 1, 3)(std::move(children));
}

} // namespace tb303
