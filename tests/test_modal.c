/* test_modal.c - Unit tests for modal editing
 *
 * Tests for vim-like modal editing modes:
 * - NORMAL mode navigation and commands
 * - INSERT mode text entry
 * - VISUAL mode selection
 * - Mode transitions
 */

#include "test_framework.h"
#include "loki/core.h"
#include "loki_internal.h"
#include "loki_selection.h"
#include <string.h>

/* Helper: Create single-line test context */
static void init_simple_ctx(editor_ctx_t *ctx, const char *text) {
    editor_ctx_init(ctx);

    ctx->numrows = 1;
    ctx->row = calloc(1, sizeof(t_erow));
    ctx->row[0].chars = strdup(text);
    ctx->row[0].size = strlen(text);
    ctx->row[0].render = strdup(text);
    ctx->row[0].rsize = strlen(text);
    ctx->row[0].hl = NULL;
    ctx->row[0].idx = 0;

    ctx->screenrows = 24;
    ctx->screencols = 80;
}

/* Helper: Create multi-line test context */
static void init_multiline_ctx(editor_ctx_t *ctx, int num_lines, const char **lines) {
    editor_ctx_init(ctx);

    ctx->numrows = num_lines;
    ctx->row = calloc(num_lines, sizeof(t_erow));

    for (int i = 0; i < num_lines; i++) {
        ctx->row[i].chars = strdup(lines[i]);
        ctx->row[i].size = strlen(lines[i]);
        ctx->row[i].render = strdup(lines[i]);
        ctx->row[i].rsize = strlen(lines[i]);
        ctx->row[i].hl = NULL;
        ctx->row[i].idx = i;
    }

    ctx->screenrows = 24;
    ctx->screencols = 80;
}

/* ============================================================================
 * NORMAL Mode Navigation Tests
 * ============================================================================ */

TEST(modal_normal_h_moves_left) {
    editor_ctx_t ctx;
    init_simple_ctx(&ctx, "hello");

    ctx.cx = 3;
    ctx.mode = MODE_NORMAL;

    modal_process_normal_mode_key(&ctx, 0, 'h');

    ASSERT_EQ(ctx.cx, 2);
    ASSERT_EQ(ctx.mode, MODE_NORMAL);

    editor_ctx_free(&ctx);
}

TEST(modal_normal_l_moves_right) {
    editor_ctx_t ctx;
    init_simple_ctx(&ctx, "hello");

    ctx.cx = 1;
    ctx.mode = MODE_NORMAL;

    modal_process_normal_mode_key(&ctx, 0, 'l');

    ASSERT_EQ(ctx.cx, 2);
    ASSERT_EQ(ctx.mode, MODE_NORMAL);

    editor_ctx_free(&ctx);
}

TEST(modal_normal_j_moves_down) {
    editor_ctx_t ctx;
    const char *lines[] = {"line1", "line2", "line3"};
    init_multiline_ctx(&ctx, 3, lines);

    ctx.cy = 0;
    ctx.mode = MODE_NORMAL;

    modal_process_normal_mode_key(&ctx, 0, 'j');

    ASSERT_EQ(ctx.cy, 1);
    ASSERT_EQ(ctx.mode, MODE_NORMAL);

    editor_ctx_free(&ctx);
}

TEST(modal_normal_k_moves_up) {
    editor_ctx_t ctx;
    const char *lines[] = {"line1", "line2", "line3"};
    init_multiline_ctx(&ctx, 3, lines);

    ctx.cy = 1;
    ctx.mode = MODE_NORMAL;

    modal_process_normal_mode_key(&ctx, 0, 'k');

    ASSERT_EQ(ctx.cy, 0);
    ASSERT_EQ(ctx.mode, MODE_NORMAL);

    editor_ctx_free(&ctx);
}

/* ============================================================================
 * NORMAL Mode Editing Tests
 * ============================================================================ */

TEST(modal_normal_x_deletes_char) {
    editor_ctx_t ctx;
    init_simple_ctx(&ctx, "hello");

    ctx.cx = 2;  /* Position at first 'l' */
    ctx.cy = 0;
    ctx.mode = MODE_NORMAL;

    modal_process_normal_mode_key(&ctx, 0, 'x');

    /* x deletes char before cursor (backspace-like) */
    ASSERT_STR_EQ(ctx.row[0].chars, "hllo");

    editor_ctx_free(&ctx);
}

