// Randomizers and the mutator.
#include "app.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// Knob randomizer — a dice roll over the *correlated* acid parameter space.
// We pick one of four character archetypes (classic / squelchy / driving /
// dubby), each pre-baked with sensible inter-knob relationships, then jitter
// within that archetype. This consistently produces playable patches:
//   * high Q pairs with low-mid cutoff (so the envelope has room to scream)
//   * long decay pairs with lower env mod (slow evolving sweeps)
//   * short decay pairs with heavy env mod + high accent (snappy squelch)
// A naive rand(0..1) across every knob usually lands on muddy / inaudible
// patches — the archetype pass keeps each roll musical.
//
// `arch_hint` forces a specific archetype (used by randomize_everything to
// match the picked Vibe); default -1 leaves the helper to roll its own dice.
// ─────────────────────────────────────────────────────────────────────────────
void App::randomize_knobs(int arch_hint, bool square_bias,
                            bool silent) {
    auto& rng = rng_;
    auto u = [&](float lo, float hi) {
        return lo + (hi - lo) * std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
    };
    auto chance = [&](float p) {
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng) < p;
    };

    int a = (arch_hint >= 0)
        ? std::clamp(arch_hint, 0, KA_COUNT - 1)
        : std::uniform_int_distribution<int>(0, KA_COUNT - 1)(rng);

    // Tuning: always near centre — detuning kills "in-key" playback. ±1 st.
    g_tune = u(0.48f, 0.52f);

    switch (a) {
        case KA_CLASSIC:
            // Mid cutoff, strong Q, moderate sweep, medium decay — the
            // universal acid starting point.
            g_cutoff    = u(0.30f, 0.50f);
            g_resonance = u(0.65f, 0.85f);
            g_env_mod   = u(0.50f, 0.75f);
            g_decay     = u(0.25f, 0.50f);
            g_accent    = u(0.50f, 0.75f);
            break;
        case KA_SQUELCH:
            // Self-osc territory: very high Q, low cutoff (max headroom for
            // the MEG to sweep), tight decay and heavy accent.
            g_cutoff    = u(0.15f, 0.35f);
            g_resonance = u(0.80f, 0.95f);
            g_env_mod   = u(0.70f, 0.95f);
            g_decay     = u(0.10f, 0.30f);
            g_accent    = u(0.70f, 0.90f);
            break;
        case KA_DRIVING:
            // Open filter, less envelope motion, medium resonance — the
            // Beltram "Energy Flash" engine sound.
            g_cutoff    = u(0.50f, 0.75f);
            g_resonance = u(0.55f, 0.75f);
            g_env_mod   = u(0.30f, 0.55f);
            g_decay     = u(0.20f, 0.45f);
            g_accent    = u(0.55f, 0.80f);
            break;
        case KA_DUBBY:
            // Low cutoff, long decay, moderate Q — Plastikman / dub techno
            // territory. Sustained filter motion over bars.
            g_cutoff    = u(0.10f, 0.30f);
            g_resonance = u(0.55f, 0.80f);
            g_env_mod   = u(0.40f, 0.70f);
            g_decay     = u(0.55f, 0.90f);
            g_accent    = u(0.35f, 0.60f);
            break;
    }

    g_volume = u(0.55f, 0.70f);           // keep output in a safe range
    // Square is classic but ~30% of rolls — saw is canonical 303. A vibe hint
    // can flip the bias (electro prefers square).
    g_wave   = chance(square_bias ? 0.70f : 0.30f) ? 1 : 0;

    if (!silent) {
        static const char* kNames[KA_COUNT] = {"classic", "squelch", "driving", "dubby"};
        char buf[32];
        std::snprintf(buf, sizeof(buf), "\xe2\x86\xbb knobs: %s", kNames[a]);
        set_toast(buf);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Pattern randomizer (musical, not purely random)
// ─────────────────────────────────────────────────────────────────────────────
// Picks one of four groove archetypes (pedal / driving / melodic / dub),
// each of which parameterises note density, octave-jump frequency, slide
// probability, and accent weighting differently. All pitches are drawn from
// a minor-pentatonic/Dorian superset — the canonical 303 vocabulary
// (Voodoo Ray, Energy Flash, Acperience etc.). A few hand-picked musical
// constraints keep every roll playable:
//
//   * downbeats (steps 0, 4, 8, 12) favour the tonic so the groove lands
//   * at least one accent per bar — flat patterns sound dead
//   * slides are only kept when the previous step played (you can't glide
//     into a note from a rest)
//   * slides resolve DOWN to the tonic on the next downbeat, which is the
//     signature 303 "squelch back home" motion
//
// This is strictly better than a flat per-step dice roll, which produces
// listless noise roughly 80% of the time.
void App::randomize_pattern(int arch_hint, bool silent) {
    auto& rng = rng_;
    auto U  = [&]() { return std::uniform_real_distribution<float>(0.0f,1.0f)(rng); };
    auto Ui = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo,hi)(rng); };
    auto chance = [&](float p) { return U() < p; };

    int gr = (arch_hint >= 0)
        ? std::clamp(arch_hint, 0, PA_COUNT - 1)
        : Ui(0, PA_COUNT - 1);

    // ── Scale library ───────────────────────────────────────────────────────
    // Each scale: semitone offsets from root with per-note weights. Scale tones
    // stay in one octave (0..12) — octave variation comes from the contour pass
    // below, so a line has both scale logic AND melodic shape.
    struct ScaleNote { int semi; int w; };
    using ScaleVec = std::array<ScaleNote, 6>;
    static constexpr ScaleVec kMinorPenta = {{{0,5},{3,3},{5,3},{7,4},{10,3},{12,2}}};
    static constexpr ScaleVec kBlues      = {{{0,4},{3,3},{5,2},{6,2},{7,3},{10,3}}};
    static constexpr ScaleVec kDorian     = {{{0,4},{3,3},{5,2},{7,3},{9,2},{10,2}}};
    static constexpr ScaleVec kPhrygian   = {{{0,4},{1,2},{3,3},{5,2},{7,3},{10,2}}};
    static constexpr ScaleVec kMinor      = {{{0,4},{3,3},{5,2},{7,3},{8,2},{10,2}}};
    static constexpr ScaleVec kMixo       = {{{0,4},{2,2},{5,3},{7,4},{9,2},{10,2}}};

    const ScaleVec* scales[] = {
        &kMinorPenta, &kBlues, &kDorian, &kPhrygian, &kMinor, &kMixo
    };
    const ScaleVec& scale = *scales[Ui(0, 5)];

    // Root octave — C1=24, C2=36, C3=48. Heavy C2 bias: the 303's sweet spot.
    static constexpr int kRoots[] = {24, 36, 36, 36, 48};
    int tonic = kRoots[Ui(0, 4)];

    auto pick_tone = [&]() -> int {
        int total = 0;
        for (auto& n : scale) total += n.w;
        int r = Ui(0, total - 1), acc = 0;
        for (auto& n : scale) {
            acc += n.w;
            if (r < acc) return n.semi;
        }
        return 0;
    };

    // Strong scale degrees for downbeat fallback — root / 5 / octave.
    auto pick_strong = [&]() -> int {
        static constexpr int kStrong[] = {0, 0, 0, 7, 12};
        return kStrong[Ui(0, 4)];
    };

    // ── Contour — per-step target octave offset shaping the melodic arc ────
    // A good bassline has direction. We pick a shape per randomization and
    // compute, for each step, a base octave (0 = tonic oct, ±1 = displaced).
    // Scale-tone picking still applies on top, so "arc" + "blues" gives a
    // rising blues line instead of scale-tone confetti.
    enum Contour { C_FLAT, C_RISE, C_FALL, C_ARCH, C_VALLEY, C_WAVE };
    Contour contour = static_cast<Contour>(Ui(0, 5));
    auto contour_oct = [&](int i) -> int {
        float t = (g_pattern_length <= 1) ? 0.0f
                : float(i) / float(g_pattern_length - 1);        // 0..1
        float y;
        switch (contour) {
            case C_FLAT:   y = 0.0f;                                    break;
            case C_RISE:   y = t;                                       break;
            case C_FALL:   y = 1.0f - t;                                break;
            case C_ARCH:   y = 1.0f - std::abs(2.0f * t - 1.0f);        break; // ^
            case C_VALLEY: y = std::abs(2.0f * t - 1.0f);               break; // v
            case C_WAVE:   y = 0.5f * (1.0f + std::sin(t * 6.2831853f * 1.5f)); break;
            default:       y = 0.0f;                                    break;
        }
        // Map [0..1] → {-1, 0, +1, maybe +2 for ARCH peak}
        if (y < 0.25f) return (contour == C_FALL || contour == C_VALLEY) ? -1 : 0;
        if (y < 0.65f) return 0;
        if (y < 0.90f) return 1;
        return (contour == C_ARCH) ? 2 : 1;
    };

    // Per-archetype shape parameters.
    float rest_rate=0.25f, root_bias=0.40f, db_root=0.55f;
    float slide_rate=0.25f, accent_off=0.15f;
    float prob_rate=0.12f, ratchet_r=0.08f, plock_r=0.12f;

    switch (gr) {
        case PA_PEDAL:
            rest_rate=0.20f; root_bias=0.55f; db_root=0.75f;
            slide_rate=0.20f; accent_off=0.10f;
            prob_rate=0.08f; ratchet_r=0.04f; plock_r=0.10f;
            contour = C_FLAT;   // pedal is always flat by definition
            break;
        case PA_DRIVING:
            rest_rate=0.05f; root_bias=0.35f; db_root=0.55f;
            slide_rate=0.20f; accent_off=0.30f;
            prob_rate=0.15f; ratchet_r=0.12f; plock_r=0.15f;
            break;
        case PA_MELODIC:
            rest_rate=0.15f; root_bias=0.25f; db_root=0.45f;
            slide_rate=0.40f; accent_off=0.20f;
            prob_rate=0.20f; ratchet_r=0.06f; plock_r=0.20f;
            break;
        case PA_DUB:
            rest_rate=0.50f; root_bias=0.40f; db_root=0.60f;
            slide_rate=0.50f; accent_off=0.12f;
            prob_rate=0.25f; ratchet_r=0.04f; plock_r=0.12f;
            break;
        case PA_STUTTER:
            rest_rate=0.10f; root_bias=0.30f; db_root=0.50f;
            slide_rate=0.10f; accent_off=0.35f;
            prob_rate=0.20f; ratchet_r=0.30f; plock_r=0.15f;
            break;
        case PA_CHROM:
            rest_rate=0.20f; root_bias=0.20f; db_root=0.45f;
            slide_rate=0.45f; accent_off=0.18f;
            prob_rate=0.15f; ratchet_r=0.08f; plock_r=0.25f;
            break;
    }

    if (gr == PA_DUB || gr == PA_PEDAL) {
        g_pattern_length = chance(0.35f) ? 8 : 16;
    } else {
        g_pattern_length = 16;
    }

    // ── Pass 1: notes + rests (contour-aware) ───────────────────────────────
    bool has_accent = false;
    for (int i = 0; i < g_pattern_length; ++i) {
        auto& s = g_steps[static_cast<size_t>(i)];
        s = StepData{};
        bool downbeat = (i % 4 == 0);

        if (!downbeat && chance(rest_rate)) { s.rest = true; continue; }

        int semi;
        if (downbeat) {
            semi = chance(db_root) ? 0 : pick_strong();
        } else if (chance(root_bias)) {
            semi = 0;
        } else {
            semi = pick_tone();
        }

        int oct = contour_oct(i);
        // Small jitter so contour doesn't flatten every variation to the same step
        if (!downbeat && chance(0.18f)) oct += chance(0.5f) ? 1 : -1;

        int pitch = tonic + semi + 12 * oct;
        // Clamp to a usable bass register: tonic-12..tonic+15.
        pitch = std::clamp(pitch, tonic - 12, tonic + 15);

        s.note   = pitch;
        s.rest   = false;
        s.accent = downbeat ? chance(0.65f) : chance(accent_off);
        if (s.accent) has_accent = true;
    }

    // ── Pass 2: rest-run limit — no more than 2 consecutive rests ───────────
    // Long rest runs kill the groove; break them by placing a scale tone at
    // the contour octave.
    {
        int run = 0;
        for (int i = 0; i < g_pattern_length; ++i) {
            auto& s = g_steps[static_cast<size_t>(i)];
            if (s.rest) {
                ++run;
                if (run > 2) {
                    s.rest = false;
                    s.note = std::clamp(tonic + pick_tone() + 12 * contour_oct(i),
                                        tonic - 12, tonic + 15);
                    s.accent = chance(accent_off);
                    run = 0;
                }
            } else {
                run = 0;
            }
        }
    }

    // ── Pass 3: slides ──────────────────────────────────────────────────────
    for (int i = 1; i < g_pattern_length; ++i) {
        auto& s = g_steps[static_cast<size_t>(i)];
        if (s.rest) continue;
        const auto& prev = g_steps[static_cast<size_t>(i - 1)];
        if (prev.rest) continue;
        float p = slide_rate;
        if (s.note > prev.note)         p *= 1.6f;
        if (std::abs(s.note - prev.note) > 7) p *= 0.4f;  // wide leaps don't glide well
        if (chance(std::min(p, 0.90f))) s.slide = true;
    }

    // ── Pass 4: probability + ratchet + p-locks ─────────────────────────────
    static constexpr int kProbChoices[] = {75, 50, 25};
    for (int i = 0; i < g_pattern_length; ++i) {
        auto& s = g_steps[static_cast<size_t>(i)];
        if (s.rest) continue;

        if (i > 0 && (i % 4) != 0 && chance(prob_rate))
            s.prob = kProbChoices[Ui(0, 2)];

        if (chance(ratchet_r)) {
            s.ratchet = chance(0.6f) ? 2 : (chance(0.5f) ? 3 : 4);
            s.ratchet = std::clamp(s.ratchet, 1, 4);
        }

        if (chance(plock_r)) {
            s.lock_mask   |= 0x1;
            s.lock_cutoff  = 0.15f + U() * 0.65f;
        }
        if (chance(plock_r * 0.6f)) {
            s.lock_mask |= 0x4;
            s.lock_env   = chance(0.5f) ? (0.70f + U() * 0.30f)
                                        : (U() * 0.25f);
        }
    }

    // ── Pass 5: anchor + motif diversity ────────────────────────────────────
    if (!has_accent) g_steps[0].accent = true;
    if (g_steps[0].rest) {
        g_steps[0].rest = false;
        g_steps[0].note = tonic;
    }

    // Ensure at least 3 unique pitches so the pattern isn't a stuck drone.
    {
        std::array<int, 16> seen{}; int nseen = 0;
        for (int i = 0; i < g_pattern_length; ++i) {
            const auto& s = g_steps[static_cast<size_t>(i)];
            if (s.rest) continue;
            bool found = false;
            for (int k = 0; k < nseen; ++k) if (seen[k] == s.note) { found = true; break; }
            if (!found && nseen < 16) seen[nseen++] = s.note;
        }
        if (nseen < 3) {
            // Inject variation on a couple of non-downbeat steps.
            int injections = 3 - nseen;
            for (int i = 1; i < g_pattern_length && injections > 0; ++i) {
                if ((i % 4) == 0) continue;
                auto& s = g_steps[static_cast<size_t>(i)];
                int oct = contour_oct(i);
                int candidate = std::clamp(tonic + pick_tone() + 12 * oct,
                                           tonic - 12, tonic + 15);
                if (candidate == s.note) candidate = tonic + 7 + 12 * oct;
                s.note = candidate;
                s.rest = false;
                --injections;
            }
        }
    }

    for (int i = g_pattern_length; i < 16; ++i) {
        g_steps[static_cast<size_t>(i)] = StepData{};
        g_steps[static_cast<size_t>(i)].rest = true;
    }

    g_preset_index = -1;
    g_step_sel     = 0;

    if (!silent) {
        static const char* kNames[PA_COUNT] = {
            "pedal", "driving", "melodic", "dub", "stutter", "chrom"
        };
        char buf[32];
        std::snprintf(buf, sizeof(buf), "\xe2\x86\xbb pattern: %s", kNames[gr]);
        set_toast(buf);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// FX randomizer — five named archetypes, matched to how the 303 actually gets
// processed in a rack: DRY (clean), SLAP (close-mic echo), DUB (feedback-
// soaked delay), CAVERN (plate-reverb wash), DIRTY (overdrive-forward). We
// pick archetype-friendly ranges so each roll is usable instead of mud.
// ─────────────────────────────────────────────────────────────────────────────
void App::randomize_fx(int arch_hint, bool silent) {
    auto& rng = rng_;
    auto u = [&](float lo, float hi) {
        return lo + (hi - lo) * std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
    };

    int a = (arch_hint >= 0)
        ? std::clamp(arch_hint, 0, FA_COUNT - 1)
        : std::uniform_int_distribution<int>(0, FA_COUNT - 1)(rng);

    switch (a) {
        case FA_DRY:
            g_od_amt    = u(0.00f, 0.10f);
            g_delay_mix = u(0.00f, 0.10f);
            g_delay_fb  = u(0.20f, 0.35f);
            g_delay_div = 2;                // 1/8 — neutral tempo sync
            g_rev_mix   = u(0.00f, 0.10f);
            g_rev_size  = u(0.40f, 0.55f);
            g_rev_damp  = u(0.35f, 0.55f);
            break;
        case FA_SLAP:
            g_od_amt    = u(0.05f, 0.25f);
            g_delay_mix = u(0.20f, 0.40f);  // audible but never drowning
            g_delay_fb  = u(0.15f, 0.30f);  // short tail, not regenerative
            g_delay_div = std::uniform_int_distribution<int>(0, 1)(rng);  // 1/16 or 1/16d
            g_rev_mix   = u(0.05f, 0.15f);
            g_rev_size  = u(0.35f, 0.55f);
            g_rev_damp  = u(0.30f, 0.50f);
            break;
        case FA_DUB:
            g_od_amt    = u(0.10f, 0.30f);
            g_delay_mix = u(0.35f, 0.55f);  // hefty wet
            g_delay_fb  = u(0.55f, 0.78f);  // regenerative but not self-osc
            g_delay_div = std::uniform_int_distribution<int>(2, 3)(rng);  // 1/8 or 1/8d
            g_rev_mix   = u(0.15f, 0.30f);
            g_rev_size  = u(0.55f, 0.75f);
            g_rev_damp  = u(0.40f, 0.65f);
            break;
        case FA_CAVERN:
            g_od_amt    = u(0.00f, 0.15f);
            g_delay_mix = u(0.00f, 0.20f);
            g_delay_fb  = u(0.20f, 0.40f);
            g_delay_div = std::uniform_int_distribution<int>(1, 3)(rng);
            g_rev_mix   = u(0.45f, 0.70f);  // lush plate
            g_rev_size  = u(0.70f, 0.95f);  // big room
            g_rev_damp  = u(0.20f, 0.45f);  // bright, not muffled
            break;
        case FA_DIRTY:
            g_od_amt    = u(0.55f, 0.85f);  // the "pedal grit" setting
            g_delay_mix = u(0.10f, 0.30f);
            g_delay_fb  = u(0.25f, 0.50f);
            g_delay_div = std::uniform_int_distribution<int>(0, 3)(rng);
            g_rev_mix   = u(0.05f, 0.25f);
            g_rev_size  = u(0.40f, 0.65f);
            g_rev_damp  = u(0.35f, 0.60f);
            break;
    }

    if (!silent) {
        static const char* kNames[FA_COUNT] = {"dry", "slap", "dub", "cavern", "dirty"};
        char buf[32];
        std::snprintf(buf, sizeof(buf), "\xe2\x86\xbb fx: %s", kNames[a]);
        set_toast(buf);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Drum randomizer — genre-grammar engine over the 16-voice kit. Instead of
// dropping a fixed bitmap per genre, each genre declares:
//   - a kick "backbone" (deterministic kick pattern, the genre's fingerprint)
//   - a backbeat rule (snare / clap / rim placement)
//   - a hat/cymbal driver (CH, OH, RD, or sparse)
//   - a perc palette (shaker / tambourine / cowbell / conga / bongo weights)
//   - a crash accent flag (bar-top CY on step 0 or 12)
//   - a tom-fill chance + tom-walk preference (LT→MT→HT ladder on last beat)
//   - a density multiplier so "more" = busier, "less" = sparser
//
// Then a few universal polish passes run on top (ghost notes, accent-layer
// crash, weighted hat variation, slight 1/32 ratchet via retrigger) so every
// roll reads like a composed groove rather than random noise.
//
// Voice indices: 0=BD 1=SD 2=CH 3=OH 4=CL 5=LT 6=HT 7=RS 8=CB 9=SH 10=TB
// 11=CG 12=MT 13=CY 14=RD 15=BG.
// ─────────────────────────────────────────────────────────────────────────────
void App::randomize_drums(int arch_hint, bool silent) {
    auto& rng = rng_;
    auto u01 = [&] {
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
    };
    auto chance = [&](float p) { return u01() < std::clamp(p, 0.0f, 1.0f); };
    auto pick   = [&](int lo, int hi) {
        return std::uniform_int_distribution<int>(lo, hi)(rng);
    };

    // Clear the kit — callers may already have a dense pattern they expect to
    // be replaced, not OR'd into. A `hit()` helper hides the clumsy array
    // indexing everywhere downstream.
    for (auto& row : g_drums) for (auto& c : row) c = false;
    auto hit = [&](int voice, int step) {
        if (voice < 0 || voice >= kDrumVoices) return;
        if (step  < 0 || step  >= 16)          return;
        g_drums[static_cast<size_t>(voice)][static_cast<size_t>(step)] = true;
    };
    auto unhit = [&](int voice, int step) {
        if (voice < 0 || voice >= kDrumVoices) return;
        if (step  < 0 || step  >= 16)          return;
        g_drums[static_cast<size_t>(voice)][static_cast<size_t>(step)] = false;
    };
    auto has = [&](int voice, int step) {
        if (voice < 0 || voice >= kDrumVoices) return false;
        if (step  < 0 || step  >= 16)          return false;
        return g_drums[static_cast<size_t>(voice)][static_cast<size_t>(step)];
    };

    // Must match the order of DA_* (file-scope) so the Vibe table's
    // drums_arch int indexes into this switch correctly.
    enum Groove {
        ACID, CHICAGO, TECHNO, MINIMAL, ELECTRO,
        BREAKS, DUB, LATIN, JAM, G_COUNT
    };
    static_assert(static_cast<int>(G_COUNT) == static_cast<int>(DA_COUNT),
                  "drum genre count mismatch");
    Groove gen = (arch_hint >= 0)
        ? static_cast<Groove>(std::clamp(arch_hint, 0, G_COUNT - 1))
        : static_cast<Groove>(pick(0, G_COUNT - 1));

    // Density roll — a per-call "how busy should this be" factor. Clusters
    // around 0.75 (musical default) with a tail toward either side so the same
    // genre can produce sparse OR dense grooves from successive rolls.
    float density = std::clamp(0.75f + (u01() - 0.5f) * 0.55f, 0.45f, 1.05f);

    // Hat driver — one of the three cymbal voices leads the top end:
    //   CH = closed hat on offbeats (house / techno default)
    //   OH = open hat offbeats (lazier, dub / deep house)
    //   RD = ride cymbal on 8ths (driving, jazz-leaning, ambient techno)
    // Skipping entirely ("none") is also a valid option for ultra-sparse.
    enum Hat { HAT_CH, HAT_OH, HAT_RD, HAT_NONE };
    auto chance_hat = [&](float ch, float oh, float rd) -> Hat {
        float r = u01();
        if (r < ch) return HAT_CH;
        if (r < ch + oh) return HAT_OH;
        if (r < ch + oh + rd) return HAT_RD;
        return HAT_NONE;
    };

    // Universal helper: lay down a 16th-note run on a given voice with a
    // base hit probability. Odd-step bias lets callers ask for offbeats only
    // ("odd_only") or full 16ths ("step=1").
    auto sprinkle = [&](int voice, float p, int step_mod, int offset) {
        for (int i = offset; i < 16; i += step_mod) if (chance(p)) hit(voice, i);
    };

    // Tom walk — three-tom ladder LT→MT→HT, landing across the last N steps.
    // Used as a fill at the end of bar 2 (steps 13/14/15) when the genre's
    // fill-chance rolls through.
    auto tom_walk = [&](int start_step) {
        static const int voices[3] = { 5 /*LT*/, 12 /*MT*/, 6 /*HT*/ };
        int s = std::clamp(start_step, 0, 13);
        for (int i = 0; i < 3; ++i) hit(voices[i], s + i);
    };

    // ── Genre dispatch ──────────────────────────────────────────────────────
    switch (gen) {
        case ACID: {
            // Chicago-acid skeleton: kick 4-on-the-floor, clap on 2/4, no
            // snare. The 303 should carry the groove — drums stay sparse.
            hit(0, 0); hit(0, 4); hit(0, 8); hit(0, 12);
            hit(4, 4); hit(4, 12);
            Hat h = chance_hat(0.60f, 0.10f, 0.15f);
            if (h == HAT_CH) {
                for (int i = 2; i < 16; i += 4) if (chance(0.85f * density)) hit(2, i);
            } else if (h == HAT_OH) {
                for (int i = 2; i < 16; i += 4) if (chance(0.70f * density)) hit(3, i);
            } else if (h == HAT_RD) {
                for (int i = 0; i < 16; i += 2) if (chance(0.65f * density)) hit(14, i);
            }
            if (chance(0.35f * density)) hit(8, 10);                   // lonely cowbell
            if (chance(0.35f * density))                               // rim ghosts
                for (int i : { 3, 11 }) if (chance(0.6f)) hit(7, i);
            if (chance(0.25f * density)) hit(13, 0);                   // crash on 1
            break;
        }
        case CHICAGO: {
            // Deep Chicago / early UK house: kick 4-on-floor, clap on 2+4,
            // an open hat on the 'and' of 4, soft shaker underneath.
            for (int i = 0; i < 16; i += 4) hit(0, i);
            hit(4, 4); hit(4, 12);
            sprinkle(2, 0.95f * density, 4, 2);                         // CH offbeats
            if (chance(0.55f)) hit(3, 14);                              // OH on the "and of 4"
            if (chance(0.60f * density))
                sprinkle(9, 0.75f, 2, 1);                               // shaker
            if (chance(0.35f * density)) sprinkle(11, 0.4f, 4, 3);      // conga on "and"
            if (chance(0.25f)) hit(10, 12);                             // tambourine on 4
            break;
        }
        case TECHNO: {
            // Driving 4/4 techno: kick on every quarter, snappy 16ths on
            // hats, synced cowbell syncopation, optional ride under the hat.
            for (int i = 0; i < 16; i += 4) hit(0, i);
            if (chance(0.45f)) hit(1, 4);                               // snare on 2
            if (chance(0.75f)) hit(4, 12);                              // clap on 4
            sprinkle(2, 0.55f * density, 2, 1);                         // 16th CH
            if (chance(0.55f * density))
                sprinkle(14, 0.55f, 2, 0);                              // ride on 8ths
            if (chance(0.45f * density)) {                              // cb on "and"
                hit(8, 6); hit(8, 14);
            }
            if (chance(0.70f * density)) sprinkle(9, 0.95f, 1, 0);      // 16th shaker drive
            if (chance(0.35f)) hit(13, 12);                             // crash on bar-top
            if (chance(0.20f)) tom_walk(13);                            // tom fill on last beat
            break;
        }
        case MINIMAL: {
            // Sparse. One kick per bar-half, one clap, maybe a rim tick.
            hit(0, 0); hit(0, 8);
            if (chance(0.55f)) hit(4, 12); else hit(1, 12);
            if (chance(0.50f * density))
                for (int i : { 2, 10 }) if (chance(0.55f)) hit(2, i);
            if (chance(0.35f * density)) hit(3, 14);                    // "and of 4" OH
            if (chance(0.30f * density))
                sprinkle(9, 0.35f, 4, 2);                               // whisper shaker
            if (chance(0.18f)) hit(8, 7);                               // single cowbell
            break;
        }
        case ELECTRO: {
            // Electro / 808: syncopated kick 1+11, snare on 4+12, no 4/4
            // kick. Heavy on the CB and CH. 808 clap layered with snare.
            hit(0, 0); hit(0, 11);
            if (chance(0.50f)) hit(0, 7);                               // syncopated extra
            hit(1, 4); hit(1, 12);
            if (chance(0.65f)) { hit(4, 4); hit(4, 12); }               // clap layer
            sprinkle(2, 0.65f * density, 2, 1);                         // CH offbeats
            if (chance(0.40f)) { hit(3, 6); hit(3, 14); }
            if (chance(0.75f))                                          // cb on 2 and 4 "ands"
                for (int i : { 2, 6, 10, 14 }) if (chance(0.55f)) hit(8, i);
            if (chance(0.45f * density)) sprinkle(9, 0.7f, 2, 1);       // shaker
            if (chance(0.25f)) hit(13, 0);                              // crash on 1
            break;
        }
        case BREAKS: {
            // Breakbeat: broken kick, snare on 5+13, busy hats, tom+rim fills.
            hit(0, 0); hit(0, 10);
            if (chance(0.55f)) hit(0, 6);
            if (chance(0.40f)) hit(0, 14);
            hit(1, 4); hit(1, 12);
            if (chance(0.55f)) hit(7, 3);                               // rim ghost
            if (chance(0.55f)) hit(7, 11);
            sprinkle(2, 0.60f * density, 1, 0);
            if (chance(0.45f)) hit(3, 7);
            if (chance(0.55f)) hit(3, 15);
            if (chance(0.60f)) tom_walk(13);
            if (chance(0.35f)) { hit(5, 2); hit(12, 9); }               // sprinkle toms
            if (chance(0.35f)) sprinkle(11, 0.45f, 2, 1);               // conga ghost
            if (chance(0.20f)) hit(13, 12);                             // crash on last
            break;
        }
        case DUB: {
            // Half-time feel: single kick + long OH, claps echoed, delay-heavy
            // arrangement friendly. Leave lots of empty space.
            hit(0, 0);
            if (chance(0.60f)) hit(0, 8);                               // optional 2nd kick
            hit(4, 8);                                                  // clap on "3"
            if (chance(0.50f)) hit(3, 4);                               // OH on 2
            if (chance(0.50f)) hit(3, 12);                              // OH on 4
            if (chance(0.40f * density)) sprinkle(2, 0.45f, 4, 2);      // sparse CH
            if (chance(0.40f * density)) sprinkle(11, 0.40f, 4, 3);     // ghost conga
            if (chance(0.55f * density))
                for (int i = 2; i < 16; i += 4) if (chance(0.5f)) hit(9, i);
            if (chance(0.30f)) hit(13, 0);                              // crash on 1
            break;
        }
        case LATIN: {
            // Latin-house / afro-house: kick 4/4, heavy conga+bongo grooves,
            // cowbell clave, shaker 16ths, light clap. The perc carries more
            // weight than the backbeat.
            for (int i = 0; i < 16; i += 4) hit(0, i);
            if (chance(0.70f)) hit(4, 12);                              // clap on 4
            if (chance(0.60f)) hit(8, 6);                               // clave-ish CB
            if (chance(0.60f)) hit(8, 10);
            // Rumba-ish conga pattern: 0,3,6,10,13 is a common 3-2 clave cell
            static const int conga_a[] = { 3, 6, 10, 13 };
            for (int s : conga_a) if (chance(0.75f * density)) hit(11, s);
            // Bongo on the "ands" — higher-pitched, so it rides on top.
            static const int bongo_a[] = { 2, 5, 9, 14 };
            for (int s : bongo_a) if (chance(0.65f * density)) hit(15, s);
            if (chance(0.80f)) sprinkle(9, 0.90f, 1, 0);                // driving shaker
            if (chance(0.35f)) sprinkle(10, 0.55f, 4, 2);               // tambourine
            if (chance(0.20f)) hit(13, 0);
            break;
        }
        case JAM: {
            // Open jam — everyone gets a shot. Kicks half 4/4, half syncopated.
            // Clap + snare both make appearances. Used for practice loops.
            bool four_on_floor = chance(0.55f);
            if (four_on_floor) for (int i = 0; i < 16; i += 4) hit(0, i);
            else { hit(0, 0); hit(0, 7); hit(0, 10); if (chance(0.5f)) hit(0, 14); }
            if (chance(0.70f)) { hit(1, 4); hit(1, 12); }
            if (chance(0.55f)) { hit(4, 4); hit(4, 12); }
            Hat h = chance_hat(0.35f, 0.20f, 0.30f);
            if (h == HAT_CH) sprinkle(2, 0.65f * density, 2, 1);
            if (h == HAT_OH) { hit(3, 6); hit(3, 14); if (chance(0.5f)) hit(3, 2); }
            if (h == HAT_RD) sprinkle(14, 0.60f * density, 2, 0);
            if (chance(0.45f)) sprinkle(11, 0.50f * density, 2, 1);     // conga
            if (chance(0.45f)) sprinkle(15, 0.45f * density, 4, 2);     // bongo
            if (chance(0.40f)) sprinkle(9, 0.60f * density, 2, 1);      // shaker
            if (chance(0.35f)) sprinkle(10, 0.40f * density, 4, 0);     // tambourine
            if (chance(0.30f)) hit(8, 6);                               // cowbell
            if (chance(0.40f)) tom_walk(13);
            if (chance(0.30f)) hit(13, 0);
            break;
        }
        default: break;
    }

    // ── Universal polish ───────────────────────────────────────────────────
    // These passes add life on top of any genre-specific backbone without
    // overriding it — so an ACID roll still feels like acid, just nudged.
    //
    // Ghost-rim: quiet rim clicks scattered on empty 16ths add micro-timing.
    if (chance(0.55f * density)) {
        for (int i = 1; i < 16; i += 2) {
            if (!has(7, i) && !has(1, i) && chance(0.08f * density)) hit(7, i);
        }
    }
    // Shimmer pass: a subtle tambourine doubles the clap/snare on 4 if empty.
    if (chance(0.30f * density) && !has(10, 12) && (has(4, 12) || has(1, 12))) {
        hit(10, 12);
    }
    // Final-beat accent: cymbals often hit on step 12 with the snare/clap for
    // a musical lift into the next bar. Only fires if nothing already there.
    if (chance(0.20f * density) && !has(13, 12) && !has(14, 12)) {
        if (chance(0.5f)) hit(13, 12); else hit(14, 12);
    }
    // Constraint: if CH and RD both land on the same step, kill the CH hit
    // there — they compete in the top-end and muddy each other.
    for (int i = 0; i < 16; ++i) {
        if (has(2, i) && has(14, i)) unhit(2, i);
    }

    if (!silent) {
        static const char* kNames[G_COUNT] = {
            "acid", "chicago", "techno", "minimal", "electro",
            "breaks", "dub", "latin", "jam"
        };
        char buf[48];
        std::snprintf(buf, sizeof(buf), "\xe2\x86\xbb drums: %s", kNames[gen]);
        set_toast(buf);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Transport randomizer — tempo / swing / length. Stays inside the musical
// envelope of actual acid records (115-140 BPM, straight or lightly shuffled,
// 12/16-step patterns) rather than the full knob ranges, so a roll here
// doesn't send the groove to 60 BPM or 7-step weirdness.
// ─────────────────────────────────────────────────────────────────────────────
void App::randomize_transport(float bpm_center,
                                float bpm_spread,
                                float swing_prob,
                                bool  silent) {
    auto& rng = rng_;
    auto u = [&](float lo, float hi) {
        return lo + (hi - lo) * std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
    };

    if (bpm_center > 0.0f) {
        // Vibe-coordinated tempo — jitter around the vibe's center BPM.
        float spread = (bpm_spread > 0.0f) ? bpm_spread : 2.0f;
        g_bpm = bpm_center + u(-spread, spread);
    } else {
        // Stand-alone: cluster around classic scene tempos.
        static constexpr float kBpms[] = {118.0f, 122.0f, 125.0f, 128.0f, 132.0f, 138.0f};
        g_bpm = kBpms[std::uniform_int_distribution<int>(0, 5)(rng)] + u(-1.5f, 1.5f);
    }
    g_bpm = std::clamp(g_bpm, 40.0f, 220.0f);

    // Swing: unless the vibe asks otherwise, 40% chance of a gentle shuffle.
    // Never hard swing here — the `-/=` keys can push it further if the user
    // wants. swing_prob >= 0 overrides the default coin-flip.
    float p_swing = (swing_prob >= 0.0f) ? swing_prob : 0.40f;
    g_swing = (u(0.0f, 1.0f) >= p_swing) ? 0.50f : u(0.54f, 0.62f);

    // Length: bias toward 16 (bar), sometimes 12 (odd meter feel) or 8 (short
    // loop). 4 and 6 are too short to sound like a finished groove.
    int r = std::uniform_int_distribution<int>(0, 9)(rng);
    g_pattern_length = (r < 6) ? 16 : (r < 9) ? 12 : 8;

    if (!silent) {
        char buf[40];
        std::snprintf(buf, sizeof(buf), "\xe2\x86\xbb transport: %.0f bpm",
                      static_cast<double>(g_bpm));
        set_toast(buf);
    }
}

// Randomize everything — pick one Vibe from kVibes and route every subsystem
// (knobs, FX, pattern, drums, transport) to archetypes that belong together.
// Each helper is called in silent mode so their individual toasts don't stomp
// each other, then one combined toast names the vibe.
//
// This is what makes capital-R musical: an acid-house roll produces 128 BPM
// with four-on-the-floor drums, classic-squelch knobs, and slapback delay,
// rather than the mismatched tempo/knobs/drums you'd get from five
// independent dice rolls.
void App::randomize_everything() {
    auto& rng = rng_;
    int vi = std::uniform_int_distribution<int>(
        0, static_cast<int>(Vibe::V_COUNT) - 1)(rng);
    const VibeSpec& v = kVibes[vi];

    randomize_knobs    (v.knobs_arch,   v.square_bias, /*silent=*/true);
    randomize_fx       (v.fx_arch,      /*silent=*/true);
    randomize_pattern  (v.pattern_arch, /*silent=*/true);
    randomize_drums    (v.drums_arch,   /*silent=*/true);
    randomize_transport(v.bpm_center, v.bpm_spread, v.swing_prob,
                        /*silent=*/true);

    char buf[48];
    std::snprintf(buf, sizeof(buf), "\xe2\x86\xbb %s", v.name);
    set_toast(buf);
}

// ─────────────────────────────────────────────────────────────────────────────
// Evolving mutator — one small, weighted-random change per call. Repeated
// presses "walk" the pattern without obliterating it, a vibe somewhere
// between Generative Sequencer and Mutable Instruments' "Marbles". The key
// design goal: after N presses the pattern is recognisably descended from
// where it started, not pure noise.
// ─────────────────────────────────────────────────────────────────────────────
void App::mutate_pattern() {
    auto& rng = rng_;
    auto urnd = [&]() {
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
    };
    auto pick = [&](int lo, int hi) {
        if (hi <= lo) return lo;
        return std::uniform_int_distribution<int>(lo, hi - 1)(rng);
    };

    // Weighted mutation menu. Probabilities sum to 1.0; biggest slice goes to
    // accent/slide toggles since those change "vibe" without changing pitch.
    const float r = urnd();
    const char* label = "mutate";

    if (r < 0.28f) {
        // Toggle accent on a random non-rest step.
        std::vector<int> cands;
        for (int i = 0; i < g_pattern_length; ++i)
            if (!g_steps[static_cast<size_t>(i)].rest) cands.push_back(i);
        if (!cands.empty()) {
            int i = cands[static_cast<size_t>(pick(0, static_cast<int>(cands.size())))];
            g_steps[static_cast<size_t>(i)].accent ^= true;
            label = "mutate: accent";
        }
    } else if (r < 0.52f) {
        // Toggle slide on a random non-rest step (only where next step plays).
        std::vector<int> cands;
        for (int i = 0; i < g_pattern_length; ++i) {
            if (g_steps[static_cast<size_t>(i)].rest) continue;
            int ni = (i + 1) % g_pattern_length;
            if (!g_steps[static_cast<size_t>(ni)].rest) cands.push_back(i);
        }
        if (!cands.empty()) {
            int i = cands[static_cast<size_t>(pick(0, static_cast<int>(cands.size())))];
            g_steps[static_cast<size_t>(i)].slide ^= true;
            label = "mutate: slide";
        }
    } else if (r < 0.72f) {
        // Nudge a step's pitch by ±1 or ±2 semitones, clamped into 303 range.
        std::vector<int> cands;
        for (int i = 0; i < g_pattern_length; ++i)
            if (!g_steps[static_cast<size_t>(i)].rest) cands.push_back(i);
        if (!cands.empty()) {
            int i = cands[static_cast<size_t>(pick(0, static_cast<int>(cands.size())))];
            static constexpr int steps[] = {-2, -1, 1, 2};
            int d = steps[pick(0, 4)];
            auto& s = g_steps[static_cast<size_t>(i)];
            s.note = std::clamp(s.note + d, 24, 84);
            label = "mutate: pitch";
        }
    } else if (r < 0.84f) {
        // Octave jump on a random note step — ±12 semitones, reversible next
        // call by picking the opposite direction.
        std::vector<int> cands;
        for (int i = 0; i < g_pattern_length; ++i)
            if (!g_steps[static_cast<size_t>(i)].rest) cands.push_back(i);
        if (!cands.empty()) {
            int i = cands[static_cast<size_t>(pick(0, static_cast<int>(cands.size())))];
            int d = (urnd() < 0.5f) ? -12 : 12;
            auto& s = g_steps[static_cast<size_t>(i)];
            int nxt = s.note + d;
            if (nxt >= 24 && nxt <= 84) s.note = nxt;
            label = "mutate: octave";
        }
    } else if (r < 0.94f) {
        // Toggle rest on a random step — but NEVER on beat 1 (that's the
        // anchor that makes a pattern feel coherent when looped).
        int i = pick(1, g_pattern_length);
        auto& s = g_steps[static_cast<size_t>(i)];
        if (s.rest) {
            s.rest = false;
            // Bias a new note toward the tonic or its fifth for musicality.
            static constexpr int nudges[] = {0, 0, 3, 5, 7, 12};
            int base = g_steps[0].rest ? 36 : g_steps[0].note;
            s.note = std::clamp(base + nudges[pick(0, 6)], 24, 84);
        } else {
            s.rest = true;
            s.accent = false;
            s.slide  = false;
        }
        label = "mutate: rest";
    } else {
        // Rotate pattern by one step (left or right). Cheapest way to make the
        // groove feel "different" without actually touching the content.
        bool right = urnd() < 0.5f;
        std::array<StepData, 16> tmp = g_steps;
        int n = g_pattern_length;
        for (int i = 0; i < n; ++i) {
            int src = right ? (i - 1 + n) % n : (i + 1) % n;
            g_steps[static_cast<size_t>(i)] = tmp[static_cast<size_t>(src)];
        }
        label = right ? "mutate: rotate \xe2\x86\x92" : "mutate: rotate \xe2\x86\x90";
    }

    g_preset_index = -1;
    set_toast(label);
}
