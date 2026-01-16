/* loki_joy.c - Joy concatenative language integration for Loki
 *
 * Integrates the Joy music language with Loki editor for livecoding.
 * Joy uses synchronous execution with stack-based semantics.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "joy.h"
#include "internal.h"
#include "lang_bridge.h"

/* Joy library headers */
#include "joy_runtime.h"
#include "joy_parser.h"
#include "joy_midi_backend.h"
#include "music_notation.h"
#include "music_context.h"
#include "midi_primitives.h"

/* ======================= Internal State ======================= */

/* Error buffer size */
#define JOY_ERROR_BUFSIZE 256

/* Per-context Joy state */
struct LokiJoyState {
    int initialized;
    JoyContext *joy_ctx;                /* The Joy interpreter context */
    char last_error[JOY_ERROR_BUFSIZE]; /* Last error message */
    jmp_buf error_jmp;                  /* Error recovery point */
    int in_eval;                        /* Currently evaluating (for error recovery) */
};

/* Get Joy state from editor context, returning NULL if not initialized */
static LokiJoyState* get_joy_state(editor_ctx_t *ctx) {
    return ctx ? ctx->joy_state : NULL;
}

/* ======================= Helper Functions ======================= */

static void set_state_error(LokiJoyState *state, const char *msg) {
    if (!state) return;
    if (msg) {
        strncpy(state->last_error, msg, sizeof(state->last_error) - 1);
        state->last_error[sizeof(state->last_error) - 1] = '\0';
    } else {
        state->last_error[0] = '\0';
    }
}

/* ======================= Initialization ======================= */

int loki_joy_init(editor_ctx_t *ctx) {
    if (!ctx) return -1;

    /* Check if already initialized for this context */
    if (ctx->joy_state && ctx->joy_state->initialized) {
        set_state_error(ctx->joy_state, "Joy already initialized");
        return -1;
    }

    /* Allocate state if needed */
    LokiJoyState *state = ctx->joy_state;
    if (!state) {
        state = (LokiJoyState *)calloc(1, sizeof(LokiJoyState));
        if (!state) return -1;
        ctx->joy_state = state;
    }

    /* Create Joy context */
    state->joy_ctx = joy_context_new();
    if (!state->joy_ctx) {
        set_state_error(state, "Failed to create Joy context");
        free(state);
        ctx->joy_state = NULL;
        return -1;
    }

    /* Register standard primitives */
    joy_register_primitives(state->joy_ctx);

    /* Initialize music notation system (creates MusicContext) */
    music_notation_init(state->joy_ctx);

    /* Register MIDI primitives (major, minor, tempo, vol, play, chord, etc.) */
    joy_midi_register_primitives(state->joy_ctx);

    /* Set parser dictionary for DEFINE support */
    joy_set_parser_dict(state->joy_ctx->dictionary);

    /* Initialize MIDI backend */
    if (joy_midi_init() != 0) {
        set_state_error(state, "Failed to initialize MIDI backend");
        music_notation_cleanup(state->joy_ctx);
        joy_context_free(state->joy_ctx);
        free(state);
        ctx->joy_state = NULL;
        return -1;
    }

    /* Open virtual MIDI port for Joy output */
    if (joy_midi_open_virtual("psnd-joy") != 0) {
        /* Non-fatal - can still use real ports */
    }

    state->initialized = 1;
    state->in_eval = 0;
    set_state_error(state, NULL);

    return 0;
}

void loki_joy_cleanup(editor_ctx_t *ctx) {
    if (!ctx) return;

    LokiJoyState *state = get_joy_state(ctx);
    if (!state || !state->initialized) return;

    /* Stop MIDI and send panic */
    joy_midi_panic();
    joy_midi_cleanup();

    /* Clean up music notation (frees MusicContext) */
    if (state->joy_ctx) {
        music_notation_cleanup(state->joy_ctx);
    }

    /* Clean up Joy context */
    if (state->joy_ctx) {
        joy_context_free(state->joy_ctx);
        state->joy_ctx = NULL;
    }

    state->initialized = 0;

    /* Free the state structure */
    free(state);
    ctx->joy_state = NULL;
}

