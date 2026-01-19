/**
 * @file context.c
 * @brief Shared audio/MIDI context implementation.
 *
 * Handles context lifecycle and priority-based event routing to backends.
 * Priority: Csound > Built-in Synth (FluidSynth or TSF) > MIDI
 *
 * BUILD_FLUID_BACKEND selects FluidSynth as the built-in synth.
 * Otherwise, TinySoundFont is used.
 */

#include "context.h"
#include "audio/audio.h"
#include "midi/midi.h"
#include "link/link.h"
#include <string.h>
#include <stdio.h>

/* Cross-platform sleep */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define usleep(us) Sleep((us) / 1000)
#else
#include <unistd.h>
#endif

/* ============================================================================
 * Built-in Synth Abstraction (FluidSynth or TSF)
 *
 * These macros select the appropriate backend at compile time.
 * ============================================================================ */

#ifdef BUILD_FLUID_BACKEND
#define BUILTIN_SYNTH_NAME "FluidSynth"
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
#define BUILTIN_SYNTH_NAME "TinySoundFont"
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
 * Context Lifecycle
 * ============================================================================ */

int shared_context_init(SharedContext* ctx) {
    if (!ctx) return -1;

    memset(ctx, 0, sizeof(*ctx));

    /* Default settings */
    ctx->tempo = 120;
    ctx->default_channel = 1;

    /* Initialize MIDI observer for port enumeration */
    shared_midi_init_observer(ctx);

    return 0;
}

void shared_context_cleanup(SharedContext* ctx) {
    if (!ctx) return;

    /* Send panic to stop any playing notes */
    shared_send_panic(ctx);

    /* Disable backends that this context had enabled */
    if (ctx->builtin_synth_enabled) {
        builtin_synth_disable();
        ctx->builtin_synth_enabled = 0;
    }

    if (ctx->csound_enabled) {
        shared_csound_disable();
        ctx->csound_enabled = 0;
    }

    if (ctx->link_enabled) {
        shared_link_enable(0);
        ctx->link_enabled = 0;
    }

    /* Cleanup MIDI resources */
    shared_midi_cleanup(ctx);

    /* Clear scale reference (not owned by context) */
    ctx->scale = NULL;
}

/* ============================================================================
 * Event Dispatch (priority routing)
 * ============================================================================ */

void shared_send_note_on(SharedContext* ctx, int channel, int pitch, int velocity) {
    if (!ctx) return;

    /* Priority 1: Csound (if enabled and available) */
    if (ctx->csound_enabled && shared_csound_is_enabled()) {
        shared_csound_send_note_on(channel, pitch, velocity);
        return;
    }

    /* Priority 2: Built-in synth (if enabled and available) */
    if (ctx->builtin_synth_enabled && builtin_synth_is_enabled()) {
        builtin_synth_send_note_on(channel, pitch, velocity);
        return;
    }

    /* Priority 3: MIDI (if port is open) */
    if (ctx->midi_out) {
        shared_midi_send_note_on(ctx, channel, pitch, velocity);
    }
}

void shared_send_note_on_freq(SharedContext* ctx, int channel, double freq,
                              int velocity, int midi_pitch) {
    if (!ctx) return;

    /* Priority 1: Csound (supports microtuning via frequency) */
    if (ctx->csound_enabled && shared_csound_is_enabled()) {
        shared_csound_send_note_on_freq(channel, freq, velocity, midi_pitch);
        return;
    }

    /* Priority 2: Built-in synth (no native frequency support, use MIDI pitch) */
    if (ctx->builtin_synth_enabled && builtin_synth_is_enabled()) {
        builtin_synth_send_note_on(channel, midi_pitch, velocity);
        return;
    }

    /* Priority 3: MIDI (no frequency support, use MIDI pitch) */
    if (ctx->midi_out) {
        shared_midi_send_note_on(ctx, channel, midi_pitch, velocity);
    }
}

void shared_send_note_off(SharedContext* ctx, int channel, int pitch) {
    if (!ctx) return;

    /* Priority 1: Csound */
    if (ctx->csound_enabled && shared_csound_is_enabled()) {
        shared_csound_send_note_off(channel, pitch);
        return;
    }

    /* Priority 2: Built-in synth */
    if (ctx->builtin_synth_enabled && builtin_synth_is_enabled()) {
        builtin_synth_send_note_off(channel, pitch);
        return;
    }

    /* Priority 3: MIDI */
    if (ctx->midi_out) {
        shared_midi_send_note_off(ctx, channel, pitch);
    }
}

void shared_send_program(SharedContext* ctx, int channel, int program) {
    if (!ctx) return;

    /* Priority 1: Csound */
    if (ctx->csound_enabled && shared_csound_is_enabled()) {
        shared_csound_send_program(channel, program);
        return;
    }

    /* Priority 2: Built-in synth */
    if (ctx->builtin_synth_enabled && builtin_synth_is_enabled()) {
        builtin_synth_send_program(channel, program);
        return;
    }

    /* Priority 3: MIDI */
    if (ctx->midi_out) {
        shared_midi_send_program(ctx, channel, program);
    }
}

void shared_send_cc(SharedContext* ctx, int channel, int cc, int value) {
    if (!ctx) return;

    /* Priority 1: Csound */
    if (ctx->csound_enabled && shared_csound_is_enabled()) {
        shared_csound_send_cc(channel, cc, value);
        return;
    }

    /* Priority 2: Built-in synth */
    if (ctx->builtin_synth_enabled && builtin_synth_is_enabled()) {
        builtin_synth_send_cc(channel, cc, value);
        return;
    }

    /* Priority 3: MIDI */
    if (ctx->midi_out) {
        shared_midi_send_cc(ctx, channel, cc, value);
    }
}

void shared_send_panic(SharedContext* ctx) {
    if (!ctx) return;

    /*
     * Unlike note events which route to a single backend by priority,
     * panic must broadcast to ALL backends to ensure no stuck notes.
     * A session may have sent notes to multiple backends (e.g., user
     * switched backends mid-session, or backend was toggled).
     */

    /* Csound */
    if (shared_csound_is_enabled()) {
        shared_csound_all_notes_off();
    }

    /* Built-in synth */
    if (builtin_synth_is_enabled()) {
        builtin_synth_all_notes_off();
    }

    /* MIDI */
    if (ctx->midi_out) {
        shared_midi_all_notes_off(ctx);
    }
}

/* ============================================================================
 * Timing Utilities
 * ============================================================================ */

int shared_ticks_to_ms(int ticks, int tempo) {
    if (tempo <= 0) tempo = 120;

    /* 128 ticks per beat (same as Alda default) */
    double ms_per_beat = 60000.0 / tempo;
    double ms_per_tick = ms_per_beat / 128.0;

    return (int)(ticks * ms_per_tick);
}

void shared_sleep_ms(SharedContext* ctx, int ms) {
    if (ms <= 0) return;

    /* Skip sleep in test mode */
    if (ctx && ctx->no_sleep_mode) return;

    usleep(ms * 1000);
}

int shared_effective_tempo(SharedContext* ctx) {
    if (!ctx) return 120;

    /* If Link is enabled, use Link tempo */
    if (ctx->link_enabled && shared_link_is_enabled()) {
        return (int)shared_link_get_tempo();
    }

    /* Otherwise use context tempo */
    return ctx->tempo;
}
