/*
 * Parser tests - converted from dogalog tests/prolog/parser.test.js
 */
#include "test_framework.h"

#include "bog.h"

/* Basic Facts */

TEST(parse_simple_atom_fact)
{
    BogArena* arena = bog_arena_create();
    char* error = NULL;
    BogProgram* program = bog_parse_program("kik(0.5).", arena, &error);

    ASSERT_NOT_NULL(program);
    ASSERT_NULL(error);
    ASSERT_EQ(program->count, 1);
    ASSERT_EQ(program->clauses[0].head->type, CPROLOG_TERM_COMPOUND);
    ASSERT_STREQ(program->clauses[0].head->value.compound.functor, "kik");
    ASSERT_EQ(program->clauses[0].head->value.compound.arity, 1);
    ASSERT_EQ(program->clauses[0].head->value.compound.args[0]->type,
              CPROLOG_TERM_NUM);
    ASSERT_NEAR(program->clauses[0].head->value.compound.args[0]->value.number,
                0.5, 1e-9);

    bog_arena_destroy(arena);
    TEST_PASS();
}

TEST(parse_fact_with_multiple_arguments)
{
    BogArena* arena = bog_arena_create();
    char* error = NULL;
    BogProgram* program = bog_parse_program("event(kick, 36, 0.95, 0).", arena,
                                            &error);

    ASSERT_NOT_NULL(program);
    ASSERT_EQ(program->count, 1);
    ASSERT_STREQ(program->clauses[0].head->value.compound.functor, "event");
    ASSERT_EQ(program->clauses[0].head->value.compound.arity, 4);
    ASSERT_EQ(program->clauses[0].head->value.compound.args[0]->type,
              CPROLOG_TERM_ATOM);
    ASSERT_STREQ(program->clauses[0].head->value.compound.args[0]->value.atom,
                 "kick");

    bog_arena_destroy(arena);
    free(error);
    TEST_PASS();
}

TEST(parse_fact_with_variables)
{
    BogArena* arena = bog_arena_create();
    char* error = NULL;
    BogProgram* program = bog_parse_program("bass(T, N).", arena, &error);

    ASSERT_NOT_NULL(program);
    ASSERT_EQ(program->count, 1);
    ASSERT_EQ(program->clauses[0].head->value.compound.arity, 2);
    ASSERT_EQ(program->clauses[0].head->value.compound.args[0]->type,
              CPROLOG_TERM_VAR);
    ASSERT_STREQ(program->clauses[0].head->value.compound.args[0]->value.atom,
                 "T");
    ASSERT_EQ(program->clauses[0].head->value.compound.args[1]->type,
              CPROLOG_TERM_VAR);
    ASSERT_STREQ(program->clauses[0].head->value.compound.args[1]->value.atom,
                 "N");

    bog_arena_destroy(arena);
    free(error);
    TEST_PASS();
}

/* REPL Query Patterns */

TEST(parse_query_with_numbers)
{
    BogArena* arena = bog_arena_create();
    char* error = NULL;
    BogProgram* program = bog_parse_program("euc(0.5, 4, 16, 4, 0).", arena,
                                            &error);

    ASSERT_NOT_NULL(program);
    ASSERT_EQ(program->count, 1);
    ASSERT_STREQ(program->clauses[0].head->value.compound.functor, "euc");
    ASSERT_EQ(program->clauses[0].head->value.compound.arity, 5);

    bog_arena_destroy(arena);
    free(error);
    TEST_PASS();
}

TEST(parse_query_with_mixed_variables_and_atoms)
{
    BogArena* arena = bog_arena_create();
    char* error = NULL;
    BogProgram* program = bog_parse_program("scale(60, ionian, 5, 0, N).",
                                            arena, &error);

    ASSERT_NOT_NULL(program);
    BogTerm** args = program->clauses[0].head->value.compound.args;
    ASSERT_EQ(args[0]->type, CPROLOG_TERM_NUM);
    ASSERT_EQ(args[1]->type, CPROLOG_TERM_ATOM);
    ASSERT_STREQ(args[1]->value.atom, "ionian");
    ASSERT_EQ(args[4]->type, CPROLOG_TERM_VAR);
    ASSERT_STREQ(args[4]->value.atom, "N");

    bog_arena_destroy(arena);
    free(error);
    TEST_PASS();
}

