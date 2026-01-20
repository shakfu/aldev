/**
 * @file test_primitives.c
 * @brief Unit tests for Joy primitives and stack operations.
 *
 * Tests stack operations (dup, swap, pop) and arithmetic primitives.
 */

#include "test_framework.h"
#include "joy_runtime.h"
#include "joy_parser.h"
#include <string.h>
#include <math.h>

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static JoyContext* ctx = NULL;

static void setup_context(void) {
    ctx = joy_context_new();
    joy_register_primitives(ctx);
}

static void teardown_context(void) {
    if (ctx) {
        joy_context_free(ctx);
        ctx = NULL;
    }
}

/* Execute Joy code and return success */
static int eval_ok(const char* code) {
    joy_eval_line(ctx, code);
    return 1;
}

/* Get top of stack as integer */
static int64_t stack_top_int(void) {
    JoyValue v = joy_stack_peek(ctx->stack);
    return v.data.integer;
}

/* Get stack depth */
static size_t stack_depth(void) {
    return joy_stack_depth(ctx->stack);
}

/* ============================================================================
 * Stack Operation Tests
 * ============================================================================ */

TEST(stack_push_integer) {
    setup_context();
    eval_ok("42");
    ASSERT_EQ(stack_depth(), 1);
    ASSERT_EQ(stack_top_int(), 42);
    teardown_context();
}

TEST(stack_push_multiple) {
    setup_context();
    eval_ok("1 2 3");
    ASSERT_EQ(stack_depth(), 3);
    ASSERT_EQ(stack_top_int(), 3);
    teardown_context();
}

TEST(stack_dup) {
    setup_context();
    eval_ok("42 dup");
    ASSERT_EQ(stack_depth(), 2);
    ASSERT_EQ(stack_top_int(), 42);
    teardown_context();
}

TEST(stack_swap) {
    setup_context();
    eval_ok("1 2 swap");
    ASSERT_EQ(stack_depth(), 2);
    ASSERT_EQ(stack_top_int(), 1);
    teardown_context();
}

TEST(stack_pop) {
    setup_context();
    eval_ok("1 2 pop");
    ASSERT_EQ(stack_depth(), 1);
    ASSERT_EQ(stack_top_int(), 1);
    teardown_context();
}

TEST(stack_pop_all) {
    setup_context();
    eval_ok("1 2 3 pop pop pop");
    ASSERT_EQ(stack_depth(), 0);
    teardown_context();
}

/* ============================================================================
 * Arithmetic Tests
 * ============================================================================ */

TEST(arithmetic_add) {
    setup_context();
    eval_ok("3 4 +");
    ASSERT_EQ(stack_depth(), 1);
    ASSERT_EQ(stack_top_int(), 7);
    teardown_context();
}

TEST(arithmetic_sub) {
    setup_context();
    eval_ok("10 3 -");
    ASSERT_EQ(stack_depth(), 1);
    ASSERT_EQ(stack_top_int(), 7);
    teardown_context();
}

TEST(arithmetic_mul) {
    setup_context();
    eval_ok("6 7 *");
    ASSERT_EQ(stack_depth(), 1);
    ASSERT_EQ(stack_top_int(), 42);
    teardown_context();
}

TEST(arithmetic_div) {
    setup_context();
    eval_ok("20 4 /");
    ASSERT_EQ(stack_depth(), 1);
    ASSERT_EQ(stack_top_int(), 5);
    teardown_context();
}

TEST(arithmetic_mod) {
    setup_context();
    eval_ok("17 5 rem");
    ASSERT_EQ(stack_depth(), 1);
    ASSERT_EQ(stack_top_int(), 2);
    teardown_context();
}

TEST(arithmetic_neg) {
    setup_context();
    eval_ok("42 neg");
    ASSERT_EQ(stack_depth(), 1);
    ASSERT_EQ(stack_top_int(), -42);
    teardown_context();
}

TEST(arithmetic_abs) {
    setup_context();
    eval_ok("-42 abs");
    ASSERT_EQ(stack_depth(), 1);
    ASSERT_EQ(stack_top_int(), 42);
    teardown_context();
}

/* ============================================================================
 * Comparison Tests
 * ============================================================================ */

TEST(comparison_eq_true) {
    setup_context();
    eval_ok("5 5 =");
    ASSERT_EQ(stack_depth(), 1);
    JoyValue v = joy_stack_peek(ctx->stack);
    ASSERT_EQ(v.type, JOY_BOOLEAN);
    ASSERT_TRUE(v.data.boolean);
    teardown_context();
}

TEST(comparison_eq_false) {
    setup_context();
    eval_ok("5 6 =");
    ASSERT_EQ(stack_depth(), 1);
    JoyValue v = joy_stack_peek(ctx->stack);
    ASSERT_EQ(v.type, JOY_BOOLEAN);
    ASSERT_FALSE(v.data.boolean);
    teardown_context();
}

