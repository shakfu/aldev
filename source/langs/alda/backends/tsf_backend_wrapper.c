/**
 * @file tsf_backend_wrapper.c
 * @brief Thin wrappers for built-in synth backend (TSF or FluidSynth).
 *
 * The actual implementation is in src/shared/audio/tsf_backend.c or
 * fluid_backend.c depending on build configuration.
 * This file provides the alda_tsf_* API by delegating to the active backend.
 */

#include "alda/tsf_backend.h"
#include "audio/audio.h"

/* ============================================================================
 * Built-in Synth Abstraction (FluidSynth or TSF)
 *
 * These macros select the appropriate backend at compile time.
 * ============================================================================ */

#ifdef BUILD_FLUID_BACKEND
#define builtin_synth_init()              shared_fluid_init()
#define builtin_synth_cleanup()           shared_fluid_cleanup()
#define builtin_synth_load_soundfont(p)   shared_fluid_load_soundfont(p)
#define builtin_synth_has_soundfont()     shared_fluid_has_soundfont()
#define builtin_synth_get_preset_count()  shared_fluid_get_preset_count()
#define builtin_synth_get_preset_name(i)  shared_fluid_get_preset_name(i)
#define builtin_synth_enable()            shared_fluid_enable()
#define builtin_synth_disable()           shared_fluid_disable()
#define builtin_synth_is_enabled()        shared_fluid_is_enabled()
#define builtin_synth_send_note_on(c,p,v) shared_fluid_send_note_on(c,p,v)
#define builtin_synth_send_note_off(c,p)  shared_fluid_send_note_off(c,p)
#define builtin_synth_send_program(c,p)   shared_fluid_send_program(c,p)
#define builtin_synth_send_cc(c,cc,v)     shared_fluid_send_cc(c,cc,v)
#define builtin_synth_all_notes_off()     shared_fluid_all_notes_off()
#else
#define builtin_synth_init()              shared_tsf_init()
#define builtin_synth_cleanup()           shared_tsf_cleanup()
#define builtin_synth_load_soundfont(p)   shared_tsf_load_soundfont(p)
#define builtin_synth_has_soundfont()     shared_tsf_has_soundfont()
#define builtin_synth_get_preset_count()  shared_tsf_get_preset_count()
#define builtin_synth_get_preset_name(i)  shared_tsf_get_preset_name(i)
#define builtin_synth_enable()            shared_tsf_enable()
#define builtin_synth_disable()           shared_tsf_disable()
#define builtin_synth_is_enabled()        shared_tsf_is_enabled()
#define builtin_synth_send_note_on(c,p,v) shared_tsf_send_note_on(c,p,v)
#define builtin_synth_send_note_off(c,p)  shared_tsf_send_note_off(c,p)
#define builtin_synth_send_program(c,p)   shared_tsf_send_program(c,p)
#define builtin_synth_send_cc(c,cc,v)     shared_tsf_send_cc(c,cc,v)
#define builtin_synth_all_notes_off()     shared_tsf_all_notes_off()
#endif

/* ============================================================================
 * Initialization and Cleanup
 * ============================================================================ */

int alda_tsf_init(void) {
    return builtin_synth_init();
}

void alda_tsf_cleanup(void) {
    builtin_synth_cleanup();
}

/* ============================================================================
 * Soundfont Management
 * ============================================================================ */

int alda_tsf_load_soundfont(const char* path) {
    /* Auto-initialize if needed */
    if (builtin_synth_init() != 0) {
        return -1;
    }
    return builtin_synth_load_soundfont(path);
}

int alda_tsf_has_soundfont(void) {
    return builtin_synth_has_soundfont();
}

int alda_tsf_get_preset_count(void) {
    return builtin_synth_get_preset_count();
}

const char* alda_tsf_get_preset_name(int index) {
    return builtin_synth_get_preset_name(index);
}

/* ============================================================================
 * Enable/Disable
 * ============================================================================ */

int alda_tsf_enable(void) {
    return builtin_synth_enable();
}

void alda_tsf_disable(void) {
    builtin_synth_disable();
}

int alda_tsf_is_enabled(void) {
    return builtin_synth_is_enabled();
}

/* ============================================================================
 * MIDI Message Sending
 * ============================================================================ */

void alda_tsf_send_note_on(int channel, int pitch, int velocity) {
    builtin_synth_send_note_on(channel, pitch, velocity);
}

void alda_tsf_send_note_off(int channel, int pitch) {
    builtin_synth_send_note_off(channel, pitch);
}

void alda_tsf_send_program(int channel, int program) {
    builtin_synth_send_program(channel, program);
}

void alda_tsf_send_cc(int channel, int cc, int value) {
    builtin_synth_send_cc(channel, cc, value);
}

void alda_tsf_all_notes_off(void) {
    builtin_synth_all_notes_off();
}
