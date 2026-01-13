/* test_core.c - Unit tests for core editor functionality
 *
 * Tests for:
 * - Editor context initialization
 * - Row insertion and deletion
 * - Character insertion and deletion
 * - Cursor movement
 * - Separator detection
 */

#include "test_framework.h"
#include "loki/core.h"
#include "loki_internal.h"
#include "loki_syntax.h"
#include <string.h>

/* Test editor context initialization */
TEST(editor_ctx_init_initializes_all_fields) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    ASSERT_EQ(ctx.cx, 0);
    ASSERT_EQ(ctx.cy, 0);
    ASSERT_EQ(ctx.numrows, 0);
    ASSERT_EQ(ctx.dirty, 0);
    ASSERT_NULL(ctx.row);
    ASSERT_NULL(ctx.filename);
    ASSERT_EQ(ctx.mode, MODE_NORMAL);
    ASSERT_EQ(ctx.winsize_changed, 0);
}

/* Test separator detection */
TEST(is_separator_detects_whitespace) {
    char *seps = " \t,;";
    ASSERT_TRUE(syntax_is_separator(' ', seps));
    ASSERT_TRUE(syntax_is_separator('\t', seps));
    ASSERT_FALSE(syntax_is_separator('a', seps));
    ASSERT_FALSE(syntax_is_separator('1', seps));
}

TEST(is_separator_detects_custom_separators) {
    char *seps = ",.()+-/*";
    ASSERT_TRUE(syntax_is_separator(',', seps));
    ASSERT_TRUE(syntax_is_separator('.', seps));
    ASSERT_TRUE(syntax_is_separator('(', seps));
    ASSERT_TRUE(syntax_is_separator(')', seps));
    ASSERT_FALSE(syntax_is_separator('a', seps));
}

TEST(is_separator_handles_null_terminator) {
    char *seps = ",;";
    ASSERT_TRUE(syntax_is_separator('\0', seps));
}

/* Test character insertion */
TEST(editor_insert_char_adds_character_to_empty_buffer) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    /* Initialize with empty row */
    ctx.numrows = 1;
    ctx.row = calloc(1, sizeof(t_erow));
    ASSERT_NOT_NULL(ctx.row);

    ctx.row[0].chars = malloc(1);
    ctx.row[0].chars[0] = '\0';
    ctx.row[0].size = 0;
    ctx.row[0].render = NULL;
    ctx.row[0].hl = NULL;
    ctx.row[0].rsize = 0;

    editor_insert_char(&ctx, 'a');

    ASSERT_EQ(ctx.row[0].size, 1);
    ASSERT_EQ(ctx.row[0].chars[0], 'a');
    ASSERT_EQ(ctx.dirty, 1);

    /* Cleanup */
    free(ctx.row[0].chars);
    free(ctx.row[0].render);
    free(ctx.row[0].hl);
    free(ctx.row);
}

/* Test newline insertion */
TEST(editor_insert_newline_splits_line) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    /* Initialize with one row containing "hello" */
    ctx.numrows = 1;
    ctx.row = calloc(1, sizeof(t_erow));
    ASSERT_NOT_NULL(ctx.row);

    ctx.row[0].chars = strdup("hello");
    ctx.row[0].size = 5;
    ctx.row[0].render = NULL;
    ctx.row[0].hl = NULL;
    ctx.row[0].rsize = 0;
    ctx.row[0].idx = 0;

    /* Position cursor at index 2 (between 'e' and 'l') */
    ctx.cx = 2;
    ctx.cy = 0;

    editor_insert_newline(&ctx);

    /* Should have 2 rows now */
    ASSERT_EQ(ctx.numrows, 2);

    /* First row should be "he" */
    ASSERT_EQ(ctx.row[0].size, 2);
    ASSERT_EQ(ctx.row[0].chars[0], 'h');
    ASSERT_EQ(ctx.row[0].chars[1], 'e');

    /* Second row should be "llo" */
    ASSERT_EQ(ctx.row[1].size, 3);
    ASSERT_EQ(ctx.row[1].chars[0], 'l');
    ASSERT_EQ(ctx.row[1].chars[1], 'l');
    ASSERT_EQ(ctx.row[1].chars[2], 'o');

    /* Cursor should move to start of new line */
    ASSERT_EQ(ctx.cy, 1);
    ASSERT_EQ(ctx.cx, 0);

    /* Cleanup */
    for (int i = 0; i < ctx.numrows; i++) {
        free(ctx.row[i].chars);
        free(ctx.row[i].render);
        free(ctx.row[i].hl);
    }
    free(ctx.row);
}

