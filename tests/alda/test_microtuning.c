/**
 * @file test_microtuning.c
 * @brief Tests for Scala scale microtuning integration with Alda.
 *
 * Tests:
 * - Scala scale file loading
 * - MIDI-to-frequency conversion
 * - Per-part scale assignment
 * - Just intonation frequency verification
 */

#include "../test_framework.h"
#include <alda/alda.h>
#include <alda/context.h>
#include <alda/interpreter.h>
#include <alda/scala.h>
#include <math.h>
#include <string.h>

/* Test data paths - relative to build directory */
#define TEST_DATA_DIR "../tests/alda/data"
#define JUST_MAJOR_SCL TEST_DATA_DIR "/just_major.scl"
#define JUST_12_SCL TEST_DATA_DIR "/just_12.scl"

/* Frequency comparison tolerance (0.1 Hz) */
#define FREQ_TOLERANCE 0.1

/* Helper to check if two frequencies are approximately equal */
static int freq_approx_equal(double a, double b) {
    return fabs(a - b) < FREQ_TOLERANCE;
}

/* ============================================================================
 * Scala Scale Loading Tests
 * ============================================================================ */

TEST(microtuning_load_scale) {
    ScalaScale *scale = scala_load(JUST_MAJOR_SCL);
    ASSERT_NOT_NULL(scale);

    /* Just major scale has 7 degrees (plus implicit 1/1) */
    int length = scala_get_length(scale);
    ASSERT_EQ(length, 7);

    scala_free(scale);
}

TEST(microtuning_scale_description) {
    ScalaScale *scale = scala_load(JUST_MAJOR_SCL);
    ASSERT_NOT_NULL(scale);

    const char *desc = scala_get_description(scale);
    ASSERT_NOT_NULL(desc);
    /* Description should contain "Just" */
    ASSERT_TRUE(strstr(desc, "Just") != NULL);

    scala_free(scale);
}

TEST(microtuning_scale_ratios) {
    ScalaScale *scale = scala_load(JUST_MAJOR_SCL);
    ASSERT_NOT_NULL(scale);

    /* Just major scale ratios:
     * Degree 0: 1/1 = 1.0 (implicit)
     * Degree 1: 9/8 = 1.125
     * Degree 2: 5/4 = 1.25
     * Degree 3: 4/3 = 1.333...
     * Degree 4: 3/2 = 1.5
     * Degree 5: 5/3 = 1.666...
     * Degree 6: 15/8 = 1.875
     * Degree 7: 2/1 = 2.0 (octave)
     */

    double r0 = scala_get_ratio(scale, 0);
    double r1 = scala_get_ratio(scale, 1);
    double r2 = scala_get_ratio(scale, 2);
    double r3 = scala_get_ratio(scale, 3);
    double r4 = scala_get_ratio(scale, 4);
    double r5 = scala_get_ratio(scale, 5);
    double r6 = scala_get_ratio(scale, 6);
    double r7 = scala_get_ratio(scale, 7);

    ASSERT_TRUE(fabs(r0 - 1.0) < 0.001);
    ASSERT_TRUE(fabs(r1 - 9.0/8.0) < 0.001);
    ASSERT_TRUE(fabs(r2 - 5.0/4.0) < 0.001);
    ASSERT_TRUE(fabs(r3 - 4.0/3.0) < 0.001);
    ASSERT_TRUE(fabs(r4 - 3.0/2.0) < 0.001);
    ASSERT_TRUE(fabs(r5 - 5.0/3.0) < 0.001);
    ASSERT_TRUE(fabs(r6 - 15.0/8.0) < 0.001);
    ASSERT_TRUE(fabs(r7 - 2.0) < 0.001);

    scala_free(scale);
}

/* ============================================================================
 * MIDI to Frequency Conversion Tests
 *
 * Note: scala_midi_to_freq maps MIDI note differences to scale degrees.
 * For a 7-note scale, every MIDI note increment = 1 scale degree.
 * For proper 12-TET chromatic input, use a 12-note scale.
 * ============================================================================ */

/* 12-note scale for testing chromatic MIDI input */
static const char *TWELVE_TET_SCL =
    "! 12tet.scl\n"
    "12-tone equal temperament\n"
    "12\n"
    "!\n"
    "100.0\n"   /* 1 - minor 2nd */
    "200.0\n"   /* 2 - major 2nd */
    "300.0\n"   /* 3 - minor 3rd */
    "400.0\n"   /* 4 - major 3rd */
    "500.0\n"   /* 5 - perfect 4th */
    "600.0\n"   /* 6 - tritone */
    "700.0\n"   /* 7 - perfect 5th */
    "800.0\n"   /* 8 - minor 6th */
    "900.0\n"   /* 9 - major 6th */
    "1000.0\n"  /* 10 - minor 7th */
    "1100.0\n"  /* 11 - major 7th */
    "1200.0\n"; /* 12 - octave */

