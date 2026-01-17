/*
 * Builtins tests - converted from dogalog tests/prolog/builtins.test.js
 */
#include "test_framework.h"

#include "bog.h"
#include "builtins.h"
#include "scheduler.h"

/* Helper to create a simple context */
static BogContext make_context(BogStateManager* sm)
{
    BogContext ctx;
    ctx.bpm = 120.0;
    ctx.state_manager = sm;
    return ctx;
}

/* Helper to invoke a builtin by name */
static bool invoke_builtin(const BogBuiltins* builtins, const char* name,
                           BogTerm** args, size_t arity, BogEnv* env,
                           const BogContext* ctx, BogBuiltinResult* result,
                           BogArena* arena)
{
    const BogBuiltin* builtin = bog_find_builtin(builtins, name);
    if (!builtin)
        return false;
    return builtin->fn(args, arity, env, ctx, result, arena);
}

/* scale builtin */

TEST(builtin_scale_maps_degree_to_midi_ionian)
{
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    BogContext ctx = make_context(NULL);
    BogEnv env;
    bog_env_init(&env);

    /* scale(60, ionian, 3, 0, N) -> N = 64 (E4) */
    BogTerm* args[5];
    args[0] = bog_make_num(arena, 60);        /* C4 */
    args[1] = bog_make_atom(arena, "ionian"); /* Major scale */
    args[2] = bog_make_num(arena, 3);         /* 3rd degree */
    args[3] = bog_make_num(arena, 0);         /* Octave offset */
    args[4] = bog_make_var(arena, "N");

    BogBuiltinResult result = { 0 };
    invoke_builtin(builtins, "scale", args, 5, &env, &ctx, &result, arena);

    ASSERT_EQ(result.count, 1);
    BogTerm* n_val = bog_env_get(&result.envs[0], "N");
    ASSERT_NOT_NULL(n_val);
    ASSERT_EQ(n_val->type, CPROLOG_TERM_NUM);
    ASSERT_NEAR(n_val->value.number, 64.0, 1e-9); /* E4 */

    for (size_t i = 0; i < result.count; i++)
        bog_env_free(&result.envs[i]);
    free(result.envs);
    bog_env_free(&env);
    bog_arena_destroy(arena);
    TEST_PASS();
}

TEST(builtin_scale_wraps_degrees_into_later_octaves)
{
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    BogContext ctx = make_context(NULL);
    BogEnv env;
    bog_env_init(&env);

    /* scale(60, ionian, 8, 0, N) -> N = 72 (C5) */
    BogTerm* args[5];
    args[0] = bog_make_num(arena, 60);
    args[1] = bog_make_atom(arena, "ionian");
    args[2] = bog_make_num(arena, 8); /* Degree 8 wraps */
    args[3] = bog_make_num(arena, 0);
    args[4] = bog_make_var(arena, "N");

    BogBuiltinResult result = { 0 };
    invoke_builtin(builtins, "scale", args, 5, &env, &ctx, &result, arena);

    ASSERT_EQ(result.count, 1);
    BogTerm* n_val = bog_env_get(&result.envs[0], "N");
    ASSERT_NEAR(n_val->value.number, 72.0, 1e-9); /* C5 */

    for (size_t i = 0; i < result.count; i++)
        bog_env_free(&result.envs[i]);
    free(result.envs);
    bog_env_free(&env);
    bog_arena_destroy(arena);
    TEST_PASS();
}

/* chord builtin */

TEST(builtin_chord_emits_each_tone_as_separate_env)
{
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    BogContext ctx = make_context(NULL);
    BogEnv env;
    bog_env_init(&env);

    /* chord(60, maj, 0, N) -> multiple N values */
    BogTerm* args[4];
    args[0] = bog_make_num(arena, 60);
    args[1] = bog_make_atom(arena, "maj");
    args[2] = bog_make_num(arena, 0);
    args[3] = bog_make_var(arena, "N");

    BogBuiltinResult result = { 0 };
    invoke_builtin(builtins, "chord", args, 4, &env, &ctx, &result, arena);

    ASSERT_EQ(result.count, 3); /* Major triad has 3 notes */

    /* Collect values */
    double values[3];
    for (size_t i = 0; i < 3; i++) {
        BogTerm* n_val = bog_env_get(&result.envs[i], "N");
        values[i] = n_val->value.number;
    }

    /* Sort and check for C-E-G (60, 64, 67) */
    for (int i = 0; i < 2; i++) {
        for (int j = i + 1; j < 3; j++) {
            if (values[i] > values[j]) {
                double tmp = values[i];
                values[i] = values[j];
                values[j] = tmp;
            }
        }
    }
    ASSERT_NEAR(values[0], 60.0, 1e-9);
    ASSERT_NEAR(values[1], 64.0, 1e-9);
    ASSERT_NEAR(values[2], 67.0, 1e-9);

    for (size_t i = 0; i < result.count; i++)
        bog_env_free(&result.envs[i]);
    free(result.envs);
    bog_env_free(&env);
    bog_arena_destroy(arena);
    TEST_PASS();
}

