/* loki_joy.c - Joy concatenative language integration for Loki
 *
 * Integrates the Joy music language with Loki editor for livecoding.
 * Joy uses synchronous execution with stack-based semantics.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "psnd.h"
#include "register.h"
#include "loki/internal.h"
#include "loki/lang_bridge.h"
#include "loki/lua.h"  /* For loki_lua_get_editor_context */
#include "lauxlib.h"   /* For luaL_checkstring, luaL_checkinteger, etc. */

/* Joy library headers */
#include "joy_runtime.h"
#include "joy_parser.h"
#include "joy_midi_backend.h"
#include "music_notation.h"
#include "music_context.h"
#include "midi_primitives.h"

/* Shared context */
#include "shared/context.h"

/* ======================= Internal State ======================= */

/* Error buffer size */
#define JOY_ERROR_BUFSIZE 256

/* Per-context Joy state */
struct LokiJoyState {
    int initialized;
    JoyContext *joy_ctx;                /* The Joy interpreter context */
    SharedContext *shared;              /* Editor-owned shared context */
    char last_error[JOY_ERROR_BUFSIZE]; /* Last error message */
    jmp_buf error_jmp;                  /* Error recovery point */
    int in_eval;                        /* Currently evaluating (for error recovery) */
    lua_State *L;                       /* Lua state for registered primitives */
    int lua_registry_ref;               /* Lua registry ref for primitive callbacks table */
};

