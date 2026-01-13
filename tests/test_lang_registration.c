/* test_lang_registration.c - Unit tests for language registration helpers
 *
 * Tests for the helper functions extracted from lua_loki_register_language:
 * - extract_language_extensions()
 * - extract_language_keywords()
 * - extract_comment_delimiters()
 * - extract_separators()
 * - extract_highlight_flags()
 */

#include "test_framework.h"
#include "loki/core.h"
#include "loki/lua.h"
#include "loki_internal.h"
#include "loki_languages.h"
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <string.h>

/* Forward declarations for helper functions (they're static, so we need to test via the main function,
 * but we can test them indirectly through Lua) */

/* Helper to create a test Lua state */
static lua_State *create_test_lua_state() {
    lua_State *L = luaL_newstate();
    if (L) {
        luaL_openlibs(L);
    }
    return L;
}

/* Test: register_language with valid minimal config */
TEST(register_language_minimal_config) {
    lua_State *L = create_test_lua_state();
    ASSERT_NOT_NULL(L);

    /* Push loki table and register_language function */
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);
    ctx.L = loki_lua_bootstrap(&ctx, NULL);

    /* Create minimal config table */
    const char *code =
        "return loki.register_language({\n"
        "  name = 'TestLang',\n"
        "  extensions = {'.test'}\n"
        "})";

    int result = luaL_dostring(ctx.L, code);
    ASSERT_EQ(result, 0);

    /* Check return value */
    ASSERT_TRUE(lua_isboolean(ctx.L, -1));
    ASSERT_TRUE(lua_toboolean(ctx.L, -1));

    /* Cleanup */
    if (ctx.L) lua_close(ctx.L);
    editor_ctx_free(&ctx);
}

/* Test: register_language with full config */
TEST(register_language_full_config) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);
    ctx.L = loki_lua_bootstrap(&ctx, NULL);

    const char *code =
        "return loki.register_language({\n"
        "  name = 'FullLang',\n"
        "  extensions = {'.fl', '.full'},\n"
        "  keywords = {'if', 'then', 'else'},\n"
        "  types = {'int', 'string'},\n"
        "  line_comment = '//',\n"
        "  block_comment_start = '/*',\n"
        "  block_comment_end = '*/',\n"
        "  separators = ',.()+-',\n"
        "  highlight_strings = true,\n"
        "  highlight_numbers = true\n"
        "})";

    int result = luaL_dostring(ctx.L, code);
    ASSERT_EQ(result, 0);
    ASSERT_TRUE(lua_isboolean(ctx.L, -1));
    ASSERT_TRUE(lua_toboolean(ctx.L, -1));

    /* Cleanup */
    if (ctx.L) lua_close(ctx.L);
    editor_ctx_free(&ctx);
}

/* Test: missing name field */
TEST(register_language_missing_name) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);
    ctx.L = loki_lua_bootstrap(&ctx, NULL);

    const char *code =
        "return loki.register_language({\n"
        "  extensions = {'.test'}\n"
        "})";

    int result = luaL_dostring(ctx.L, code);
    ASSERT_EQ(result, 0);

    /* Should return nil and error message */
    ASSERT_TRUE(lua_isnil(ctx.L, -2));
    ASSERT_TRUE(lua_isstring(ctx.L, -1));

    /* Cleanup */
    if (ctx.L) lua_close(ctx.L);
    editor_ctx_free(&ctx);
}

/* Test: missing extensions field */
TEST(register_language_missing_extensions) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);
    ctx.L = loki_lua_bootstrap(&ctx, NULL);

    const char *code =
        "return loki.register_language({\n"
        "  name = 'TestLang'\n"
        "})";

    int result = luaL_dostring(ctx.L, code);
    ASSERT_EQ(result, 0);

    /* Should return nil and error message */
    ASSERT_TRUE(lua_isnil(ctx.L, -2));
    ASSERT_TRUE(lua_isstring(ctx.L, -1));

    /* Cleanup */
    if (ctx.L) lua_close(ctx.L);
    editor_ctx_free(&ctx);
}

/* Test: empty extensions table */
TEST(register_language_empty_extensions) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);
    ctx.L = loki_lua_bootstrap(&ctx, NULL);

    const char *code =
        "return loki.register_language({\n"
        "  name = 'TestLang',\n"
        "  extensions = {}\n"
        "})";

    int result = luaL_dostring(ctx.L, code);
    ASSERT_EQ(result, 0);

    /* Should return nil and error message */
    ASSERT_TRUE(lua_isnil(ctx.L, -2));
    ASSERT_TRUE(lua_isstring(ctx.L, -1));

    /* Cleanup */
    if (ctx.L) lua_close(ctx.L);
    editor_ctx_free(&ctx);
}

