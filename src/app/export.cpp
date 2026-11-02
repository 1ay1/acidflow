// WAV/stems bounce, live recording, MIDI file and pattern-code export.
#include "app.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// WAV export — offline bounce the current pattern (4 loops) to
// ~/.config/acidflow/bounce.wav via the engine's synchronous render path.
// ─────────────────────────────────────────────────────────────────────────────
bool App::export_wav() {
    std::error_code ec;
    std::filesystem::create_directories(config_dir(), ec);
    if (ec) return false;

    auto path = config_dir() / "bounce.wav";
    (void)path;
    // Pack each step into the 32-bit layout acid_render_wav → acid_seq_set_step
    // unpacks: midi in the low byte, flags at bit 8 (rest=1|acc=2|slide=4),
    // prob at bit 11 (7 bits), ratchet-1 at bit 18 (2 bits).
    int notes[16];
    for (int i = 0; i < 16; ++i) {
        const auto& s = g_steps[static_cast<size_t>(i)];
        int flags = 0;
        if (s.rest)   flags |= 1;
        if (s.accent) flags |= 2;
        if (s.slide)  flags |= 4;
        int p = std::clamp(s.prob,    0, 100);
        int r = std::clamp(s.ratchet, 1,   4) - 1;
        notes[i] = (s.note & 0xFF)
                 | ((flags & 0x7) << 8)
                 | ((p     & 0x7F) << 11)
                 | ((r     & 0x3)  << 18);
    }
    // The bounce runs on a private engine (copying the live knobs), so the
    // audio thread keeps playing. Stems are the dry synth and drum buses.
    const auto dir = config_dir();
    int rc = acid_render_stems((dir / "bounce.wav").string().c_str(),
                               (dir / "bounce_synth.wav").string().c_str(),
                               (dir / "bounce_drums.wav").string().c_str(),
                               notes, g_pattern_length, g_bpm, g_swing, /*loops=*/4);

    set_toast(rc == 0 ? "\xe2\x9c\x93 wav \xe2\x86\x92 bounce.wav + stems"
                      : "\xe2\x9c\x97 wav failed");
    return rc == 0;
}


