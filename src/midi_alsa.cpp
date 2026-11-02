// Linux MIDI backend — an ALSA sequencer client with one input and one output
// port (connect with aconnect or a DAW). Inbound events are decoded to bytes
// and handed to midi_core; outbound goes through midi_core's pump.

#include "audio.hpp"
#include "midi_core.hpp"

#include <alsa/asoundlib.h>

#include <atomic>
#include <thread>
#include <vector>

namespace {

snd_seq_t*        g_seq      = nullptr;
int               g_port_in  = -1;
int               g_port_out = -1;
std::thread       g_worker;
std::atomic<bool> g_run{false};

void send(uint8_t status, uint8_t d1, uint8_t d2, int /*len*/) {
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_source(&ev, g_port_out);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);
    const uint8_t ch = status & 0x0F;
    switch (status & 0xF0) {
        case 0x90: snd_seq_ev_set_noteon(&ev, ch, d1, d2); break;
        case 0x80: snd_seq_ev_set_noteoff(&ev, ch, d1, d2); break;
        case 0xB0: snd_seq_ev_set_controller(&ev, ch, d1, d2); break;
        case 0xF0:
            switch (status) {
                case 0xF8: ev.type = SND_SEQ_EVENT_CLOCK;    break;
                case 0xFA: ev.type = SND_SEQ_EVENT_START;    break;
                case 0xFB: ev.type = SND_SEQ_EVENT_CONTINUE; break;
                case 0xFC: ev.type = SND_SEQ_EVENT_STOP;     break;
                default: return;
            }
            break;
        default: return;
    }
    snd_seq_event_output_direct(g_seq, &ev);
}

void handle_input(const snd_seq_event_t* ev) {
    using acid::midi::on_message;
    switch (ev->type) {
        case SND_SEQ_EVENT_CLOCK:    on_message(0xF8, 0, 0); break;
        case SND_SEQ_EVENT_START:    on_message(0xFA, 0, 0); break;
        case SND_SEQ_EVENT_CONTINUE: on_message(0xFB, 0, 0); break;
        case SND_SEQ_EVENT_STOP:     on_message(0xFC, 0, 0); break;
        case SND_SEQ_EVENT_NOTEON:
            on_message(0x90 | (ev->data.note.channel & 0x0F), ev->data.note.note, ev->data.note.velocity);
            break;
        case SND_SEQ_EVENT_NOTEOFF:
            on_message(0x80 | (ev->data.note.channel & 0x0F), ev->data.note.note, 0);
            break;
        case SND_SEQ_EVENT_CONTROLLER:
            on_message(0xB0 | (ev->data.control.channel & 0x0F),
                       static_cast<uint8_t>(ev->data.control.param),
                       static_cast<uint8_t>(ev->data.control.value));
            break;
        default: break;
    }
}

void midi_thread() {
    int npfd = snd_seq_poll_descriptors_count(g_seq, POLLIN);
    if (npfd <= 0) npfd = 1;
    std::vector<struct pollfd> pfd(static_cast<size_t>(npfd));
    snd_seq_poll_descriptors(g_seq, pfd.data(), npfd, POLLIN);

    acid::midi::Pump pump;
    while (g_run.load(std::memory_order_relaxed)) {
        // 1 ms wakeups are plenty for 24 PPQN at 300 BPM (8.3 ms per pulse).
        if (poll(pfd.data(), static_cast<nfds_t>(npfd), 1) > 0) {
            snd_seq_event_t* ev = nullptr;
            while (snd_seq_event_input(g_seq, &ev) >= 0 && ev) handle_input(ev);
        }
        pump.run(send);
    }
    pump.shutdown(send);
}

}  // namespace

extern "C" {

void acid_midi_start(void) {
    if (g_seq) return;
    if (snd_seq_open(&g_seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
        g_seq = nullptr;
        return;
    }
    snd_seq_set_client_name(g_seq, "acidflow");
    g_port_in = snd_seq_create_simple_port(
        g_seq, "in", SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    g_port_out = snd_seq_create_simple_port(
        g_seq, "out", SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    if (g_port_in < 0 || g_port_out < 0) {
        snd_seq_close(g_seq);
        g_seq = nullptr;
        return;
    }
    snd_seq_nonblock(g_seq, 1);
    g_run.store(true, std::memory_order_relaxed);
    g_worker = std::thread(midi_thread);
}

void acid_midi_stop(void) {
    if (!g_seq) return;
    g_run.store(false, std::memory_order_relaxed);
    if (g_worker.joinable()) g_worker.join();
    snd_seq_close(g_seq);
    g_seq = nullptr;
    g_port_in = g_port_out = -1;
}

}  // extern "C"
