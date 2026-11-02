// Two engines join one Link session over loopback: the second one's
// sequencer should lock to the first one's tempo and bar phase.
// Built only with -DACIDFLOW_LINK=ON.
#include <ableton/Link.hpp>

#include "audio.hpp"
#include "engine.hpp"
#include "t.hpp"

#include <chrono>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

TEST("acid follows a Link peer's tempo and phase") {
    // The other app in the session: plain Link at 131 BPM.
    ableton::Link peer(131.0);
    peer.enable(true);

    acid_link_enable(1);
    // Wait for discovery (multicast on loopback).
    for (int i = 0; i < 100 && acid_link_peers() == 0; ++i) std::this_thread::sleep_for(50ms);
    REQUIRE(acid_link_peers() > 0);

    acid::set_sample_rate(48000);
    acid_seq_set_pattern_length(16);
    acid_seq_set_bpm(100.0f);   // wrong on purpose; Link should override
    acid_seq_play();
    std::vector<float> buf(2 * 256);
    // Run in real time for a while so the beat clock and ours meet.
    for (int i = 0; i < 300; ++i) {
        acid::render_live(buf.data(), 256);
        std::this_thread::sleep_for(std::chrono::microseconds(5333));
    }
    CHECK_NEAR(acid_current_bpm(), 131.0, 0.01);

    // Compare our step (16ths in a 1-bar pattern) with the peer's phase.
    const auto st = peer.captureAppSessionState();
    const double phase = st.phaseAtTime(peer.clock().micros(), 4.0);   // 0..4 beats
    const int want = static_cast<int>(phase * 4.0) % 16;
    const int have = acid_seq_current_step();
    int d = std::abs(want - have);
    d = std::min(d, 16 - d);
    std::printf("      peer step %d, acid step %d\n", want, have);
    CHECK(d <= 1);
    acid_seq_stop();
    acid_link_enable(0);
}

TEST_MAIN()