TEST(parse_query_with_underscore_variable)
{
    BogArena* arena = bog_arena_create();
    char* error = NULL;
    BogProgram* program = bog_parse_program("event(Voice, _, _, 0).", arena,
                                            &error);

    ASSERT_NOT_NULL(program);
    BogTerm** args = program->clauses[0].head->value.compound.args;
    ASSERT_EQ(args[0]->type, CPROLOG_TERM_VAR);
    ASSERT_EQ(args[1]->type, CPROLOG_TERM_VAR);
    ASSERT_STREQ(args[1]->value.atom, "_");

    bog_arena_destroy(arena);
    free(error);
    TEST_PASS();
}

TEST(parse_query_with_list)
{
    BogArena* arena = bog_arena_create();
    char* error = NULL;
    BogProgram* program = bog_parse_program("choose([60, 64, 67], N).", arena,
                                            &error);

    ASSERT_NOT_NULL(program);
    BogTerm** args = program->clauses[0].head->value.compound.args;
    ASSERT_EQ(args[0]->type, CPROLOG_TERM_LIST);
    ASSERT_EQ(args[0]->value.list.length, 3);

    bog_arena_destroy(arena);
    free(error);
    TEST_PASS();
}

/* Clauses with Bodies */

TEST(parse_simple_rule)
{
    BogArena* arena = bog_arena_create();
    char* error = NULL;
    BogProgram* program = bog_parse_program("kik(T) :- euc(T, 4, 16, 4, 0).",
                                            arena, &error);

    ASSERT_NOT_NULL(program);
    ASSERT_EQ(program->count, 1);
    ASSERT_STREQ(program->clauses[0].head->value.compound.functor, "kik");
    ASSERT_EQ(program->clauses[0].body.count, 1);
    ASSERT_EQ(program->clauses[0].body.items[0].kind, CPROLOG_GOAL_TERM);
    ASSERT_STREQ(
        program->clauses[0].body.items[0].data.term->value.compound.functor,
        "euc");

    bog_arena_destroy(arena);
    free(error);
    TEST_PASS();
}

TEST(parse_rule_with_multiple_body_goals)
{
    BogArena* arena = bog_arena_create();
    char* error = NULL;
    BogProgram* program = bog_parse_program("test(X) :- foo(X), bar(X).", arena,
                                            &error);

    ASSERT_NOT_NULL(program);
    ASSERT_EQ(program->count, 1);
    /* Body has 2 goals */
    ASSERT_EQ(program->clauses[0].body.count, 2);
    ASSERT_STREQ(
        program->clauses[0].body.items[0].data.term->value.compound.functor,
        "foo");
    ASSERT_STREQ(
        program->clauses[0].body.items[1].data.term->value.compound.functor,
        "bar");

    bog_arena_destroy(arena);
    free(error);
    TEST_PASS();
}

TEST(parse_infix_is_operator)
{
    BogArena* arena = bog_arena_create();
    char* error = NULL;
    BogProgram* program = bog_parse_program("test(X) :- X is 2 + 3.", arena,
                                            &error);

    ASSERT_NOT_NULL(program);
    ASSERT_NULL(error);
    ASSERT_EQ(program->count, 1);
    ASSERT_EQ(program->clauses[0].body.count, 1);

    /* Goal should be is(X, 2+3) as a compound term */
    BogTerm* goal_term = program->clauses[0].body.items[0].data.term;
    ASSERT_EQ(goal_term->type, CPROLOG_TERM_COMPOUND);
    ASSERT_STREQ(goal_term->value.compound.functor, "is");
    ASSERT_EQ(goal_term->value.compound.arity, 2);

    /* First arg is variable X */
    ASSERT_EQ(goal_term->value.compound.args[0]->type, CPROLOG_TERM_VAR);
    ASSERT_STREQ(goal_term->value.compound.args[0]->value.atom, "X");

    /* Second arg is expression 2 + 3 */
    ASSERT_EQ(goal_term->value.compound.args[1]->type, CPROLOG_TERM_EXPR);
    ASSERT_EQ(goal_term->value.compound.args[1]->value.expr.op, '+');

    bog_arena_destroy(arena);
    free(error);
    TEST_PASS();
}

/* Edge Cases */

