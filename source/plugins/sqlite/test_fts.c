/**
 * @file test_fts.c
 * @brief Unit tests for the FTS5 search index plugin.
 */

#include "test_framework.h"
#include "fts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* Helper to create a temp directory */
static char temp_dir[256];

static void create_temp_dir(void) {
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/fts_test_%d", getpid());
    mkdir(temp_dir, 0755);
}

static void cleanup_temp_dir(void) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", temp_dir);
    system(cmd);
}

/* Helper to create a test file */
static void create_test_file(const char *name, const char *content) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", temp_dir, name);
    FILE *f = fopen(path, "w");
    if (f) {
        fputs(content, f);
        fclose(f);
    }
}

/* ============================================================================
 * Test cases
 * ============================================================================ */

TEST(test_fts_open_close) {
    FTSIndex *idx = NULL;

    /* Open in-memory database */
    int rc = fts_open(&idx, ":memory:");
    ASSERT_EQ(rc, 0);
    ASSERT_NOT_NULL(idx);

    fts_close(idx);
}

TEST(test_fts_open_file) {
    create_temp_dir();

    char db_path[512];
    snprintf(db_path, sizeof(db_path), "%s/test.db", temp_dir);

    FTSIndex *idx = NULL;
    int rc = fts_open(&idx, db_path);
    ASSERT_EQ(rc, 0);
    ASSERT_NOT_NULL(idx);

    fts_close(idx);
    cleanup_temp_dir();
}

TEST(test_fts_index_file) {
    create_temp_dir();
    create_test_file("test.lua", "-- A test file\nlocal chord = 'major'\nprint(chord)");

    FTSIndex *idx = NULL;
    fts_open(&idx, ":memory:");

    char path[512];
    snprintf(path, sizeof(path), "%s/test.lua", temp_dir);

    int rc = fts_index_file(idx, path);
    ASSERT_EQ(rc, 0);

    /* Verify stats */
    FTSStats stats;
    fts_get_stats(idx, &stats);
    ASSERT_EQ(stats.file_count, 1);

    fts_close(idx);
    cleanup_temp_dir();
}

TEST(test_fts_index_directory) {
    create_temp_dir();
    create_test_file("one.lua", "local x = 1");
    create_test_file("two.lua", "local y = 2");
    create_test_file("three.txt", "plain text file");
    create_test_file("skip.bin", "binary content");  /* Should be skipped */

    FTSIndex *idx = NULL;
    fts_open(&idx, ":memory:");

    int count = fts_index_directory(idx, temp_dir, FTS_INDEX_FULL);
    ASSERT_EQ(count, 3);  /* .lua and .txt, not .bin */

    FTSStats stats;
    fts_get_stats(idx, &stats);
    ASSERT_EQ(stats.file_count, 3);

    fts_close(idx);
    cleanup_temp_dir();
}

TEST(test_fts_search_basic) {
    create_temp_dir();
    create_test_file("music.lua", "local chord = 'C major'\nlocal scale = 'pentatonic'");
    create_test_file("other.lua", "local x = 42");

    FTSIndex *idx = NULL;
    fts_open(&idx, ":memory:");
    fts_index_directory(idx, temp_dir, FTS_INDEX_FULL);

    FTSResult *results = NULL;
    int count = 0;

    int rc = fts_search(idx, "chord", &results, &count, 10);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(count, 1);
    ASSERT_NOT_NULL(results);
    ASSERT_NOT_NULL(results[0].path);
    ASSERT_TRUE(strstr(results[0].path, "music.lua") != NULL);

    fts_results_free(results, count);
    fts_close(idx);
    cleanup_temp_dir();
}

TEST(test_fts_search_phrase) {
    create_temp_dir();
    create_test_file("song.alda", "piano: c d e f g\n# C major scale");

    FTSIndex *idx = NULL;
    fts_open(&idx, ":memory:");
    fts_index_directory(idx, temp_dir, FTS_INDEX_FULL);

    FTSResult *results = NULL;
    int count = 0;

    /* FTS5 phrase search */
    int rc = fts_search(idx, "\"major scale\"", &results, &count, 10);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(count, 1);

    fts_results_free(results, count);
    fts_close(idx);
    cleanup_temp_dir();
}

