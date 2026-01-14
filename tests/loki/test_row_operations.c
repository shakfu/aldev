/* test_row_operations.c - Unit tests for row manipulation operations
 *
 * Tests for:
 * - Row insertion at various positions
 * - Row deletion and cleanup
 * - Row content updates (render, syntax)
 * - Line merging (backspace at line start)
 * - Tab expansion
 * - Long line handling
 */

#include "test_framework.h"
#include "loki/core.h"
#include "loki/internal.h"
#include <string.h>
#include <stdlib.h>

/* Helper: Initialize empty editor context */
static void init_empty_ctx(editor_ctx_t *ctx) {
    editor_ctx_init(ctx);
    ctx->screenrows = 24;
    ctx->screencols = 80;
}

/* Helper: Create context with specific rows */
static void init_ctx_with_rows(editor_ctx_t *ctx, int num_rows, const char **content) {
    init_empty_ctx(ctx);

    ctx->numrows = num_rows;
    ctx->row = calloc(num_rows, sizeof(t_erow));

    for (int i = 0; i < num_rows; i++) {
        ctx->row[i].chars = strdup(content[i]);
        ctx->row[i].size = strlen(content[i]);
        ctx->row[i].render = strdup(content[i]);
        ctx->row[i].rsize = strlen(content[i]);
        ctx->row[i].hl = NULL;
        ctx->row[i].idx = i;
    }
}

/* ============================================================================
 * Row Insertion Tests
 * ============================================================================ */

TEST(row_insert_into_empty_buffer) {
    editor_ctx_t ctx;
    init_empty_ctx(&ctx);

    editor_insert_row(&ctx, 0, "first line", 10);

    ASSERT_EQ(ctx.numrows, 1);
    ASSERT_STR_EQ(ctx.row[0].chars, "first line");
    ASSERT_EQ(ctx.row[0].size, 10);
    ASSERT_EQ(ctx.row[0].idx, 0);

    editor_ctx_free(&ctx);
}

TEST(row_insert_at_beginning) {
    editor_ctx_t ctx;
    const char *rows[] = {"existing line"};
    init_ctx_with_rows(&ctx, 1, rows);

    editor_insert_row(&ctx, 0, "new first", 9);

    ASSERT_EQ(ctx.numrows, 2);
    ASSERT_STR_EQ(ctx.row[0].chars, "new first");
    ASSERT_STR_EQ(ctx.row[1].chars, "existing line");

    /* Verify indices updated */
    ASSERT_EQ(ctx.row[0].idx, 0);
    ASSERT_EQ(ctx.row[1].idx, 1);

    editor_ctx_free(&ctx);
}

TEST(row_insert_at_end) {
    editor_ctx_t ctx;
    const char *rows[] = {"first line"};
    init_ctx_with_rows(&ctx, 1, rows);

    editor_insert_row(&ctx, 1, "second line", 11);

    ASSERT_EQ(ctx.numrows, 2);
    ASSERT_STR_EQ(ctx.row[0].chars, "first line");
    ASSERT_STR_EQ(ctx.row[1].chars, "second line");
    ASSERT_EQ(ctx.row[1].idx, 1);

    editor_ctx_free(&ctx);
}

TEST(row_insert_in_middle) {
    editor_ctx_t ctx;
    const char *rows[] = {"line one", "line three"};
    init_ctx_with_rows(&ctx, 2, rows);

    editor_insert_row(&ctx, 1, "line two", 8);

    ASSERT_EQ(ctx.numrows, 3);
    ASSERT_STR_EQ(ctx.row[0].chars, "line one");
    ASSERT_STR_EQ(ctx.row[1].chars, "line two");
    ASSERT_STR_EQ(ctx.row[2].chars, "line three");

    /* Verify all indices */
    for (int i = 0; i < 3; i++) {
        ASSERT_EQ(ctx.row[i].idx, i);
    }

    editor_ctx_free(&ctx);
}

TEST(row_insert_empty_line) {
    editor_ctx_t ctx;
    const char *rows[] = {"line one", "line two"};
    init_ctx_with_rows(&ctx, 2, rows);

    editor_insert_row(&ctx, 1, "", 0);

    ASSERT_EQ(ctx.numrows, 3);
    ASSERT_STR_EQ(ctx.row[1].chars, "");
    ASSERT_EQ(ctx.row[1].size, 0);

    editor_ctx_free(&ctx);
}

TEST(row_insert_sets_dirty_flag) {
    editor_ctx_t ctx;
    init_empty_ctx(&ctx);

    ctx.dirty = 0;

    editor_insert_row(&ctx, 0, "new line", 8);

    ASSERT_TRUE(ctx.dirty > 0);

    editor_ctx_free(&ctx);
}