TEST(comparison_lt) {
    setup_context();
    eval_ok("3 5 <");
    ASSERT_EQ(stack_depth(), 1);
    JoyValue v = joy_stack_peek(ctx->stack);
    ASSERT_TRUE(v.data.boolean);
    teardown_context();
}

TEST(comparison_gt) {
    setup_context();
    eval_ok("5 3 >");
    ASSERT_EQ(stack_depth(), 1);
    JoyValue v = joy_stack_peek(ctx->stack);
    ASSERT_TRUE(v.data.boolean);
    teardown_context();
}

/* ============================================================================
 * Boolean Logic Tests
 * ============================================================================ */

TEST(logic_and_true) {
    setup_context();
    eval_ok("true true and");
    ASSERT_EQ(stack_depth(), 1);
    JoyValue v = joy_stack_peek(ctx->stack);
    ASSERT_TRUE(v.data.boolean);
    teardown_context();
}

TEST(logic_and_false) {
    setup_context();
    eval_ok("true false and");
    ASSERT_EQ(stack_depth(), 1);
    JoyValue v = joy_stack_peek(ctx->stack);
    ASSERT_FALSE(v.data.boolean);
    teardown_context();
}

TEST(logic_or_true) {
    setup_context();
    eval_ok("false true or");
    ASSERT_EQ(stack_depth(), 1);
    JoyValue v = joy_stack_peek(ctx->stack);
    ASSERT_TRUE(v.data.boolean);
    teardown_context();
}

TEST(logic_not) {
    setup_context();
    eval_ok("true not");
    ASSERT_EQ(stack_depth(), 1);
    JoyValue v = joy_stack_peek(ctx->stack);
    ASSERT_FALSE(v.data.boolean);
    teardown_context();
}

/* ============================================================================
 * List Operation Tests
 * ============================================================================ */

TEST(list_cons) {
    setup_context();
    eval_ok("1 [2 3] cons");
    ASSERT_EQ(stack_depth(), 1);
    JoyValue v = joy_stack_peek(ctx->stack);
    ASSERT_EQ(v.type, JOY_LIST);
    ASSERT_EQ(v.data.list->length, 3);
    teardown_context();
}

TEST(list_first) {
    setup_context();
    eval_ok("[1 2 3] first");
    ASSERT_EQ(stack_depth(), 1);
    ASSERT_EQ(stack_top_int(), 1);
    teardown_context();
}

TEST(list_rest) {
    setup_context();
    eval_ok("[1 2 3] rest");
    ASSERT_EQ(stack_depth(), 1);
    JoyValue v = joy_stack_peek(ctx->stack);
    ASSERT_EQ(v.type, JOY_LIST);
    ASSERT_EQ(v.data.list->length, 2);
    teardown_context();
}

TEST(list_size) {
    setup_context();
    eval_ok("[1 2 3 4 5] size");
    ASSERT_EQ(stack_depth(), 1);
    ASSERT_EQ(stack_top_int(), 5);
    teardown_context();
}

TEST(list_null_empty) {
    setup_context();
    eval_ok("[] null");
    ASSERT_EQ(stack_depth(), 1);
    JoyValue v = joy_stack_peek(ctx->stack);
    ASSERT_TRUE(v.data.boolean);
    teardown_context();
}

TEST(list_null_nonempty) {
    setup_context();
    eval_ok("[1] null");
    ASSERT_EQ(stack_depth(), 1);
    JoyValue v = joy_stack_peek(ctx->stack);
    ASSERT_FALSE(v.data.boolean);
    teardown_context();
}

/* ============================================================================
 * Combinator Tests
 * ============================================================================ */

TEST(combinator_i) {
    setup_context();
    eval_ok("[42] i");
    ASSERT_EQ(stack_depth(), 1);
    ASSERT_EQ(stack_top_int(), 42);
    teardown_context();
}

TEST(combinator_dip) {
    setup_context();
    eval_ok("1 2 [10 +] dip");
    ASSERT_EQ(stack_depth(), 2);
    /* After dip: bottom should be 11 (1+10), top should be 2 */
    ASSERT_EQ(stack_top_int(), 2);
    teardown_context();
}

TEST(combinator_ifte_true) {
    setup_context();
    /* ifte preserves the original value, so stack has: 5, "positive" */
    eval_ok("5 [0 >] [\"positive\"] [\"negative\"] ifte");
    ASSERT_EQ(stack_depth(), 2);
    JoyValue v = joy_stack_peek(ctx->stack);
    ASSERT_EQ(v.type, JOY_STRING);
    ASSERT_STR_EQ(v.data.string, "positive");
    teardown_context();
}

