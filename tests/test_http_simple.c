/* Simple test to debug HTTP security */
#include "test_framework.h"
#include "loki/core.h"
#include "loki/lua.h"
#include "loki_internal.h"
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

TEST(simple_test) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);
    ctx.L = loki_lua_bootstrap(&ctx, NULL);

    ASSERT_NOT_NULL(ctx.L);

    /* Just try to call the function and see if it exists */
    lua_getglobal(ctx.L, "loki");
    ASSERT_TRUE(lua_istable(ctx.L, -1));

    lua_getfield(ctx.L, -1, "async_http");
    ASSERT_TRUE(lua_isfunction(ctx.L, -1));

    if (ctx.L) lua_close(ctx.L);
    editor_ctx_free(&ctx);
}

TEST(reject_ftp) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);
    ctx.L = loki_lua_bootstrap(&ctx, NULL);

    const char *code = "return loki.async_http('ftp://test.com', 'GET', nil, {}, 'cb')";
    int result = luaL_dostring(ctx.L, code);

    ASSERT_EQ(result, 0);
    ASSERT_TRUE(lua_isnil(ctx.L, -1));

    if (ctx.L) lua_close(ctx.L);
    editor_ctx_free(&ctx);
}

BEGIN_TEST_SUITE("HTTP Simple")
    RUN_TEST(simple_test);
    RUN_TEST(reject_ftp);
END_TEST_SUITE()
