// Placeholder MIDI backend for platforms we haven't written a real one for
// yet (macOS / Windows). acid_midi_start is a no-op so the UI toggles still
// set engine state flags — the audio path just doesn't forward anything.
// When we ship CoreMIDI / WinMM backends they'll drop in here by build flag.

#include "audio.hpp"

extern "C" {

void acid_midi_start(void) {}
void acid_midi_stop(void)  {}

} // extern "C"
