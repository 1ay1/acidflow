// acidflow — the terminal app. App (src/app/) holds the state and
// behaviour, src/app/program.hpp is the jaal Program around it.
#include "app/program.hpp"

int main() {
    acid_start();
    acid_midi_start();   // No-op on platforms without a MIDI backend.

    int rc = maya::run<Acidflow>({
        .title        = "tb-303",
        .mouse        = true,
        .hover_motion = true,   // bare motion drives hover focus
        .mode         = Mode::Fullscreen,
    });

    acid_midi_stop();
    acid_stop();
    return rc;
}
