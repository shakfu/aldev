/* test_buffers.c - Unit tests for multiple buffer management
 *
 * Tests buffer creation, switching, closing, and tab rendering.
 */

#include "test_framework.h"
#include "loki/core.h"
#include "loki_internal.h"
#include "loki_buffers.h"
#include <stdio.h>
#include <string.h>

/* Helper: Initialize a minimal editor context for testing */
static void init_test_context(editor_ctx_t *ctx) {
    memset(ctx, 0, sizeof(editor_ctx_t));
    ctx->cx = 0;
    ctx->cy = 0;
    ctx->rowoff = 0;
    ctx->coloff = 0;
    ctx->numrows = 0;
    ctx->row = NULL;
    ctx->dirty = 0;
    ctx->filename = NULL;
    ctx->syntax = NULL;
    ctx->mode = MODE_NORMAL;
    ctx->screencols = 80;
    ctx->screenrows = 24;
}

/* Test: Buffer initialization */
TEST(buffer_init) {
    editor_ctx_t ctx;
    init_test_context(&ctx);

    ASSERT_EQ(buffers_init(&ctx), 0);
    ASSERT_EQ(buffer_count(), 1);
    ASSERT_NOT_NULL(buffer_get_current());
    ASSERT_TRUE(buffer_get_current_id() >= 0);

    buffers_free();
}

/* Test: Buffer creation */
TEST(buffer_create) {
    editor_ctx_t ctx;
    init_test_context(&ctx);

    buffers_init(&ctx);

    int id1 = buffer_get_current_id();
    int id2 = buffer_create(NULL);

    ASSERT_TRUE(id2 > 0);
    ASSERT_NEQ(id2, id1);
    ASSERT_EQ(buffer_count(), 2);

    buffers_free();
}

/* Test: Buffer switching */
TEST(buffer_switch) {
    editor_ctx_t ctx;
    init_test_context(&ctx);

    buffers_init(&ctx);

    int id1 = buffer_get_current_id();
    int id2 = buffer_create(NULL);

    ASSERT_EQ(buffer_switch(id2), 0);
    ASSERT_EQ(buffer_get_current_id(), id2);

    ASSERT_EQ(buffer_switch(id1), 0);
    ASSERT_EQ(buffer_get_current_id(), id1);

    buffers_free();
}

/* Test: Buffer next/previous navigation */
TEST(buffer_navigation) {
    editor_ctx_t ctx;
    init_test_context(&ctx);

    buffers_init(&ctx);

    int id1 = buffer_get_current_id();
    int id2 = buffer_create(NULL);
    int id3 = buffer_create(NULL);

    buffer_switch(id1);  /* Start at buffer 1 */

    int next = buffer_next();
    ASSERT_EQ(next, id2);

    next = buffer_next();
    ASSERT_EQ(next, id3);

    next = buffer_next();
    ASSERT_EQ(next, id1);

    int prev = buffer_prev();
    ASSERT_EQ(prev, id3);

    buffers_free();
}

/* Test: Buffer closing */
TEST(buffer_close) {
    editor_ctx_t ctx;
    init_test_context(&ctx);

    buffers_init(&ctx);

    int id1 = buffer_get_current_id();
    int id2 = buffer_create(NULL);
    int id3 = buffer_create(NULL);

    ASSERT_EQ(buffer_count(), 3);

    /* Close buffer 2 */
    buffer_switch(id2);
    ASSERT_EQ(buffer_close(id2, 0), 0);
    ASSERT_EQ(buffer_count(), 2);

    /* Current buffer should have switched */
    int current = buffer_get_current_id();
    ASSERT_NEQ(current, id2);

    /* Cannot close last buffer */
    buffer_switch(id1);
    buffer_close(id3, 0);
    ASSERT_NEQ(buffer_close(id1, 0), 0);

    buffers_free();
}

/* Test: Buffer modified state */
TEST(buffer_modified) {
    editor_ctx_t ctx;
    init_test_context(&ctx);

    buffers_init(&ctx);

    int id = buffer_get_current_id();
    editor_ctx_t *buf_ctx = buffer_get_current();

    ASSERT_EQ(buffer_is_modified(id), 0);

    /* Mark buffer as modified */
    buf_ctx->dirty = 1;

    ASSERT_EQ(buffer_is_modified(id), 1);

    /* Try to close modified buffer without force */
    ASSERT_NEQ(buffer_close(id, 0), 0);

    buffers_free();
}

/* Test: Buffer display name */
TEST(buffer_display_name) {
    editor_ctx_t ctx;
    init_test_context(&ctx);

    buffers_init(&ctx);

    int id = buffer_get_current_id();
    const char *name = buffer_get_display_name(id);

    ASSERT_NOT_NULL(name);
    ASSERT_STR_EQ(name, "[No Name]");

    buffers_free();
}

/* Test: Buffer list */
TEST(buffer_list) {
    editor_ctx_t ctx;
    init_test_context(&ctx);

    buffers_init(&ctx);

    int id1 = buffer_get_current_id();
    int id2 = buffer_create(NULL);
    int id3 = buffer_create(NULL);

    int ids[MAX_BUFFERS];
    int count = buffer_get_list(ids);

    ASSERT_EQ(count, 3);
    ASSERT_EQ(ids[0], id1);
    ASSERT_EQ(ids[1], id2);
    ASSERT_EQ(ids[2], id3);

    buffers_free();
}

/* Test: Maximum buffers limit */
TEST(buffer_limit) {
    editor_ctx_t ctx;
    init_test_context(&ctx);

    buffers_init(&ctx);

    /* Create MAX_BUFFERS - 1 more buffers (already have 1) */
    for (int i = 1; i < MAX_BUFFERS; i++) {
        int id = buffer_create(NULL);
        ASSERT_TRUE(id > 0);
    }

    ASSERT_EQ(buffer_count(), MAX_BUFFERS);

    /* Try to create one more - should fail */
    int overflow_id = buffer_create(NULL);
    ASSERT_EQ(overflow_id, -1);
    ASSERT_EQ(buffer_count(), MAX_BUFFERS);

    buffers_free();
}

/* Test: Buffer tab rendering */
TEST(buffer_tabs_rendering) {
    editor_ctx_t ctx;
    init_test_context(&ctx);

    buffers_init(&ctx);

    /* Single buffer - should not render tabs */
    struct abuf ab = {NULL, 0};
    buffers_render_tabs(&ab, 80);
    ASSERT_EQ(ab.len, 0);

    /* Multiple buffers - should render tabs */
    buffer_create(NULL);
    buffer_create(NULL);

    ab.b = NULL;
    ab.len = 0;
    buffers_render_tabs(&ab, 80);
    ASSERT_TRUE(ab.len > 0);

    if (ab.b) free(ab.b);

    buffers_free();
}

/* Test suite */
BEGIN_TEST_SUITE("Buffer Management")
    RUN_TEST(buffer_init);
    RUN_TEST(buffer_create);
    RUN_TEST(buffer_switch);
    RUN_TEST(buffer_navigation);
    RUN_TEST(buffer_close);
    RUN_TEST(buffer_modified);
    RUN_TEST(buffer_display_name);
    RUN_TEST(buffer_list);
    RUN_TEST(buffer_limit);
    RUN_TEST(buffer_tabs_rendering);
END_TEST_SUITE()