/* Get Joy state from editor context, returning NULL if not initialized */
static LokiJoyState* get_joy_state(editor_ctx_t *ctx) {
    return ctx ? ctx->model.joy_state : NULL;
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

/* ======================= Lua Primitive Callback Support ======================= */

/* Registry key for storing the primitive callbacks table */
static const char *JOY_LUA_PRIMITIVES_KEY = "joy_lua_primitives";

/* Global pointer to current Joy state (for primitive wrapper access) */
static LokiJoyState *g_current_lua_state = NULL;

/* Convert a JoyValue to a Lua value and push it onto the Lua stack */
static void joy_value_to_lua(lua_State *L, JoyValue v) {
    switch (v.type) {
        case JOY_INTEGER:
            lua_pushinteger(L, v.data.integer);
            break;
        case JOY_FLOAT:
            lua_pushnumber(L, v.data.floating);
            break;
        case JOY_BOOLEAN:
            lua_pushboolean(L, v.data.boolean);
            break;
        case JOY_CHAR:
            lua_pushlstring(L, &v.data.character, 1);
            break;
        case JOY_STRING:
            lua_pushstring(L, v.data.string ? v.data.string : "");
            break;
        case JOY_SYMBOL:
            /* Represent symbol as table {type="symbol", value="name"} */
            lua_newtable(L);
            lua_pushstring(L, "symbol");
            lua_setfield(L, -2, "type");
            lua_pushstring(L, v.data.symbol ? v.data.symbol : "");
            lua_setfield(L, -2, "value");
            break;
        case JOY_LIST:
            /* Convert list to Lua array */
            lua_newtable(L);
            if (v.data.list) {
                for (size_t i = 0; i < v.data.list->length; i++) {
                    joy_value_to_lua(L, v.data.list->items[i]);
                    lua_rawseti(L, -2, (int)i + 1);
                }
            }
            break;
        case JOY_QUOTATION:
            /* Represent quotation as table {type="quotation", value={...}} */
            lua_newtable(L);
            lua_pushstring(L, "quotation");
            lua_setfield(L, -2, "type");
            lua_newtable(L);
            if (v.data.quotation) {
                for (size_t i = 0; i < v.data.quotation->length; i++) {
                    joy_value_to_lua(L, v.data.quotation->terms[i]);
                    lua_rawseti(L, -2, (int)i + 1);
                }
            }
            lua_setfield(L, -2, "value");
            break;
        case JOY_SET:
            /* Represent set as table {type="set", value=number} */
            lua_newtable(L);
            lua_pushstring(L, "set");
            lua_setfield(L, -2, "type");
            lua_pushinteger(L, (lua_Integer)v.data.set);
            lua_setfield(L, -2, "value");
            break;
        case JOY_FILE:
            /* Files can't be passed to Lua - push nil */
            lua_pushnil(L);
            break;
    }
}

/* Convert a Lua value at given index to a JoyValue */
static JoyValue lua_to_joy_value(lua_State *L, int idx) {
    int t = lua_type(L, idx);

    switch (t) {
        case LUA_TNUMBER:
            if (lua_isinteger(L, idx)) {
                return joy_integer(lua_tointeger(L, idx));
            } else {
                return joy_float(lua_tonumber(L, idx));
            }
        case LUA_TBOOLEAN:
            return joy_boolean(lua_toboolean(L, idx));
        case LUA_TSTRING:
            return joy_string(lua_tostring(L, idx));
        case LUA_TTABLE: {
            /* Check if it's a typed table (quotation, symbol, set) */
            lua_getfield(L, idx, "type");
            if (lua_isstring(L, -1)) {
                const char *type = lua_tostring(L, -1);
                lua_pop(L, 1);

                if (strcmp(type, "quotation") == 0) {
                    lua_getfield(L, idx, "value");
                    JoyQuotation *quot = joy_quotation_new(8);
                    if (lua_istable(L, -1)) {
                        size_t len = lua_rawlen(L, -1);
                        for (size_t i = 1; i <= len; i++) {
                            lua_rawgeti(L, -1, (int)i);
                            JoyValue term = lua_to_joy_value(L, -1);
                            joy_quotation_push(quot, term);
                            lua_pop(L, 1);
                        }
                    }
                    lua_pop(L, 1);
                    JoyValue v;
                    v.type = JOY_QUOTATION;
                    v.data.quotation = quot;
                    return v;
                } else if (strcmp(type, "symbol") == 0) {
                    lua_getfield(L, idx, "value");
                    const char *name = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
                    lua_pop(L, 1);
                    return joy_symbol(name);
                } else if (strcmp(type, "set") == 0) {
                    lua_getfield(L, idx, "value");
                    uint64_t set = lua_isinteger(L, -1) ? (uint64_t)lua_tointeger(L, -1) : 0;
                    lua_pop(L, 1);
                    JoyValue v;
                    v.type = JOY_SET;
                    v.data.set = set;
                    return v;
                }
            } else {
                lua_pop(L, 1);
            }

            /* Plain table - treat as list */
            JoyList *list = joy_list_new(8);
            size_t len = lua_rawlen(L, idx);
            for (size_t i = 1; i <= len; i++) {
                lua_rawgeti(L, idx, (int)i);
                JoyValue item = lua_to_joy_value(L, -1);
                joy_list_push(list, item);
                lua_pop(L, 1);
            }
            JoyValue v;
            v.type = JOY_LIST;
            v.data.list = list;
            return v;
        }
        case LUA_TNIL:
        default:
            /* Default to false for nil/unknown */
            return joy_boolean(false);
    }
}

/* Convert Joy stack to Lua array table */
static void joy_stack_to_lua_table(lua_State *L, JoyStack *stack) {
    lua_newtable(L);
    for (size_t i = 0; i < stack->depth; i++) {
        joy_value_to_lua(L, stack->items[i]);
        lua_rawseti(L, -2, (int)i + 1);
    }
}

/* Replace Joy stack contents with Lua array table at top of Lua stack */
static int lua_table_to_joy_stack(lua_State *L, JoyStack *stack) {
    if (!lua_istable(L, -1)) {
        return -1;
    }

    /* Clear existing stack */
    joy_stack_clear(stack);

    /* Push each element from Lua table */
    size_t len = lua_rawlen(L, -1);
    for (size_t i = 1; i <= len; i++) {
        lua_rawgeti(L, -1, (int)i);
        JoyValue v = lua_to_joy_value(L, -1);
        joy_stack_push(stack, v);
        lua_pop(L, 1);
    }

    return 0;
}

/* Wrapper primitive that calls a Lua function */
static void joy_lua_primitive_wrapper(JoyContext *ctx) {
    LokiJoyState *state = g_current_lua_state;
    if (!state || !state->L || state->lua_registry_ref == LUA_NOREF) {
        joy_error("Lua primitive: no Lua state available");
        return;
    }

    lua_State *L = state->L;

    /* Get the primitive name from user_data (set during registration) */
    /* We use a lookup approach: find which primitive is currently executing */
    /* This is set by the caller via a thread-local or context field */

    /* Get the primitives table from registry */
    lua_rawgeti(L, LUA_REGISTRYINDEX, state->lua_registry_ref);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        joy_error("Lua primitive: registry corrupted");
        return;
    }

    /* The primitive name is stored in the JoyWord that called us.
     * We need to look it up. For now, we use a workaround:
     * store the current primitive name in the context's user_data temporarily.
     * This is set by a custom execution wrapper. */
    const char *prim_name = (const char *)ctx->user_data;
    if (!prim_name) {
        lua_pop(L, 1);
        joy_error("Lua primitive: no primitive name");
        return;
    }

    /* Get the callback function */
    lua_getfield(L, -1, prim_name);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        joy_error("Lua primitive: callback not found");
        return;
    }

    /* Remove primitives table, keep function */
    lua_remove(L, -2);

    /* Convert Joy stack to Lua table and pass as argument */
    joy_stack_to_lua_table(L, ctx->stack);

    /* Call the Lua function with 1 argument (stack), expecting 1-2 results */
    int status = lua_pcall(L, 1, 2, 0);
    if (status != LUA_OK) {
        const char *err = lua_tostring(L, -1);
        char buf[256];
        snprintf(buf, sizeof(buf), "Lua primitive error: %s", err ? err : "unknown");
        lua_pop(L, 1);
        joy_error(buf);
        return;
    }

    /* Check for error return (nil, "error message") */
    if (lua_isnil(L, -2)) {
        const char *err = lua_isstring(L, -1) ? lua_tostring(L, -1) : "unknown error";
        char buf[256];
        snprintf(buf, sizeof(buf), "Lua primitive: %s", err);
        lua_pop(L, 2);
        joy_error(buf);
        return;
    }

    /* Convert returned table back to Joy stack */
    lua_pop(L, 1);  /* Pop second return value (nil or unused) */
    if (lua_table_to_joy_stack(L, ctx->stack) != 0) {
        lua_pop(L, 1);
        joy_error("Lua primitive: invalid return value (expected table)");
        return;
    }
    lua_pop(L, 1);
}

