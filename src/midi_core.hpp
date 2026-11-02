#pragma once
// Platform-neutral MIDI logic shared by the ALSA, CoreMIDI and WinMM
// backends. A backend only moves bytes: it feeds every inbound message to
// midi::on_message and, on its worker thread, calls midi::pump with a function
// that sends one outbound short message.
//
// CC learn lives here too: the UI arms learn for a parameter id, the next CC
// that arrives binds to it, and later values for that CC land in a lock-free
// table the UI applies on its tick (so knob state stays owned by the UI).

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>

#include "audio.hpp"

namespace acid::midi {

inline double now_s() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// ── CC map ──────────────────────────────────────────────────────────────────
// param ids are the UI's (0..kMaxParams-1). Each CC (0..119) maps to at most
// one param; `value` holds the latest 0..127 and `seq` bumps on each change.
inline constexpr int kMaxParams = 32;
inline constexpr int kNumCC     = 120;

struct CCState {
    std::array<std::atomic<int8_t>, kNumCC>   cc_to_param{};   // -1 = unbound
    std::array<std::atomic<uint8_t>, kMaxParams> value{};
    std::array<std::atomic<uint32_t>, kMaxParams> seq{};
    std::atomic<int> learn_param{-1};      // armed param, or -1
    std::atomic<int> last_learned_cc{-1};
    CCState() { for (auto& c : cc_to_param) c.store(-1); }
};

inline CCState& cc() {
    static CCState s;
    return s;
}

// Handle one complete inbound message (status + up to 2 data bytes).
inline void on_message(uint8_t status, uint8_t d1, uint8_t d2) {
    switch (status) {
        case 0xF8: acid_midi_feed_clock(now_s()); return;
        case 0xFA:
        case 0xFB: acid_midi_feed_start_stop(1); return;
        case 0xFC: acid_midi_feed_start_stop(0); return;
        default: break;
    }
    const uint8_t type = status & 0xF0;
    if (type == 0x90 && d2 > 0) { acid_midi_note_on_ext(d1, d2); return; }
    if (type == 0x80 || (type == 0x90 && d2 == 0)) { acid_midi_note_off_ext(d1); return; }
    if (type == 0xB0 && d1 < kNumCC) {
        auto& s = cc();
        const int armed = s.learn_param.exchange(-1);
        if (armed >= 0 && armed < kMaxParams) {
            // One CC per param: clear any old binding for this param first.
            for (auto& m : s.cc_to_param)
                if (m.load() == armed) m.store(-1);
            s.cc_to_param[d1].store(static_cast<int8_t>(armed));
            s.last_learned_cc.store(d1);
        }
        const int p = s.cc_to_param[d1].load();
        if (p >= 0) {
            s.value[static_cast<size_t>(p)].store(d2, std::memory_order_relaxed);
            s.seq[static_cast<size_t>(p)].fetch_add(1, std::memory_order_release);
        }
    }
}

// Running-status byte stream parser, for backends that hand us raw bytes.
struct Parser {
    uint8_t status = 0, d[2] = {}, need = 0, have = 0;
    void feed(uint8_t b) {
        if (b >= 0xF8) { on_message(b, 0, 0); return; }        // realtime
        if (b & 0x80) {
            status = b; have = 0;
            const uint8_t t = b & 0xF0;
            need = (t == 0xC0 || t == 0xD0) ? 1 : (b >= 0xF0 ? 0 : 2);
            if (need == 0) { if (b < 0xF0) on_message(b, 0, 0); status = b >= 0xF0 ? 0 : status; }
            return;
        }
        if (!status) return;
        d[have++] = b;
        if (have >= need) { on_message(status, d[0], need > 1 ? d[1] : 0); have = 0; }
    }
};

// ── Outbound pump ───────────────────────────────────────────────────────────
// Call every ~1 ms from the backend thread. Forwards engine note events and
// paces 24 PPQN clock from the engine BPM while MIDI out is on.
struct Pump {
    double next_clk = 0.0;
    bool   last_playing = false;

    template <class Send>   // Send(uint8_t status, uint8_t d1, uint8_t d2, int len)
    void run(Send&& send) {
        unsigned char buf[64 * 4];
        const int n = acid_midi_consume_events(buf, 64);
        for (int i = 0; i < n; ++i) {
            const uint8_t type = buf[i * 4], ch = buf[i * 4 + 1] & 0x0F;
            if (type == 1)      send(static_cast<uint8_t>(0x90 | ch), buf[i * 4 + 2], buf[i * 4 + 3], 3);
            else if (type == 2) send(static_cast<uint8_t>(0x80 | ch), buf[i * 4 + 2], buf[i * 4 + 3], 3);
        }
        const bool out_on  = acid_midi_out_enabled() != 0;
        const bool playing = acid_is_playing() != 0;
        if (out_on) {
            if (playing && !last_playing) { send(0xFA, 0, 0, 1); next_clk = now_s(); }
            else if (!playing && last_playing) send(0xFC, 0, 0, 1);
            if (playing) {
                const double now = now_s();
                float bpm = acid_current_bpm();
                if (bpm < 20.0f) bpm = 20.0f;
                const double dt = 60.0 / (static_cast<double>(bpm) * 24.0);
                // Catch up at most 4 late pulses, then snap forward.
                for (int k = 0; k < 4 && now >= next_clk; ++k) { send(0xF8, 0, 0, 1); next_clk += dt; }
                if (now - next_clk > 0.05) next_clk = now + dt;
            }
        }
        last_playing = playing;
    }

    template <class Send>
    void shutdown(Send&& send) {
        if (!acid_midi_out_enabled()) return;
        send(0xFC, 0, 0, 1);
        for (uint8_t ch : {0, 9}) send(static_cast<uint8_t>(0xB0 | ch), 123, 0, 3);  // all notes off
    }
};

}  // namespace acid::midi
