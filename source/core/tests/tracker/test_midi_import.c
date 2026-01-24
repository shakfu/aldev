/**
 * test_midi_import.c - Tests for MIDI file import functionality
 */

#include "test_framework.h"
#include "tracker_midi_import.h"
#include "tracker_model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*============================================================================
 * Import Options Tests
 *============================================================================*/

TEST(import_options_defaults) {
    TrackerMidiImportOptions opts;
    tracker_midi_import_options_init(&opts);

    ASSERT_EQ(opts.rows_per_beat, 4);
    ASSERT_EQ(opts.ticks_per_row, 6);
    ASSERT_EQ(opts.pattern_rows, 64);
    ASSERT_EQ(opts.quantize_strength, 100);
    ASSERT_EQ(opts.velocity_threshold, 1);
    ASSERT_EQ(opts.include_velocity, 1);
    ASSERT_EQ(opts.split_by_channel, 1);
    ASSERT_EQ(opts.max_tracks, 16);
}

/*============================================================================
 * Error Handling Tests
 *============================================================================*/

TEST(import_null_filename) {
    TrackerSong* song = tracker_midi_import(NULL, NULL);
    ASSERT_NULL(song);

    const char* err = tracker_midi_import_error();
    ASSERT_NOT_NULL(err);
    ASSERT_TRUE(strstr(err, "filename") != NULL);
}

TEST(import_empty_filename) {
    TrackerSong* song = tracker_midi_import("", NULL);
    ASSERT_NULL(song);

    const char* err = tracker_midi_import_error();
    ASSERT_NOT_NULL(err);
    ASSERT_TRUE(strstr(err, "filename") != NULL);
}

TEST(import_nonexistent_file) {
    TrackerSong* song = tracker_midi_import("/tmp/does_not_exist_xyz123.mid", NULL);
    ASSERT_NULL(song);

    const char* err = tracker_midi_import_error();
    ASSERT_NOT_NULL(err);
    ASSERT_TRUE(strstr(err, "Failed") != NULL || strstr(err, "read") != NULL);
}

/*============================================================================
 * Song Creation Tests
 *============================================================================*/

TEST(roundtrip_basic) {
    /* Create a simple test song to verify song model works */
    TrackerSong* original = tracker_song_new("Roundtrip Test");
    ASSERT_NOT_NULL(original);

    original->bpm = 140;
    original->rows_per_beat = 4;
    original->ticks_per_row = 6;

    /* Create a pattern with some notes */
    TrackerPattern* pattern = tracker_pattern_new(16, 2, "Test Pattern");
    ASSERT_NOT_NULL(pattern);

    /* Set up track channels */
    pattern->tracks[0].default_channel = 0;
    pattern->tracks[1].default_channel = 1;

    /* Add some notes */
    TrackerCell* cell = tracker_pattern_get_cell(pattern, 0, 0);
    tracker_cell_set_expression(cell, "C4", NULL);

    cell = tracker_pattern_get_cell(pattern, 4, 0);
    tracker_cell_set_expression(cell, "E4", NULL);

    cell = tracker_pattern_get_cell(pattern, 8, 0);
    tracker_cell_set_expression(cell, "G4", NULL);

    cell = tracker_pattern_get_cell(pattern, 0, 1);
    tracker_cell_set_expression(cell, "C3", NULL);

    tracker_song_add_pattern(original, pattern);

    /* Validate song structure */
    ASSERT_EQ(original->num_patterns, 1);
    ASSERT_EQ(original->bpm, 140);

    TrackerPattern* p = tracker_song_get_pattern(original, 0);
    ASSERT_NOT_NULL(p);
    ASSERT_EQ(p->num_tracks, 2);
    ASSERT_EQ(p->num_rows, 16);

    tracker_song_free(original);
}

/*============================================================================
 * Test Runner
 *============================================================================*/

/* Global test stats - required by test framework */
test_stats_t test_stats;

BEGIN_TEST_SUITE("MIDI Import")
    /* Options tests */
    RUN_TEST(import_options_defaults);

    /* Error handling tests */
    RUN_TEST(import_null_filename);
    RUN_TEST(import_empty_filename);
    RUN_TEST(import_nonexistent_file);

    /* Song creation tests */
    RUN_TEST(roundtrip_basic);
END_TEST_SUITE()
