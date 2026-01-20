/**
 * @file test_parser.c
 * @brief Unit tests for Joy parser.
 *
 * Tests parsing for Joy tokens and quotations.
 */

#include "test_framework.h"
#include "joy_runtime.h"
#include "joy_parser.h"
#include <string.h>
#include <math.h>

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/* Parse source and return quotation, asserting it's not NULL */
static JoyQuotation* parse_ok(const char* source) {
    JoyQuotation* quot = joy_parse(source);
    return quot;
}

/* ============================================================================
 * Integer Parsing Tests
 * ============================================================================ */

TEST(parse_integer_positive) {
    JoyQuotation* quot = parse_ok("42");
    ASSERT_NOT_NULL(quot);
    ASSERT_EQ(quot->length, 1);
    ASSERT_EQ(quot->terms[0].type, JOY_INTEGER);
    ASSERT_EQ(quot->terms[0].data.integer, 42);
    joy_quotation_free(quot);
}

TEST(parse_integer_negative) {
    JoyQuotation* quot = parse_ok("-17");
    ASSERT_NOT_NULL(quot);
    ASSERT_EQ(quot->length, 1);
    ASSERT_EQ(quot->terms[0].type, JOY_INTEGER);
    ASSERT_EQ(quot->terms[0].data.integer, -17);
    joy_quotation_free(quot);
}

TEST(parse_integer_zero) {
    JoyQuotation* quot = parse_ok("0");
    ASSERT_NOT_NULL(quot);
    ASSERT_EQ(quot->length, 1);
    ASSERT_EQ(quot->terms[0].type, JOY_INTEGER);
    ASSERT_EQ(quot->terms[0].data.integer, 0);
    joy_quotation_free(quot);
}

/* ============================================================================
 * Float Parsing Tests
 * ============================================================================ */

TEST(parse_float_simple) {
    JoyQuotation* quot = parse_ok("3.14");
    ASSERT_NOT_NULL(quot);
    ASSERT_EQ(quot->length, 1);
    ASSERT_EQ(quot->terms[0].type, JOY_FLOAT);
    ASSERT_TRUE(fabs(quot->terms[0].data.floating - 3.14) < 0.001);
    joy_quotation_free(quot);
}

TEST(parse_float_negative) {
    JoyQuotation* quot = parse_ok("-2.5");
    ASSERT_NOT_NULL(quot);
    ASSERT_EQ(quot->length, 1);
    ASSERT_EQ(quot->terms[0].type, JOY_FLOAT);
    ASSERT_TRUE(fabs(quot->terms[0].data.floating - (-2.5)) < 0.001);
    joy_quotation_free(quot);
}

/* ============================================================================
 * String Parsing Tests
 * ============================================================================ */

TEST(parse_string_simple) {
    JoyQuotation* quot = parse_ok("\"hello\"");
    ASSERT_NOT_NULL(quot);
    ASSERT_EQ(quot->length, 1);
    ASSERT_EQ(quot->terms[0].type, JOY_STRING);
    ASSERT_STR_EQ(quot->terms[0].data.string, "hello");
    joy_quotation_free(quot);
}

TEST(parse_string_empty) {
    JoyQuotation* quot = parse_ok("\"\"");
    ASSERT_NOT_NULL(quot);
    ASSERT_EQ(quot->length, 1);
    ASSERT_EQ(quot->terms[0].type, JOY_STRING);
    ASSERT_STR_EQ(quot->terms[0].data.string, "");
    joy_quotation_free(quot);
}

TEST(parse_string_with_spaces) {
    JoyQuotation* quot = parse_ok("\"hello world\"");
    ASSERT_NOT_NULL(quot);
    ASSERT_EQ(quot->length, 1);
    ASSERT_EQ(quot->terms[0].type, JOY_STRING);
    ASSERT_STR_EQ(quot->terms[0].data.string, "hello world");
    joy_quotation_free(quot);
}

/* ============================================================================
 * Boolean Parsing Tests
 * ============================================================================ */

TEST(parse_boolean_true) {
    JoyQuotation* quot = parse_ok("true");
    ASSERT_NOT_NULL(quot);
    ASSERT_EQ(quot->length, 1);
    ASSERT_EQ(quot->terms[0].type, JOY_BOOLEAN);
    ASSERT_TRUE(quot->terms[0].data.boolean);
    joy_quotation_free(quot);
}

TEST(parse_boolean_false) {
    JoyQuotation* quot = parse_ok("false");
    ASSERT_NOT_NULL(quot);
    ASSERT_EQ(quot->length, 1);
    ASSERT_EQ(quot->terms[0].type, JOY_BOOLEAN);
    ASSERT_FALSE(quot->terms[0].data.boolean);
    joy_quotation_free(quot);
}

