/**
 * @file lua_fts.h
 * @brief Lua bindings header for the FTS5 search index plugin.
 */

#ifndef PSND_LUA_FTS_H
#define PSND_LUA_FTS_H

#include <lua.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register the loki.fts module.
 *
 * Call this during Lua binding setup with the loki table on top of stack.
 *
 * @param L Lua state.
 */
void lua_register_fts_module(lua_State *L);

/**
 * @brief Cleanup FTS resources.
 *
 * Call this before closing the Lua state to properly close the database.
 *
 * @param L Lua state.
 */
void lua_fts_cleanup(lua_State *L);

#ifdef __cplusplus
}
#endif

#endif /* PSND_LUA_FTS_H */
