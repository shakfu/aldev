#ifndef LOKI_LUA_H
#define LOKI_LUA_H

#include <lua.h>
#include "loki/core.h"  /* For editor_ctx_t typedef */

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*loki_lua_report_fn)(const char *message, void *userdata);

struct loki_lua_opts {
    int bind_editor;           /* Non-zero to load editor bindings */
    int bind_http;             /* Unused (HTTP support removed) */
    int load_config;           /* Non-zero to load .psnd/init.lua and ~/.psnd/init.lua */
    const char *config_override; /* Optional absolute path to init.lua */
    const char *project_root;  /* Optional project root for .psnd/ discovery */
    const char *extra_lua_path;/* Optional extra package.path entries */
    loki_lua_report_fn reporter; /* Optional reporter for init errors */
    void *reporter_userdata;   /* Context passed to reporter */
};

lua_State *loki_lua_bootstrap(editor_ctx_t *ctx, const struct loki_lua_opts *opts);
const char *loki_lua_runtime(void);
void loki_lua_bind_minimal(lua_State *L);
void loki_lua_bind_editor(lua_State *L);
int loki_lua_load_config(lua_State *L, const struct loki_lua_opts *opts);
void loki_lua_install_namespaces(lua_State *L);

#ifdef __cplusplus
}
#endif

#endif /* LOKI_LUA_H */