TEST(parse_multiple_clauses)
{
    BogArena* arena = bog_arena_create();
    char* error = NULL;
    BogProgram* program = bog_parse_program("foo(1). bar(2).", arena, &error);

    ASSERT_NOT_NULL(program);
    ASSERT_EQ(program->count, 2);
    ASSERT_STREQ(program->clauses[0].head->value.compound.functor, "foo");
    ASSERT_STREQ(program->clauses[1].head->value.compound.functor, "bar");

    bog_arena_destroy(arena);
    free(error);
    TEST_PASS();
}

TEST(parse_handles_comments)
{
    BogArena* arena = bog_arena_create();
    char* error = NULL;
    BogProgram* program = bog_parse_program("% This is a comment\nkik(0.5).",
                                            arena, &error);

    ASSERT_NOT_NULL(program);
    ASSERT_EQ(program->count, 1);
    ASSERT_STREQ(program->clauses[0].head->value.compound.functor, "kik");

    bog_arena_destroy(arena);
    free(error);
    TEST_PASS();
}

TEST(parse_empty_list)
{
    BogArena* arena = bog_arena_create();
    char* error = NULL;
    BogProgram* program = bog_parse_program("test([]).", arena, &error);

    ASSERT_NOT_NULL(program);
    BogTerm* arg = program->clauses[0].head->value.compound.args[0];
    ASSERT_EQ(arg->type, CPROLOG_TERM_LIST);
    ASSERT_EQ(arg->value.list.length, 0);

    bog_arena_destroy(arena);
    free(error);
    TEST_PASS();
}

TEST(parse_nested_compounds)
{
    BogArena* arena = bog_arena_create();
    char* error = NULL;
    BogProgram* program = bog_parse_program("test(foo(bar(1))).", arena,
                                            &error);

    ASSERT_NOT_NULL(program);
    BogTerm* arg = program->clauses[0].head->value.compound.args[0];
    ASSERT_EQ(arg->type, CPROLOG_TERM_COMPOUND);
    ASSERT_STREQ(arg->value.compound.functor, "foo");
    ASSERT_EQ(arg->value.compound.args[0]->type, CPROLOG_TERM_COMPOUND);
    ASSERT_STREQ(arg->value.compound.args[0]->value.compound.functor, "bar");

    bog_arena_destroy(arena);
    free(error);
    TEST_PASS();
}

/* Error Cases */

TEST(parse_error_missing_period)
{
    BogArena* arena = bog_arena_create();
    char* error = NULL;
    BogProgram* program = bog_parse_program("kik(0.5)", arena, &error);

    ASSERT_NULL(program);
    ASSERT_NOT_NULL(error);

    bog_arena_destroy(arena);
    free(error);
    TEST_PASS();
}

TEST(parse_error_unbalanced_parentheses)
{
    BogArena* arena = bog_arena_create();
    char* error = NULL;
    BogProgram* program = bog_parse_program("kik(0.5.", arena, &error);

    ASSERT_NULL(program);
    ASSERT_NOT_NULL(error);

    bog_arena_destroy(arena);
    free(error);
    TEST_PASS();
}

int main(void)
{
    printf("Parser Tests\n");
    printf("============\n");

    /* Basic Facts */
    RUN_TEST(parse_simple_atom_fact);
    RUN_TEST(parse_fact_with_multiple_arguments);
    RUN_TEST(parse_fact_with_variables);

    /* REPL Query Patterns */
    RUN_TEST(parse_query_with_numbers);
    RUN_TEST(parse_query_with_mixed_variables_and_atoms);
    RUN_TEST(parse_query_with_underscore_variable);
    RUN_TEST(parse_query_with_list);

    /* Clauses with Bodies */
    RUN_TEST(parse_simple_rule);
    RUN_TEST(parse_rule_with_multiple_body_goals);
    RUN_TEST(parse_infix_is_operator);

    /* Edge Cases */
    RUN_TEST(parse_multiple_clauses);
    RUN_TEST(parse_handles_comments);
    RUN_TEST(parse_empty_list);
    RUN_TEST(parse_nested_compounds);

    /* Error Cases */
    RUN_TEST(parse_error_missing_period);
    RUN_TEST(parse_error_unbalanced_parentheses);

    TEST_SUMMARY();
    return TEST_EXIT_CODE();
}
