// Platform-agnostic TB-303-style synth DSP.
//
// The engine holds all shared state (parameters, note command queue, filter
// state) and exposes three things:
//
//   * `set_sample_rate(sr)` — called by the backend once before audio starts,
//     so the filter/envelope maths can scale to whatever rate the OS gave us.
//     ALSA and CoreAudio will resample for us, so they just pass 44100;
//     WASAPI in shared mode forces the device's mix rate (usually 48000).
//   * `render(out, frames)` — called on the audio thread to fill a mono
//     float32 output buffer at the current sample rate.
//   * the `acid_*` C functions (declared in audio.hpp) — the UI-facing façade.
//     Parameter and note-trigger calls are defined in engine.cpp; only
//     `acid_start` / `acid_stop` live in the platform backend.
#pragma once

namespace acid {

inline constexpr int kDefaultSampleRate = 44100;
inline constexpr int kBufFrames         = 256;   // ~5–6 ms per buffer

void set_sample_rate(int sr);
int  sample_rate();

void render(float* out, int frames);

}  // namespace acid
