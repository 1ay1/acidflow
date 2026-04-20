#pragma once
// Shared step-sequencer data types. Used by both the sequencer view and the
// transport panel (the preset list renders a mini pattern contour per row).

#include <array>
#include <cstdint>

namespace tb303 {

// note names for semitones 0-11 (chromatic within an octave, 0 = C)
inline constexpr std::array<const char*, 12> NOTE_NAMES = {
    "C ", "C#", "D ", "D#", "E ", "F ",
    "F#", "G ", "G#", "A ", "A#", "B "
};

// Per-step parameter-lock flags. Each bit, when set, means "this step
// overrides the global knob for that parameter while it's playing". Values
// live in `lock_{cutoff,res,env,accent}` below (0..1, same mapping as the
// knobs). Inspired by Elektron-style p-locks — a characteristic trick for
// making one-bar acid lines feel composed rather than looped.
inline constexpr uint8_t LOCK_CUTOFF = 1 << 0;
inline constexpr uint8_t LOCK_RES    = 1 << 1;
inline constexpr uint8_t LOCK_ENV    = 1 << 2;
inline constexpr uint8_t LOCK_ACCENT = 1 << 3;

struct StepData {
    int     note        = 36;       // MIDI note (C2 = 36)
    bool    rest        = true;
    bool    accent      = false;
    bool    slide       = false;
    int     prob        = 100;      // 0..100, 100 = always plays (default)
    int     ratchet     = 1;        // 1..4 triggers per step (1 = classic single hit)
    uint8_t lock_mask   = 0;
    float   lock_cutoff = 0.0f;
    float   lock_res    = 0.0f;
    float   lock_env    = 0.0f;
    float   lock_accent = 0.0f;
};

} // namespace tb303
