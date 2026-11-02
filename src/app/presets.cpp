// Built-in presets and the randomizer's "vibes" tables.
#include "app.hpp"

namespace {

// Helpers for compact preset construction. Notes are MIDI numbers.
//   NN = rest, otherwise pitch; flag chars: 'a' accent, 's' slide.
static constexpr int _ = -1;  // rest sentinel

static StepData sd(int note, const char* flags = "") {
    StepData s;
    if (note < 0) {
        s.rest = true;
    } else {
        s.rest   = false;
        s.note   = note;
        for (const char* p = flags; *p; ++p) {
            if (*p == 'a') s.accent = true;
            if (*p == 's') s.slide  = true;
        }
    }
    return s;
}

// Acid patterns inspired by iconic TB-303 tracks. Precise sequencer data for
// these records isn't publicly documented — these are community-consensus
// reconstructions (Attack Mag tutorials, KVR / Gearspace mega-threads, Robin
// Whittle's notes at firstpr.com.au). They're "plausible representations"
// that evoke each track rather than forensic reproductions.
//
// MIDI notes used below: E1=28, G1=31, A1=33, B1=35, C2=36, D2=38, Eb2=39,
// E2=40, F2=41, G2=43, A2=45, C3=48, Eb3=51.

}  // namespace

const std::array<Preset, 16> g_presets = {{
    // 1. Phuture — "Acid Tracks" (1987). The founding acid record. Pedal
    //    point on C with two octave-ups on accented slides. The song's real
    //    identity is Pierre's live cutoff sweep — the notes are almost
    //    incidental.
    {"Acid Tracks", {{
        sd(36, "a"), sd(36),      sd(36),      sd(36, "as"),
        sd(48),      sd(36),      sd(36, "a"), sd(36),
        sd(36),      sd(36, "s"), sd(48, "a"), sd(36),
        sd(36),      sd(36, "a"), sd(39, "s"), sd(36),
    }}},
    // 2. Josh Wink — "Higher State of Consciousness". Pattern is deliberately
    //    simple; the song is the slow rising cutoff across many bars.
    {"Higher State", {{
        sd(36, "a"), sd(36),      sd(48, "s"), sd(36),
        sd(36, "a"), sd(36),      sd(39, "s"), sd(36),
        sd(36, "a"), sd(36),      sd(48, "s"), sd(36),
        sd(36, "a"), sd(43),      sd(39, "s"), sd(36),
    }}},
    // 3. Hardfloor — "Acperience 1" style. A-minor, heavy accents, octave-
    //    plus slides back to root produce the signature Hardfloor scream.
    {"Acperience", {{
        sd(33, "a"), sd(_),       sd(45, "s"), sd(33),
        sd(33, "a"), sd(36),      sd(40, "s"), sd(33),
        sd(33, "a"), sd(_),       sd(45, "s"), sd(43),
        sd(40, "a"), sd(38),      sd(36, "s"), sd(33),
    }}},
    // 4. Adonis — "No Way Back". Early Chicago acid. Call-and-response
    //    between the C root and upper Eb/F — one of the first 303 lines
    //    with a real riff shape rather than pure pedal tone.
    {"No Way Back", {{
        sd(36, "a"), sd(_),       sd(36),      sd(39, "s"),
        sd(36),      sd(_),       sd(36, "a"), sd(31),
        sd(36),      sd(_),       sd(36),      sd(39, "s"),
        sd(41, "a"), sd(39),      sd(36),      sd(31),
    }}},
    // 5. A Guy Called Gerald — "Voodoo Ray" (actually an MC-202, but the
    //    line translates). A minor pentatonic, open filter, "sung" rather
    //    than squelchy — recommend lowering RES when loading this one.
    {"Voodoo Ray", {{
        sd(33),      sd(_),       sd(45, "a"), sd(_),
        sd(40, "s"), sd(_),       sd(33),      sd(_),
        sd(33),      sd(_),       sd(45, "a"), sd(_),
        sd(43, "s"), sd(40),      sd(33),      sd(_),
    }}},
    // 6. Joey Beltram — "Energy Flash" style. Relentless 16ths on the E
    //    root — a rave engine, not a melody.
    {"Energy Flash", {{
        sd(28, "a"), sd(28),      sd(28),      sd(28, "a"),
        sd(28),      sd(28, "s"), sd(35),      sd(28),
        sd(28, "a"), sd(28),      sd(28),      sd(31, "s"),
        sd(28, "a"), sd(28),      sd(28),      sd(28),
    }}},
    // 7. Plastikman (Richie Hawtin). Sparse, high-resonance, long-decay
    //    minimalism — space and filter movement carry the groove.
    {"Plastikman", {{
        sd(36, "a"), sd(_),       sd(_),       sd(36),
        sd(_),       sd(36, "s"), sd(_),       sd(39, "a"),
        sd(_),       sd(36),      sd(_),       sd(_),
        sd(36, "a"), sd(_),       sd(39, "s"), sd(_),
    }}},
    // 8. Hoover / dub-techno — extremely sparse, long decay, subsonic.
    //    Filter movement over many bars is the performance.
    {"Hoover Dub", {{
        sd(36, "a"), sd(_),       sd(_),       sd(_),
        sd(_),       sd(_),       sd(_),       sd(_),
        sd(36, "a"), sd(_),       sd(_),       sd(31, "s"),
        sd(_),       sd(_),       sd(_),       sd(_),
    }}},
    // 9. Squelch — heavy-resonance showcase, single-octave jumps to drive
    //    the accent envelope stack.
    {"Squelch", {{
        sd(36, "a"), sd(_),       sd(48, "s"), sd(_),
        sd(36),      sd(_),       sd(48, "a"), sd(_),
        sd(36, "a"), sd(_),       sd(48, "s"), sd(_),
        sd(36),      sd(_),       sd(48, "a"), sd(_),
    }}},
    // 10. Detroit — rolling Underground Resistance bassline, A minor.
    {"Detroit", {{
        sd(33, "a"), sd(_),       sd(33),      sd(_),
        sd(40, "s"), sd(_),       sd(33, "a"), sd(_),
        sd(33),      sd(36, "s"), sd(_),       sd(40),
        sd(33, "a"), sd(_),       sd(31),      sd(33),
    }}},
    // 11. Dub 303 — half-time feel, extremely spacey.
    {"Dub 303", {{
        sd(24, "a"), sd(_),       sd(_),       sd(_),
        sd(_),       sd(_),       sd(36, "s"), sd(_),
        sd(24, "a"), sd(_),       sd(_),       sd(_),
        sd(36),      sd(_),       sd(31, "s"), sd(_),
    }}},
    // 12. Liquid — wall-to-wall slides. With the slide-doesn't-retrigger
    //    envelope fix, this one should now do the "continuously flowing"
    //    acid thing rather than retriggering every step.
    {"Liquid", {{
        sd(36, "a"), sd(43, "s"), sd(48, "s"), sd(43, "s"),
        sd(36),      sd(41, "s"), sd(48, "s"), sd(41, "s"),
        sd(36, "a"), sd(43, "s"), sd(48, "s"), sd(55, "s"),
        sd(48, "s"), sd(43, "s"), sd(41, "s"), sd(36),
    }}},
    // 13. Humanoid / Stakker — busy FSOL-era, octave hops every step.
    {"Humanoid", {{
        sd(36, "a"), sd(41, "s"), sd(48),      sd(41),
        sd(36),      sd(41, "s"), sd(48, "a"), sd(43),
        sd(36, "a"), sd(41, "s"), sd(51),      sd(48),
        sd(43),      sd(41),      sd(48, "s"), sd(36),
    }}},
    // 14. Melodic — longer phrased line, pseudo-arpeggio through a minor.
    {"Melodic", {{
        sd(36, "a"), sd(39),      sd(43),      sd(48, "s"),
        sd(46),      sd(43),      sd(39),      sd(36),
        sd(36, "a"), sd(43, "s"), sd(48),      sd(51),
        sd(48),      sd(43),      sd(39, "s"), sd(36),
    }}},
    // 15. Bassline — UK bassline swing, syncopated.
    {"Bassline", {{
        sd(36, "a"), sd(_),       sd(36),      sd(48, "a"),
        sd(_),       sd(36, "s"), sd(43),      sd(36),
        sd(36, "a"), sd(_),       sd(38),      sd(43, "s"),
        sd(_),       sd(36),      sd(48, "a"), sd(_),
    }}},
    // 16. Blank slate for the user to sketch on.
    {"Empty", {{
        sd(_), sd(_), sd(_), sd(_),  sd(_), sd(_), sd(_), sd(_),
        sd(_), sd(_), sd(_), sd(_),  sd(_), sd(_), sd(_), sd(_),
    }}},
}};

