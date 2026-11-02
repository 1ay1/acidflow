// acidflow as a CLAP instrument. The same engine as the app: notes in (with
// legato = slide and velocity >= 100 = accent, like the 303), stereo out,
// and the voice + FX knobs as automatable parameters.
//
// Each plugin instance owns an acid::Engine and binds the C API to it for
// the duration of a call, so several instances never share state.

#include <clap/clap.h>

#include "audio.hpp"
#include "engine.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace {

// ── Parameters ──────────────────────────────────────────────────────────────
struct ParamDef {
    clap_id     id;
    const char* name;
    const char* module;
    double      min, max, def;
    bool        stepped;
    void (*apply)(double);
};

void set_wave(double v)  { acid_set_waveform(v >= 0.5 ? 1 : 0); }
void set_div(double v)   { acid_set_delay_division(static_cast<int>(std::lround(v))); }
void set_tune(double v)  { acid_set_tuning_semi(static_cast<float>(v)); }
#define F(fn) [](double v) { fn(static_cast<float>(v)); }

const ParamDef kParams[] = {
    {0,  "Tune",       "Osc",    -12, 12,  0.0,  false, set_tune},
    {1,  "Waveform",   "Osc",      0,  1,  0.0,  true,  set_wave},
    {2,  "Cutoff",     "Filter",   0,  1,  0.40, false, F(acid_set_cutoff)},
    {3,  "Resonance",  "Filter",   0,  1,  0.75, false, F(acid_set_resonance)},
    {4,  "Env Mod",    "Filter",   0,  1,  0.65, false, F(acid_set_env_mod)},
    {5,  "Decay",      "Env",      0,  1,  0.30, false, F(acid_set_decay)},
    {6,  "Accent",     "Amp",      0,  1,  0.65, false, F(acid_set_accent_amt)},
    {7,  "Drive",      "Amp",      0,  1,  0.20, false, F(acid_set_drive)},
    {8,  "Volume",     "Amp",      0,  1,  0.65, false, F(acid_set_volume)},
    {9,  "Overdrive",  "FX",       0,  1,  0.0,  false, F(acid_set_od_amt)},
    {10, "Delay Mix",  "FX",       0,  1,  0.0,  false, F(acid_set_delay_mix)},
    {11, "Delay FB",   "FX",       0,  1,  0.35, false, F(acid_set_delay_feedback)},
    {12, "Delay Time", "FX",       0,  3,  2.0,  true,  set_div},
    {13, "Reverb Mix", "FX",       0,  1,  0.0,  false, F(acid_set_rev_mix)},
    {14, "Reverb Size","FX",       0,  1,  0.55, false, F(acid_set_rev_size)},
    {15, "Reverb Damp","FX",       0,  1,  0.35, false, F(acid_set_rev_damp)},
};
#undef F
constexpr uint32_t kNumParams = sizeof(kParams) / sizeof(kParams[0]);

// ── Plugin instance ─────────────────────────────────────────────────────────
struct Plugin {
    clap_plugin_t                  plugin{};
    const clap_host_t*             host = nullptr;
    std::unique_ptr<acid::Engine>  engine;
    std::vector<float>             scratch;
    // Main-thread view of parameter values (also what state save writes).
    std::atomic<double>            values[kNumParams];
    std::atomic<bool>              dirty{true};    // push values on next process
    int                            held = -1;      // most recent held note (mono)
    std::vector<int>               stack;          // held notes, for legato/last-note

    Plugin() { for (uint32_t i = 0; i < kNumParams; ++i) values[i].store(kParams[i].def); }
};

Plugin* self(const clap_plugin_t* p) { return static_cast<Plugin*>(p->plugin_data); }

void push_all(Plugin& s) {
    for (uint32_t i = 0; i < kNumParams; ++i) kParams[i].apply(s.values[i].load());
}

float hz(int note) { return 440.0f * std::pow(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f); }

