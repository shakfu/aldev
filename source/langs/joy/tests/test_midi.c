/**
 * @file test_midi.c
 * @brief Unit tests for Joy MIDI primitives and note parsing.
 *
 * Tests note name to MIDI number conversion and MIDI-related operations.
 * Does not require actual MIDI output for testing.
 */

#include "test_framework.h"
#include "joy_runtime.h"
#include "joy_parser.h"
#include "music_context.h"
#include "music_notation.h"
#include "midi_primitives.h"
#include <string.h>
#include <math.h>

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static JoyContext* ctx = NULL;

static void setup_context(void) {
    ctx = joy_context_new();
    joy_register_primitives(ctx);

    /* Initialize music notation - sets up symbol transformer and creates MusicContext */
    music_notation_init(ctx);

    /* Register MIDI primitives (major, minor, tempo, vol, etc.) */
    joy_midi_register_primitives(ctx);
}

static void teardown_context(void) {
    if (ctx) {
        music_notation_cleanup(ctx);
        joy_context_free(ctx);
        ctx = NULL;
    }
}

/* Execute Joy code */
static void eval_ok(const char* code) {
    joy_eval_line(ctx, code);
}

/* Get top of stack as integer */
static int64_t stack_top_int(void) {
    JoyValue v = joy_stack_peek(ctx->stack);
    return v.data.integer;
}

/* Get stack depth */
static size_t stack_depth(void) {
    return joy_stack_depth(ctx->stack);
}

/* ============================================================================
 * Note Name Parsing Tests (parse-time conversion)
 * ============================================================================ */

TEST(note_c4_is_60) {
    setup_context();
    eval_ok("c");  /* c without octave = c4 = 60 */
    ASSERT_EQ(stack_depth(), 1);
    ASSERT_EQ(stack_top_int(), 60);
    teardown_context();
}

TEST(note_c5_is_72) {
    setup_context();
    eval_ok("c5");
    ASSERT_EQ(stack_depth(), 1);
    ASSERT_EQ(stack_top_int(), 72);
    teardown_context();
}

TEST(note_c3_is_48) {
    setup_context();
    eval_ok("c3");
    ASSERT_EQ(stack_depth(), 1);
    ASSERT_EQ(stack_top_int(), 48);
    teardown_context();
}

TEST(note_a4_is_69) {
    setup_context();
    eval_ok("a");  /* a4 = 69 (concert A) */
    ASSERT_EQ(stack_depth(), 1);
    ASSERT_EQ(stack_top_int(), 69);
    teardown_context();
}

TEST(note_sharp_c_plus_is_61) {
    setup_context();
    eval_ok("c+");  /* C# = 61 */
    ASSERT_EQ(stack_depth(), 1);
    ASSERT_EQ(stack_top_int(), 61);
    teardown_context();
}

TEST(note_flat_d_minus_is_61) {
    setup_context();
    eval_ok("d-");  /* Db = 61 */
    ASSERT_EQ(stack_depth(), 1);
    ASSERT_EQ(stack_top_int(), 61);
    teardown_context();
}

TEST(note_double_sharp) {
    setup_context();
    eval_ok("c++");  /* C## = 62 = D */
    ASSERT_EQ(stack_depth(), 1);
    ASSERT_EQ(stack_top_int(), 62);
    teardown_context();
}

TEST(note_list_c_d_e) {
    setup_context();
    eval_ok("[c d e]");
    ASSERT_EQ(stack_depth(), 1);
    JoyValue v = joy_stack_peek(ctx->stack);
    ASSERT_EQ(v.type, JOY_LIST);
    ASSERT_EQ(v.data.list->length, 3);
    ASSERT_EQ(v.data.list->items[0].data.integer, 60);  /* c */
    ASSERT_EQ(v.data.list->items[1].data.integer, 62);  /* d */
    ASSERT_EQ(v.data.list->items[2].data.integer, 64);  /* e */
    teardown_context();
}

TEST(note_list_c_major_chord) {
    setup_context();
    eval_ok("[c e g]");
    ASSERT_EQ(stack_depth(), 1);
    JoyValue v = joy_stack_peek(ctx->stack);
    ASSERT_EQ(v.type, JOY_LIST);
    ASSERT_EQ(v.data.list->length, 3);
    ASSERT_EQ(v.data.list->items[0].data.integer, 60);  /* c */
    ASSERT_EQ(v.data.list->items[1].data.integer, 64);  /* e */
    ASSERT_EQ(v.data.list->items[2].data.integer, 67);  /* g */
    teardown_context();
}

/* ============================================================================
 * Note Arithmetic Tests (transpose via map)
 * ============================================================================ */

TEST(transpose_note_by_7) {
    setup_context();
    eval_ok("c 7 +");  /* c (60) + 7 = 67 (g) */
    ASSERT_EQ(stack_depth(), 1);
    ASSERT_EQ(stack_top_int(), 67);
    teardown_context();
}

