// Save slots and takes.
#include "app.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// Pattern persistence — save/load current 16-step pattern as JSON-ish text
// under ~/.config/acidflow/. One slot per keyboard digit 1..9; slot 0 is
// "last", used for the quick-load round-trip.
// ─────────────────────────────────────────────────────────────────────────────
std::filesystem::path App::config_dir() {
#if defined(_WIN32)
    const char* appdata = std::getenv("APPDATA");
    std::filesystem::path base = appdata ? appdata : ".";
#else
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    const char* home = std::getenv("HOME");
    std::filesystem::path base = xdg ? xdg : (home ? std::string(home) + "/.config" : std::string("."));
#endif
    return base / "acidflow";
}

// YYYYMMDD_HHMMSS in local time — a filename-safe timestamp so takes don't
// collide and sort chronologically when the user lists the directory.
std::string App::timestamp_now() {
    std::time_t now = std::time(nullptr);
    std::tm lt{};
#if defined(_WIN32)
    localtime_s(&lt, &now);
#else
    localtime_r(&now, &lt);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d%02d%02d_%02d%02d%02d",
                  lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
                  lt.tm_hour, lt.tm_min, lt.tm_sec);
    return buf;
}


std::filesystem::path App::pattern_path(int slot) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "slot_%d.txt", slot);
    return config_dir() / buf;
}

// Minimal human-readable format — one step per line, fields space-separated:
//   note rest accent slide
// Header line carries pattern_length + bpm so we restore full state.
// Writes the current state to `path` in the v8 slot format. No toast, no
// backup — just the raw write, so callers can reuse this for takes and
// autosaves without surprise side effects.
bool App::save_slot_to(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) return false;

    std::ofstream f(path);
    if (!f) return false;

    // v8: adds knobs + fx lines after the drum block so v4-v7 readers stop
    // at EOF cleanly. v7 had kDrumVoices (16) drum rows; v8 keeps that and
    // appends swing/wave/tune/knobs and od/delay/reverb FX params.
    f << "# acidflow pattern v8\n";
    f << g_pattern_length << " " << g_bpm << "\n";
    // Per-step line (v3+):
    //   note rest accent slide prob ratchet lockmask lockC lockR lockE lockA
    // Older readers (v1/v2) stop at field 4/6 and silently default the
    // trailing fields.
    for (int i = 0; i < 16; ++i) {
        const auto& s = g_steps[static_cast<size_t>(i)];
        f << s.note << " " << (s.rest ? 1 : 0) << " "
          << (s.accent ? 1 : 0) << " " << (s.slide ? 1 : 0) << " "
          << s.prob << " " << s.ratchet << " "
          << static_cast<int>(s.lock_mask) << " "
          << s.lock_cutoff << " " << s.lock_res << " "
          << s.lock_env    << " " << s.lock_accent << "\n";
    }
    // v4 drum block — one line per voice, 16 ints (0/1) + final drum master.
    // Kept as a trailing section so v3 readers just stop at EOF.
    f << "drums " << g_drum_master << "\n";
    for (int v = 0; v < kDrumVoices; ++v) {
        for (int i = 0; i < 16; ++i) {
            f << (g_drums[static_cast<size_t>(v)][static_cast<size_t>(i)] ? 1 : 0);
            if (i < 15) f << " ";
        }
        f << "\n";
    }
    // v8 knobs + FX block — appended after drums so v4-v7 readers stop at EOF
    // cleanly. The wave field is an int; all others are floats on [0..1] except
    // delay_div (0..3) which is also an int.
    f << "knobs " << g_swing << " " << g_wave << " " << g_tune
      << " " << g_cutoff    << " " << g_resonance << " " << g_env_mod
      << " " << g_decay     << " " << g_accent    << " " << g_drive
      << " " << g_volume    << "\n";
    f << "fx " << g_od_amt   << " " << g_delay_mix << " " << g_delay_fb
      << " " << g_delay_div  << " " << g_rev_mix   << " " << g_rev_size
      << " " << g_rev_damp   << "\n";
    return static_cast<bool>(f);
}