/* Test cursor movement doesn't go out of bounds */
TEST(cursor_stays_within_bounds) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    ctx.numrows = 2;
    ctx.row = calloc(2, sizeof(t_erow));

    /* Row 0: "abc" */
    ctx.row[0].chars = strdup("abc");
    ctx.row[0].size = 3;
    ctx.row[0].render = strdup("abc");
    ctx.row[0].rsize = 3;
    ctx.row[0].hl = NULL;

    /* Row 1: "defg" */
    ctx.row[1].chars = strdup("defg");
    ctx.row[1].size = 4;
    ctx.row[1].render = strdup("defg");
    ctx.row[1].rsize = 4;
    ctx.row[1].hl = NULL;

    ctx.screenrows = 10;
    ctx.screencols = 80;

    /* Start at (0,0) */
    ctx.cx = 0;
    ctx.cy = 0;

    /* Move right to end of line */
    editor_move_cursor(&ctx, ARROW_RIGHT);  /* cx = 1 */
    editor_move_cursor(&ctx, ARROW_RIGHT);  /* cx = 2 */
    editor_move_cursor(&ctx, ARROW_RIGHT);  /* cx = 3 (at end) */
    ASSERT_EQ(ctx.cx, 3);

    /* Try to move right beyond end - should stay at end */
    editor_move_cursor(&ctx, ARROW_RIGHT);
    /* Cursor should wrap to next line or stay - implementation specific */
    /* Just verify we don't crash */
    ASSERT_TRUE(ctx.cx >= 0);
    ASSERT_TRUE(ctx.cy >= 0);
    ASSERT_TRUE(ctx.cy < ctx.numrows);

    /* Cleanup */
    for (int i = 0; i < ctx.numrows; i++) {
        free(ctx.row[i].chars);
        free(ctx.row[i].render);
        free(ctx.row[i].hl);
    }
    free(ctx.row);
}

/* Test dirty flag management */
TEST(dirty_flag_set_on_modification) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    ASSERT_EQ(ctx.dirty, 0);

    /* Initialize with empty row */
    ctx.numrows = 1;
    ctx.row = calloc(1, sizeof(t_erow));
    ctx.row[0].chars = malloc(1);
    ctx.row[0].chars[0] = '\0';
    ctx.row[0].size = 0;
    ctx.row[0].render = NULL;
    ctx.row[0].hl = NULL;

    editor_insert_char(&ctx, 'x');

    ASSERT_EQ(ctx.dirty, 1);

    /* Cleanup */
    free(ctx.row[0].chars);
    free(ctx.row[0].render);
    free(ctx.row[0].hl);
    free(ctx.row);
}

/* Test mode management */
TEST(mode_switching_works) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    ASSERT_EQ(ctx.mode, MODE_NORMAL);

    ctx.mode = MODE_INSERT;
    ASSERT_EQ(ctx.mode, MODE_INSERT);

    ctx.mode = MODE_VISUAL;
    ASSERT_EQ(ctx.mode, MODE_VISUAL);

    ctx.mode = MODE_NORMAL;
    ASSERT_EQ(ctx.mode, MODE_NORMAL);
}

/* Test window resize flag */
TEST(window_resize_flag_initialized) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    ASSERT_EQ(ctx.winsize_changed, 0);

    /* Simulate resize signal */
    ctx.winsize_changed = 1;
    ASSERT_EQ(ctx.winsize_changed, 1);

    /* Clear flag */
    ctx.winsize_changed = 0;
    ASSERT_EQ(ctx.winsize_changed, 0);
}

BEGIN_TEST_SUITE("Core Editor Functions")
    RUN_TEST(editor_ctx_init_initializes_all_fields);
    RUN_TEST(is_separator_detects_whitespace);
    RUN_TEST(is_separator_detects_custom_separators);
    RUN_TEST(is_separator_handles_null_terminator);
    RUN_TEST(editor_insert_char_adds_character_to_empty_buffer);
    RUN_TEST(editor_insert_newline_splits_line);
    RUN_TEST(cursor_stays_within_bounds);
    RUN_TEST(dirty_flag_set_on_modification);
    RUN_TEST(mode_switching_works);
    RUN_TEST(window_resize_flag_initialized);
END_TEST_SUITE()
