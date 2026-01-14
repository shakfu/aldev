/**
 * @file test_interpreter.c
 * @brief Unit tests for Alda interpreter.
 *
 * Tests MIDI event generation including tempo, volume, polyphony,
 * markers, variables, and other core interpreter functionality.
 */

#include "../test_framework.h"
#include <alda/alda.h>
#include <alda/context.h>
#include <alda/interpreter.h>
#include <alda/scheduler.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/* Initialize context for testing */
static void test_context_init(AldaContext* ctx) {
    alda_context_init(ctx);
    alda_set_no_sleep(ctx, 1);  /* Disable timing for tests */
}

/* Count events of a specific type */
static int count_events(AldaContext* ctx, AldaEventType type) {
    int count = 0;
    for (int i = 0; i < ctx->event_count; i++) {
        if (ctx->events[i].type == type) {
            count++;
        }
    }
    return count;
}

/* Find first event of type */
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

/* Find note-on event with specific pitch */
static AldaScheduledEvent* find_note_on(AldaContext* ctx, int pitch) {
    for (int i = 0; i < ctx->event_count; i++) {
        if (ctx->events[i].type == ALDA_EVT_NOTE_ON &&
            ctx->events[i].data1 == pitch) {
            return &ctx->events[i];
        }
    }
    return NULL;
}

/* ============================================================================
 * Basic Note Tests
 * ============================================================================ */

TEST(interpret_single_note) {
    AldaContext ctx;
    test_context_init(&ctx);

    int result = alda_interpret_string(&ctx, "piano: c", "test");
    ASSERT_EQ(result, 0);

    /* Should have note-on and note-off events */
    ASSERT_EQ(count_events(&ctx, ALDA_EVT_NOTE_ON), 1);
    ASSERT_EQ(count_events(&ctx, ALDA_EVT_NOTE_OFF), 1);

    /* C4 = MIDI pitch 60 */
    AldaScheduledEvent* note_on = find_event(&ctx, ALDA_EVT_NOTE_ON, 0);
    ASSERT_NOT_NULL(note_on);
    ASSERT_EQ(note_on->data1, 60);

    alda_context_cleanup(&ctx);
}

TEST(interpret_note_with_accidentals) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* C# (sharp) */
    int result = alda_interpret_string(&ctx, "piano: c+", "test");
    ASSERT_EQ(result, 0);

    AldaScheduledEvent* note = find_event(&ctx, ALDA_EVT_NOTE_ON, 0);
    ASSERT_NOT_NULL(note);
    ASSERT_EQ(note->data1, 61);  /* C#4 = 61 */

    alda_context_cleanup(&ctx);

    /* Db (flat) */
    test_context_init(&ctx);
    result = alda_interpret_string(&ctx, "piano: d-", "test");
    ASSERT_EQ(result, 0);

    note = find_event(&ctx, ALDA_EVT_NOTE_ON, 0);
    ASSERT_NOT_NULL(note);
    ASSERT_EQ(note->data1, 61);  /* Db4 = 61 */

    alda_context_cleanup(&ctx);
}

TEST(interpret_note_with_octave) {
    AldaContext ctx;
    test_context_init(&ctx);

    int result = alda_interpret_string(&ctx, "piano: o5 c", "test");
    ASSERT_EQ(result, 0);

    AldaScheduledEvent* note = find_event(&ctx, ALDA_EVT_NOTE_ON, 0);
    ASSERT_NOT_NULL(note);
    ASSERT_EQ(note->data1, 72);  /* C5 = 72 */

    alda_context_cleanup(&ctx);
}

TEST(interpret_octave_up_down) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* Default octave is 4, > goes up, < goes down */
    int result = alda_interpret_string(&ctx, "piano: c > c < < c", "test");
    ASSERT_EQ(result, 0);

    ASSERT_EQ(count_events(&ctx, ALDA_EVT_NOTE_ON), 3);

    /* First C at octave 4 */
    AldaScheduledEvent* note1 = find_event(&ctx, ALDA_EVT_NOTE_ON, 0);
    ASSERT_NOT_NULL(note1);
    ASSERT_EQ(note1->data1, 60);

    /* Second C at octave 5 */
    AldaScheduledEvent* note2 = find_event(&ctx, ALDA_EVT_NOTE_ON, 1);
    ASSERT_NOT_NULL(note2);
    ASSERT_EQ(note2->data1, 72);

    /* Third C at octave 3 */
    AldaScheduledEvent* note3 = find_event(&ctx, ALDA_EVT_NOTE_ON, 2);
    ASSERT_NOT_NULL(note3);
    ASSERT_EQ(note3->data1, 48);

    alda_context_cleanup(&ctx);
}

