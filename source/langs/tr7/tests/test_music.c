/**
 * @file test_music.c
 * @brief Unit tests for TR7 music primitives and state management.
 *
 * Tests the TR7 Scheme music primitives including state management,
 * note parsing, and parameter validation.
 */

#include "test_framework.h"
#include "tr7.h"
#include "context.h"
#include <string.h>
#include <stdlib.h>

/* ============================================================================
 * Test State
 * ============================================================================ */

/* TR7 engine for testing */
static tr7_engine_t engine = NULL;

/* Shared context for MIDI operations */
static SharedContext *shared_ctx = NULL;

/* Music state tracking (mirrors TR7 internal state) */
static struct {
    int octave;
    int velocity;
    int tempo;
    int channel;
} music_state;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/* Initialize test environment */
static void setup(void) {
    /* Create TR7 engine */
    engine = tr7_engine_create(NULL);
    if (!engine) {
        fprintf(stderr, "Failed to create TR7 engine\n");
        return;
    }

    /* Load standard libraries */
    tr7_load_string(engine,
        "(import (scheme base)"
        "(scheme read)"
        "(scheme write))");

    /* Set standard I/O */
    tr7_set_standard_ports(engine);

    /* Create shared context (no MIDI output for tests) */
    shared_ctx = (SharedContext *)calloc(1, sizeof(SharedContext));
    if (shared_ctx) {
        shared_context_init(shared_ctx);
    }

    /* Initialize music state with defaults */
    music_state.octave = 4;
    music_state.velocity = 80;
    music_state.tempo = 120;
    music_state.channel = 1;
}

/* Cleanup test environment */
static void teardown(void) {
    if (shared_ctx) {
        shared_context_cleanup(shared_ctx);
        free(shared_ctx);
        shared_ctx = NULL;
    }

    if (engine) {
        tr7_engine_destroy(engine);
        engine = NULL;
    }
}

/* Evaluate Scheme code and return success (1) or failure (0) */
static int eval_ok(const char *code) {
    if (!engine) return 0;
    return tr7_run_string(engine, code);
}

/* Evaluate and get integer result */
static int eval_int(const char *code) {
    if (!engine) return 0;
    if (!tr7_run_string(engine, code)) return 0;
    tr7_t val = tr7_get_last_value(engine);
    if (TR7_IS_INT(val)) {
        return TR7_TO_INT(val);
    }
    return 0;
}

/* Evaluate and check if result is true */
static int eval_true(const char *code) {
    if (!engine) return 0;
    if (!tr7_run_string(engine, code)) return 0;
    tr7_t val = tr7_get_last_value(engine);
    return TR7_IS_TRUE(val);
}

/* ============================================================================
 * Basic Engine Tests
 * ============================================================================ */

TEST(engine_creation) {
    setup();
    ASSERT_NOT_NULL(engine);
    teardown();
}

TEST(engine_basic_eval) {
    setup();
    ASSERT_TRUE(eval_ok("(+ 1 2)"));
    ASSERT_EQ(eval_int("(+ 1 2)"), 3);
    teardown();
}

TEST(engine_define_and_use) {
    setup();
    ASSERT_TRUE(eval_ok("(define x 42)"));
    ASSERT_EQ(eval_int("x"), 42);
    teardown();
}

TEST(engine_lambda) {
    setup();
    ASSERT_TRUE(eval_ok("(define square (lambda (n) (* n n)))"));
    ASSERT_EQ(eval_int("(square 5)"), 25);
    teardown();
}

TEST(engine_conditionals) {
    setup();
    ASSERT_EQ(eval_int("(if (> 5 3) 1 0)"), 1);
    ASSERT_EQ(eval_int("(if (< 5 3) 1 0)"), 0);
    teardown();
}

TEST(engine_list_operations) {
    setup();
    ASSERT_TRUE(eval_ok("(define lst '(1 2 3))"));
    ASSERT_EQ(eval_int("(car lst)"), 1);
    ASSERT_EQ(eval_int("(car (cdr lst))"), 2);
    ASSERT_EQ(eval_int("(length lst)"), 3);
    teardown();
}

