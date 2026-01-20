/**
 * @file test_integration.c
 * @brief Integration tests for end-to-end Alda parsing and MIDI generation.
 *
 * These tests verify complete workflows:
 * - Parse Alda source code
 * - Generate MIDI events
 * - Verify correct output
 *
 * Focus on combinations of features and edge cases not covered
 * by the shared_suite unit tests.
 */

#include "test_framework.h"
#include <alda/alda.h>
#include <alda/context.h>
#include <alda/interpreter.h>
#include <alda/scheduler.h>
#include <string.h>

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static void test_context_init(AldaContext* ctx) {
    alda_context_init(ctx);
    alda_set_no_sleep(ctx, 1);
}

static int count_events(AldaContext* ctx, AldaEventType type) {
    int count = 0;
    for (int i = 0; i < ctx->event_count; i++) {
        if (ctx->events[i].type == type) {
            count++;
        }
    }
    return count;
}

static AldaScheduledEvent* find_note_on(AldaContext* ctx, int pitch) {
    for (int i = 0; i < ctx->event_count; i++) {
        if (ctx->events[i].type == ALDA_EVT_NOTE_ON &&
            ctx->events[i].data1 == pitch) {
            return &ctx->events[i];
        }
    }
    return NULL;
}

static AldaScheduledEvent* find_event(AldaContext* ctx, AldaEventType type, int skip) {
    for (int i = 0; i < ctx->event_count; i++) {
        if (ctx->events[i].type == type) {
            if (skip == 0) {
                return &ctx->events[i];
            }
            skip--;
        }
    }
    return NULL;
}

/* Count note-on events on a specific channel */
static int count_notes_on_channel(AldaContext* ctx, int channel) {
    int count = 0;
    for (int i = 0; i < ctx->event_count; i++) {
        if (ctx->events[i].type == ALDA_EVT_NOTE_ON &&
            ctx->events[i].channel == channel) {
            count++;
        }
    }
    return count;
}

/* ============================================================================
 * Multi-Part Integration Tests
 * ============================================================================ */

TEST(integration_two_parts_simultaneous) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* Two instruments playing simultaneously - use distinct pitches */
    int result = alda_interpret_string(&ctx,
        "piano: c d e\n"
        "violin: o5 g a b", "test");  /* Higher octave to avoid overlap */
    ASSERT_EQ(result, 0);

    /* Each part plays 3 notes = 6 total */
    ASSERT_EQ(count_events(&ctx, ALDA_EVT_NOTE_ON), 6);

    /* Piano is channel 0, violin is channel 1 */
    ASSERT_EQ(count_notes_on_channel(&ctx, 0), 3);
    ASSERT_EQ(count_notes_on_channel(&ctx, 1), 3);

    /* Both start at the same time (tick 0) */
    AldaScheduledEvent* piano_c = find_note_on(&ctx, 60);  /* C4 */
    AldaScheduledEvent* violin_g = find_note_on(&ctx, 79); /* G5 */
    ASSERT_NOT_NULL(piano_c);
    ASSERT_NOT_NULL(violin_g);
    ASSERT_EQ(piano_c->tick, 0);
    ASSERT_EQ(violin_g->tick, 0);

    alda_context_cleanup(&ctx);
}

TEST(integration_part_switching) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* Switch between parts mid-score */
    int result = alda_interpret_string(&ctx,
        "piano: c d\n"
        "violin: e f\n"
        "piano: g a", "test");
    ASSERT_EQ(result, 0);

    /* Piano plays c d g a = 4 notes, violin plays e f = 2 notes */
    ASSERT_EQ(count_notes_on_channel(&ctx, 0), 4);
    ASSERT_EQ(count_notes_on_channel(&ctx, 1), 2);

    alda_context_cleanup(&ctx);
}

/* ============================================================================
 * Variable and Reference Integration Tests
 * ============================================================================ */

TEST(integration_variable_reuse) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* Variable used multiple times across parts */
    int result = alda_interpret_string(&ctx,
        "motif = c d e\n"
        "piano: motif motif\n"
        "violin: motif", "test");
    ASSERT_EQ(result, 0);

    /* Piano plays motif twice = 6 notes, violin once = 3 notes */
    ASSERT_EQ(count_notes_on_channel(&ctx, 0), 6);
    ASSERT_EQ(count_notes_on_channel(&ctx, 1), 3);

    alda_context_cleanup(&ctx);
}

