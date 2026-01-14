/* test_syntax.c - Unit tests for syntax highlighting
 *
 * Tests for syntax highlighting engine:
 * - Keyword detection (primary and type keywords)
 * - String highlighting (quotes, escapes)
 * - Comment highlighting (single-line and multi-line)
 * - Number literal detection
 * - Multi-line comment state tracking
 * - Language-specific highlighting
 */

#include "test_framework.h"
#include "loki/core.h"
#include "loki/internal.h"
#include "loki/syntax.h"
#include <string.h>
#include <stdlib.h>

/* Helper: Create row with text and apply C syntax highlighting */
static void init_c_syntax_row(editor_ctx_t *ctx, t_erow *row, const char *text) {
    memset(row, 0, sizeof(t_erow));
    row->chars = strdup(text);
    row->size = strlen(text);
    row->render = strdup(text);
    row->rsize = strlen(text);
    row->hl = calloc(row->rsize, 1);
    row->idx = 0;

    /* Set C syntax for context */
    extern struct t_editor_syntax HLDB[];
    ctx->syntax = &HLDB[0];  /* C is first in HLDB */

    syntax_update_row(ctx, row);
}

/* Helper: Free row resources */
static void free_row(t_erow *row) {
    free(row->chars);
    free(row->render);
    free(row->hl);
}

/* ============================================================================
 * Keyword Highlighting Tests
 * ============================================================================ */

TEST(syntax_c_keyword1_if) {
    editor_ctx_t ctx;
    t_erow row;
    editor_ctx_init(&ctx);

    init_c_syntax_row(&ctx, &row, "if (x)");

    /* "if" should be highlighted as KEYWORD1 */
    ASSERT_EQ(row.hl[0], HL_KEYWORD1);
    ASSERT_EQ(row.hl[1], HL_KEYWORD1);
    /* Space after keyword should be NORMAL */
    ASSERT_EQ(row.hl[2], HL_NORMAL);

    free_row(&row);
    editor_ctx_free(&ctx);
}

TEST(syntax_c_keyword1_return) {
    editor_ctx_t ctx;
    t_erow row;
    editor_ctx_init(&ctx);

    init_c_syntax_row(&ctx, &row, "return 0;");

    /* "return" should be highlighted as KEYWORD1 */
    ASSERT_EQ(row.hl[0], HL_KEYWORD1);
    ASSERT_EQ(row.hl[1], HL_KEYWORD1);
    ASSERT_EQ(row.hl[2], HL_KEYWORD1);
    ASSERT_EQ(row.hl[3], HL_KEYWORD1);
    ASSERT_EQ(row.hl[4], HL_KEYWORD1);
    ASSERT_EQ(row.hl[5], HL_KEYWORD1);

    free_row(&row);
    editor_ctx_free(&ctx);
}

TEST(syntax_c_keyword2_int) {
    editor_ctx_t ctx;
    t_erow row;
    editor_ctx_init(&ctx);

    init_c_syntax_row(&ctx, &row, "int x;");

    /* "int" should be highlighted as KEYWORD2 (type) */
    ASSERT_EQ(row.hl[0], HL_KEYWORD2);
    ASSERT_EQ(row.hl[1], HL_KEYWORD2);
    ASSERT_EQ(row.hl[2], HL_KEYWORD2);

    free_row(&row);
    editor_ctx_free(&ctx);
}

TEST(syntax_c_keyword_requires_separator) {
    editor_ctx_t ctx;
    t_erow row;
    editor_ctx_init(&ctx);

    /* "ifx" should NOT be highlighted (no separator after "if") */
    init_c_syntax_row(&ctx, &row, "ifx");

    ASSERT_EQ(row.hl[0], HL_NORMAL);
    ASSERT_EQ(row.hl[1], HL_NORMAL);
    ASSERT_EQ(row.hl[2], HL_NORMAL);

    free_row(&row);
    editor_ctx_free(&ctx);
}

/* ============================================================================
 * String Highlighting Tests
 * ============================================================================ */

