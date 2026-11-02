// Ableton Link: tempo and beat phase shared with other apps on the LAN.
// Built only with -DACIDFLOW_LINK=ON (Link is GPLv2+); link_stub.cpp is used
// otherwise.
//
// The audio backend calls acid_link_tick once per block before rendering. We
// read the session's beat at the block's output time and post it to the
// engine, which aligns its step clock. BPM changes from the UI go the other
// way through acid_link_set_tempo.

#include "audio.hpp"

#include <ableton/Link.hpp>

#include <atomic>
#include <chrono>
#include <memory>

namespace {
constexpr double kQuantum = 4.0;   // one bar

std::unique_ptr<ableton::Link> g_link;
std::atomic<float>              g_pending_tempo{0.0f};

ableton::Link& link() {
    if (!g_link) g_link = std::make_unique<ableton::Link>(120.0);
    return *g_link;
}
}  // namespace

extern "C" {

void acid_link_enable(int on) {
    link().enable(on != 0);
    if (on) link().enableStartStopSync(true);
}
int  acid_link_enabled(void) { return g_link && g_link->isEnabled() ? 1 : 0; }
int  acid_link_peers(void)   { return g_link ? static_cast<int>(g_link->numPeers()) : 0; }

void acid_link_set_tempo(float bpm) { g_pending_tempo.store(bpm, std::memory_order_relaxed); }

void acid_link_tick(int frames, int sample_rate) {
    if (!g_link || !g_link->isEnabled() || sample_rate <= 0) return;
    auto state = g_link->captureAudioSessionState();
    const auto now = g_link->clock().micros();
    if (const float t = g_pending_tempo.exchange(0.0f); t > 0.0f) {
        state.setTempo(t, now);
        g_link->commitAudioSessionState(state);
    }
    // The block we're about to render reaches the speaker one block later.
    const auto latency = std::chrono::microseconds(
        static_cast<long long>(1e6 * frames / sample_rate));
    const double beat = state.beatAtTime(now + latency, kQuantum);
    acid_seq_sync_beat(beat, static_cast<float>(state.tempo()));
}

}  // extern "C"