TEST(interpret_note_sequence) {
    AldaContext ctx;
    test_context_init(&ctx);

    int result = alda_interpret_string(&ctx, "piano: c d e f g", "test");
    ASSERT_EQ(result, 0);

    ASSERT_EQ(count_events(&ctx, ALDA_EVT_NOTE_ON), 5);

    /* Verify pitches: C=60, D=62, E=64, F=65, G=67 */
    AldaScheduledEvent* c = find_event(&ctx, ALDA_EVT_NOTE_ON, 0);
    AldaScheduledEvent* d = find_event(&ctx, ALDA_EVT_NOTE_ON, 1);
    AldaScheduledEvent* e = find_event(&ctx, ALDA_EVT_NOTE_ON, 2);
    AldaScheduledEvent* f = find_event(&ctx, ALDA_EVT_NOTE_ON, 3);
    AldaScheduledEvent* g = find_event(&ctx, ALDA_EVT_NOTE_ON, 4);

    ASSERT_EQ(c->data1, 60);
    ASSERT_EQ(d->data1, 62);
    ASSERT_EQ(e->data1, 64);
    ASSERT_EQ(f->data1, 65);
    ASSERT_EQ(g->data1, 67);

    alda_context_cleanup(&ctx);
}

/* ============================================================================
 * Duration Tests
 * ============================================================================ */

TEST(interpret_note_durations) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* Quarter note (4), half note (2), whole note (1) */
    int result = alda_interpret_string(&ctx, "piano: c4 c2 c1", "test");
    ASSERT_EQ(result, 0);

    alda_events_sort(&ctx);

    ASSERT_EQ(count_events(&ctx, ALDA_EVT_NOTE_ON), 3);

    /* Verify timing: quarter = 480 ticks, half = 960, whole = 1920 */
    AldaScheduledEvent* n1 = find_event(&ctx, ALDA_EVT_NOTE_ON, 0);
    AldaScheduledEvent* n2 = find_event(&ctx, ALDA_EVT_NOTE_ON, 1);
    AldaScheduledEvent* n3 = find_event(&ctx, ALDA_EVT_NOTE_ON, 2);

    ASSERT_EQ(n1->tick, 0);
    ASSERT_EQ(n2->tick, 480);     /* After quarter note */
    ASSERT_EQ(n3->tick, 1440);    /* After quarter + half */

    alda_context_cleanup(&ctx);
}

TEST(interpret_dotted_duration) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* Dotted quarter = 480 + 240 = 720 ticks */
    int result = alda_interpret_string(&ctx, "piano: c4. c4", "test");
    ASSERT_EQ(result, 0);

    alda_events_sort(&ctx);

    AldaScheduledEvent* n1 = find_event(&ctx, ALDA_EVT_NOTE_ON, 0);
    AldaScheduledEvent* n2 = find_event(&ctx, ALDA_EVT_NOTE_ON, 1);

    ASSERT_EQ(n1->tick, 0);
    ASSERT_EQ(n2->tick, 720);  /* After dotted quarter */

    alda_context_cleanup(&ctx);
}

TEST(interpret_tied_duration) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* Tied quarter + eighth = 480 + 240 = 720 ticks */
    int result = alda_interpret_string(&ctx, "piano: c4~8 c4", "test");
    ASSERT_EQ(result, 0);

    alda_events_sort(&ctx);

    AldaScheduledEvent* n1 = find_event(&ctx, ALDA_EVT_NOTE_ON, 0);
    AldaScheduledEvent* n2 = find_event(&ctx, ALDA_EVT_NOTE_ON, 1);

    ASSERT_EQ(n1->tick, 0);
    ASSERT_EQ(n2->tick, 720);

    alda_context_cleanup(&ctx);
}

/* ============================================================================
 * Rest Tests
 * ============================================================================ */

TEST(interpret_rest) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* Note, rest, note - second note should be offset */
    int result = alda_interpret_string(&ctx, "piano: c4 r4 c4", "test");
    ASSERT_EQ(result, 0);

    alda_events_sort(&ctx);

    ASSERT_EQ(count_events(&ctx, ALDA_EVT_NOTE_ON), 2);

    AldaScheduledEvent* n1 = find_event(&ctx, ALDA_EVT_NOTE_ON, 0);
    AldaScheduledEvent* n2 = find_event(&ctx, ALDA_EVT_NOTE_ON, 1);

    ASSERT_EQ(n1->tick, 0);
    ASSERT_EQ(n2->tick, 960);  /* After quarter note + quarter rest */

    alda_context_cleanup(&ctx);
}

