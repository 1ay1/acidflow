// Linux MIDI backend — exposes acidflow as an ALSA sequencer client so other
// tools (DAWs, aconnect, standalone synths) can talk MIDI with it.
//
// Design: one worker thread that does two jobs:
//   1. poll() the input port for incoming events (clock, start/stop, notes)
//      and feed them back into the engine via the audio.hpp hooks.
//   2. between polls, drain the engine's out-event ring and forward any
//      note-on/off events; emit 24 PPQN clock pulses self-timed from
//      acid_current_bpm() when MIDI out is enabled and the sequencer is
//      playing.
//
// The ALSA sequencer gives us microsecond-precision scheduling for our own
// queue, but we don't need it — we send events immediately as the audio
// thread pushes them. Input uses a short poll timeout so clock-out pacing
// still ticks on schedule.

#include "audio.hpp"

#include <alsa/asoundlib.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

namespace {

snd_seq_t*         g_seq        = nullptr;
int                g_port_in    = -1;
int                g_port_out   = -1;
std::thread        g_worker;
std::atomic<bool>  g_run        {false};

inline double monotonic_seconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

inline void send_short(uint8_t status, uint8_t d1, uint8_t d2) {
    // snd_seq_ev_set_* macros fill a prepared event struct. We route to our
    // output port and ask ALSA to deliver directly (subscribers pick it up).
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_source(&ev, g_port_out);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);

    uint8_t type    = status & 0xF0;
    uint8_t channel = status & 0x0F;
    switch (type) {
        case 0x90: snd_seq_ev_set_noteon (&ev, channel, d1, d2); break;
        case 0x80: snd_seq_ev_set_noteoff(&ev, channel, d1, d2); break;
        default:   return;  // short-message sender only handles note on/off
    }
    snd_seq_event_output_direct(g_seq, &ev);
}

inline void send_realtime(uint8_t status) {
    // Clock / start / continue / stop live outside the channel-voice space;
    // ALSA has dedicated event types for them so we don't go through a raw
    // byte-stream encoder.
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_source(&ev, g_port_out);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);
    switch (status) {
        case 0xF8: ev.type = SND_SEQ_EVENT_CLOCK;    break;
        case 0xFA: ev.type = SND_SEQ_EVENT_START;    break;
        case 0xFB: ev.type = SND_SEQ_EVENT_CONTINUE; break;
        case 0xFC: ev.type = SND_SEQ_EVENT_STOP;     break;
        default:   return;
    }
    snd_seq_event_output_direct(g_seq, &ev);
}

// Drain the engine's outgoing event ring and push each as a short MIDI msg.
void forward_engine_events() {
    unsigned char buf[64 * 4];
    int evts = acid_midi_consume_events(buf, 64);
    for (int i = 0; i < evts; ++i) {
        uint8_t type = buf[i * 4 + 0];
        uint8_t ch   = buf[i * 4 + 1] & 0x0F;
        uint8_t d1   = buf[i * 4 + 2];
        uint8_t d2   = buf[i * 4 + 3];
        if (type == 1)      send_short(0x90 | ch, d1, d2);
        else if (type == 2) send_short(0x80 | ch, d1, d2);
    }
}

// Handle one inbound MIDI event. Routes clock / transport into the engine's
// sync path, and forwards note on/off straight to the bass voice so a USB
// controller acts like jam-mode piano.
void handle_input(const snd_seq_event_t* ev) {
    switch (ev->type) {
        case SND_SEQ_EVENT_CLOCK:
            acid_midi_feed_clock(monotonic_seconds());
            break;
        case SND_SEQ_EVENT_START:
        case SND_SEQ_EVENT_CONTINUE:
            acid_midi_feed_start_stop(1);
            break;
        case SND_SEQ_EVENT_STOP:
            acid_midi_feed_start_stop(0);
            break;
        case SND_SEQ_EVENT_NOTEON:
            // velocity 0 is the common "note off" encoding.
            if (ev->data.note.velocity == 0) {
                acid_midi_note_off_ext(ev->data.note.note);
            } else {
                acid_midi_note_on_ext(ev->data.note.note,
                                      ev->data.note.velocity);
            }
            break;
        case SND_SEQ_EVENT_NOTEOFF:
            acid_midi_note_off_ext(ev->data.note.note);
            break;
        default:
            break;
    }
}

