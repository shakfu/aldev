/**
 * @file joy_midi_backend.c
 * @brief MIDI backend wrapper for Joy language.
 *
 * Provides the MIDI interface that Joy's primitives expect.
 * Delegates to the shared audio/MIDI backend for actual I/O.
 */

#include "joy_midi_backend.h"
#include "context.h"        /* SharedContext */
#include "midi/midi.h"      /* shared_midi_* */
#include "audio/audio.h"    /* shared_tsf_*, shared_csound_* */
#include "link/link.h"      /* shared_link_* */
#include <stdio.h>
#include <stdlib.h>

/* Cross-platform sleep for fallback */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

/* ============================================================================
 * Module State
 * ============================================================================ */

static SharedContext* g_shared = NULL;
static int g_current_channel = 1;
static int g_initialized = 0;

/* ============================================================================
 * Public API - Initialization
 * ============================================================================ */

int joy_midi_init(void) {
    if (g_initialized) return 0;

    /* Allocate and initialize shared context */
    g_shared = (SharedContext*)malloc(sizeof(SharedContext));
    if (!g_shared) {
        fprintf(stderr, "Joy MIDI: Failed to allocate shared context\n");
        return -1;
    }

    if (shared_context_init(g_shared) != 0) {
        free(g_shared);
        g_shared = NULL;
        fprintf(stderr, "Joy MIDI: Failed to initialize shared context\n");
        return -1;
    }

    g_current_channel = 1;
    g_initialized = 1;

    return 0;
}

void joy_midi_cleanup(void) {
    if (!g_initialized) return;

    if (g_shared) {
        joy_midi_panic();
        shared_context_cleanup(g_shared);
        free(g_shared);
        g_shared = NULL;
    }

    g_initialized = 0;
}

/* ============================================================================
 * Public API - Port Management
 * ============================================================================ */

void joy_midi_list_ports(void) {
    if (!g_shared) {
        if (joy_midi_init() != 0) return;
    }
    shared_midi_list_ports(g_shared);
}

int joy_midi_open_port(int port_idx) {
    if (!g_shared) {
        if (joy_midi_init() != 0) return -1;
    }

    int ret = shared_midi_open_port(g_shared, port_idx);
    if (ret == 0) {
        const char* name = shared_midi_get_port_name(g_shared, port_idx);
        printf("Joy MIDI: Opened port %d: %s\n", port_idx, name ? name : "(unknown)");
    }
    return ret;
}

int joy_midi_open_virtual(const char* name) {
    if (!g_shared) {
        if (joy_midi_init() != 0) return -1;
    }

    int ret = shared_midi_open_virtual(g_shared, name ? name : "JoyMIDI");
    if (ret == 0) {
        printf("Joy MIDI: Created virtual port '%s'\n", name ? name : "JoyMIDI");
    }
    return ret;
}

void joy_midi_close(void) {
    if (g_shared) {
        joy_midi_panic();
        shared_midi_close(g_shared);
        printf("Joy MIDI: Port closed\n");
    }
}

int joy_midi_is_open(void) {
    return g_shared && shared_midi_is_open(g_shared);
}

/* ============================================================================
 * Public API - Channel Management
 * ============================================================================ */

void joy_midi_set_channel(int channel) {
    if (channel < 1) channel = 1;
    if (channel > 16) channel = 16;
    g_current_channel = channel;
}

int joy_midi_get_channel(void) {
    return g_current_channel;
}

/* ============================================================================
 * Public API - MIDI Messages
 * ============================================================================ */

void joy_midi_note_on(int pitch, int velocity) {
    joy_midi_note_on_ch(g_current_channel, pitch, velocity);
}

void joy_midi_note_off(int pitch) {
    joy_midi_note_off_ch(g_current_channel, pitch);
}

void joy_midi_note_on_ch(int channel, int pitch, int velocity) {
    if (!g_shared) return;

    /* Priority 1: Csound (via shared backend) */
    if (g_shared->csound_enabled && shared_csound_is_enabled()) {
        shared_csound_send_note_on(channel, pitch, velocity);
        return;
    }

    /* Priority 2+: TSF/MIDI via shared context */
    shared_send_note_on(g_shared, channel, pitch, velocity);
}

