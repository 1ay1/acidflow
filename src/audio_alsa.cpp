// Linux audio backend — drives `acid::render()` into ALSA PCM.
//
// ALSA's "blocking writei" model is the simplest thing that gives tight
// latency: we open the default playback device with a period of kBufFrames,
// then a worker thread loops "render one buffer → write it". The OS blocks
// us inside snd_pcm_writei until the ring buffer has space, which naturally
// paces the thread to real-time with no extra scheduling.
//
// On an xrun (underflow from a scheduler hiccup or the main thread hogging
// the CPU) we transparently `prepare` the device and keep going rather than
// bailing out — a momentary click is far better than a silent synth.

#include "audio.hpp"
#include "engine.hpp"

#include <alsa/asoundlib.h>

#include <atomic>
#include <thread>
#include <vector>

namespace {

snd_pcm_t*       g_pcm    = nullptr;
std::thread      g_worker;
std::atomic<bool> g_run   {false};

void audio_thread() {
    std::vector<float> buf(acid::kBufFrames);
    while (g_run.load(std::memory_order_relaxed)) {
        acid::render(buf.data(), acid::kBufFrames);
        snd_pcm_sframes_t w = snd_pcm_writei(g_pcm, buf.data(), acid::kBufFrames);
        if (w < 0) {
            // -EPIPE is an xrun (buffer underflow). Recover and carry on;
            // other errors usually mean the device went away, in which case
            // there's nothing useful we can do from the audio thread.
            int err = snd_pcm_recover(g_pcm, static_cast<int>(w), 1);
            if (err < 0) break;
        }
    }
}

}  // namespace

extern "C" {

void acid_start() {
    if (g_pcm) return;

    acid::set_sample_rate(acid::kDefaultSampleRate);

    if (snd_pcm_open(&g_pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        g_pcm = nullptr;
        return;
    }

    // One-shot "easy" config: float32 mono, ~23 ms ring buffer so a couple
    // of scheduler hiccups worth of jitter are absorbed before we glitch
    // audibly. `allow_resample=1` lets ALSA resample transparently if the
    // card doesn't natively support our rate.
    if (snd_pcm_set_params(g_pcm,
                           SND_PCM_FORMAT_FLOAT_LE,
                           SND_PCM_ACCESS_RW_INTERLEAVED,
                           1,                              // channels
                           acid::kDefaultSampleRate,
                           1,                              // allow resampling
                           23 * 1000 /* µs — target latency */) < 0) {
        snd_pcm_close(g_pcm);
        g_pcm = nullptr;
        return;
    }

    g_run.store(true, std::memory_order_relaxed);
    g_worker = std::thread(audio_thread);
}

void acid_stop() {
    if (!g_pcm) return;
    g_run.store(false, std::memory_order_relaxed);
    if (g_worker.joinable()) g_worker.join();
    snd_pcm_drop(g_pcm);
    snd_pcm_close(g_pcm);
    g_pcm = nullptr;
}

}  // extern "C"