/* ============================================================================
 * Symbol Parsing Tests
 * ============================================================================ */

TEST(parse_symbol_simple) {
    JoyQuotation* quot = parse_ok("dup");
    ASSERT_NOT_NULL(quot);
    ASSERT_EQ(quot->length, 1);
    ASSERT_EQ(quot->terms[0].type, JOY_SYMBOL);
    ASSERT_STR_EQ(quot->terms[0].data.symbol, "dup");
    joy_quotation_free(quot);
}

TEST(parse_symbol_with_hyphen) {
    JoyQuotation* quot = parse_ok("note-on");
    ASSERT_NOT_NULL(quot);
    ASSERT_EQ(quot->length, 1);
    ASSERT_EQ(quot->terms[0].type, JOY_SYMBOL);
    ASSERT_STR_EQ(quot->terms[0].data.symbol, "note-on");
    joy_quotation_free(quot);
}

/* ============================================================================
 * List Syntax Parsing Tests (Joy uses [] for lists)
 * ============================================================================ */

TEST(parse_bracket_list_empty) {
    JoyQuotation* quot = parse_ok("[]");
    ASSERT_NOT_NULL(quot);
    ASSERT_EQ(quot->length, 1);
    ASSERT_EQ(quot->terms[0].type, JOY_LIST);
    ASSERT_EQ(quot->terms[0].data.list->length, 0);
    joy_quotation_free(quot);
}

TEST(parse_bracket_list_single) {
    JoyQuotation* quot = parse_ok("[42]");
    ASSERT_NOT_NULL(quot);
    ASSERT_EQ(quot->length, 1);
    ASSERT_EQ(quot->terms[0].type, JOY_LIST);
    ASSERT_EQ(quot->terms[0].data.list->length, 1);
    ASSERT_EQ(quot->terms[0].data.list->items[0].type, JOY_INTEGER);
    ASSERT_EQ(quot->terms[0].data.list->items[0].data.integer, 42);
    joy_quotation_free(quot);
}

TEST(parse_bracket_list_multiple) {
    JoyQuotation* quot = parse_ok("[1 2 3]");
    ASSERT_NOT_NULL(quot);
    ASSERT_EQ(quot->length, 1);
    ASSERT_EQ(quot->terms[0].type, JOY_LIST);
    ASSERT_EQ(quot->terms[0].data.list->length, 3);
    joy_quotation_free(quot);
}

TEST(parse_bracket_list_nested) {
    JoyQuotation* quot = parse_ok("[[1 2] [3 4]]");
    ASSERT_NOT_NULL(quot);
    ASSERT_EQ(quot->length, 1);
    ASSERT_EQ(quot->terms[0].type, JOY_LIST);
    ASSERT_EQ(quot->terms[0].data.list->length, 2);
    /* First inner list */
    JoyValue first = quot->terms[0].data.list->items[0];
    ASSERT_EQ(first.type, JOY_LIST);
    ASSERT_EQ(first.data.list->length, 2);
    joy_quotation_free(quot);
}

/* ============================================================================
 * Set Parsing Tests (Joy uses {} for sets with integer members 0-63)
 * ============================================================================ */

TEST(parse_set_empty) {
    JoyQuotation* quot = parse_ok("{}");
    ASSERT_NOT_NULL(quot);
    ASSERT_EQ(quot->length, 1);
    ASSERT_EQ(quot->terms[0].type, JOY_SET);
    ASSERT_EQ(quot->terms[0].data.set, 0);  /* Empty set = 0 */
    joy_quotation_free(quot);
}

TEST(parse_set_integers) {
    JoyQuotation* quot = parse_ok("{1 2 3}");
    ASSERT_NOT_NULL(quot);
    ASSERT_EQ(quot->length, 1);
    ASSERT_EQ(quot->terms[0].type, JOY_SET);
    /* Set with 1, 2, 3 should have bits 1, 2, 3 set = 0b1110 = 14 */
    ASSERT_EQ(quot->terms[0].data.set, 14);
    joy_quotation_free(quot);
}

/* ============================================================================
 * Multiple Terms Tests
 * ============================================================================ */

TEST(parse_multiple_integers) {
    JoyQuotation* quot = parse_ok("1 2 3");
    ASSERT_NOT_NULL(quot);
    ASSERT_EQ(quot->length, 3);
    ASSERT_EQ(quot->terms[0].data.integer, 1);
    ASSERT_EQ(quot->terms[1].data.integer, 2);
    ASSERT_EQ(quot->terms[2].data.integer, 3);
    joy_quotation_free(quot);
}

