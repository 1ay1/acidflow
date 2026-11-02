// Load acidflow.clap like a host would and play a note through it.
//   clap_test path/to/acidflow.clap
#include <clap/clap.h>

#include <dlfcn.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {
const void* host_ext(const clap_host_t*, const char*) { return nullptr; }
void host_noop(const clap_host_t*) {}
const clap_host_t kHost{CLAP_VERSION_INIT, nullptr, "clap_test", "acidflow", "", "1",
                        host_ext, host_noop, host_noop, host_noop};

struct Events {
    std::vector<clap_event_note_t> notes;
    static uint32_t size(const clap_input_events_t* l) {
        return uint32_t(static_cast<const Events*>(l->ctx)->notes.size());
    }
    static const clap_event_header_t* get(const clap_input_events_t* l, uint32_t i) {
        return &static_cast<const Events*>(l->ctx)->notes[i].header;
    }
};
bool push_out(const clap_output_events_t*, const clap_event_header_t*) { return true; }

int fails = 0;
void check(bool ok, const char* what) {
    std::printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) ++fails;
}
}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    void* lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    check(lib != nullptr, "module loads");
    if (!lib) { std::printf("%s\n", dlerror()); return 1; }
    auto* entry = static_cast<const clap_plugin_entry_t*>(dlsym(lib, "clap_entry"));
    check(entry && entry->init(argv[1]), "clap_entry inits");
    auto* fac = static_cast<const clap_plugin_factory_t*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    check(fac && fac->get_plugin_count(fac) == 1, "factory has one plugin");
    const auto* desc = fac->get_plugin_descriptor(fac, 0);
    const clap_plugin_t* p = fac->create_plugin(fac, &kHost, desc->id);
    check(p && p->init(p), "plugin creates and inits");

    auto* params = static_cast<const clap_plugin_params_t*>(p->get_extension(p, CLAP_EXT_PARAMS));
    check(params && params->count(p) >= 16, "exposes parameters");
    check(p->activate(p, 48000, 1, 512), "activates");
    p->start_processing(p);

    std::vector<float> l(512), r(512);
    float* chans[2] = {l.data(), r.data()};
    clap_audio_buffer_t out{};
    out.data32 = chans;
    out.channel_count = 2;

    Events ev;
    clap_input_events_t in{&ev, Events::size, Events::get};
    clap_output_events_t oe{nullptr, push_out};

    double energy = 0;
    bool finite = true;
    for (int block = 0; block < 100; ++block) {
        ev.notes.clear();
        if (block == 0 || block == 50) {
            clap_event_note_t n{};
            n.header = {sizeof(n), 100, CLAP_CORE_EVENT_SPACE_ID,
                        uint16_t(block == 0 ? CLAP_EVENT_NOTE_ON : CLAP_EVENT_NOTE_OFF), 0};
            n.note_id = -1; n.port_index = 0; n.channel = 0; n.key = 36; n.velocity = 1.0;
            ev.notes.push_back(n);
        }
        clap_process_t proc{};
        proc.steady_time = block * 512;
        proc.frames_count = 512;
        proc.audio_outputs = &out;
        proc.audio_outputs_count = 1;
        proc.in_events = &in;
        proc.out_events = &oe;
        p->process(p, &proc);
        for (int i = 0; i < 512; ++i) {
            finite = finite && std::isfinite(l[i]) && std::isfinite(r[i]);
            if (block < 50) energy += l[i] * l[i];
        }
    }
    check(finite, "output is finite");
    check(energy > 1.0, "a note makes sound");

    // State round trip.
    struct Buf { std::vector<uint8_t> d; size_t rd = 0; } buf;
    clap_ostream_t os{&buf, [](const clap_ostream_t* s, const void* src, uint64_t n) -> int64_t {
        auto* b = static_cast<Buf*>(s->ctx);
        b->d.insert(b->d.end(), static_cast<const uint8_t*>(src), static_cast<const uint8_t*>(src) + n);
        return int64_t(n); }};
    clap_istream_t is{&buf, [](const clap_istream_t* s, void* dst, uint64_t n) -> int64_t {
        auto* b = static_cast<Buf*>(s->ctx);
        n = std::min<uint64_t>(n, b->d.size() - b->rd);
        std::memcpy(dst, b->d.data() + b->rd, n); b->rd += n; return int64_t(n); }};
    auto* state = static_cast<const clap_plugin_state_t*>(p->get_extension(p, CLAP_EXT_STATE));
    check(state && state->save(p, &os) && state->load(p, &is), "state saves and loads");

    p->stop_processing(p);
    p->deactivate(p);
    p->destroy(p);
    entry->deinit();
    std::printf("%s\n", fails ? "FAILED" : "all ok");
    return fails ? 1 : 0;
}
