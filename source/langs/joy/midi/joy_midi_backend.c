/**
 * @file joy_midi_backend.c
 * @brief MIDI backend wrapper for Joy language.
 *
 * Provides the MIDI interface that Joy's primitives expect.
 * Delegates to the shared audio/MIDI backend for actual I/O.
 *
 * All functions take SharedContext* as first parameter - no globals.
 */

#include "joy_midi_backend.h"
#include "psnd.h"           /* PSND_MIDI_PORT_NAME */
#include "context.h"        /* SharedContext */
#include "midi/midi.h"      /* shared_midi_* */
#include "audio/audio.h"    /* shared_tsf_*, shared_csound_* */
#include "link/link.h"      /* shared_link_* */
#include <stdio.h>
#include <stdlib.h>

/* ============================================================================
 * Port Management
 * ============================================================================ */

void joy_midi_list_ports(SharedContext* ctx) {
    if (ctx) {
        shared_midi_list_ports(ctx);
    }
}

int joy_midi_open_port(SharedContext* ctx, int port_idx) {
    if (!ctx) return -1;

    int ret = shared_midi_open_port(ctx, port_idx);
    if (ret == 0) {
        const char* name = shared_midi_get_port_name(ctx, port_idx);
        printf("Joy MIDI: Opened port %d: %s\n", port_idx, name ? name : "(unknown)");
    }
    return ret;
}

int joy_midi_open_virtual(SharedContext* ctx, const char* name) {
    if (!ctx) return -1;

    int ret = shared_midi_open_virtual(ctx, name ? name : PSND_MIDI_PORT_NAME);
    if (ret == 0) {
        printf("Joy MIDI: Created virtual port '%s'\n", name ? name : PSND_MIDI_PORT_NAME);
    }
    return ret;
}

void joy_midi_close(SharedContext* ctx) {
    if (ctx) {
        joy_midi_panic(ctx);
        shared_midi_close(ctx);
        printf("Joy MIDI: Port closed\n");
    }
}

int joy_midi_is_open(SharedContext* ctx) {
    return ctx && shared_midi_is_open(ctx);
}

/* ============================================================================
 * MIDI Messages
 * ============================================================================ */

void joy_midi_note_on(SharedContext* ctx, int channel, int pitch, int velocity) {
    if (!ctx) return;

    /* Priority 1: Csound (via shared backend) */
    if (ctx->csound_enabled && shared_csound_is_enabled()) {
        shared_csound_send_note_on(channel, pitch, velocity);
        return;
    }

    /* Priority 2+: TSF/MIDI via shared context */
    shared_send_note_on(ctx, channel, pitch, velocity);
}

void joy_midi_note_off(SharedContext* ctx, int channel, int pitch) {
    if (!ctx) return;

    /* Priority 1: Csound */
    if (ctx->csound_enabled && shared_csound_is_enabled()) {
        shared_csound_send_note_off(channel, pitch);
        return;
    }

    shared_send_note_off(ctx, channel, pitch);
}

void joy_midi_program(SharedContext* ctx, int channel, int program) {
    if (!ctx) return;

    /* Route to Csound if enabled */
    if (ctx->csound_enabled && shared_csound_is_enabled()) {
        shared_csound_send_program(channel, program);
        return;
    }

    shared_send_program(ctx, channel, program);
}

void joy_midi_cc(SharedContext* ctx, int channel, int cc, int value) {
    if (!ctx) return;

    /* Route to Csound if enabled */
    if (ctx->csound_enabled && shared_csound_is_enabled()) {
        shared_csound_send_cc(channel, cc, value);
        return;
    }

    shared_send_cc(ctx, channel, cc, value);
}

void joy_midi_panic(SharedContext* ctx) {
    if (!ctx) return;

    /* Stop Csound notes if enabled */
    if (ctx->csound_enabled && shared_csound_is_enabled()) {
        shared_csound_all_notes_off();
    }

    shared_send_panic(ctx);
}

