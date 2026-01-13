/* test_search.c - Unit tests for text search functionality
 *
 * Tests for incremental text search:
 * - Match finding (forward and backward)
 * - Navigation between matches
 * - Wrapping at file boundaries
 * - Case sensitivity
 * - Multiple matches
 * - Edge cases (empty buffer, no match, etc.)
 */

#include "test_framework.h"
#include "loki/core.h"
#include "loki_internal.h"
#include <string.h>
#include <stdlib.h>

/* Helper: Create multi-line buffer with content */
static void init_search_buffer(editor_ctx_t *ctx, int num_lines, const char **lines) {
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

/* Helper: Free buffer resources */
static void free_search_buffer(editor_ctx_t *ctx) {
    for (int i = 0; i < ctx->numrows; i++) {
        free(ctx->row[i].chars);
        free(ctx->row[i].render);
        if (ctx->row[i].hl) free(ctx->row[i].hl);
    }
    free(ctx->row);
    ctx->row = NULL;
    ctx->numrows = 0;
}

/* ============================================================================
 * Basic Search Tests
 * ============================================================================ */

TEST(search_find_simple_match) {
    editor_ctx_t ctx;
    const char *lines[] = {
        "hello world",
        "foo bar",
        "test line"
    };
    init_search_buffer(&ctx, 3, lines);

    int match_offset = 0;
    int result = editor_find_next_match(&ctx, "foo", -1, 1, &match_offset);

    ASSERT_EQ(result, 1);  /* Found in row 1 */
    ASSERT_EQ(match_offset, 0);  /* At start of line */

    free_search_buffer(&ctx);
    editor_ctx_free(&ctx);
}

TEST(search_find_match_mid_line) {
    editor_ctx_t ctx;
    const char *lines[] = {
        "hello world",
        "the quick brown fox",
        "test line"
    };
    init_search_buffer(&ctx, 3, lines);

    int match_offset = 0;
    int result = editor_find_next_match(&ctx, "quick", -1, 1, &match_offset);

    ASSERT_EQ(result, 1);  /* Found in row 1 */
    ASSERT_EQ(match_offset, 4);  /* After "the " */

    free_search_buffer(&ctx);
    editor_ctx_free(&ctx);
}

TEST(search_no_match_found) {
    editor_ctx_t ctx;
    const char *lines[] = {
        "hello world",
        "foo bar",
        "test line"
    };
    init_search_buffer(&ctx, 3, lines);

    int match_offset = 0;
    int result = editor_find_next_match(&ctx, "notfound", -1, 1, &match_offset);

    ASSERT_EQ(result, -1);  /* No match */

    free_search_buffer(&ctx);
    editor_ctx_free(&ctx);
}

TEST(search_empty_query) {
    editor_ctx_t ctx;
    const char *lines[] = {
        "hello world"
    };
    init_search_buffer(&ctx, 1, lines);

    int match_offset = 0;
    int result = editor_find_next_match(&ctx, "", -1, 1, &match_offset);

    ASSERT_EQ(result, -1);  /* Empty query returns no match */

    free_search_buffer(&ctx);
    editor_ctx_free(&ctx);
}

TEST(search_empty_buffer) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);
    ctx.numrows = 0;
    ctx.row = NULL;

    int match_offset = 0;
    int result = editor_find_next_match(&ctx, "test", -1, 1, &match_offset);

    ASSERT_EQ(result, -1);  /* Empty buffer returns no match */

    editor_ctx_free(&ctx);
}

/* ============================================================================
 * Forward Navigation Tests
 * ============================================================================ */

TEST(search_forward_from_start) {
    editor_ctx_t ctx;
    const char *lines[] = {
        "first line",
        "second test",
        "third test",
        "fourth line"
    };
    init_search_buffer(&ctx, 4, lines);

    int match_offset = 0;
    int result = editor_find_next_match(&ctx, "test", -1, 1, &match_offset);

    ASSERT_EQ(result, 1);  /* First match in row 1 */
    ASSERT_EQ(match_offset, 7);  /* Position of "test" */

    free_search_buffer(&ctx);
    editor_ctx_free(&ctx);
}