/* Lua primitive data structure */
typedef struct {
    char *name;
    LokiJoyState *state;
} LuaPrimitiveData;

/* Global registry for Lua primitives (indexed dispatch) */
#define MAX_LUA_PRIMITIVES 64
static LuaPrimitiveData g_lua_prim_registry[MAX_LUA_PRIMITIVES];
static int g_lua_prim_count = 0;

/* Indexed dispatch function */
static void dispatch_lua_prim_by_index(JoyContext *ctx, int idx) {
    if (idx < 0 || idx >= g_lua_prim_count) {
        joy_error("Lua primitive: invalid index");
        return;
    }

    LuaPrimitiveData *data = &g_lua_prim_registry[idx];
    if (!data->name || !data->state) {
        joy_error("Lua primitive: invalid data");
        return;
    }

    /* Set up state and call wrapper */
    g_current_lua_state = data->state;

    /* Temporarily set primitive name in user_data */
    void *saved = ctx->user_data;
    ctx->user_data = (void *)data->name;

    joy_lua_primitive_wrapper(ctx);

    ctx->user_data = saved;
}

/* Trampoline macros - create unique function for each slot */
#define MAKE_TRAMPOLINE(n) static void lua_prim_##n(JoyContext *ctx) { dispatch_lua_prim_by_index(ctx, n); }

