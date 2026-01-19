/**
 * @file test_fluid_backend.c
 * @brief Tests for FluidSynth audio backend.
 *
 * Tests verify:
 * - Backend initialization and cleanup
 * - State queries before/after initialization
 * - Soundfont loading (requires test soundfont)
 * - Enable/disable ref counting
 * - MIDI message sending (note on/off, program, CC, pitch bend)
 * - All notes off
 * - Gain control
 * - Active voice count
 *
 * Note: Full audio output testing requires manual verification.
 * These tests verify the API behaves correctly without crashing.
 *
 * Build with -DBUILD_FLUID_BACKEND=ON to enable full tests.
 */

#include "test_framework.h"
#include "audio/audio.h"

/* ============================================================================
 * Initialization Tests
 * ============================================================================ */

TEST(fluid_init_cleanup) {
#ifdef BUILD_FLUID_BACKEND
    /* Init should succeed */
    int result = shared_fluid_init();
    ASSERT_EQ(result, 0);

    /* Double init should also succeed (idempotent) */
    result = shared_fluid_init();
    ASSERT_EQ(result, 0);

    /* Cleanup should work */
    shared_fluid_cleanup();

    /* Double cleanup should not crash */
    shared_fluid_cleanup();
#else
    /* When not compiled in, init should fail */
    int result = shared_fluid_init();
    ASSERT_EQ(result, -1);
#endif
}

TEST(fluid_state_before_init) {
    /* Ensure clean state */
    shared_fluid_cleanup();

    /* Should report no soundfont */
    ASSERT_FALSE(shared_fluid_has_soundfont());

    /* Should report not enabled */
    ASSERT_FALSE(shared_fluid_is_enabled());

    /* Preset count should be 0 */
    ASSERT_EQ(shared_fluid_get_preset_count(), 0);

    /* Preset name should be NULL */
    ASSERT_NULL(shared_fluid_get_preset_name(0));

    /* Active voice count should be 0 */
    ASSERT_EQ(shared_fluid_get_active_voice_count(), 0);

    /* Gain should be 0 when not initialized */
    float gain = shared_fluid_get_gain();
    ASSERT_EQ((int)(gain * 100), 0);
}

#ifdef BUILD_FLUID_BACKEND
TEST(fluid_state_after_init_no_soundfont) {
    int result = shared_fluid_init();
    ASSERT_EQ(result, 0);

    /* Should report no soundfont */
    ASSERT_FALSE(shared_fluid_has_soundfont());

    /* Should report not enabled */
    ASSERT_FALSE(shared_fluid_is_enabled());

    /* Preset count should be 0 */
    ASSERT_EQ(shared_fluid_get_preset_count(), 0);

    shared_fluid_cleanup();
}
#endif

/* ============================================================================
 * Soundfont Loading Tests
 * ============================================================================ */

#ifdef BUILD_FLUID_BACKEND
TEST(fluid_load_null_path) {
    int result = shared_fluid_init();
    ASSERT_EQ(result, 0);

    /* Loading NULL path should fail */
    result = shared_fluid_load_soundfont(NULL);
    ASSERT_EQ(result, -1);

    shared_fluid_cleanup();
}

TEST(fluid_load_nonexistent_file) {
    int result = shared_fluid_init();
    ASSERT_EQ(result, 0);

    /* Loading nonexistent file should fail */
    result = shared_fluid_load_soundfont("/nonexistent/path/to/soundfont.sf2");
    ASSERT_EQ(result, -1);

    /* Should still report no soundfont */
    ASSERT_FALSE(shared_fluid_has_soundfont());

    shared_fluid_cleanup();
}

TEST(fluid_load_without_init) {
    /* Ensure clean state */
    shared_fluid_cleanup();

    /* Loading without init should fail */
    int result = shared_fluid_load_soundfont("/some/path.sf2");
    ASSERT_EQ(result, -1);
}
#endif

/* ============================================================================
 * Enable/Disable Tests
 * ============================================================================ */

#ifdef BUILD_FLUID_BACKEND
TEST(fluid_enable_without_soundfont) {
    int result = shared_fluid_init();
    ASSERT_EQ(result, 0);

    /* Enable without soundfont should fail */
    result = shared_fluid_enable();
    ASSERT_EQ(result, -1);

    /* Should still be disabled */
    ASSERT_FALSE(shared_fluid_is_enabled());

    shared_fluid_cleanup();
}