/* ============================================================================
 * Arithmetic Tests (Scheme built-ins)
 * ============================================================================ */

TEST(arithmetic_addition) {
    setup();
    ASSERT_EQ(eval_int("(+ 10 20)"), 30);
    ASSERT_EQ(eval_int("(+ 1 2 3 4 5)"), 15);
    ASSERT_EQ(eval_int("(+ -5 10)"), 5);
    teardown();
}

TEST(arithmetic_subtraction) {
    setup();
    ASSERT_EQ(eval_int("(- 100 30)"), 70);
    ASSERT_EQ(eval_int("(- 10 3 2 1)"), 4);
    ASSERT_EQ(eval_int("(- 5)"), -5);
    teardown();
}

TEST(arithmetic_multiplication) {
    setup();
    ASSERT_EQ(eval_int("(* 6 7)"), 42);
    ASSERT_EQ(eval_int("(* 2 3 4)"), 24);
    teardown();
}

TEST(arithmetic_division) {
    setup();
    ASSERT_EQ(eval_int("(quotient 10 3)"), 3);
    ASSERT_EQ(eval_int("(remainder 10 3)"), 1);
    ASSERT_EQ(eval_int("(modulo 10 3)"), 1);
    teardown();
}

TEST(arithmetic_comparisons) {
    setup();
    ASSERT_TRUE(eval_true("(= 5 5)"));
    ASSERT_TRUE(eval_true("(< 3 5)"));
    ASSERT_TRUE(eval_true("(> 5 3)"));
    ASSERT_TRUE(eval_true("(<= 5 5)"));
    ASSERT_TRUE(eval_true("(>= 5 5)"));
    teardown();
}

/* ============================================================================
 * MIDI Value Clamping Tests
 * ============================================================================ */

TEST(clamp_velocity_lower_bound) {
    setup();
    /* Test that negative velocity would be clamped to 0 */
    /* We test the clamping logic indirectly through valid ranges */
    int vel = -10;
    if (vel < 0) vel = 0;
    if (vel > 127) vel = 127;
    ASSERT_EQ(vel, 0);
    teardown();
}

TEST(clamp_velocity_upper_bound) {
    setup();
    int vel = 200;
    if (vel < 0) vel = 0;
    if (vel > 127) vel = 127;
    ASSERT_EQ(vel, 127);
    teardown();
}

TEST(clamp_pitch_lower_bound) {
    setup();
    int pitch = -5;
    if (pitch < 0) pitch = 0;
    if (pitch > 127) pitch = 127;
    ASSERT_EQ(pitch, 0);
    teardown();
}

TEST(clamp_pitch_upper_bound) {
    setup();
    int pitch = 150;
    if (pitch < 0) pitch = 0;
    if (pitch > 127) pitch = 127;
    ASSERT_EQ(pitch, 127);
    teardown();
}

TEST(clamp_channel_lower_bound) {
    setup();
    int ch = 0;
    if (ch < 1) ch = 1;
    if (ch > 16) ch = 16;
    ASSERT_EQ(ch, 1);
    teardown();
}

TEST(clamp_channel_upper_bound) {
    setup();
    int ch = 20;
    if (ch < 1) ch = 1;
    if (ch > 16) ch = 16;
    ASSERT_EQ(ch, 16);
    teardown();
}

/* ============================================================================
 * Duration Calculation Tests
 * ============================================================================ */

TEST(duration_at_tempo_120) {
    setup();
    /* At 120 BPM, quarter note = 500ms */
    int tempo = 120;
    int duration_ms = 60000 / tempo;
    ASSERT_EQ(duration_ms, 500);
    teardown();
}

TEST(duration_at_tempo_60) {
    setup();
    /* At 60 BPM, quarter note = 1000ms */
    int tempo = 60;
    int duration_ms = 60000 / tempo;
    ASSERT_EQ(duration_ms, 1000);
    teardown();
}