TEST(microtuning_midi_to_freq_root) {
    ScalaScale *scale = scala_load(JUST_MAJOR_SCL);
    ASSERT_NOT_NULL(scale);

    /* Root note (C4 = 60) at 261.63 Hz should return exactly 261.63 Hz */
    double freq = scala_midi_to_freq(scale, 60, 60, 261.6255653);
    ASSERT_TRUE(freq_approx_equal(freq, 261.6255653));

    scala_free(scale);
}

TEST(microtuning_midi_to_freq_12tet_fifth) {
    /* Use 12-TET scale for chromatic MIDI input */
    ScalaScale *scale = scala_load_string(TWELVE_TET_SCL, strlen(TWELVE_TET_SCL));
    ASSERT_NOT_NULL(scale);

    /* G4 (MIDI 67) = 7 semitones above C4 */
    double c4_freq = 261.6255653;
    double expected_g4 = c4_freq * pow(2.0, 7.0/12.0);  /* 12-TET fifth */
    double freq = scala_midi_to_freq(scale, 67, 60, c4_freq);
    ASSERT_TRUE(freq_approx_equal(freq, expected_g4));

    scala_free(scale);
}

TEST(microtuning_midi_to_freq_12tet_major_third) {
    ScalaScale *scale = scala_load_string(TWELVE_TET_SCL, strlen(TWELVE_TET_SCL));
    ASSERT_NOT_NULL(scale);

    /* E4 (MIDI 64) = 4 semitones above C4 */
    double c4_freq = 261.6255653;
    double expected_e4 = c4_freq * pow(2.0, 4.0/12.0);  /* 12-TET major third */
    double freq = scala_midi_to_freq(scale, 64, 60, c4_freq);
    ASSERT_TRUE(freq_approx_equal(freq, expected_e4));

    scala_free(scale);
}

TEST(microtuning_midi_to_freq_12tet_octave_up) {
    ScalaScale *scale = scala_load_string(TWELVE_TET_SCL, strlen(TWELVE_TET_SCL));
    ASSERT_NOT_NULL(scale);

    /* C5 (MIDI 72) should be exactly 2x C4 */
    double c4_freq = 261.6255653;
    double expected_c5 = c4_freq * 2.0;
    double freq = scala_midi_to_freq(scale, 72, 60, c4_freq);
    ASSERT_TRUE(freq_approx_equal(freq, expected_c5));

    scala_free(scale);
}

TEST(microtuning_midi_to_freq_12tet_octave_down) {
    ScalaScale *scale = scala_load_string(TWELVE_TET_SCL, strlen(TWELVE_TET_SCL));
    ASSERT_NOT_NULL(scale);

    /* C3 (MIDI 48) should be exactly 0.5x C4 */
    double c4_freq = 261.6255653;
    double expected_c3 = c4_freq * 0.5;
    double freq = scala_midi_to_freq(scale, 48, 60, c4_freq);
    ASSERT_TRUE(freq_approx_equal(freq, expected_c3));

    scala_free(scale);
}

TEST(microtuning_7note_scale_degree_mapping) {
    /* For 7-note scales, MIDI note increment = scale degree increment.
     * MIDI 60 = degree 0 (1/1)
     * MIDI 64 = degree 4 (3/2 = perfect fifth in just major)
     */
    ScalaScale *scale = scala_load(JUST_MAJOR_SCL);
    ASSERT_NOT_NULL(scale);

    double c4_freq = 261.6255653;

    /* Degree 4 (fifth) = MIDI root + 4 = 64 */
    double expected_fifth = c4_freq * 1.5;  /* 3/2 ratio */
    double freq = scala_midi_to_freq(scale, 64, 60, c4_freq);
    ASSERT_TRUE(freq_approx_equal(freq, expected_fifth));

    /* Degree 2 (third) = MIDI root + 2 = 62 */
    double expected_third = c4_freq * 1.25;  /* 5/4 ratio */
    freq = scala_midi_to_freq(scale, 62, 60, c4_freq);
    ASSERT_TRUE(freq_approx_equal(freq, expected_third));

    scala_free(scale);
}