TEST(transpose_list_by_12) {
    setup_context();
    eval_ok("[c d e] [12 +] map");  /* transpose up one octave */
    ASSERT_EQ(stack_depth(), 1);
    JoyValue v = joy_stack_peek(ctx->stack);
    ASSERT_EQ(v.type, JOY_LIST);
    ASSERT_EQ(v.data.list->length, 3);
    ASSERT_EQ(v.data.list->items[0].data.integer, 72);  /* c5 */
    ASSERT_EQ(v.data.list->items[1].data.integer, 74);  /* d5 */
    ASSERT_EQ(v.data.list->items[2].data.integer, 76);  /* e5 */
    teardown_context();
}

/* ============================================================================
 * Rest Parsing Tests
 * ============================================================================ */

TEST(rest_is_minus_one) {
    setup_context();
    eval_ok("r");  /* rest = -1 */
    ASSERT_EQ(stack_depth(), 1);
    ASSERT_EQ(stack_top_int(), -1);
    teardown_context();
}

/* ============================================================================
 * Music Theory Primitives Tests
 * ============================================================================ */

TEST(major_chord_from_c) {
    setup_context();
    eval_ok("c major");  /* c=60 -> [60, 64, 67] */
    ASSERT_EQ(stack_depth(), 1);
    JoyValue v = joy_stack_peek(ctx->stack);
    ASSERT_EQ(v.type, JOY_LIST);
    ASSERT_EQ(v.data.list->length, 3);
    ASSERT_EQ(v.data.list->items[0].data.integer, 60);  /* root */
    ASSERT_EQ(v.data.list->items[1].data.integer, 64);  /* major 3rd */
    ASSERT_EQ(v.data.list->items[2].data.integer, 67);  /* perfect 5th */
    teardown_context();
}

TEST(minor_chord_from_a) {
    setup_context();
    eval_ok("a minor");  /* a=69 -> [69, 72, 76] */
    ASSERT_EQ(stack_depth(), 1);
    JoyValue v = joy_stack_peek(ctx->stack);
    ASSERT_EQ(v.type, JOY_LIST);
    ASSERT_EQ(v.data.list->length, 3);
    ASSERT_EQ(v.data.list->items[0].data.integer, 69);  /* root */
    ASSERT_EQ(v.data.list->items[1].data.integer, 72);  /* minor 3rd */
    ASSERT_EQ(v.data.list->items[2].data.integer, 76);  /* perfect 5th */
    teardown_context();
}

TEST(dom7_chord_from_g) {
    setup_context();
    eval_ok("g dom7");  /* g=67 -> [67, 71, 74, 77] */
    ASSERT_EQ(stack_depth(), 1);
    JoyValue v = joy_stack_peek(ctx->stack);
    ASSERT_EQ(v.type, JOY_LIST);
    ASSERT_EQ(v.data.list->length, 4);
    ASSERT_EQ(v.data.list->items[0].data.integer, 67);  /* root G */
    ASSERT_EQ(v.data.list->items[1].data.integer, 71);  /* major 3rd B */
    ASSERT_EQ(v.data.list->items[2].data.integer, 74);  /* perfect 5th D */
    ASSERT_EQ(v.data.list->items[3].data.integer, 77);  /* minor 7th F */
    teardown_context();
}

/* ============================================================================
 * Music Context Tests (tempo, velocity)
 * ============================================================================ */

TEST(tempo_sets_context) {
    setup_context();
    MusicContext* mctx = music_get_context(ctx);
    ASSERT_EQ(mctx->tempo, 120);  /* default tempo */
    eval_ok("90 tempo");
    ASSERT_EQ(mctx->tempo, 90);
    teardown_context();
}

TEST(vol_sets_velocity) {
    setup_context();
    MusicContext* mctx = music_get_context(ctx);
    eval_ok("50 vol");  /* 50% volume */
    /* 50 * 127 / 100 = 63 */
    ASSERT_EQ(mctx->velocity, 63);
    teardown_context();
}

TEST(quant_sets_quantization) {
    setup_context();
    MusicContext* mctx = music_get_context(ctx);
    ASSERT_EQ(mctx->quantization, 90);  /* default */
    eval_ok("80 quant");
    ASSERT_EQ(mctx->quantization, 80);
    teardown_context();
}

/* ============================================================================
 * Test Runner
 * ============================================================================ */


BEGIN_TEST_SUITE("Joy MIDI Tests")

    /* Note parsing */
    RUN_TEST(note_c4_is_60);
    RUN_TEST(note_c5_is_72);
    RUN_TEST(note_c3_is_48);
    RUN_TEST(note_a4_is_69);
    RUN_TEST(note_sharp_c_plus_is_61);
    RUN_TEST(note_flat_d_minus_is_61);
    RUN_TEST(note_double_sharp);
    RUN_TEST(note_list_c_d_e);
    RUN_TEST(note_list_c_major_chord);

    /* Note arithmetic */
    RUN_TEST(transpose_note_by_7);
    RUN_TEST(transpose_list_by_12);

    /* Rest */
    RUN_TEST(rest_is_minus_one);

    /* Music theory primitives */
    RUN_TEST(major_chord_from_c);
    RUN_TEST(minor_chord_from_a);
    RUN_TEST(dom7_chord_from_g);

    /* Music context */
    RUN_TEST(tempo_sets_context);
    RUN_TEST(vol_sets_velocity);
    RUN_TEST(quant_sets_quantization);

END_TEST_SUITE()
