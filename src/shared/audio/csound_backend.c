/**
 * @file csound_backend.c
 * @brief Shared Csound backend stubs.
 *
 * For now, these are stub implementations. The actual Csound backend
 * remains in src/alda/csound_backend.c. A future refactor can move
 * the implementation here.
 *
 * The context routing logic checks both csound_enabled and shared_csound_is_enabled(),
 * so if Csound is not available, events will route to TSF or MIDI instead.
 */

#include "audio.h"

/* Stub implementations - Csound not yet moved to shared */

int shared_csound_init(void) {
    return -1;  /* Not available in shared yet */
}

void shared_csound_cleanup(void) {
    /* No-op */
}

int shared_csound_load(const char* path) {
    (void)path;
    return -1;  /* Not available */
}

int shared_csound_enable(void) {
    return -1;  /* Not available */
}

void shared_csound_disable(void) {
    /* No-op */
}

int shared_csound_is_enabled(void) {
    return 0;  /* Never enabled - routes to TSF/MIDI instead */
}

void shared_csound_send_note_on(int channel, int pitch, int velocity) {
    (void)channel;
    (void)pitch;
    (void)velocity;
}

void shared_csound_send_note_on_freq(int channel, double freq, int velocity, int midi_pitch) {
    (void)channel;
    (void)freq;
    (void)velocity;
    (void)midi_pitch;
}

void shared_csound_send_note_off(int channel, int pitch) {
    (void)channel;
    (void)pitch;
}

void shared_csound_send_program(int channel, int program) {
    (void)channel;
    (void)program;
}

void shared_csound_send_cc(int channel, int cc, int value) {
    (void)channel;
    (void)cc;
    (void)value;
}

void shared_csound_all_notes_off(void) {
    /* No-op */
}
