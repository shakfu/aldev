/*
 * Minimal test framework for bog
 */
#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;
static const char* g_current_test = NULL;

#define TEST(name)                                                             \
    static void test_##name(void);                                             \
    static void run_test_##name(void)                                          \
    {                                                                          \
        g_current_test = #name;                                                \
        g_tests_run++;                                                         \
        test_##name();                                                         \
    }                                                                          \
    static void test_##name(void)

#define RUN_TEST(name)                                                         \
    do {                                                                       \
        run_test_##name();                                                     \
    } while (0)

#define ASSERT(cond)                                                           \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "  FAIL: %s\n    %s:%d: %s\n", g_current_test,     \
                    __FILE__, __LINE__, #cond);                                \
            g_tests_failed++;                                                  \
            return;                                                            \
        }                                                                      \
    } while (0)

#define ASSERT_EQ(a, b)                                                        \
    do {                                                                       \
        if ((a) != (b)) {                                                      \
            fprintf(stderr, "  FAIL: %s\n    %s:%d: %s != %s\n",               \
                    g_current_test, __FILE__, __LINE__, #a, #b);               \
            g_tests_failed++;                                                  \
            return;                                                            \
        }                                                                      \
    } while (0)

#define ASSERT_STREQ(a, b)                                                     \
    do {                                                                       \
        if (strcmp((a), (b)) != 0) {                                           \
            fprintf(stderr, "  FAIL: %s\n    %s:%d: \"%s\" != \"%s\"\n",       \
                    g_current_test, __FILE__, __LINE__, (a), (b));             \
            g_tests_failed++;                                                  \
            return;                                                            \
        }                                                                      \
    } while (0)

#define ASSERT_NEAR(a, b, epsilon)                                             \
    do {                                                                       \
        if (fabs((a) - (b)) > (epsilon)) {                                     \
            fprintf(stderr, "  FAIL: %s\n    %s:%d: %g != %g (eps=%g)\n",      \
                    g_current_test, __FILE__, __LINE__, (double)(a),           \
                    (double)(b), (double)(epsilon));                           \
            g_tests_failed++;                                                  \
            return;                                                            \
        }                                                                      \
    } while (0)

#define ASSERT_NULL(ptr)                                                       \
    do {                                                                       \
        if ((ptr) != NULL) {                                                   \
            fprintf(stderr, "  FAIL: %s\n    %s:%d: %s is not NULL\n",         \
                    g_current_test, __FILE__, __LINE__, #ptr);                 \
            g_tests_failed++;                                                  \
            return;                                                            \
        }                                                                      \
    } while (0)

#define ASSERT_NOT_NULL(ptr)                                                   \
    do {                                                                       \
        if ((ptr) == NULL) {                                                   \
            fprintf(stderr, "  FAIL: %s\n    %s:%d: %s is NULL\n",             \
                    g_current_test, __FILE__, __LINE__, #ptr);                 \
            g_tests_failed++;                                                  \
            return;                                                            \
        }                                                                      \
    } while (0)

#define TEST_PASS()                                                            \
    do {                                                                       \
        g_tests_passed++;                                                      \
        printf("  PASS: %s\n", g_current_test);                                \
    } while (0)

#define TEST_SUMMARY()                                                         \
    do {                                                                       \
        printf("\n%d tests, %d passed, %d failed\n", g_tests_run,              \
               g_tests_passed, g_tests_failed);                                \
    } while (0)

#define TEST_EXIT_CODE() (g_tests_failed > 0 ? 1 : 0)

#endif /* TEST_FRAMEWORK_H */
