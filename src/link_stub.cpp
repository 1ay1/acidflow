// No Ableton Link in this build (configure with -DACIDFLOW_LINK=ON).
#include "audio.hpp"

extern "C" {
void acid_link_enable(int) {}
int  acid_link_enabled(void) { return 0; }
int  acid_link_peers(void) { return 0; }
void acid_link_set_tempo(float) {}
void acid_link_tick(int, int) {}
}