TEST(modal_normal_i_enters_insert) {
    editor_ctx_t ctx;
    init_simple_ctx(&ctx, "hello");

    ctx.cx = 2;
    ctx.mode = MODE_NORMAL;

    modal_process_normal_mode_key(&ctx, 0, 'i');

    ASSERT_EQ(ctx.mode, MODE_INSERT);
    ASSERT_EQ(ctx.cx, 2);  /* Stays in place */

    editor_ctx_free(&ctx);
}

TEST(modal_normal_a_enters_insert_after) {
    editor_ctx_t ctx;
    init_simple_ctx(&ctx, "hello");

    ctx.cx = 2;
    ctx.mode = MODE_NORMAL;

    modal_process_normal_mode_key(&ctx, 0, 'a');

    ASSERT_EQ(ctx.mode, MODE_INSERT);
    ASSERT_EQ(ctx.cx, 3);  /* Moved right */

    editor_ctx_free(&ctx);
}

TEST(modal_normal_o_inserts_line_below) {
    editor_ctx_t ctx;
    const char *lines[] = {"line1", "line2"};
    init_multiline_ctx(&ctx, 2, lines);

    ctx.cy = 0;
    ctx.mode = MODE_NORMAL;

    modal_process_normal_mode_key(&ctx, 0, 'o');

    ASSERT_EQ(ctx.mode, MODE_INSERT);
    ASSERT_EQ(ctx.numrows, 3);
    ASSERT_EQ(ctx.cy, 1);  /* On new line */

    editor_ctx_free(&ctx);
}

TEST(modal_normal_O_inserts_line_above) {
    editor_ctx_t ctx;
    const char *lines[] = {"line1", "line2"};
    init_multiline_ctx(&ctx, 2, lines);

    ctx.cy = 1;
    ctx.mode = MODE_NORMAL;

    modal_process_normal_mode_key(&ctx, 0, 'O');

    ASSERT_EQ(ctx.mode, MODE_INSERT);
    ASSERT_EQ(ctx.numrows, 3);
    ASSERT_EQ(ctx.cy, 1);  /* Stays on inserted line */

    editor_ctx_free(&ctx);
}

/* ============================================================================
 * INSERT Mode Tests
 * ============================================================================ */

TEST(modal_insert_char_insertion) {
    editor_ctx_t ctx;
    init_simple_ctx(&ctx, "hllo");

    ctx.cx = 1;
    ctx.cy = 0;
    ctx.mode = MODE_INSERT;

    modal_process_insert_mode_key(&ctx, 0, 'e');

    ASSERT_STR_EQ(ctx.row[0].chars, "hello");
    ASSERT_EQ(ctx.cx, 2);

    editor_ctx_free(&ctx);
}

TEST(modal_insert_esc_returns_normal) {
    editor_ctx_t ctx;
    init_simple_ctx(&ctx, "hello");

    ctx.cx = 3;
    ctx.mode = MODE_INSERT;

    modal_process_insert_mode_key(&ctx, 0, ESC);

    ASSERT_EQ(ctx.mode, MODE_NORMAL);
    ASSERT_EQ(ctx.cx, 2);  /* Moved left */

    editor_ctx_free(&ctx);
}

TEST(modal_insert_esc_at_start) {
    editor_ctx_t ctx;
    init_simple_ctx(&ctx, "hello");

    ctx.cx = 0;
    ctx.coloff = 0;
    ctx.mode = MODE_INSERT;

    modal_process_insert_mode_key(&ctx, 0, ESC);

    ASSERT_EQ(ctx.mode, MODE_NORMAL);
    ASSERT_EQ(ctx.cx, 0);  /* Stays at start */

    editor_ctx_free(&ctx);
}

TEST(modal_insert_enter_creates_newline) {
    editor_ctx_t ctx;
    init_simple_ctx(&ctx, "hello");

    ctx.cx = 5;
    ctx.cy = 0;
    ctx.mode = MODE_INSERT;

    modal_process_insert_mode_key(&ctx, 0, ENTER);

    ASSERT_EQ(ctx.numrows, 2);
    ASSERT_EQ(ctx.cy, 1);
    ASSERT_EQ(ctx.cx, 0);

    editor_ctx_free(&ctx);
}

