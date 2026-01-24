/**
 * @file lua_fts.c
 * @brief Lua bindings for the FTS5 search index plugin.
 *
 * Exposes the FTS functionality via the loki.fts module:
 *
 *   loki.fts.index([path], [incremental])  -- Index directory
 *   loki.fts.search(query, [limit])        -- Full-text search
 *   loki.fts.find(pattern, [limit])        -- Path search (glob)
 *   loki.fts.stats()                       -- Get index statistics
 *   loki.fts.rebuild()                     -- Full reindex
 *   loki.fts.clear()                       -- Clear index
 */

#include "lua_fts.h"
#include "fts.h"

#include <lua.h>
#include <lauxlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Registry key for FTS index handle */
static const char *FTS_INDEX_KEY = "psnd.fts.index";

/* Default paths */
static const char *DEFAULT_DB_NAME = "index.db";
static const char *DEFAULT_PSND_DIR = ".psnd";

/* ----------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------------- */

static FTSIndex *get_fts_index(lua_State *L) {
    lua_pushlightuserdata(L, (void *)FTS_INDEX_KEY);
    lua_gettable(L, LUA_REGISTRYINDEX);
    FTSIndex *idx = (FTSIndex *)lua_touserdata(L, -1);
    lua_pop(L, 1);
    return idx;
}

static void set_fts_index(lua_State *L, FTSIndex *idx) {
    lua_pushlightuserdata(L, (void *)FTS_INDEX_KEY);
    lua_pushlightuserdata(L, idx);
    lua_settable(L, LUA_REGISTRYINDEX);
}

/* Build default .psnd path */
static int get_default_psnd_path(char *buf, size_t size) {
    const char *home = getenv("HOME");
    if (!home) return -1;
    snprintf(buf, size, "%s/%s", home, DEFAULT_PSND_DIR);
    return 0;
}

/* Build default database path */
static int get_default_db_path(char *buf, size_t size) {
    const char *home = getenv("HOME");
    if (!home) return -1;
    snprintf(buf, size, "%s/%s/%s", home, DEFAULT_PSND_DIR, DEFAULT_DB_NAME);
    return 0;
}

/* Ensure FTS index is open, lazily initializing if needed */
static FTSIndex *ensure_fts_index(lua_State *L) {
    FTSIndex *idx = get_fts_index(L);
    if (idx) return idx;

    /* Lazy initialization */
    char db_path[1024];
    if (get_default_db_path(db_path, sizeof(db_path)) != 0) {
        return NULL;
    }

    if (fts_open(&idx, db_path) != 0) {
        return NULL;
    }

    set_fts_index(L, idx);
    return idx;
}

/* ----------------------------------------------------------------------------
 * Lua API functions
 * ---------------------------------------------------------------------------- */

/**
 * loki.fts.index([path], [incremental]) -> count | nil, error
 *
 * Index a directory. Defaults to ~/.psnd with incremental indexing.
 */
static int lua_fts_index(lua_State *L) {
    FTSIndex *idx = ensure_fts_index(L);
    if (!idx) {
        lua_pushnil(L);
        lua_pushstring(L, "FTS index not initialized");
        return 2;
    }

    /* Get path argument or use default */
    char default_path[1024];
    const char *path;

    if (lua_gettop(L) >= 1 && !lua_isnil(L, 1)) {
        path = luaL_checkstring(L, 1);
    } else {
        if (get_default_psnd_path(default_path, sizeof(default_path)) != 0) {
            lua_pushnil(L);
            lua_pushstring(L, "Cannot determine default path");
            return 2;
        }
        path = default_path;
    }

    /* Get incremental flag (default true) */
    int incremental = 1;
    if (lua_gettop(L) >= 2) {
        incremental = lua_toboolean(L, 2);
    }

    FTSIndexFlags flags = incremental ? FTS_INDEX_INCREMENTAL : FTS_INDEX_FULL;
    int count = fts_index_directory(idx, path, flags);

    if (count < 0) {
        lua_pushnil(L);
        const char *err = fts_get_error(idx);
        lua_pushstring(L, err ? err : "Indexing failed");
        return 2;
    }

    lua_pushinteger(L, count);
    return 1;
}

/**
 * loki.fts.search(query, [limit]) -> results | nil, error
 *
 * Search indexed content. Returns array of {path, snippet, rank}.
 */
static int lua_fts_search(lua_State *L) {
    FTSIndex *idx = ensure_fts_index(L);
    if (!idx) {
        lua_pushnil(L);
        lua_pushstring(L, "FTS index not initialized");
        return 2;
    }

    const char *query = luaL_checkstring(L, 1);
    int limit = (int)luaL_optinteger(L, 2, 20);

    FTSResult *results;
    int count;

    if (fts_search(idx, query, &results, &count, limit) != 0) {
        lua_pushnil(L);
        const char *err = fts_get_error(idx);
        lua_pushstring(L, err ? err : "Search failed");
        return 2;
    }

    /* Build results table */
    lua_createtable(L, count, 0);

    for (int i = 0; i < count; i++) {
        lua_createtable(L, 0, 4);

        if (results[i].path) {
            lua_pushstring(L, results[i].path);
            lua_setfield(L, -2, "path");
        }

        if (results[i].snippet) {
            lua_pushstring(L, results[i].snippet);
            lua_setfield(L, -2, "snippet");
        }

        lua_pushinteger(L, results[i].line);
        lua_setfield(L, -2, "line");

        lua_pushnumber(L, results[i].rank);
        lua_setfield(L, -2, "rank");

        lua_rawseti(L, -2, i + 1);
    }

    fts_results_free(results, count);
    return 1;
}

