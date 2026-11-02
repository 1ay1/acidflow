// JACK driver. libjack is loaded with dlopen at runtime, so the binary has no
// hard dependency on it: if it (or a running server) isn't there we return
// false and the caller falls back to ALSA. Works with PipeWire's JACK shim.
//
// JACK calls our process callback on its own real-time thread with the
// server's buffer size and rate; we render straight into its two ports.

#include "audio_backend.hpp"
#include "engine.hpp"

#include <dlfcn.h>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace {

// The handful of libjack symbols we use, declared by hand so no JACK headers
// are needed to build.
using jack_client_t   = struct _jack_client;
using jack_port_t     = struct _jack_port;
using jack_nframes_t  = uint32_t;
using JackProcessCallback = int (*)(jack_nframes_t, void*);
constexpr unsigned long kJackPortIsOutput   = 0x2;
constexpr unsigned long kJackPortIsPhysical = 0x4;
constexpr int           kJackNoStartServer  = 0x01;

struct Jack {
    void* lib = nullptr;
    jack_client_t* (*client_open)(const char*, int, int*, ...) = nullptr;
    int   (*client_close)(jack_client_t*) = nullptr;
    int   (*set_process_callback)(jack_client_t*, JackProcessCallback, void*) = nullptr;
    jack_port_t* (*port_register)(jack_client_t*, const char*, const char*, unsigned long, unsigned long) = nullptr;
    void* (*port_get_buffer)(jack_port_t*, jack_nframes_t) = nullptr;
    const char* (*port_name)(const jack_port_t*) = nullptr;
    int   (*activate)(jack_client_t*) = nullptr;
    int   (*deactivate)(jack_client_t*) = nullptr;
    jack_nframes_t (*get_sample_rate)(jack_client_t*) = nullptr;
    const char** (*get_ports)(jack_client_t*, const char*, const char*, unsigned long) = nullptr;
    int   (*connect)(jack_client_t*, const char*, const char*) = nullptr;
    void  (*free_)(void*) = nullptr;

    bool load() {
        for (const char* n : {"libjack.so.0", "libjack.so"})
            if ((lib = dlopen(n, RTLD_NOW | RTLD_LOCAL))) break;
        if (!lib) return false;
        auto sym = [&](auto& fn, const char* name) {
            fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(dlsym(lib, name));
            return fn != nullptr;
        };
        bool ok = sym(client_open, "jack_client_open") && sym(client_close, "jack_client_close")
               && sym(set_process_callback, "jack_set_process_callback")
               && sym(port_register, "jack_port_register")
               && sym(port_get_buffer, "jack_port_get_buffer")
               && sym(port_name, "jack_port_name") && sym(activate, "jack_activate")
               && sym(deactivate, "jack_deactivate")
               && sym(get_sample_rate, "jack_get_sample_rate")
               && sym(get_ports, "jack_get_ports") && sym(connect, "jack_connect")
               && sym(free_, "jack_free");
        if (!ok) { dlclose(lib); lib = nullptr; }
        return ok;
    }
};

Jack           g_jack;
jack_client_t* g_client = nullptr;
jack_port_t*   g_port[2] = {};
std::vector<float> g_scratch;   // interleaved render buffer

int process(jack_nframes_t n, void*) {
    auto* l = static_cast<float*>(g_jack.port_get_buffer(g_port[0], n));
    auto* r = static_cast<float*>(g_jack.port_get_buffer(g_port[1], n));
    jack_nframes_t done = 0;
    while (done < n) {
        const int chunk = static_cast<int>(std::min<jack_nframes_t>(n - done, acid::kBufFrames));
        acid::render(g_scratch.data(), chunk);
        for (int i = 0; i < chunk; ++i) {
            l[done + i] = g_scratch[2 * i];
            r[done + i] = g_scratch[2 * i + 1];
        }
        done += static_cast<jack_nframes_t>(chunk);
    }
    return 0;
}

}  // namespace

namespace acid::backend {

bool jack_start() {
    if (g_client) return true;
    if (!g_jack.lib && !g_jack.load()) return false;
    int status = 0;
    g_client = g_jack.client_open("acidflow", kJackNoStartServer, &status);
    if (!g_client) return false;

    acid::set_sample_rate(static_cast<int>(g_jack.get_sample_rate(g_client)));
    g_scratch.assign(static_cast<size_t>(acid::kBufFrames) * acid::kChannels, 0.0f);
    static const char* kAudioType = "32 bit float mono audio";
    g_port[0] = g_jack.port_register(g_client, "out_l", kAudioType, kJackPortIsOutput, 0);
    g_port[1] = g_jack.port_register(g_client, "out_r", kAudioType, kJackPortIsOutput, 0);
    if (!g_port[0] || !g_port[1]
        || g_jack.set_process_callback(g_client, process, nullptr) != 0
        || g_jack.activate(g_client) != 0) {
        g_jack.client_close(g_client);
        g_client = nullptr;
        return false;
    }
    // Auto-connect to the first two physical playback ports.
    if (const char** phys = g_jack.get_ports(g_client, nullptr, kAudioType,
                                             kJackPortIsPhysical | 0x1 /* IsInput */)) {
        for (int c = 0; c < 2 && phys[c]; ++c)
            g_jack.connect(g_client, g_jack.port_name(g_port[c]), phys[c]);
        g_jack.free_(phys);
    }
    return true;
}

void jack_stop() {
    if (!g_client) return;
    g_jack.deactivate(g_client);
    g_jack.client_close(g_client);
    g_client = nullptr;
}

}  // namespace acid::backend