// Monophonic, last-note priority. A note played while another is held slides.
void note_on(Plugin& s, int key, double vel) {
    const bool legato = !s.stack.empty();
    s.stack.erase(std::remove(s.stack.begin(), s.stack.end(), key), s.stack.end());
    s.stack.push_back(key);
    s.held = key;
    acid_note_on(hz(key), vel >= 100.0 / 127.0 ? 1 : 0, legato ? 1 : 0, 0.0f);
}
void note_off(Plugin& s, int key) {
    s.stack.erase(std::remove(s.stack.begin(), s.stack.end(), key), s.stack.end());
    if (s.stack.empty()) { s.held = -1; acid_note_off(); return; }
    if (s.held == key) {           // fall back to the previous held note, sliding
        s.held = s.stack.back();
        acid_note_on(hz(s.held), 0, 1, 0.0f);
    }
}

void handle_event(Plugin& s, const clap_event_header_t* h) {
    if (h->space_id != CLAP_CORE_EVENT_SPACE_ID) return;
    switch (h->type) {
        case CLAP_EVENT_NOTE_ON: {
            auto* e = reinterpret_cast<const clap_event_note_t*>(h);
            note_on(s, e->key, e->velocity);
            break;
        }
        case CLAP_EVENT_NOTE_OFF:
        case CLAP_EVENT_NOTE_CHOKE: {
            auto* e = reinterpret_cast<const clap_event_note_t*>(h);
            note_off(s, e->key);
            break;
        }
        case CLAP_EVENT_MIDI: {
            auto* e = reinterpret_cast<const clap_event_midi_t*>(h);
            const uint8_t st = e->data[0] & 0xF0;
            if (st == 0x90 && e->data[2] > 0) note_on(s, e->data[1], e->data[2] / 127.0);
            else if (st == 0x80 || st == 0x90) note_off(s, e->data[1]);
            break;
        }
        case CLAP_EVENT_PARAM_VALUE: {
            auto* e = reinterpret_cast<const clap_event_param_value_t*>(h);
            if (e->param_id < kNumParams) {
                const auto& d = kParams[e->param_id];
                const double v = std::clamp(e->value, d.min, d.max);
                s.values[e->param_id].store(v);
                d.apply(v);
            }
            break;
        }
        default: break;
    }
}

// ── clap_plugin ─────────────────────────────────────────────────────────────
bool p_init(const clap_plugin_t*) { return true; }
void p_destroy(const clap_plugin_t* p) { delete self(p); }

bool p_activate(const clap_plugin_t* p, double sr, uint32_t, uint32_t max_frames) {
    auto& s = *self(p);
    s.engine = std::make_unique<acid::Engine>(static_cast<int>(sr));
    s.scratch.assign(static_cast<size_t>(std::max<uint32_t>(max_frames, 1)) * 2, 0.0f);
    s.dirty.store(true);
    return true;
}
void p_deactivate(const clap_plugin_t* p) { self(p)->engine.reset(); }
bool p_start_processing(const clap_plugin_t*) { return true; }
void p_stop_processing(const clap_plugin_t*) {}
void p_reset(const clap_plugin_t* p) {
    auto& s = *self(p);
    if (!s.engine) return;
    acid::Engine::Bind b(*s.engine);
    acid_note_off();
    s.stack.clear();
    s.held = -1;
}

clap_process_status p_process(const clap_plugin_t* p, const clap_process_t* proc) {
    auto& s = *self(p);
    if (!s.engine || proc->audio_outputs_count < 1) return CLAP_PROCESS_ERROR;
    acid::Engine::Bind bind(*s.engine);
    if (s.dirty.exchange(false)) push_all(s);

    const uint32_t n = proc->frames_count;
    if (s.scratch.size() < size_t(n) * 2) s.scratch.resize(size_t(n) * 2);
    float** out = proc->audio_outputs[0].data32;
    const uint32_t chans = proc->audio_outputs[0].channel_count;

    const uint32_t nev = proc->in_events->size(proc->in_events);
    uint32_t ev = 0, done = 0;
    // Render in slices between events so notes and automation land on the
    // exact sample.
    while (done < n) {
        while (ev < nev) {
            const auto* h = proc->in_events->get(proc->in_events, ev);
            if (h->time > done) break;
            handle_event(s, h);
            ++ev;
        }
        uint32_t next = n;
        if (ev < nev) next = std::min(n, proc->in_events->get(proc->in_events, ev)->time);
        const uint32_t len = std::max<uint32_t>(next - done, 1);
        s.engine->render(s.scratch.data(), static_cast<int>(len));
        for (uint32_t i = 0; i < len; ++i) {
            if (chans > 0) out[0][done + i] = s.scratch[2 * i];
            if (chans > 1) out[1][done + i] = s.scratch[2 * i + 1];
        }
        done += len;
    }
    while (ev < nev) handle_event(s, proc->in_events->get(proc->in_events, ev++));
    return CLAP_PROCESS_CONTINUE;
}