TEST(syntax_string_double_quotes) {
    editor_ctx_t ctx;
    t_erow row;
    editor_ctx_init(&ctx);

    init_c_syntax_row(&ctx, &row, "\"hello\"");

    /* Entire string including quotes should be HL_STRING */
    for (int i = 0; i < row.rsize; i++) {
        ASSERT_EQ(row.hl[i], HL_STRING);
    }

    free_row(&row);
    editor_ctx_free(&ctx);
}

TEST(syntax_string_single_quotes) {
    editor_ctx_t ctx;
    t_erow row;
    editor_ctx_init(&ctx);

    init_c_syntax_row(&ctx, &row, "'a'");

    /* Entire string including quotes should be HL_STRING */
    ASSERT_EQ(row.hl[0], HL_STRING);
    ASSERT_EQ(row.hl[1], HL_STRING);
    ASSERT_EQ(row.hl[2], HL_STRING);

    free_row(&row);
    editor_ctx_free(&ctx);
}

TEST(syntax_string_escape_sequence) {
    editor_ctx_t ctx;
    t_erow row;
    editor_ctx_init(&ctx);

    init_c_syntax_row(&ctx, &row, "\"a\\nb\"");

    /* Escape sequence should also be HL_STRING */
    for (int i = 0; i < row.rsize; i++) {
        ASSERT_EQ(row.hl[i], HL_STRING);
    }

    free_row(&row);
    editor_ctx_free(&ctx);
}

TEST(syntax_string_unterminated) {
    editor_ctx_t ctx;
    t_erow row;
    editor_ctx_init(&ctx);

    init_c_syntax_row(&ctx, &row, "\"unterminated");

    /* Unterminated string should still be highlighted */
    for (int i = 0; i < row.rsize; i++) {
        ASSERT_EQ(row.hl[i], HL_STRING);
    }

    free_row(&row);
    editor_ctx_free(&ctx);
}

/* ============================================================================
 * Comment Highlighting Tests
 * ============================================================================ */

TEST(syntax_c_single_line_comment) {
    editor_ctx_t ctx;
    t_erow row;
    editor_ctx_init(&ctx);

    init_c_syntax_row(&ctx, &row, "// comment");

    /* Entire line should be HL_COMMENT */
    for (int i = 0; i < row.rsize; i++) {
        ASSERT_EQ(row.hl[i], HL_COMMENT);
    }

    free_row(&row);
    editor_ctx_free(&ctx);
}

TEST(syntax_c_single_line_comment_after_code) {
    editor_ctx_t ctx;
    t_erow row;
    editor_ctx_init(&ctx);

    init_c_syntax_row(&ctx, &row, "int x; // comment");

    /* "int" should be keyword */
    ASSERT_EQ(row.hl[0], HL_KEYWORD2);
    ASSERT_EQ(row.hl[1], HL_KEYWORD2);
    ASSERT_EQ(row.hl[2], HL_KEYWORD2);

    /* Comment part should be HL_COMMENT */
    ASSERT_EQ(row.hl[7], HL_COMMENT);  /* First / */
    ASSERT_EQ(row.hl[8], HL_COMMENT);  /* Second / */

    free_row(&row);
    editor_ctx_free(&ctx);
}

TEST(syntax_c_multiline_comment_start) {
    editor_ctx_t ctx;
    t_erow row;
    editor_ctx_init(&ctx);

    init_c_syntax_row(&ctx, &row, "/* comment");

    /* Multi-line comment should be HL_MLCOMMENT */
    for (int i = 0; i < row.rsize; i++) {
        ASSERT_EQ(row.hl[i], HL_MLCOMMENT);
    }

    /* Row should have open comment flag */
    ASSERT_TRUE(row.hl_oc);

    free_row(&row);
    editor_ctx_free(&ctx);
}

TEST(syntax_c_multiline_comment_complete) {
    editor_ctx_t ctx;
    t_erow row;
    editor_ctx_init(&ctx);

    init_c_syntax_row(&ctx, &row, "/* comment */");

    /* Complete multi-line comment should be HL_MLCOMMENT */
    for (int i = 0; i < row.rsize; i++) {
        ASSERT_EQ(row.hl[i], HL_MLCOMMENT);
    }

    /* Row should NOT have open comment flag (it's closed) */
    ASSERT_FALSE(row.hl_oc);

    free_row(&row);
    editor_ctx_free(&ctx);
}

