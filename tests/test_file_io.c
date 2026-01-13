/* test_file_io.c - Integration tests for file I/O
 *
 * Tests for:
 * - File loading
 * - File saving
 * - Binary file detection
 * - CRLF handling
 * - Large file handling
 */

#include "test_framework.h"
#include "loki/core.h"
#include "loki_internal.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

#define TEST_FILE_DIR "/tmp/loki_test"

/* Setup: Create test directory */
static void setup_test_dir(void) {
    mkdir(TEST_FILE_DIR, 0755);
}

/* Teardown: Clean up test files */
static void cleanup_test_files(void) {
    system("rm -rf " TEST_FILE_DIR);
}

/* Helper: Create a test file with given content */
static void create_test_file(const char *filename, const char *content) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", TEST_FILE_DIR, filename);
    FILE *f = fopen(path, "w");
    if (f) {
        fputs(content, f);
        fclose(f);
    }
}

/* Helper: Read file content */
static char *read_test_file(const char *filename) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", TEST_FILE_DIR, filename);
    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *content = malloc(size + 1);
    if (content) {
        size_t read = fread(content, 1, size, f);
        content[read] = '\0';
    }
    fclose(f);
    return content;
}

/* Test loading a simple text file */
TEST(editor_open_loads_simple_file) {
    setup_test_dir();
    create_test_file("simple.txt", "Hello\nWorld\n");

    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    char path[256];
    snprintf(path, sizeof(path), "%s/simple.txt", TEST_FILE_DIR);

    int result = editor_open(&ctx, path);

    ASSERT_EQ(result, 0);
    ASSERT_EQ(ctx.numrows, 2);
    ASSERT_STR_EQ(ctx.row[0].chars, "Hello");
    ASSERT_STR_EQ(ctx.row[1].chars, "World");
    ASSERT_EQ(ctx.dirty, 0);

    /* Cleanup */
    editor_ctx_free(&ctx);
    cleanup_test_files();
}

/* Test loading file with CRLF line endings */
TEST(editor_open_handles_crlf) {
    setup_test_dir();
    create_test_file("crlf.txt", "Line1\r\nLine2\r\nLine3\r\n");

    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    char path[256];
    snprintf(path, sizeof(path), "%s/crlf.txt", TEST_FILE_DIR);

    int result = editor_open(&ctx, path);

    ASSERT_EQ(result, 0);
    ASSERT_EQ(ctx.numrows, 3);
    /* Should strip both \r and \n */
    ASSERT_STR_EQ(ctx.row[0].chars, "Line1");
    ASSERT_STR_EQ(ctx.row[1].chars, "Line2");
    ASSERT_STR_EQ(ctx.row[2].chars, "Line3");

    /* Cleanup */
    editor_ctx_free(&ctx);
    cleanup_test_files();
}

/* Test binary file detection */
TEST(editor_open_rejects_binary_file) {
    setup_test_dir();

    /* Create binary file with null bytes */
    char path[256];
    snprintf(path, sizeof(path), "%s/binary.bin", TEST_FILE_DIR);
    FILE *f = fopen(path, "wb");
    if (f) {
        char binary_data[] = {0x00, 0x01, 0x02, 0xFF, 0xFE};
        fwrite(binary_data, 1, sizeof(binary_data), f);
        fclose(f);
    }

    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    int result = editor_open(&ctx, path);

    /* Should reject binary file */
    ASSERT_NEQ(result, 0);
    ASSERT_EQ(ctx.numrows, 0);

    /* Cleanup */
    editor_ctx_free(&ctx);
    cleanup_test_files();
}

/* Test empty file */
TEST(editor_open_handles_empty_file) {
    setup_test_dir();
    create_test_file("empty.txt", "");

    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    char path[256];
    snprintf(path, sizeof(path), "%s/empty.txt", TEST_FILE_DIR);

    int result = editor_open(&ctx, path);

    ASSERT_EQ(result, 0);
    ASSERT_EQ(ctx.numrows, 0);

    /* Cleanup */
    editor_ctx_free(&ctx);
    cleanup_test_files();
}

/* Test saving file */
TEST(editor_save_writes_content) {
    setup_test_dir();

    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    /* Set up context with 2 rows */
    ctx.numrows = 2;
    ctx.row = calloc(2, sizeof(t_erow));

    ctx.row[0].chars = strdup("First line");
    ctx.row[0].size = 10;
    ctx.row[0].idx = 0;

    ctx.row[1].chars = strdup("Second line");
    ctx.row[1].size = 11;
    ctx.row[1].idx = 1;

    char path[256];
    snprintf(path, sizeof(path), "%s/output.txt", TEST_FILE_DIR);
    ctx.filename = strdup(path);
    ctx.dirty = 1;

    int result = editor_save(&ctx);

    ASSERT_EQ(result, 0);
    ASSERT_EQ(ctx.dirty, 0);

    /* Verify file content */
    char *content = read_test_file("output.txt");
    ASSERT_NOT_NULL(content);
    ASSERT_STR_EQ(content, "First line\nSecond line\n");

    free(content);
    editor_ctx_free(&ctx);
    cleanup_test_files();
}

/* Test file with no trailing newline */
TEST(editor_open_handles_no_trailing_newline) {
    setup_test_dir();

    char path[256];
    snprintf(path, sizeof(path), "%s/no_newline.txt", TEST_FILE_DIR);
    FILE *f = fopen(path, "w");
    if (f) {
        fputs("Line without newline", f);  /* No \n at end */
        fclose(f);
    }

    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    int result = editor_open(&ctx, path);

    ASSERT_EQ(result, 0);
    ASSERT_EQ(ctx.numrows, 1);
    ASSERT_STR_EQ(ctx.row[0].chars, "Line without newline");

    /* Cleanup */
    editor_ctx_free(&ctx);
    cleanup_test_files();
}

/* Test loading nonexistent file */
TEST(editor_open_handles_nonexistent_file) {
    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    int result = editor_open(&ctx, "/nonexistent/path/to/file.txt");

    /* Should fail gracefully */
    ASSERT_NEQ(result, 0);
    ASSERT_EQ(ctx.numrows, 0);

    /* Cleanup */
    editor_ctx_free(&ctx);
}

/* Test loading file with long lines */
TEST(editor_open_handles_long_lines) {
    setup_test_dir();

    /* Create file with a very long line */
    char path[256];
    snprintf(path, sizeof(path), "%s/long_line.txt", TEST_FILE_DIR);
    FILE *f = fopen(path, "w");
    if (f) {
        for (int i = 0; i < 1000; i++) {
            fputc('a', f);
        }
        fputc('\n', f);
        fclose(f);
    }

    editor_ctx_t ctx;
    editor_ctx_init(&ctx);

    int result = editor_open(&ctx, path);

    ASSERT_EQ(result, 0);
    ASSERT_EQ(ctx.numrows, 1);
    ASSERT_EQ(ctx.row[0].size, 1000);

    /* Cleanup */
    editor_ctx_free(&ctx);
    cleanup_test_files();
}

BEGIN_TEST_SUITE("File I/O Integration")
    RUN_TEST(editor_open_loads_simple_file);
    RUN_TEST(editor_open_handles_crlf);
    RUN_TEST(editor_open_rejects_binary_file);
    RUN_TEST(editor_open_handles_empty_file);
    RUN_TEST(editor_save_writes_content);
    RUN_TEST(editor_open_handles_no_trailing_newline);
    RUN_TEST(editor_open_handles_nonexistent_file);
    RUN_TEST(editor_open_handles_long_lines);
END_TEST_SUITE()