TEST(modal_insert_backspace_deletes) {
    editor_ctx_t ctx;
    init_simple_ctx(&ctx, "hello");

    ctx.cx = 5;
    ctx.cy = 0;
    ctx.mode = MODE_INSERT;

    modal_process_insert_mode_key(&ctx, 0, BACKSPACE);

    ASSERT_STR_EQ(ctx.row[0].chars, "hell");

    editor_ctx_free(&ctx);
}

/* ============================================================================
 * VISUAL Mode Tests
 * ============================================================================ */

TEST(modal_visual_v_enters_visual) {
    editor_ctx_t ctx;
    init_simple_ctx(&ctx, "hello");

    ctx.cx = 2;
    ctx.coloff = 0;
    ctx.rowoff = 0;
    ctx.mode = MODE_NORMAL;

    modal_process_normal_mode_key(&ctx, 0, 'v');

    ASSERT_EQ(ctx.mode, MODE_VISUAL);
    ASSERT_TRUE(ctx.sel_active);
    /* Selection should be in file coordinates (coloff + cx, rowoff + cy) */
    ASSERT_EQ(ctx.sel_start_x, 2);  /* 0 + 2 */
    ASSERT_EQ(ctx.sel_start_y, 0);  /* 0 + 0 */
    ASSERT_EQ(ctx.sel_end_x, 2);
    ASSERT_EQ(ctx.sel_end_y, 0);

    editor_ctx_free(&ctx);
}

TEST(modal_visual_h_extends_left) {
    editor_ctx_t ctx;
    init_simple_ctx(&ctx, "hello");

    ctx.cx = 3;
    ctx.coloff = 0;
    ctx.rowoff = 0;
    ctx.mode = MODE_VISUAL;
    ctx.sel_active = 1;
    ctx.sel_start_x = 3;
    ctx.sel_start_y = 0;
    ctx.sel_end_x = 3;
    ctx.sel_end_y = 0;

    modal_process_visual_mode_key(&ctx, 0, 'h');

    ASSERT_EQ(ctx.cx, 2);
    /* Selection end should be in file coordinates (coloff + cx) */
    ASSERT_EQ(ctx.sel_end_x, 2);  /* 0 + 2 */

    editor_ctx_free(&ctx);
}

TEST(modal_visual_l_extends_right) {
    editor_ctx_t ctx;
    init_simple_ctx(&ctx, "hello");

    ctx.cx = 2;
    ctx.coloff = 0;
    ctx.rowoff = 0;
    ctx.mode = MODE_VISUAL;
    ctx.sel_active = 1;
    ctx.sel_start_x = 2;
    ctx.sel_start_y = 0;
    ctx.sel_end_x = 2;
    ctx.sel_end_y = 0;

    modal_process_visual_mode_key(&ctx, 0, 'l');

    ASSERT_EQ(ctx.cx, 3);
    /* Selection end should be in file coordinates (coloff + cx) */
    ASSERT_EQ(ctx.sel_end_x, 3);  /* 0 + 3 */

    editor_ctx_free(&ctx);
}

TEST(modal_visual_esc_returns_normal) {
    editor_ctx_t ctx;
    init_simple_ctx(&ctx, "hello");

    ctx.mode = MODE_VISUAL;
    ctx.sel_active = 1;

    modal_process_visual_mode_key(&ctx, 0, ESC);

    ASSERT_EQ(ctx.mode, MODE_NORMAL);
    ASSERT_FALSE(ctx.sel_active);

    editor_ctx_free(&ctx);
}

TEST(modal_visual_y_yanks) {
    editor_ctx_t ctx;
    init_simple_ctx(&ctx, "hello");

    ctx.mode = MODE_VISUAL;
    ctx.sel_active = 1;
    ctx.sel_start_x = 0;
    ctx.sel_start_y = 0;
    ctx.sel_end_x = 4;
    ctx.sel_end_y = 0;

    modal_process_visual_mode_key(&ctx, 0, 'y');

    ASSERT_EQ(ctx.mode, MODE_NORMAL);
    ASSERT_FALSE(ctx.sel_active);

    editor_ctx_free(&ctx);
}