/* transpose builtin */

TEST(builtin_transpose_offsets_pitch)
{
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    BogContext ctx = make_context(NULL);
    BogEnv env;
    bog_env_init(&env);

    /* transpose(60, 7, X) -> X = 67 */
    BogTerm* args[3];
    args[0] = bog_make_num(arena, 60);
    args[1] = bog_make_num(arena, 7);
    args[2] = bog_make_var(arena, "X");

    BogBuiltinResult result = { 0 };
    invoke_builtin(builtins, "transpose", args, 3, &env, &ctx, &result, arena);

    ASSERT_EQ(result.count, 1);
    BogTerm* x_val = bog_env_get(&result.envs[0], "X");
    ASSERT_NEAR(x_val->value.number, 67.0, 1e-9);

    for (size_t i = 0; i < result.count; i++)
        bog_env_free(&result.envs[i]);
    free(result.envs);
    bog_env_free(&env);
    bog_arena_destroy(arena);
    TEST_PASS();
}

/* rotate builtin */

TEST(builtin_rotate_rotates_list)
{
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    BogContext ctx = make_context(NULL);
    BogEnv env;
    bog_env_init(&env);

    /* rotate([1,2,3], 1, X) -> X = [2,3,1] */
    BogTerm* items[3];
    items[0] = bog_make_num(arena, 1);
    items[1] = bog_make_num(arena, 2);
    items[2] = bog_make_num(arena, 3);
    BogTerm* list = bog_make_list(arena, items, 3, NULL);

    BogTerm* args[3];
    args[0] = list;
    args[1] = bog_make_num(arena, 1);
    args[2] = bog_make_var(arena, "X");

    BogBuiltinResult result = { 0 };
    invoke_builtin(builtins, "rotate", args, 3, &env, &ctx, &result, arena);

    ASSERT_EQ(result.count, 1);
    BogTerm* x_val = bog_env_get(&result.envs[0], "X");
    ASSERT_NOT_NULL(x_val);
    ASSERT_EQ(x_val->type, CPROLOG_TERM_LIST);
    ASSERT_EQ(x_val->value.list.length, 3);
    ASSERT_NEAR(x_val->value.list.items[0]->value.number, 2.0, 1e-9);
    ASSERT_NEAR(x_val->value.list.items[1]->value.number, 3.0, 1e-9);
    ASSERT_NEAR(x_val->value.list.items[2]->value.number, 1.0, 1e-9);

    for (size_t i = 0; i < result.count; i++)
        bog_env_free(&result.envs[i]);
    free(result.envs);
    bog_env_free(&env);
    bog_arena_destroy(arena);
    TEST_PASS();
}

/* range builtin */

TEST(builtin_range_yields_numbers_over_range)
{
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    BogContext ctx = make_context(NULL);
    BogEnv env;
    bog_env_init(&env);

    /* range(0, 2, 1, X) -> X = 0, 1, 2 */
    BogTerm* args[4];
    args[0] = bog_make_num(arena, 0);
    args[1] = bog_make_num(arena, 2);
    args[2] = bog_make_num(arena, 1);
    args[3] = bog_make_var(arena, "X");

    BogBuiltinResult result = { 0 };
    invoke_builtin(builtins, "range", args, 4, &env, &ctx, &result, arena);

    ASSERT_EQ(result.count, 3);

    double vals[3];
    for (size_t i = 0; i < 3; i++) {
        BogTerm* x_val = bog_env_get(&result.envs[i], "X");
        vals[i] = x_val->value.number;
    }
    ASSERT_NEAR(vals[0], 0.0, 1e-9);
    ASSERT_NEAR(vals[1], 1.0, 1e-9);
    ASSERT_NEAR(vals[2], 2.0, 1e-9);

    for (size_t i = 0; i < result.count; i++)
        bog_env_free(&result.envs[i]);
    free(result.envs);
    bog_env_free(&env);
    bog_arena_destroy(arena);
    TEST_PASS();
}