TEST(integration_nested_brackets_and_repeat) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* Nested structures */
    int result = alda_interpret_string(&ctx,
        "piano: [c d [e f]*2]*2", "test");
    ASSERT_EQ(result, 0);

    /* [e f]*2 = 4 notes, [c d + 4]*2 = (2+4)*2 = 12 notes */
    ASSERT_EQ(count_events(&ctx, ALDA_EVT_NOTE_ON), 12);

    alda_context_cleanup(&ctx);
}

/* ============================================================================
 * Octave Manipulation Integration Tests
 * ============================================================================ */

TEST(integration_octave_across_parts) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* Different octaves in different parts */
    int result = alda_interpret_string(&ctx,
        "piano: o3 c\n"
        "violin: o6 c", "test");
    ASSERT_EQ(result, 0);

    /* Piano C3 = 48, Violin C6 = 84 */
    AldaScheduledEvent* piano_c = find_note_on(&ctx, 48);
    AldaScheduledEvent* violin_c = find_note_on(&ctx, 84);
    ASSERT_NOT_NULL(piano_c);
    ASSERT_NOT_NULL(violin_c);

    alda_context_cleanup(&ctx);
}

TEST(integration_octave_shifts_in_chord) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* Octave shifts within chord notation */
    /* c/>e/<g means: C4, octave up, E5, octave down, G4 */
    int result = alda_interpret_string(&ctx,
        "piano: c/>e/<g", "test");
    ASSERT_EQ(result, 0);

    /* Three notes in chord */
    ASSERT_EQ(count_events(&ctx, ALDA_EVT_NOTE_ON), 3);

    /* C4=60, E5=76, G4=67 */
    ASSERT_NOT_NULL(find_note_on(&ctx, 60));
    ASSERT_NOT_NULL(find_note_on(&ctx, 76));
    ASSERT_NOT_NULL(find_note_on(&ctx, 67));

    alda_context_cleanup(&ctx);
}

/* ============================================================================
 * Voice (Polyphony) Integration Tests
 * ============================================================================ */

TEST(integration_voices_with_different_rhythms) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* Two voices with different note counts */
    int result = alda_interpret_string(&ctx,
        "piano: V1: c d e f V2: g2 a2", "test");
    ASSERT_EQ(result, 0);

    /* V1: 4 notes, V2: 2 notes = 6 total on same channel */
    ASSERT_EQ(count_events(&ctx, ALDA_EVT_NOTE_ON), 6);
    ASSERT_EQ(count_notes_on_channel(&ctx, 0), 6);

    alda_context_cleanup(&ctx);
}

/* ============================================================================
 * Timing and Tempo Integration Tests
 * ============================================================================ */

TEST(integration_tempo_affects_all_parts) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* Tempo set before parts */
    int result = alda_interpret_string(&ctx,
        "(tempo 60)\n"
        "piano: c d\n"
        "violin: e f", "test");
    ASSERT_EQ(result, 0);

    /* At 60 BPM, quarter note = 1 second */
    /* Both parts should have same timing */
    ASSERT_EQ(count_events(&ctx, ALDA_EVT_NOTE_ON), 4);

    /* Second notes should start at 480 ticks (one quarter note at 60 BPM) */
    AldaScheduledEvent* piano_d = find_note_on(&ctx, 62);  /* D4 */
    AldaScheduledEvent* violin_f = find_note_on(&ctx, 65); /* F4 (default octave) */
    ASSERT_NOT_NULL(piano_d);
    ASSERT_NOT_NULL(violin_f);
    ASSERT_EQ(piano_d->tick, 480);
    ASSERT_EQ(violin_f->tick, 480);

    alda_context_cleanup(&ctx);
}

TEST(integration_tempo_change_mid_score) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* Change tempo during playback */
    int result = alda_interpret_string(&ctx,
        "piano: (tempo 120) c (tempo 60) d", "test");
    ASSERT_EQ(result, 0);

    ASSERT_EQ(count_events(&ctx, ALDA_EVT_NOTE_ON), 2);

    /* C at 120 BPM (480 ticks), D after tempo change to 60 BPM */
    AldaScheduledEvent* c = find_note_on(&ctx, 60);
    AldaScheduledEvent* d = find_note_on(&ctx, 62);
    ASSERT_NOT_NULL(c);
    ASSERT_NOT_NULL(d);
    /* D should start at 480 ticks (after C at 120 BPM) */
    ASSERT_EQ(d->tick, 480);

    alda_context_cleanup(&ctx);
}