/* ============================================================================
 * Chord Tests
 * ============================================================================ */

TEST(interpret_chord) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* C major chord */
    int result = alda_interpret_string(&ctx, "piano: c/e/g", "test");
    ASSERT_EQ(result, 0);

    ASSERT_EQ(count_events(&ctx, ALDA_EVT_NOTE_ON), 3);

    /* All notes should start at same tick */
    AldaScheduledEvent* c = find_note_on(&ctx, 60);
    AldaScheduledEvent* e = find_note_on(&ctx, 64);
    AldaScheduledEvent* g = find_note_on(&ctx, 67);

    ASSERT_NOT_NULL(c);
    ASSERT_NOT_NULL(e);
    ASSERT_NOT_NULL(g);

    ASSERT_EQ(c->tick, e->tick);
    ASSERT_EQ(e->tick, g->tick);

    alda_context_cleanup(&ctx);
}

TEST(interpret_chord_with_octave_change) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* Chord spanning octaves */
    int result = alda_interpret_string(&ctx, "piano: c/>e/>g", "test");
    ASSERT_EQ(result, 0);

    ASSERT_EQ(count_events(&ctx, ALDA_EVT_NOTE_ON), 3);

    /* C4=60, E5=76, G6=91 */
    AldaScheduledEvent* c = find_note_on(&ctx, 60);
    AldaScheduledEvent* e = find_note_on(&ctx, 76);
    AldaScheduledEvent* g = find_note_on(&ctx, 91);

    ASSERT_NOT_NULL(c);
    ASSERT_NOT_NULL(e);
    ASSERT_NOT_NULL(g);

    alda_context_cleanup(&ctx);
}

/* ============================================================================
 * Tempo Tests
 * ============================================================================ */

TEST(interpret_tempo_attribute) {
    AldaContext ctx;
    test_context_init(&ctx);

    int result = alda_interpret_string(&ctx, "piano: (tempo 180) c", "test");
    ASSERT_EQ(result, 0);

    /* Should have a tempo change event */
    ASSERT_EQ(count_events(&ctx, ALDA_EVT_TEMPO), 1);

    AldaScheduledEvent* tempo = find_event(&ctx, ALDA_EVT_TEMPO, 0);
    ASSERT_NOT_NULL(tempo);
    ASSERT_EQ(tempo->data1, 180);

    alda_context_cleanup(&ctx);
}

TEST(interpret_tempo_change_mid_score) {
    AldaContext ctx;
    test_context_init(&ctx);

    int result = alda_interpret_string(&ctx, "piano: c4 (tempo 180) c4", "test");
    ASSERT_EQ(result, 0);

    ASSERT_EQ(count_events(&ctx, ALDA_EVT_TEMPO), 1);

    AldaScheduledEvent* tempo = find_event(&ctx, ALDA_EVT_TEMPO, 0);
    ASSERT_NOT_NULL(tempo);
    ASSERT_EQ(tempo->tick, 480);  /* After first quarter note */
    ASSERT_EQ(tempo->data1, 180);

    alda_context_cleanup(&ctx);
}

/* ============================================================================
 * Volume/Dynamics Tests
 * ============================================================================ */

TEST(interpret_volume_attribute) {
    AldaContext ctx;
    test_context_init(&ctx);

    int result = alda_interpret_string(&ctx, "piano: (volume 100) c", "test");
    ASSERT_EQ(result, 0);

    /* Note velocity should reflect volume */
    AldaScheduledEvent* note = find_event(&ctx, ALDA_EVT_NOTE_ON, 0);
    ASSERT_NOT_NULL(note);
    ASSERT_EQ(note->data2, 127);  /* 100% volume = max velocity */

    alda_context_cleanup(&ctx);
}

TEST(interpret_dynamics) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* ff (fortissimo) should give high velocity (88 per aldakit) */
    int result = alda_interpret_string(&ctx, "piano: (ff) c", "test");
    ASSERT_EQ(result, 0);

    AldaScheduledEvent* note = find_event(&ctx, ALDA_EVT_NOTE_ON, 0);
    ASSERT_NOT_NULL(note);
    ASSERT_EQ(note->data2, 88);  /* ff = 88 velocity per aldakit */

    alda_context_cleanup(&ctx);

    /* pp (pianissimo) should give low velocity (39 per aldakit) */
    test_context_init(&ctx);
    result = alda_interpret_string(&ctx, "piano: (pp) c", "test");
    ASSERT_EQ(result, 0);

    note = find_event(&ctx, ALDA_EVT_NOTE_ON, 0);
    ASSERT_NOT_NULL(note);
    ASSERT_EQ(note->data2, 39);  /* pp = 39 velocity per aldakit */

    alda_context_cleanup(&ctx);
}

