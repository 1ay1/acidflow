#pragma once
// Every user action in one table. The key handler, the command palette, the
// help screen and the hint bar all read from it, so a shortcut can't be
// documented in one place and do something else in another.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "tb303/section.hpp"

struct App;

enum class Scope : uint8_t {
    Global,      // works from anywhere
    Knobs, FX, Sequencer, Drums, Transport,
};

inline const char* scope_name(Scope s) {
    switch (s) {
        case Scope::Global:    return "Everywhere";
        case Scope::Knobs:     return "Synth voice";
        case Scope::FX:        return "FX rack";
        case Scope::Sequencer: return "Sequencer";
        case Scope::Drums:     return "Drums";
        case Scope::Transport: return "Transport";
    }
    return "";
}

inline Scope scope_of(tb303::Section s) {
    switch (s) {
        case tb303::Section::Knobs:     return Scope::Knobs;
        case tb303::Section::FX:        return Scope::FX;
        case tb303::Section::Sequencer: return Scope::Sequencer;
        case tb303::Section::Drums:     return Scope::Drums;
        case tb303::Section::Transport: return Scope::Transport;
    }
    return Scope::Global;
}

struct Action {
    const char* id;        // stable name (tests, palette history)
    Scope       scope;
    const char* keys;      // how it's shown: "Space", "r", "←→", "u / U"
    const char* name;      // short verb phrase: "Play / stop"
    const char* help;      // one plain sentence for help and the palette
    // Hint-bar priority within its scope: 1 = always shown, 2 = if room,
    // 0 = never in the bar (still in help and the palette).
    int         hint = 0;
    // What the palette runs. Null for actions that only make sense as keys
    // (arrows, typing notes): those are documented but not listed to run.
    void (*run)(App&) = nullptr;
    // Also offer this in the palette while another section has focus; it
    // switches focus first. Off for edits that need a cursor position.
    bool palette_anywhere = true;
};

const std::vector<Action>& actions();

// The actions that apply right now: the focused section's, then global.
std::vector<const Action*> actions_for(Scope focused);

// Simple fuzzy score: every query character must appear in order in
// name+help+keys (case-insensitive). Higher is better; -1 = no match.
int fuzzy_score(std::string_view query, const Action& a);