TEST(parse_mixed_types) {
    JoyQuotation* quot = parse_ok("42 \"hello\" true");
    ASSERT_NOT_NULL(quot);
    ASSERT_EQ(quot->length, 3);
    ASSERT_EQ(quot->terms[0].type, JOY_INTEGER);
    ASSERT_EQ(quot->terms[1].type, JOY_STRING);
    ASSERT_EQ(quot->terms[2].type, JOY_BOOLEAN);
    joy_quotation_free(quot);
}

TEST(parse_expression_with_symbols) {
    JoyQuotation* quot = parse_ok("1 2 add");
    ASSERT_NOT_NULL(quot);
    ASSERT_EQ(quot->length, 3);
    ASSERT_EQ(quot->terms[0].type, JOY_INTEGER);
    ASSERT_EQ(quot->terms[1].type, JOY_INTEGER);
    ASSERT_EQ(quot->terms[2].type, JOY_SYMBOL);
    ASSERT_STR_EQ(quot->terms[2].data.symbol, "add");
    joy_quotation_free(quot);
}

/* ============================================================================
 * Comment Tests (Joy uses \ for line comments, (* *) for block comments)
 * ============================================================================ */

TEST(parse_line_comment) {
    JoyQuotation* quot = parse_ok("42 \\ this is a comment");
    ASSERT_NOT_NULL(quot);
    ASSERT_EQ(quot->length, 1);
    ASSERT_EQ(quot->terms[0].type, JOY_INTEGER);
    ASSERT_EQ(quot->terms[0].data.integer, 42);
    joy_quotation_free(quot);
}

TEST(parse_block_comment) {
    JoyQuotation* quot = parse_ok("42 (* this is a block comment *) 17");
    ASSERT_NOT_NULL(quot);
    ASSERT_EQ(quot->length, 2);
    ASSERT_EQ(quot->terms[0].data.integer, 42);
    ASSERT_EQ(quot->terms[1].data.integer, 17);
    joy_quotation_free(quot);
}

/* ============================================================================
 * Empty Input Tests
 * ============================================================================ */

TEST(parse_empty_string) {
    JoyQuotation* quot = parse_ok("");
    ASSERT_NOT_NULL(quot);
    ASSERT_EQ(quot->length, 0);
    joy_quotation_free(quot);
}

TEST(parse_whitespace_only) {
    JoyQuotation* quot = parse_ok("   \t  ");
    ASSERT_NOT_NULL(quot);
    ASSERT_EQ(quot->length, 0);
    joy_quotation_free(quot);
}

/* ============================================================================
 * Character Parsing Tests
 * ============================================================================ */

TEST(parse_char_simple) {
    JoyQuotation* quot = parse_ok("'a");
    ASSERT_NOT_NULL(quot);
    ASSERT_EQ(quot->length, 1);
    ASSERT_EQ(quot->terms[0].type, JOY_CHAR);
    ASSERT_EQ(quot->terms[0].data.character, 'a');
    joy_quotation_free(quot);
}

/* ============================================================================
 * Test Runner
 * ============================================================================ */


BEGIN_TEST_SUITE("Joy Parser Tests")

    /* Integer parsing */
    RUN_TEST(parse_integer_positive);
    RUN_TEST(parse_integer_negative);
    RUN_TEST(parse_integer_zero);

    /* Float parsing */
    RUN_TEST(parse_float_simple);
    RUN_TEST(parse_float_negative);

    /* String parsing */
    RUN_TEST(parse_string_simple);
    RUN_TEST(parse_string_empty);
    RUN_TEST(parse_string_with_spaces);

    /* Boolean parsing */
    RUN_TEST(parse_boolean_true);
    RUN_TEST(parse_boolean_false);

    /* Symbol parsing */
    RUN_TEST(parse_symbol_simple);
    RUN_TEST(parse_symbol_with_hyphen);

    /* List syntax parsing ([] creates lists in this Joy) */
    RUN_TEST(parse_bracket_list_empty);
    RUN_TEST(parse_bracket_list_single);
    RUN_TEST(parse_bracket_list_multiple);
    RUN_TEST(parse_bracket_list_nested);

    /* Set parsing ({} creates sets) */
    RUN_TEST(parse_set_empty);
    RUN_TEST(parse_set_integers);

    /* Multiple terms */
    RUN_TEST(parse_multiple_integers);
    RUN_TEST(parse_mixed_types);
    RUN_TEST(parse_expression_with_symbols);

    /* Comments */
    RUN_TEST(parse_line_comment);
    RUN_TEST(parse_block_comment);

    /* Empty input */
    RUN_TEST(parse_empty_string);
    RUN_TEST(parse_whitespace_only);

    /* Characters */
    RUN_TEST(parse_char_simple);

END_TEST_SUITE()
