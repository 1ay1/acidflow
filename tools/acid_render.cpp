// acid_render — offline render through the public C API.
//   acid_render out.wav [loops]
// Plays a fixed acid line with drums, delay and reverb on, so every DSP path
// is exercised. Used as a regression reference and by the tests.
#include "audio.hpp"
#include "engine.hpp"

#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: %s out.wav [loops]\n", argv[0]); return 2; }
    const int loops = argc > 2 ? std::atoi(argv[2]) : 2;

    acid::set_sample_rate(44100);
    acid_set_cutoff(0.30f);
    acid_set_resonance(0.85f);
    acid_set_env_mod(0.70f);
    acid_set_decay(0.35f);
    acid_set_accent_amt(0.70f);
    acid_set_drive(0.35f);
    acid_set_volume(0.70f);
    acid_set_tuning_semi(0.0f);
    acid_set_waveform(0);
    acid_set_delay_mix(0.30f);
    acid_set_delay_feedback(0.45f);
    acid_set_delay_division(3);
    acid_set_od_amt(0.25f);
    acid_set_rev_mix(0.25f);
    acid_set_rev_size(0.60f);
    acid_set_rev_damp(0.40f);

    // BD on the beat, CH on the offbeat 8ths, SD on 2 and 4, CL on step 15.
    acid_seq_set_drum_lane(0, 0x1111);
    acid_seq_set_drum_lane(2, 0x4444);
    acid_seq_set_drum_lane(1, 0x1010);
    acid_seq_set_drum_lane(4, 0x4000);
    acid_seq_set_drum_master(0.8f);

    // midi | flags<<8 (rest=1, accent=2, slide=4) | prob<<11 | (ratchet-1)<<18
    auto st = [](int midi, int flags, int ratchet = 1) {
        return (midi & 0xFF) | ((flags & 7) << 8) | (100 << 11) | ((ratchet - 1) << 18);
    };
    const int notes[16] = {
        st(36, 2), st(36, 0), st(48, 4), st(36, 0),
        st(39, 2), st(36, 1), st(43, 4), st(46, 0),
        st(36, 2, 2), st(36, 0), st(48, 2), st(51, 4),
        st(36, 0), st(34, 1), st(36, 2), st(41, 4),
    };
    return acid_render_wav(argv[1], notes, 16, 128.0f, 0.56f, loops) == 0 ? 0 : 1;
}