TEST(modal_visual_selection_highlighting) {
    editor_ctx_t ctx;
    init_simple_ctx(&ctx, "hello world");

    /* Set up visual mode selection in file coordinates */
    ctx.mode = MODE_VISUAL;
    ctx.sel_active = 1;
    ctx.sel_start_x = 2;  /* Start at 'l' in "hello" */
    ctx.sel_start_y = 0;
    ctx.sel_end_x = 7;    /* End at 'w' in "world" */
    ctx.sel_end_y = 0;

    /* Test that characters in range are selected */
    ASSERT_TRUE(is_selected(&ctx, 0, 2));   /* 'l' at position 2 */
    ASSERT_TRUE(is_selected(&ctx, 0, 3));   /* 'l' at position 3 */
    ASSERT_TRUE(is_selected(&ctx, 0, 4));   /* 'o' at position 4 */
    ASSERT_TRUE(is_selected(&ctx, 0, 5));   /* ' ' at position 5 */
    ASSERT_TRUE(is_selected(&ctx, 0, 6));   /* 'w' at position 6 */

    /* Test that characters outside range are not selected */
    ASSERT_FALSE(is_selected(&ctx, 0, 0));  /* 'h' at position 0 */
    ASSERT_FALSE(is_selected(&ctx, 0, 1));  /* 'e' at position 1 */
    ASSERT_FALSE(is_selected(&ctx, 0, 7));  /* 'o' at position 7 (end is exclusive) */
    ASSERT_FALSE(is_selected(&ctx, 0, 8));  /* 'r' at position 8 */

    editor_ctx_free(&ctx);
}

/* ============================================================================
 * Mode Transition Tests
 * ============================================================================ */

TEST(modal_default_is_normal) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    ASSERT_EQ(ctx.mode, MODE_NORMAL);

    editor_ctx_free(&ctx);
}

TEST(modal_normal_insert_normal_cycle) {
    editor_ctx_t ctx;
    init_simple_ctx(&ctx, "test");

    ctx.mode = MODE_NORMAL;

    /* NORMAL -> INSERT */
    modal_process_normal_mode_key(&ctx, 0, 'i');
    ASSERT_EQ(ctx.mode, MODE_INSERT);

    /* INSERT -> NORMAL */
    modal_process_insert_mode_key(&ctx, 0, ESC);
    ASSERT_EQ(ctx.mode, MODE_NORMAL);

    editor_ctx_free(&ctx);
}

TEST(modal_normal_visual_normal_cycle) {
    editor_ctx_t ctx;
    init_simple_ctx(&ctx, "test");

    ctx.mode = MODE_NORMAL;

    /* NORMAL -> VISUAL */
    modal_process_normal_mode_key(&ctx, 0, 'v');
    ASSERT_EQ(ctx.mode, MODE_VISUAL);
    ASSERT_TRUE(ctx.sel_active);

    /* VISUAL -> NORMAL */
    modal_process_visual_mode_key(&ctx, 0, ESC);
    ASSERT_EQ(ctx.mode, MODE_NORMAL);
    ASSERT_FALSE(ctx.sel_active);

    editor_ctx_free(&ctx);
}

BEGIN_TEST_SUITE("Modal Editing")
    /* NORMAL mode navigation */
    RUN_TEST(modal_normal_h_moves_left);
    RUN_TEST(modal_normal_l_moves_right);
    RUN_TEST(modal_normal_j_moves_down);
    RUN_TEST(modal_normal_k_moves_up);

    /* NORMAL mode editing */
    RUN_TEST(modal_normal_x_deletes_char);
    RUN_TEST(modal_normal_i_enters_insert);
    RUN_TEST(modal_normal_a_enters_insert_after);
    RUN_TEST(modal_normal_o_inserts_line_below);
    RUN_TEST(modal_normal_O_inserts_line_above);

    /* INSERT mode */
    RUN_TEST(modal_insert_char_insertion);
    RUN_TEST(modal_insert_esc_returns_normal);
    RUN_TEST(modal_insert_esc_at_start);
    RUN_TEST(modal_insert_enter_creates_newline);
    RUN_TEST(modal_insert_backspace_deletes);

    /* VISUAL mode */
    RUN_TEST(modal_visual_v_enters_visual);
    RUN_TEST(modal_visual_h_extends_left);
    RUN_TEST(modal_visual_l_extends_right);
    RUN_TEST(modal_visual_esc_returns_normal);
    RUN_TEST(modal_visual_y_yanks);
    RUN_TEST(modal_visual_selection_highlighting);

    /* Mode transitions */
    RUN_TEST(modal_default_is_normal);
    RUN_TEST(modal_normal_insert_normal_cycle);
    RUN_TEST(modal_normal_visual_normal_cycle);
END_TEST_SUITE()