MAKE_TRAMPOLINE(0)  MAKE_TRAMPOLINE(1)  MAKE_TRAMPOLINE(2)  MAKE_TRAMPOLINE(3)
MAKE_TRAMPOLINE(4)  MAKE_TRAMPOLINE(5)  MAKE_TRAMPOLINE(6)  MAKE_TRAMPOLINE(7)
MAKE_TRAMPOLINE(8)  MAKE_TRAMPOLINE(9)  MAKE_TRAMPOLINE(10) MAKE_TRAMPOLINE(11)
MAKE_TRAMPOLINE(12) MAKE_TRAMPOLINE(13) MAKE_TRAMPOLINE(14) MAKE_TRAMPOLINE(15)
MAKE_TRAMPOLINE(16) MAKE_TRAMPOLINE(17) MAKE_TRAMPOLINE(18) MAKE_TRAMPOLINE(19)
MAKE_TRAMPOLINE(20) MAKE_TRAMPOLINE(21) MAKE_TRAMPOLINE(22) MAKE_TRAMPOLINE(23)
MAKE_TRAMPOLINE(24) MAKE_TRAMPOLINE(25) MAKE_TRAMPOLINE(26) MAKE_TRAMPOLINE(27)
MAKE_TRAMPOLINE(28) MAKE_TRAMPOLINE(29) MAKE_TRAMPOLINE(30) MAKE_TRAMPOLINE(31)
MAKE_TRAMPOLINE(32) MAKE_TRAMPOLINE(33) MAKE_TRAMPOLINE(34) MAKE_TRAMPOLINE(35)
MAKE_TRAMPOLINE(36) MAKE_TRAMPOLINE(37) MAKE_TRAMPOLINE(38) MAKE_TRAMPOLINE(39)
MAKE_TRAMPOLINE(40) MAKE_TRAMPOLINE(41) MAKE_TRAMPOLINE(42) MAKE_TRAMPOLINE(43)
MAKE_TRAMPOLINE(44) MAKE_TRAMPOLINE(45) MAKE_TRAMPOLINE(46) MAKE_TRAMPOLINE(47)
MAKE_TRAMPOLINE(48) MAKE_TRAMPOLINE(49) MAKE_TRAMPOLINE(50) MAKE_TRAMPOLINE(51)
MAKE_TRAMPOLINE(52) MAKE_TRAMPOLINE(53) MAKE_TRAMPOLINE(54) MAKE_TRAMPOLINE(55)
MAKE_TRAMPOLINE(56) MAKE_TRAMPOLINE(57) MAKE_TRAMPOLINE(58) MAKE_TRAMPOLINE(59)
MAKE_TRAMPOLINE(60) MAKE_TRAMPOLINE(61) MAKE_TRAMPOLINE(62) MAKE_TRAMPOLINE(63)

/* Array of trampoline function pointers */
static JoyPrimitive g_lua_prim_trampolines[MAX_LUA_PRIMITIVES] = {
    lua_prim_0,  lua_prim_1,  lua_prim_2,  lua_prim_3,
    lua_prim_4,  lua_prim_5,  lua_prim_6,  lua_prim_7,
    lua_prim_8,  lua_prim_9,  lua_prim_10, lua_prim_11,
    lua_prim_12, lua_prim_13, lua_prim_14, lua_prim_15,
    lua_prim_16, lua_prim_17, lua_prim_18, lua_prim_19,
    lua_prim_20, lua_prim_21, lua_prim_22, lua_prim_23,
    lua_prim_24, lua_prim_25, lua_prim_26, lua_prim_27,
    lua_prim_28, lua_prim_29, lua_prim_30, lua_prim_31,
    lua_prim_32, lua_prim_33, lua_prim_34, lua_prim_35,
    lua_prim_36, lua_prim_37, lua_prim_38, lua_prim_39,
    lua_prim_40, lua_prim_41, lua_prim_42, lua_prim_43,
    lua_prim_44, lua_prim_45, lua_prim_46, lua_prim_47,
    lua_prim_48, lua_prim_49, lua_prim_50, lua_prim_51,
    lua_prim_52, lua_prim_53, lua_prim_54, lua_prim_55,
    lua_prim_56, lua_prim_57, lua_prim_58, lua_prim_59,
    lua_prim_60, lua_prim_61, lua_prim_62, lua_prim_63,
};

/* ======================= Initialization ======================= */