TEST(microtuning_just12_chromatic) {
    /* 12-note just intonation scale for chromatic MIDI input */
    ScalaScale *scale = scala_load(JUST_12_SCL);
    ASSERT_NOT_NULL(scale);

    ASSERT_EQ(scala_get_length(scale), 12);

    double c4_freq = 261.6255653;

    /* Test just perfect fifth (G4 = MIDI 67, 7 semitones above C4) */
    double expected_g4 = c4_freq * 1.5;  /* 3/2 ratio */
    double freq = scala_midi_to_freq(scale, 67, 60, c4_freq);
    ASSERT_TRUE(freq_approx_equal(freq, expected_g4));

    /* Test just major third (E4 = MIDI 64, 4 semitones above C4) */
    double expected_e4 = c4_freq * 1.25;  /* 5/4 ratio */
    freq = scala_midi_to_freq(scale, 64, 60, c4_freq);
    ASSERT_TRUE(freq_approx_equal(freq, expected_e4));

    /* Test just perfect fourth (F4 = MIDI 65, 5 semitones above C4) */
    double expected_f4 = c4_freq * (4.0/3.0);  /* 4/3 ratio */
    freq = scala_midi_to_freq(scale, 65, 60, c4_freq);
    ASSERT_TRUE(freq_approx_equal(freq, expected_f4));

    /* Test just minor third (Eb4 = MIDI 63, 3 semitones above C4) */
    double expected_eb4 = c4_freq * 1.2;  /* 6/5 ratio */
    freq = scala_midi_to_freq(scale, 63, 60, c4_freq);
    ASSERT_TRUE(freq_approx_equal(freq, expected_eb4));

    /* Test octave */
    double expected_c5 = c4_freq * 2.0;
    freq = scala_midi_to_freq(scale, 72, 60, c4_freq);
    ASSERT_TRUE(freq_approx_equal(freq, expected_c5));

    scala_free(scale);
}

/* ============================================================================
 * Per-Part Scale Assignment Tests
 * ============================================================================ */

TEST(microtuning_part_scale_assignment) {
    AldaContext ctx;
    alda_context_init(&ctx);
    alda_set_no_sleep(&ctx, 1);

    /* Create a part */
    AldaPartState *part = alda_get_or_create_part(&ctx, "piano");
    ASSERT_NOT_NULL(part);

    /* Initially no scale */
    ASSERT_NULL(part->scale);

    /* Load and assign scale */
    ScalaScale *scale = scala_load(JUST_MAJOR_SCL);
    ASSERT_NOT_NULL(scale);

    part->scale = scale;
    part->scale_root_note = 60;
    part->scale_root_freq = 261.6255653;

    /* Verify assignment */
    ASSERT_NOT_NULL(part->scale);
    ASSERT_EQ(part->scale_root_note, 60);
    ASSERT_TRUE(freq_approx_equal(part->scale_root_freq, 261.6255653));

    scala_free(scale);
    alda_context_cleanup(&ctx);
}

TEST(microtuning_different_parts_different_scales) {
    AldaContext ctx;
    alda_context_init(&ctx);
    alda_set_no_sleep(&ctx, 1);

    /* Create two parts */
    AldaPartState *piano = alda_get_or_create_part(&ctx, "piano");
    AldaPartState *violin = alda_get_or_create_part(&ctx, "violin");
    ASSERT_NOT_NULL(piano);
    ASSERT_NOT_NULL(violin);

    /* Load scale for piano only */
    ScalaScale *scale = scala_load(JUST_MAJOR_SCL);
    ASSERT_NOT_NULL(scale);

    piano->scale = scale;
    piano->scale_root_note = 60;
    piano->scale_root_freq = 261.6255653;

    /* Violin remains in 12-TET */
    ASSERT_NOT_NULL(piano->scale);
    ASSERT_NULL(violin->scale);

    scala_free(scale);
    alda_context_cleanup(&ctx);
}

TEST(microtuning_part_find_with_scale) {
    AldaContext ctx;
    alda_context_init(&ctx);
    alda_set_no_sleep(&ctx, 1);

    /* Parse Alda to create parts */
    int result = alda_interpret_string(&ctx, "piano: c d e", "test");
    ASSERT_EQ(result, 0);

    /* Find the part and assign scale */
    AldaPartState *part = alda_find_part(&ctx, "piano");
    ASSERT_NOT_NULL(part);

    ScalaScale *scale = scala_load(JUST_MAJOR_SCL);
    ASSERT_NOT_NULL(scale);

    part->scale = scale;
    part->scale_root_note = 60;
    part->scale_root_freq = 261.6255653;

    /* Verify we can find it again with scale intact */
    AldaPartState *found = alda_find_part(&ctx, "piano");
    ASSERT_NOT_NULL(found);
    ASSERT_NOT_NULL(found->scale);

    scala_free(scale);
    alda_context_cleanup(&ctx);
}

