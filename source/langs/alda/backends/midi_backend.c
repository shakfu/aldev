/**
 * @file midi_backend.c
 * @brief MIDI I/O backend for Alda using shared context.
 *
 * This file provides the alda_midi_* API by delegating to the shared
 * audio/MIDI backend. Event routing (Csound > TSF > MIDI) is handled
 * by the shared context.
 */

#include "alda/midi_backend.h"
#include "alda/tsf_backend.h"
#include "alda/csound_backend.h"
#include "alda/scala.h"
#include "context.h"      /* SharedContext */
#include "midi/midi.h"    /* shared_midi_* */
#include <stdio.h>
#include <string.h>

/* Cross-platform sleep */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define usleep(us) Sleep((us) / 1000)
#else
#include <unistd.h>
#endif

/* ============================================================================
 * Internal: Sync shared context with Alda flags
 * ============================================================================ */

/**
 * Sync Alda's tsf/csound enable flags to shared context.
 * Also sync no_sleep_mode for test compatibility.
 */
static void sync_shared_context(AldaContext* ctx) {
    if (!ctx || !ctx->shared) return;

    ctx->shared->builtin_synth_enabled = ctx->builtin_synth_enabled;
    ctx->shared->csound_enabled = ctx->csound_enabled;
    ctx->shared->no_sleep_mode = ctx->no_sleep_mode;
    ctx->shared->tempo = ctx->global_tempo;
}

/* ============================================================================
 * Initialization and Cleanup
 * ============================================================================ */

void alda_midi_init_observer(AldaContext* ctx) {
    if (!ctx || !ctx->shared) return;

    /* Delegate to shared context - no legacy observer needed */
    shared_midi_init_observer(ctx->shared);
}

void alda_midi_cleanup(AldaContext* ctx) {
    if (!ctx) return;

    /*
     * Shared context MIDI is cleaned up by alda_context_cleanup via
     * shared_context_cleanup. We just need to send panic and clear
     * our synced pointer. The actual handle is owned by shared context.
     */
    if (ctx->midi_out != NULL) {
        alda_midi_all_notes_off(ctx);
        ctx->midi_out = NULL;  /* Don't free - owned by shared context */
    }
}

/* ============================================================================
 * Port Management
 * ============================================================================ */

void alda_midi_list_ports(AldaContext* ctx) {
    if (!ctx || !ctx->shared) return;

    shared_midi_list_ports(ctx->shared);
}

int alda_midi_open_port(AldaContext* ctx, int port_idx) {
    if (!ctx || !ctx->shared) return -1;

    int result = shared_midi_open_port(ctx->shared, port_idx);
    /* Sync midi_out pointer for API compatibility */
    ctx->midi_out = ctx->shared->midi_out;
    return result;
}

int alda_midi_open_virtual(AldaContext* ctx, const char* name) {
    if (!ctx || !ctx->shared || !name) return -1;

    int result = shared_midi_open_virtual(ctx->shared, name);
    /* Sync midi_out pointer for API compatibility */
    ctx->midi_out = ctx->shared->midi_out;
    if (ctx->verbose_mode && result == 0) {
        printf("Created virtual MIDI output: %s\n", name);
    }
    return result;
}

int alda_midi_open_by_name(AldaContext* ctx, const char* name) {
    if (!ctx || !ctx->shared || !name) return -1;

    int result = shared_midi_open_by_name(ctx->shared, name);
    ctx->midi_out = ctx->shared->midi_out;
    return result;
}

int alda_midi_open_auto(AldaContext* ctx, const char* virtual_name) {
    if (!ctx || !ctx->shared) return -1;

    int result = shared_midi_open_auto(ctx->shared, virtual_name);
    ctx->midi_out = ctx->shared->midi_out;
    return result;
}

void alda_midi_close(AldaContext* ctx) {
    if (!ctx || !ctx->shared) return;

    shared_midi_close(ctx->shared);
    ctx->midi_out = NULL;
    if (ctx->verbose_mode) {
        printf("MIDI output closed\n");
    }
}

int alda_midi_is_open(AldaContext* ctx) {
    if (!ctx || !ctx->shared) return 0;

    return shared_midi_is_open(ctx->shared);
}

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * Find a part that uses the given MIDI channel.
 * Returns NULL if no part uses this channel.
 */
static AldaPartState* find_part_by_channel(AldaContext* ctx, int channel) {
    if (!ctx) return NULL;
    for (int i = 0; i < ctx->part_count; i++) {
        if (ctx->parts[i].channel == channel) {
            return &ctx->parts[i];
        }
    }
    return NULL;
}

/* ============================================================================
 * MIDI Message Sending (routes through shared context)
 * ============================================================================ */

void alda_midi_send_note_on(AldaContext* ctx, int channel, int pitch, int velocity) {
    if (!ctx || !ctx->shared) return;

    /* Sync flags to shared context */
    sync_shared_context(ctx);

    /* Handle Csound microtuning specially (requires part lookup) */
    if (ctx->csound_enabled && alda_csound_is_enabled()) {
        AldaPartState* part = find_part_by_channel(ctx, channel);
        if (part && part->scale) {
            /* Convert MIDI pitch to frequency using the part's scale */
            double freq = scala_midi_to_freq(
                (const ScalaScale*)part->scale,
                pitch,
                part->scale_root_note,
                part->scale_root_freq
            );
            alda_csound_send_note_on_freq(channel, freq, velocity, pitch);
            return;
        }
        /* No scale - fall through to shared routing */
    }

    /* Route through shared context (handles Csound > TSF > MIDI priority) */
    shared_send_note_on(ctx->shared, channel, pitch, velocity);
}

void alda_midi_send_note_off(AldaContext* ctx, int channel, int pitch) {
    if (!ctx || !ctx->shared) return;

    /* Sync flags to shared context */
    sync_shared_context(ctx);

    /* Route through shared context */
    shared_send_note_off(ctx->shared, channel, pitch);
}

void alda_midi_send_program(AldaContext* ctx, int channel, int program) {
    if (!ctx || !ctx->shared) return;

    /* Sync flags to shared context */
    sync_shared_context(ctx);

    /* Route through shared context */
    shared_send_program(ctx->shared, channel, program);
}

void alda_midi_send_cc(AldaContext* ctx, int channel, int cc, int value) {
    if (!ctx || !ctx->shared) return;

    /* Sync flags to shared context */
    sync_shared_context(ctx);

    /* Route through shared context */
    shared_send_cc(ctx->shared, channel, cc, value);
}

void alda_midi_all_notes_off(AldaContext* ctx) {
    if (!ctx || !ctx->shared) return;

    /* Sync flags to shared context */
    sync_shared_context(ctx);

    /* Route through shared context */
    shared_send_panic(ctx->shared);
}

/* ============================================================================
 * Timing
 * ============================================================================ */

void alda_midi_sleep_ms(AldaContext* ctx, int ms) {
    if (ms <= 0) return;
    if (!ctx || !ctx->shared) return;

    sync_shared_context(ctx);
    shared_sleep_ms(ctx->shared, ms);
}