bool App::save_pattern_slot(int slot) {
    auto path = pattern_path(slot);

    // One-deep .bak so an accidental Shift-N that clobbers a good slot can
    // still be recovered — either by hand (rename the .bak back) or via the
    // in-app undo. We only keep the most recent previous revision; filling
    // the config directory with historical revisions would be noise for the
    // 99% of saves that are intentional.
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        auto bak = path;
        bak.replace_extension(".bak.txt");
        std::filesystem::remove(bak, ec);
        std::filesystem::rename(path, bak, ec);
    }

    bool ok = save_slot_to(path);
    if (ok) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "\xe2\x9c\x93 saved slot %d", slot);
        set_toast(buf);
    }
    return ok;
}

// Core slot-file reader. Shared between numbered-slot loading (digit keys)
// and take picker loading (live_<timestamp>.txt files alongside wavs).
// `label` appears in the "loaded X" toast when quiet is false — e.g. "slot 3"
// or "take 20260421_153900".
bool App::load_slot_from_path(const std::filesystem::path& path,
                                const std::string& label,
                                bool quiet)
{
    std::ifstream f(path);
    if (!f) return false;

    std::string header;
    std::getline(f, header);
    // v2 adds trailing prob+ratchet; v3 adds p-lock fields; v4 adds a drum
    // block (5 voices); v5 extends drums to 9; v6 extends to 12 (SH/TB/CG);
    // v7 extends to 16 (MT/CY/RD/BG); v8 adds knobs + fx lines after drums.
    // The reader always consumes up to kDrumVoices rows and bails at EOF,
    // so older files leave newer voices empty.
    bool v8 = header.find("v8") != std::string::npos;
    bool v7 = v8 || header.find("v7") != std::string::npos;
    bool v6 = v7 || header.find("v6") != std::string::npos;
    bool v5 = v6 || header.find("v5") != std::string::npos;
    bool v4 = v5 || header.find("v4") != std::string::npos;
    bool v3 = v4 || header.find("v3") != std::string::npos;
    bool v2 = v3 || header.find("v2") != std::string::npos;
    (void)v7; (void)v6; (void)v5;

    int plen; float bpm;
    if (!(f >> plen >> bpm)) return false;
    g_pattern_length = std::clamp(plen, 4, 16);
    g_bpm = std::clamp(bpm, 40.0f, 220.0f);

    for (int i = 0; i < 16; ++i) {
        int note, rest, acc, sld;
        if (!(f >> note >> rest >> acc >> sld)) break;
        auto& s = g_steps[static_cast<size_t>(i)];
        s.note   = std::clamp(note, 12, 96);
        s.rest   = rest != 0;
        s.accent = acc != 0;
        s.slide  = sld != 0;
        s.prob    = 100;
        s.ratchet = 1;
        s.lock_mask   = 0;
        s.lock_cutoff = 0.0f;
        s.lock_res    = 0.0f;
        s.lock_env    = 0.0f;
        s.lock_accent = 0.0f;
        if (v2) {
            int prob, rat;
            if (f >> prob >> rat) {
                s.prob    = std::clamp(prob, 0, 100);
                s.ratchet = std::clamp(rat,  1,   4);
            }
        }
        if (v3) {
            int   lm; float lc, lr, le, la;
            if (f >> lm >> lc >> lr >> le >> la) {
                s.lock_mask   = static_cast<uint8_t>(lm & 0xF);
                s.lock_cutoff = std::clamp(lc, 0.0f, 1.0f);
                s.lock_res    = std::clamp(lr, 0.0f, 1.0f);
                s.lock_env    = std::clamp(le, 0.0f, 1.0f);
                s.lock_accent = std::clamp(la, 0.0f, 1.0f);
            }
        }
    }
    // v4+: drum block — "drums <master>\n" header, then N rows of 16 ints
    // (v4 = 5 rows, v5 = 9 rows). We always read up to kDrumVoices rows and
    // stop early if the stream dries up, so v4 files still load cleanly with
    // the 4 extra voices empty. Always reset the kit first so older-format
    // loads wipe any drums from a prior session rather than leaking them
    // into a drum-less pattern.
    for (auto& row : g_drums) for (auto& c : row) c = false;
    if (v4) {
        std::string tag; float dm = 1.0f;
        if (f >> tag >> dm && tag == "drums") {
            g_drum_master = std::clamp(dm, 0.0f, 1.5f);
            for (int v = 0; v < kDrumVoices; ++v) {
                bool row_ok = true;
                for (int i = 0; i < 16; ++i) {
                    int x = 0;
                    if (!(f >> x)) { row_ok = false; break; }
                    g_drums[static_cast<size_t>(v)][static_cast<size_t>(i)] = x != 0;
                }
                if (!row_ok) break;      // short file → leave remaining voices empty
            }
        }
    }
    // v8: knobs + FX lines appended after the drum block.
    if (v8) {
        std::string tag;
        if (f >> tag && tag == "knobs") {
            float sw, tune, cut, res, em, dec, acc, drv, vol;
            int   wav;
            if (f >> sw >> wav >> tune >> cut >> res >> em >> dec >> acc >> drv >> vol) {
                g_swing      = std::clamp(sw,   0.50f, 0.75f);
                g_wave       = std::clamp(wav, 0, 1);
                g_tune       = std::clamp(tune, 0.0f, 1.0f);
                g_cutoff     = std::clamp(cut,  0.0f, 1.0f);
                g_resonance  = std::clamp(res,  0.0f, 1.0f);
                g_env_mod    = std::clamp(em,   0.0f, 1.0f);
                g_decay      = std::clamp(dec,  0.0f, 1.0f);
                g_accent     = std::clamp(acc,  0.0f, 1.0f);
                g_drive      = std::clamp(drv,  0.0f, 1.0f);
                g_volume     = std::clamp(vol,  0.0f, 1.0f);
            }
        }
        if (f >> tag && tag == "fx") {
            float od, dmix, dfb, rmix, rsize, rdamp;
            int   ddiv;
            if (f >> od >> dmix >> dfb >> ddiv >> rmix >> rsize >> rdamp) {
                g_od_amt    = std::clamp(od,    0.0f, 1.0f);
                g_delay_mix = std::clamp(dmix,  0.0f, 1.0f);
                g_delay_fb  = std::clamp(dfb,   0.0f, 1.0f);
                g_delay_div = std::clamp(ddiv,  0, 3);
                g_rev_mix   = std::clamp(rmix,  0.0f, 1.0f);
                g_rev_size  = std::clamp(rsize, 0.0f, 1.0f);
                g_rev_damp  = std::clamp(rdamp, 0.0f, 1.0f);
            }
        }
    }
    g_preset_index = -1;
    g_step_sel = 0;
    if (!quiet) {
        std::string msg = "\xe2\x86\xba loaded ";
        msg += label;
        set_toast(msg);
    }
    return true;
}