/* Test: extension without dot */
TEST(register_language_extension_without_dot) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);
    ctx.L = loki_lua_bootstrap(&ctx, NULL);

    const char *code =
        "return loki.register_language({\n"
        "  name = 'TestLang',\n"
        "  extensions = {'test'}\n"  /* Missing dot */
        "})";

    int result = luaL_dostring(ctx.L, code);
    ASSERT_EQ(result, 0);

    /* Should return nil and error message */
    ASSERT_TRUE(lua_isnil(ctx.L, -2));
    ASSERT_TRUE(lua_isstring(ctx.L, -1));

    /* Cleanup */
    if (ctx.L) lua_close(ctx.L);
    editor_ctx_free(&ctx);
}

/* Test: multiple extensions */
TEST(register_language_multiple_extensions) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);
    ctx.L = loki_lua_bootstrap(&ctx, NULL);

    const char *code =
        "return loki.register_language({\n"
        "  name = 'TestLang',\n"
        "  extensions = {'.t1', '.t2', '.t3'}\n"
        "})";

    int result = luaL_dostring(ctx.L, code);
    ASSERT_EQ(result, 0);
    ASSERT_TRUE(lua_isboolean(ctx.L, -1));
    ASSERT_TRUE(lua_toboolean(ctx.L, -1));

    /* Cleanup */
    if (ctx.L) lua_close(ctx.L);
    editor_ctx_free(&ctx);
}

/* Test: keywords without types */
TEST(register_language_keywords_only) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);
    ctx.L = loki_lua_bootstrap(&ctx, NULL);

    const char *code =
        "return loki.register_language({\n"
        "  name = 'TestLang',\n"
        "  extensions = {'.test'},\n"
        "  keywords = {'if', 'else', 'while'}\n"
        "})";

    int result = luaL_dostring(ctx.L, code);
    ASSERT_EQ(result, 0);
    ASSERT_TRUE(lua_isboolean(ctx.L, -1));
    ASSERT_TRUE(lua_toboolean(ctx.L, -1));

    /* Cleanup */
    if (ctx.L) lua_close(ctx.L);
    editor_ctx_free(&ctx);
}

/* Test: types without keywords */
TEST(register_language_types_only) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);
    ctx.L = loki_lua_bootstrap(&ctx, NULL);

    const char *code =
        "return loki.register_language({\n"
        "  name = 'TestLang',\n"
        "  extensions = {'.test'},\n"
        "  types = {'int', 'string', 'bool'}\n"
        "})";

    int result = luaL_dostring(ctx.L, code);
    ASSERT_EQ(result, 0);
    ASSERT_TRUE(lua_isboolean(ctx.L, -1));
    ASSERT_TRUE(lua_toboolean(ctx.L, -1));

    /* Cleanup */
    if (ctx.L) lua_close(ctx.L);
    editor_ctx_free(&ctx);
}

/* Test: line comment only */
TEST(register_language_line_comment_only) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);
    ctx.L = loki_lua_bootstrap(&ctx, NULL);

    const char *code =
        "return loki.register_language({\n"
        "  name = 'TestLang',\n"
        "  extensions = {'.test'},\n"
        "  line_comment = '#'\n"
        "})";

    int result = luaL_dostring(ctx.L, code);
    ASSERT_EQ(result, 0);
    ASSERT_TRUE(lua_isboolean(ctx.L, -1));
    ASSERT_TRUE(lua_toboolean(ctx.L, -1));

    /* Cleanup */
    if (ctx.L) lua_close(ctx.L);
    editor_ctx_free(&ctx);
}

/* Test: line comment too long */
TEST(register_language_line_comment_too_long) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);
    ctx.L = loki_lua_bootstrap(&ctx, NULL);

    const char *code =
        "return loki.register_language({\n"
        "  name = 'TestLang',\n"
        "  extensions = {'.test'},\n"
        "  line_comment = '####'\n"  /* Too long (max 3 chars) */
        "})";

    int result = luaL_dostring(ctx.L, code);
    ASSERT_EQ(result, 0);

    /* Should return nil and error message */
    ASSERT_TRUE(lua_isnil(ctx.L, -2));
    ASSERT_TRUE(lua_isstring(ctx.L, -1));

    /* Cleanup */
    if (ctx.L) lua_close(ctx.L);
    editor_ctx_free(&ctx);
}

/* Test: block comments */
TEST(register_language_block_comments) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);
    ctx.L = loki_lua_bootstrap(&ctx, NULL);

    const char *code =
        "return loki.register_language({\n"
        "  name = 'TestLang',\n"
        "  extensions = {'.test'},\n"
        "  block_comment_start = '/*',\n"
        "  block_comment_end = '*/'\n"
        "})";

    int result = luaL_dostring(ctx.L, code);
    ASSERT_EQ(result, 0);
    ASSERT_TRUE(lua_isboolean(ctx.L, -1));
    ASSERT_TRUE(lua_toboolean(ctx.L, -1));

    /* Cleanup */
    if (ctx.L) lua_close(ctx.L);
    editor_ctx_free(&ctx);
}

