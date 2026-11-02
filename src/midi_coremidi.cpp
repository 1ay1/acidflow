// macOS MIDI backend — CoreMIDI virtual source + destination named
// "acidflow", so DAWs and other apps see it like a hardware port. Also
// listens on every existing physical source, so a plugged-in controller just
// works.

#include "audio.hpp"
#include "midi_core.hpp"

#include <CoreMIDI/CoreMIDI.h>
#include <mach/mach_time.h>

#include <atomic>
#include <thread>

namespace {

MIDIClientRef     g_client = 0;
MIDIPortRef       g_in_port = 0;
MIDIEndpointRef   g_src = 0, g_dst = 0;
std::thread       g_worker;
std::atomic<bool> g_run{false};
acid::midi::Parser g_parser;   // only touched from the CoreMIDI read thread

void read_proc(const MIDIPacketList* list, void*, void*) {
    const MIDIPacket* p = &list->packet[0];
    for (UInt32 i = 0; i < list->numPackets; ++i) {
        for (UInt16 b = 0; b < p->length; ++b) g_parser.feed(p->data[b]);
        p = MIDIPacketNext(p);
    }
}

void send(uint8_t status, uint8_t d1, uint8_t d2, int len) {
    Byte buf[64];
    auto* list = reinterpret_cast<MIDIPacketList*>(buf);
    MIDIPacket* pkt = MIDIPacketListInit(list);
    const Byte msg[3] = {status, d1, d2};
    pkt = MIDIPacketListAdd(list, sizeof(buf), pkt, mach_absolute_time(),
                            static_cast<ByteCount>(len), msg);
    if (pkt) MIDIReceived(g_src, list);
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
    if (g_client) return;
    if (MIDIClientCreate(CFSTR("acidflow"), nullptr, nullptr, &g_client) != noErr) return;
    MIDISourceCreate(g_client, CFSTR("acidflow"), &g_src);
    MIDIDestinationCreate(g_client, CFSTR("acidflow"), read_proc, nullptr, &g_dst);
    MIDIInputPortCreate(g_client, CFSTR("acidflow in"), read_proc, nullptr, &g_in_port);
    for (ItemCount i = 0, n = MIDIGetNumberOfSources(); i < n; ++i)
        MIDIPortConnectSource(g_in_port, MIDIGetSource(i), nullptr);
    g_run.store(true, std::memory_order_relaxed);
    g_worker = std::thread(worker);
}

void acid_midi_stop(void) {
    if (!g_client) return;
    g_run.store(false, std::memory_order_relaxed);
    if (g_worker.joinable()) g_worker.join();
    MIDIClientDispose(g_client);
    g_client = 0; g_in_port = 0; g_src = 0; g_dst = 0;
}

}  // extern "C"