// ── Extensions ──────────────────────────────────────────────────────────────
uint32_t ports_count(const clap_plugin_t*, bool is_input) { return is_input ? 0 : 1; }
bool ports_get(const clap_plugin_t*, uint32_t index, bool is_input, clap_audio_port_info_t* info) {
    if (is_input || index != 0) return false;
    info->id = 0;
    std::snprintf(info->name, sizeof(info->name), "Out");
    info->flags = CLAP_AUDIO_PORT_IS_MAIN;
    info->channel_count = 2;
    info->port_type = CLAP_PORT_STEREO;
    info->in_place_pair = CLAP_INVALID_ID;
    return true;
}
const clap_plugin_audio_ports_t kAudioPorts{ports_count, ports_get};

uint32_t nports_count(const clap_plugin_t*, bool is_input) { return is_input ? 1 : 0; }
bool nports_get(const clap_plugin_t*, uint32_t index, bool is_input, clap_note_port_info_t* info) {
    if (!is_input || index != 0) return false;
    info->id = 0;
    info->supported_dialects = CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI;
    info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
    std::snprintf(info->name, sizeof(info->name), "Notes");
    return true;
}
const clap_plugin_note_ports_t kNotePorts{nports_count, nports_get};

uint32_t params_count(const clap_plugin_t*) { return kNumParams; }
bool params_info(const clap_plugin_t*, uint32_t i, clap_param_info_t* info) {
    if (i >= kNumParams) return false;
    const auto& d = kParams[i];
    std::memset(info, 0, sizeof(*info));
    info->id = d.id;
    info->flags = CLAP_PARAM_IS_AUTOMATABLE | (d.stepped ? CLAP_PARAM_IS_STEPPED : 0);
    std::snprintf(info->name, sizeof(info->name), "%s", d.name);
    std::snprintf(info->module, sizeof(info->module), "%s", d.module);
    info->min_value = d.min;
    info->max_value = d.max;
    info->default_value = d.def;
    return true;
}
bool params_value(const clap_plugin_t* p, clap_id id, double* v) {
    if (id >= kNumParams) return false;
    *v = self(p)->values[id].load();
    return true;
}
bool params_to_text(const clap_plugin_t*, clap_id id, double v, char* out, uint32_t cap) {
    if (id >= kNumParams) return false;
    static const char* kDiv[] = {"1/16", "1/16d", "1/8", "1/8d"};
    if (id == 1)       std::snprintf(out, cap, "%s", v >= 0.5 ? "square" : "saw");
    else if (id == 12) std::snprintf(out, cap, "%s", kDiv[std::clamp(int(std::lround(v)), 0, 3)]);
    else if (id == 0)  std::snprintf(out, cap, "%+.1f st", v);
    else if (id == 2)  std::snprintf(out, cap, "%.0f Hz", acid::cutoff_knob_hz(float(v)));
    else               std::snprintf(out, cap, "%.0f%%", v * 100.0);
    return true;
}
bool params_from_text(const clap_plugin_t*, clap_id id, const char* text, double* v) {
    if (id >= kNumParams) return false;
    *v = std::atof(text);
    return true;
}
void params_flush(const clap_plugin_t* p, const clap_input_events_t* in, const clap_output_events_t*) {
    auto& s = *self(p);
    // Called when not processing: apply to the engine if it exists.
    std::unique_ptr<acid::Engine::Bind> b;
    if (s.engine) b = std::make_unique<acid::Engine::Bind>(*s.engine);
    for (uint32_t i = 0, n = in->size(in); i < n; ++i) {
        const auto* h = in->get(in, i);
        if (h->space_id == CLAP_CORE_EVENT_SPACE_ID && h->type == CLAP_EVENT_PARAM_VALUE) {
            auto* e = reinterpret_cast<const clap_event_param_value_t*>(h);
            if (e->param_id < kNumParams) s.values[e->param_id].store(e->value);
        }
    }
    s.dirty.store(true);
}
const clap_plugin_params_t kParamsExt{params_count, params_info, params_value,
                                     params_to_text, params_from_text, params_flush};

