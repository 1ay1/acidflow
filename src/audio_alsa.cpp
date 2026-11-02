// Linux ALSA backend — drives `acid::render()` into an ALSA PCM.
//
// A worker thread loops "render one buffer → writei". ALSA blocks us inside
// snd_pcm_writei until the ring has room, which paces the thread to real time.
// On an xrun we recover and keep going: a click beats a silent synth.

#include "audio_backend.hpp"
#include "engine.hpp"

#include <alsa/asoundlib.h>

#include <atomic>
#include <thread>
#include <vector>

namespace {

snd_pcm_t*        g_pcm = nullptr;
std::thread       g_worker;
std::atomic<bool> g_run{false};

void audio_thread() {
    std::vector<float> buf(static_cast<size_t>(acid::kBufFrames) * acid::kChannels);
    while (g_run.load(std::memory_order_relaxed)) {
        acid::render_live(buf.data(), acid::kBufFrames);
        snd_pcm_sframes_t w = snd_pcm_writei(g_pcm, buf.data(), acid::kBufFrames);
        if (w < 0 && snd_pcm_recover(g_pcm, static_cast<int>(w), 1) < 0) break;
    }
}

}  // namespace

namespace acid::backend {

bool alsa_start() {
    if (g_pcm) return true;
    acid::set_sample_rate(acid::kDefaultSampleRate);
    if (snd_pcm_open(&g_pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        g_pcm = nullptr;
        return false;
    }
    // float32 stereo, ~23 ms ring. allow_resample lets ALSA convert if the
    // card doesn't run at our rate.
    if (snd_pcm_set_params(g_pcm, SND_PCM_FORMAT_FLOAT_LE,
                           SND_PCM_ACCESS_RW_INTERLEAVED, acid::kChannels,
                           acid::kDefaultSampleRate, 1, 23 * 1000) < 0) {
        snd_pcm_close(g_pcm);
        g_pcm = nullptr;
        return false;
    }
    g_run.store(true, std::memory_order_relaxed);
    g_worker = std::thread(audio_thread);
    return true;
}

void alsa_stop() {
    if (!g_pcm) return;
    g_run.store(false, std::memory_order_relaxed);
    if (g_worker.joinable()) g_worker.join();
    snd_pcm_drop(g_pcm);
    snd_pcm_close(g_pcm);
    g_pcm = nullptr;
}

}  // namespace acid::backend