// Numbered-slot wrapper around `load_slot_from_path`. Used by digit keys and
// song mode; the take picker calls `load_slot_from_path` directly with the
// live-recording .txt path.
bool App::load_pattern_slot(int slot, bool quiet) {
    char lbl[24];
    std::snprintf(lbl, sizeof(lbl), "slot %d", slot);
    return load_slot_from_path(pattern_path(slot), lbl, quiet);
}

// Scan the config dir for live_*.txt take files and sort newest-first so the
// most recent recording sits at the top of the picker. Silent on I/O errors —
// we treat a missing / unreadable dir as "no takes".
void App::refresh_takes() {
    g_takes_entries.clear();
    std::error_code ec;
    auto dir = config_dir();
    if (!std::filesystem::exists(dir, ec)) return;

    for (auto& de : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!de.is_regular_file(ec)) continue;
        auto name = de.path().filename().string();
        if (name.rfind("live_", 0) != 0) continue;
        if (de.path().extension() != ".txt") continue;
        g_takes_entries.push_back(de.path());
    }

    std::sort(g_takes_entries.begin(), g_takes_entries.end(),
        [](const std::filesystem::path& a, const std::filesystem::path& b) {
            std::error_code e1, e2;
            auto ta = std::filesystem::last_write_time(a, e1);
            auto tb = std::filesystem::last_write_time(b, e2);
            return ta > tb;                             // newest first
        });

    if (g_takes_sel >= static_cast<int>(g_takes_entries.size())) {
        g_takes_sel = 0;
    }
}


void App::open_take_picker() {
    refresh_takes();
    g_takes_sel = 0;
    if (g_takes_entries.empty()) {
        set_toast("no takes found (press W during playback to record)");
        return;
    }
    g_takes_open = true;
}

// Song-mode helper — find the next saved slot after `after` (cyclic, 1..9).
// Returns 0 when nothing is saved at all. When `after` is already saved and is
// the only saved slot, it returns `after` so song mode loops the same pattern.