TEST(test_fts_search_no_results) {
    create_temp_dir();
    create_test_file("test.lua", "hello world");

    FTSIndex *idx = NULL;
    fts_open(&idx, ":memory:");
    fts_index_directory(idx, temp_dir, FTS_INDEX_FULL);

    FTSResult *results = NULL;
    int count = 0;

    int rc = fts_search(idx, "nonexistent", &results, &count, 10);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(count, 0);

    fts_results_free(results, count);
    fts_close(idx);
    cleanup_temp_dir();
}

TEST(test_fts_search_paths) {
    create_temp_dir();
    create_test_file("alda_music.lua", "alda stuff");
    create_test_file("joy_music.lua", "joy stuff");
    create_test_file("other.txt", "other");

    FTSIndex *idx = NULL;
    fts_open(&idx, ":memory:");
    fts_index_directory(idx, temp_dir, FTS_INDEX_FULL);

    FTSResult *results = NULL;
    int count = 0;

    /* Search for files with "alda" in path */
    int rc = fts_search_paths(idx, "*alda*", &results, &count, 10);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(count, 1);
    ASSERT_NOT_NULL(results[0].path);
    ASSERT_TRUE(strstr(results[0].path, "alda") != NULL);

    fts_results_free(results, count);
    fts_close(idx);
    cleanup_temp_dir();
}

TEST(test_fts_incremental_index) {
    create_temp_dir();
    create_test_file("test.lua", "version 1");

    FTSIndex *idx = NULL;
    char db_path[512];
    snprintf(db_path, sizeof(db_path), "%s/index.db", temp_dir);
    fts_open(&idx, db_path);

    /* First index */
    int count1 = fts_index_directory(idx, temp_dir, FTS_INDEX_FULL);
    ASSERT_EQ(count1, 1);

    /* Second index (incremental, no changes) */
    int count2 = fts_index_directory(idx, temp_dir, FTS_INDEX_INCREMENTAL);
    ASSERT_EQ(count2, 0);  /* No files changed */

    /* Modify file and reindex */
    sleep(1);  /* Ensure mtime changes */
    create_test_file("test.lua", "version 2");

    int count3 = fts_index_directory(idx, temp_dir, FTS_INDEX_INCREMENTAL);
    ASSERT_EQ(count3, 1);  /* File changed */

    fts_close(idx);
    cleanup_temp_dir();
}

TEST(test_fts_clear) {
    create_temp_dir();
    create_test_file("test.lua", "content");

    FTSIndex *idx = NULL;
    fts_open(&idx, ":memory:");
    fts_index_directory(idx, temp_dir, FTS_INDEX_FULL);

    FTSStats stats;
    fts_get_stats(idx, &stats);
    ASSERT_EQ(stats.file_count, 1);

    fts_clear(idx);

    fts_get_stats(idx, &stats);
    ASSERT_EQ(stats.file_count, 0);

    fts_close(idx);
    cleanup_temp_dir();
}

TEST(test_fts_remove_file) {
    create_temp_dir();
    create_test_file("keep.lua", "keep this");
    create_test_file("remove.lua", "remove this");

    FTSIndex *idx = NULL;
    fts_open(&idx, ":memory:");
    fts_index_directory(idx, temp_dir, FTS_INDEX_FULL);

    FTSStats stats;
    fts_get_stats(idx, &stats);
    ASSERT_EQ(stats.file_count, 2);

    /* Remove one file from index */
    char path[512];
    snprintf(path, sizeof(path), "%s/remove.lua", temp_dir);
    fts_remove_file(idx, path);

    fts_get_stats(idx, &stats);
    ASSERT_EQ(stats.file_count, 1);

    fts_close(idx);
    cleanup_temp_dir();
}

/* ============================================================================
 * Test runner
 * ============================================================================ */

BEGIN_TEST_SUITE("FTS5 Search Index")
    RUN_TEST(test_fts_open_close);
    RUN_TEST(test_fts_open_file);
    RUN_TEST(test_fts_index_file);
    RUN_TEST(test_fts_index_directory);
    RUN_TEST(test_fts_search_basic);
    RUN_TEST(test_fts_search_phrase);
    RUN_TEST(test_fts_search_no_results);
    RUN_TEST(test_fts_search_paths);
    RUN_TEST(test_fts_incremental_index);
    RUN_TEST(test_fts_clear);
    RUN_TEST(test_fts_remove_file);
END_TEST_SUITE()