bool App::toggle_live_record() {
    if (acid_is_recording()) {
        std::error_code ec;
        std::filesystem::create_directories(config_dir(), ec);
        std::string stem = g_live_rec_stem.empty() ? timestamp_now() : g_live_rec_stem;
        auto wav_path  = config_dir() / ("live_" + stem + ".wav");
        auto slot_path = config_dir() / ("live_" + stem + ".txt");
        int rc = acid_record_end(wav_path.string().c_str());
        g_live_rec_stem.clear();
        if (rc == 0) {
            save_slot_to(slot_path);    // best-effort; failure is non-fatal
            char toast[96];
            std::snprintf(toast, sizeof(toast),
                          "\xe2\x9c\x93 take \xe2\x86\x92 live_%s.wav", stem.c_str());
            set_toast(toast);
        } else {
            set_toast("\xe2\x9c\x97 live save failed");
        }
        return rc == 0;
    }
    int rc = acid_record_begin(kLiveRecordMaxSeconds);
    if (rc != 0) {
        set_toast("\xe2\x9c\x97 record begin failed");
        return false;
    }
    g_live_rec_stem = timestamp_now();
    set_toast("\xe2\x97\x8f REC  (W again to stop)");
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// MIDI export — write the current pattern as a Standard MIDI File (type 0) to
// ~/.config/acidflow/bounce.mid so the user can drop the groove into any DAW.
// Represents accent via higher velocity and slide as a small note-overlap so
// DAWs render it as legato/portamento. One loop of the pattern is written —
// a MIDI file can be looped natively by any host, unlike a WAV bounce.
// ─────────────────────────────────────────────────────────────────────────────
bool App::export_mid() {
    std::error_code ec;
    std::filesystem::create_directories(config_dir(), ec);
    if (ec) { set_toast("\xe2\x9c\x97 mid failed"); return false; }

    auto path = config_dir() / "bounce.mid";
    FILE* f = std::fopen(path.string().c_str(), "wb");
    if (!f) { set_toast("\xe2\x9c\x97 mid failed"); return false; }

    // 480 PPQN is the de-facto SMF standard; any DAW reads it without squinting.
    // At 480 ticks per quarter, a 16th = 120 ticks — swing then splits a pair
    // of 16ths as (240 * ratio, 240 * (1-ratio)) matching the audio-thread math.
    constexpr int kPPQN           = 480;
    constexpr int kSixteenthTicks = kPPQN / 4;
    const int plen = g_pattern_length;

    // Absolute tick at the start of each step (index i in [0, plen]).
    std::vector<int> step_tick(static_cast<size_t>(plen + 1), 0);
    for (int i = 0; i < plen; ++i) {
        bool  even      = (i % 2) == 0;
        float dur_16ths = 2.0f * (even ? g_swing : (1.0f - g_swing));
        int   dur       = static_cast<int>(std::round(dur_16ths * kSixteenthTicks));
        step_tick[static_cast<size_t>(i + 1)] = step_tick[static_cast<size_t>(i)] + dur;
    }

    struct Event { int tick; uint8_t status; uint8_t note; uint8_t vel; };
    std::vector<Event> events;
    events.reserve(static_cast<size_t>(plen * 2));

    for (int i = 0; i < plen; ++i) {
        const auto& s = g_steps[static_cast<size_t>(i)];
        if (s.rest) continue;
        // Probability is a live-performance thing. Exporting a file that
        // plays different notes every time you re-open it would be surprising,
        // so prob is ignored here (always emit the notes the user sees).
        uint8_t vel  = s.accent ? 112 : 80;
        int     midi = std::clamp(s.note, 0, 127);
        int     rat  = std::clamp(s.ratchet, 1, 4);
        int     step_on  = step_tick[static_cast<size_t>(i)];
        int     step_end = step_tick[static_cast<size_t>(i + 1)];
        int     step_len = step_end - step_on;

        for (int k = 0; k < rat; ++k) {
            int on_tick = step_on + step_len * k / rat;
            int off_tick;
            const bool last_sub = (k == rat - 1);
            if (last_sub && s.slide && i + 1 < plen
                && !g_steps[static_cast<size_t>(i + 1)].rest) {
                // 8-tick overlap with the next step's first note → legato.
                off_tick = step_end + 8;
            } else if (last_sub) {
                // 4-tick gap at the step boundary.
                off_tick = step_end - 4;
            } else {
                // Mid-step ratchet: gap-then-retrigger so DAWs hear each
                // sub-hit as a distinct note.
                int sub_end = step_on + step_len * (k + 1) / rat;
                off_tick = sub_end - 2;
            }
            if (off_tick <= on_tick) off_tick = on_tick + 1;
            events.push_back({on_tick,  0x90, static_cast<uint8_t>(midi), vel});
            events.push_back({off_tick, 0x80, static_cast<uint8_t>(midi), 0});
        }
    }

    // Stable sort: note-off before note-on at identical tick so back-to-back
    // same-pitch notes don't leave a hanging gate on the receiver.
    std::stable_sort(events.begin(), events.end(),
        [](const Event& a, const Event& b) {
            if (a.tick != b.tick) return a.tick < b.tick;
            return a.status < b.status;
        });

    // ── Encode the track body as a byte vector, then emit the file chunks ──
    std::vector<uint8_t> track;
    auto emit = [&](std::initializer_list<uint8_t> bs) {
        for (auto b : bs) track.push_back(b);
    };
    auto emit_vlq = [&](int v) {
        uint8_t buf[5];
        int n = 0;
        buf[n++] = static_cast<uint8_t>(v & 0x7F);
        v >>= 7;
        while (v > 0) {
            buf[n++] = static_cast<uint8_t>(0x80 | (v & 0x7F));
            v >>= 7;
        }
        for (int i = n - 1; i >= 0; --i) track.push_back(buf[i]);
    };

    // Tempo meta (FF 51 03 <us per quarter, 3 bytes big-endian>).
    uint32_t us_per_qn = static_cast<uint32_t>(60000000.0f / g_bpm);
    emit_vlq(0);
    emit({0xFF, 0x51, 0x03,
          static_cast<uint8_t>((us_per_qn >> 16) & 0xFF),
          static_cast<uint8_t>((us_per_qn >> 8) & 0xFF),
          static_cast<uint8_t>(us_per_qn & 0xFF)});

    // Time signature 4/4 with 24 MIDI clocks per quarter.
    emit_vlq(0);
    emit({0xFF, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08});

    // Track name ("TB-303").
    emit_vlq(0);
    emit({0xFF, 0x03, 0x06,
          'T', 'B', '-', '3', '0', '3'});

    int last_tick = 0;
    for (const auto& e : events) {
        emit_vlq(e.tick - last_tick);
        last_tick = e.tick;
        track.push_back(e.status);
        track.push_back(e.note);
        track.push_back(e.vel);
    }

    // End of track.
    emit_vlq(0);
    emit({0xFF, 0x2F, 0x00});

    // ── File chunks ────────────────────────────────────────────────────────
    auto write_be32 = [&](uint32_t v) {
        uint8_t b[4] = {
            static_cast<uint8_t>((v >> 24) & 0xFF),
            static_cast<uint8_t>((v >> 16) & 0xFF),
            static_cast<uint8_t>((v >> 8) & 0xFF),
            static_cast<uint8_t>(v & 0xFF),
        };
        std::fwrite(b, 1, 4, f);
    };
    auto write_be16 = [&](uint16_t v) {
        uint8_t b[2] = {
            static_cast<uint8_t>((v >> 8) & 0xFF),
            static_cast<uint8_t>(v & 0xFF),
        };
        std::fwrite(b, 1, 2, f);
    };

    std::fwrite("MThd", 1, 4, f);
    write_be32(6);
    write_be16(0);                 // format 0 = single multi-channel track
    write_be16(1);                 // 1 track
    write_be16(kPPQN);

    std::fwrite("MTrk", 1, 4, f);
    write_be32(static_cast<uint32_t>(track.size()));
    std::fwrite(track.data(), 1, track.size(), f);
    std::fclose(f);

    set_toast("\xe2\x9c\x93 mid \xe2\x86\x92 bounce.mid");
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Pattern text export/import — a tweet-sized base64 code representing the
// entire patch (steps, knobs, FX, tempo, swing) so users can paste grooves
// into chat or commit them to a gist. URL-safe alphabet so the code can sit
// bare in URLs / terminals / markdown without escapes.
//
// Byte 0:   version (1 = legacy, 2 = +prob/ratchet, 3 = +p-locks,
//                    4 = +9 drums, 5 = +3 drum voices: SH/TB/CG,
//                    6 = +4 drum voices: MT/CY/RD/BG)
// Byte 1:   pattern_length
// Bytes 2-3: bpm * 10 as LE u16    (e.g. 1225 = 122.5 bpm)
// Byte 4:   swing u8  (0=0.50, 255=0.75)
// Byte 5:   waveform (0/1)
// Byte 6:   tuning u8 (0..255 maps to the knob's 0..1)
// Bytes 7-13: knobs cutoff/res/env/decay/accent/drive/vol u8
// Bytes 14-20: FX od/dmix/dfb/delay_div/rmix/rsize/rdamp (dfdiv is 0-3)
// Bytes 21-52: 16 steps × (note u8, flags u8) where flags = rest|accent<<1|slide<<2
// Bytes 53-84: 16 steps × (prob u8 [0..100], ratchet u8 [1..4])  — v2+
// Bytes 85-164: 16 steps × (mask u8, lockC u8, lockR u8, lockE u8, lockA u8)
//               — v3+ only. Lock values are the same u8-scaled knob mapping.
// Byte 165: drum_master u8  (0..255 maps to 0..1.5 — scaled-clamp on decode) — v4+
// Bytes 166-.. : N × drum_mask u16 LE  (bit i = hit on step i) — v4+
//                v4 codes carry 9 voices, v5 carries 12, v6 carries
//                kDrumVoices (16).
// Total 165 / 184 / 190 / 198 bytes → ~245 / 254 / 264 base64 chars. Older
// readers silently zero the fields they don't understand. Roundtrips via
// bit-exact encode/decode.
// ─────────────────────────────────────────────────────────────────────────────
std::string App::base64_encode(const uint8_t* data, size_t n) {
    static constexpr char kAlpha[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string out;
    out.reserve((n + 2) / 3 * 4);
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < n) v |= static_cast<uint32_t>(data[i + 1]) << 8;
        if (i + 2 < n) v |= static_cast<uint32_t>(data[i + 2]);
        out += kAlpha[(v >> 18) & 0x3F];
        out += kAlpha[(v >> 12) & 0x3F];
        if (i + 1 < n) out += kAlpha[(v >> 6) & 0x3F];
        if (i + 2 < n) out += kAlpha[v & 0x3F];
    }
    return out;
}


bool App::base64_decode(const std::string& s, std::vector<uint8_t>& out) {
    int8_t rev[256];
    for (int i = 0; i < 256; ++i) rev[i] = -1;
    static constexpr char kAlpha[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    for (int i = 0; i < 64; ++i) rev[static_cast<uint8_t>(kAlpha[i])] = static_cast<int8_t>(i);
    // Also accept standard base64 so users pasting +// variants still work.
    rev[static_cast<uint8_t>('+')] = 62;
    rev[static_cast<uint8_t>('/')] = 63;

    std::string clean;
    clean.reserve(s.size());
    for (char c : s) {
        unsigned uc = static_cast<unsigned char>(c);
        if (uc <= 0x20 || c == '=') continue;     // skip whitespace + padding
        if (rev[uc] < 0) return false;
        clean += c;
    }

    out.clear();
    out.reserve(clean.size() * 3 / 4);
    for (size_t i = 0; i < clean.size(); i += 4) {
        uint32_t v = 0;
        int take = 0;
        for (int k = 0; k < 4 && i + k < clean.size(); ++k) {
            v = (v << 6) | static_cast<uint32_t>(rev[static_cast<uint8_t>(clean[i + k])]);
            take++;
        }
        v <<= (4 - take) * 6;
        if (take >= 2) out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        if (take >= 3) out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        if (take >= 4) out.push_back(static_cast<uint8_t>(v & 0xFF));
    }
    return true;
}


std::string App::pattern_encode_string() {
    // 165 bytes of v3 header + 1 byte drum_master + kDrumVoices × 2 drum masks.
    constexpr size_t kN = 165 + 1 + kDrumVoices * 2;
    uint8_t buf[kN] = {};
    auto u8 = [](float v) -> uint8_t {
        int i = static_cast<int>(std::round(std::clamp(v, 0.0f, 1.0f) * 255.0f));
        return static_cast<uint8_t>(std::clamp(i, 0, 255));
    };
    buf[0] = 6;                                                // version
    buf[1] = static_cast<uint8_t>(std::clamp(g_pattern_length, 1, 16));
    int bpm10 = std::clamp(static_cast<int>(std::round(g_bpm * 10.0f)), 200, 3000);
    buf[2] = static_cast<uint8_t>(bpm10 & 0xFF);
    buf[3] = static_cast<uint8_t>((bpm10 >> 8) & 0xFF);
    float sw_n = std::clamp((g_swing - 0.50f) / 0.25f, 0.0f, 1.0f);
    buf[4]  = static_cast<uint8_t>(std::round(sw_n * 255.0f));
    buf[5]  = static_cast<uint8_t>(g_wave & 1);
    buf[6]  = u8(g_tune);
    buf[7]  = u8(g_cutoff);
    buf[8]  = u8(g_resonance);
    buf[9]  = u8(g_env_mod);
    buf[10] = u8(g_decay);
    buf[11] = u8(g_accent);
    buf[12] = u8(g_drive);
    buf[13] = u8(g_volume);
    buf[14] = u8(g_od_amt);
    buf[15] = u8(g_delay_mix);
    buf[16] = u8(g_delay_fb);
    buf[17] = static_cast<uint8_t>(std::clamp(g_delay_div, 0, 3));
    buf[18] = u8(g_rev_mix);
    buf[19] = u8(g_rev_size);
    buf[20] = u8(g_rev_damp);
    for (int i = 0; i < 16; ++i) {
        const auto& s = g_steps[static_cast<size_t>(i)];
        buf[21 + i * 2]     = static_cast<uint8_t>(std::clamp(s.note, 0, 127));
        uint8_t fl = 0;
        if (s.rest)   fl |= 1;
        if (s.accent) fl |= 2;
        if (s.slide)  fl |= 4;
        buf[21 + i * 2 + 1] = fl;
        buf[53 + i * 2]     = static_cast<uint8_t>(std::clamp(s.prob,    0, 100));
        buf[53 + i * 2 + 1] = static_cast<uint8_t>(std::clamp(s.ratchet, 1,   4));
        const size_t L = 85 + i * 5;
        buf[L + 0] = static_cast<uint8_t>(s.lock_mask & 0xF);
        buf[L + 1] = u8(s.lock_cutoff);
        buf[L + 2] = u8(s.lock_res);
        buf[L + 3] = u8(s.lock_env);
        buf[L + 4] = u8(s.lock_accent);
    }
    // v4 drum block. Master is 0..1.5, scale-encode as u8 where 255 = 1.5.
    {
        int dm = static_cast<int>(std::round(
            std::clamp(g_drum_master, 0.0f, 1.5f) / 1.5f * 255.0f));
        buf[165] = static_cast<uint8_t>(std::clamp(dm, 0, 255));
        for (int v = 0; v < kDrumVoices; ++v) {
            uint16_t mask = 0;
            for (int i = 0; i < 16; ++i) {
                if (g_drums[static_cast<size_t>(v)][static_cast<size_t>(i)])
                    mask |= static_cast<uint16_t>(1u << i);
            }
            buf[166 + v * 2 + 0] = static_cast<uint8_t>(mask & 0xFF);
            buf[166 + v * 2 + 1] = static_cast<uint8_t>((mask >> 8) & 0xFF);
        }
    }
    return base64_encode(buf, kN);
}


bool App::pattern_decode_string(const std::string& code) {
    std::vector<uint8_t> buf;
    if (!base64_decode(code, buf)) return false;
    if (buf.size() < 53 || buf[0] < 1 || buf[0] > 6) return false;
    const bool v2 = (buf[0] >= 2) && (buf.size() >= 85);
    const bool v3 = (buf[0] >= 3) && (buf.size() >= 165);
    // Drum block: v4 codes carry 9 voices (184 bytes), v5 carries 12 (190
    // bytes), v6 carries kDrumVoices (16 → 198 bytes). We detect the version
    // from byte 0 and read only as many voices as that version promised so
    // short payloads decode cleanly without reaching past the buffer.
    const int  v_drums  = (buf[0] >= 6) ? kDrumVoices
                        : (buf[0] >= 5) ? 12
                        : 9;
    const bool v4 = (buf[0] >= 4) && (buf.size() >= 165 + 1 + static_cast<size_t>(v_drums) * 2);
    auto f8 = [](uint8_t v) -> float { return static_cast<float>(v) / 255.0f; };

    g_pattern_length = std::clamp(static_cast<int>(buf[1]), 1, 16);
    uint16_t bpm10 = static_cast<uint16_t>(buf[2])
                   | (static_cast<uint16_t>(buf[3]) << 8);
    g_bpm       = std::clamp(static_cast<float>(bpm10) / 10.0f, 40.0f, 300.0f);
    g_swing     = std::clamp(0.50f + f8(buf[4]) * 0.25f, 0.50f, 0.75f);
    g_wave      = buf[5] & 1;
    g_tune      = f8(buf[6]);
    g_cutoff    = f8(buf[7]);
    g_resonance = f8(buf[8]);
    g_env_mod   = f8(buf[9]);
    g_decay     = f8(buf[10]);
    g_accent    = f8(buf[11]);
    g_drive     = f8(buf[12]);
    g_volume    = f8(buf[13]);
    g_od_amt    = f8(buf[14]);
    g_delay_mix = f8(buf[15]);
    g_delay_fb  = f8(buf[16]);
    g_delay_div = std::clamp(static_cast<int>(buf[17]), 0, 3);
    g_rev_mix   = f8(buf[18]);
    g_rev_size  = f8(buf[19]);
    g_rev_damp  = f8(buf[20]);
    for (int i = 0; i < 16; ++i) {
        auto& s = g_steps[static_cast<size_t>(i)];
        s.note   = std::clamp(static_cast<int>(buf[21 + i * 2]), 12, 96);
        uint8_t fl = buf[21 + i * 2 + 1];
        s.rest   = (fl & 1) != 0;
        s.accent = (fl & 2) != 0;
        s.slide  = (fl & 4) != 0;
        if (v2) {
            s.prob    = std::clamp(static_cast<int>(buf[53 + i * 2]),     0, 100);
            s.ratchet = std::clamp(static_cast<int>(buf[53 + i * 2 + 1]), 1,   4);
        } else {
            s.prob    = 100;
            s.ratchet = 1;
        }
        if (v3) {
            const size_t L = 85 + i * 5;
            s.lock_mask   = static_cast<uint8_t>(buf[L + 0] & 0xF);
            s.lock_cutoff = f8(buf[L + 1]);
            s.lock_res    = f8(buf[L + 2]);
            s.lock_env    = f8(buf[L + 3]);
            s.lock_accent = f8(buf[L + 4]);
        } else {
            s.lock_mask   = 0;
            s.lock_cutoff = s.lock_res = s.lock_env = s.lock_accent = 0.0f;
        }
    }
    // Drum block. Always wipe the kit first so older codes (v1-v3, no drums)
    // land in a cleared grid rather than leaking the previous pattern's hits.
    // v4 payloads carry 9 voices; v5 adds SH/TB/CG; v6 adds MT/CY/RD/BG.
    // Unknown (newer) voices stay silent when loading older codes.
    for (auto& row : g_drums) for (auto& c : row) c = false;
    if (v4) {
        g_drum_master = std::clamp(
            static_cast<float>(buf[165]) / 255.0f * 1.5f, 0.0f, 1.5f);
        for (int v = 0; v < v_drums; ++v) {
            uint16_t m = static_cast<uint16_t>(buf[166 + v * 2 + 0])
                       | (static_cast<uint16_t>(buf[166 + v * 2 + 1]) << 8);
            for (int i = 0; i < 16; ++i) {
                g_drums[static_cast<size_t>(v)][static_cast<size_t>(i)]
                    = (m & (1u << i)) != 0;
            }
        }
    } else {
        // v1-v3 codes carry no drums; leave drum_master at its current value
        // (the user's knob state) rather than forcing it to 1.0.
    }
    g_preset_index = -1;
    g_step_sel     = 0;
    return true;
}


bool App::export_pattern_text() {
    std::error_code ec;
    std::filesystem::create_directories(config_dir(), ec);
    if (ec) { set_toast("\xe2\x9c\x97 text failed"); return false; }

    auto path = config_dir() / "pattern.txt";
    std::ofstream f(path);
    if (!f) { set_toast("\xe2\x9c\x97 text failed"); return false; }
    f << pattern_encode_string() << '\n';
    f.close();

    set_toast("\xe2\x9c\x93 text \xe2\x86\x92 pattern.txt");
    return true;
}


bool App::import_pattern_text() {
    auto path = config_dir() / "pattern.txt";
    std::ifstream f(path);
    if (!f) { set_toast("\xe2\x9c\x97 no pattern.txt"); return false; }

    std::string code, line;
    while (std::getline(f, line)) code += line;
    if (!pattern_decode_string(code)) {
        set_toast("\xe2\x9c\x97 bad pattern code");
        return false;
    }
    set_toast("\xe2\x86\xba text \xe2\x86\x90 pattern.txt");
    return true;
}
