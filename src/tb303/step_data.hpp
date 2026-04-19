#pragma once
// Shared step-sequencer data types. Used by both the sequencer view and the
// transport panel (the preset list renders a mini pattern contour per row).

#include <array>

namespace tb303 {

// note names for semitones 0-11 (chromatic within an octave, 0 = C)
inline constexpr std::array<const char*, 12> NOTE_NAMES = {
    "C ", "C#", "D ", "D#", "E ", "F ",
    "F#", "G ", "G#", "A ", "A#", "B "
};

struct StepData {
    int  note    = 36;       // MIDI note (C2 = 36)
    bool rest    = true;
    bool accent  = false;
    bool slide   = false;
};

} // namespace tb303
