/* test_selection.c - Unit tests for text selection and clipboard operations
 *
 * Tests for:
 * - is_selected() position checking (single-line and multi-line)
 * - base64_encode() encoding correctness
 * - get_selection_text() text extraction
 * - delete_selection() text removal
 * - Selection boundary edge cases
 */

#include "test_framework.h"
#include "loki/core.h"
#include "loki/internal.h"
#include "loki/selection.h"
#include <string.h>
#include <stdlib.h>

/* Helper: Create single-line test context */
static void init_single_line_ctx(editor_ctx_t *ctx, const char *text) {
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
 * is_selected() Tests - Single Line Selection
 * ============================================================================ */

TEST(selection_single_line_in_range) {
    editor_ctx_t ctx;
    init_single_line_ctx(&ctx, "hello world");

    /* Selection from column 2 to 6: "llo w" */
    ctx.sel_active = 1;
    ctx.sel_start_x = 2;
    ctx.sel_start_y = 0;
    ctx.sel_end_x = 7;
    ctx.sel_end_y = 0;

    /* Positions within selection */
    ASSERT_TRUE(is_selected(&ctx, 0, 2));   /* Start */
    ASSERT_TRUE(is_selected(&ctx, 0, 4));   /* Middle */
    ASSERT_TRUE(is_selected(&ctx, 0, 6));   /* Before end */

    /* Positions outside selection */
    ASSERT_FALSE(is_selected(&ctx, 0, 0));  /* Before start */
    ASSERT_FALSE(is_selected(&ctx, 0, 1));  /* Before start */
    ASSERT_FALSE(is_selected(&ctx, 0, 7));  /* At end (exclusive) */
    ASSERT_FALSE(is_selected(&ctx, 0, 10)); /* After end */

    editor_ctx_free(&ctx);
}

TEST(selection_inactive_returns_false) {
    editor_ctx_t ctx;
    init_single_line_ctx(&ctx, "hello world");

    /* Selection inactive */
    ctx.sel_active = 0;
    ctx.sel_start_x = 2;
    ctx.sel_start_y = 0;
    ctx.sel_end_x = 7;
    ctx.sel_end_y = 0;

    /* Should return false for any position when inactive */
    ASSERT_FALSE(is_selected(&ctx, 0, 2));
    ASSERT_FALSE(is_selected(&ctx, 0, 4));
    ASSERT_FALSE(is_selected(&ctx, 0, 7));

    editor_ctx_free(&ctx);
}

TEST(selection_reversed_single_line) {
    editor_ctx_t ctx;
    init_single_line_ctx(&ctx, "hello world");

    /* Selection reversed (end before start) */
    ctx.sel_active = 1;
    ctx.sel_start_x = 7;  /* End actually first */
    ctx.sel_start_y = 0;
    ctx.sel_end_x = 2;    /* Start actually last */
    ctx.sel_end_y = 0;

    /* Should still detect correctly (normalized internally) */
    ASSERT_TRUE(is_selected(&ctx, 0, 2));
    ASSERT_TRUE(is_selected(&ctx, 0, 5));
    ASSERT_FALSE(is_selected(&ctx, 0, 7));

    editor_ctx_free(&ctx);
}

/* ============================================================================
 * is_selected() Tests - Multi-Line Selection
 * ============================================================================ */

TEST(selection_multiline_first_row) {
    editor_ctx_t ctx;
    const char *lines[] = {"first line", "second line", "third line"};
    init_multiline_ctx(&ctx, 3, lines);

    /* Selection from row 0 col 3 to row 2 col 5 */
    ctx.sel_active = 1;
    ctx.sel_start_x = 3;
    ctx.sel_start_y = 0;
    ctx.sel_end_x = 5;
    ctx.sel_end_y = 2;

    /* First row: positions from start_x to end of line */
    ASSERT_FALSE(is_selected(&ctx, 0, 0));
    ASSERT_FALSE(is_selected(&ctx, 0, 2));
    ASSERT_TRUE(is_selected(&ctx, 0, 3));  /* Start of selection */
    ASSERT_TRUE(is_selected(&ctx, 0, 6));  /* Rest of first line */
    ASSERT_TRUE(is_selected(&ctx, 0, 9));  /* End of first line */

    editor_ctx_free(&ctx);
}

TEST(selection_multiline_middle_row) {
    editor_ctx_t ctx;
    const char *lines[] = {"first line", "second line", "third line"};
    init_multiline_ctx(&ctx, 3, lines);

    /* Selection from row 0 col 3 to row 2 col 5 */
    ctx.sel_active = 1;
    ctx.sel_start_x = 3;
    ctx.sel_start_y = 0;
    ctx.sel_end_x = 5;
    ctx.sel_end_y = 2;

    /* Middle row: entire line selected */
    ASSERT_TRUE(is_selected(&ctx, 1, 0));
    ASSERT_TRUE(is_selected(&ctx, 1, 5));
    ASSERT_TRUE(is_selected(&ctx, 1, 10));

    editor_ctx_free(&ctx);
}

TEST(selection_multiline_last_row) {
    editor_ctx_t ctx;
    const char *lines[] = {"first line", "second line", "third line"};
    init_multiline_ctx(&ctx, 3, lines);

    /* Selection from row 0 col 3 to row 2 col 5 */
    ctx.sel_active = 1;
    ctx.sel_start_x = 3;
    ctx.sel_start_y = 0;
    ctx.sel_end_x = 5;
    ctx.sel_end_y = 2;

    /* Last row: positions from start to end_x (exclusive) */
    ASSERT_TRUE(is_selected(&ctx, 2, 0));
    ASSERT_TRUE(is_selected(&ctx, 2, 4));
    ASSERT_FALSE(is_selected(&ctx, 2, 5));  /* End position (exclusive) */
    ASSERT_FALSE(is_selected(&ctx, 2, 8));

    editor_ctx_free(&ctx);
}

TEST(selection_multiline_reversed) {
    editor_ctx_t ctx;
    const char *lines[] = {"line one", "line two", "line three"};
    init_multiline_ctx(&ctx, 3, lines);

    /* Selection reversed: end before start */
    ctx.sel_active = 1;
    ctx.sel_start_x = 4;
    ctx.sel_start_y = 2;  /* Start at row 2 (will be normalized to end) */
    ctx.sel_end_x = 2;
    ctx.sel_end_y = 0;    /* End at row 0 (will be normalized to start) */

    /* Should normalize and work correctly */
    ASSERT_TRUE(is_selected(&ctx, 0, 2));
    ASSERT_TRUE(is_selected(&ctx, 1, 0));  /* Middle row fully selected */
    ASSERT_TRUE(is_selected(&ctx, 2, 3));
    ASSERT_FALSE(is_selected(&ctx, 2, 4)); /* End position */

    editor_ctx_free(&ctx);
}

TEST(selection_row_out_of_range) {
    editor_ctx_t ctx;
    const char *lines[] = {"line one", "line two"};
    init_multiline_ctx(&ctx, 2, lines);

    ctx.sel_active = 1;
    ctx.sel_start_x = 0;
    ctx.sel_start_y = 0;
    ctx.sel_end_x = 5;
    ctx.sel_end_y = 1;

    /* Row outside selection range */
    ASSERT_FALSE(is_selected(&ctx, 3, 0));
    ASSERT_FALSE(is_selected(&ctx, -1, 0));

    editor_ctx_free(&ctx);
}

/* ============================================================================
 * base64_encode() Tests
 * ============================================================================ */

TEST(base64_encode_empty_string) {
    char *result = base64_encode("", 0);
    ASSERT_NOT_NULL(result);
    ASSERT_STR_EQ(result, "");
    free(result);
}

TEST(base64_encode_single_char) {
    /* 'M' encodes to "TQ==" */
    char *result = base64_encode("M", 1);
    ASSERT_NOT_NULL(result);
    ASSERT_STR_EQ(result, "TQ==");
    free(result);
}

TEST(base64_encode_two_chars) {
    /* "Ma" encodes to "TWE=" */
    char *result = base64_encode("Ma", 2);
    ASSERT_NOT_NULL(result);
    ASSERT_STR_EQ(result, "TWE=");
    free(result);
}

TEST(base64_encode_three_chars) {
    /* "Man" encodes to "TWFu" (no padding) */
    char *result = base64_encode("Man", 3);
    ASSERT_NOT_NULL(result);
    ASSERT_STR_EQ(result, "TWFu");
    free(result);
}

TEST(base64_encode_hello_world) {
    /* "Hello, World!" encodes to "SGVsbG8sIFdvcmxkIQ==" */
    char *result = base64_encode("Hello, World!", 13);
    ASSERT_NOT_NULL(result);
    ASSERT_STR_EQ(result, "SGVsbG8sIFdvcmxkIQ==");
    free(result);
}

TEST(base64_encode_binary_data) {
    /* Binary data with null bytes */
    char binary[] = {0x00, 0x01, 0x02, 0x03};
    char *result = base64_encode(binary, 4);
    ASSERT_NOT_NULL(result);
    /* Should produce valid base64 output */
    ASSERT_TRUE(strlen(result) > 0);
    free(result);
}

/* ============================================================================
 * get_selection_text() Tests
 * ============================================================================ */

TEST(get_selection_text_single_line) {
    editor_ctx_t ctx;
    init_single_line_ctx(&ctx, "hello world");

    ctx.sel_active = 1;
    ctx.sel_start_x = 0;
    ctx.sel_start_y = 0;
    ctx.sel_end_x = 5;
    ctx.sel_end_y = 0;

    char *text = get_selection_text(&ctx);
    ASSERT_NOT_NULL(text);
    ASSERT_STR_EQ(text, "hello");
    free(text);

    editor_ctx_free(&ctx);
}

TEST(get_selection_text_middle_of_line) {
    editor_ctx_t ctx;
    init_single_line_ctx(&ctx, "hello world");

    ctx.sel_active = 1;
    ctx.sel_start_x = 6;
    ctx.sel_start_y = 0;
    ctx.sel_end_x = 11;
    ctx.sel_end_y = 0;

    char *text = get_selection_text(&ctx);
    ASSERT_NOT_NULL(text);
    ASSERT_STR_EQ(text, "world");
    free(text);

    editor_ctx_free(&ctx);
}

TEST(get_selection_text_multiline) {
    editor_ctx_t ctx;
    const char *lines[] = {"line one", "line two", "line three"};
    init_multiline_ctx(&ctx, 3, lines);

    /* Select from row 0 col 5 to row 2 col 4 */
    ctx.sel_active = 1;
    ctx.sel_start_x = 5;
    ctx.sel_start_y = 0;
    ctx.sel_end_x = 4;
    ctx.sel_end_y = 2;

    char *text = get_selection_text(&ctx);
    ASSERT_NOT_NULL(text);
    /* Should be "one\nline two\nline" */
    ASSERT_STR_EQ(text, "one\nline two\nline");
    free(text);

    editor_ctx_free(&ctx);
}

TEST(get_selection_text_no_selection) {
    editor_ctx_t ctx;
    init_single_line_ctx(&ctx, "hello world");

    ctx.sel_active = 0;

    char *text = get_selection_text(&ctx);
    ASSERT_NULL(text);

    editor_ctx_free(&ctx);
}

TEST(get_selection_text_entire_line) {
    editor_ctx_t ctx;
    init_single_line_ctx(&ctx, "hello");

    ctx.sel_active = 1;
    ctx.sel_start_x = 0;
    ctx.sel_start_y = 0;
    ctx.sel_end_x = 5;
    ctx.sel_end_y = 0;

    char *text = get_selection_text(&ctx);
    ASSERT_NOT_NULL(text);
    ASSERT_STR_EQ(text, "hello");
    free(text);

    editor_ctx_free(&ctx);
}

/* ============================================================================
 * delete_selection() Tests
 * ============================================================================ */

TEST(delete_selection_single_line_middle) {
    editor_ctx_t ctx;
    init_single_line_ctx(&ctx, "hello world");

    ctx.sel_active = 1;
    ctx.sel_start_x = 5;
    ctx.sel_start_y = 0;
    ctx.sel_end_x = 6;
    ctx.sel_end_y = 0;

    int deleted = delete_selection(&ctx);

    ASSERT_TRUE(deleted > 0);
    ASSERT_FALSE(ctx.sel_active);
    ASSERT_STR_EQ(ctx.row[0].chars, "helloworld");

    editor_ctx_free(&ctx);
}

TEST(delete_selection_single_line_start) {
    editor_ctx_t ctx;
    init_single_line_ctx(&ctx, "hello world");

    ctx.sel_active = 1;
    ctx.sel_start_x = 0;
    ctx.sel_start_y = 0;
    ctx.sel_end_x = 6;
    ctx.sel_end_y = 0;

    int deleted = delete_selection(&ctx);

    ASSERT_TRUE(deleted > 0);
    ASSERT_STR_EQ(ctx.row[0].chars, "world");

    editor_ctx_free(&ctx);
}

TEST(delete_selection_no_selection) {
    editor_ctx_t ctx;
    init_single_line_ctx(&ctx, "hello world");

    ctx.sel_active = 0;

    int deleted = delete_selection(&ctx);

    ASSERT_EQ(deleted, 0);
    ASSERT_STR_EQ(ctx.row[0].chars, "hello world");

    editor_ctx_free(&ctx);
}

TEST(delete_selection_clears_selection) {
    editor_ctx_t ctx;
    init_single_line_ctx(&ctx, "hello");

    ctx.sel_active = 1;
    ctx.sel_start_x = 0;
    ctx.sel_start_y = 0;
    ctx.sel_end_x = 2;
    ctx.sel_end_y = 0;

    delete_selection(&ctx);

    ASSERT_FALSE(ctx.sel_active);

    editor_ctx_free(&ctx);
}

TEST(delete_selection_sets_dirty_flag) {
    editor_ctx_t ctx;
    init_single_line_ctx(&ctx, "hello");

    ctx.dirty = 0;
    ctx.sel_active = 1;
    ctx.sel_start_x = 0;
    ctx.sel_start_y = 0;
    ctx.sel_end_x = 2;
    ctx.sel_end_y = 0;

    delete_selection(&ctx);

    ASSERT_TRUE(ctx.dirty > 0);

    editor_ctx_free(&ctx);
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST(selection_empty_buffer) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);
    ctx.numrows = 0;
    ctx.row = NULL;

    ctx.sel_active = 1;
    ctx.sel_start_x = 0;
    ctx.sel_start_y = 0;
    ctx.sel_end_x = 5;
    ctx.sel_end_y = 0;

    /* Should not crash - is_selected checks sel_active and row range.
     * Row 0 is within start_y to end_y range even if buffer is empty,
     * so the function may return true for positions in the logical range.
     * The key test here is that it doesn't crash. */
    is_selected(&ctx, 0, 0);

    /* delete_selection should handle empty buffer gracefully */
    int deleted = delete_selection(&ctx);
    ASSERT_EQ(deleted, 0);

    editor_ctx_free(&ctx);
}