int loki_joy_is_initialized(editor_ctx_t *ctx) {
    LokiJoyState *state = get_joy_state(ctx);
    return state ? state->initialized : 0;
}

/* ======================= Evaluation ======================= */

int loki_joy_eval(editor_ctx_t *ctx, const char *code) {
    LokiJoyState *state = get_joy_state(ctx);

    if (!state || !state->initialized) {
        if (state) set_state_error(state, "Joy not initialized");
        return -1;
    }

    if (!code || !code[0]) {
        return 0; /* Empty code is OK */
    }

    /* Set up error recovery */
    state->in_eval = 1;
    state->joy_ctx->error_jmp = &state->error_jmp;
    joy_set_current_context(state->joy_ctx);

    if (setjmp(state->error_jmp) != 0) {
        /* Error occurred during evaluation */
        state->in_eval = 0;
        state->joy_ctx->error_jmp = NULL;
        /* Error message was set by joy_error() */
        return -1;
    }

    /* Evaluate the code */
    joy_eval_line(state->joy_ctx, code);

    state->in_eval = 0;
    state->joy_ctx->error_jmp = NULL;
    set_state_error(state, NULL);

    return 0;
}

int loki_joy_load_file(editor_ctx_t *ctx, const char *path) {
    LokiJoyState *state = get_joy_state(ctx);

    if (!state || !state->initialized) {
        if (state) set_state_error(state, "Joy not initialized");
        return -1;
    }

    if (!path) {
        set_state_error(state, "No file path provided");
        return -1;
    }

    /* Set up error recovery */
    state->in_eval = 1;
    state->joy_ctx->error_jmp = &state->error_jmp;
    joy_set_current_context(state->joy_ctx);

    if (setjmp(state->error_jmp) != 0) {
        state->in_eval = 0;
        state->joy_ctx->error_jmp = NULL;
        return -1;
    }

    int result = joy_load_file(state->joy_ctx, path);

    state->in_eval = 0;
    state->joy_ctx->error_jmp = NULL;

    if (result != 0) {
        set_state_error(state, "Failed to load file");
        return -1;
    }

    set_state_error(state, NULL);
    return 0;
}

int loki_joy_define(editor_ctx_t *ctx, const char *name, const char *body) {
    LokiJoyState *state = get_joy_state(ctx);

    if (!state || !state->initialized) {
        if (state) set_state_error(state, "Joy not initialized");
        return -1;
    }

    if (!name || !body) {
        set_state_error(state, "Name and body required");
        return -1;
    }

    /* Parse the body into a quotation */
    JoyQuotation *quot = joy_parse(body);
    if (!quot) {
        set_state_error(state, "Failed to parse definition body");
        return -1;
    }

    /* Define the word */
    joy_dict_define_quotation(state->joy_ctx->dictionary, name, quot);
    set_state_error(state, NULL);

    return 0;
}

/* ======================= Playback Control ======================= */

void loki_joy_stop(editor_ctx_t *ctx) {
    LokiJoyState *state = get_joy_state(ctx);
    if (!state || !state->initialized) return;

    joy_midi_panic();
}

int loki_joy_open_port(editor_ctx_t *ctx, int port_idx) {
    LokiJoyState *state = get_joy_state(ctx);
    if (!state || !state->initialized) {
        if (state) set_state_error(state, "Joy not initialized");
        return -1;
    }

    if (joy_midi_open_port(port_idx) != 0) {
        set_state_error(state, "Failed to open MIDI port");
        return -1;
    }

    set_state_error(state, NULL);
    return 0;
}

