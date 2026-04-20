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

// Tempo-synced delay (master bus). Division: 0=1/16, 1=1/16d, 2=1/8, 3=1/8d.
// Time is derived from the sequencer's BPM so repeats always lock to the beat.
// mix=0 bypasses the wet path; feedback is clamped < 1 to stay stable.
void acid_set_delay_mix(float v);
void acid_set_delay_feedback(float v);
void acid_set_delay_division(int v);

// Pre-filter overdrive (soft tanh clip between osc and ladder input).
void acid_set_od_amt(float v);

// Plate reverb on the master bus. size scales decay time, damp rolls off HF
// inside the feedback path so longer tails darken like a real plate.
void acid_set_rev_mix(float v);
void acid_set_rev_size(float v);
void acid_set_rev_damp(float v);

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
// MIDI note numbers. `prob` is 0..100 — probability (percent) the step will
// fire. `ratchet` is 1..4 equal sub-triggers per step (1 = classic single
// hit). `swing` is 0.50 (straight) .. 0.75 (hard shuffle).
void  acid_seq_set_step(int idx, int midi, int flags, int prob, int ratchet);

// Per-step parameter locks. `mask` bits: cutoff=1, res=2, env=4, accent=8.
// When a bit is set, `cutoff_v / res_v / env_v / accent_v` override the
// global knob for the duration of that step. All values are normalised 0..1
// (same mapping the global knob atomics use).
void  acid_seq_set_step_locks(int idx, int mask,
                              float cutoff_v, float res_v,
                              float env_v,    float accent_v);
void  acid_seq_set_pattern_length(int n);
void  acid_seq_set_bpm(float bpm);
void  acid_seq_set_swing(float s);
void  acid_seq_play(void);
void  acid_seq_stop(void);

// Drum machine (Phase 4.3). Five synthesized voices run alongside the bass:
//   voice 0 = BD (kick)    voice 3 = OH (open hat)
//   voice 1 = SD (snare)   voice 4 = CL (clap)
//   voice 2 = CH (closed hat)
// Each voice has a 16-bit step mask (bit i = hit on step i) and a per-voice
// gain. A single master knob scales the drum bus before the FX chain; 0 = mute.
void  acid_seq_set_drum_lane(int voice, int mask_16bit);
void  acid_seq_set_drum_gain(int voice, float v);
void  acid_seq_set_drum_master(float v);
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

// Runtime sample rate the engine is operating at. Needed by the UI to map FFT
// bin indices to absolute frequencies (bin i = i * sr / 2048).
int acid_sample_rate(void);

// Snapshot magnitude spectrum of the last 2048 output samples, windowed with
// Hann and FFT'd. Writes up to `n_bins` bins (max 1024) linearly spaced from
// 0 Hz to sr/2 into `out_mags`. Returns bins written. Intended for UI-thread
// use at render rate (~30 Hz) — safe to call cheaply every frame.
int acid_fft_bins(float* out_mags, int n_bins);

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

// ── MIDI bridge (Phase 5) ───────────────────────────────────────────────────
// Start/stop the platform's MIDI backend. On Linux this creates an ALSA
// sequencer client with one input + one output port (connect with aconnect or
// your DAW). No-op on platforms without a backend — UI toggles still work,
// the synth just doesn't forward anything.
void  acid_midi_start(void);
void  acid_midi_stop(void);

// Toggle MIDI output (note events + drum hits on ch 10 + clock when playing).
// When off, the engine's audio thread skips pushing into the ring entirely so
// there's zero overhead for users who never touch MIDI.
void  acid_midi_set_out_enabled(int on);
int   acid_midi_out_enabled(void);
// Toggle MIDI input sync — clock / start / stop messages drive the sequencer.
// Note-on/off messages always trigger the bass voice when the backend is up;
// this switch only affects tempo/transport slaving.
void  acid_midi_set_sync_in(int on);
int   acid_midi_sync_in(void);

// Internal hooks for the MIDI backend thread (do not call from UI).
void  acid_midi_feed_clock(double now_seconds);
void  acid_midi_feed_start_stop(int start);
int   acid_midi_consume_events(unsigned char* out, int max_evts);
void  acid_midi_note_on_ext(int midi_note, int velocity);
void  acid_midi_note_off_ext(int midi_note);

// Engine-state accessors the MIDI backend needs for clock-out pacing.
float acid_current_bpm(void);
int   acid_is_playing(void);

#ifdef __cplusplus
}
#endif
