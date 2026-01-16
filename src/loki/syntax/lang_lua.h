/* lang_lua.h - Lua language definition for syntax highlighting */

#ifndef LOKI_LANG_LUA_H
#define LOKI_LANG_LUA_H

/* Minimal Lua keywords (for markdown code blocks) */
static char *Lua_HL_keywords[] = {
    "function","if","else","elseif","for","while","return","local","end",
    "string|","number|","boolean|","table|",NULL
};

static char *Lua_HL_extensions[] = {".lua",NULL};

#endif /* LOKI_LANG_LUA_H */