/* ============================================================================
 * Row Update Tests
 * ============================================================================ */

TEST(row_update_creates_render) {
    editor_ctx_t ctx;
    init_empty_ctx(&ctx);

    editor_insert_row(&ctx, 0, "simple text", 11);

    /* render should match chars for non-special content */
    ASSERT_NOT_NULL(ctx.row[0].render);
    ASSERT_STR_EQ(ctx.row[0].render, "simple text");
    ASSERT_EQ(ctx.row[0].rsize, 11);

    editor_ctx_free(&ctx);
}

TEST(row_update_expands_tabs) {
    editor_ctx_t ctx;
    init_empty_ctx(&ctx);

    editor_insert_row(&ctx, 0, "a\tb", 3);

    /* Tab should be expanded (default 8 or 4 spaces typically) */
    ASSERT_NOT_NULL(ctx.row[0].render);
    ASSERT_TRUE(ctx.row[0].rsize > 3);  /* Expanded size > raw size */

    editor_ctx_free(&ctx);
}

TEST(row_update_multiple_tabs) {
    editor_ctx_t ctx;
    init_empty_ctx(&ctx);

    editor_insert_row(&ctx, 0, "\t\t\t", 3);

    /* Three tabs should expand significantly */
    ASSERT_TRUE(ctx.row[0].rsize >= 3);

    editor_ctx_free(&ctx);
}

/* ============================================================================
 * Character Insertion Tests
 * ============================================================================ */

TEST(char_insert_at_beginning) {
    editor_ctx_t ctx;
    const char *rows[] = {"hello"};
    init_ctx_with_rows(&ctx, 1, rows);

    ctx.cx = 0;
    ctx.cy = 0;

    editor_insert_char(&ctx, 'X');

    ASSERT_STR_EQ(ctx.row[0].chars, "Xhello");
    ASSERT_EQ(ctx.row[0].size, 6);
    ASSERT_EQ(ctx.cx, 1);

    editor_ctx_free(&ctx);
}

TEST(char_insert_in_middle) {
    editor_ctx_t ctx;
    const char *rows[] = {"helo"};
    init_ctx_with_rows(&ctx, 1, rows);

    ctx.cx = 2;
    ctx.cy = 0;

    editor_insert_char(&ctx, 'l');

    ASSERT_STR_EQ(ctx.row[0].chars, "hello");
    ASSERT_EQ(ctx.cx, 3);

    editor_ctx_free(&ctx);
}

TEST(char_insert_at_end) {
    editor_ctx_t ctx;
    const char *rows[] = {"hello"};
    init_ctx_with_rows(&ctx, 1, rows);

    ctx.cx = 5;
    ctx.cy = 0;

    editor_insert_char(&ctx, '!');

    ASSERT_STR_EQ(ctx.row[0].chars, "hello!");
    ASSERT_EQ(ctx.cx, 6);

    editor_ctx_free(&ctx);
}

TEST(char_insert_into_empty_row) {
    editor_ctx_t ctx;
    const char *rows[] = {""};
    init_ctx_with_rows(&ctx, 1, rows);

    ctx.cx = 0;
    ctx.cy = 0;

    editor_insert_char(&ctx, 'a');

    ASSERT_STR_EQ(ctx.row[0].chars, "a");
    ASSERT_EQ(ctx.row[0].size, 1);

    editor_ctx_free(&ctx);
}

/* ============================================================================
 * Character Deletion Tests
 * ============================================================================ */

TEST(char_delete_at_end) {
    editor_ctx_t ctx;
    const char *rows[] = {"hello!"};
    init_ctx_with_rows(&ctx, 1, rows);

    ctx.cx = 6;
    ctx.cy = 0;

    editor_del_char(&ctx);

    ASSERT_STR_EQ(ctx.row[0].chars, "hello");
    ASSERT_EQ(ctx.cx, 5);

    editor_ctx_free(&ctx);
}

TEST(char_delete_in_middle) {
    editor_ctx_t ctx;
    const char *rows[] = {"helllo"};
    init_ctx_with_rows(&ctx, 1, rows);

    ctx.cx = 4;  /* After first 'l' */
    ctx.cy = 0;

    editor_del_char(&ctx);

    ASSERT_STR_EQ(ctx.row[0].chars, "hello");

    editor_ctx_free(&ctx);
}

