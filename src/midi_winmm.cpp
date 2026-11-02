// Windows MIDI backend — WinMM. Opens every MIDI input device (controllers,
// loopMIDI ports) and the first output device. Windows has no virtual ports
// of its own; use loopMIDI to patch into a DAW.

#include "audio.hpp"
#include "midi_core.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include <atomic>
#include <thread>
#include <vector>

namespace {

std::vector<HMIDIIN> g_ins;
HMIDIOUT             g_out = nullptr;
std::thread          g_worker;
std::atomic<bool>    g_run{false};
bool                 g_started = false;

void CALLBACK in_proc(HMIDIIN, UINT msg, DWORD_PTR, DWORD_PTR p1, DWORD_PTR) {
    if (msg != MIM_DATA) return;
    const auto w = static_cast<DWORD>(p1);
    acid::midi::on_message(static_cast<uint8_t>(w & 0xFF),
                           static_cast<uint8_t>((w >> 8) & 0x7F),
                           static_cast<uint8_t>((w >> 16) & 0x7F));
}

void send(uint8_t status, uint8_t d1, uint8_t d2, int /*len*/) {
    if (g_out) midiOutShortMsg(g_out, static_cast<DWORD>(status | (d1 << 8) | (d2 << 16)));
}

void worker() {
    timeBeginPeriod(1);                 // 1 ms sleeps for clock pacing
    acid::midi::Pump pump;
    while (g_run.load(std::memory_order_relaxed)) {
        pump.run(send);
        Sleep(1);
    }
    pump.shutdown(send);
    timeEndPeriod(1);
}

}  // namespace

extern "C" {

void acid_midi_start(void) {
    if (g_started) return;
    g_started = true;
    for (UINT i = 0, n = midiInGetNumDevs(); i < n; ++i) {
        HMIDIIN h = nullptr;
        if (midiInOpen(&h, i, reinterpret_cast<DWORD_PTR>(in_proc), 0, CALLBACK_FUNCTION) == MMSYSERR_NOERROR) {
            midiInStart(h);
            g_ins.push_back(h);
        }
    }
    if (midiOutGetNumDevs() > 0 && midiOutOpen(&g_out, 0, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
        g_out = nullptr;
    g_run.store(true, std::memory_order_relaxed);
    g_worker = std::thread(worker);
}

void acid_midi_stop(void) {
    if (!g_started) return;
    g_run.store(false, std::memory_order_relaxed);
    if (g_worker.joinable()) g_worker.join();
    for (HMIDIIN h : g_ins) { midiInStop(h); midiInClose(h); }
    g_ins.clear();
    if (g_out) { midiOutReset(g_out); midiOutClose(g_out); g_out = nullptr; }
    g_started = false;
}

}  // extern "C"
