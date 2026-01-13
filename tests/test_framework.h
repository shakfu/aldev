/* test_framework.h - Simple C testing framework
 *
 * A minimal testing framework with no external dependencies.
 * Provides assertion macros and test runner infrastructure.
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Test statistics */
typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
    int current_test_failed;
    const char *current_test_name;
} test_stats_t;

extern test_stats_t test_stats;

/* Color output for terminals */
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_RESET   "\x1b[0m"

/* Test macros */
#define TEST(name) \
    static void test_##name(void); \
    static void test_##name##_wrapper(void) { \
        test_stats.current_test_name = #name; \
        test_stats.current_test_failed = 0; \
        test_##name(); \
        if (!test_stats.current_test_failed) { \
            test_stats.passed_tests++; \
            printf(COLOR_GREEN "  ✓ " COLOR_RESET "%s\n", #name); \
        } \
    } \
    static void test_##name(void)

#define RUN_TEST(name) do { \
    test_stats.total_tests++; \
    test_##name##_wrapper(); \
} while(0)

/* Assertion macros */
#define ASSERT_TRUE(condition) do { \
    if (!(condition)) { \
        printf(COLOR_RED "  ✗ " COLOR_RESET "%s:%d: Assertion failed: %s\n", \
               __FILE__, __LINE__, #condition); \
        test_stats.current_test_failed = 1; \
        test_stats.failed_tests++; \
        return; \
    } \
} while(0)

#define ASSERT_FALSE(condition) ASSERT_TRUE(!(condition))

#define ASSERT_EQ(actual, expected) do { \
    if ((actual) != (expected)) { \
        printf(COLOR_RED "  ✗ " COLOR_RESET "%s:%d: Expected %d, got %d\n", \
               __FILE__, __LINE__, (int)(expected), (int)(actual)); \
        test_stats.current_test_failed = 1; \
        test_stats.failed_tests++; \
        return; \
    } \
} while(0)

#define ASSERT_NEQ(actual, expected) do { \
    if ((actual) == (expected)) { \
        printf(COLOR_RED "  ✗ " COLOR_RESET "%s:%d: Expected not equal to %d\n", \
               __FILE__, __LINE__, (int)(expected)); \
        test_stats.current_test_failed = 1; \
        test_stats.failed_tests++; \
        return; \
    } \
} while(0)

#define ASSERT_STR_EQ(actual, expected) do { \
    if (strcmp((actual), (expected)) != 0) { \
        printf(COLOR_RED "  ✗ " COLOR_RESET "%s:%d: Expected \"%s\", got \"%s\"\n", \
               __FILE__, __LINE__, (expected), (actual)); \
        test_stats.current_test_failed = 1; \
        test_stats.failed_tests++; \
        return; \
    } \
} while(0)

#define ASSERT_NULL(ptr) do { \
    if ((ptr) != NULL) { \
        printf(COLOR_RED "  ✗ " COLOR_RESET "%s:%d: Expected NULL pointer\n", \
               __FILE__, __LINE__); \
        test_stats.current_test_failed = 1; \
        test_stats.failed_tests++; \
        return; \
    } \
} while(0)

#define ASSERT_NOT_NULL(ptr) do { \
    if ((ptr) == NULL) { \
        printf(COLOR_RED "  ✗ " COLOR_RESET "%s:%d: Expected non-NULL pointer\n", \
               __FILE__, __LINE__); \
        test_stats.current_test_failed = 1; \
        test_stats.failed_tests++; \
        return; \
    } \
} while(0)

/* Test suite infrastructure */
#define BEGIN_TEST_SUITE(name) \
    int main(void) { \
        printf("\n" COLOR_YELLOW "Running test suite: " COLOR_RESET "%s\n\n", name); \
        test_stats.total_tests = 0; \
        test_stats.passed_tests = 0; \
        test_stats.failed_tests = 0;

#define END_TEST_SUITE() \
        printf("\n" COLOR_YELLOW "Results: " COLOR_RESET); \
        if (test_stats.failed_tests == 0) { \
            printf(COLOR_GREEN "%d/%d tests passed\n" COLOR_RESET, \
                   test_stats.passed_tests, test_stats.total_tests); \
            return 0; \
        } else { \
            printf(COLOR_RED "%d/%d tests passed, %d failed\n" COLOR_RESET, \
                   test_stats.passed_tests, test_stats.total_tests, \
                   test_stats.failed_tests); \
            return 1; \
        } \
    }

/* Helper for setup/teardown */
typedef void (*test_func_t)(void);

void run_test_with_setup(test_func_t setup, test_func_t test, test_func_t teardown);

#endif /* TEST_FRAMEWORK_H */