/* ============================================================================
 * Repeat Tests
 * ============================================================================ */

TEST(interpret_simple_repeat) {
    AldaContext ctx;
    test_context_init(&ctx);

    int result = alda_interpret_string(&ctx, "piano: c *3", "test");
    ASSERT_EQ(result, 0);

    ASSERT_EQ(count_events(&ctx, ALDA_EVT_NOTE_ON), 3);

    alda_context_cleanup(&ctx);
}

TEST(interpret_repeat_sequence) {
    AldaContext ctx;
    test_context_init(&ctx);

    int result = alda_interpret_string(&ctx, "piano: [c d] *2", "test");
    ASSERT_EQ(result, 0);

    ASSERT_EQ(count_events(&ctx, ALDA_EVT_NOTE_ON), 4);  /* c d c d */

    alda_context_cleanup(&ctx);
}

TEST(interpret_alternate_endings) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* '1 plays on first rep, '2 plays on second rep */
    int result = alda_interpret_string(&ctx, "piano: [c d '1 e '2 f] *2", "test");
    ASSERT_EQ(result, 0);

    /* First rep: c d e, Second rep: c d f = 6 notes */
    ASSERT_EQ(count_events(&ctx, ALDA_EVT_NOTE_ON), 6);

    alda_context_cleanup(&ctx);
}

/* ============================================================================
 * Variable Tests
 * ============================================================================ */

TEST(interpret_variable_definition_and_reference) {
    AldaContext ctx;
    test_context_init(&ctx);

    int result = alda_interpret_string(&ctx, "piano: motif = [c d e] motif motif", "test");
    ASSERT_EQ(result, 0);

    /* motif (c d e) played twice = 6 notes */
    ASSERT_EQ(count_events(&ctx, ALDA_EVT_NOTE_ON), 6);

    alda_context_cleanup(&ctx);
}

TEST(interpret_variable_redefine) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* Use newlines to separate variable definitions and uses */
    int result = alda_interpret_string(&ctx, 
        "piano:\n"
        "x = c\n"
        "x\n"
        "x = d\n"
        "x\n"
        "x = e\n"
        "x", "test");
    ASSERT_EQ(result, 0);

    ASSERT_EQ(count_events(&ctx, ALDA_EVT_NOTE_ON), 3);

    /* Pitches should be c, d, e */
    AldaScheduledEvent* n1 = find_event(&ctx, ALDA_EVT_NOTE_ON, 0);
    AldaScheduledEvent* n2 = find_event(&ctx, ALDA_EVT_NOTE_ON, 1);
    AldaScheduledEvent* n3 = find_event(&ctx, ALDA_EVT_NOTE_ON, 2);

    ASSERT_EQ(n1->data1, 60);  /* C */
    ASSERT_EQ(n2->data1, 62);  /* D */
    ASSERT_EQ(n3->data1, 64);  /* E */

    alda_context_cleanup(&ctx);
}

/* ============================================================================
 * Marker Tests
 * ============================================================================ */

TEST(interpret_marker_and_at_marker) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* Define marker after c, continue with d e, then jump back and play f */
    /* Marker is placed after c, so @here goes back to tick 480 */
    int result = alda_interpret_string(&ctx, 
        "piano: c4 %here d4 e4 @here f4", "test");
    ASSERT_EQ(result, 0);

    alda_events_sort(&ctx);

    /* Should have 4 notes: c, d, e, f */
    ASSERT_EQ(count_events(&ctx, ALDA_EVT_NOTE_ON), 4);

    /* c starts at 0 */
    /* %here marker placed at tick 480 (after c) */
    /* d starts at 480 */
    /* e starts at 960 */
    /* @here jumps back to tick 480 */
    /* f starts at 480 (same time as d) */

    /* Find all note-ons */
    int c_tick = -1, d_tick = -1, e_tick = -1, f_tick = -1;

    for (int i = 0; i < ctx.event_count; i++) {
        if (ctx.events[i].type == ALDA_EVT_NOTE_ON) {
            int pitch = ctx.events[i].data1;
            int tick = ctx.events[i].tick;

            if (pitch == 60) { c_tick = tick; }
            else if (pitch == 62) { d_tick = tick; }
            else if (pitch == 64) { e_tick = tick; }
            else if (pitch == 65) { f_tick = tick; }
        }
    }

    ASSERT_EQ(c_tick, 0);
    ASSERT_EQ(d_tick, 480);
    ASSERT_EQ(e_tick, 960);
    ASSERT_EQ(f_tick, 480);  /* f at same time as d (jumped back via marker) */

    alda_context_cleanup(&ctx);
}

