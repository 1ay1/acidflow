// macOS MIDI backend — CoreMIDI virtual source + destination named
// "acidflow", so DAWs and other apps see it like a hardware port. Also
// listens on every existing physical source, so a plugged-in controller just
// works. The CoreMIDI calls live in coremidi_shim.c.

#include "audio.hpp"
#include "coremidi_shim.h"
#include "midi_core.hpp"

#include <atomic>
#include <chrono>
#include <thread>

namespace {

bool              g_open = false;
std::thread       g_worker;
std::atomic<bool> g_run{false};
acid::midi::Parser g_parser;   // only touched from the CoreMIDI read thread

void on_bytes(const unsigned char* data, int len) {
    for (int i = 0; i < len; ++i) g_parser.feed(data[i]);
}

void send(uint8_t status, uint8_t d1, uint8_t d2, int len) {
    const unsigned char msg[3] = {status, d1, d2};
    acid_cm_send(msg, len);
}

void worker() {
    acid::midi::Pump pump;
    while (g_run.load(std::memory_order_relaxed)) {
        pump.run(send);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    pump.shutdown(send);
}

}  // namespace

extern "C" {

void acid_midi_start(void) {
    if (g_open) return;
    if (!acid_cm_open(on_bytes)) return;
    g_open = true;
    g_run.store(true, std::memory_order_relaxed);
    g_worker = std::thread(worker);
}

void acid_midi_stop(void) {
    if (!g_open) return;
    g_run.store(false, std::memory_order_relaxed);
    if (g_worker.joinable()) g_worker.join();
    acid_cm_close();
    g_open = false;
}

}  // extern "C"
