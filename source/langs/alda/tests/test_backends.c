/**
 * @file test_backends.c
 * @brief Smoke tests for audio backends (TSF, MIDI, Csound).
 *
 * These tests verify basic backend functionality without producing audio:
 * - Initialization and cleanup cycles
 * - State checks (is_open, is_enabled, has_soundfont)
 * - Error handling for missing resources
 * - Safe behavior when called in wrong states
 */

#include "test_framework.h"
#include <alda/context.h>
#include <alda/tsf_backend.h>
#include <alda/midi_backend.h>

/* Only include csound if enabled */
#ifdef BUILD_CSOUND_BACKEND
#include <alda/csound_backend.h>
#endif

/* ============================================================================
 * TSF Backend Tests
 * ============================================================================ */

TEST(tsf_init_cleanup_cycle) {
    /* Init should succeed */
    int result = alda_tsf_init();
    ASSERT_EQ(result, 0);

    /* Cleanup should not crash */
    alda_tsf_cleanup();
}

TEST(tsf_double_init) {
    /* First init */
    int result = alda_tsf_init();
    ASSERT_EQ(result, 0);

    /* Second init should succeed (idempotent) */
    result = alda_tsf_init();
    ASSERT_EQ(result, 0);

    alda_tsf_cleanup();
}

TEST(tsf_cleanup_without_init) {
    /* Cleanup without init should not crash */
    alda_tsf_cleanup();
}

TEST(tsf_no_soundfont_initially) {
    int result = alda_tsf_init();
    ASSERT_EQ(result, 0);

    /* No soundfont loaded initially */
    ASSERT_FALSE(alda_tsf_has_soundfont());
    ASSERT_EQ(alda_tsf_get_preset_count(), 0);

    alda_tsf_cleanup();
}

TEST(tsf_load_nonexistent_soundfont) {
    int result = alda_tsf_init();
    ASSERT_EQ(result, 0);

    /* Loading non-existent file should fail */
    result = alda_tsf_load_soundfont("/nonexistent/path/to/file.sf2");
    ASSERT_EQ(result, -1);
    ASSERT_FALSE(alda_tsf_has_soundfont());

    alda_tsf_cleanup();
}

TEST(tsf_enable_without_soundfont) {
    int result = alda_tsf_init();
    ASSERT_EQ(result, 0);

    /* Enable without soundfont should fail */
    result = alda_tsf_enable();
    ASSERT_EQ(result, -1);
    ASSERT_FALSE(alda_tsf_is_enabled());

    alda_tsf_cleanup();
}

TEST(tsf_is_enabled_initially_false) {
    int result = alda_tsf_init();
    ASSERT_EQ(result, 0);

    /* Should not be enabled initially */
    ASSERT_FALSE(alda_tsf_is_enabled());

    alda_tsf_cleanup();
}

TEST(tsf_disable_when_not_enabled) {
    int result = alda_tsf_init();
    ASSERT_EQ(result, 0);

    /* Disable when not enabled should not crash */
    alda_tsf_disable();
    ASSERT_FALSE(alda_tsf_is_enabled());

    alda_tsf_cleanup();
}

TEST(tsf_all_notes_off_when_disabled) {
    int result = alda_tsf_init();
    ASSERT_EQ(result, 0);

    /* All notes off when disabled should not crash */
    alda_tsf_all_notes_off();

    alda_tsf_cleanup();
}

TEST(tsf_get_preset_name_invalid_index) {
    int result = alda_tsf_init();
    ASSERT_EQ(result, 0);

    /* Invalid index should return NULL */
    ASSERT_NULL(alda_tsf_get_preset_name(-1));
    ASSERT_NULL(alda_tsf_get_preset_name(0));  /* No soundfont loaded */
    ASSERT_NULL(alda_tsf_get_preset_name(1000));

    alda_tsf_cleanup();
}

/* ============================================================================
 * MIDI Backend Tests
 * ============================================================================ */

TEST(midi_init_cleanup_cycle) {
    AldaContext ctx;
    alda_context_init(&ctx);

    /* Init observer */
    alda_midi_init_observer(&ctx);

    /* Cleanup should not crash */
    alda_midi_cleanup(&ctx);

    alda_context_cleanup(&ctx);
}

TEST(midi_double_init) {
    AldaContext ctx;
    alda_context_init(&ctx);

    /* First init */
    alda_midi_init_observer(&ctx);

    /* Second init should not crash (idempotent) */
    alda_midi_init_observer(&ctx);

    alda_midi_cleanup(&ctx);
    alda_context_cleanup(&ctx);
}

