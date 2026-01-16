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

    ctx->shared->tsf_enabled = ctx->tsf_enabled;
    ctx->shared->csound_enabled = ctx->csound_enabled;
    ctx->shared->no_sleep_mode = ctx->no_sleep_mode;
    ctx->shared->tempo = ctx->global_tempo;
}

/* ============================================================================
 * Port Enumeration Callback (for legacy compatibility)
 * ============================================================================ */

typedef struct {
    AldaContext* ctx;
} EnumContext;

static void on_output_port_found(void* user_ctx, const libremidi_midi_out_port* port) {
    EnumContext* ec = (EnumContext*)user_ctx;
    AldaContext* ctx = ec->ctx;

    if (ctx->out_port_count >= ALDA_MAX_PORTS) return;

    libremidi_midi_out_port_clone(port, &ctx->out_ports[ctx->out_port_count]);
    ctx->out_port_count++;
}

/* ============================================================================
 * Initialization and Cleanup
 * ============================================================================ */

void alda_midi_init_observer(AldaContext* ctx) {
    if (!ctx) return;

    /* Initialize shared context observer */
    if (ctx->shared) {
        shared_midi_init_observer(ctx->shared);
    }

    /* Also maintain legacy port list for backward compatibility */
    int ret = 0;

    /* Free existing observer if any */
    if (ctx->midi_observer != NULL) {
        for (int i = 0; i < ctx->out_port_count; i++) {
            if (ctx->out_ports[i]) {
                libremidi_midi_out_port_free(ctx->out_ports[i]);
                ctx->out_ports[i] = NULL;
            }
        }
        ctx->out_port_count = 0;
        libremidi_midi_observer_free(ctx->midi_observer);
        ctx->midi_observer = NULL;
    }

    /* Create observer configuration */
    libremidi_observer_configuration observer_conf;
    ret = libremidi_midi_observer_configuration_init(&observer_conf);
    if (ret != 0) {
        fprintf(stderr, "Failed to init observer config: %d\n", ret);
        return;
    }

    observer_conf.track_hardware = true;
    observer_conf.track_virtual = true;
    observer_conf.track_any = true;

    /* Create API configuration */
    libremidi_api_configuration api_conf;
    ret = libremidi_midi_api_configuration_init(&api_conf);
    if (ret != 0) {
        fprintf(stderr, "Failed to init API config: %d\n", ret);
        return;
    }

    api_conf.configuration_type = Observer;
    api_conf.api = UNSPECIFIED;

    /* Create observer */
    ret = libremidi_midi_observer_new(&observer_conf, &api_conf, &ctx->midi_observer);
    if (ret != 0) {
        fprintf(stderr, "Failed to create MIDI observer: %d\n", ret);
        return;
    }

    /* Enumerate output ports */
    ctx->out_port_count = 0;
    EnumContext ec = { .ctx = ctx };
    ret = libremidi_midi_observer_enumerate_output_ports(ctx->midi_observer, &ec, on_output_port_found);
    if (ret != 0) {
        fprintf(stderr, "Failed to enumerate ports: %d\n", ret);
    }
}

void alda_midi_cleanup(AldaContext* ctx) {
    if (!ctx) return;

    /* Cleanup shared context MIDI */
    if (ctx->shared) {
        shared_midi_cleanup(ctx->shared);
    }

    /* Also cleanup legacy handles */
    if (ctx->midi_out != NULL) {
        alda_midi_all_notes_off(ctx);
        libremidi_midi_out_free(ctx->midi_out);
        ctx->midi_out = NULL;
    }

    /* Free legacy ports */
    for (int i = 0; i < ctx->out_port_count; i++) {
        if (ctx->out_ports[i]) {
            libremidi_midi_out_port_free(ctx->out_ports[i]);
            ctx->out_ports[i] = NULL;
        }
    }
    ctx->out_port_count = 0;

    /* Free legacy observer */
    if (ctx->midi_observer != NULL) {
        libremidi_midi_observer_free(ctx->midi_observer);
        ctx->midi_observer = NULL;
    }
}

/* ============================================================================
 * Port Management
 * ============================================================================ */

void alda_midi_list_ports(AldaContext* ctx) {
    if (!ctx) return;

    /* Use shared context if available */
    if (ctx->shared) {
        shared_midi_list_ports(ctx->shared);
        return;
    }

    /* Fallback to legacy implementation */
    alda_midi_init_observer(ctx);

    printf("MIDI outputs:\n");
    if (ctx->out_port_count == 0) {
        printf("  (none - use virtual port)\n");
    } else {
        for (int i = 0; i < ctx->out_port_count; i++) {
            const char* name = NULL;
            size_t len = 0;
            if (libremidi_midi_out_port_name(ctx->out_ports[i], &name, &len) == 0) {
                printf("  %d: %s\n", i, name);
            }
        }
    }
}

