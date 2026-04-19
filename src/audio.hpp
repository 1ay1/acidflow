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

#ifdef __cplusplus
}
#endif