TEST(char_delete_at_beginning_merges_lines) {
    editor_ctx_t ctx;
    const char *rows[] = {"first", "second"};
    init_ctx_with_rows(&ctx, 2, rows);

    ctx.cx = 0;
    ctx.cy = 1;

    editor_del_char(&ctx);

    /* Should merge lines */
    ASSERT_EQ(ctx.numrows, 1);
    ASSERT_STR_EQ(ctx.row[0].chars, "firstsecond");

    editor_ctx_free(&ctx);
}

TEST(char_delete_at_very_beginning_does_nothing) {
    editor_ctx_t ctx;
    const char *rows[] = {"hello"};
    init_ctx_with_rows(&ctx, 1, rows);

    ctx.cx = 0;
    ctx.cy = 0;

    editor_del_char(&ctx);

    /* At file beginning - nothing to delete */
    ASSERT_EQ(ctx.numrows, 1);
    ASSERT_STR_EQ(ctx.row[0].chars, "hello");

    editor_ctx_free(&ctx);
}

/* ============================================================================
 * Newline Insertion Tests
 * ============================================================================ */

TEST(newline_splits_line_at_cursor) {
    editor_ctx_t ctx;
    const char *rows[] = {"hello world"};
    init_ctx_with_rows(&ctx, 1, rows);

    ctx.cx = 5;  /* After "hello" */
    ctx.cy = 0;

    editor_insert_newline(&ctx);

    ASSERT_EQ(ctx.numrows, 2);
    ASSERT_STR_EQ(ctx.row[0].chars, "hello");
    ASSERT_STR_EQ(ctx.row[1].chars, " world");

    editor_ctx_free(&ctx);
}

TEST(newline_at_line_start_creates_empty_line_above) {
    editor_ctx_t ctx;
    const char *rows[] = {"content"};
    init_ctx_with_rows(&ctx, 1, rows);

    ctx.cx = 0;
    ctx.cy = 0;

    editor_insert_newline(&ctx);

    ASSERT_EQ(ctx.numrows, 2);
    ASSERT_STR_EQ(ctx.row[0].chars, "");
    ASSERT_STR_EQ(ctx.row[1].chars, "content");

    editor_ctx_free(&ctx);
}

TEST(newline_at_line_end_creates_empty_line_below) {
    editor_ctx_t ctx;
    const char *rows[] = {"content"};
    init_ctx_with_rows(&ctx, 1, rows);

    ctx.cx = 7;  /* End of "content" */
    ctx.cy = 0;

    editor_insert_newline(&ctx);

    ASSERT_EQ(ctx.numrows, 2);
    ASSERT_STR_EQ(ctx.row[0].chars, "content");
    ASSERT_STR_EQ(ctx.row[1].chars, "");

    editor_ctx_free(&ctx);
}

TEST(newline_moves_cursor_to_new_line) {
    editor_ctx_t ctx;
    const char *rows[] = {"hello world"};
    init_ctx_with_rows(&ctx, 1, rows);

    ctx.cx = 6;
    ctx.cy = 0;

    editor_insert_newline(&ctx);

    ASSERT_EQ(ctx.cy, 1);
    ASSERT_EQ(ctx.cx, 0);

    editor_ctx_free(&ctx);
}

/* ============================================================================
 * Long Line Tests
 * ============================================================================ */

TEST(row_handles_long_line) {
    editor_ctx_t ctx;
    init_empty_ctx(&ctx);

    /* Create a very long line */
    char long_line[2048];
    memset(long_line, 'x', sizeof(long_line) - 1);
    long_line[sizeof(long_line) - 1] = '\0';

    editor_insert_row(&ctx, 0, long_line, sizeof(long_line) - 1);

    ASSERT_EQ(ctx.row[0].size, 2047);
    ASSERT_NOT_NULL(ctx.row[0].chars);
    ASSERT_NOT_NULL(ctx.row[0].render);

    editor_ctx_free(&ctx);
}

TEST(char_insert_into_long_line) {
    editor_ctx_t ctx;
    init_empty_ctx(&ctx);

    /* Create long line */
    char long_line[1024];
    memset(long_line, 'a', sizeof(long_line) - 1);
    long_line[sizeof(long_line) - 1] = '\0';

    editor_insert_row(&ctx, 0, long_line, sizeof(long_line) - 1);

    ctx.cx = 500;  /* Middle of line */
    ctx.cy = 0;

    editor_insert_char(&ctx, 'X');

    ASSERT_EQ(ctx.row[0].size, 1024);
    ASSERT_EQ(ctx.row[0].chars[500], 'X');

    editor_ctx_free(&ctx);
}

/* ============================================================================
 * Multiple Row Operations
 * ============================================================================ */

