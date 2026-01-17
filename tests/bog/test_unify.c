/*
 * Unification tests - converted from dogalog tests/prolog/unify.test.js
 */
#include "test_framework.h"

#include "bog.h"

TEST(unify_binds_variables_to_atoms)
{
    BogArena* arena = bog_arena_create();
    BogEnv env;
    bog_env_init(&env);

    BogTerm* var = bog_make_var(arena, "X");
    BogTerm* atom = bog_make_atom(arena, "kick");

    bool result = bog_unify(var, atom, &env, arena);

    ASSERT(result);
    BogTerm* bound = bog_env_get(&env, "X");
    ASSERT_NOT_NULL(bound);
    ASSERT_EQ(bound->type, CPROLOG_TERM_ATOM);
    ASSERT_STREQ(bound->value.atom, "kick");

    bog_env_free(&env);
    bog_arena_destroy(arena);
    TEST_PASS();
}

TEST(unify_fails_on_mismatched_atoms)
{
    BogArena* arena = bog_arena_create();
    BogEnv env;
    bog_env_init(&env);

    BogTerm* atom1 = bog_make_atom(arena, "kick");
    BogTerm* atom2 = bog_make_atom(arena, "snare");

    bool result = bog_unify(atom1, atom2, &env, arena);

    ASSERT(!result);

    bog_env_free(&env);
    bog_arena_destroy(arena);
    TEST_PASS();
}

TEST(unify_handles_lists_element_wise)
{
    BogArena* arena = bog_arena_create();
    BogEnv env;
    bog_env_init(&env);

    BogTerm* items1[2];
    items1[0] = bog_make_num(arena, 1);
    items1[1] = bog_make_var(arena, "X");
    BogTerm* list1 = bog_make_list(arena, items1, 2, NULL);

    BogTerm* items2[2];
    items2[0] = bog_make_num(arena, 1);
    items2[1] = bog_make_num(arena, 2);
    BogTerm* list2 = bog_make_list(arena, items2, 2, NULL);

    bool result = bog_unify(list1, list2, &env, arena);

    ASSERT(result);
    BogTerm* bound = bog_env_get(&env, "X");
    ASSERT_NOT_NULL(bound);
    ASSERT_EQ(bound->type, CPROLOG_TERM_NUM);
    ASSERT_NEAR(bound->value.number, 2.0, 1e-9);

    bog_env_free(&env);
    bog_arena_destroy(arena);
    TEST_PASS();
}

TEST(unify_numbers_equal)
{
    BogArena* arena = bog_arena_create();
    BogEnv env;
    bog_env_init(&env);

    BogTerm* num1 = bog_make_num(arena, 42.5);
    BogTerm* num2 = bog_make_num(arena, 42.5);

    bool result = bog_unify(num1, num2, &env, arena);

    ASSERT(result);

    bog_env_free(&env);
    bog_arena_destroy(arena);
    TEST_PASS();
}

TEST(unify_numbers_not_equal)
{
    BogArena* arena = bog_arena_create();
    BogEnv env;
    bog_env_init(&env);

    BogTerm* num1 = bog_make_num(arena, 42.5);
    BogTerm* num2 = bog_make_num(arena, 42.6);

    bool result = bog_unify(num1, num2, &env, arena);

    ASSERT(!result);

    bog_env_free(&env);
    bog_arena_destroy(arena);
    TEST_PASS();
}

TEST(unify_compound_terms)
{
    BogArena* arena = bog_arena_create();
    BogEnv env;
    bog_env_init(&env);

    /* foo(X, 2) */
    BogTerm* args1[2];
    args1[0] = bog_make_var(arena, "X");
    args1[1] = bog_make_num(arena, 2);
    BogTerm* compound1 = bog_make_compound(arena, "foo", args1, 2);

    /* foo(1, Y) */
    BogTerm* args2[2];
    args2[0] = bog_make_num(arena, 1);
    args2[1] = bog_make_var(arena, "Y");
    BogTerm* compound2 = bog_make_compound(arena, "foo", args2, 2);

    bool result = bog_unify(compound1, compound2, &env, arena);

    ASSERT(result);

    BogTerm* boundX = bog_env_get(&env, "X");
    ASSERT_NOT_NULL(boundX);
    ASSERT_EQ(boundX->type, CPROLOG_TERM_NUM);
    ASSERT_NEAR(boundX->value.number, 1.0, 1e-9);

    BogTerm* boundY = bog_env_get(&env, "Y");
    ASSERT_NOT_NULL(boundY);
    ASSERT_EQ(boundY->type, CPROLOG_TERM_NUM);
    ASSERT_NEAR(boundY->value.number, 2.0, 1e-9);

    bog_env_free(&env);
    bog_arena_destroy(arena);
    TEST_PASS();
}

TEST(unify_fails_different_functors)
{
    BogArena* arena = bog_arena_create();
    BogEnv env;
    bog_env_init(&env);

    BogTerm* args1[1];
    args1[0] = bog_make_num(arena, 1);
    BogTerm* compound1 = bog_make_compound(arena, "foo", args1, 1);

    BogTerm* args2[1];
    args2[0] = bog_make_num(arena, 1);
    BogTerm* compound2 = bog_make_compound(arena, "bar", args2, 1);

    bool result = bog_unify(compound1, compound2, &env, arena);

    ASSERT(!result);

    bog_env_free(&env);
    bog_arena_destroy(arena);
    TEST_PASS();
}

TEST(unify_fails_different_arity)
{
    BogArena* arena = bog_arena_create();
    BogEnv env;
    bog_env_init(&env);

    BogTerm* args1[1];
    args1[0] = bog_make_num(arena, 1);
    BogTerm* compound1 = bog_make_compound(arena, "foo", args1, 1);

    BogTerm* args2[2];
    args2[0] = bog_make_num(arena, 1);
    args2[1] = bog_make_num(arena, 2);
    BogTerm* compound2 = bog_make_compound(arena, "foo", args2, 2);

    bool result = bog_unify(compound1, compound2, &env, arena);

    ASSERT(!result);

    bog_env_free(&env);
    bog_arena_destroy(arena);
    TEST_PASS();
}

TEST(unify_lists_different_lengths_fail)
{
    BogArena* arena = bog_arena_create();
    BogEnv env;
    bog_env_init(&env);

    BogTerm* items1[2];
    items1[0] = bog_make_num(arena, 1);
    items1[1] = bog_make_num(arena, 2);
    BogTerm* list1 = bog_make_list(arena, items1, 2, NULL);

    BogTerm* items2[3];
    items2[0] = bog_make_num(arena, 1);
    items2[1] = bog_make_num(arena, 2);
    items2[2] = bog_make_num(arena, 3);
    BogTerm* list2 = bog_make_list(arena, items2, 3, NULL);

    bool result = bog_unify(list1, list2, &env, arena);

    ASSERT(!result);

    bog_env_free(&env);
    bog_arena_destroy(arena);
    TEST_PASS();
}

int main(void)
{
    printf("Unification Tests\n");
    printf("=================\n");

    RUN_TEST(unify_binds_variables_to_atoms);
    RUN_TEST(unify_fails_on_mismatched_atoms);
    RUN_TEST(unify_handles_lists_element_wise);
    RUN_TEST(unify_numbers_equal);
    RUN_TEST(unify_numbers_not_equal);
    RUN_TEST(unify_compound_terms);
    RUN_TEST(unify_fails_different_functors);
    RUN_TEST(unify_fails_different_arity);
    RUN_TEST(unify_lists_different_lengths_fail);

    TEST_SUMMARY();
    return TEST_EXIT_CODE();
}