void joy_midi_sleep_ms(SharedContext* ctx, int ms) {
    if (ctx) {
        shared_sleep_ms(ctx, ms);
    }
}

/* ============================================================================
 * TSF Backend Control
 * ============================================================================ */

int joy_tsf_load_soundfont(const char* path) {
    /* Auto-initialize TSF backend if needed */
    if (shared_tsf_init() != 0) {
        return -1;
    }
    return shared_tsf_load_soundfont(path);
}

int joy_tsf_enable(SharedContext* ctx) {
    if (!ctx) return -1;

    int ret = shared_tsf_enable();
    if (ret == 0) {
        ctx->builtin_synth_enabled = 1;
        /* Set default program (piano=0) for all channels so TSF knows which sound to use */
        for (int ch = 1; ch <= 16; ch++) {
            shared_tsf_send_program(ch, 0);
        }
    }
    return ret;
}

void joy_tsf_disable(SharedContext* ctx) {
    if (ctx) {
        ctx->builtin_synth_enabled = 0;
    }
    shared_tsf_disable();
}

int joy_tsf_is_enabled(SharedContext* ctx) {
    return ctx && ctx->builtin_synth_enabled && shared_tsf_is_enabled();
}

/* ============================================================================
 * Csound Backend Control
 * ============================================================================ */

int joy_csound_init(void) {
    return shared_csound_init();
}

void joy_csound_cleanup(SharedContext* ctx) {
    joy_csound_disable(ctx);
    shared_csound_cleanup();
}

int joy_csound_load(const char* path) {
    /* Auto-initialize if needed */
    if (shared_csound_init() != 0) {
        return -1;
    }
    return shared_csound_load(path);
}

int joy_csound_enable(SharedContext* ctx) {
    if (!ctx) return -1;

    int ret = shared_csound_enable();
    if (ret == 0) {
        ctx->csound_enabled = 1;
        /* Disable TSF when Csound is enabled (Csound takes priority) */
        if (ctx->builtin_synth_enabled) {
            ctx->builtin_synth_enabled = 0;
        }
    }
    return ret;
}

void joy_csound_disable(SharedContext* ctx) {
    if (ctx) {
        ctx->csound_enabled = 0;
    }
    shared_csound_disable();
}

int joy_csound_is_enabled(SharedContext* ctx) {
    return ctx && ctx->csound_enabled && shared_csound_is_enabled();
}

int joy_csound_play_file(const char* path, int verbose) {
    return shared_csound_play_file(path, verbose);
}

const char* joy_csound_get_error(void) {
    return shared_csound_get_error();
}

/* ============================================================================
 * Ableton Link Support
 * ============================================================================ */

int joy_link_init(double bpm) {
    return shared_link_init(bpm);
}

void joy_link_cleanup(void) {
    shared_link_cleanup();
}

int joy_link_enable(void) {
    if (!shared_link_is_initialized()) {
        /* Auto-initialize with default tempo */
        if (shared_link_init(120.0) != 0) {
            return -1;
        }
    }
    shared_link_enable(1);
    return 0;
}

void joy_link_disable(void) {
    shared_link_enable(0);
}

int joy_link_is_enabled(void) {
    return shared_link_is_enabled();
}

double joy_link_get_tempo(void) {
    if (!shared_link_is_initialized()) {
        return 0.0;
    }
    return shared_link_get_tempo();
}

void joy_link_set_tempo(double bpm) {
    if (!shared_link_is_initialized()) {
        return;
    }
    shared_link_set_tempo(bpm);
}

double joy_link_get_beat(double quantum) {
    if (!shared_link_is_initialized()) {
        return 0.0;
    }
    return shared_link_get_beat(quantum);
}

double joy_link_get_phase(double quantum) {
    if (!shared_link_is_initialized()) {
        return 0.0;
    }
    return shared_link_get_phase(quantum);
}

int joy_link_num_peers(void) {
    if (!shared_link_is_initialized()) {
        return 0;
    }
    return (int)shared_link_num_peers();
}
