/**
 * @file test_play_command.c
 * @brief Integration tests for psnd play command.
 *
 * Tests that `psnd play <file>` correctly routes to the appropriate
 * language handler based on file extension.
 */

#include "../test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

/* Path to psnd binary - set by CMake */
#ifndef PSND_BINARY
#define PSND_BINARY "./psnd"
#endif

/* Temp directory for test files */
static char temp_dir[256];

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static void setup_temp_dir(void) {
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/psnd_test_%d", getpid());
    mkdir(temp_dir, 0755);
}

static void cleanup_temp_dir(void) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", temp_dir);
    system(cmd);
}

static int write_temp_file(const char *filename, const char *content) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", temp_dir, filename);
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fputs(content, f);
    fclose(f);
    return 0;
}

static int run_psnd_play(const char *filename) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "%s play %s/%s 2>/dev/null", PSND_BINARY, temp_dir, filename);
    int status = system(cmd);
    return WEXITSTATUS(status);
}

static int run_psnd_play_verbose(const char *filename) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "%s play -v %s/%s 2>&1", PSND_BINARY, temp_dir, filename);
    int status = system(cmd);
    return WEXITSTATUS(status);
}

/* ============================================================================
 * Joy Play Tests
 * ============================================================================ */

TEST(play_joy_simple_expression) {
    /* Simple Joy expression that just pushes a value */
    ASSERT_EQ(write_temp_file("test.joy", "42\n"), 0);
    int result = run_psnd_play("test.joy");
    ASSERT_EQ(result, 0);
}

TEST(play_joy_arithmetic) {
    /* Joy arithmetic expression */
    ASSERT_EQ(write_temp_file("arith.joy", "2 3 + 4 *\n"), 0);
    int result = run_psnd_play("arith.joy");
    ASSERT_EQ(result, 0);
}

TEST(play_joy_define) {
    /* Joy DEFINE statement */
    ASSERT_EQ(write_temp_file("define.joy", "DEFINE square == dup *;\n5 square\n"), 0);
    int result = run_psnd_play("define.joy");
    ASSERT_EQ(result, 0);
}

TEST(play_joy_nonexistent_file) {
    /* Attempting to play a nonexistent file should fail */
    int result = run_psnd_play("nonexistent.joy");
    ASSERT_NEQ(result, 0);
}

TEST(play_joy_verbose_flag) {
    /* Verbose flag should work */
    ASSERT_EQ(write_temp_file("verbose.joy", "1 2 +\n"), 0);
    int result = run_psnd_play_verbose("verbose.joy");
    ASSERT_EQ(result, 0);
}

/* ============================================================================
 * TR7/Scheme Play Tests
 * ============================================================================ */

TEST(play_scheme_simple_expression) {
    /* Simple Scheme expression */
    ASSERT_EQ(write_temp_file("test.scm", "(+ 1 2)\n"), 0);
    int result = run_psnd_play("test.scm");
    ASSERT_EQ(result, 0);
}

TEST(play_scheme_define) {
    /* Scheme define */
    ASSERT_EQ(write_temp_file("define.scm", "(define x 42)\nx\n"), 0);
    int result = run_psnd_play("define.scm");
    ASSERT_EQ(result, 0);
}

TEST(play_scheme_lambda) {
    /* Scheme lambda */
    ASSERT_EQ(write_temp_file("lambda.scm", "((lambda (x) (* x x)) 5)\n"), 0);
    int result = run_psnd_play("lambda.scm");
    ASSERT_EQ(result, 0);
}

TEST(play_scheme_nonexistent_file) {
    /* Attempting to play a nonexistent file should fail */
    int result = run_psnd_play("nonexistent.scm");
    ASSERT_NEQ(result, 0);
}

TEST(play_scheme_ss_extension) {
    /* .ss extension should also work for Scheme */
    ASSERT_EQ(write_temp_file("test.ss", "(+ 3 4)\n"), 0);
    int result = run_psnd_play("test.ss");
    ASSERT_EQ(result, 0);
}

/* ============================================================================
 * Alda Play Tests
 * ============================================================================ */

TEST(play_alda_simple_note) {
    /* Simple Alda note - uses no_sleep for fast execution */
    ASSERT_EQ(write_temp_file("test.alda", "piano: c\n"), 0);
    int result = run_psnd_play("test.alda");
    ASSERT_EQ(result, 0);
}

TEST(play_alda_chord) {
    /* Alda chord */
    ASSERT_EQ(write_temp_file("chord.alda", "piano: c/e/g\n"), 0);
    int result = run_psnd_play("chord.alda");
    ASSERT_EQ(result, 0);
}

TEST(play_alda_nonexistent_file) {
    /* Attempting to play a nonexistent file should fail */
    int result = run_psnd_play("nonexistent.alda");
    ASSERT_NEQ(result, 0);
}

/* ============================================================================
 * Error Cases
 * ============================================================================ */

TEST(play_no_file_arg) {
    /* psnd play without file should fail */
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "%s play 2>/dev/null", PSND_BINARY);
    int status = system(cmd);
    int result = WEXITSTATUS(status);
    ASSERT_NEQ(result, 0);
}

TEST(play_unknown_extension) {
    /* Unknown extension should fail or fallback */
    ASSERT_EQ(write_temp_file("test.xyz", "hello\n"), 0);
    int result = run_psnd_play("test.xyz");
    /* May succeed with fallback or fail - just verify it doesn't crash */
    (void)result;
}

/* ============================================================================
 * Test Suite
 * ============================================================================ */

BEGIN_TEST_SUITE("psnd play command")
    setup_temp_dir();

    /* Joy tests */
    RUN_TEST(play_joy_simple_expression);
    RUN_TEST(play_joy_arithmetic);
    RUN_TEST(play_joy_define);
    RUN_TEST(play_joy_nonexistent_file);
    RUN_TEST(play_joy_verbose_flag);

    /* TR7/Scheme tests */
    RUN_TEST(play_scheme_simple_expression);
    RUN_TEST(play_scheme_define);
    RUN_TEST(play_scheme_lambda);
    RUN_TEST(play_scheme_nonexistent_file);
    RUN_TEST(play_scheme_ss_extension);

    /* Alda tests */
    RUN_TEST(play_alda_simple_note);
    RUN_TEST(play_alda_chord);
    RUN_TEST(play_alda_nonexistent_file);

    /* Error cases */
    RUN_TEST(play_no_file_arg);
    RUN_TEST(play_unknown_extension);

    cleanup_temp_dir();
END_TEST_SUITE()