/* ============================================================================
 * Cents/Ratio Conversion Tests
 * ============================================================================ */

TEST(microtuning_cents_to_ratio) {
    /* 100 cents = 1 semitone = 2^(1/12) */
    double ratio = scala_cents_to_ratio(100.0);
    double expected = pow(2.0, 1.0/12.0);
    ASSERT_TRUE(fabs(ratio - expected) < 0.0001);
}

TEST(microtuning_cents_to_ratio_octave) {
    /* 1200 cents = 1 octave = 2.0 */
    double ratio = scala_cents_to_ratio(1200.0);
    ASSERT_TRUE(fabs(ratio - 2.0) < 0.0001);
}

TEST(microtuning_ratio_to_cents) {
    /* 2.0 ratio = 1200 cents */
    double cents = scala_ratio_to_cents(2.0);
    ASSERT_TRUE(fabs(cents - 1200.0) < 0.1);
}

TEST(microtuning_ratio_to_cents_fifth) {
    /* 3/2 = ~702 cents (just perfect fifth) */
    double cents = scala_ratio_to_cents(1.5);
    ASSERT_TRUE(fabs(cents - 701.955) < 0.1);
}

/* ============================================================================
 * Scale Loading from String Tests
 * ============================================================================ */

TEST(microtuning_load_from_string) {
    const char *scl_data =
        "! test.scl\n"
        "Test 12-TET\n"
        "12\n"
        "!\n"
        "100.0\n"
        "200.0\n"
        "300.0\n"
        "400.0\n"
        "500.0\n"
        "600.0\n"
        "700.0\n"
        "800.0\n"
        "900.0\n"
        "1000.0\n"
        "1100.0\n"
        "1200.0\n";

    ScalaScale *scale = scala_load_string(scl_data, strlen(scl_data));
    ASSERT_NOT_NULL(scale);

    int length = scala_get_length(scale);
    ASSERT_EQ(length, 12);

    /* Verify octave (degree 12) = 2.0 */
    double octave_ratio = scala_get_ratio(scale, 12);
    ASSERT_TRUE(fabs(octave_ratio - 2.0) < 0.001);

    scala_free(scale);
}

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

TEST(microtuning_load_nonexistent) {
    ScalaScale *scale = scala_load("nonexistent_file.scl");
    ASSERT_NULL(scale);
}

TEST(microtuning_null_scale_ratio) {
    double ratio = scala_get_ratio(NULL, 0);
    ASSERT_TRUE(ratio < 0);  /* Should return -1.0 on error */
}

TEST(microtuning_invalid_degree) {
    ScalaScale *scale = scala_load(JUST_MAJOR_SCL);
    ASSERT_NOT_NULL(scale);

    /* Request invalid degree (negative) */
    double ratio = scala_get_ratio(scale, -1);
    ASSERT_TRUE(ratio < 0);

    scala_free(scale);
}

/* ============================================================================
 * Test Runner
 * ============================================================================ */

test_stats_t test_stats;

BEGIN_TEST_SUITE("Alda Microtuning Tests")

    /* Scale loading */
    RUN_TEST(microtuning_load_scale);
    RUN_TEST(microtuning_scale_description);
    RUN_TEST(microtuning_scale_ratios);

    /* MIDI to frequency */
    RUN_TEST(microtuning_midi_to_freq_root);
    RUN_TEST(microtuning_midi_to_freq_12tet_fifth);
    RUN_TEST(microtuning_midi_to_freq_12tet_major_third);
    RUN_TEST(microtuning_midi_to_freq_12tet_octave_up);
    RUN_TEST(microtuning_midi_to_freq_12tet_octave_down);
    RUN_TEST(microtuning_7note_scale_degree_mapping);
    RUN_TEST(microtuning_just12_chromatic);

    /* Per-part assignment */
    RUN_TEST(microtuning_part_scale_assignment);
    RUN_TEST(microtuning_different_parts_different_scales);
    RUN_TEST(microtuning_part_find_with_scale);

    /* Cents/ratio conversion */
    RUN_TEST(microtuning_cents_to_ratio);
    RUN_TEST(microtuning_cents_to_ratio_octave);
    RUN_TEST(microtuning_ratio_to_cents);
    RUN_TEST(microtuning_ratio_to_cents_fifth);

    /* String loading */
    RUN_TEST(microtuning_load_from_string);

    /* Error handling */
    RUN_TEST(microtuning_load_nonexistent);
    RUN_TEST(microtuning_null_scale_ratio);
    RUN_TEST(microtuning_invalid_degree);

END_TEST_SUITE()