TEST(syntax_c_multiline_comment_continuation) {
    editor_ctx_t ctx;
    t_erow row1, row2;
    editor_ctx_init(&ctx);

    /* Setup multi-line buffer */
    ctx.numrows = 2;
    ctx.row = calloc(2, sizeof(t_erow));

    /* First line: start of multiline comment */
    ctx.row[0].chars = strdup("/* comment");
    ctx.row[0].size = strlen(ctx.row[0].chars);
    ctx.row[0].render = strdup(ctx.row[0].chars);
    ctx.row[0].rsize = ctx.row[0].size;
    ctx.row[0].hl = calloc(ctx.row[0].rsize, 1);
    ctx.row[0].idx = 0;

    /* Second line: continuation */
    ctx.row[1].chars = strdup("still comment */");
    ctx.row[1].size = strlen(ctx.row[1].chars);
    ctx.row[1].render = strdup(ctx.row[1].chars);
    ctx.row[1].rsize = ctx.row[1].size;
    ctx.row[1].hl = calloc(ctx.row[1].rsize, 1);
    ctx.row[1].idx = 1;

    extern struct t_editor_syntax HLDB[];
    ctx.syntax = &HLDB[0];  /* C syntax */

    /* Update syntax for both rows */
    syntax_update_row(&ctx, &ctx.row[0]);
    syntax_update_row(&ctx, &ctx.row[1]);

    /* First row should have open comment */
    ASSERT_TRUE(ctx.row[0].hl_oc);

    /* Second row should continue comment */
    for (int i = 0; i < ctx.row[1].rsize; i++) {
        ASSERT_EQ(ctx.row[1].hl[i], HL_MLCOMMENT);
    }

    /* Second row should NOT have open comment (it's closed) */
    ASSERT_FALSE(ctx.row[1].hl_oc);

    /* Free manually created buffer */
    for (int i = 0; i < ctx.numrows; i++) {
        free(ctx.row[i].chars);
        free(ctx.row[i].render);
        free(ctx.row[i].hl);
    }
    free(ctx.row);
    ctx.row = NULL;
    ctx.numrows = 0;

    editor_ctx_free(&ctx);
}

/* ============================================================================
 * Number Highlighting Tests
 * ============================================================================ */

TEST(syntax_number_integer) {
    editor_ctx_t ctx;
    t_erow row;
    editor_ctx_init(&ctx);

    init_c_syntax_row(&ctx, &row, "123");

    /* All digits should be HL_NUMBER */
    ASSERT_EQ(row.hl[0], HL_NUMBER);
    ASSERT_EQ(row.hl[1], HL_NUMBER);
    ASSERT_EQ(row.hl[2], HL_NUMBER);

    free_row(&row);
    editor_ctx_free(&ctx);
}

TEST(syntax_number_decimal) {
    editor_ctx_t ctx;
    t_erow row;
    editor_ctx_init(&ctx);

    init_c_syntax_row(&ctx, &row, "123.456");

    /* All digits and decimal point should be HL_NUMBER */
    for (int i = 0; i < row.rsize; i++) {
        ASSERT_EQ(row.hl[i], HL_NUMBER);
    }

    free_row(&row);
    editor_ctx_free(&ctx);
}

TEST(syntax_number_after_separator) {
    editor_ctx_t ctx;
    t_erow row;
    editor_ctx_init(&ctx);

    init_c_syntax_row(&ctx, &row, "x=42");

    /* 42 should be highlighted */
    ASSERT_EQ(row.hl[2], HL_NUMBER);
    ASSERT_EQ(row.hl[3], HL_NUMBER);

    free_row(&row);
    editor_ctx_free(&ctx);
}

