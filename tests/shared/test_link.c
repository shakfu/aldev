/**
 * @file test_link.c
 * @brief Tests for Ableton Link shared backend.
 *
 * Tests verify basic Link functionality:
 * - Initialization and cleanup cycles
 * - Enable/disable state
 * - Tempo get/set
 * - Peer count (will be 0 in tests)
 * - Start/stop sync
 */

#include "test_framework.h"
#include "link/link.h"

/* ============================================================================
 * Initialization Tests
 * ============================================================================ */

TEST(link_init_cleanup_cycle) {
    /* Init should succeed */
    int result = shared_link_init(120.0);
    ASSERT_EQ(result, 0);
    ASSERT_TRUE(shared_link_is_initialized());

    /* Cleanup should not crash */
    shared_link_cleanup();
    ASSERT_FALSE(shared_link_is_initialized());
}

TEST(link_double_init) {
    /* First init */
    int result = shared_link_init(120.0);
    ASSERT_EQ(result, 0);

    /* Second init should succeed (idempotent) */
    result = shared_link_init(140.0);
    ASSERT_EQ(result, 0);

    shared_link_cleanup();
}

TEST(link_cleanup_without_init) {
    /* Cleanup without init should not crash */
    shared_link_cleanup();
}

/* ============================================================================
 * Enable/Disable Tests
 * ============================================================================ */

TEST(link_disabled_initially) {
    int result = shared_link_init(120.0);
    ASSERT_EQ(result, 0);

    /* Link should be disabled initially */
    ASSERT_FALSE(shared_link_is_enabled());

    shared_link_cleanup();
}

TEST(link_enable_disable) {
    int result = shared_link_init(120.0);
    ASSERT_EQ(result, 0);

    /* Enable Link */
    shared_link_enable(1);
    ASSERT_TRUE(shared_link_is_enabled());

    /* Disable Link */
    shared_link_enable(0);
    ASSERT_FALSE(shared_link_is_enabled());

    shared_link_cleanup();
}

TEST(link_enable_when_not_initialized) {
    /* Should not crash when not initialized */
    shared_link_enable(1);
    ASSERT_FALSE(shared_link_is_enabled());
}

/* ============================================================================
 * Tempo Tests
 * ============================================================================ */

TEST(link_initial_tempo) {
    int result = shared_link_init(120.0);
    ASSERT_EQ(result, 0);

    /* Should have initial tempo */
    double tempo = shared_link_get_tempo();
    ASSERT_TRUE(tempo > 0);

    shared_link_cleanup();
}

TEST(link_set_tempo) {
    int result = shared_link_init(120.0);
    ASSERT_EQ(result, 0);

    shared_link_enable(1);

    /* Set tempo */
    shared_link_set_tempo(140.0);
    double tempo = shared_link_get_tempo();
    /* Allow small floating point difference */
    ASSERT_TRUE(tempo >= 139.0 && tempo <= 141.0);

    shared_link_cleanup();
}

TEST(link_effective_tempo_disabled) {
    int result = shared_link_init(120.0);
    ASSERT_EQ(result, 0);

    /* When disabled, should return fallback */
    double tempo = shared_link_effective_tempo(90.0);
    ASSERT_TRUE(tempo >= 89.0 && tempo <= 91.0);

    shared_link_cleanup();
}

TEST(link_effective_tempo_enabled) {
    int result = shared_link_init(120.0);
    ASSERT_EQ(result, 0);

    shared_link_enable(1);
    shared_link_set_tempo(150.0);

    /* When enabled, should return Link tempo */
    double tempo = shared_link_effective_tempo(90.0);
    ASSERT_TRUE(tempo >= 149.0 && tempo <= 151.0);

    shared_link_cleanup();
}

/* ============================================================================
 * Peer Tests
 * ============================================================================ */

TEST(link_no_peers_in_test) {
    int result = shared_link_init(120.0);
    ASSERT_EQ(result, 0);

    shared_link_enable(1);

    /* In test environment, we have no peers */
    uint64_t peers = shared_link_num_peers();
    ASSERT_EQ(peers, 0);

    shared_link_cleanup();
}

/* ============================================================================
 * Start/Stop Sync Tests
 * ============================================================================ */

TEST(link_start_stop_sync_disabled_initially) {
    int result = shared_link_init(120.0);
    ASSERT_EQ(result, 0);

    ASSERT_FALSE(shared_link_is_start_stop_sync_enabled());

    shared_link_cleanup();
}

TEST(link_enable_start_stop_sync) {
    int result = shared_link_init(120.0);
    ASSERT_EQ(result, 0);

    shared_link_enable_start_stop_sync(1);
    ASSERT_TRUE(shared_link_is_start_stop_sync_enabled());

    shared_link_enable_start_stop_sync(0);
    ASSERT_FALSE(shared_link_is_start_stop_sync_enabled());

    shared_link_cleanup();
}

TEST(link_playing_state) {
    int result = shared_link_init(120.0);
    ASSERT_EQ(result, 0);

    shared_link_enable(1);

    /* Initially not playing */
    ASSERT_FALSE(shared_link_is_playing());

    /* Set playing */
    shared_link_set_playing(1);
    ASSERT_TRUE(shared_link_is_playing());

    /* Stop playing */
    shared_link_set_playing(0);
    ASSERT_FALSE(shared_link_is_playing());

    shared_link_cleanup();
}

/* ============================================================================
 * Beat/Phase Tests
 * ============================================================================ */

TEST(link_get_beat) {
    int result = shared_link_init(120.0);
    ASSERT_EQ(result, 0);

    shared_link_enable(1);

    /* Should return a beat value (may be 0 or any value) */
    double beat = shared_link_get_beat(4.0);
    /* Just verify it doesn't crash and returns a reasonable value */
    ASSERT_TRUE(beat >= 0.0);

    shared_link_cleanup();
}

TEST(link_get_phase) {
    int result = shared_link_init(120.0);
    ASSERT_EQ(result, 0);

    shared_link_enable(1);

    /* Phase should be in range [0, quantum) */
    double phase = shared_link_get_phase(4.0);
    ASSERT_TRUE(phase >= 0.0 && phase < 4.0);

    shared_link_cleanup();
}

/* ============================================================================
 * Test Runner
 * ============================================================================ */

BEGIN_TEST_SUITE("Ableton Link Tests")

    /* Initialization */
    RUN_TEST(link_init_cleanup_cycle);
    RUN_TEST(link_double_init);
    RUN_TEST(link_cleanup_without_init);

    /* Enable/Disable */
    RUN_TEST(link_disabled_initially);
    RUN_TEST(link_enable_disable);
    RUN_TEST(link_enable_when_not_initialized);

    /* Tempo */
    RUN_TEST(link_initial_tempo);
    RUN_TEST(link_set_tempo);
    RUN_TEST(link_effective_tempo_disabled);
    RUN_TEST(link_effective_tempo_enabled);

    /* Peers */
    RUN_TEST(link_no_peers_in_test);

    /* Start/Stop Sync */
    RUN_TEST(link_start_stop_sync_disabled_initially);
    RUN_TEST(link_enable_start_stop_sync);
    RUN_TEST(link_playing_state);

    /* Beat/Phase */
    RUN_TEST(link_get_beat);
    RUN_TEST(link_get_phase);

END_TEST_SUITE()
