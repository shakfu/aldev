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
#include "../internal.h"
#include "../lang_bridge.h"
#include "loki/lua.h"  /* For loki_lua_get_editor_context */
#include "lauxlib.h"   /* For luaL_checkstring, luaL_checkinteger, etc. */

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

/* ======================= Lua API Bindings ======================= */

/* Lua API: loki.joy.init() - Initialize Joy subsystem */
static int lua_joy_init(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);

    int result = loki_joy_init(ctx);
    if (result != 0) {
        lua_pushnil(L);
        const char *err = loki_joy_get_error(ctx);
        lua_pushstring(L, err ? err : "Failed to initialize Joy");
        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}

/* Lua API: loki.joy.cleanup() - Cleanup Joy subsystem */
static int lua_joy_cleanup(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    loki_joy_cleanup(ctx);
    return 0;
}

/* Lua API: loki.joy.is_initialized() - Check if Joy is initialized */
static int lua_joy_is_initialized(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    lua_pushboolean(L, loki_joy_is_initialized(ctx));
    return 1;
}

/* Lua API: loki.joy.eval(code) - Evaluate Joy code synchronously */
static int lua_joy_eval(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    const char *code = luaL_checkstring(L, 1);

    int result = loki_joy_eval(ctx, code);
    if (result != 0) {
        lua_pushnil(L);
        const char *err = loki_joy_get_error(ctx);
        lua_pushstring(L, err ? err : "Joy evaluation failed");
        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}

/* Lua API: loki.joy.load(path) - Load and evaluate a Joy source file */
static int lua_joy_load(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    const char *path = luaL_checkstring(L, 1);

    int result = loki_joy_load_file(ctx, path);
    if (result != 0) {
        lua_pushnil(L);
        const char *err = loki_joy_get_error(ctx);
        lua_pushstring(L, err ? err : "Failed to load Joy file");
        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}

/* Lua API: loki.joy.define(name, body) - Define a new Joy word */
static int lua_joy_define(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    const char *name = luaL_checkstring(L, 1);
    const char *body = luaL_checkstring(L, 2);

    int result = loki_joy_define(ctx, name, body);
    if (result != 0) {
        lua_pushnil(L);
        const char *err = loki_joy_get_error(ctx);
        lua_pushstring(L, err ? err : "Failed to define Joy word");
        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}

/* Lua API: loki.joy.stop() - Stop all MIDI playback and send panic */
static int lua_joy_stop(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    loki_joy_stop(ctx);
    return 0;
}

/* Lua API: loki.joy.open_port(index) - Open a MIDI output port by index */
static int lua_joy_open_port(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    int port_idx = (int)luaL_checkinteger(L, 1);

    int result = loki_joy_open_port(ctx, port_idx);
    if (result != 0) {
        lua_pushnil(L);
        const char *err = loki_joy_get_error(ctx);
        lua_pushstring(L, err ? err : "Failed to open MIDI port");
        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}

/* Lua API: loki.joy.open_virtual(name) - Create a virtual MIDI output port */
static int lua_joy_open_virtual(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    const char *name = NULL;

    if (lua_gettop(L) >= 1 && lua_isstring(L, 1)) {
        name = lua_tostring(L, 1);
    }

    int result = loki_joy_open_virtual(ctx, name);
    if (result != 0) {
        lua_pushnil(L);
        const char *err = loki_joy_get_error(ctx);
        lua_pushstring(L, err ? err : "Failed to create virtual MIDI port");
        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}

/* Lua API: loki.joy.list_ports() - List available MIDI output ports */
static int lua_joy_list_ports(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    loki_joy_list_ports(ctx);
    return 0;
}

/* Lua API: loki.joy.push(value) - Push a value onto the Joy stack */
static int lua_joy_push(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);

    if (lua_isinteger(L, 1) || lua_isnumber(L, 1)) {
        int value = (int)lua_tointeger(L, 1);
        loki_joy_push_int(ctx, value);
    } else if (lua_isstring(L, 1)) {
        const char *value = lua_tostring(L, 1);
        loki_joy_push_string(ctx, value);
    } else {
        lua_pushnil(L);
        lua_pushstring(L, "Joy push: expected integer or string");
        return 2;
    }

    return 0;
}

/* Lua API: loki.joy.stack_depth() - Get the current stack depth */
static int lua_joy_stack_depth(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    lua_pushinteger(L, loki_joy_stack_depth(ctx));
    return 1;
}

/* Lua API: loki.joy.stack_clear() - Clear the Joy stack */
static int lua_joy_stack_clear(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    loki_joy_stack_clear(ctx);
    return 0;
}

/* Lua API: loki.joy.stack_print() - Print the Joy stack (for debugging) */
static int lua_joy_stack_print(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    loki_joy_stack_print(ctx);
    return 0;
}

/* Lua API: loki.joy.get_error() - Get last error message */
static int lua_joy_get_error(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    const char *err = loki_joy_get_error(ctx);

    if (err) {
        lua_pushstring(L, err);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

/* Register joy module as loki.joy subtable */
static void joy_register_lua_api(lua_State *L) {
    /* Get loki global table */
    lua_getglobal(L, "loki");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    /* Create joy subtable */
    lua_newtable(L);

    lua_pushcfunction(L, lua_joy_init);
    lua_setfield(L, -2, "init");
    lua_pushcfunction(L, lua_joy_cleanup);
    lua_setfield(L, -2, "cleanup");
    lua_pushcfunction(L, lua_joy_is_initialized);
    lua_setfield(L, -2, "is_initialized");
    lua_pushcfunction(L, lua_joy_eval);
    lua_setfield(L, -2, "eval");
    lua_pushcfunction(L, lua_joy_load);
    lua_setfield(L, -2, "load");
    lua_pushcfunction(L, lua_joy_define);
    lua_setfield(L, -2, "define");
    lua_pushcfunction(L, lua_joy_stop);
    lua_setfield(L, -2, "stop");
    lua_pushcfunction(L, lua_joy_open_port);
    lua_setfield(L, -2, "open_port");
    lua_pushcfunction(L, lua_joy_open_virtual);
    lua_setfield(L, -2, "open_virtual");
    lua_pushcfunction(L, lua_joy_list_ports);
    lua_setfield(L, -2, "list_ports");
    lua_pushcfunction(L, lua_joy_push);
    lua_setfield(L, -2, "push");
    lua_pushcfunction(L, lua_joy_stack_depth);
    lua_setfield(L, -2, "stack_depth");
    lua_pushcfunction(L, lua_joy_stack_clear);
    lua_setfield(L, -2, "stack_clear");
    lua_pushcfunction(L, lua_joy_stack_print);
    lua_setfield(L, -2, "stack_print");
    lua_pushcfunction(L, lua_joy_get_error);
    lua_setfield(L, -2, "get_error");

    lua_setfield(L, -2, "joy");  /* Set as loki.joy */
    lua_pop(L, 1);  /* Pop loki table */
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

    /* Lua API registration */
    .register_lua_api = joy_register_lua_api,
};

/* Register Joy with the language bridge at startup */
__attribute__((constructor))
static void joy_register_language(void) {
    loki_lang_register(&joy_lang_ops);
}