TEST(syntax_number_not_after_letter) {
    editor_ctx_t ctx;
    t_erow row;
    editor_ctx_init(&ctx);

    init_c_syntax_row(&ctx, &row, "abc123");

    /* "abc123" should not be highlighted as number (no separator before digit) */
    /* It will be normal text */
    ASSERT_EQ(row.hl[3], HL_NORMAL);
    ASSERT_EQ(row.hl[4], HL_NORMAL);
    ASSERT_EQ(row.hl[5], HL_NORMAL);

    free_row(&row);
    editor_ctx_free(&ctx);
}

/* ============================================================================
 * Separator Detection Tests
 * ============================================================================ */

TEST(syntax_separator_space) {
    editor_ctx_t ctx;
    t_erow row;
    editor_ctx_init(&ctx);

    init_c_syntax_row(&ctx, &row, "if return");

    /* Both keywords should be detected (space is separator) */
    ASSERT_EQ(row.hl[0], HL_KEYWORD1);  /* if */
    ASSERT_EQ(row.hl[1], HL_KEYWORD1);
    ASSERT_EQ(row.hl[3], HL_KEYWORD1);  /* return */

    free_row(&row);
    editor_ctx_free(&ctx);
}

TEST(syntax_separator_paren) {
    editor_ctx_t ctx;
    t_erow row;
    editor_ctx_init(&ctx);

    init_c_syntax_row(&ctx, &row, "if(");

    /* "if" should be detected (paren is separator) */
    ASSERT_EQ(row.hl[0], HL_KEYWORD1);
    ASSERT_EQ(row.hl[1], HL_KEYWORD1);

    free_row(&row);
    editor_ctx_free(&ctx);
}

/* ============================================================================
 * Language-Specific Tests
 * ============================================================================ */

TEST(syntax_python_comment) {
    editor_ctx_t ctx;
    t_erow row;
    editor_ctx_init(&ctx);

    memset(&row, 0, sizeof(row));
    row.chars = strdup("# comment");
    row.size = strlen(row.chars);
    row.render = strdup(row.chars);
    row.rsize = strlen(row.render);
    row.hl = calloc(row.rsize, 1);
    row.idx = 0;

    /* Set Python syntax */
    extern struct t_editor_syntax HLDB[];
    ctx.syntax = &HLDB[1];  /* Python is second in HLDB */

    syntax_update_row(&ctx, &row);

    /* Single-character comment delimiters like "#" should work correctly */
    for (int i = 0; i < row.rsize; i++) {
        ASSERT_EQ(row.hl[i], HL_COMMENT);  /* Entire line is a comment */
    }

    free_row(&row);
    editor_ctx_free(&ctx);
}

TEST(syntax_lua_comment) {
    editor_ctx_t ctx;
    t_erow row;
    editor_ctx_init(&ctx);

    memset(&row, 0, sizeof(row));
    row.chars = strdup("-- comment");
    row.size = strlen(row.chars);
    row.render = strdup(row.chars);
    row.rsize = strlen(row.render);
    row.hl = calloc(row.rsize, 1);
    row.idx = 0;

    /* Set Lua syntax */
    extern struct t_editor_syntax HLDB[];
    ctx.syntax = &HLDB[2];  /* Lua is third in HLDB */

    syntax_update_row(&ctx, &row);

    /* Lua comment should be HL_COMMENT */
    for (int i = 0; i < row.rsize; i++) {
        ASSERT_EQ(row.hl[i], HL_COMMENT);
    }

    free_row(&row);
    editor_ctx_free(&ctx);
}

TEST(syntax_python_keyword) {
    editor_ctx_t ctx;
    t_erow row;
    editor_ctx_init(&ctx);

    memset(&row, 0, sizeof(row));
    row.chars = strdup("def func:");
    row.size = strlen(row.chars);
    row.render = strdup(row.chars);
    row.rsize = strlen(row.render);
    row.hl = calloc(row.rsize, 1);
    row.idx = 0;

    /* Set Python syntax */
    extern struct t_editor_syntax HLDB[];
    ctx.syntax = &HLDB[1];  /* Python */

    syntax_update_row(&ctx, &row);

    /* "def" should be KEYWORD1 */
    ASSERT_EQ(row.hl[0], HL_KEYWORD1);
    ASSERT_EQ(row.hl[1], HL_KEYWORD1);
    ASSERT_EQ(row.hl[2], HL_KEYWORD1);

    free_row(&row);
    editor_ctx_free(&ctx);
}