TEST(selection_single_character) {
    editor_ctx_t ctx;
    init_single_line_ctx(&ctx, "a");

    ctx.sel_active = 1;
    ctx.sel_start_x = 0;
    ctx.sel_start_y = 0;
    ctx.sel_end_x = 1;
    ctx.sel_end_y = 0;

    char *text = get_selection_text(&ctx);
    ASSERT_NOT_NULL(text);
    ASSERT_STR_EQ(text, "a");
    free(text);

    editor_ctx_free(&ctx);
}

TEST(selection_zero_width) {
    editor_ctx_t ctx;
    init_single_line_ctx(&ctx, "hello");

    /* Start and end at same position */
    ctx.sel_active = 1;
    ctx.sel_start_x = 2;
    ctx.sel_start_y = 0;
    ctx.sel_end_x = 2;
    ctx.sel_end_y = 0;

    /* Zero-width selection - nothing should be selected */
    ASSERT_FALSE(is_selected(&ctx, 0, 2));

    editor_ctx_free(&ctx);
}

BEGIN_TEST_SUITE("Selection and Clipboard")
    /* is_selected() - single line */
    RUN_TEST(selection_single_line_in_range);
    RUN_TEST(selection_inactive_returns_false);
    RUN_TEST(selection_reversed_single_line);

    /* is_selected() - multi-line */
    RUN_TEST(selection_multiline_first_row);
    RUN_TEST(selection_multiline_middle_row);
    RUN_TEST(selection_multiline_last_row);
    RUN_TEST(selection_multiline_reversed);
    RUN_TEST(selection_row_out_of_range);

    /* base64_encode() */
    RUN_TEST(base64_encode_empty_string);
    RUN_TEST(base64_encode_single_char);
    RUN_TEST(base64_encode_two_chars);
    RUN_TEST(base64_encode_three_chars);
    RUN_TEST(base64_encode_hello_world);
    RUN_TEST(base64_encode_binary_data);

    /* get_selection_text() */
    RUN_TEST(get_selection_text_single_line);
    RUN_TEST(get_selection_text_middle_of_line);
    RUN_TEST(get_selection_text_multiline);
    RUN_TEST(get_selection_text_no_selection);
    RUN_TEST(get_selection_text_entire_line);

    /* delete_selection() */
    RUN_TEST(delete_selection_single_line_middle);
    RUN_TEST(delete_selection_single_line_start);
    RUN_TEST(delete_selection_no_selection);
    RUN_TEST(delete_selection_clears_selection);
    RUN_TEST(delete_selection_sets_dirty_flag);

    /* Edge cases */
    RUN_TEST(selection_empty_buffer);
    RUN_TEST(selection_single_character);
    RUN_TEST(selection_zero_width);
END_TEST_SUITE()
