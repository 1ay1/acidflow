// Device stubs for the UI tests: the app links against these instead of a
// real audio/MIDI backend.
#include "audio.hpp"

extern "C" {
void acid_start(void) {}
void acid_stop(void) {}
const char* acid_audio_driver(void) { return "test"; }
void acid_midi_start(void) {}
void acid_midi_stop(void) {}
}
