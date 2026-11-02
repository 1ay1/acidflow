// Linux acid_start/acid_stop: JACK first (it gives the lowest latency and
// patches into a DAW), ALSA if no JACK server is running.

#include "audio.hpp"
#include "audio_backend.hpp"

#include <cstdlib>
#include <cstring>

namespace {
enum class Driver { None, Jack, Alsa };
Driver g_driver = Driver::None;
}  // namespace

extern "C" {

void acid_start() {
    if (g_driver != Driver::None) return;
    const char* want = std::getenv("ACIDFLOW_AUDIO");
    const bool force_alsa = want && std::strcmp(want, "alsa") == 0;
    const bool force_jack = want && std::strcmp(want, "jack") == 0;
    if (!force_alsa && acid::backend::jack_start()) { g_driver = Driver::Jack; return; }
    if (!force_jack && acid::backend::alsa_start()) { g_driver = Driver::Alsa; return; }
}

void acid_stop() {
    switch (g_driver) {
        case Driver::Jack: acid::backend::jack_stop(); break;
        case Driver::Alsa: acid::backend::alsa_stop(); break;
        case Driver::None: break;
    }
    g_driver = Driver::None;
}

const char* acid_audio_driver(void) {
    switch (g_driver) {
        case Driver::Jack: return "jack";
        case Driver::Alsa: return "alsa";
        case Driver::None: break;
    }
    return "none";
}

}  // extern "C"