/* ============================================================================
 * Voice Tests (Polyphony)
 * ============================================================================ */

TEST(interpret_voices) {
    AldaContext ctx;
    test_context_init(&ctx);

    int result = alda_interpret_string(&ctx, 
        "piano: V1: c d e V2: g a b V0:", "test");
    ASSERT_EQ(result, 0);

    ASSERT_EQ(count_events(&ctx, ALDA_EVT_NOTE_ON), 6);

    /* Both voices start at same tick (0) */
    AldaScheduledEvent* c = find_note_on(&ctx, 60);  /* C in V1 */
    AldaScheduledEvent* g = find_note_on(&ctx, 67);  /* G in V2 */

    ASSERT_NOT_NULL(c);
    ASSERT_NOT_NULL(g);
    ASSERT_EQ(c->tick, 0);
    ASSERT_EQ(g->tick, 0);

    alda_context_cleanup(&ctx);
}

TEST(interpret_voice_timing) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* Voice 1 has 3 quarter notes, voice 2 has 2 */
    int result = alda_interpret_string(&ctx, 
        "piano: V1: c4 d4 e4 V2: g2 V0:", "test");
    ASSERT_EQ(result, 0);

    alda_events_sort(&ctx);

    /* After V0:, part tick should be at max voice tick (3*480 = 1440) */
    /* Verify c, d, e are at 0, 480, 960 */
    /* Verify g is at 0 */

    int found_ticks[4] = {-1, -1, -1, -1};
    for (int i = 0; i < ctx.event_count; i++) {
        if (ctx.events[i].type == ALDA_EVT_NOTE_ON) {
            switch (ctx.events[i].data1) {
                case 60: found_ticks[0] = ctx.events[i].tick; break;  /* c */
                case 62: found_ticks[1] = ctx.events[i].tick; break;  /* d */
                case 64: found_ticks[2] = ctx.events[i].tick; break;  /* e */
                case 67: found_ticks[3] = ctx.events[i].tick; break;  /* g */
            }
        }
    }

    ASSERT_EQ(found_ticks[0], 0);     /* c */
    ASSERT_EQ(found_ticks[1], 480);   /* d */
    ASSERT_EQ(found_ticks[2], 960);   /* e */
    ASSERT_EQ(found_ticks[3], 0);     /* g */

    alda_context_cleanup(&ctx);
}

/* ============================================================================
 * Cram Expression Tests
 * ============================================================================ */

TEST(interpret_cram_basic) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* 4 notes crammed into 1 quarter note duration */
    int result = alda_interpret_string(&ctx, "piano: {c d e f}4", "test");
    ASSERT_EQ(result, 0);

    ASSERT_EQ(count_events(&ctx, ALDA_EVT_NOTE_ON), 4);

    /* Total duration should be 480 (quarter note), each note 120 */
    alda_events_sort(&ctx);

    int ticks[4] = {-1, -1, -1, -1};
    int idx = 0;
    for (int i = 0; i < ctx.event_count && idx < 4; i++) {
        if (ctx.events[i].type == ALDA_EVT_NOTE_ON) {
            ticks[idx++] = ctx.events[i].tick;
        }
    }

    ASSERT_EQ(ticks[0], 0);
    ASSERT_EQ(ticks[1], 120);
    ASSERT_EQ(ticks[2], 240);
    ASSERT_EQ(ticks[3], 360);

    alda_context_cleanup(&ctx);
}

/* ============================================================================
 * Key Signature Tests
 * ============================================================================ */

TEST(interpret_key_signature) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* G major: F is sharp (using key-sig with tonic and mode) */
    int result = alda_interpret_string(&ctx, "piano: (key-sig '(g major)) f", "test");
    ASSERT_EQ(result, 0);

    AldaScheduledEvent* note = find_event(&ctx, ALDA_EVT_NOTE_ON, 0);
    ASSERT_NOT_NULL(note);
    ASSERT_EQ(note->data1, 66);  /* F# = 66 in G major */

    alda_context_cleanup(&ctx);
}