TEST(multiple_insertions) {
    editor_ctx_t ctx;
    init_empty_ctx(&ctx);

    for (int i = 0; i < 10; i++) {
        char line[32];
        snprintf(line, sizeof(line), "line %d", i);
        editor_insert_row(&ctx, i, line, strlen(line));
    }

    ASSERT_EQ(ctx.numrows, 10);

    for (int i = 0; i < 10; i++) {
        char expected[32];
        snprintf(expected, sizeof(expected), "line %d", i);
        ASSERT_STR_EQ(ctx.row[i].chars, expected);
        ASSERT_EQ(ctx.row[i].idx, i);
    }

    editor_ctx_free(&ctx);
}

TEST(interleaved_insert_delete) {
    editor_ctx_t ctx;
    const char *rows[] = {"line one", "line two", "line three"};
    init_ctx_with_rows(&ctx, 3, rows);

    /* Delete middle line content by setting cursor there */
    ctx.cy = 1;
    ctx.cx = 8;

    /* Insert char */
    editor_insert_char(&ctx, '!');
    ASSERT_STR_EQ(ctx.row[1].chars, "line two!");

    /* Delete char */
    editor_del_char(&ctx);
    ASSERT_STR_EQ(ctx.row[1].chars, "line two");

    editor_ctx_free(&ctx);
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST(row_insert_null_content_safe) {
    editor_ctx_t ctx;
    init_empty_ctx(&ctx);

    /* Empty content should be safe */
    editor_insert_row(&ctx, 0, "", 0);

    ASSERT_EQ(ctx.numrows, 1);
    ASSERT_EQ(ctx.row[0].size, 0);
    ASSERT_NOT_NULL(ctx.row[0].chars);

    editor_ctx_free(&ctx);
}

TEST(empty_buffer_operations) {
    editor_ctx_t ctx;
    init_empty_ctx(&ctx);

    /* Delete on empty buffer should not crash */
    ctx.cx = 0;
    ctx.cy = 0;
    editor_del_char(&ctx);

    ASSERT_EQ(ctx.numrows, 0);

    editor_ctx_free(&ctx);
}

TEST(special_characters_in_row) {
    editor_ctx_t ctx;
    init_empty_ctx(&ctx);

    /* Row with special characters */
    editor_insert_row(&ctx, 0, "hello\tworld\t!", 13);

    ASSERT_EQ(ctx.row[0].size, 13);
    ASSERT_TRUE(ctx.row[0].rsize > 13);  /* Tabs expand */

    editor_ctx_free(&ctx);
}

TEST(unicode_aware_row) {
    editor_ctx_t ctx;
    init_empty_ctx(&ctx);

    /* UTF-8 content (stored as bytes) */
    editor_insert_row(&ctx, 0, "café", 5);  /* UTF-8: c, a, f, é (2 bytes), = 5 bytes */

    ASSERT_EQ(ctx.row[0].size, 5);

    editor_ctx_free(&ctx);
}

BEGIN_TEST_SUITE("Row Operations")
    /* Row insertion */
    RUN_TEST(row_insert_into_empty_buffer);
    RUN_TEST(row_insert_at_beginning);
    RUN_TEST(row_insert_at_end);
    RUN_TEST(row_insert_in_middle);
    RUN_TEST(row_insert_empty_line);
    RUN_TEST(row_insert_sets_dirty_flag);

    /* Row update */
    RUN_TEST(row_update_creates_render);
    RUN_TEST(row_update_expands_tabs);
    RUN_TEST(row_update_multiple_tabs);

    /* Character insertion */
    RUN_TEST(char_insert_at_beginning);
    RUN_TEST(char_insert_in_middle);
    RUN_TEST(char_insert_at_end);
    RUN_TEST(char_insert_into_empty_row);

    /* Character deletion */
    RUN_TEST(char_delete_at_end);
    RUN_TEST(char_delete_in_middle);
    RUN_TEST(char_delete_at_beginning_merges_lines);
    RUN_TEST(char_delete_at_very_beginning_does_nothing);

    /* Newline insertion */
    RUN_TEST(newline_splits_line_at_cursor);
    RUN_TEST(newline_at_line_start_creates_empty_line_above);
    RUN_TEST(newline_at_line_end_creates_empty_line_below);
    RUN_TEST(newline_moves_cursor_to_new_line);

    /* Long lines */
    RUN_TEST(row_handles_long_line);
    RUN_TEST(char_insert_into_long_line);

    /* Multiple operations */
    RUN_TEST(multiple_insertions);
    RUN_TEST(interleaved_insert_delete);

    /* Edge cases */
    RUN_TEST(row_insert_null_content_safe);
    RUN_TEST(empty_buffer_operations);
    RUN_TEST(special_characters_in_row);
    RUN_TEST(unicode_aware_row);
END_TEST_SUITE()