TEST(midi_not_open_initially) {
    AldaContext ctx;
    alda_context_init(&ctx);

    alda_midi_init_observer(&ctx);

    /* Not open initially */
    ASSERT_FALSE(alda_midi_is_open(&ctx));

    alda_midi_cleanup(&ctx);
    alda_context_cleanup(&ctx);
}

TEST(midi_close_when_not_open) {
    AldaContext ctx;
    alda_context_init(&ctx);

    alda_midi_init_observer(&ctx);

    /* Close when not open should not crash */
    alda_midi_close(&ctx);
    ASSERT_FALSE(alda_midi_is_open(&ctx));

    alda_midi_cleanup(&ctx);
    alda_context_cleanup(&ctx);
}

TEST(midi_open_invalid_port) {
    AldaContext ctx;
    alda_context_init(&ctx);

    alda_midi_init_observer(&ctx);

    /* Open invalid port index should fail */
    int result = alda_midi_open_port(&ctx, 9999);
    ASSERT_EQ(result, -1);
    ASSERT_FALSE(alda_midi_is_open(&ctx));

    alda_midi_cleanup(&ctx);
    alda_context_cleanup(&ctx);
}

TEST(midi_open_negative_port) {
    AldaContext ctx;
    alda_context_init(&ctx);

    alda_midi_init_observer(&ctx);

    /* Negative port index should fail */
    int result = alda_midi_open_port(&ctx, -1);
    ASSERT_EQ(result, -1);
    ASSERT_FALSE(alda_midi_is_open(&ctx));

    alda_midi_cleanup(&ctx);
    alda_context_cleanup(&ctx);
}

TEST(midi_all_notes_off_when_closed) {
    AldaContext ctx;
    alda_context_init(&ctx);

    alda_midi_init_observer(&ctx);

    /* All notes off when closed should not crash */
    alda_midi_all_notes_off(&ctx);

    alda_midi_cleanup(&ctx);
    alda_context_cleanup(&ctx);
}

TEST(midi_send_when_closed) {
    AldaContext ctx;
    alda_context_init(&ctx);

    alda_midi_init_observer(&ctx);

    /* Send messages when closed should not crash */
    alda_midi_send_note_on(&ctx, 1, 60, 100);
    alda_midi_send_note_off(&ctx, 1, 60);
    alda_midi_send_program(&ctx, 1, 0);
    alda_midi_send_cc(&ctx, 1, 7, 100);

    alda_midi_cleanup(&ctx);
    alda_context_cleanup(&ctx);
}

TEST(midi_list_ports_no_crash) {
    AldaContext ctx;
    alda_context_init(&ctx);

    alda_midi_init_observer(&ctx);

    /* List ports should not crash (output goes to stdout) */
    alda_midi_list_ports(&ctx);

    alda_midi_cleanup(&ctx);
    alda_context_cleanup(&ctx);
}

TEST(midi_open_by_name_nonexistent) {
    AldaContext ctx;
    alda_context_init(&ctx);

    alda_midi_init_observer(&ctx);

    /* Open by nonexistent name - behavior depends on implementation */
    /* May create virtual port or fail - either is valid */
    int result = alda_midi_open_by_name(&ctx, "NonExistentPortName123456");
    /* Just verify it doesn't crash, result depends on platform */
    (void)result;

    alda_midi_close(&ctx);
    alda_midi_cleanup(&ctx);
    alda_context_cleanup(&ctx);
}

/* ============================================================================
 * Csound Backend Tests (conditional)
 * ============================================================================ */

#ifdef BUILD_CSOUND_BACKEND

TEST(csound_init_cleanup_cycle) {
    /* Init should succeed */
    int result = alda_csound_init();
    ASSERT_EQ(result, 0);

    /* Cleanup should not crash */
    alda_csound_cleanup();
}

TEST(csound_double_init) {
    /* First init */
    int result = alda_csound_init();
    ASSERT_EQ(result, 0);

    /* Second init should succeed (idempotent) */
    result = alda_csound_init();
    ASSERT_EQ(result, 0);

    alda_csound_cleanup();
}

TEST(csound_cleanup_without_init) {
    /* Cleanup without init should not crash */
    alda_csound_cleanup();
}

TEST(csound_no_instruments_initially) {
    int result = alda_csound_init();
    ASSERT_EQ(result, 0);

    /* No instruments loaded initially */
    ASSERT_FALSE(alda_csound_has_instruments());

    alda_csound_cleanup();
}