void midi_thread() {
    // poll file descriptors for the input side — lets us block for ~1ms at a
    // time without burning CPU when nothing's arriving, while still waking up
    // often enough to pace clock-out at the tightest tempo (24 PPQN × 300 BPM
    // ≈ 120 pulses/s → 8.3 ms between pulses, so 1 ms wake granularity is fine).
    int npfd = snd_seq_poll_descriptors_count(g_seq, POLLIN);
    if (npfd <= 0) npfd = 1;
    std::vector<struct pollfd> pfd(static_cast<size_t>(npfd));
    snd_seq_poll_descriptors(g_seq, pfd.data(), npfd, POLLIN);

    // Clock-out scheduler. We derive "next pulse time" from g_seq_bpm each
    // pulse so tempo changes during playback don't drift.
    double next_clk_t = monotonic_seconds();
    bool   last_playing = false;

    while (g_run.load(std::memory_order_relaxed)) {
        // ── Incoming ────────────────────────────────────────────────────────
        if (poll(pfd.data(), static_cast<nfds_t>(npfd), 1) > 0) {
            snd_seq_event_t* ev = nullptr;
            while (snd_seq_event_input(g_seq, &ev) >= 0 && ev) {
                handle_input(ev);
                // ALSA reuses its internal buffer — no free needed for direct
                // events (the library owns the pointer).
            }
        }

        // ── Outgoing notes ──────────────────────────────────────────────────
        forward_engine_events();

        // ── Clock out ───────────────────────────────────────────────────────
        // Only emit when the user has enabled MIDI out. Track the engine's
        // play/stop edges so we send 0xFA / 0xFC at the boundary too.
        bool out_on = acid_midi_out_enabled() != 0;
        bool playing = acid_is_playing() != 0;
        if (out_on) {
            if (playing && !last_playing) {
                send_realtime(0xFA);
                next_clk_t = monotonic_seconds();
            } else if (!playing && last_playing) {
                send_realtime(0xFC);
            }
            if (playing) {
                double now = monotonic_seconds();
                float  bpm = acid_current_bpm();
                if (bpm < 20.0f) bpm = 20.0f;
                double pulse_dt = 60.0 / (static_cast<double>(bpm) * 24.0);
                // Burst-catch-up: if we were late (e.g. scheduler hiccup), emit
                // up to 4 pulses in one pass rather than trying to squeeze
                // them into sub-ms back-to-back — real masters drop late
                // pulses anyway and we'd rather stay aligned to the next tick.
                int caught = 0;
                while (now >= next_clk_t && caught < 4) {
                    send_realtime(0xF8);
                    next_clk_t += pulse_dt;
                    ++caught;
                }
                // If we fell way behind, snap forward so we don't spam.
                if (now - next_clk_t > 0.05) next_clk_t = now + pulse_dt;
            }
        }
        last_playing = playing;
    }

    // On shutdown: be polite and send stop + all-notes-off on bass/drum
    // channels so external gear doesn't hang on a note we fired.
    if (acid_midi_out_enabled()) {
        send_realtime(0xFC);
        // CC 123 (all notes off) on channels 1 + 10.
        snd_seq_event_t ev;
        for (int ch : { 0, 9 }) {
            snd_seq_ev_clear(&ev);
            snd_seq_ev_set_source(&ev, g_port_out);
            snd_seq_ev_set_subs(&ev);
            snd_seq_ev_set_direct(&ev);
            snd_seq_ev_set_controller(&ev, ch, 123, 0);
            snd_seq_event_output_direct(g_seq, &ev);
        }
    }
}

} // namespace

extern "C" {

void acid_midi_start(void) {
    if (g_seq) return;
    if (snd_seq_open(&g_seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
        g_seq = nullptr;
        return;
    }
    snd_seq_set_client_name(g_seq, "acidflow");

    g_port_in  = snd_seq_create_simple_port(
        g_seq, "in",
        SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
        SND_SEQ_PORT_TYPE_APPLICATION | SND_SEQ_PORT_TYPE_MIDI_GENERIC);
    g_port_out = snd_seq_create_simple_port(
        g_seq, "out",
        SND_SEQ_PORT_CAP_READ  | SND_SEQ_PORT_CAP_SUBS_READ,
        SND_SEQ_PORT_TYPE_APPLICATION | SND_SEQ_PORT_TYPE_MIDI_GENERIC);

    if (g_port_in < 0 || g_port_out < 0) {
        snd_seq_close(g_seq);
        g_seq = nullptr;
        return;
    }

    g_run.store(true, std::memory_order_relaxed);
    g_worker = std::thread(midi_thread);
}

void acid_midi_stop(void) {
    if (!g_seq) return;
    g_run.store(false, std::memory_order_relaxed);
    if (g_worker.joinable()) g_worker.join();
    snd_seq_close(g_seq);
    g_seq      = nullptr;
    g_port_in  = -1;
    g_port_out = -1;
}

} // extern "C"