TEST(interpret_natural_overrides_key_sig) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* G major has F#, but f_ means natural */
    int result = alda_interpret_string(&ctx, "piano: (key-sig '(g major)) f_", "test");
    ASSERT_EQ(result, 0);

    AldaScheduledEvent* note = find_event(&ctx, ALDA_EVT_NOTE_ON, 0);
    ASSERT_NOT_NULL(note);
    ASSERT_EQ(note->data1, 65);  /* F natural = 65 */

    alda_context_cleanup(&ctx);
}

/* ============================================================================
 * Transpose Tests
 * ============================================================================ */

TEST(interpret_transpose) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* Transpose up 2 semitones */
    int result = alda_interpret_string(&ctx, "piano: (transpose 2) c", "test");
    ASSERT_EQ(result, 0);

    AldaScheduledEvent* note = find_event(&ctx, ALDA_EVT_NOTE_ON, 0);
    ASSERT_NOT_NULL(note);
    ASSERT_EQ(note->data1, 62);  /* C + 2 = D */

    alda_context_cleanup(&ctx);
}

TEST(interpret_transpose_negative) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* Transpose down 3 semitones */
    int result = alda_interpret_string(&ctx, "piano: (transpose -3) c", "test");
    ASSERT_EQ(result, 0);

    AldaScheduledEvent* note = find_event(&ctx, ALDA_EVT_NOTE_ON, 0);
    ASSERT_NOT_NULL(note);
    ASSERT_EQ(note->data1, 57);  /* C - 3 = A */

    alda_context_cleanup(&ctx);
}

/* ============================================================================
 * Pan Tests
 * ============================================================================ */

TEST(interpret_pan) {
    AldaContext ctx;
    test_context_init(&ctx);

    int result = alda_interpret_string(&ctx, "piano: (pan 0) c", "test");
    ASSERT_EQ(result, 0);

    /* Should have a pan event */
    ASSERT_EQ(count_events(&ctx, ALDA_EVT_PAN), 1);

    AldaScheduledEvent* pan = find_event(&ctx, ALDA_EVT_PAN, 0);
    ASSERT_NOT_NULL(pan);
    /* For ALDA_EVT_PAN, data1 stores the pan value (0-127) */
    ASSERT_EQ(pan->data1, 0);    /* Hard left (0% -> 0) */

    alda_context_cleanup(&ctx);
}

/* ============================================================================
 * Quantization Tests
 * ============================================================================ */

TEST(interpret_quantization) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* quant 50 means note plays for 50% of its duration */
    int result = alda_interpret_string(&ctx, "piano: (quant 50) c4", "test");
    ASSERT_EQ(result, 0);

    alda_events_sort(&ctx);

    AldaScheduledEvent* note_on = find_event(&ctx, ALDA_EVT_NOTE_ON, 0);
    AldaScheduledEvent* note_off = find_event(&ctx, ALDA_EVT_NOTE_OFF, 0);

    ASSERT_NOT_NULL(note_on);
    ASSERT_NOT_NULL(note_off);

    /* Quarter note = 480 ticks, 50% quant = 240 ticks sounding */
    int duration = note_off->tick - note_on->tick;
    ASSERT_EQ(duration, 240);

    alda_context_cleanup(&ctx);
}

/* ============================================================================
 * Multiple Parts Tests
 * ============================================================================ */

TEST(interpret_multiple_parts) {
    AldaContext ctx;
    test_context_init(&ctx);

    int result = alda_interpret_string(&ctx, 
        "piano: c d e\nviolin: g a b", "test");
    ASSERT_EQ(result, 0);

    /* Should have program changes for both instruments */
    ASSERT_EQ(count_events(&ctx, ALDA_EVT_PROGRAM), 2);
    ASSERT_EQ(count_events(&ctx, ALDA_EVT_NOTE_ON), 6);

    alda_context_cleanup(&ctx);
}

TEST(interpret_part_group) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* Two instruments playing same notes */
    int result = alda_interpret_string(&ctx, "piano/violin: c d", "test");
    ASSERT_EQ(result, 0);

    /* Each instrument plays both notes = 4 note-ons */
    ASSERT_EQ(count_events(&ctx, ALDA_EVT_NOTE_ON), 4);

    /* Should have 2 program changes (one per instrument) */
    ASSERT_EQ(count_events(&ctx, ALDA_EVT_PROGRAM), 2);

    alda_context_cleanup(&ctx);
}

/* ============================================================================
 * Program Change Tests
 * ============================================================================ */