int loki_joy_open_virtual(editor_ctx_t *ctx, const char *name) {
    LokiJoyState *state = get_joy_state(ctx);
    if (!state || !state->initialized) {
        if (state) set_state_error(state, "Joy not initialized");
        return -1;
    }

    if (joy_midi_open_virtual(name) != 0) {
        set_state_error(state, "Failed to create virtual MIDI port");
        return -1;
    }

    set_state_error(state, NULL);
    return 0;
}

void loki_joy_list_ports(editor_ctx_t *ctx) {
    LokiJoyState *state = get_joy_state(ctx);
    if (!state || !state->initialized) return;

    joy_midi_list_ports();
}

/* ======================= Stack Operations ======================= */

void loki_joy_push_int(editor_ctx_t *ctx, int value) {
    LokiJoyState *state = get_joy_state(ctx);
    if (!state || !state->initialized) return;

    joy_stack_push(state->joy_ctx->stack, joy_integer(value));
}

void loki_joy_push_string(editor_ctx_t *ctx, const char *value) {
    LokiJoyState *state = get_joy_state(ctx);
    if (!state || !state->initialized) return;

    joy_stack_push(state->joy_ctx->stack, joy_string(value));
}

int loki_joy_stack_depth(editor_ctx_t *ctx) {
    LokiJoyState *state = get_joy_state(ctx);
    if (!state || !state->initialized) return 0;

    return (int)joy_stack_depth(state->joy_ctx->stack);
}

void loki_joy_stack_clear(editor_ctx_t *ctx) {
    LokiJoyState *state = get_joy_state(ctx);
    if (!state || !state->initialized) return;

    joy_stack_clear(state->joy_ctx->stack);
}

void loki_joy_stack_print(editor_ctx_t *ctx) {
    LokiJoyState *state = get_joy_state(ctx);
    if (!state || !state->initialized) return;

    joy_stack_print(state->joy_ctx->stack);
}

/* ======================= Utility Functions ======================= */

const char *loki_joy_get_error(editor_ctx_t *ctx) {
    LokiJoyState *state = get_joy_state(ctx);
    if (!state) return NULL;

    return state->last_error[0] ? state->last_error : NULL;
}

/* ======================= Language Bridge Registration ======================= */

/* Wrapper for backend configuration */
static int joy_bridge_configure_backend(editor_ctx_t *ctx, const char *sf_path, const char *csd_path) {
    (void)ctx;  /* Joy uses global backend state */

    /* CSD takes precedence over soundfont */
    if (csd_path && *csd_path) {
        if (joy_csound_load(csd_path) == 0) {
            if (joy_csound_enable() == 0) {
                return 0;  /* Success with Csound */
            }
        }
        return -1;  /* Csound requested but failed */
    }

    if (sf_path && *sf_path) {
        if (joy_tsf_load_soundfont(sf_path) == 0) {
            if (joy_tsf_enable() == 0) {
                return 0;  /* Success with TSF */
            }
        }
        return -1;  /* Soundfont requested but failed */
    }

    return 1;  /* No backend requested */
}

/* Language operations for Joy */
static const LokiLangOps joy_lang_ops = {
    .name = "joy",
    .extensions = {".joy", NULL},

    /* Lifecycle */
    .init = loki_joy_init,
    .cleanup = loki_joy_cleanup,
    .is_initialized = loki_joy_is_initialized,

    /* Main loop - Joy doesn't need async callbacks */
    .check_callbacks = NULL,

    /* Playback */
    .eval = loki_joy_eval,
    .stop = loki_joy_stop,
    .is_playing = NULL,  /* Joy is synchronous */

    /* Export - Joy doesn't support MIDI export yet */
    .has_events = NULL,
    .populate_shared_buffer = NULL,

    /* Error */
    .get_error = loki_joy_get_error,

    /* Backend configuration */
    .configure_backend = joy_bridge_configure_backend,
};

/* Register Joy with the language bridge at startup */
__attribute__((constructor))
static void joy_register_language(void) {
    loki_lang_register(&joy_lang_ops);
}
