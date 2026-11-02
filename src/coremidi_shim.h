#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*acid_cm_bytes_fn)(const unsigned char* data, int len);

// Creates the "acidflow" virtual source/destination and listens on every
// physical source. on_bytes runs on CoreMIDI's read thread. Returns 0 on failure.
int  acid_cm_open(acid_cm_bytes_fn on_bytes);
void acid_cm_send(const unsigned char* msg, int len);
void acid_cm_close(void);

#ifdef __cplusplus
}
#endif