/* lt/gt builtins */

TEST(builtin_lt_tests_numeric_ordering)
{
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    BogContext ctx = make_context(NULL);
    BogEnv env;
    bog_env_init(&env);

    /* lt(1, 2) -> succeeds */
    BogTerm* args1[2];
    args1[0] = bog_make_num(arena, 1);
    args1[1] = bog_make_num(arena, 2);

    BogBuiltinResult result1 = { 0 };
    invoke_builtin(builtins, "lt", args1, 2, &env, &ctx, &result1, arena);
    ASSERT_EQ(result1.count, 1);
    for (size_t i = 0; i < result1.count; i++)
        bog_env_free(&result1.envs[i]);
    free(result1.envs);

    /* lt(2, 1) -> fails */
    BogTerm* args2[2];
    args2[0] = bog_make_num(arena, 2);
    args2[1] = bog_make_num(arena, 1);

    BogBuiltinResult result2 = { 0 };
    invoke_builtin(builtins, "lt", args2, 2, &env, &ctx, &result2, arena);
    ASSERT_EQ(result2.count, 0);
    free(result2.envs);

    bog_env_free(&env);
    bog_arena_destroy(arena);
    TEST_PASS();
}

TEST(builtin_gt_tests_numeric_ordering)
{
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    BogContext ctx = make_context(NULL);
    BogEnv env;
    bog_env_init(&env);

    /* gt(2, 1) -> succeeds */
    BogTerm* args[2];
    args[0] = bog_make_num(arena, 2);
    args[1] = bog_make_num(arena, 1);

    BogBuiltinResult result = { 0 };
    invoke_builtin(builtins, "gt", args, 2, &env, &ctx, &result, arena);
    ASSERT_EQ(result.count, 1);

    for (size_t i = 0; i < result.count; i++)
        bog_env_free(&result.envs[i]);
    free(result.envs);
    bog_env_free(&env);
    bog_arena_destroy(arena);
    TEST_PASS();
}

/* within builtin */

TEST(builtin_within_accepts_times_inside_bounds)
{
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    BogContext ctx = make_context(NULL);
    BogEnv env;
    bog_env_init(&env);

    /* within(1, 0, 2) -> succeeds */
    BogTerm* args1[3];
    args1[0] = bog_make_num(arena, 1);
    args1[1] = bog_make_num(arena, 0);
    args1[2] = bog_make_num(arena, 2);

    BogBuiltinResult result1 = { 0 };
    invoke_builtin(builtins, "within", args1, 3, &env, &ctx, &result1, arena);
    ASSERT_EQ(result1.count, 1);
    for (size_t i = 0; i < result1.count; i++)
        bog_env_free(&result1.envs[i]);
    free(result1.envs);

    /* within(3, 0, 2) -> fails */
    BogTerm* args2[3];
    args2[0] = bog_make_num(arena, 3);
    args2[1] = bog_make_num(arena, 0);
    args2[2] = bog_make_num(arena, 2);

    BogBuiltinResult result2 = { 0 };
    invoke_builtin(builtins, "within", args2, 3, &env, &ctx, &result2, arena);
    ASSERT_EQ(result2.count, 0);
    free(result2.envs);

    bog_env_free(&env);
    bog_arena_destroy(arena);
    TEST_PASS();
}

/* distinct builtin */