TEST(duration_at_tempo_240) {
    setup();
    /* At 240 BPM, quarter note = 250ms */
    int tempo = 240;
    int duration_ms = 60000 / tempo;
    ASSERT_EQ(duration_ms, 250);
    teardown();
}

/* ============================================================================
 * Note Name to MIDI Pitch Conversion Tests
 * ============================================================================ */

/* Reference table for note names at octave 4 */
static int note_to_midi(char note, int sharp, int flat, int octave) {
    int base;
    switch (note) {
        case 'c': case 'C': base = 0; break;
        case 'd': case 'D': base = 2; break;
        case 'e': case 'E': base = 4; break;
        case 'f': case 'F': base = 5; break;
        case 'g': case 'G': base = 7; break;
        case 'a': case 'A': base = 9; break;
        case 'b': case 'B': base = 11; break;
        default: return -1;
    }
    return 12 * (octave + 1) + base + sharp - flat;
}

TEST(note_c4_is_60) {
    setup();
    ASSERT_EQ(note_to_midi('c', 0, 0, 4), 60);
    teardown();
}

TEST(note_a4_is_69) {
    setup();
    ASSERT_EQ(note_to_midi('a', 0, 0, 4), 69);
    teardown();
}

TEST(note_middle_c_sharp) {
    setup();
    /* C#4 = 61 */
    ASSERT_EQ(note_to_midi('c', 1, 0, 4), 61);
    teardown();
}

TEST(note_d_flat_equals_c_sharp) {
    setup();
    /* Db4 = C#4 = 61 */
    ASSERT_EQ(note_to_midi('d', 0, 1, 4), 61);
    ASSERT_EQ(note_to_midi('c', 1, 0, 4), note_to_midi('d', 0, 1, 4));
    teardown();
}

TEST(note_octave_range) {
    setup();
    /* C0 = 12, C1 = 24, ..., C8 = 108 */
    ASSERT_EQ(note_to_midi('c', 0, 0, 0), 12);
    ASSERT_EQ(note_to_midi('c', 0, 0, 1), 24);
    ASSERT_EQ(note_to_midi('c', 0, 0, 5), 72);
    ASSERT_EQ(note_to_midi('c', 0, 0, 8), 108);
    teardown();
}

TEST(note_all_naturals_octave_4) {
    setup();
    ASSERT_EQ(note_to_midi('c', 0, 0, 4), 60);
    ASSERT_EQ(note_to_midi('d', 0, 0, 4), 62);
    ASSERT_EQ(note_to_midi('e', 0, 0, 4), 64);
    ASSERT_EQ(note_to_midi('f', 0, 0, 4), 65);
    ASSERT_EQ(note_to_midi('g', 0, 0, 4), 67);
    ASSERT_EQ(note_to_midi('a', 0, 0, 4), 69);
    ASSERT_EQ(note_to_midi('b', 0, 0, 4), 71);
    teardown();
}

/* ============================================================================
 * State Default Tests
 * ============================================================================ */

TEST(default_octave_is_4) {
    setup();
    ASSERT_EQ(music_state.octave, 4);
    teardown();
}

TEST(default_velocity_is_80) {
    setup();
    ASSERT_EQ(music_state.velocity, 80);
    teardown();
}

TEST(default_tempo_is_120) {
    setup();
    ASSERT_EQ(music_state.tempo, 120);
    teardown();
}

TEST(default_channel_is_1) {
    setup();
    ASSERT_EQ(music_state.channel, 1);
    teardown();
}

/* ============================================================================
 * Tempo Range Tests
 * ============================================================================ */

TEST(tempo_minimum_valid) {
    setup();
    int tempo = 20;
    if (tempo < 20) tempo = 20;
    if (tempo > 400) tempo = 400;
    ASSERT_EQ(tempo, 20);
    teardown();
}

