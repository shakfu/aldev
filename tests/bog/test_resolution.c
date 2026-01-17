/*
 * Resolution/Engine tests - converted from dogalog tests/prolog/engine.test.js
 */
#include "test_framework.h"

#include "bog.h"
#include "builtins.h"

TEST(resolution_resolves_builtin_choose)
{
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    char* error = NULL;

    /* event(sine, N, 0.8, T) :- every(T, 1), choose([40,43], N). */
    BogProgram* program = bog_parse_program(
        "event(sine, N, 0.8, T) :- every(T, 1), choose([40,43], N).", arena,
        &error);

    ASSERT_NOT_NULL(program);
    ASSERT_NULL(error);

    /* Query: event(Voice, Pitch, Vel, 0) */
    BogTerm* query_args[4];
    query_args[0] = bog_make_var(arena, "Voice");
    query_args[1] = bog_make_var(arena, "Pitch");
    query_args[2] = bog_make_var(arena, "Vel");
    query_args[3] = bog_make_num(arena, 0);
    BogTerm* query = bog_make_compound(arena, "event", query_args, 4);

    /* Create goal list */
    BogGoal goal;
    goal.kind = CPROLOG_GOAL_TERM;
    goal.data.term = query;

    BogGoalList goals;
    goals.items = &goal;
    goals.count = 1;

    BogContext ctx;
    ctx.bpm = 120.0;
    ctx.state_manager = NULL;

    BogEnv env;
    bog_env_init(&env);
    BogSolutions solutions = { 0 };

    bog_resolve(&goals, &env, program, &ctx, builtins, &solutions, arena);

    /* Should get 2 solutions (40 and 43) */
    ASSERT_EQ(solutions.count, 2);

    /* Extract pitches */
    double pitches[2];
    for (size_t i = 0; i < 2; i++) {
        BogTerm* pitch = bog_subst_term(bog_make_var(arena, "Pitch"),
                                        &solutions.envs[i], arena);
        ASSERT_NOT_NULL(pitch);
        ASSERT_EQ(pitch->type, CPROLOG_TERM_NUM);
        pitches[i] = pitch->value.number;
    }

    ASSERT_NEAR(pitches[0], 40.0, 1e-9);
    ASSERT_NEAR(pitches[1], 43.0, 1e-9);

    for (size_t i = 0; i < solutions.count; i++)
        bog_env_free(&solutions.envs[i]);
    free(solutions.envs);
    bog_env_free(&env);
    free(error);
    bog_arena_destroy(arena);
    TEST_PASS();
}

TEST(resolution_supports_euclidean_rhythm_gating)
{
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    char* error = NULL;

    /* event(kick, 36, 1.0, T) :- euc(T, 4, 16, 4, 0). */
    BogProgram* program = bog_parse_program(
        "event(kick, 36, 1.0, T) :- euc(T, 4, 16, 4, 0).", arena, &error);

    ASSERT_NOT_NULL(program);

    /* Query: event(Voice, Pitch, Vel, 0) - at time 0, kick should fire */
    BogTerm* query_args[4];
    query_args[0] = bog_make_var(arena, "Voice");
    query_args[1] = bog_make_var(arena, "Pitch");
    query_args[2] = bog_make_var(arena, "Vel");
    query_args[3] = bog_make_num(arena, 0);
    BogTerm* query = bog_make_compound(arena, "event", query_args, 4);

    BogGoal goal;
    goal.kind = CPROLOG_GOAL_TERM;
    goal.data.term = query;

    BogGoalList goals;
    goals.items = &goal;
    goals.count = 1;

    BogContext ctx;
    ctx.bpm = 120.0;
    ctx.state_manager = NULL;

    BogEnv env;
    bog_env_init(&env);
    BogSolutions solutions = { 0 };

    bog_resolve(&goals, &env, program, &ctx, builtins, &solutions, arena);

    ASSERT_EQ(solutions.count, 1);

    for (size_t i = 0; i < solutions.count; i++)
        bog_env_free(&solutions.envs[i]);
    free(solutions.envs);
    bog_env_free(&env);
    free(error);
    bog_arena_destroy(arena);
    TEST_PASS();
}