TEST(combinator_ifte_false) {
    setup_context();
    /* ifte preserves the original value, so stack has: -5, "negative" */
    eval_ok("-5 [0 >] [\"positive\"] [\"negative\"] ifte");
    ASSERT_EQ(stack_depth(), 2);
    JoyValue v = joy_stack_peek(ctx->stack);
    ASSERT_EQ(v.type, JOY_STRING);
    ASSERT_STR_EQ(v.data.string, "negative");
    teardown_context();
}

/* ============================================================================
 * Map/Filter Tests
 * ============================================================================ */

TEST(map_double) {
    setup_context();
    eval_ok("[1 2 3] [2 *] map");
    ASSERT_EQ(stack_depth(), 1);
    JoyValue v = joy_stack_peek(ctx->stack);
    ASSERT_EQ(v.type, JOY_LIST);
    ASSERT_EQ(v.data.list->length, 3);
    ASSERT_EQ(v.data.list->items[0].data.integer, 2);
    ASSERT_EQ(v.data.list->items[1].data.integer, 4);
    ASSERT_EQ(v.data.list->items[2].data.integer, 6);
    teardown_context();
}

TEST(filter_positive) {
    setup_context();
    eval_ok("[-2 -1 0 1 2] [0 >] filter");
    ASSERT_EQ(stack_depth(), 1);
    JoyValue v = joy_stack_peek(ctx->stack);
    ASSERT_EQ(v.type, JOY_LIST);
    ASSERT_EQ(v.data.list->length, 2);
    ASSERT_EQ(v.data.list->items[0].data.integer, 1);
    ASSERT_EQ(v.data.list->items[1].data.integer, 2);
    teardown_context();
}

/* ============================================================================
 * Fold Test
 * ============================================================================ */

TEST(fold_sum) {
    setup_context();
    /* fold takes: aggregate init quotation */
    eval_ok("[1 2 3 4 5] 0 [+] fold");
    ASSERT_EQ(stack_depth(), 1);
    ASSERT_EQ(stack_top_int(), 15);
    teardown_context();
}

/* ============================================================================
 * Times Test
 * ============================================================================ */

TEST(times_repeat) {
    setup_context();
    /* times takes: count quotation (pops quot first, then count) */
    eval_ok("1 4 [2 *] times");
    ASSERT_EQ(stack_depth(), 1);
    ASSERT_EQ(stack_top_int(), 16);  /* 1 * 2^4 = 16 */
    teardown_context();
}

/* ============================================================================
 * String Operation Tests
 * ============================================================================ */

TEST(string_concat) {
    setup_context();
    eval_ok("\"hello\" \" world\" concat");
    ASSERT_EQ(stack_depth(), 1);
    JoyValue v = joy_stack_peek(ctx->stack);
    ASSERT_EQ(v.type, JOY_STRING);
    ASSERT_STR_EQ(v.data.string, "hello world");
    teardown_context();
}

/* ============================================================================
 * Test Runner
 * ============================================================================ */


BEGIN_TEST_SUITE("Joy Primitives Tests")

    /* Stack operations */
    RUN_TEST(stack_push_integer);
    RUN_TEST(stack_push_multiple);
    RUN_TEST(stack_dup);
    RUN_TEST(stack_swap);
    RUN_TEST(stack_pop);
    RUN_TEST(stack_pop_all);

    /* Arithmetic */
    RUN_TEST(arithmetic_add);
    RUN_TEST(arithmetic_sub);
    RUN_TEST(arithmetic_mul);
    RUN_TEST(arithmetic_div);
    RUN_TEST(arithmetic_mod);
    RUN_TEST(arithmetic_neg);
    RUN_TEST(arithmetic_abs);

    /* Comparison */
    RUN_TEST(comparison_eq_true);
    RUN_TEST(comparison_eq_false);
    RUN_TEST(comparison_lt);
    RUN_TEST(comparison_gt);

    /* Boolean logic */
    RUN_TEST(logic_and_true);
    RUN_TEST(logic_and_false);
    RUN_TEST(logic_or_true);
    RUN_TEST(logic_not);

    /* List operations */
    RUN_TEST(list_cons);
    RUN_TEST(list_first);
    RUN_TEST(list_rest);
    RUN_TEST(list_size);
    RUN_TEST(list_null_empty);
    RUN_TEST(list_null_nonempty);

    /* Combinators */
    RUN_TEST(combinator_i);
    RUN_TEST(combinator_dip);
    RUN_TEST(combinator_ifte_true);
    RUN_TEST(combinator_ifte_false);

    /* Map/Filter */
    RUN_TEST(map_double);
    RUN_TEST(filter_positive);

    /* Fold */
    RUN_TEST(fold_sum);

    /* Times */
    RUN_TEST(times_repeat);

    /* Strings */
    RUN_TEST(string_concat);

END_TEST_SUITE()
