/**
 * @file tsf_backend_wrapper.c
 * @brief Thin wrappers for TSF backend (delegates to shared library).
 *
 * The actual TSF implementation is in src/shared/audio/tsf_backend.c.
 * This file provides the alda_tsf_* API by delegating to shared_tsf_*.
 */

#include "alda/tsf_backend.h"
#include "audio/audio.h"

/* ============================================================================
 * Initialization and Cleanup
 * ============================================================================ */

int alda_tsf_init(void) {
    return shared_tsf_init();
}

void alda_tsf_cleanup(void) {
    shared_tsf_cleanup();
}

/* ============================================================================
 * Soundfont Management
 * ============================================================================ */

int alda_tsf_load_soundfont(const char* path) {
    return shared_tsf_load_soundfont(path);
}

int alda_tsf_has_soundfont(void) {
    return shared_tsf_has_soundfont();
}

int alda_tsf_get_preset_count(void) {
    return shared_tsf_get_preset_count();
}

const char* alda_tsf_get_preset_name(int index) {
    return shared_tsf_get_preset_name(index);
}

/* ============================================================================
 * Enable/Disable
 * ============================================================================ */

int alda_tsf_enable(void) {
    return shared_tsf_enable();
}

void alda_tsf_disable(void) {
    shared_tsf_disable();
}

int alda_tsf_is_enabled(void) {
    return shared_tsf_is_enabled();
}

/* ============================================================================
 * MIDI Message Sending
 * ============================================================================ */

void alda_tsf_send_note_on(int channel, int pitch, int velocity) {
    shared_tsf_send_note_on(channel, pitch, velocity);
}

void alda_tsf_send_note_off(int channel, int pitch) {
    shared_tsf_send_note_off(channel, pitch);
}

void alda_tsf_send_program(int channel, int program) {
    shared_tsf_send_program(channel, program);
}

void alda_tsf_send_cc(int channel, int cc, int value) {
    shared_tsf_send_cc(channel, cc, value);
}

void alda_tsf_all_notes_off(void) {
    shared_tsf_all_notes_off();
}
