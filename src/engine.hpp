// Platform-agnostic TB-303-style synth DSP.
//
// The engine is a self-contained object (acid::Engine). The process has one
// default instance that the audio backends render and the `acid_*` C API in
// audio.hpp drives. Offline renders, tests and the plugin build make their
// own Engine and point the C API at it for the current thread with
// Engine::Bind, so they never touch the live instance.
//
// Output is interleaved stereo float32 (L, R, L, R, ...), `frames` frames.
#pragma once

namespace acid {

inline constexpr int kDefaultSampleRate = 44100;
inline constexpr int kBufFrames         = 256;   // ~5–6 ms per buffer
inline constexpr int kChannels          = 2;

// The default (live) instance.
void set_sample_rate(int sr);
int  sample_rate();
void render(float* out, int frames);

// The CUTOFF knob's base frequency in Hz (before envelope and accent).
float cutoff_knob_hz(float knob01);

struct Core;

class Engine {
  public:
    explicit Engine(int sample_rate = kDefaultSampleRate);
    ~Engine();
    Engine(const Engine&)            = delete;
    Engine& operator=(const Engine&) = delete;

    void render(float* out, int frames);
    int  sample_rate() const;

    // While alive, acid_* calls on this thread go to `e`.
    class Bind {
      public:
        explicit Bind(Engine& e);
        ~Bind();
        Bind(const Bind&)            = delete;
        Bind& operator=(const Bind&) = delete;
      private:
        Core* prev_;
    };

  private:
    Core* core_;
};

}  // namespace acid
