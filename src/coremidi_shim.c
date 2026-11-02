// CoreMIDI calls, kept in C so Apple's clang compiles them. gcc can't parse
// the blocks syntax in CoreMIDI's headers.

#include "coremidi_shim.h"

#include <CoreMIDI/CoreMIDI.h>
#include <mach/mach_time.h>

static MIDIClientRef   g_client = 0;
static MIDIPortRef     g_in_port = 0;
static MIDIEndpointRef g_src = 0, g_dst = 0;
static acid_cm_bytes_fn g_on_bytes = 0;

static void read_proc(const MIDIPacketList* list, void* a, void* b) {
    (void)a; (void)b;
    const MIDIPacket* p = &list->packet[0];
    for (UInt32 i = 0; i < list->numPackets; ++i) {
        if (g_on_bytes) g_on_bytes(p->data, p->length);
        p = MIDIPacketNext(p);
    }
}

int acid_cm_open(acid_cm_bytes_fn on_bytes) {
    if (g_client) return 1;
    g_on_bytes = on_bytes;
    if (MIDIClientCreate(CFSTR("acidflow"), 0, 0, &g_client) != noErr) return 0;
    MIDISourceCreate(g_client, CFSTR("acidflow"), &g_src);
    MIDIDestinationCreate(g_client, CFSTR("acidflow"), read_proc, 0, &g_dst);
    MIDIInputPortCreate(g_client, CFSTR("acidflow in"), read_proc, 0, &g_in_port);
    for (ItemCount i = 0, n = MIDIGetNumberOfSources(); i < n; ++i)
        MIDIPortConnectSource(g_in_port, MIDIGetSource(i), 0);
    return 1;
}

void acid_cm_send(const unsigned char* msg, int len) {
    if (!g_src) return;
    Byte buf[64];
    MIDIPacketList* list = (MIDIPacketList*)buf;
    MIDIPacket* pkt = MIDIPacketListInit(list);
    pkt = MIDIPacketListAdd(list, sizeof(buf), pkt, mach_absolute_time(),
                            (ByteCount)len, msg);
    if (pkt) MIDIReceived(g_src, list);
}

void acid_cm_close(void) {
    if (!g_client) return;
    MIDIClientDispose(g_client);
    g_client = 0; g_in_port = 0; g_src = 0; g_dst = 0;
    g_on_bytes = 0;
}