TEST(search_forward_next_match) {
    editor_ctx_t ctx;
    const char *lines[] = {
        "first line",
        "second test",
        "third test",
        "fourth line"
    };
    init_search_buffer(&ctx, 4, lines);

    int match_offset = 0;
    /* Find first match */
    int result = editor_find_next_match(&ctx, "test", -1, 1, &match_offset);
    ASSERT_EQ(result, 1);

    /* Find next match from row 1 */
    result = editor_find_next_match(&ctx, "test", 1, 1, &match_offset);
    ASSERT_EQ(result, 2);  /* Next match in row 2 */
    ASSERT_EQ(match_offset, 6);

    free_search_buffer(&ctx);
    editor_ctx_free(&ctx);
}

TEST(search_forward_wraps_to_start) {
    editor_ctx_t ctx;
    const char *lines[] = {
        "test line",
        "middle line",
        "end line"
    };
    init_search_buffer(&ctx, 3, lines);

    int match_offset = 0;
    /* Search from row 1 (past the only match) */
    int result = editor_find_next_match(&ctx, "test", 1, 1, &match_offset);

    ASSERT_EQ(result, 0);  /* Wraps to row 0 */
    ASSERT_EQ(match_offset, 0);

    free_search_buffer(&ctx);
    editor_ctx_free(&ctx);
}

/* ============================================================================
 * Backward Navigation Tests
 * ============================================================================ */

TEST(search_backward_from_end) {
    editor_ctx_t ctx;
    const char *lines[] = {
        "first test",
        "second test",
        "third line"
    };
    init_search_buffer(&ctx, 3, lines);

    int match_offset = 0;
    /* Start from row 2 (last row), search backward */
    int result = editor_find_next_match(&ctx, "test", 2, -1, &match_offset);

    ASSERT_EQ(result, 1);  /* Found in row 1 */
    ASSERT_EQ(match_offset, 7);

    free_search_buffer(&ctx);
    editor_ctx_free(&ctx);
}

TEST(search_backward_prev_match) {
    editor_ctx_t ctx;
    const char *lines[] = {
        "first test",
        "second test",
        "third line"
    };
    init_search_buffer(&ctx, 3, lines);

    int match_offset = 0;
    /* Start from row 1, search backward */
    int result = editor_find_next_match(&ctx, "test", 1, -1, &match_offset);

    ASSERT_EQ(result, 0);  /* Found in row 0 */
    ASSERT_EQ(match_offset, 6);

    free_search_buffer(&ctx);
    editor_ctx_free(&ctx);
}

TEST(search_backward_wraps_to_end) {
    editor_ctx_t ctx;
    const char *lines[] = {
        "first line",
        "second line",
        "test line"
    };
    init_search_buffer(&ctx, 3, lines);

    int match_offset = 0;
    /* Start from row 0 (before the only match), search backward */
    int result = editor_find_next_match(&ctx, "test", 0, -1, &match_offset);

    ASSERT_EQ(result, 2);  /* Wraps to row 2 */
    ASSERT_EQ(match_offset, 0);

    free_search_buffer(&ctx);
    editor_ctx_free(&ctx);
}

/* ============================================================================
 * Case Sensitivity Tests
 * ============================================================================ */

TEST(search_case_sensitive_exact) {
    editor_ctx_t ctx;
    const char *lines[] = {
        "Hello World",
        "hello world",
        "HELLO WORLD"
    };
    init_search_buffer(&ctx, 3, lines);

    int match_offset = 0;
    int result = editor_find_next_match(&ctx, "hello", -1, 1, &match_offset);

    ASSERT_EQ(result, 1);  /* Only matches lowercase "hello" */
    ASSERT_EQ(match_offset, 0);

    free_search_buffer(&ctx);
    editor_ctx_free(&ctx);
}

