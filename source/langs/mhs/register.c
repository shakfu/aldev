/**
 * @file register.c
 * @brief MHS (Micro Haskell MIDI) editor integration for Loki.
 *
 * Registers MHS with the Loki editor's language bridge and provides
 * Lua API bindings (loki.mhs namespace).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "loki/internal.h"
#include "loki/lang_bridge.h"
#include "loki/lua.h"
#include "mhs_context.h"
#include "midi_ffi.h"

#include "lua.h"
#include "lauxlib.h"

/* ======================= Lua API Bindings ======================= */

/* loki.mhs.init() - Initialize MHS */
static int lua_mhs_init(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    int result = loki_mhs_init(ctx);
    if (result != 0) {
        const char *err = loki_mhs_get_error(ctx);
        lua_pushnil(L);
        lua_pushstring(L, err ? err : "Failed to initialize MHS");
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

/* loki.mhs.cleanup() - Cleanup MHS */
static int lua_mhs_cleanup(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    loki_mhs_cleanup(ctx);
    return 0;
}

/* loki.mhs.is_initialized() - Check if initialized */
static int lua_mhs_is_initialized(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    lua_pushboolean(L, loki_mhs_is_initialized(ctx));
    return 1;
}

/* loki.mhs.eval(code) - Evaluate Haskell code */
static int lua_mhs_eval(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    const char *code = luaL_checkstring(L, 1);
    int result = loki_mhs_eval(ctx, code);
    if (result != 0) {
        const char *err = loki_mhs_get_error(ctx);
        lua_pushnil(L);
        lua_pushstring(L, err ? err : "Evaluation failed");
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

/* loki.mhs.eval_file(path) - Evaluate Haskell file */
static int lua_mhs_eval_file(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    const char *path = luaL_checkstring(L, 1);
    int result = loki_mhs_eval_file(ctx, path);
    if (result != 0) {
        const char *err = loki_mhs_get_error(ctx);
        lua_pushnil(L);
        lua_pushstring(L, err ? err : "File evaluation failed");
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

/* loki.mhs.stop() - Stop playback */
static int lua_mhs_stop(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    loki_mhs_stop(ctx);
    return 0;
}

/* loki.mhs.is_playing() - Check if playing */
static int lua_mhs_is_playing(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    lua_pushboolean(L, loki_mhs_is_playing(ctx));
    return 1;
}

/* loki.mhs.get_error() - Get last error */
static int lua_mhs_get_error(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    const char *err = loki_mhs_get_error(ctx);
    if (err) {
        lua_pushstring(L, err);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

/* loki.mhs.list_ports() - List MIDI ports */
static int lua_mhs_list_ports(lua_State *L) {
    int count = loki_mhs_list_ports();
    lua_createtable(L, count, 0);
    for (int i = 0; i < count; i++) {
        lua_pushstring(L, loki_mhs_port_name(i));
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

/* loki.mhs.open_port(index) - Open MIDI port by index */
static int lua_mhs_open_port(lua_State *L) {
    int index = (int)luaL_checkinteger(L, 1) - 1;  /* Convert from 1-indexed */
    int result = loki_mhs_open_port(index);
    if (result != 0) {
        lua_pushnil(L);
        lua_pushstring(L, "Failed to open port");
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

/* loki.mhs.open_virtual(name) - Open virtual MIDI port */
static int lua_mhs_open_virtual(lua_State *L) {
    const char *name = luaL_optstring(L, 1, "psnd-mhs");
    int result = loki_mhs_open_virtual(name);
    if (result != 0) {
        lua_pushnil(L);
        lua_pushstring(L, "Failed to open virtual port");
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

/* Register MHS Lua API under loki.mhs */
static void mhs_register_lua_api(lua_State *L) {
    if (!loki_lua_begin_api(L, "mhs")) return;

    loki_lua_add_func(L, "init", lua_mhs_init);
    loki_lua_add_func(L, "cleanup", lua_mhs_cleanup);
    loki_lua_add_func(L, "is_initialized", lua_mhs_is_initialized);
    loki_lua_add_func(L, "eval", lua_mhs_eval);
    loki_lua_add_func(L, "eval_file", lua_mhs_eval_file);
    loki_lua_add_func(L, "stop", lua_mhs_stop);
    loki_lua_add_func(L, "is_playing", lua_mhs_is_playing);
    loki_lua_add_func(L, "get_error", lua_mhs_get_error);
    loki_lua_add_func(L, "list_ports", lua_mhs_list_ports);
    loki_lua_add_func(L, "open_port", lua_mhs_open_port);
    loki_lua_add_func(L, "open_virtual", lua_mhs_open_virtual);

    loki_lua_end_api(L, "mhs");
}

/* ======================= Language Bridge Wrappers ======================= */

/* Bridge init wrapper */
static int mhs_bridge_init(editor_ctx_t *ctx) {
    return loki_mhs_init(ctx);
}

/* Bridge eval wrapper */
static int mhs_bridge_eval(editor_ctx_t *ctx, const char *code) {
    return loki_mhs_eval(ctx, code);
}

/* Bridge stop wrapper */
static void mhs_bridge_stop(editor_ctx_t *ctx) {
    loki_mhs_stop(ctx);
}

/* ======================= Language Operations ======================= */

static const LokiLangOps mhs_lang_ops = {
    .name = "mhs",
    .extensions = {".hs", ".mhs", NULL},

    /* Lifecycle */
    .init = mhs_bridge_init,
    .cleanup = loki_mhs_cleanup,
    .is_initialized = loki_mhs_is_initialized,

    /* Main loop - MHS is synchronous */
    .check_callbacks = NULL,

    /* Playback */
    .eval = mhs_bridge_eval,
    .stop = mhs_bridge_stop,
    .is_playing = loki_mhs_is_playing,

    /* Export (not supported yet) */
    .has_events = NULL,
    .populate_shared_buffer = NULL,

    /* Error */
    .get_error = loki_mhs_get_error,

    /* Backend configuration (not supported yet) */
    .configure_backend = NULL,

    /* Lua API registration */
    .register_lua_api = mhs_register_lua_api,
};

/* ======================= Registration ======================= */

/* Register MHS with the language bridge - called from loki_lang_init() */
void mhs_loki_lang_init(void) {
    loki_lang_register(&mhs_lang_ops);
}