int loki_joy_init(editor_ctx_t *ctx) {
    if (!ctx) return -1;

    /* Check if already initialized for this context */
    if (ctx->model.joy_state && ctx->model.joy_state->initialized) {
        set_state_error(ctx->model.joy_state, "Joy already initialized");
        return -1;
    }

    /* Allocate state if needed */
    LokiJoyState *state = ctx->model.joy_state;
    if (!state) {
        state = (LokiJoyState *)calloc(1, sizeof(LokiJoyState));
        if (!state) return -1;
        ctx->model.joy_state = state;
    }

    /* Create Joy context */
    state->joy_ctx = joy_context_new();
    if (!state->joy_ctx) {
        set_state_error(state, "Failed to create Joy context");
        free(state);
        ctx->model.joy_state = NULL;
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

    /* Use editor-owned SharedContext instead of allocating our own.
     * This centralizes audio/MIDI/Link state across all languages. */
    if (ctx->model.shared) {
        state->shared = ctx->model.shared;
    } else {
        set_state_error(state, "No shared context available");
        music_notation_cleanup(state->joy_ctx);
        joy_context_free(state->joy_ctx);
        free(state);
        ctx->model.joy_state = NULL;
        return -1;
    }

    /* Link SharedContext to MusicContext so primitives can access it */
    MusicContext* mctx = music_get_context(state->joy_ctx);
    if (mctx) {
        music_context_set_shared(mctx, state->shared);
    }

    /* Open virtual MIDI port for Joy output */
    if (joy_midi_open_virtual(state->shared, PSND_MIDI_PORT_NAME) != 0) {
        /* Non-fatal - can still use real ports */
    }

    state->initialized = 1;
    state->in_eval = 0;
    state->L = NULL;  /* Set later when Lua API is registered */
    state->lua_registry_ref = LUA_NOREF;
    set_state_error(state, NULL);

    return 0;
}

void loki_joy_cleanup(editor_ctx_t *ctx) {
    if (!ctx) return;

    LokiJoyState *state = get_joy_state(ctx);
    if (!state || !state->initialized) return;

    /* Stop MIDI and send panic */
    joy_midi_panic(state->shared);

    /* SharedContext is NOT cleaned up here - editor owns it.
     * Just clear the pointer to avoid dangling reference. */
    state->shared = NULL;

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
    ctx->model.joy_state = NULL;
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

    joy_midi_panic(state->shared);
}

int loki_joy_open_port(editor_ctx_t *ctx, int port_idx) {
    LokiJoyState *state = get_joy_state(ctx);
    if (!state || !state->initialized) {
        if (state) set_state_error(state, "Joy not initialized");
        return -1;
    }

    if (joy_midi_open_port(state->shared, port_idx) != 0) {
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

    if (joy_midi_open_virtual(state->shared, name) != 0) {
        set_state_error(state, "Failed to create virtual MIDI port");
        return -1;
    }

    set_state_error(state, NULL);
    return 0;
}

void loki_joy_list_ports(editor_ctx_t *ctx) {
    LokiJoyState *state = get_joy_state(ctx);
    if (!state || !state->initialized) return;

    joy_midi_list_ports(state->shared);
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

/* Lua API: loki.joy.register_primitive(name, callback) - Register a Lua function as Joy primitive
 *
 * The callback receives the Joy stack as a Lua array (index 1 = bottom).
 * It should return the modified stack, or nil + error message on failure.
 *
 * Example:
 *   loki.joy.register_primitive("double", function(stack)
 *       if #stack < 1 then return nil, "stack underflow" end
 *       local top = table.remove(stack)
 *       table.insert(stack, top * 2)
 *       return stack
 *   end)
 */
static int lua_joy_register_primitive(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    LokiJoyState *state = get_joy_state(ctx);

    if (!state || !state->initialized) {
        lua_pushnil(L);
        lua_pushstring(L, "Joy not initialized");
        return 2;
    }

    /* Check arguments */
    const char *name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    if (!name || !*name) {
        lua_pushnil(L);
        lua_pushstring(L, "Primitive name required");
        return 2;
    }

    /* Store Lua state reference if not already done */
    if (state->L == NULL) {
        state->L = L;
    }

    /* Create or get the primitives table in registry */
    if (state->lua_registry_ref == LUA_NOREF) {
        lua_newtable(L);
        state->lua_registry_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    }

    /* Store the callback in the primitives table */
    lua_rawgeti(L, LUA_REGISTRYINDEX, state->lua_registry_ref);
    lua_pushvalue(L, 2);  /* Push the callback */
    lua_setfield(L, -2, name);  /* primitives[name] = callback */
    lua_pop(L, 1);  /* Pop primitives table */

    /* Check if we already have a slot for this primitive (replacement) */
    int prim_idx = -1;
    for (int i = 0; i < g_lua_prim_count; i++) {
        if (g_lua_prim_registry[i].name && strcmp(g_lua_prim_registry[i].name, name) == 0) {
            /* Reuse existing slot - just update the state pointer */
            g_lua_prim_registry[i].state = state;
            prim_idx = i;
            break;
        }
    }

    if (prim_idx < 0) {
        /* Need a new slot */
        if (g_lua_prim_count >= MAX_LUA_PRIMITIVES) {
            lua_pushnil(L);
            lua_pushstring(L, "Too many Lua primitives registered (max 64)");
            return 2;
        }

        prim_idx = g_lua_prim_count++;
        g_lua_prim_registry[prim_idx].name = strdup(name);
        g_lua_prim_registry[prim_idx].state = state;
    }

    /* Register the primitive with the corresponding trampoline function */
    JoyDict *dict = state->joy_ctx->dictionary;

    /* Remove existing definition if present */
    JoyWord *existing = joy_dict_lookup(dict, name);
    if (existing) {
        joy_dict_remove(dict, name);
    }

    /* Register with the indexed trampoline */
    joy_dict_define_user(dict, name, g_lua_prim_trampolines[prim_idx]);

    lua_pushboolean(L, 1);
    return 1;
}

/* Register joy module as loki.joy subtable */
static void joy_register_lua_api(lua_State *L) {
    if (!loki_lua_begin_api(L, "joy")) return;

    loki_lua_add_func(L, "init", lua_joy_init);
    loki_lua_add_func(L, "cleanup", lua_joy_cleanup);
    loki_lua_add_func(L, "is_initialized", lua_joy_is_initialized);
    loki_lua_add_func(L, "eval", lua_joy_eval);
    loki_lua_add_func(L, "load", lua_joy_load);
    loki_lua_add_func(L, "define", lua_joy_define);
    loki_lua_add_func(L, "stop", lua_joy_stop);
    loki_lua_add_func(L, "open_port", lua_joy_open_port);
    loki_lua_add_func(L, "open_virtual", lua_joy_open_virtual);
    loki_lua_add_func(L, "list_ports", lua_joy_list_ports);
    loki_lua_add_func(L, "push", lua_joy_push);
    loki_lua_add_func(L, "stack_depth", lua_joy_stack_depth);
    loki_lua_add_func(L, "stack_clear", lua_joy_stack_clear);
    loki_lua_add_func(L, "stack_print", lua_joy_stack_print);
    loki_lua_add_func(L, "get_error", lua_joy_get_error);
    loki_lua_add_func(L, "register_primitive", lua_joy_register_primitive);

    loki_lua_end_api(L, "joy");
}

/* ======================= Language Bridge Registration ======================= */

/* Wrapper for backend configuration */
static int joy_bridge_configure_backend(editor_ctx_t *ctx, const char *sf_path, const char *csd_path) {
    LokiJoyState *state = get_joy_state(ctx);
    SharedContext *shared = state ? state->shared : NULL;

    /* CSD takes precedence over soundfont */
    if (csd_path && *csd_path) {
        if (joy_csound_load(csd_path) == 0) {
            if (joy_csound_enable(shared) == 0) {
                return 0;  /* Success with Csound */
            }
        }
        return -1;  /* Csound requested but failed */
    }

    if (sf_path && *sf_path) {
        if (joy_tsf_load_soundfont(sf_path) == 0) {
            if (joy_tsf_enable(shared) == 0) {
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

/* Register Joy with the language bridge - called from loki_lang_init() */
void joy_loki_lang_init(void) {
    loki_lang_register(&joy_lang_ops);
}