TEST(csound_load_nonexistent_csd) {
    int result = alda_csound_init();
    ASSERT_EQ(result, 0);

    /* Loading non-existent file should fail */
    result = alda_csound_load_csd("/nonexistent/path/to/file.csd");
    ASSERT_EQ(result, -1);
    ASSERT_FALSE(alda_csound_has_instruments());

    alda_csound_cleanup();
}

TEST(csound_enable_without_instruments) {
    int result = alda_csound_init();
    ASSERT_EQ(result, 0);

    /* Enable without instruments should fail */
    result = alda_csound_enable();
    ASSERT_EQ(result, -1);
    ASSERT_FALSE(alda_csound_is_enabled());

    alda_csound_cleanup();
}

TEST(csound_is_enabled_initially_false) {
    int result = alda_csound_init();
    ASSERT_EQ(result, 0);

    /* Should not be enabled initially */
    ASSERT_FALSE(alda_csound_is_enabled());

    alda_csound_cleanup();
}

TEST(csound_disable_when_not_enabled) {
    int result = alda_csound_init();
    ASSERT_EQ(result, 0);

    /* Disable when not enabled should not crash */
    alda_csound_disable();
    ASSERT_FALSE(alda_csound_is_enabled());

    alda_csound_cleanup();
}

TEST(csound_all_notes_off_when_disabled) {
    int result = alda_csound_init();
    ASSERT_EQ(result, 0);

    /* All notes off when disabled should not crash */
    alda_csound_all_notes_off();

    alda_csound_cleanup();
}

TEST(csound_get_sample_rate_uninitialized) {
    /* Sample rate before init should be 0 */
    ASSERT_EQ(alda_csound_get_sample_rate(), 0);
}

TEST(csound_get_channels_uninitialized) {
    /* Channels before init should be 0 */
    ASSERT_EQ(alda_csound_get_channels(), 0);
}

TEST(csound_playback_not_active_initially) {
    int result = alda_csound_init();
    ASSERT_EQ(result, 0);

    /* Playback should not be active initially */
    ASSERT_FALSE(alda_csound_playback_active());

    alda_csound_cleanup();
}

TEST(csound_stop_playback_when_not_playing) {
    int result = alda_csound_init();
    ASSERT_EQ(result, 0);

    /* Stop playback when not playing should not crash */
    alda_csound_stop_playback();

    alda_csound_cleanup();
}

#endif /* BUILD_CSOUND_BACKEND */

/* ============================================================================
 * Test Runner
 * ============================================================================ */


BEGIN_TEST_SUITE("Audio Backend Smoke Tests")

    /* TSF Backend tests */
    RUN_TEST(tsf_init_cleanup_cycle);
    RUN_TEST(tsf_double_init);
    RUN_TEST(tsf_cleanup_without_init);
    RUN_TEST(tsf_no_soundfont_initially);
    RUN_TEST(tsf_load_nonexistent_soundfont);
    RUN_TEST(tsf_enable_without_soundfont);
    RUN_TEST(tsf_is_enabled_initially_false);
    RUN_TEST(tsf_disable_when_not_enabled);
    RUN_TEST(tsf_all_notes_off_when_disabled);
    RUN_TEST(tsf_get_preset_name_invalid_index);

    /* MIDI Backend tests */
    RUN_TEST(midi_init_cleanup_cycle);
    RUN_TEST(midi_double_init);
    RUN_TEST(midi_not_open_initially);
    RUN_TEST(midi_close_when_not_open);
    RUN_TEST(midi_open_invalid_port);
    RUN_TEST(midi_open_negative_port);
    RUN_TEST(midi_all_notes_off_when_closed);
    RUN_TEST(midi_send_when_closed);
    RUN_TEST(midi_list_ports_no_crash);
    RUN_TEST(midi_open_by_name_nonexistent);

#ifdef BUILD_CSOUND_BACKEND
    /* Csound Backend tests */
    RUN_TEST(csound_init_cleanup_cycle);
    RUN_TEST(csound_double_init);
    RUN_TEST(csound_cleanup_without_init);
    RUN_TEST(csound_no_instruments_initially);
    RUN_TEST(csound_load_nonexistent_csd);
    RUN_TEST(csound_enable_without_instruments);
    RUN_TEST(csound_is_enabled_initially_false);
    RUN_TEST(csound_disable_when_not_enabled);
    RUN_TEST(csound_all_notes_off_when_disabled);
    RUN_TEST(csound_get_sample_rate_uninitialized);
    RUN_TEST(csound_get_channels_uninitialized);
    RUN_TEST(csound_playback_not_active_initially);
    RUN_TEST(csound_stop_playback_when_not_playing);
#endif

END_TEST_SUITE()
