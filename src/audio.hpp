// Thin C-style interface to the acid-synth. Implementation lives in audio.cpp
// (compiled separately with Apple Clang) because the macOS AudioQueue headers
// drag in CoreMIDI / mach bits that GCC can't parse. This header exposes only
// plain scalar types so it links cleanly across compilers.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void acid_start(void);
void acid_stop(void);

// All knobs take normalised 0..1 float, except set_waveform (0=saw,1=square)
// and set_tuning_semi which is ±12 semitones.
void acid_set_cutoff(float v);
void acid_set_resonance(float v);
void acid_set_env_mod(float v);
void acid_set_decay(float v);
void acid_set_accent_amt(float v);
void acid_set_volume(float v);
void acid_set_tuning_semi(float v);
void acid_set_waveform(int v);

// Trigger a note. accent/slide are 0/1 bools. step_sec is the step duration
// used to size the portamento ramp.
void acid_note_on(float freq, int accent, int slide, float step_sec);
void acid_note_off(void);

// Pull the most-recent `n` samples out of the scope ring (capped at the ring
// size, 4096). The UI polls this each render frame to draw the oscilloscope.
// Returns the number of samples actually written. If the engine hasn't run
// yet (or `n` is 0), writes nothing and returns 0.
int acid_scope_tail(float* dst, int n);

// Peak output level over the last ~100ms of audio — a decaying envelope the UI
// can use to drive level meters. 0..1+ (can briefly exceed 1 during tanh
// clipping peaks).
float acid_output_peak(void);

// Offline WAV bounce: render `pattern_length * step_sec * loops` seconds of
// audio into `path` at 44.1kHz, 16-bit PCM mono, with the sequencer driving
// notes as per the passed pattern. Uses the same DSP as live playback but
// runs synchronously so it can't drop frames. The UI should NOT be running
// `acid_start()` for the duration of the call (engine globals are shared).
// Returns 0 on success, -1 on file-open failure.
//
// The `notes` array carries 16 entries; each slot is (midi, flags) packed as
// one int per step where flags are bits {rest=1, accent=2, slide=4}.
int acid_render_wav(const char* path,
                    const int*  notes,       // pattern_length entries
                    int         pattern_length,
                    float       step_sec,
                    int         loops);

#ifdef __cplusplus
}
#endif