TEST(interpret_program_change) {
    AldaContext ctx;
    test_context_init(&ctx);

    int result = alda_interpret_string(&ctx, "piano: c", "test");
    ASSERT_EQ(result, 0);

    ASSERT_EQ(count_events(&ctx, ALDA_EVT_PROGRAM), 1);

    AldaScheduledEvent* prog = find_event(&ctx, ALDA_EVT_PROGRAM, 0);
    ASSERT_NOT_NULL(prog);
    ASSERT_EQ(prog->data1, 0);  /* Piano = GM program 0 */

    alda_context_cleanup(&ctx);

    /* Test violin */
    test_context_init(&ctx);
    result = alda_interpret_string(&ctx, "violin: c", "test");
    ASSERT_EQ(result, 0);

    prog = find_event(&ctx, ALDA_EVT_PROGRAM, 0);
    ASSERT_NOT_NULL(prog);
    ASSERT_EQ(prog->data1, 40);  /* Violin = GM program 40 */

    alda_context_cleanup(&ctx);
}

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

TEST(interpret_undefined_variable_error) {
    AldaContext ctx;
    test_context_init(&ctx);

    int result = alda_interpret_string(&ctx, "piano: undefined_var", "test");
    ASSERT_EQ(result, -1);  /* Should fail */

    alda_context_cleanup(&ctx);
}

TEST(interpret_undefined_marker_error) {
    AldaContext ctx;
    test_context_init(&ctx);

    int result = alda_interpret_string(&ctx, "piano: @nonexistent", "test");
    ASSERT_EQ(result, -1);  /* Should fail */

    alda_context_cleanup(&ctx);
}

TEST(interpret_no_part_error) {
    AldaContext ctx;
    test_context_init(&ctx);

    /* Notes without declaring a part first */
    int result = alda_interpret_string(&ctx, "c d e", "test");
    ASSERT_EQ(result, -1);  /* Should fail */

    alda_context_cleanup(&ctx);
}

/* ============================================================================
 * Pitch Calculation Unit Tests
 * ============================================================================ */

TEST(calculate_pitch_basic) {
    /* C4 = 60 */
    ASSERT_EQ(alda_calculate_pitch('c', NULL, 4, NULL), 60);
    ASSERT_EQ(alda_calculate_pitch('C', NULL, 4, NULL), 60);

    /* D4 = 62 */
    ASSERT_EQ(alda_calculate_pitch('d', NULL, 4, NULL), 62);

    /* A4 = 69 */
    ASSERT_EQ(alda_calculate_pitch('a', NULL, 4, NULL), 69);
}

TEST(calculate_pitch_octaves) {
    /* C0 = 12 */
    ASSERT_EQ(alda_calculate_pitch('c', NULL, 0, NULL), 12);

    /* C5 = 72 */
    ASSERT_EQ(alda_calculate_pitch('c', NULL, 5, NULL), 72);

    /* C8 = 108 */
    ASSERT_EQ(alda_calculate_pitch('c', NULL, 8, NULL), 108);
}

TEST(calculate_pitch_accidentals) {
    /* C#4 = 61 */
    ASSERT_EQ(alda_calculate_pitch('c', "+", 4, NULL), 61);

    /* Db4 = 61 */
    ASSERT_EQ(alda_calculate_pitch('d', "-", 4, NULL), 61);

    /* C##4 (double sharp) = 62 */
    ASSERT_EQ(alda_calculate_pitch('c', "++", 4, NULL), 62);

    /* Dbb4 (double flat) = 60 */
    ASSERT_EQ(alda_calculate_pitch('d', "--", 4, NULL), 60);
}

TEST(calculate_pitch_with_key_sig) {
    /* G major: F# */
    int key_g_major[7] = {0, 0, 0, 1, 0, 0, 0};  /* F is sharp */

    /* F in G major should be F# = 66 */
    ASSERT_EQ(alda_calculate_pitch('f', NULL, 4, key_g_major), 66);

    /* F natural (_) in G major should still be F = 65 */
    ASSERT_EQ(alda_calculate_pitch('f', "_", 4, key_g_major), 65);

    /* F# explicitly in G major = 66 (explicit overrides) */
    ASSERT_EQ(alda_calculate_pitch('f', "+", 4, key_g_major), 66);
}

/* ============================================================================
 * Duration Calculation Unit Tests
 * ============================================================================ */