int alda_midi_open_port(AldaContext* ctx, int port_idx) {
    if (!ctx) return -1;

    /* Use shared context */
    if (ctx->shared) {
        int result = shared_midi_open_port(ctx->shared, port_idx);
        /* Sync midi_out pointer for legacy compatibility */
        ctx->midi_out = ctx->shared->midi_out;
        return result;
    }

    /* Fallback to legacy implementation */
    alda_midi_init_observer(ctx);

    if (port_idx < 0 || port_idx >= ctx->out_port_count) {
        fprintf(stderr, "Invalid port index: %d (have %d ports)\n",
                port_idx, ctx->out_port_count);
        return -1;
    }

    /* Close existing output */
    if (ctx->midi_out != NULL) {
        libremidi_midi_out_free(ctx->midi_out);
        ctx->midi_out = NULL;
    }

    int ret = 0;

    /* Create MIDI configuration */
    libremidi_midi_configuration midi_conf;
    ret = libremidi_midi_configuration_init(&midi_conf);
    if (ret != 0) {
        fprintf(stderr, "Failed to init MIDI config\n");
        return -1;
    }

    midi_conf.version = MIDI1;
    midi_conf.out_port = ctx->out_ports[port_idx];

    /* Create API configuration */
    libremidi_api_configuration api_conf;
    ret = libremidi_midi_api_configuration_init(&api_conf);
    if (ret != 0) {
        fprintf(stderr, "Failed to init API config\n");
        return -1;
    }

    api_conf.configuration_type = Output;
    api_conf.api = UNSPECIFIED;

    /* Open output */
    ret = libremidi_midi_out_new(&midi_conf, &api_conf, &ctx->midi_out);
    if (ret != 0) {
        fprintf(stderr, "Failed to open MIDI output: %d\n", ret);
        return -1;
    }

    const char* name = NULL;
    size_t len = 0;
    libremidi_midi_out_port_name(ctx->out_ports[port_idx], &name, &len);
    if (ctx->verbose_mode) {
        printf("Opened MIDI output: %s\n", name);
    }

    return 0;
}

int alda_midi_open_virtual(AldaContext* ctx, const char* name) {
    if (!ctx || !name) return -1;

    /* Use shared context */
    if (ctx->shared) {
        int result = shared_midi_open_virtual(ctx->shared, name);
        /* Sync midi_out pointer for legacy compatibility */
        ctx->midi_out = ctx->shared->midi_out;
        if (ctx->verbose_mode && result == 0) {
            printf("Created virtual MIDI output: %s\n", name);
        }
        return result;
    }

    /* Fallback to legacy implementation */
    alda_midi_init_observer(ctx);

    /* Close existing output */
    if (ctx->midi_out != NULL) {
        libremidi_midi_out_free(ctx->midi_out);
        ctx->midi_out = NULL;
    }

    int ret = 0;

    /* Create MIDI configuration */
    libremidi_midi_configuration midi_conf;
    ret = libremidi_midi_configuration_init(&midi_conf);
    if (ret != 0) {
        fprintf(stderr, "Failed to init MIDI config\n");
        return -1;
    }

    midi_conf.version = MIDI1;
    midi_conf.virtual_port = true;
    midi_conf.port_name = name;

    /* Create API configuration */
    libremidi_api_configuration api_conf;
    ret = libremidi_midi_api_configuration_init(&api_conf);
    if (ret != 0) {
        fprintf(stderr, "Failed to init API config\n");
        return -1;
    }

    api_conf.configuration_type = Output;
    api_conf.api = UNSPECIFIED;

    /* Create virtual output */
    ret = libremidi_midi_out_new(&midi_conf, &api_conf, &ctx->midi_out);
    if (ret != 0) {
        fprintf(stderr, "Failed to create virtual MIDI output: %d\n", ret);
        return -1;
    }

    if (ctx->verbose_mode) {
        printf("Created virtual MIDI output: %s\n", name);
    }

    return 0;
}

int alda_midi_open_by_name(AldaContext* ctx, const char* name) {
    if (!ctx || !name) return -1;

    /* Use shared context */
    if (ctx->shared) {
        int result = shared_midi_open_by_name(ctx->shared, name);
        ctx->midi_out = ctx->shared->midi_out;
        return result;
    }

    /* Fallback to legacy implementation */
    alda_midi_init_observer(ctx);

    /* Search for substring match in hardware port names */
    for (int i = 0; i < ctx->out_port_count; i++) {
        const char* port_name = NULL;
        size_t len = 0;
        if (libremidi_midi_out_port_name(ctx->out_ports[i], &port_name, &len) == 0) {
            if (strstr(port_name, name) != NULL) {
                /* Found a match */
                return alda_midi_open_port(ctx, i);
            }
        }
    }

    /* No hardware port matched - create virtual port */
    return alda_midi_open_virtual(ctx, name);
}