TEST(search_case_sensitive_uppercase) {
    editor_ctx_t ctx;
    const char *lines[] = {
        "hello world",
        "Hello World",
        "HELLO WORLD"
    };
    init_search_buffer(&ctx, 3, lines);

    int match_offset = 0;
    int result = editor_find_next_match(&ctx, "HELLO", -1, 1, &match_offset);

    ASSERT_EQ(result, 2);  /* Only matches uppercase "HELLO" */
    ASSERT_EQ(match_offset, 0);

    free_search_buffer(&ctx);
    editor_ctx_free(&ctx);
}

/* ============================================================================
 * Multiple Matches Tests
 * ============================================================================ */

TEST(search_multiple_matches_in_buffer) {
    editor_ctx_t ctx;
    const char *lines[] = {
        "test one",
        "test two",
        "test three"
    };
    init_search_buffer(&ctx, 3, lines);

    int match_offset = 0;

    /* First match */
    int result = editor_find_next_match(&ctx, "test", -1, 1, &match_offset);
    ASSERT_EQ(result, 0);

    /* Second match */
    result = editor_find_next_match(&ctx, "test", 0, 1, &match_offset);
    ASSERT_EQ(result, 1);

    /* Third match */
    result = editor_find_next_match(&ctx, "test", 1, 1, &match_offset);
    ASSERT_EQ(result, 2);

    free_search_buffer(&ctx);
    editor_ctx_free(&ctx);
}