void joy_midi_note_off_ch(int channel, int pitch) {
    if (!g_shared) return;

    /* Priority 1: Csound */
    if (g_shared->csound_enabled && shared_csound_is_enabled()) {
        shared_csound_send_note_off(channel, pitch);
        return;
    }

    shared_send_note_off(g_shared, channel, pitch);
}

void joy_midi_program(int channel, int program) {
    if (!g_shared) return;

    /* Route to Csound if enabled */
    if (g_shared->csound_enabled && shared_csound_is_enabled()) {
        shared_csound_send_program(channel, program);
        return;
    }

    shared_send_program(g_shared, channel, program);
}

void joy_midi_cc(int channel, int cc, int value) {
    if (!g_shared) return;

    /* Route to Csound if enabled */
    if (g_shared->csound_enabled && shared_csound_is_enabled()) {
        shared_csound_send_cc(channel, cc, value);
        return;
    }

    shared_send_cc(g_shared, channel, cc, value);
}

void joy_midi_panic(void) {
    if (!g_shared) return;

    /* Stop Csound notes if enabled */
    if (g_shared->csound_enabled && shared_csound_is_enabled()) {
        shared_csound_all_notes_off();
    }

    shared_send_panic(g_shared);
}

void joy_midi_sleep_ms(int ms) {
    if (g_shared) {
        shared_sleep_ms(g_shared, ms);
    } else if (ms > 0) {
        /* Fallback to direct sleep if no context */
#ifdef _WIN32
        Sleep(ms);
#else
        usleep(ms * 1000);
#endif
    }
}

/* ============================================================================
 * TSF Backend Control (new functions for Joy)
 * ============================================================================ */

int joy_tsf_load_soundfont(const char* path) {
    /* Auto-initialize TSF backend if needed */
    if (shared_tsf_init() != 0) {
        return -1;
    }
    return shared_tsf_load_soundfont(path);
}

int joy_tsf_enable(void) {
    if (!g_shared) {
        if (joy_midi_init() != 0) return -1;
    }

    int ret = shared_tsf_enable();
    if (ret == 0) {
        g_shared->tsf_enabled = 1;
        /* Set default program (piano=0) for all channels so TSF knows which sound to use */
        for (int ch = 1; ch <= 16; ch++) {
            shared_tsf_send_program(ch, 0);
        }
    }
    return ret;
}

void joy_tsf_disable(void) {
    if (g_shared) {
        g_shared->tsf_enabled = 0;
    }
    shared_tsf_disable();
}

int joy_tsf_is_enabled(void) {
    return g_shared && g_shared->tsf_enabled && shared_tsf_is_enabled();
}

/* ============================================================================
 * Csound Backend Control (via shared backend)
 * ============================================================================ */

int joy_csound_init(void) {
    return shared_csound_init();
}

void joy_csound_cleanup(void) {
    /* Disable first to clear shared context flag */
    joy_csound_disable();
    shared_csound_cleanup();
}

int joy_csound_load(const char* path) {
    /* Auto-initialize if needed */
    if (shared_csound_init() != 0) {
        return -1;
    }
    return shared_csound_load(path);
}

int joy_csound_enable(void) {
    if (!g_shared) {
        if (joy_midi_init() != 0) return -1;
    }

    int ret = shared_csound_enable();
    if (ret == 0) {
        g_shared->csound_enabled = 1;
        /* Disable TSF when Csound is enabled (Csound takes priority) */
        if (g_shared->tsf_enabled) {
            g_shared->tsf_enabled = 0;
        }
    }
    return ret;
}

void joy_csound_disable(void) {
    if (g_shared) {
        g_shared->csound_enabled = 0;
    }
    shared_csound_disable();
}

int joy_csound_is_enabled(void) {
    return g_shared && g_shared->csound_enabled && shared_csound_is_enabled();
}

int joy_csound_play_file(const char* path, int verbose) {
    return shared_csound_play_file(path, verbose);
}

const char* joy_csound_get_error(void) {
    return shared_csound_get_error();
}

/* ============================================================================
 * Shared Context Access (for advanced use)
 * ============================================================================ */

SharedContext* joy_get_shared_context(void) {
    return g_shared;
}

void joy_set_shared_context(SharedContext* ctx) {
    /* If we have an existing context we own, clean it up */
    if (g_shared && g_initialized) {
        shared_context_cleanup(g_shared);
        free(g_shared);
    }

    g_shared = ctx;
    g_initialized = (ctx != NULL) ? 1 : 0;
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
