/**
 * @file midi_input.c
 * @brief MIDI input handling for CC -> parameter binding.
 *
 * Receives MIDI CC messages and routes them to bound parameters.
 * Runs in libremidi's callback thread - all operations must be thread-safe.
 */

#include "midi.h"
#include "../context.h"
#include "../param/param.h"
#include <libremidi/libremidi-c.h>
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * MIDI Message Callback
 *
 * Called from libremidi's thread when a MIDI message is received.
 * Must be fast and thread-safe.
 * ============================================================================ */

static void on_midi_in_message(void* user_data, libremidi_timestamp timestamp,
                                const libremidi_midi1_symbol* data, size_t len) {
    (void)timestamp;  /* Unused */

    SharedContext* ctx = (SharedContext*)user_data;
    if (!ctx || !data || len < 1) return;

    uint8_t status = data[0];

    /* Only handle CC messages (0xBn where n is channel) */
    if ((status & 0xF0) == 0xB0 && len >= 3) {
        int channel = (status & 0x0F) + 1;  /* 1-based */
        int cc = data[1] & 0x7F;
        int value = data[2] & 0x7F;

        /* Route to parameter system */
        shared_param_handle_midi_cc(ctx, channel, cc, value);
    }
}

/* ============================================================================
 * Input Port Enumeration Callback
 * ============================================================================ */

typedef struct {
    SharedContext* ctx;
} InEnumContext;