// One row per Vibe. Order matches the Vibe enum 1:1 so we can index by int.
const VibeSpec kVibes[static_cast<int>(Vibe::V_COUNT)] = {
    {"acid house",    KA_CLASSIC, PA_PEDAL,   FA_SLAP,    DA_ACID,    128.0f, 2.0f, 0.30f, false},
    {"deep house",    KA_CLASSIC, PA_MELODIC, FA_DUB,     DA_CHICAGO, 122.0f, 2.5f, 0.55f, false},
    {"techno",        KA_DRIVING, PA_DRIVING, FA_SLAP,    DA_TECHNO,  132.0f, 2.5f, 0.15f, false},
    {"hardfloor",     KA_SQUELCH, PA_MELODIC, FA_DIRTY,   DA_TECHNO,  138.0f, 2.0f, 0.10f, false},
    {"electro",       KA_CLASSIC, PA_DRIVING, FA_SLAP,    DA_ELECTRO, 125.0f, 2.0f, 0.20f, true },
    {"minimal",       KA_DUBBY,   PA_PEDAL,   FA_DRY,     DA_MINIMAL, 128.0f, 1.5f, 0.25f, false},
    {"dub techno",    KA_DUBBY,   PA_DUB,     FA_CAVERN,  DA_DUB,     118.0f, 2.5f, 0.50f, false},
    {"breakbeat",     KA_DRIVING, PA_MELODIC, FA_DIRTY,   DA_BREAKS,  135.0f, 3.0f, 0.20f, false},
    {"latin house",   KA_CLASSIC, PA_PEDAL,   FA_SLAP,    DA_LATIN,   124.0f, 2.0f, 0.55f, false},
    {"ambient acid",  KA_DUBBY,   PA_DUB,     FA_CAVERN,  DA_MINIMAL, 100.0f, 4.0f, 0.20f, false},
};
