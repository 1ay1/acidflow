// macOS audio backend — drives `acid::render()` through AudioQueue.
//
// AudioQueue uses a callback-per-buffer model: the OS hands us an empty
// buffer, we fill it and hand it back. We pre-queue three buffers so the
// pipeline is always a frame or two ahead of the output. The DSP itself
// lives in engine.cpp; this file only owns the OS audio plumbing.
//
// Note: on some macOS SDK + GCC combinations the AudioToolbox headers fail
// to parse (they pull in mach-specific bits). The project's top-level
// CMakeLists compiles this file separately with Apple Clang on macOS to
// avoid that; the symbols still link cleanly because only the C façade
// crosses the boundary.

#include "audio.hpp"
#include "engine.hpp"

#include <AudioToolbox/AudioQueue.h>
#include <CoreAudio/CoreAudioTypes.h>

#include <cstring>

namespace {

AudioQueueRef       g_queue = nullptr;
AudioQueueBufferRef g_bufs[3] = {};

constexpr int kCoreAudioSR = 44100;

void render_cb(void* /*user*/, AudioQueueRef q, AudioQueueBufferRef buf) {
    float* out = static_cast<float*>(buf->mAudioData);
    acid::render(out, acid::kBufFrames);
    buf->mAudioDataByteSize = acid::kBufFrames * sizeof(float);
    AudioQueueEnqueueBuffer(q, buf, 0, nullptr);
}

}  // namespace

extern "C" {

void acid_start() {
    if (g_queue) return;

    acid::set_sample_rate(kCoreAudioSR);

    AudioStreamBasicDescription fmt = {};
    fmt.mSampleRate       = kCoreAudioSR;
    fmt.mFormatID         = kAudioFormatLinearPCM;
    fmt.mFormatFlags      = kLinearPCMFormatFlagIsFloat | kLinearPCMFormatFlagIsPacked;
    fmt.mBitsPerChannel   = 32;
    fmt.mChannelsPerFrame = 1;
    fmt.mBytesPerFrame    = 4;
    fmt.mFramesPerPacket  = 1;
    fmt.mBytesPerPacket   = 4;

    AudioQueueNewOutput(&fmt, &render_cb, nullptr,
                        nullptr, nullptr, 0, &g_queue);
    for (int i = 0; i < 3; i++) {
        AudioQueueAllocateBuffer(g_queue,
                                 acid::kBufFrames * sizeof(float),
                                 &g_bufs[i]);
        g_bufs[i]->mAudioDataByteSize = acid::kBufFrames * sizeof(float);
        std::memset(g_bufs[i]->mAudioData, 0, acid::kBufFrames * sizeof(float));
        AudioQueueEnqueueBuffer(g_queue, g_bufs[i], 0, nullptr);
    }
    AudioQueueStart(g_queue, nullptr);
}

void acid_stop() {
    if (!g_queue) return;
    AudioQueueStop(g_queue, true);
    AudioQueueDispose(g_queue, true);
    g_queue = nullptr;
}

}  // extern "C"