// State: a version byte then one double per parameter, in id order.
bool state_save(const clap_plugin_t* p, const clap_ostream_t* os) {
    const uint8_t ver = 1;
    if (os->write(os, &ver, 1) != 1) return false;
    for (uint32_t i = 0; i < kNumParams; ++i) {
        const double v = self(p)->values[i].load();
        if (os->write(os, &v, sizeof v) != sizeof v) return false;
    }
    return true;
}
bool state_load(const clap_plugin_t* p, const clap_istream_t* is) {
    uint8_t ver = 0;
    if (is->read(is, &ver, 1) != 1 || ver != 1) return false;
    for (uint32_t i = 0; i < kNumParams; ++i) {
        double v = 0;
        if (is->read(is, &v, sizeof v) != sizeof v) return false;
        self(p)->values[i].store(std::clamp(v, kParams[i].min, kParams[i].max));
    }
    self(p)->dirty.store(true);
    return true;
}
const clap_plugin_state_t kState{state_save, state_load};

// The delay and plate ring for a few seconds after the last note.
uint32_t tail_get(const clap_plugin_t* p) {
    return static_cast<uint32_t>((self(p)->engine ? self(p)->engine->sample_rate() : 48000) * 6);
}
const clap_plugin_tail_t kTail{tail_get};

const void* p_get_extension(const clap_plugin_t*, const char* id) {
    if (!std::strcmp(id, CLAP_EXT_AUDIO_PORTS)) return &kAudioPorts;
    if (!std::strcmp(id, CLAP_EXT_NOTE_PORTS))  return &kNotePorts;
    if (!std::strcmp(id, CLAP_EXT_PARAMS))      return &kParamsExt;
    if (!std::strcmp(id, CLAP_EXT_STATE))       return &kState;
    if (!std::strcmp(id, CLAP_EXT_TAIL))        return &kTail;
    return nullptr;
}
void p_on_main_thread(const clap_plugin_t*) {}

// ── Factory / entry ─────────────────────────────────────────────────────────
const char* const kFeatures[] = {CLAP_PLUGIN_FEATURE_INSTRUMENT, CLAP_PLUGIN_FEATURE_SYNTHESIZER,
                                 CLAP_PLUGIN_FEATURE_MONO, CLAP_PLUGIN_FEATURE_STEREO, nullptr};
const clap_plugin_descriptor_t kDesc = {
    CLAP_VERSION_INIT, "com.acidflow.tb303", "acidflow 303", "acidflow",
    "https://github.com/1ay1/acidflow", "", "", "0.2.0",
    "TB-303 style bass synth: diode-ladder filter, tape delay, plate reverb", kFeatures,
};

uint32_t f_count(const clap_plugin_factory_t*) { return 1; }
const clap_plugin_descriptor_t* f_desc(const clap_plugin_factory_t*, uint32_t i) {
    return i == 0 ? &kDesc : nullptr;
}
const clap_plugin_t* f_create(const clap_plugin_factory_t*, const clap_host_t* host, const char* id) {
    if (!clap_version_is_compatible(host->clap_version) || std::strcmp(id, kDesc.id) != 0)
        return nullptr;
    auto* s = new Plugin;
    s->host = host;
    s->plugin.desc = &kDesc;
    s->plugin.plugin_data = s;
    s->plugin.init = p_init;
    s->plugin.destroy = p_destroy;
    s->plugin.activate = p_activate;
    s->plugin.deactivate = p_deactivate;
    s->plugin.start_processing = p_start_processing;
    s->plugin.stop_processing = p_stop_processing;
    s->plugin.reset = p_reset;
    s->plugin.process = p_process;
    s->plugin.get_extension = p_get_extension;
    s->plugin.on_main_thread = p_on_main_thread;
    return &s->plugin;
}
const clap_plugin_factory_t kFactory{f_count, f_desc, f_create};

bool e_init(const char*) { return true; }
void e_deinit() {}
const void* e_get_factory(const char* id) {
    return std::strcmp(id, CLAP_PLUGIN_FACTORY_ID) == 0 ? &kFactory : nullptr;
}

}  // namespace

extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    CLAP_VERSION_INIT, e_init, e_deinit, e_get_factory,
};
