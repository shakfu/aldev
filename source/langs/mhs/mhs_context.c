/**
 * @file mhs_context.c
 * @brief MHS (Micro Haskell MIDI) state management for editor integration.
 *
 * Implements the lifecycle and playback functions for MHS in the Loki editor.
 */

#include "mhs_context.h"
#include "loki/internal.h"
#include "shared/context.h"
#include "midi_ffi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define PATH_MAX MAX_PATH
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* External MHS main (from mhs.c) */
extern int mhs_main(int argc, char **argv);

/* External SharedContext setter for midi_ffi */
extern void midi_ffi_set_shared(struct SharedContext *ctx);

/* Get MHS state from editor context */
static LokiMhsState *get_mhs_state(editor_ctx_t *ctx) {
    return ctx ? ctx->model.mhs_state : NULL;
}

/* Set error message */
static void set_error(LokiMhsState *state, const char *msg) {
    if (!state) return;
    if (msg) {
        strncpy(state->last_error, msg, MHS_ERROR_BUFSIZE - 1);
        state->last_error[MHS_ERROR_BUFSIZE - 1] = '\0';
    } else {
        state->last_error[0] = '\0';
    }
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

int loki_mhs_init(editor_ctx_t *ctx) {
    if (!ctx) return -1;

    /* Check if already initialized */
    if (ctx->model.mhs_state && ctx->model.mhs_state->initialized) {
        set_error(ctx->model.mhs_state, "MHS already initialized");
        return -1;
    }

    /* Allocate state if needed */
    LokiMhsState *state = ctx->model.mhs_state;
    if (!state) {
        state = (LokiMhsState *)calloc(1, sizeof(LokiMhsState));
        if (!state) return -1;
        ctx->model.mhs_state = state;
    }

    /* Get shared context from editor */
    if (!ctx->model.shared) {
        set_error(state, "No shared context available");
        free(state);
        ctx->model.mhs_state = NULL;
        return -1;
    }
    state->shared = ctx->model.shared;

    /* Route midi_ffi through SharedContext */
    midi_ffi_set_shared(state->shared);

    /* Initialize MIDI subsystem */
    if (mhs_midi_init() != 0) {
        set_error(state, "Failed to initialize MIDI");
        free(state);
        ctx->model.mhs_state = NULL;
        return -1;
    }

    /* Open virtual MIDI port */
    if (midi_open_virtual("psnd-mhs") != 0) {
        /* Try to open first available port instead */
        int port_count = midi_list_ports();
        if (port_count > 0) {
            if (midi_open(0) != 0) {
                set_error(state, "Failed to open MIDI port");
                mhs_midi_cleanup();
                free(state);
                ctx->model.mhs_state = NULL;
                return -1;
            }
        }
    }

    state->initialized = 1;
    state->is_playing = 0;
    set_error(state, NULL);

    return 0;
}

void loki_mhs_cleanup(editor_ctx_t *ctx) {
    if (!ctx) return;

    LokiMhsState *state = get_mhs_state(ctx);
    if (!state) return;

    /* Stop any active playback */
    if (state->is_playing) {
        midi_panic();
        state->is_playing = 0;
    }

    /* Clear SharedContext routing */
    midi_ffi_set_shared(NULL);

    /* Cleanup MIDI */
    mhs_midi_cleanup();

    state->initialized = 0;

    /* Free state */
    free(state);
    ctx->model.mhs_state = NULL;
}

int loki_mhs_is_initialized(editor_ctx_t *ctx) {
    LokiMhsState *state = get_mhs_state(ctx);
    return state ? state->initialized : 0;
}

/* ============================================================================
 * Playback Functions
 * ============================================================================ */

int loki_mhs_eval(editor_ctx_t *ctx, const char *code) {
    LokiMhsState *state = get_mhs_state(ctx);
    if (!state || !state->initialized) {
        if (state) set_error(state, "MHS not initialized");
        return -1;
    }

    if (!code || !*code) {
        set_error(state, "Empty code");
        return -1;
    }

    /* Create a temporary file for the code */
    char temp_path[PATH_MAX];
#ifdef _WIN32
    char temp_dir[PATH_MAX];
    GetTempPathA(PATH_MAX, temp_dir);
    snprintf(temp_path, sizeof(temp_path), "%s\\mhs_eval_%d.hs", temp_dir, (int)GetCurrentProcessId());
#else
    snprintf(temp_path, sizeof(temp_path), "/tmp/mhs_eval_%d.hs", (int)getpid());
#endif

    /* Write code to temp file */
    FILE *f = fopen(temp_path, "w");
    if (!f) {
        set_error(state, "Failed to create temp file");
        return -1;
    }

    /* Add standard imports for MIDI */
    fprintf(f, "import Midi\n");
    fprintf(f, "import Music\n");
    fprintf(f, "import MusicPerform\n\n");
    fprintf(f, "%s\n", code);
    fclose(f);

    /* Mark as playing */
    state->is_playing = 1;

    /* Run through MHS */
    char *args[] = {"mhs", "-r", temp_path, NULL};
    int result = mhs_main(3, args);

    /* Cleanup */
    state->is_playing = 0;
    unlink(temp_path);

    if (result != 0) {
        set_error(state, "MHS evaluation failed");
        return -1;
    }

    set_error(state, NULL);
    return 0;
}

int loki_mhs_eval_file(editor_ctx_t *ctx, const char *path) {
    LokiMhsState *state = get_mhs_state(ctx);
    if (!state || !state->initialized) {
        if (state) set_error(state, "MHS not initialized");
        return -1;
    }

    if (!path || !*path) {
        set_error(state, "Invalid file path");
        return -1;
    }

    /* Mark as playing */
    state->is_playing = 1;

    /* Run file through MHS */
    char *args[] = {"mhs", "-r", (char *)path, NULL};
    int result = mhs_main(3, args);

    state->is_playing = 0;

    if (result != 0) {
        set_error(state, "MHS file evaluation failed");
        return -1;
    }

    set_error(state, NULL);
    return 0;
}

void loki_mhs_stop(editor_ctx_t *ctx) {
    LokiMhsState *state = get_mhs_state(ctx);
    if (!state || !state->initialized) return;

    /* Send MIDI panic */
    midi_panic();
    state->is_playing = 0;
}

int loki_mhs_is_playing(editor_ctx_t *ctx) {
    LokiMhsState *state = get_mhs_state(ctx);
    return (state && state->is_playing) ? 1 : 0;
}

/* ============================================================================
 * Error Handling
 * ============================================================================ */

const char *loki_mhs_get_error(editor_ctx_t *ctx) {
    LokiMhsState *state = get_mhs_state(ctx);
    if (!state) return NULL;
    return state->last_error[0] ? state->last_error : NULL;
}

/* ============================================================================
 * MIDI Port Functions
 * ============================================================================ */

int loki_mhs_list_ports(void) {
    return midi_list_ports();
}

const char *loki_mhs_port_name(int index) {
    return midi_port_name(index);
}

int loki_mhs_open_port(int index) {
    return midi_open(index);
}

int loki_mhs_open_virtual(const char *name) {
    return midi_open_virtual(name);
}