/* Test: block comment too long */
TEST(register_language_block_comment_too_long) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);
    ctx.L = loki_lua_bootstrap(&ctx, NULL);

    const char *code =
        "return loki.register_language({\n"
        "  name = 'TestLang',\n"
        "  extensions = {'.test'},\n"
        "  block_comment_start = '/*****'\n"  /* Too long (max 5 chars) */
        "})";

    int result = luaL_dostring(ctx.L, code);
    ASSERT_EQ(result, 0);

    /* Should return nil and error message */
    ASSERT_TRUE(lua_isnil(ctx.L, -2));
    ASSERT_TRUE(lua_isstring(ctx.L, -1));

    /* Cleanup */
    if (ctx.L) lua_close(ctx.L);
    editor_ctx_free(&ctx);
}

/* Test: custom separators */
TEST(register_language_custom_separators) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);
    ctx.L = loki_lua_bootstrap(&ctx, NULL);

    const char *code =
        "return loki.register_language({\n"
        "  name = 'TestLang',\n"
        "  extensions = {'.test'},\n"
        "  separators = ',.()'\n"
        "})";

    int result = luaL_dostring(ctx.L, code);
    ASSERT_EQ(result, 0);
    ASSERT_TRUE(lua_isboolean(ctx.L, -1));
    ASSERT_TRUE(lua_toboolean(ctx.L, -1));

    /* Cleanup */
    if (ctx.L) lua_close(ctx.L);
    editor_ctx_free(&ctx);
}

/* Test: disable string highlighting */
TEST(register_language_disable_string_highlighting) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);
    ctx.L = loki_lua_bootstrap(&ctx, NULL);

    const char *code =
        "return loki.register_language({\n"
        "  name = 'TestLang',\n"
        "  extensions = {'.test'},\n"
        "  highlight_strings = false\n"
        "})";

    int result = luaL_dostring(ctx.L, code);
    ASSERT_EQ(result, 0);
    ASSERT_TRUE(lua_isboolean(ctx.L, -1));
    ASSERT_TRUE(lua_toboolean(ctx.L, -1));

    /* Cleanup */
    if (ctx.L) lua_close(ctx.L);
    editor_ctx_free(&ctx);
}

/* Test: disable number highlighting */
TEST(register_language_disable_number_highlighting) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);
    ctx.L = loki_lua_bootstrap(&ctx, NULL);

    const char *code =
        "return loki.register_language({\n"
        "  name = 'TestLang',\n"
        "  extensions = {'.test'},\n"
        "  highlight_numbers = false\n"
        "})";

    int result = luaL_dostring(ctx.L, code);
    ASSERT_EQ(result, 0);
    ASSERT_TRUE(lua_isboolean(ctx.L, -1));
    ASSERT_TRUE(lua_toboolean(ctx.L, -1));

    /* Cleanup */
    if (ctx.L) lua_close(ctx.L);
    editor_ctx_free(&ctx);
}

/* Test: invalid argument type (not a table) */
TEST(register_language_invalid_argument) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);
    ctx.L = loki_lua_bootstrap(&ctx, NULL);

    const char *code = "return loki.register_language('not a table')";

    int result = luaL_dostring(ctx.L, code);
    ASSERT_EQ(result, 0);

    /* Should return nil and error message */
    ASSERT_TRUE(lua_isnil(ctx.L, -2));
    ASSERT_TRUE(lua_isstring(ctx.L, -1));

    /* Cleanup */
    if (ctx.L) lua_close(ctx.L);
    editor_ctx_free(&ctx);
}

BEGIN_TEST_SUITE("Language Registration")
    RUN_TEST(register_language_minimal_config);
    RUN_TEST(register_language_full_config);
    RUN_TEST(register_language_missing_name);
    RUN_TEST(register_language_missing_extensions);
    RUN_TEST(register_language_empty_extensions);
    RUN_TEST(register_language_extension_without_dot);
    RUN_TEST(register_language_multiple_extensions);
    RUN_TEST(register_language_keywords_only);
    RUN_TEST(register_language_types_only);
    RUN_TEST(register_language_line_comment_only);
    RUN_TEST(register_language_line_comment_too_long);
    RUN_TEST(register_language_block_comments);
    RUN_TEST(register_language_block_comment_too_long);
    RUN_TEST(register_language_custom_separators);
    RUN_TEST(register_language_disable_string_highlighting);
    RUN_TEST(register_language_disable_number_highlighting);
    RUN_TEST(register_language_invalid_argument);
END_TEST_SUITE()