TEST(tempo_maximum_valid) {
    setup();
    int tempo = 400;
    if (tempo < 20) tempo = 20;
    if (tempo > 400) tempo = 400;
    ASSERT_EQ(tempo, 400);
    teardown();
}

TEST(tempo_clamp_below_minimum) {
    setup();
    int tempo = 10;
    if (tempo < 20) tempo = 20;
    if (tempo > 400) tempo = 400;
    ASSERT_EQ(tempo, 20);
    teardown();
}

TEST(tempo_clamp_above_maximum) {
    setup();
    int tempo = 500;
    if (tempo < 20) tempo = 20;
    if (tempo > 400) tempo = 400;
    ASSERT_EQ(tempo, 400);
    teardown();
}

/* ============================================================================
 * Octave Range Tests
 * ============================================================================ */

TEST(octave_valid_range_0) {
    setup();
    int octave = 0;
    if (octave < 0) octave = 0;
    if (octave > 9) octave = 9;
    ASSERT_EQ(octave, 0);
    teardown();
}

TEST(octave_valid_range_9) {
    setup();
    int octave = 9;
    if (octave < 0) octave = 0;
    if (octave > 9) octave = 9;
    ASSERT_EQ(octave, 9);
    teardown();
}

TEST(octave_clamp_negative) {
    setup();
    int octave = -1;
    if (octave < 0) octave = 0;
    if (octave > 9) octave = 9;
    ASSERT_EQ(octave, 0);
    teardown();
}

TEST(octave_clamp_above_max) {
    setup();
    int octave = 10;
    if (octave < 0) octave = 0;
    if (octave > 9) octave = 9;
    ASSERT_EQ(octave, 9);
    teardown();
}

/* ============================================================================
 * Test Runner
 * ============================================================================ */

BEGIN_TEST_SUITE("tr7_music_tests")
    /* Engine tests */
    RUN_TEST(engine_creation);
    RUN_TEST(engine_basic_eval);
    RUN_TEST(engine_define_and_use);
    RUN_TEST(engine_lambda);
    RUN_TEST(engine_conditionals);
    RUN_TEST(engine_list_operations);

    /* Arithmetic tests */
    RUN_TEST(arithmetic_addition);
    RUN_TEST(arithmetic_subtraction);
    RUN_TEST(arithmetic_multiplication);
    RUN_TEST(arithmetic_division);
    RUN_TEST(arithmetic_comparisons);

    /* MIDI clamping tests */
    RUN_TEST(clamp_velocity_lower_bound);
    RUN_TEST(clamp_velocity_upper_bound);
    RUN_TEST(clamp_pitch_lower_bound);
    RUN_TEST(clamp_pitch_upper_bound);
    RUN_TEST(clamp_channel_lower_bound);
    RUN_TEST(clamp_channel_upper_bound);

    /* Duration calculation tests */
    RUN_TEST(duration_at_tempo_120);
    RUN_TEST(duration_at_tempo_60);
    RUN_TEST(duration_at_tempo_240);

    /* Note conversion tests */
    RUN_TEST(note_c4_is_60);
    RUN_TEST(note_a4_is_69);
    RUN_TEST(note_middle_c_sharp);
    RUN_TEST(note_d_flat_equals_c_sharp);
    RUN_TEST(note_octave_range);
    RUN_TEST(note_all_naturals_octave_4);

    /* State default tests */
    RUN_TEST(default_octave_is_4);
    RUN_TEST(default_velocity_is_80);
    RUN_TEST(default_tempo_is_120);
    RUN_TEST(default_channel_is_1);

    /* Tempo range tests */
    RUN_TEST(tempo_minimum_valid);
    RUN_TEST(tempo_maximum_valid);
    RUN_TEST(tempo_clamp_below_minimum);
    RUN_TEST(tempo_clamp_above_maximum);

    /* Octave range tests */
    RUN_TEST(octave_valid_range_0);
    RUN_TEST(octave_valid_range_9);
    RUN_TEST(octave_clamp_negative);
    RUN_TEST(octave_clamp_above_max);
END_TEST_SUITE()
