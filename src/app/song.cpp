// Song mode and auto-arrange.
#include "app.hpp"

int App::song_next_slot(int after) {
    for (int step = 1; step <= 9; ++step) {
        int s = ((after - 1 + step) % 9) + 1;
        if (std::filesystem::exists(pattern_path(s))) return s;
    }
    return 0;
}


void App::auto_arrange() {
    push_undo();
    stop_playback();

    auto& rng = rng_;

    // Pick a coherent vibe — all 9 slots will sit inside the same tempo /
    // palette / drum genre, so the arrangement feels like one song rather
    // than nine random rolls glued together.
    int vi = std::uniform_int_distribution<int>(
        0, static_cast<int>(Vibe::V_COUNT) - 1)(rng);
    const VibeSpec& v = kVibes[vi];

    // Pin transport once so every slot shares tempo + swing.
    randomize_transport(v.bpm_center, v.bpm_spread, v.swing_prob,
                        /*silent=*/true);
    g_pattern_length = 16;

    // Generate the spine — a base pattern, drum kit, knob patch, and FX
    // chain that later sections will vary off. Capturing here means mutate_
    // pattern() calls below each compose from the same starting line.
    randomize_pattern(v.pattern_arch, /*silent=*/true);
    randomize_drums  (v.drums_arch,   /*silent=*/true);
    randomize_knobs  (v.knobs_arch,   v.square_bias, /*silent=*/true);
    randomize_fx     (v.fx_arch,      /*silent=*/true);
    Snapshot base = capture_snapshot();

    // Section plan — bars total 32+32+32+64+32+16+64+32+16 = 320 bars.
    // At 16 steps × (60/bpm)/4 per step, one bar ≈ 60/bpm*4 sec. 122 BPM
    // gives ~1.97 sec/bar → 320 × 1.97 ≈ 10.5 minutes.
    constexpr std::array<ArrSlot, 9> kPlan = {{
        {SEC_INTRO,     32},
        {SEC_BASS_IN,   32},
        {SEC_BUILD,     32},
        {SEC_PEAK_A,    64},
        {SEC_VARIATION, 32},
        {SEC_BREAK,     16},
        {SEC_DROP,      64},
        {SEC_PEAK_B,    32},
        {SEC_OUTRO,     16},
    }};

    auto clear_voices_except = [&](std::initializer_list<int> keep) {
        for (int vc = 0; vc < kDrumVoices; ++vc) {
            bool kept = false;
            for (int k : keep) if (k == vc) { kept = true; break; }
            if (!kept)
                for (auto& c : g_drums[static_cast<size_t>(vc)]) c = false;
        }
    };
    auto silence_bass = [&]() {
        for (auto& s : g_steps) {
            s = StepData{};
            s.rest = true;
        }
    };

    for (int slot = 1; slot <= 9; ++slot) {
        apply_snapshot(base);
        const ArrSlot& sl = kPlan[static_cast<size_t>(slot - 1)];

        switch (sl.role) {
            case SEC_INTRO:
                silence_bass();
                clear_voices_except({0, 2}); // BD + CH
                g_cutoff    = 0.20f;
                g_resonance = 0.45f;
                g_delay_mix = 0.00f;
                g_rev_mix   = 0.15f;
                g_drive     = 0.10f;
                break;

            case SEC_BASS_IN:
                // Keep bass notes only on the four downbeats — sparse phrase
                // that lets the filter opening carry the tension.
                for (int i = 0; i < 16; ++i)
                    if (i % 4 != 0) g_steps[static_cast<size_t>(i)].rest = true;
                clear_voices_except({0, 1, 2, 4}); // BD/SD/CH/CL
                g_cutoff    = 0.28f;
                g_resonance = 0.58f;
                g_env_mod   = 0.55f;
                g_delay_mix = 0.10f;
                break;

            case SEC_BUILD:
                // Mutate the bass a few times so repeat ears hear motion
                // without losing the original melodic fingerprint. Rising
                // resonance + open filter signals "about to peak".
                for (int i = 0; i < 2; ++i) mutate_pattern();
                g_cutoff    = 0.38f;
                g_resonance = 0.72f;
                g_env_mod   = 0.78f;
                g_delay_mix = 0.15f;
                g_rev_mix   = 0.25f;
                break;

            case SEC_PEAK_A:
                // Full mix, bright + driven. The 64-bar hold means the peak
                // sits in the listener's ear long enough to feel locked in.
                g_cutoff    = 0.55f;
                g_resonance = 0.78f;
                g_env_mod   = 0.70f;
                g_drive     = 0.38f;
                g_delay_mix = 0.18f;
                break;

            case SEC_VARIATION:
                // Peak-ish intensity but bass is reshaped — gives the
                // listener something new without leaving the drop.
                for (int i = 0; i < 3; ++i) mutate_pattern();
                g_cutoff    = 0.50f;
                g_resonance = 0.75f;
                g_env_mod   = 0.72f;
                g_drive     = 0.32f;
                g_delay_mix = 0.22f;
                break;

            case SEC_BREAK:
                // Classic breakdown: drums drop to the kick on 1/9, bass
                // keeps going but soaked in FX. Short (16 bars) so it
                // doesn't kill momentum.
                clear_voices_except({0});
                for (auto& c : g_drums[0]) c = false;
                g_drums[0][0] = true;
                g_drums[0][8] = true;
                g_cutoff    = 0.30f;
                g_resonance = 0.65f;
                g_delay_mix = 0.35f;
                g_rev_mix   = 0.40f;
                break;

            case SEC_DROP:
                // Full mix returns + a crash on beat 1 for the "everything
                // in" moment. Slightly more drive than peak-A so the drop
                // feels bigger than the earlier peak.
                g_cutoff    = 0.60f;
                g_resonance = 0.80f;
                g_env_mod   = 0.72f;
                g_drive     = 0.48f;
                g_delay_mix = 0.18f;
                if (13 < kDrumVoices) g_drums[13][0] = true;   // CY on 1
                break;

            case SEC_PEAK_B:
                // Second-wind peak. New mutations + open hats on offbeats
                // change the top end — same drop energy, different detail.
                for (int i = 0; i < 4; ++i) mutate_pattern();
                for (int i = 2; i < 16; i += 4)
                    if (3 < kDrumVoices) g_drums[3][i] = true; // OH offbeats
                g_cutoff    = 0.58f;
                g_resonance = 0.82f;
                g_env_mod   = 0.74f;
                g_drive     = 0.40f;
                g_delay_mix = 0.25f;
                break;

            case SEC_OUTRO:
                // Final taper: bass on downbeats, drums off, filter closes.
                for (int i = 0; i < 16; ++i)
                    if (i % 4 != 0) g_steps[static_cast<size_t>(i)].rest = true;
                for (auto& row : g_drums) for (auto& c : row) c = false;
                g_drums[0][0] = true;                          // lone BD on 1
                g_cutoff    = 0.18f;
                g_resonance = 0.50f;
                g_env_mod   = 0.50f;
                g_volume    = 0.50f;
                g_delay_mix = 0.20f;
                g_rev_mix   = 0.45f;
                break;
        }

        // Rotate any existing slot file out of the way (one-deep .bak) so a
        // prior manual save isn't silently destroyed by auto-arrange.
        auto path = pattern_path(slot);
        std::error_code ec;
        if (std::filesystem::exists(path, ec)) {
            auto bak = path;
            bak.replace_extension(".bak.txt");
            std::filesystem::remove(bak, ec);
            std::filesystem::rename(path, bak, ec);
        }
        save_slot_to(path);
        g_song_bars[static_cast<size_t>(slot)] = sl.bars;
    }

    // Load slot 1 so the UI mirrors what's about to play, then boot song
    // mode + transport. User hears the intro immediately.
    load_pattern_slot(1, /*quiet=*/true);
    g_song_mode      = true;
    g_song_slot      = 1;
    g_song_bars_left = g_song_bars[1];
    start_playback();

    // Compute approx total duration so the toast tells the user how long a
    // take would run (helps decide whether to press W for live recording).
    int total_bars = 0;
    for (const auto& p : kPlan) total_bars += p.bars;
    float sec_per_bar = 60.0f / std::max(20.0f, g_bpm) * 4.0f;
    int   minutes = static_cast<int>(total_bars * sec_per_bar / 60.0f + 0.5f);

    char buf[96];
    std::snprintf(buf, sizeof(buf),
                  "\xe2\x99\xaa arrangement: %s  \xc2\xb7 ~%d min  \xc2\xb7 W to record",
                  v.name, minutes);
    set_toast(buf);
}