TEST(syntax_lua_keyword) {
    editor_ctx_t ctx;
    t_erow row;
    editor_ctx_init(&ctx);

    memset(&row, 0, sizeof(row));
    row.chars = strdup("function test()");
    row.size = strlen(row.chars);
    row.render = strdup(row.chars);
    row.rsize = strlen(row.render);
    row.hl = calloc(row.rsize, 1);
    row.idx = 0;

    /* Set Lua syntax */
    extern struct t_editor_syntax HLDB[];
    ctx.syntax = &HLDB[2];  /* Lua */

    syntax_update_row(&ctx, &row);

    /* "function" should be KEYWORD1 */
    ASSERT_EQ(row.hl[0], HL_KEYWORD1);
    ASSERT_EQ(row.hl[1], HL_KEYWORD1);

    free_row(&row);
    editor_ctx_free(&ctx);
}

/* ============================================================================
 * Mixed Content Tests
 * ============================================================================ */

TEST(syntax_mixed_keyword_and_string) {
    editor_ctx_t ctx;
    t_erow row;
    editor_ctx_init(&ctx);

    init_c_syntax_row(&ctx, &row, "return \"text\";");

    /* "return" should be KEYWORD1 */
    ASSERT_EQ(row.hl[0], HL_KEYWORD1);

    /* String should be HL_STRING */
    ASSERT_EQ(row.hl[7], HL_STRING);  /* Opening quote */
    ASSERT_EQ(row.hl[8], HL_STRING);  /* 't' */
    ASSERT_EQ(row.hl[12], HL_STRING); /* Closing quote */

    free_row(&row);
    editor_ctx_free(&ctx);
}

TEST(syntax_mixed_keyword_and_number) {
    editor_ctx_t ctx;
    t_erow row;
    editor_ctx_init(&ctx);

    init_c_syntax_row(&ctx, &row, "return 42;");

    /* "return" should be KEYWORD1 */
    ASSERT_EQ(row.hl[0], HL_KEYWORD1);

    /* Number should be HL_NUMBER */
    ASSERT_EQ(row.hl[7], HL_NUMBER);
    ASSERT_EQ(row.hl[8], HL_NUMBER);

    free_row(&row);
    editor_ctx_free(&ctx);
}

BEGIN_TEST_SUITE("Syntax Highlighting")
    /* Keyword tests */
    RUN_TEST(syntax_c_keyword1_if);
    RUN_TEST(syntax_c_keyword1_return);
    RUN_TEST(syntax_c_keyword2_int);
    RUN_TEST(syntax_c_keyword_requires_separator);

    /* String tests */
    RUN_TEST(syntax_string_double_quotes);
    RUN_TEST(syntax_string_single_quotes);
    RUN_TEST(syntax_string_escape_sequence);
    RUN_TEST(syntax_string_unterminated);

    /* Comment tests */
    RUN_TEST(syntax_c_single_line_comment);
    RUN_TEST(syntax_c_single_line_comment_after_code);
    RUN_TEST(syntax_c_multiline_comment_start);
    RUN_TEST(syntax_c_multiline_comment_complete);
    RUN_TEST(syntax_c_multiline_comment_continuation);

    /* Number tests */
    RUN_TEST(syntax_number_integer);
    RUN_TEST(syntax_number_decimal);
    RUN_TEST(syntax_number_after_separator);
    RUN_TEST(syntax_number_not_after_letter);

    /* Separator tests */
    RUN_TEST(syntax_separator_space);
    RUN_TEST(syntax_separator_paren);

    /* Language-specific tests */
    RUN_TEST(syntax_python_comment);
    RUN_TEST(syntax_lua_comment);
    RUN_TEST(syntax_python_keyword);
    RUN_TEST(syntax_lua_keyword);

    /* Mixed content tests */
    RUN_TEST(syntax_mixed_keyword_and_string);
    RUN_TEST(syntax_mixed_keyword_and_number);
END_TEST_SUITE()
