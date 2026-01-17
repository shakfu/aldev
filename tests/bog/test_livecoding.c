/*
 * Livecoding/Code Validator tests - converted from dogalog tests/livecoding/codeValidator.test.js
 */
#include "test_framework.h"

#include "bog.h"
#include "livecoding.h"

TEST(validator_validates_correct_prolog_syntax)
{
    const char* code = "event(kick, 36, 0.9, T) :- beat(T, 1).";
    BogProgram* program = NULL;
    BogArena* arena = NULL;
    char* error = NULL;

    bool valid = bog_validate_code(code, &program, &arena, &error);

    ASSERT(valid);
    ASSERT_NOT_NULL(program);
    ASSERT_EQ(program->count, 1);
    ASSERT_NULL(error);

    free(error);
    bog_arena_destroy(arena);
    TEST_PASS();
}

TEST(validator_returns_error_for_invalid_syntax)
{
    const char* code = "event(kick, 36, 0.9, T) :- beat(T, 1";
    BogProgram* program = NULL;
    BogArena* arena = NULL;
    char* error = NULL;

    bool valid = bog_validate_code(code, &program, &arena, &error);

    ASSERT(!valid);
    ASSERT_NULL(program);
    ASSERT_NOT_NULL(error);

    free(error);
    bog_arena_destroy(arena);
    TEST_PASS();
}

TEST(validator_handles_empty_code_as_valid)
{
    const char* code = "";
    BogProgram* program = NULL;
    BogArena* arena = NULL;
    char* error = NULL;

    bool valid = bog_validate_code(code, &program, &arena, &error);

    ASSERT(valid);
    ASSERT_NOT_NULL(program);
    ASSERT_EQ(program->count, 0);

    free(error);
    bog_arena_destroy(arena);
    TEST_PASS();
}

TEST(validator_handles_whitespace_only_code_as_valid)
{
    const char* code = "   \n  \t  ";
    BogProgram* program = NULL;
    BogArena* arena = NULL;
    char* error = NULL;

    bool valid = bog_validate_code(code, &program, &arena, &error);

    ASSERT(valid);
    ASSERT_NOT_NULL(program);
    ASSERT_EQ(program->count, 0);

    free(error);
    bog_arena_destroy(arena);
    TEST_PASS();
}

TEST(validator_validates_multiple_clauses)
{
    const char* code = "event(kick, 36, 0.9, T) :- beat(T, 1).\n"
                       "event(snare, 38, 0.8, T) :- beat(T, 2).";
    BogProgram* program = NULL;
    BogArena* arena = NULL;
    char* error = NULL;

    bool valid = bog_validate_code(code, &program, &arena, &error);

    ASSERT(valid);
    ASSERT_NOT_NULL(program);
    ASSERT_EQ(program->count, 2);

    free(error);
    bog_arena_destroy(arena);
    TEST_PASS();
}

TEST(validator_handles_partial_edits_during_typing)
{
    const char* code = "event(kick";
    BogProgram* program = NULL;
    BogArena* arena = NULL;
    char* error = NULL;

    bool valid = bog_validate_code(code, &program, &arena, &error);

    ASSERT(!valid);
    ASSERT_NOT_NULL(error);

    free(error);
    bog_arena_destroy(arena);
    TEST_PASS();
}

TEST(validator_handles_comments)
{
    const char* code = "% This is a comment\nevent(kick, 36, 0.9, T).";
    BogProgram* program = NULL;
    BogArena* arena = NULL;
    char* error = NULL;

    bool valid = bog_validate_code(code, &program, &arena, &error);

    ASSERT(valid);
    ASSERT_NOT_NULL(program);
    ASSERT_EQ(program->count, 1);

    free(error);
    bog_arena_destroy(arena);
    TEST_PASS();
}

int main(void)
{
    printf("Livecoding/Validator Tests\n");
    printf("==========================\n");

    RUN_TEST(validator_validates_correct_prolog_syntax);
    RUN_TEST(validator_returns_error_for_invalid_syntax);
    RUN_TEST(validator_handles_empty_code_as_valid);
    RUN_TEST(validator_handles_whitespace_only_code_as_valid);
    RUN_TEST(validator_validates_multiple_clauses);
    RUN_TEST(validator_handles_partial_edits_during_typing);
    RUN_TEST(validator_handles_comments);

    TEST_SUMMARY();
    return TEST_EXIT_CODE();
}
