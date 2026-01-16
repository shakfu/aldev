/**
 * @file csound_backend.c
 * @brief Alda-specific Csound backend wrappers.
 *
 * This file provides thin wrappers around the shared Csound backend
 * (src/shared/audio/csound_backend.c) for backward compatibility with
 * existing Alda code. The actual implementation is in the shared layer.
 *
 * All alda_csound_* functions delegate to shared_csound_* functions.
 */

#include "alda/csound_backend.h"
#include "audio/audio.h"

/* ============================================================================
 * Initialization and Cleanup
 * ============================================================================ */

int alda_csound_init(void) {
    return shared_csound_init();
}

void alda_csound_cleanup(void) {
    shared_csound_cleanup();
}

/* ============================================================================
 * Instrument Loading
 * ============================================================================ */

int alda_csound_load_csd(const char* path) {
    return shared_csound_load(path);
}

int alda_csound_compile_orc(const char* orc) {
    return shared_csound_compile_orc(orc);
}

int alda_csound_has_instruments(void) {
    return shared_csound_has_instruments();
}

/* ============================================================================
 * Enable/Disable
 * ============================================================================ */

int alda_csound_enable(void) {
    return shared_csound_enable();
}

void alda_csound_disable(void) {
    shared_csound_disable();
}

int alda_csound_is_enabled(void) {
    return shared_csound_is_enabled();
}

/* ============================================================================
 * MIDI Message Sending
 * ============================================================================ */

void alda_csound_send_note_on(int channel, int pitch, int velocity) {
    shared_csound_send_note_on(channel, pitch, velocity);
}

void alda_csound_send_note_on_freq(int channel, double freq, int velocity, int midi_pitch) {
    shared_csound_send_note_on_freq(channel, freq, velocity, midi_pitch);
}

void alda_csound_send_note_off(int channel, int pitch) {
    shared_csound_send_note_off(channel, pitch);
}

void alda_csound_send_program(int channel, int program) {
    shared_csound_send_program(channel, program);
}

void alda_csound_send_cc(int channel, int cc, int value) {
    shared_csound_send_cc(channel, cc, value);
}

void alda_csound_send_pitch_bend(int channel, int bend) {
    shared_csound_send_pitch_bend(channel, bend);
}

void alda_csound_all_notes_off(void) {
    shared_csound_all_notes_off();
}

/* ============================================================================
 * Audio Rendering
 * ============================================================================ */

int alda_csound_get_sample_rate(void) {
    return shared_csound_get_sample_rate();
}

int alda_csound_get_channels(void) {
    return shared_csound_get_channels();
}

void alda_csound_render(float* output, int frames) {
    shared_csound_render(output, frames);
}

/* ============================================================================
 * Error Handling
 * ============================================================================ */

const char* alda_csound_get_error(void) {
    return shared_csound_get_error();
}

/* ============================================================================
 * Standalone Playback
 * ============================================================================ */

int alda_csound_play_file(const char* path, int verbose) {
    return shared_csound_play_file(path, verbose);
}

int alda_csound_play_file_async(const char* path) {
    return shared_csound_play_file_async(path);
}

int alda_csound_playback_active(void) {
    return shared_csound_playback_active();
}

void alda_csound_stop_playback(void) {
    shared_csound_stop_playback();
}