/* ============================================================================
 * Complex Composition Integration Tests
 * ============================================================================ */

TEST(integration_realistic_composition) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* A realistic short composition */
    int result = alda_interpret_string(&ctx,
        "(tempo 100)\n"
        "theme = c4 d4 e4 d4\n"
        "piano:\n"
        "(volume 80)\n"
        "theme | theme\n"
        "violin:\n"
        "(volume 60)\n"
        "o5 e2 f2 | g2 f2", "test");
    ASSERT_EQ(result, 0);

    /* Piano: 8 notes (theme twice), Violin: 4 notes */
    ASSERT_EQ(count_notes_on_channel(&ctx, 0), 8);
    ASSERT_EQ(count_notes_on_channel(&ctx, 1), 4);

    alda_context_cleanup(&ctx);
}

TEST(integration_chord_progression) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* Simple chord progression */
    int result = alda_interpret_string(&ctx,
        "piano:\n"
        "c/e/g c/e/g f/a/>c < c/e/g", "test");
    ASSERT_EQ(result, 0);

    /* 4 chords, 3 notes each = 12 notes */
    ASSERT_EQ(count_events(&ctx, ALDA_EVT_NOTE_ON), 12);

    alda_context_cleanup(&ctx);
}

/* ============================================================================
 * Edge Case Integration Tests
 * ============================================================================ */

TEST(integration_empty_part) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* Part with no notes */
    int result = alda_interpret_string(&ctx,
        "piano:\n"
        "violin: c", "test");
    ASSERT_EQ(result, 0);

    /* Only violin plays */
    ASSERT_EQ(count_events(&ctx, ALDA_EVT_NOTE_ON), 1);

    alda_context_cleanup(&ctx);
}

TEST(integration_very_long_tie) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* Long tied note */
    int result = alda_interpret_string(&ctx,
        "piano: c1~1~1~1", "test");
    ASSERT_EQ(result, 0);

    /* Single note-on */
    ASSERT_EQ(count_events(&ctx, ALDA_EVT_NOTE_ON), 1);

    alda_context_cleanup(&ctx);
}

TEST(integration_rapid_octave_changes) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* Rapid octave changes */
    int result = alda_interpret_string(&ctx,
        "piano: c > c > c < c < c", "test");
    ASSERT_EQ(result, 0);

    /* 5 notes at different octaves */
    ASSERT_EQ(count_events(&ctx, ALDA_EVT_NOTE_ON), 5);

    /* C4=60, C5=72, C6=84, C5=72, C4=60 */
    /* There should be 2 C4s, 2 C5s, 1 C6 */
    int c4_count = 0, c5_count = 0, c6_count = 0;
    for (int i = 0; i < ctx.event_count; i++) {
        if (ctx.events[i].type == ALDA_EVT_NOTE_ON) {
            if (ctx.events[i].data1 == 60) c4_count++;
            else if (ctx.events[i].data1 == 72) c5_count++;
            else if (ctx.events[i].data1 == 84) c6_count++;
        }
    }
    ASSERT_EQ(c4_count, 2);
    ASSERT_EQ(c5_count, 2);
    ASSERT_EQ(c6_count, 1);

    alda_context_cleanup(&ctx);
}

/* ============================================================================
 * Test Runner
 * ============================================================================ */


BEGIN_TEST_SUITE("Alda Integration Tests")

    /* Multi-part tests */
    RUN_TEST(integration_two_parts_simultaneous);
    RUN_TEST(integration_part_switching);

    /* Variable tests */
    RUN_TEST(integration_variable_reuse);
    RUN_TEST(integration_nested_brackets_and_repeat);

    /* Octave tests */
    RUN_TEST(integration_octave_across_parts);
    RUN_TEST(integration_octave_shifts_in_chord);

    /* Voice tests */
    RUN_TEST(integration_voices_with_different_rhythms);

    /* Tempo tests */
    RUN_TEST(integration_tempo_affects_all_parts);
    RUN_TEST(integration_tempo_change_mid_score);

    /* Complex compositions */
    RUN_TEST(integration_realistic_composition);
    RUN_TEST(integration_chord_progression);

    /* Edge cases */
    RUN_TEST(integration_empty_part);
    RUN_TEST(integration_very_long_tie);
    RUN_TEST(integration_rapid_octave_changes);

END_TEST_SUITE()
