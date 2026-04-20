#pragma once
// Full-screen-ish modal shown when user presses ?. Mouse-free and scannable.

#include <vector>

#include <maya/maya.hpp>

#include "theme.hpp"

namespace tb303 {

using namespace maya;

[[nodiscard]] inline Element build_help_overlay(int scroll_offset = 0) {
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
    children.push_back(row("M  (S-m)",     "evolve pattern — one small mutation per press"));
    children.push_back(row("T  (S-t)",     "cycle colour theme (classic / cyber / moss / ice / mono)"));
    children.push_back(row("e",            "export pattern as WAV (4 loops)"));
    children.push_back(row("E  (S-e)",     "export pattern as MIDI file (.mid)"));
    children.push_back(row("p",            "export pattern as shareable text (pattern.txt)"));
    children.push_back(row("P  (S-p)",     "import pattern from pattern.txt"));
    children.push_back(row("!..(  (S-1..9)","save pattern to slot 1..9"));
    children.push_back(row("1..9",         "load pattern from slot 1..9"));
    children.push_back(row("n",            "song mode (chain saved slots on wrap)"));
    children.push_back(row("k",            "jam mode (two-octave live piano)"));
    children.push_back(row("O  (S-o)",     "toggle MIDI out (notes + drums + clock)"));
    children.push_back(row("I  (S-i)",     "toggle MIDI sync (follow external clock)"));
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

    children.push_back(heading("FX Rack"));
    children.push_back(row("\xe2\x86\x90 / \xe2\x86\x92",  "select FX knob"));
    children.push_back(row("\xe2\x86\x91 / \xe2\x86\x93",  "adjust  (DLY TIME steps 1/16\xe2\x86\x92" "1/8d)"));
    children.push_back(row("0",            "reset FX knob"));
    children.push_back(Element{TextElement{.content = ""}});

    children.push_back(heading("Sequencer"));
    children.push_back(row("\xe2\x86\x90 / \xe2\x86\x92",  "select step"));
    children.push_back(row("\xe2\x86\x91 / \xe2\x86\x93",  "transpose semitone"));
    children.push_back(row("< / >",        "octave down / up"));
    children.push_back(row("a",            "toggle accent on step"));
    children.push_back(row("s",            "toggle slide into next step"));
    children.push_back(row("m",            "toggle mute/rest (note on / off)"));
    children.push_back(row("v",            "cycle probability (100/75/50/25)"));
    children.push_back(row("j",            "cycle ratchet (1/2/3/4 hits per step)"));
    children.push_back(row("F  (S-f)",     "p-lock cutoff (snapshot current knob)"));
    children.push_back(row("G  (S-g)",     "p-lock resonance"));
    children.push_back(row("H  (S-h)",     "p-lock env mod"));
    children.push_back(row("J  (S-j)",     "p-lock accent amt"));
    children.push_back(row("L  (S-l)",     "clear all p-locks on step"));
    children.push_back(row("x",            "clear step (note + flags)"));
    children.push_back(row("c d e f g a b","set root pitch at current octave"));
    children.push_back(Element{TextElement{.content = ""}});

    children.push_back(heading("Drums"));
    children.push_back(row("\xe2\x86\x90 / \xe2\x86\x92",  "select step"));
    children.push_back(row("\xe2\x86\x91 / \xe2\x86\x93",  "select voice (BD / SD / CH / OH / CL …)"));
    children.push_back(row("space / x",    "toggle hit on selected cell"));
    children.push_back(row("s",            "mute / unmute selected voice (shown as BDM)"));
    children.push_back(row("m",            "mute / unmute entire drum bus"));
    children.push_back(row("1..9, 0",      "quick-toggle step 1..10 on current voice"));
    children.push_back(row("[ / ]",        "drum bus master send"));
    children.push_back(row("c",            "clear current voice row"));
    children.push_back(row("C  (S-c)",     "clear entire kit"));
    children.push_back(Element{TextElement{.content = ""}});

    children.push_back(heading("Jam mode"));
    children.push_back(row("k",            "enter jam mode (Esc to leave)"));
    children.push_back(row("z s x d c v g b h n j m", "lower-octave piano (C..B)"));
    children.push_back(row("q 2 w 3 e r 5 t 6 y 7 u", "upper-octave piano"));
    children.push_back(row("\xe2\x86\x90 / \xe2\x86\x92",  "octave down / up"));
    children.push_back(row("'",            "toggle accent for next notes"));
    children.push_back(row(";",            "toggle slide for next notes"));
    children.push_back(Element{TextElement{.content = ""}});

    children.push_back(heading("Transport"));
    children.push_back(row("\xe2\x86\x91 / \xe2\x86\x93",  "previous / next preset"));
    children.push_back(row(", / .",        "previous / next preset (alt)"));
    children.push_back(row("[ / ]",        "bpm -/+ 2"));
    children.push_back(row("{ / }",        "pattern length"));
    children.push_back(row("- / =",        "swing (50% straight \xe2\x86\x92 75% hard shuffle)"));
    children.push_back(Element{TextElement{.content = ""}});

    children.push_back(heading("Mouse"));
    children.push_back(row("click knob",   "focus + select"));
    children.push_back(row("drag knob",    "adjust (vertical motion)"));
    children.push_back(row("scroll knob",  "adjust by 5%"));
    children.push_back(row("right-click",  "reset knob to default"));
    children.push_back(row("click step",   "select step to edit"));
    children.push_back(row("scroll step",  "transpose by semitone"));
    children.push_back(row("right-click",  "toggle rest on step"));
    children.push_back(row("click title",  "play / pause"));
    children.push_back(row("scroll title", "nudge bpm"));
    children.push_back(row("scroll right", "browse presets"));

    // Apply vertical scroll by dropping N children from the top. The parent
    // passes the offset in; hit-test code clamps it when the wheel fires.
    if (scroll_offset > 0) {
        int skip = std::min<int>(scroll_offset, static_cast<int>(children.size()));
        children.erase(children.begin(), children.begin() + skip);
    }

    return dsl::vstack()
        .border(BorderStyle::Double)
        .border_color(clr_accent())
        .border_text(" HELP — press ? or Esc to close (scroll to read more) ",
                     BorderTextPos::Top)
        .align_self(Align::Stretch)      // fill cross-axis (width) of parent
        .grow(1.0f)                      // fill remaining vertical space
        .overflow(Overflow::Hidden)      // let narrow windows clip cleanly
        .padding(1, 3, 1, 3)(std::move(children));
}

} // namespace tb303
