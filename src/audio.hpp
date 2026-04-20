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
void acid_set_drive(float v);
void acid_set_volume(float v);
void acid_set_tuning_semi(float v);
void acid_set_waveform(int v);

// Trigger a note. accent/slide are 0/1 bools. step_sec is the step duration
// used to size the portamento ramp. These are the legacy external-trigger
// path; live playback now uses the in-engine sequencer (acid_seq_*) so notes
// fire at sample-accurate boundaries rather than on UI ticks.
void acid_note_on(float freq, int accent, int slide, float step_sec);
void acid_note_off(void);

// ── Audio-thread sequencer ──────────────────────────────────────────────────
// The UI writes pattern + transport state into atomics; the audio thread
// schedules step boundaries on a sample clock and fires notes itself. This
// eliminates the ~33 ms timing jitter that a UI-driven scheduler has and makes
// live playback match the offline WAV bounce to the sample.
//
// `flags` is a bitmask: rest=1, accent=2, slide=4. `midi` is the pitch in
// MIDI note numbers. `swing` is 0.50 (straight) .. 0.75 (hard shuffle).
void  acid_seq_set_step(int idx, int midi, int flags);
void  acid_seq_set_pattern_length(int n);
void  acid_seq_set_bpm(float bpm);
void  acid_seq_set_swing(float s);
void  acid_seq_play(void);
void  acid_seq_stop(void);
int   acid_seq_current_step(void);   // -1 when stopped
float acid_seq_step_phase(void);     // 0..1 within current step, for UI fades

// Pull the most-recent `n` samples out of the scope ring (capped at the ring
// size, 4096). The UI polls this each render frame to draw the oscilloscope.
// Returns the number of samples actually written. If the engine hasn't run
// yet (or `n` is 0), writes nothing and returns 0.
int acid_scope_tail(float* dst, int n);

// Peak output level over the last ~100ms of audio — a decaying envelope the UI
// can use to drive level meters. 0..1+ (can briefly exceed 1 during tanh
// clipping peaks).
float acid_output_peak(void);

// Current effective filter cutoff in Hz, i.e. the smoothed CUTOFF knob after
// envelope and accent modulation have been applied. Used by the UI to draw a
// live bar on the filter-response chart — so the graph "screams" with the
// accent envelope rather than sitting still.
float acid_live_fc(void);

// Offline WAV bounce: render `loops` repetitions of the given pattern into
// `path` at the engine's sample rate, 16-bit PCM mono. Uses the same in-engine
// sequencer as live playback — swing and BPM apply identically — and runs
// synchronously so it can't drop frames. The UI should NOT be running
// `acid_start()` for the duration of the call (engine globals are shared).
// Returns 0 on success, -1 on file-open failure.
//
// The `notes` array carries pattern_length entries; each slot is (midi, flags)
// packed as one int per step where flags bits are {rest=1, accent=2, slide=4}.
int acid_render_wav(const char* path,
                    const int*  notes,       // pattern_length entries
                    int         pattern_length,
                    float       bpm,
                    float       swing,       // 0.50..0.75
                    int         loops);

#ifdef __cplusplus
}
#endif
