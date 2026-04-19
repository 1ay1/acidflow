// Windows audio backend — drives `acid::render()` through WASAPI shared mode.
//
// Shared mode is the right default: it routes through the system audio
// engine (same mixer as every other app), doesn't require admin rights,
// and works with whatever device the user has selected as default.
//
// Caveat: in shared mode the client is forced to use the device's current
// mix format. That's almost always 32-bit float, but usually at 48 kHz, not
// our preferred 44.1 kHz. We handle this by telling the engine to render at
// whatever rate the device reports (`acid::set_sample_rate`) and letting
// the DSP recompute its filter/envelope constants accordingly. If the
// device mix format is multi-channel, we upmix the mono render into the
// first two channels and leave the rest silent.

#include "audio.hpp"
#include "engine.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <objbase.h>

#include <atomic>
#include <thread>
#include <vector>

namespace {

IMMDeviceEnumerator* g_enumerator = nullptr;
IMMDevice*           g_device     = nullptr;
IAudioClient*        g_client     = nullptr;
IAudioRenderClient*  g_render     = nullptr;
HANDLE               g_event      = nullptr;
WAVEFORMATEX*        g_mix_fmt    = nullptr;
UINT32               g_buf_frames = 0;
UINT32               g_channels   = 1;
std::thread          g_worker;
std::atomic<bool>    g_run        {false};
bool                 g_com_init   = false;

// Null-safe COM release — IID_PPV_ARGS-style cleanup helper.
template <typename T>
void safe_release(T*& p) { if (p) { p->Release(); p = nullptr; } }

void audio_thread() {
    std::vector<float> mono(acid::kBufFrames);
    g_client->Start();

    while (g_run.load(std::memory_order_relaxed)) {
        // Event-driven pull: WASAPI signals us when it wants more samples.
        // 2 s timeout is an upper bound for "the device is still alive" —
        // shouldn't ever fire in normal operation.
        if (WaitForSingleObject(g_event, 2000) != WAIT_OBJECT_0) continue;

        UINT32 padding = 0;
        if (FAILED(g_client->GetCurrentPadding(&padding))) break;
        UINT32 avail = g_buf_frames - padding;
        if (avail == 0) continue;

        BYTE* data = nullptr;
        if (FAILED(g_render->GetBuffer(avail, &data))) break;
        float* out = reinterpret_cast<float*>(data);

        UINT32 done = 0;
        while (done < avail) {
            UINT32 chunk = std::min<UINT32>(avail - done,
                                            static_cast<UINT32>(acid::kBufFrames));
            acid::render(mono.data(), static_cast<int>(chunk));
            // Splat mono into every channel of the device mix format.
            for (UINT32 i = 0; i < chunk; ++i) {
                float s = mono[i];
                for (UINT32 c = 0; c < g_channels; ++c) {
                    out[(done + i) * g_channels + c] = s;
                }
            }
            done += chunk;
        }

        g_render->ReleaseBuffer(avail, 0);
    }

    g_client->Stop();
}

}  // namespace

extern "C" {

void acid_start() {
    if (g_client) return;

    // COINIT_MULTITHREADED — WASAPI is fine with either; multithreaded means
    // we don't care about message pumps on this thread.
    if (SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
        g_com_init = true;
    }

    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                reinterpret_cast<void**>(&g_enumerator)))) {
        return;
    }
    if (FAILED(g_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &g_device))) {
        return;
    }
    if (FAILED(g_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
                                  nullptr, reinterpret_cast<void**>(&g_client)))) {
        return;
    }
    if (FAILED(g_client->GetMixFormat(&g_mix_fmt))) return;

    // WASAPI shared mode forces the device's mix format. Bend the engine to
    // whatever rate that is — much simpler than resampling ourselves.
    acid::set_sample_rate(static_cast<int>(g_mix_fmt->nSamplesPerSec));
    g_channels = g_mix_fmt->nChannels;

    const REFERENCE_TIME kHundredNs = 10000 * 10; // ~10 ms default buffer
    if (FAILED(g_client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                    AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                    kHundredNs, 0, g_mix_fmt, nullptr))) {
        return;
    }

    g_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!g_event || FAILED(g_client->SetEventHandle(g_event))) return;

    if (FAILED(g_client->GetBufferSize(&g_buf_frames))) return;
    if (FAILED(g_client->GetService(__uuidof(IAudioRenderClient),
                                    reinterpret_cast<void**>(&g_render)))) {
        return;
    }

    g_run.store(true, std::memory_order_relaxed);
    g_worker = std::thread(audio_thread);
}

void acid_stop() {
    if (!g_client) return;
    g_run.store(false, std::memory_order_relaxed);
    if (g_event) SetEvent(g_event);  // kick the thread out of its wait
    if (g_worker.joinable()) g_worker.join();

    if (g_event) { CloseHandle(g_event); g_event = nullptr; }
    if (g_mix_fmt) { CoTaskMemFree(g_mix_fmt); g_mix_fmt = nullptr; }
    safe_release(g_render);
    safe_release(g_client);
    safe_release(g_device);
    safe_release(g_enumerator);

    if (g_com_init) { CoUninitialize(); g_com_init = false; }
}

}  // extern "C"