TEST(search_multiple_matches_same_line) {
    editor_ctx_t ctx;
    const char *lines[] = {
        "test test test"
    };
    init_search_buffer(&ctx, 1, lines);

    int match_offset = 0;
    /* strstr finds first occurrence */
    int result = editor_find_next_match(&ctx, "test", -1, 1, &match_offset);

    ASSERT_EQ(result, 0);
    ASSERT_EQ(match_offset, 0);  /* First "test" at position 0 */

    free_search_buffer(&ctx);
    editor_ctx_free(&ctx);
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST(search_single_line_buffer) {
    editor_ctx_t ctx;
    const char *lines[] = {
        "hello world test"
    };
    init_search_buffer(&ctx, 1, lines);

    int match_offset = 0;
    int result = editor_find_next_match(&ctx, "test", -1, 1, &match_offset);

    ASSERT_EQ(result, 0);
    ASSERT_EQ(match_offset, 12);

    free_search_buffer(&ctx);
    editor_ctx_free(&ctx);
}

TEST(search_match_entire_line) {
    editor_ctx_t ctx;
    const char *lines[] = {
        "first",
        "test",
        "third"
    };
    init_search_buffer(&ctx, 3, lines);

    int match_offset = 0;
    int result = editor_find_next_match(&ctx, "test", -1, 1, &match_offset);

    ASSERT_EQ(result, 1);
    ASSERT_EQ(match_offset, 0);

    free_search_buffer(&ctx);
    editor_ctx_free(&ctx);
}

TEST(search_partial_word_match) {
    editor_ctx_t ctx;
    const char *lines[] = {
        "testing contest fastest"
    };
    init_search_buffer(&ctx, 1, lines);

    int match_offset = 0;
    /* Search finds "test" in "testing" (partial match) */
    int result = editor_find_next_match(&ctx, "test", -1, 1, &match_offset);

    ASSERT_EQ(result, 0);
    ASSERT_EQ(match_offset, 0);  /* Finds "test" in "testing" */

    free_search_buffer(&ctx);
    editor_ctx_free(&ctx);
}

TEST(search_with_special_chars) {
    editor_ctx_t ctx;
    const char *lines[] = {
        "hello(world)",
        "test[123]",
        "foo{bar}"
    };
    init_search_buffer(&ctx, 3, lines);

    int match_offset = 0;
    int result = editor_find_next_match(&ctx, "[123]", -1, 1, &match_offset);

    ASSERT_EQ(result, 1);
    ASSERT_EQ(match_offset, 4);

    free_search_buffer(&ctx);
    editor_ctx_free(&ctx);
}

TEST(search_null_context) {
    int match_offset = 0;
    int result = editor_find_next_match(NULL, "test", -1, 1, &match_offset);

    ASSERT_EQ(result, -1);  /* NULL context returns -1 */
}

TEST(search_null_query) {
    editor_ctx_t ctx;
    const char *lines[] = { "test" };
    init_search_buffer(&ctx, 1, lines);

    int match_offset = 0;
    int result = editor_find_next_match(&ctx, NULL, -1, 1, &match_offset);

    ASSERT_EQ(result, -1);  /* NULL query returns -1 */

    free_search_buffer(&ctx);
    editor_ctx_free(&ctx);
}

TEST(search_null_match_offset) {
    editor_ctx_t ctx;
    const char *lines[] = { "test" };
    init_search_buffer(&ctx, 1, lines);

    int result = editor_find_next_match(&ctx, "test", -1, 1, NULL);

    ASSERT_EQ(result, -1);  /* NULL match_offset returns -1 */

    free_search_buffer(&ctx);
    editor_ctx_free(&ctx);
}

/* ============================================================================
 * Wrapping Behavior Tests
 * ============================================================================ */

TEST(search_forward_complete_wrap) {
    editor_ctx_t ctx;
    const char *lines[] = {
        "test",
        "middle",
        "end"
    };
    init_search_buffer(&ctx, 3, lines);

    int match_offset = 0;
    /* Start from row 0, find match in row 0 */
    int result = editor_find_next_match(&ctx, "test", -1, 1, &match_offset);
    ASSERT_EQ(result, 0);

    /* Continue from row 0, should wrap and find row 0 again */
    result = editor_find_next_match(&ctx, "test", 0, 1, &match_offset);
    ASSERT_EQ(result, 0);  /* Wraps back to start */

    free_search_buffer(&ctx);
    editor_ctx_free(&ctx);
}

TEST(search_backward_complete_wrap) {
    editor_ctx_t ctx;
    const char *lines[] = {
        "start",
        "middle",
        "test"
    };
    init_search_buffer(&ctx, 3, lines);

    int match_offset = 0;
    /* Start from row 2, find match in row 2 */
    int result = editor_find_next_match(&ctx, "test", 3, -1, &match_offset);
    ASSERT_EQ(result, 2);

    /* Continue from row 2 backward, should wrap to row 2 */
    result = editor_find_next_match(&ctx, "test", 2, -1, &match_offset);
    ASSERT_EQ(result, 2);  /* Wraps back to end */

    free_search_buffer(&ctx);
    editor_ctx_free(&ctx);
}

BEGIN_TEST_SUITE("Search")
    /* Basic search */
    RUN_TEST(search_find_simple_match);
    RUN_TEST(search_find_match_mid_line);
    RUN_TEST(search_no_match_found);
    RUN_TEST(search_empty_query);
    RUN_TEST(search_empty_buffer);

    /* Forward navigation */
    RUN_TEST(search_forward_from_start);
    RUN_TEST(search_forward_next_match);
    RUN_TEST(search_forward_wraps_to_start);

    /* Backward navigation */
    RUN_TEST(search_backward_from_end);
    RUN_TEST(search_backward_prev_match);
    RUN_TEST(search_backward_wraps_to_end);

    /* Case sensitivity */
    RUN_TEST(search_case_sensitive_exact);
    RUN_TEST(search_case_sensitive_uppercase);

    /* Multiple matches */
    RUN_TEST(search_multiple_matches_in_buffer);
    RUN_TEST(search_multiple_matches_same_line);

    /* Edge cases */
    RUN_TEST(search_single_line_buffer);
    RUN_TEST(search_match_entire_line);
    RUN_TEST(search_partial_word_match);
    RUN_TEST(search_with_special_chars);
    RUN_TEST(search_null_context);
    RUN_TEST(search_null_query);
    RUN_TEST(search_null_match_offset);

    /* Wrapping behavior */
    RUN_TEST(search_forward_complete_wrap);
    RUN_TEST(search_backward_complete_wrap);
END_TEST_SUITE()