static void on_input_port_found(void* user_ctx, const libremidi_midi_in_port* port) {
    InEnumContext* ec = (InEnumContext*)user_ctx;
    SharedContext* ctx = ec->ctx;

    if (ctx->in_port_count >= SHARED_MAX_PORTS) return;

    libremidi_midi_in_port_clone(port, &ctx->in_ports[ctx->in_port_count]);
    ctx->in_port_count++;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void shared_midi_in_init_observer(SharedContext* ctx) {
    if (!ctx) return;

    /* Free existing ports */
    for (int i = 0; i < ctx->in_port_count; i++) {
        if (ctx->in_ports[i]) {
            libremidi_midi_in_port_free(ctx->in_ports[i]);
            ctx->in_ports[i] = NULL;
        }
    }
    ctx->in_port_count = 0;

    /* Use existing observer to enumerate input ports */
    if (!ctx->observer) {
        /* Observer not initialized - call output init first */
        shared_midi_init_observer(ctx);
    }

    if (!ctx->observer) {
        fprintf(stderr, "MIDI In: No observer available\n");
        return;
    }

    /* Enumerate input ports */
    InEnumContext ec = { .ctx = ctx };
    int ret = libremidi_midi_observer_enumerate_input_ports(ctx->observer, &ec, on_input_port_found);
    if (ret != 0) {
        fprintf(stderr, "MIDI In: Failed to enumerate input ports: %d\n", ret);
    }
}

void shared_midi_in_list_ports(SharedContext* ctx) {
    if (!ctx) return;

    shared_midi_in_init_observer(ctx);

    printf("MIDI inputs:\n");
    if (ctx->in_port_count == 0) {
        printf("  (none)\n");
    } else {
        for (int i = 0; i < ctx->in_port_count; i++) {
            const char* name = NULL;
            size_t len = 0;
            if (libremidi_midi_in_port_name(ctx->in_ports[i], &name, &len) == 0) {
                printf("  %d: %s\n", i, name);
            }
        }
    }
}

int shared_midi_in_get_port_count(SharedContext* ctx) {
    if (!ctx) return 0;
    return ctx->in_port_count;
}

const char* shared_midi_in_get_port_name(SharedContext* ctx, int port_idx) {
    if (!ctx || port_idx < 0 || port_idx >= ctx->in_port_count) return NULL;

    const char* name = NULL;
    size_t len = 0;
    if (libremidi_midi_in_port_name(ctx->in_ports[port_idx], &name, &len) == 0) {
        return name;
    }
    return NULL;
}

int shared_midi_in_open_port(SharedContext* ctx, int port_idx) {
    if (!ctx) return -1;

    shared_midi_in_init_observer(ctx);

    if (port_idx < 0 || port_idx >= ctx->in_port_count) {
        fprintf(stderr, "MIDI In: Invalid port index: %d (have %d ports)\n",
                port_idx, ctx->in_port_count);
        return -1;
    }

    /* Close existing input */
    if (ctx->midi_in != NULL) {
        libremidi_midi_in_free(ctx->midi_in);
        ctx->midi_in = NULL;
    }

    int ret = 0;

    /* Create MIDI configuration */
    libremidi_midi_configuration midi_conf;
    ret = libremidi_midi_configuration_init(&midi_conf);
    if (ret != 0) {
        fprintf(stderr, "MIDI In: Failed to init config\n");
        return -1;
    }

    midi_conf.version = MIDI1;
    midi_conf.in_port = ctx->in_ports[port_idx];

    /* Set up message callback */
    midi_conf.on_midi1_message.context = ctx;
    midi_conf.on_midi1_message.callback = on_midi_in_message;

    /* Create API configuration */
    libremidi_api_configuration api_conf;
    ret = libremidi_midi_api_configuration_init(&api_conf);
    if (ret != 0) {
        fprintf(stderr, "MIDI In: Failed to init API config\n");
        return -1;
    }

    api_conf.configuration_type = Input;
    api_conf.api = UNSPECIFIED;

    /* Open input */
    ret = libremidi_midi_in_new(&midi_conf, &api_conf, &ctx->midi_in);
    if (ret != 0) {
        fprintf(stderr, "MIDI In: Failed to open input: %d\n", ret);
        return -1;
    }

    const char* name = shared_midi_in_get_port_name(ctx, port_idx);
    fprintf(stderr, "[MIDI In] Opened port %d: %s\n", port_idx, name ? name : "(unknown)");

    return 0;
}

int shared_midi_in_open_virtual(SharedContext* ctx, const char* name) {
    if (!ctx || !name) return -1;

    shared_midi_in_init_observer(ctx);

    /* Close existing input */
    if (ctx->midi_in != NULL) {
        libremidi_midi_in_free(ctx->midi_in);
        ctx->midi_in = NULL;
    }

    int ret = 0;

    /* Create MIDI configuration */
    libremidi_midi_configuration midi_conf;
    ret = libremidi_midi_configuration_init(&midi_conf);
    if (ret != 0) {
        fprintf(stderr, "MIDI In: Failed to init config\n");
        return -1;
    }

    midi_conf.version = MIDI1;
    midi_conf.virtual_port = true;
    midi_conf.port_name = name;

    /* Set up message callback */
    midi_conf.on_midi1_message.context = ctx;
    midi_conf.on_midi1_message.callback = on_midi_in_message;

    /* Create API configuration */
    libremidi_api_configuration api_conf;
    ret = libremidi_midi_api_configuration_init(&api_conf);
    if (ret != 0) {
        fprintf(stderr, "MIDI In: Failed to init API config\n");
        return -1;
    }

    api_conf.configuration_type = Input;
    api_conf.api = UNSPECIFIED;

    /* Create virtual input */
    ret = libremidi_midi_in_new(&midi_conf, &api_conf, &ctx->midi_in);
    if (ret != 0) {
        fprintf(stderr, "MIDI In: Failed to create virtual input: %d\n", ret);
        return -1;
    }

    fprintf(stderr, "[MIDI In] Created virtual port: %s\n", name);

    return 0;
}

void shared_midi_in_close(SharedContext* ctx) {
    if (!ctx) return;

    if (ctx->midi_in != NULL) {
        libremidi_midi_in_free(ctx->midi_in);
        ctx->midi_in = NULL;
    }
}

int shared_midi_in_is_open(SharedContext* ctx) {
    return ctx && ctx->midi_in != NULL;
}

void shared_midi_in_cleanup(SharedContext* ctx) {
    if (!ctx) return;

    /* Close input */
    shared_midi_in_close(ctx);

    /* Free ports */
    for (int i = 0; i < ctx->in_port_count; i++) {
        if (ctx->in_ports[i]) {
            libremidi_midi_in_port_free(ctx->in_ports[i]);
            ctx->in_ports[i] = NULL;
        }
    }
    ctx->in_port_count = 0;
}