TEST(resolution_handles_multiple_clauses)
{
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    char* error = NULL;

    /* Multiple clauses for same predicate */
    BogProgram* program = bog_parse_program("foo(1). foo(2). foo(3).", arena,
                                            &error);

    ASSERT_NOT_NULL(program);
    ASSERT_EQ(program->count, 3);

    /* Query: foo(X) */
    BogTerm* query_args[1];
    query_args[0] = bog_make_var(arena, "X");
    BogTerm* query = bog_make_compound(arena, "foo", query_args, 1);

    BogGoal goal;
    goal.kind = CPROLOG_GOAL_TERM;
    goal.data.term = query;

    BogGoalList goals;
    goals.items = &goal;
    goals.count = 1;

    BogContext ctx;
    ctx.bpm = 120.0;
    ctx.state_manager = NULL;

    BogEnv env;
    bog_env_init(&env);
    BogSolutions solutions = { 0 };

    bog_resolve(&goals, &env, program, &ctx, builtins, &solutions, arena);

    /* Should get 3 solutions */
    ASSERT_EQ(solutions.count, 3);

    for (size_t i = 0; i < solutions.count; i++)
        bog_env_free(&solutions.envs[i]);
    free(solutions.envs);
    bog_env_free(&env);
    free(error);
    bog_arena_destroy(arena);
    TEST_PASS();
}

TEST(resolution_handles_conjunctive_goals)
{
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    char* error = NULL;

    /* test(X) :- foo(X), bar(X). with foo(1). foo(2). bar(2). */
    BogProgram* program = bog_parse_program(
        "foo(1). foo(2). bar(2). test(X) :- foo(X), bar(X).", arena, &error);

    ASSERT_NOT_NULL(program);

    /* Query: test(X) - should only match X=2 */
    BogTerm* query_args[1];
    query_args[0] = bog_make_var(arena, "X");
    BogTerm* query = bog_make_compound(arena, "test", query_args, 1);

    BogGoal goal;
    goal.kind = CPROLOG_GOAL_TERM;
    goal.data.term = query;

    BogGoalList goals;
    goals.items = &goal;
    goals.count = 1;

    BogContext ctx;
    ctx.bpm = 120.0;
    ctx.state_manager = NULL;

    BogEnv env;
    bog_env_init(&env);
    BogSolutions solutions = { 0 };

    bog_resolve(&goals, &env, program, &ctx, builtins, &solutions, arena);

    /* Should get 1 solution: X=2 */
    ASSERT_EQ(solutions.count, 1);

    BogTerm* x_val = bog_subst_term(bog_make_var(arena, "X"),
                                    &solutions.envs[0], arena);
    ASSERT_NEAR(x_val->value.number, 2.0, 1e-9);

    for (size_t i = 0; i < solutions.count; i++)
        bog_env_free(&solutions.envs[i]);
    free(solutions.envs);
    bog_env_free(&env);
    free(error);
    bog_arena_destroy(arena);
    TEST_PASS();
}

TEST(resolution_handles_is_builtin)
{
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    char* error = NULL;

    /* test(X) :- X is 2 + 3. - using infix notation */
    BogProgram* program = bog_parse_program("test(X) :- X is 2 + 3.", arena,
                                            &error);

    ASSERT_NOT_NULL(program);

    /* Query: test(X) */
    BogTerm* query_args[1];
    query_args[0] = bog_make_var(arena, "X");
    BogTerm* query = bog_make_compound(arena, "test", query_args, 1);

    BogGoal goal;
    goal.kind = CPROLOG_GOAL_TERM;
    goal.data.term = query;

    BogGoalList goals;
    goals.items = &goal;
    goals.count = 1;

    BogContext ctx;
    ctx.bpm = 120.0;
    ctx.state_manager = NULL;

    BogEnv env;
    bog_env_init(&env);
    BogSolutions solutions = { 0 };

    bog_resolve(&goals, &env, program, &ctx, builtins, &solutions, arena);

    ASSERT_EQ(solutions.count, 1);

    BogTerm* x_val = bog_subst_term(bog_make_var(arena, "X"),
                                    &solutions.envs[0], arena);
    ASSERT_NEAR(x_val->value.number, 5.0, 1e-9);

    for (size_t i = 0; i < solutions.count; i++)
        bog_env_free(&solutions.envs[i]);
    free(solutions.envs);
    bog_env_free(&env);
    free(error);
    bog_arena_destroy(arena);
    TEST_PASS();
}

int main(void)
{
    printf("Resolution Tests\n");
    printf("================\n");

    RUN_TEST(resolution_resolves_builtin_choose);
    RUN_TEST(resolution_supports_euclidean_rhythm_gating);
    RUN_TEST(resolution_handles_multiple_clauses);
    RUN_TEST(resolution_handles_conjunctive_goals);
    RUN_TEST(resolution_handles_is_builtin);

    TEST_SUMMARY();
    return TEST_EXIT_CODE();
}