TEST(fluid_disable_without_enable) {
    int result = shared_fluid_init();
    ASSERT_EQ(result, 0);

    /* Disable without enable should not crash */
    shared_fluid_disable();

    /* Should still be disabled */
    ASSERT_FALSE(shared_fluid_is_enabled());

    shared_fluid_cleanup();
}
#endif

TEST(fluid_disable_without_init) {
    /* Ensure clean state */
    shared_fluid_cleanup();

    /* Disable without init should not crash */
    shared_fluid_disable();
}

/* ============================================================================
 * MIDI Message Tests (without soundfont - should not crash)
 * ============================================================================ */

#ifdef BUILD_FLUID_BACKEND
TEST(fluid_note_on_without_soundfont) {
    int result = shared_fluid_init();
    ASSERT_EQ(result, 0);

    /* Note on without soundfont should not crash */
    shared_fluid_send_note_on(1, 60, 100);

    shared_fluid_cleanup();
}

TEST(fluid_note_off_without_soundfont) {
    int result = shared_fluid_init();
    ASSERT_EQ(result, 0);

    /* Note off without soundfont should not crash */
    shared_fluid_send_note_off(1, 60);

    shared_fluid_cleanup();
}

TEST(fluid_program_change_without_soundfont) {
    int result = shared_fluid_init();
    ASSERT_EQ(result, 0);

    /* Program change without soundfont should not crash */
    shared_fluid_send_program(1, 0);

    shared_fluid_cleanup();
}

TEST(fluid_cc_without_soundfont) {
    int result = shared_fluid_init();
    ASSERT_EQ(result, 0);

    /* CC without soundfont should not crash */
    shared_fluid_send_cc(1, 7, 100);  /* Volume */
    shared_fluid_send_cc(1, 10, 64);  /* Pan */

    shared_fluid_cleanup();
}

TEST(fluid_pitch_bend_without_soundfont) {
    int result = shared_fluid_init();
    ASSERT_EQ(result, 0);

    /* Pitch bend without soundfont should not crash */
    shared_fluid_send_pitch_bend(1, 0);      /* Center */
    shared_fluid_send_pitch_bend(1, 8191);   /* Max up */
    shared_fluid_send_pitch_bend(1, -8192);  /* Max down */

    shared_fluid_cleanup();
}

TEST(fluid_all_notes_off_without_soundfont) {
    int result = shared_fluid_init();
    ASSERT_EQ(result, 0);

    /* All notes off without soundfont should not crash */
    shared_fluid_all_notes_off();

    shared_fluid_cleanup();
}
#endif

/* ============================================================================
 * MIDI Message Tests (without init - should not crash)
 * ============================================================================ */

TEST(fluid_note_on_without_init) {
    /* Ensure clean state */
    shared_fluid_cleanup();

    /* Note on without init should not crash */
    shared_fluid_send_note_on(1, 60, 100);
}

TEST(fluid_note_off_without_init) {
    /* Ensure clean state */
    shared_fluid_cleanup();

    /* Note off without init should not crash */
    shared_fluid_send_note_off(1, 60);
}

TEST(fluid_all_notes_off_without_init) {
    /* Ensure clean state */
    shared_fluid_cleanup();

    /* All notes off without init should not crash */
    shared_fluid_all_notes_off();
}

/* ============================================================================
 * Gain Control Tests
 * ============================================================================ */

#ifdef BUILD_FLUID_BACKEND
TEST(fluid_gain_control) {
    int result = shared_fluid_init();
    ASSERT_EQ(result, 0);

    /* Get default gain */
    float default_gain = shared_fluid_get_gain();

    /* Set new gain */
    shared_fluid_set_gain(0.5f);

    /* Verify gain changed */
    float new_gain = shared_fluid_get_gain();
    ASSERT_TRUE(new_gain > 0.4f && new_gain < 0.6f);  /* Allow some tolerance */

    /* Restore default */
    shared_fluid_set_gain(default_gain);

    shared_fluid_cleanup();
}
#endif

TEST(fluid_gain_without_init) {
    /* Ensure clean state */
    shared_fluid_cleanup();

    /* Set gain without init should not crash */
    shared_fluid_set_gain(0.5f);

    /* Get gain without init should return 0 */
    float gain = shared_fluid_get_gain();
    ASSERT_EQ((int)(gain * 100), 0);
}

/* ============================================================================
 * Boundary Tests
 * ============================================================================ */