int alda_midi_open_auto(AldaContext* ctx, const char* virtual_name) {
    if (!ctx) return -1;

    /* Use shared context */
    if (ctx->shared) {
        int result = shared_midi_open_auto(ctx->shared, virtual_name);
        ctx->midi_out = ctx->shared->midi_out;
        return result;
    }

    /* Fallback to legacy implementation */
    alda_midi_init_observer(ctx);

    /* If hardware ports available, open the first one */
    if (ctx->out_port_count > 0) {
        return alda_midi_open_port(ctx, 0);
    }

    /* No hardware ports - create virtual port */
    return alda_midi_open_virtual(ctx, virtual_name);
}

void alda_midi_close(AldaContext* ctx) {
    if (!ctx) return;

    /* Use shared context */
    if (ctx->shared) {
        shared_midi_close(ctx->shared);
        ctx->midi_out = NULL;
        if (ctx->verbose_mode) {
            printf("MIDI output closed\n");
        }
        return;
    }

    /* Fallback to legacy implementation */
    if (ctx->midi_out != NULL) {
        alda_midi_all_notes_off(ctx);
        libremidi_midi_out_free(ctx->midi_out);
        ctx->midi_out = NULL;
        if (ctx->verbose_mode) {
            printf("MIDI output closed\n");
        }
    }
}

int alda_midi_is_open(AldaContext* ctx) {
    if (!ctx) return 0;

    /* Check shared context first */
    if (ctx->shared) {
        return shared_midi_is_open(ctx->shared);
    }

    return ctx->midi_out != NULL;
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
    if (!ctx) return;

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
    if (ctx->shared) {
        shared_send_note_on(ctx->shared, channel, pitch, velocity);
        return;
    }

    /* Fallback: direct MIDI send */
    if (ctx->midi_out) {
        unsigned char msg[3];
        msg[0] = 0x90 | ((channel - 1) & 0x0F);
        msg[1] = pitch & 0x7F;
        msg[2] = velocity & 0x7F;
        libremidi_midi_out_send_message(ctx->midi_out, msg, 3);
    }
}

void alda_midi_send_note_off(AldaContext* ctx, int channel, int pitch) {
    if (!ctx) return;

    /* Sync flags to shared context */
    sync_shared_context(ctx);

    /* Route through shared context */
    if (ctx->shared) {
        shared_send_note_off(ctx->shared, channel, pitch);
        return;
    }

    /* Fallback: direct MIDI send */
    if (ctx->midi_out) {
        unsigned char msg[3];
        msg[0] = 0x80 | ((channel - 1) & 0x0F);
        msg[1] = pitch & 0x7F;
        msg[2] = 0;
        libremidi_midi_out_send_message(ctx->midi_out, msg, 3);
    }
}

void alda_midi_send_program(AldaContext* ctx, int channel, int program) {
    if (!ctx) return;

    /* Sync flags to shared context */
    sync_shared_context(ctx);

    /* Route through shared context */
    if (ctx->shared) {
        shared_send_program(ctx->shared, channel, program);
        return;
    }

    /* Fallback: direct MIDI send */
    if (ctx->midi_out) {
        unsigned char msg[2];
        msg[0] = 0xC0 | ((channel - 1) & 0x0F);
        msg[1] = program & 0x7F;
        libremidi_midi_out_send_message(ctx->midi_out, msg, 2);
    }
}

void alda_midi_send_cc(AldaContext* ctx, int channel, int cc, int value) {
    if (!ctx) return;

    /* Sync flags to shared context */
    sync_shared_context(ctx);

    /* Route through shared context */
    if (ctx->shared) {
        shared_send_cc(ctx->shared, channel, cc, value);
        return;
    }

    /* Fallback: direct MIDI send */
    if (ctx->midi_out) {
        unsigned char msg[3];
        msg[0] = 0xB0 | ((channel - 1) & 0x0F);
        msg[1] = cc & 0x7F;
        msg[2] = value & 0x7F;
        libremidi_midi_out_send_message(ctx->midi_out, msg, 3);
    }
}

void alda_midi_all_notes_off(AldaContext* ctx) {
    if (!ctx) return;

    /* Sync flags to shared context */
    sync_shared_context(ctx);

    /* Route through shared context */
    if (ctx->shared) {
        shared_send_panic(ctx->shared);
        return;
    }

    /* Fallback: direct MIDI send */
    if (ctx->midi_out) {
        for (int ch = 0; ch < 16; ch++) {
            unsigned char msg[3];
            msg[0] = 0xB0 | ch;
            msg[1] = 123;  /* All Notes Off */
            msg[2] = 0;
            libremidi_midi_out_send_message(ctx->midi_out, msg, 3);
        }
    }
}

/* ============================================================================
 * Timing
 * ============================================================================ */

void alda_midi_sleep_ms(AldaContext* ctx, int ms) {
    if (ms <= 0) return;

    /* Use shared sleep if available (respects no_sleep_mode) */
    if (ctx && ctx->shared) {
        sync_shared_context(ctx);
        shared_sleep_ms(ctx->shared, ms);
        return;
    }

    /* Fallback: direct sleep */
    if (!alda_no_sleep(ctx)) {
        usleep(ms * 1000);
    }
}