TEST(builtin_distinct_fails_with_duplicates)
{
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    BogContext ctx = make_context(NULL);
    BogEnv env;
    bog_env_init(&env);

    /* distinct([1, 1]) -> fails */
    BogTerm* items1[2];
    items1[0] = bog_make_num(arena, 1);
    items1[1] = bog_make_num(arena, 1);
    BogTerm* list1 = bog_make_list(arena, items1, 2, NULL);

    BogTerm* args1[1];
    args1[0] = list1;

    BogBuiltinResult result1 = { 0 };
    invoke_builtin(builtins, "distinct", args1, 1, &env, &ctx, &result1, arena);
    ASSERT_EQ(result1.count, 0);
    free(result1.envs);

    /* distinct([1, 2]) -> succeeds */
    BogTerm* items2[2];
    items2[0] = bog_make_num(arena, 1);
    items2[1] = bog_make_num(arena, 2);
    BogTerm* list2 = bog_make_list(arena, items2, 2, NULL);

    BogTerm* args2[1];
    args2[0] = list2;

    BogBuiltinResult result2 = { 0 };
    invoke_builtin(builtins, "distinct", args2, 1, &env, &ctx, &result2, arena);
    ASSERT_EQ(result2.count, 1);
    for (size_t i = 0; i < result2.count; i++)
        bog_env_free(&result2.envs[i]);
    free(result2.envs);

    bog_env_free(&env);
    bog_arena_destroy(arena);
    TEST_PASS();
}

/* choose builtin */

TEST(builtin_choose_yields_all_elements)
{
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    BogContext ctx = make_context(NULL);
    BogEnv env;
    bog_env_init(&env);

    /* choose([40, 43], N) -> N = 40, N = 43 */
    BogTerm* items[2];
    items[0] = bog_make_num(arena, 40);
    items[1] = bog_make_num(arena, 43);
    BogTerm* list = bog_make_list(arena, items, 2, NULL);

    BogTerm* args[2];
    args[0] = list;
    args[1] = bog_make_var(arena, "N");

    BogBuiltinResult result = { 0 };
    invoke_builtin(builtins, "choose", args, 2, &env, &ctx, &result, arena);

    ASSERT_EQ(result.count, 2);
    BogTerm* n1 = bog_env_get(&result.envs[0], "N");
    BogTerm* n2 = bog_env_get(&result.envs[1], "N");
    ASSERT_NEAR(n1->value.number, 40.0, 1e-9);
    ASSERT_NEAR(n2->value.number, 43.0, 1e-9);

    for (size_t i = 0; i < result.count; i++)
        bog_env_free(&result.envs[i]);
    free(result.envs);
    bog_env_free(&env);
    bog_arena_destroy(arena);
    TEST_PASS();
}

/* add builtin */

TEST(builtin_add_performs_addition)
{
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    BogContext ctx = make_context(NULL);
    BogEnv env;
    bog_env_init(&env);

    /* add(2, 3, X) -> X = 5 */
    BogTerm* args[3];
    args[0] = bog_make_num(arena, 2);
    args[1] = bog_make_num(arena, 3);
    args[2] = bog_make_var(arena, "X");

    BogBuiltinResult result = { 0 };
    invoke_builtin(builtins, "add", args, 3, &env, &ctx, &result, arena);

    ASSERT_EQ(result.count, 1);
    BogTerm* x_val = bog_env_get(&result.envs[0], "X");
    ASSERT_NEAR(x_val->value.number, 5.0, 1e-9);

    for (size_t i = 0; i < result.count; i++)
        bog_env_free(&result.envs[i]);
    free(result.envs);
    bog_env_free(&env);
    bog_arena_destroy(arena);
    TEST_PASS();
}

int main(void)
{
    printf("Builtins Tests\n");
    printf("==============\n");

    /* scale */
    RUN_TEST(builtin_scale_maps_degree_to_midi_ionian);
    RUN_TEST(builtin_scale_wraps_degrees_into_later_octaves);

    /* chord */
    RUN_TEST(builtin_chord_emits_each_tone_as_separate_env);

    /* transpose */
    RUN_TEST(builtin_transpose_offsets_pitch);

    /* rotate */
    RUN_TEST(builtin_rotate_rotates_list);

    /* range */
    RUN_TEST(builtin_range_yields_numbers_over_range);

    /* lt/gt */
    RUN_TEST(builtin_lt_tests_numeric_ordering);
    RUN_TEST(builtin_gt_tests_numeric_ordering);

    /* within */
    RUN_TEST(builtin_within_accepts_times_inside_bounds);

    /* distinct */
    RUN_TEST(builtin_distinct_fails_with_duplicates);

    /* choose */
    RUN_TEST(builtin_choose_yields_all_elements);

    /* add */
    RUN_TEST(builtin_add_performs_addition);

    TEST_SUMMARY();
    return TEST_EXIT_CODE();
}