TEST(duration_to_ticks_basic) {
    /* Whole note = 1920 */
    ASSERT_EQ(alda_duration_to_ticks(1, 0), 1920);

    /* Half note = 960 */
    ASSERT_EQ(alda_duration_to_ticks(2, 0), 960);

    /* Quarter note = 480 */
    ASSERT_EQ(alda_duration_to_ticks(4, 0), 480);

    /* Eighth note = 240 */
    ASSERT_EQ(alda_duration_to_ticks(8, 0), 240);

    /* Sixteenth = 120 */
    ASSERT_EQ(alda_duration_to_ticks(16, 0), 120);
}

TEST(duration_to_ticks_dotted) {
    /* Dotted quarter = 480 + 240 = 720 */
    ASSERT_EQ(alda_duration_to_ticks(4, 1), 720);

    /* Double dotted quarter = 480 + 240 + 120 = 840 */
    ASSERT_EQ(alda_duration_to_ticks(4, 2), 840);
}

TEST(ms_to_ticks) {
    /* At 120 BPM: 1 beat = 500ms, 1 beat = 480 ticks */
    /* So 1000ms = 2 beats = 960 ticks */
    ASSERT_EQ(alda_ms_to_ticks(1000, 120), 960);

    /* 500ms at 120 BPM = 480 ticks */
    ASSERT_EQ(alda_ms_to_ticks(500, 120), 480);
}

TEST(apply_quant) {
    /* 100% quant = full duration */
    ASSERT_EQ(alda_apply_quant(480, 100), 480);

    /* 50% quant = half duration */
    ASSERT_EQ(alda_apply_quant(480, 50), 240);

    /* 90% quant (default) */
    ASSERT_EQ(alda_apply_quant(480, 90), 432);
}

/* ============================================================================
 * Test Suite Main
 * ============================================================================ */

BEGIN_TEST_SUITE("Alda Interpreter")
    /* Basic notes */
    RUN_TEST(interpret_single_note);
    RUN_TEST(interpret_note_with_accidentals);
    RUN_TEST(interpret_note_with_octave);
    RUN_TEST(interpret_octave_up_down);
    RUN_TEST(interpret_note_sequence);

    /* Durations */
    RUN_TEST(interpret_note_durations);
    RUN_TEST(interpret_dotted_duration);
    RUN_TEST(interpret_tied_duration);

    /* Rests */
    RUN_TEST(interpret_rest);

    /* Chords */
    RUN_TEST(interpret_chord);
    RUN_TEST(interpret_chord_with_octave_change);

    /* Tempo */
    RUN_TEST(interpret_tempo_attribute);
    RUN_TEST(interpret_tempo_change_mid_score);

    /* Volume/Dynamics */
    RUN_TEST(interpret_volume_attribute);
    RUN_TEST(interpret_dynamics);

    /* Repeats */
    RUN_TEST(interpret_simple_repeat);
    RUN_TEST(interpret_repeat_sequence);
    RUN_TEST(interpret_alternate_endings);

    /* Variables */
    RUN_TEST(interpret_variable_definition_and_reference);
    RUN_TEST(interpret_variable_redefine);

    /* Markers */
    RUN_TEST(interpret_marker_and_at_marker);

    /* Voices (Polyphony) */
    RUN_TEST(interpret_voices);
    RUN_TEST(interpret_voice_timing);

    /* Cram */
    RUN_TEST(interpret_cram_basic);

    /* Key Signature */
    RUN_TEST(interpret_key_signature);
    RUN_TEST(interpret_natural_overrides_key_sig);

    /* Transpose */
    RUN_TEST(interpret_transpose);
    RUN_TEST(interpret_transpose_negative);

    /* Pan */
    RUN_TEST(interpret_pan);

    /* Quantization */
    RUN_TEST(interpret_quantization);

    /* Multiple Parts */
    RUN_TEST(interpret_multiple_parts);
    RUN_TEST(interpret_part_group);

    /* Program Changes */
    RUN_TEST(interpret_program_change);

    /* Error handling */
    RUN_TEST(interpret_undefined_variable_error);
    RUN_TEST(interpret_undefined_marker_error);
    RUN_TEST(interpret_no_part_error);

    /* Pitch calculation unit tests */
    RUN_TEST(calculate_pitch_basic);
    RUN_TEST(calculate_pitch_octaves);
    RUN_TEST(calculate_pitch_accidentals);
    RUN_TEST(calculate_pitch_with_key_sig);

    /* Duration calculation unit tests */
    RUN_TEST(duration_to_ticks_basic);
    RUN_TEST(duration_to_ticks_dotted);
    RUN_TEST(ms_to_ticks);
    RUN_TEST(apply_quant);
END_TEST_SUITE()