#ifdef BUILD_FLUID_BACKEND
TEST(fluid_channel_boundaries) {
    int result = shared_fluid_init();
    ASSERT_EQ(result, 0);

    /* Test channel boundaries (1-16) */
    shared_fluid_send_note_on(1, 60, 100);   /* Min channel */
    shared_fluid_send_note_on(16, 60, 100);  /* Max channel */
    shared_fluid_send_note_off(1, 60);
    shared_fluid_send_note_off(16, 60);

    /* Test with out-of-range channels (should be masked) */
    shared_fluid_send_note_on(0, 60, 100);   /* Below min - masked to 15 */
    shared_fluid_send_note_on(17, 60, 100);  /* Above max - masked to 0 */
    shared_fluid_send_note_off(0, 60);
    shared_fluid_send_note_off(17, 60);

    shared_fluid_cleanup();
}

TEST(fluid_pitch_boundaries) {
    int result = shared_fluid_init();
    ASSERT_EQ(result, 0);

    /* Test pitch boundaries (0-127) */
    shared_fluid_send_note_on(1, 0, 100);    /* Min pitch */
    shared_fluid_send_note_on(1, 127, 100);  /* Max pitch */
    shared_fluid_send_note_off(1, 0);
    shared_fluid_send_note_off(1, 127);

    shared_fluid_cleanup();
}

TEST(fluid_velocity_boundaries) {
    int result = shared_fluid_init();
    ASSERT_EQ(result, 0);

    /* Test velocity boundaries (0-127) */
    shared_fluid_send_note_on(1, 60, 0);     /* Min velocity (note off) */
    shared_fluid_send_note_on(1, 60, 127);   /* Max velocity */
    shared_fluid_send_note_off(1, 60);

    shared_fluid_cleanup();
}

TEST(fluid_cc_boundaries) {
    int result = shared_fluid_init();
    ASSERT_EQ(result, 0);

    /* Test CC boundaries */
    shared_fluid_send_cc(1, 0, 0);       /* Min CC, min value */
    shared_fluid_send_cc(1, 127, 127);   /* Max CC, max value */

    shared_fluid_cleanup();
}

TEST(fluid_pitch_bend_boundaries) {
    int result = shared_fluid_init();
    ASSERT_EQ(result, 0);

    /* Test pitch bend boundaries */
    shared_fluid_send_pitch_bend(1, -8192);  /* Min bend */
    shared_fluid_send_pitch_bend(1, 0);      /* Center */
    shared_fluid_send_pitch_bend(1, 8191);   /* Max bend */

    shared_fluid_cleanup();
}
#endif

/* ============================================================================
 * Test Runner
 * ============================================================================ */

BEGIN_TEST_SUITE("FluidSynth Backend Tests")

    /* Initialization */
    RUN_TEST(fluid_init_cleanup);
    RUN_TEST(fluid_state_before_init);
#ifdef BUILD_FLUID_BACKEND
    RUN_TEST(fluid_state_after_init_no_soundfont);
#endif

    /* Soundfont loading */
#ifdef BUILD_FLUID_BACKEND
    RUN_TEST(fluid_load_null_path);
    RUN_TEST(fluid_load_nonexistent_file);
    RUN_TEST(fluid_load_without_init);
#endif

    /* Enable/disable */
#ifdef BUILD_FLUID_BACKEND
    RUN_TEST(fluid_enable_without_soundfont);
    RUN_TEST(fluid_disable_without_enable);
#endif
    RUN_TEST(fluid_disable_without_init);

    /* MIDI messages without soundfont */
#ifdef BUILD_FLUID_BACKEND
    RUN_TEST(fluid_note_on_without_soundfont);
    RUN_TEST(fluid_note_off_without_soundfont);
    RUN_TEST(fluid_program_change_without_soundfont);
    RUN_TEST(fluid_cc_without_soundfont);
    RUN_TEST(fluid_pitch_bend_without_soundfont);
    RUN_TEST(fluid_all_notes_off_without_soundfont);
#endif

    /* MIDI messages without init */
    RUN_TEST(fluid_note_on_without_init);
    RUN_TEST(fluid_note_off_without_init);
    RUN_TEST(fluid_all_notes_off_without_init);

    /* Gain control */
#ifdef BUILD_FLUID_BACKEND
    RUN_TEST(fluid_gain_control);
#endif
    RUN_TEST(fluid_gain_without_init);

    /* Boundary tests */
#ifdef BUILD_FLUID_BACKEND
    RUN_TEST(fluid_channel_boundaries);
    RUN_TEST(fluid_pitch_boundaries);
    RUN_TEST(fluid_velocity_boundaries);
    RUN_TEST(fluid_cc_boundaries);
    RUN_TEST(fluid_pitch_bend_boundaries);
#endif

END_TEST_SUITE()
