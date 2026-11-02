#pragma once
// Linux audio drivers. audio_linux.cpp picks one at acid_start(): JACK when a
// JACK (or PipeWire-JACK) server is running, otherwise ALSA. ACIDFLOW_AUDIO
// (jack|alsa) forces one.

namespace acid::backend {

bool alsa_start();
void alsa_stop();

bool jack_start();          // false when libjack or a server isn't there
void jack_stop();

}  // namespace acid::backend