/**
 * loki.fts.find(pattern, [limit]) -> results | nil, error
 *
 * Search file paths using glob pattern. Returns array of {path}.
 */
static int lua_fts_find(lua_State *L) {
    FTSIndex *idx = ensure_fts_index(L);
    if (!idx) {
        lua_pushnil(L);
        lua_pushstring(L, "FTS index not initialized");
        return 2;
    }

    const char *pattern = luaL_checkstring(L, 1);
    int limit = (int)luaL_optinteger(L, 2, 50);

    FTSResult *results;
    int count;

    if (fts_search_paths(idx, pattern, &results, &count, limit) != 0) {
        lua_pushnil(L);
        const char *err = fts_get_error(idx);
        lua_pushstring(L, err ? err : "Path search failed");
        return 2;
    }

    /* Build results table */
    lua_createtable(L, count, 0);

    for (int i = 0; i < count; i++) {
        lua_createtable(L, 0, 1);

        if (results[i].path) {
            lua_pushstring(L, results[i].path);
            lua_setfield(L, -2, "path");
        }

        lua_rawseti(L, -2, i + 1);
    }

    fts_results_free(results, count);
    return 1;
}

/**
 * loki.fts.stats() -> table | nil, error
 *
 * Get index statistics.
 */
static int lua_fts_stats(lua_State *L) {
    FTSIndex *idx = ensure_fts_index(L);
    if (!idx) {
        lua_pushnil(L);
        lua_pushstring(L, "FTS index not initialized");
        return 2;
    }

    FTSStats stats;
    if (fts_get_stats(idx, &stats) != 0) {
        lua_pushnil(L);
        lua_pushstring(L, "Cannot get stats");
        return 2;
    }

    lua_createtable(L, 0, 4);

    lua_pushinteger(L, stats.file_count);
    lua_setfield(L, -2, "file_count");

    lua_pushinteger(L, (lua_Integer)stats.total_bytes);
    lua_setfield(L, -2, "total_bytes");

    lua_pushinteger(L, (lua_Integer)stats.last_indexed);
    lua_setfield(L, -2, "last_indexed");

    lua_pushinteger(L, (lua_Integer)stats.index_size);
    lua_setfield(L, -2, "index_size");

    /* Add human-readable last_indexed */
    if (stats.last_indexed > 0) {
        time_t t = (time_t)stats.last_indexed;
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&t));
        lua_pushstring(L, time_str);
        lua_setfield(L, -2, "last_indexed_str");
    }

    return 1;
}

/**
 * loki.fts.rebuild() -> count | nil, error
 *
 * Full reindex of default path.
 */
static int lua_fts_rebuild(lua_State *L) {
    FTSIndex *idx = ensure_fts_index(L);
    if (!idx) {
        lua_pushnil(L);
        lua_pushstring(L, "FTS index not initialized");
        return 2;
    }

    char default_path[1024];
    if (get_default_psnd_path(default_path, sizeof(default_path)) != 0) {
        lua_pushnil(L);
        lua_pushstring(L, "Cannot determine default path");
        return 2;
    }

    int count = fts_index_directory(idx, default_path, FTS_INDEX_FULL);

    if (count < 0) {
        lua_pushnil(L);
        const char *err = fts_get_error(idx);
        lua_pushstring(L, err ? err : "Rebuild failed");
        return 2;
    }

    lua_pushinteger(L, count);
    return 1;
}

/**
 * loki.fts.clear() -> boolean
 *
 * Clear all indexed data.
 */
static int lua_fts_clear(lua_State *L) {
    FTSIndex *idx = ensure_fts_index(L);
    if (!idx) {
        lua_pushboolean(L, 0);
        return 1;
    }

    int rc = fts_clear(idx);
    lua_pushboolean(L, rc == 0);
    return 1;
}

/**
 * loki.fts.close() -> boolean
 *
 * Close the FTS index (for cleanup).
 */
static int lua_fts_close_index(lua_State *L) {
    FTSIndex *idx = get_fts_index(L);
    if (idx) {
        fts_close(idx);
        set_fts_index(L, NULL);
    }
    lua_pushboolean(L, 1);
    return 1;
}

/* ----------------------------------------------------------------------------
 * Module registration
 * ---------------------------------------------------------------------------- */

static const luaL_Reg fts_funcs[] = {
    {"index",   lua_fts_index},
    {"search",  lua_fts_search},
    {"find",    lua_fts_find},
    {"stats",   lua_fts_stats},
    {"rebuild", lua_fts_rebuild},
    {"clear",   lua_fts_clear},
    {"close",   lua_fts_close_index},
    {NULL, NULL}
};

void lua_register_fts_module(lua_State *L) {
    /* Assumes loki table is on top of stack */
    lua_newtable(L);

    for (const luaL_Reg *f = fts_funcs; f->name; f++) {
        lua_pushcfunction(L, f->func);
        lua_setfield(L, -2, f->name);
    }

    lua_setfield(L, -2, "fts");
}

void lua_fts_cleanup(lua_State *L) {
    FTSIndex *idx = get_fts_index(L);
    if (idx) {
        fts_close(idx);
        set_fts_index(L, NULL);
    }
}
