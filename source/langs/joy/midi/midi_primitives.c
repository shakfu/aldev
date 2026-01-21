/*
 * midi_primitives.c - MIDI primitive implementations for Joy
 *
 * Uses SharedContext from MusicContext for all MIDI/audio operations.
 * No global state - context flows through JoyContext->user_data.
 */

#include "joy_runtime.h"
#include "midi_primitives.h"
#include "joy_async.h"
#include "joy_midi_backend.h"
#include "music/music_theory.h"
#include "music_context.h"
#include "music_notation.h"
#include "psnd.h"         /* PSND_MIDI_PORT_NAME */
#include "context.h"      /* SharedContext, shared_send_* */
#include "midi/midi.h"    /* shared_midi_* */
#include "param/param.h"  /* shared_param_* */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

/* Helper macros (matching joy_primitives.c pattern) */
#define REQUIRE(n, op) \
    if (ctx->stack->depth < (n)) { joy_error_underflow(op, n, ctx->stack->depth); return; }

#define POP() joy_stack_pop(ctx->stack)
#define PUSH(v) joy_stack_push(ctx->stack, v)

#define EXPECT_TYPE(v, t, op) \
    if ((v).type != (t)) { joy_error_type(op, #t, (v).type); return; }

/* ============================================================================
 * Context-aware MIDI Helpers
 * ============================================================================
 *
 * These functions use MusicContext (which contains SharedContext and channel)
 * for all MIDI operations. No global state.
 */

/* Check if audio/MIDI output is available */
static int music_output_available(MusicContext* mctx) {
    if (!mctx || !mctx->shared) return 0;
    SharedContext* s = mctx->shared;
    return s->midi_out || s->builtin_synth_enabled || s->csound_enabled;
}

/* Send note-on using MusicContext's channel and SharedContext */
static void send_note_on_ctx(MusicContext* mctx, int pitch, int velocity) {
    if (!mctx || !mctx->shared) return;
    shared_send_note_on(mctx->shared, mctx->channel, pitch, velocity);
}

/* Send note-off using MusicContext's channel and SharedContext */
static void send_note_off_ctx(MusicContext* mctx, int pitch) {
    if (!mctx || !mctx->shared) return;
    shared_send_note_off(mctx->shared, mctx->channel, pitch);
}

/* ============================================================================
 * Port Management Primitives
 * ============================================================================ */

void midi_list_(JoyContext* ctx) {
    MusicContext* mctx = (MusicContext*)ctx->user_data;
    if (mctx && mctx->shared) {
        shared_midi_list_ports(mctx->shared);
    }
}

void midi_virtual_(JoyContext* ctx) {
    MusicContext* mctx = (MusicContext*)ctx->user_data;
    if (mctx && mctx->shared) {
        if (shared_midi_open_virtual(mctx->shared, PSND_MIDI_PORT_NAME) == 0) {
            printf("Created virtual MIDI port: " PSND_MIDI_PORT_NAME "\n");
        }
    }
}

void midi_open_(JoyContext* ctx) {
    REQUIRE(1, "midi-open");
    JoyValue v = POP();
    EXPECT_TYPE(v, JOY_INTEGER, "midi-open");

    MusicContext* mctx = (MusicContext*)ctx->user_data;
    if (!mctx || !mctx->shared) {
        printf("No music context available\n");
        return;
    }

    int port_idx = (int)v.data.integer;
    if (shared_midi_open_port(mctx->shared, port_idx) != 0) {
        printf("Failed to open MIDI port %d\n", port_idx);
    } else {
        const char* name = shared_midi_get_port_name(mctx->shared, port_idx);
        printf("Opened MIDI port %d: %s\n", port_idx, name ? name : "(unknown)");
    }
}

void midi_close_(JoyContext* ctx) {
    MusicContext* mctx = (MusicContext*)ctx->user_data;
    if (mctx && mctx->shared) {
        shared_midi_close(mctx->shared);
    }
}

/* ============================================================================
 * Note Operations
 * ============================================================================ */

void midi_note_(JoyContext* ctx) {
    REQUIRE(3, "midi-note");

    JoyValue dur_v = POP();
    JoyValue vel_v = POP();
    JoyValue pitch_v = POP();

    EXPECT_TYPE(dur_v, JOY_INTEGER, "midi-note");
    EXPECT_TYPE(vel_v, JOY_INTEGER, "midi-note");
    EXPECT_TYPE(pitch_v, JOY_INTEGER, "midi-note");

    int pitch = (int)pitch_v.data.integer;
    int velocity = (int)vel_v.data.integer;
    int duration = (int)dur_v.data.integer;

    MusicContext* mctx = (MusicContext*)ctx->user_data;
    if (!music_output_available(mctx)) {
        printf("No MIDI output open\n");
        return;
    }

    send_note_on_ctx(mctx, pitch, velocity);
    if (duration > 0) {
        shared_sleep_ms(mctx->shared, duration);
    }
    send_note_off_ctx(mctx, pitch);
}

void midi_note_on_(JoyContext* ctx) {
    REQUIRE(2, "midi-note-on");

    JoyValue vel_v = POP();
    JoyValue pitch_v = POP();

    EXPECT_TYPE(vel_v, JOY_INTEGER, "midi-note-on");
    EXPECT_TYPE(pitch_v, JOY_INTEGER, "midi-note-on");

    int pitch = (int)pitch_v.data.integer;
    int velocity = (int)vel_v.data.integer;

    MusicContext* mctx = (MusicContext*)ctx->user_data;
    send_note_on_ctx(mctx, pitch, velocity);
}

void midi_note_off_(JoyContext* ctx) {
    REQUIRE(1, "midi-note-off");

    JoyValue pitch_v = POP();
    EXPECT_TYPE(pitch_v, JOY_INTEGER, "midi-note-off");

    int pitch = (int)pitch_v.data.integer;

    MusicContext* mctx = (MusicContext*)ctx->user_data;
    send_note_off_ctx(mctx, pitch);
}

void midi_chord_(JoyContext* ctx) {
    REQUIRE(3, "midi-chord");

    JoyValue dur_v = POP();
    JoyValue vel_v = POP();
    JoyValue list_v = POP();

    EXPECT_TYPE(dur_v, JOY_INTEGER, "midi-chord");
    EXPECT_TYPE(vel_v, JOY_INTEGER, "midi-chord");
    EXPECT_TYPE(list_v, JOY_LIST, "midi-chord");

    int velocity = (int)vel_v.data.integer;
    int duration = (int)dur_v.data.integer;
    JoyList* pitches = list_v.data.list;

    MusicContext* mctx = (MusicContext*)ctx->user_data;
    if (!music_output_available(mctx)) {
        printf("No MIDI output open\n");
        joy_value_free(&list_v);
        return;
    }

    /* Note on for all pitches */
    for (size_t i = 0; i < pitches->length; i++) {
        if (pitches->items[i].type == JOY_INTEGER) {
            int pitch = (int)pitches->items[i].data.integer;
            send_note_on_ctx(mctx, pitch, velocity);
        }
    }

    if (duration > 0) {
        shared_sleep_ms(mctx->shared, duration);
    }

    /* Note off for all pitches */
    for (size_t i = 0; i < pitches->length; i++) {
        if (pitches->items[i].type == JOY_INTEGER) {
            int pitch = (int)pitches->items[i].data.integer;
            send_note_off_ctx(mctx, pitch);
        }
    }

    joy_value_free(&list_v);
}

/* ============================================================================
 * Control Messages
 * ============================================================================ */

void midi_cc_(JoyContext* ctx) {
    REQUIRE(2, "midi-cc");

    JoyValue val_v = POP();
    JoyValue cc_v = POP();

    EXPECT_TYPE(val_v, JOY_INTEGER, "midi-cc");
    EXPECT_TYPE(cc_v, JOY_INTEGER, "midi-cc");

    int cc = (int)cc_v.data.integer;
    int value = (int)val_v.data.integer;

    MusicContext* mctx = (MusicContext*)ctx->user_data;
    if (!music_output_available(mctx)) return;

    shared_send_cc(mctx->shared, mctx->channel, cc, value);
}

void midi_program_(JoyContext* ctx) {
    REQUIRE(1, "midi-program");

    JoyValue prog_v = POP();
    EXPECT_TYPE(prog_v, JOY_INTEGER, "midi-program");

    int program = (int)prog_v.data.integer;

    MusicContext* mctx = (MusicContext*)ctx->user_data;
    if (!music_output_available(mctx)) return;

    shared_send_program(mctx->shared, mctx->channel, program);
}

void midi_panic_(JoyContext* ctx) {
    MusicContext* mctx = (MusicContext*)ctx->user_data;
    if (!mctx || !mctx->shared) {
        printf("No music context available\n");
        return;
    }

    shared_send_panic(mctx->shared);
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

void midi_sleep_(JoyContext* ctx) {
    REQUIRE(1, "midi-sleep");

    JoyValue ms_v = POP();
    EXPECT_TYPE(ms_v, JOY_INTEGER, "midi-sleep");

    MusicContext* mctx = (MusicContext*)ctx->user_data;
    int ms = (int)ms_v.data.integer;
    if (ms > 0 && mctx && mctx->shared) {
        shared_sleep_ms(mctx->shared, ms);
    }
}

void pitch_(JoyContext* ctx) {
    REQUIRE(1, "pitch");

    JoyValue str_v = POP();
    EXPECT_TYPE(str_v, JOY_STRING, "pitch");

    int midi_num = music_parse_pitch(str_v.data.string);
    joy_value_free(&str_v);

    if (midi_num < 0) {
        joy_error("pitch: invalid pitch name");
        return;
    }

    PUSH(joy_integer(midi_num));
}

void tempo_(JoyContext* ctx) {
    REQUIRE(1, "tempo");

    JoyValue bpm_v = POP();
    EXPECT_TYPE(bpm_v, JOY_INTEGER, "tempo");

    int bpm = (int)bpm_v.data.integer;
    if (bpm < 1) bpm = 1;
    if (bpm > 999) bpm = 999;

    MusicContext* mctx = (MusicContext*)ctx->user_data;
    if (mctx) {
        mctx->tempo = bpm;
        /* Recalculate duration based on current note value and new tempo */
        mctx->duration_ms = music_duration_to_ms(mctx->duration_value, bpm);
    }
}

/* Helper to play with a specific duration */
static void play_with_duration(JoyContext* ctx, int value, const char* name) {
    MusicContext* mctx = (MusicContext*)ctx->user_data;
    if (!mctx) return;

    /* Save current duration */
    int old_value = mctx->duration_value;
    int old_ms = mctx->duration_ms;

    /* Set new duration */
    mctx->duration_value = value;
    mctx->duration_ms = music_duration_to_ms(value, mctx->tempo);

    /* Pop and play */
    if (ctx->stack->depth < 1) {
        joy_error_underflow(name, 1, ctx->stack->depth);
        return;
    }

    JoyValue val = POP();
    if (val.type == JOY_INTEGER) {
        /* Single note */
        PUSH(val);
        music_play_(ctx);
    } else if (val.type == JOY_LIST) {
        /* List of notes */
        PUSH(val);
        music_play_(ctx);
    } else {
        joy_error_type(name, "integer or list", val.type);
        joy_value_free(&val);
    }

    /* Restore duration */
    mctx->duration_value = old_value;
    mctx->duration_ms = old_ms;
}

void whole_(JoyContext* ctx) { play_with_duration(ctx, 1, "whole"); }
void half_(JoyContext* ctx) { play_with_duration(ctx, 2, "half"); }
void quarter_(JoyContext* ctx) { play_with_duration(ctx, 4, "quarter"); }
void eighth_(JoyContext* ctx) { play_with_duration(ctx, 8, "eighth"); }
void sixteenth_(JoyContext* ctx) { play_with_duration(ctx, 16, "sixteenth"); }

void quant_(JoyContext* ctx) {
    REQUIRE(1, "quant");

    JoyValue q_v = POP();
    EXPECT_TYPE(q_v, JOY_INTEGER, "quant");

    int q = (int)q_v.data.integer;
    if (q < 0) q = 0;
    if (q > 100) q = 100;

    MusicContext* mctx = (MusicContext*)ctx->user_data;
    if (mctx) {
        mctx->quantization = q;
    }
}

void vol_(JoyContext* ctx) {
    REQUIRE(1, "vol");

    JoyValue vol_v = POP();
    EXPECT_TYPE(vol_v, JOY_INTEGER, "vol");

    int vol = (int)vol_v.data.integer;
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;

    /* Scale 0-100 to 0-127 */
    int velocity = vol * 127 / 100;

    MusicContext* mctx = (MusicContext*)ctx->user_data;
    if (mctx) {
        mctx->velocity = velocity;
    }
}

/* ============================================================================
 * Music Theory
 * ============================================================================ */

void major_chord_(JoyContext* ctx) {
    REQUIRE(1, "major");

    JoyValue root_v = POP();
    EXPECT_TYPE(root_v, JOY_INTEGER, "major");

    int root = (int)root_v.data.integer;
    int pitches[3];
    music_build_chord(root, CHORD_MAJOR, CHORD_TRIAD_SIZE, pitches);

    JoyList* list = joy_list_new(3);
    for (int i = 0; i < 3; i++) {
        joy_list_push(list, joy_integer(pitches[i]));
    }

    JoyValue result = {.type = JOY_LIST};
    result.data.list = list;
    PUSH(result);
}

void minor_chord_(JoyContext* ctx) {
    REQUIRE(1, "minor");

    JoyValue root_v = POP();
    EXPECT_TYPE(root_v, JOY_INTEGER, "minor");

    int root = (int)root_v.data.integer;
    int pitches[3];
    music_build_chord(root, CHORD_MINOR, CHORD_TRIAD_SIZE, pitches);

    JoyList* list = joy_list_new(3);
    for (int i = 0; i < 3; i++) {
        joy_list_push(list, joy_integer(pitches[i]));
    }

    JoyValue result = {.type = JOY_LIST};
    result.data.list = list;
    PUSH(result);
}

void dim_chord_(JoyContext* ctx) {
    REQUIRE(1, "dim");

    JoyValue root_v = POP();
    EXPECT_TYPE(root_v, JOY_INTEGER, "dim");

    int root = (int)root_v.data.integer;
    int pitches[3];
    music_build_chord(root, CHORD_DIM, CHORD_TRIAD_SIZE, pitches);

    JoyList* list = joy_list_new(3);
    for (int i = 0; i < 3; i++) {
        joy_list_push(list, joy_integer(pitches[i]));
    }

    JoyValue result = {.type = JOY_LIST};
    result.data.list = list;
    PUSH(result);
}

void aug_chord_(JoyContext* ctx) {
    REQUIRE(1, "aug");

    JoyValue root_v = POP();
    EXPECT_TYPE(root_v, JOY_INTEGER, "aug");

    int root = (int)root_v.data.integer;
    int pitches[3];
    music_build_chord(root, CHORD_AUG, CHORD_TRIAD_SIZE, pitches);

    JoyList* list = joy_list_new(3);
    for (int i = 0; i < 3; i++) {
        joy_list_push(list, joy_integer(pitches[i]));
    }

    JoyValue result = {.type = JOY_LIST};
    result.data.list = list;
    PUSH(result);
}

void dom7_chord_(JoyContext* ctx) {
    REQUIRE(1, "dom7");

    JoyValue root_v = POP();
    EXPECT_TYPE(root_v, JOY_INTEGER, "dom7");

    int root = (int)root_v.data.integer;
    int pitches[4];
    music_build_chord(root, CHORD_DOM7, CHORD_7TH_SIZE, pitches);

    JoyList* list = joy_list_new(4);
    for (int i = 0; i < 4; i++) {
        joy_list_push(list, joy_integer(pitches[i]));
    }

    JoyValue result = {.type = JOY_LIST};
    result.data.list = list;
    PUSH(result);
}

void maj7_chord_(JoyContext* ctx) {
    REQUIRE(1, "maj7");

    JoyValue root_v = POP();
    EXPECT_TYPE(root_v, JOY_INTEGER, "maj7");

    int root = (int)root_v.data.integer;
    int pitches[4];
    music_build_chord(root, CHORD_MAJ7, CHORD_7TH_SIZE, pitches);

    JoyList* list = joy_list_new(4);
    for (int i = 0; i < 4; i++) {
        joy_list_push(list, joy_integer(pitches[i]));
    }

    JoyValue result = {.type = JOY_LIST};
    result.data.list = list;
    PUSH(result);
}

void min7_chord_(JoyContext* ctx) {
    REQUIRE(1, "min7");

    JoyValue root_v = POP();
    EXPECT_TYPE(root_v, JOY_INTEGER, "min7");

    int root = (int)root_v.data.integer;
    int pitches[4];
    music_build_chord(root, CHORD_MIN7, CHORD_7TH_SIZE, pitches);

    JoyList* list = joy_list_new(4);
    for (int i = 0; i < 4; i++) {
        joy_list_push(list, joy_integer(pitches[i]));
    }

    JoyValue result = {.type = JOY_LIST};
    result.data.list = list;
    PUSH(result);
}

void transpose_(JoyContext* ctx) {
    REQUIRE(2, "transpose");

    JoyValue n_v = POP();
    JoyValue pitch_v = POP();

    EXPECT_TYPE(n_v, JOY_INTEGER, "transpose");
    EXPECT_TYPE(pitch_v, JOY_INTEGER, "transpose");

    int pitch = (int)pitch_v.data.integer;
    int n = (int)n_v.data.integer;

    int result = pitch + n;
    /* Clamp to MIDI range */
    if (result < 0) result = 0;
    if (result > 127) result = 127;

    PUSH(joy_integer(result));
}

/* ============================================================================
 * Channel Operations
 * ============================================================================ */

void channel_(JoyContext* ctx) {
    /* N channel - set current MIDI channel (1-16) */
    REQUIRE(1, "channel");

    JoyValue n = POP();
    EXPECT_TYPE(n, JOY_INTEGER, "channel");

    int ch = (int)n.data.integer;
    if (ch < 1) ch = 1;
    if (ch > 16) ch = 16;

    MusicContext* mctx = (MusicContext*)ctx->user_data;
    if (mctx) {
        mctx->channel = ch;
    }
}

void chan_(JoyContext* ctx) {
    /* [P] N chan - execute quotation P on channel N, restore channel after */
    REQUIRE(2, "chan");

    JoyValue n = POP();
    JoyValue p = POP();
    EXPECT_TYPE(n, JOY_INTEGER, "chan");

    MusicContext* mctx = (MusicContext*)ctx->user_data;
    if (!mctx) {
        fprintf(stderr, "chan: no music context\n");
        joy_value_free(&p);
        return;
    }

    int old_channel = mctx->channel;
    int ch = (int)n.data.integer;
    if (ch < 1) ch = 1;
    if (ch > 16) ch = 16;
    mctx->channel = ch;

    /* Execute quotation or list */
    if (p.type == JOY_QUOTATION) {
        joy_execute_quotation(ctx, p.data.quotation);
    } else if (p.type == JOY_LIST) {
        /* Treat list as sequence of notes - build schedule and play async */
        if (!music_output_available(mctx)) {
            fprintf(stderr, "chan: no MIDI output (use midi-virtual or midi-open first)\n");
        } else {
            /* Build a schedule for all notes in the list */
            MidiSchedule* sched = schedule_new();
            if (sched) {
                int current_time = 0;
                for (size_t i = 0; i < p.data.list->length; i++) {
                    JoyValue item = p.data.list->items[i];
                    int pitch = -1;

                    if (item.type == JOY_INTEGER) {
                        pitch = (int)item.data.integer;
                    } else if (item.type == JOY_SYMBOL) {
                        pitch = music_parse_pitch(item.data.symbol);
                    }

                    if (pitch >= 0 && pitch != -1) {  /* -1 is REST_MARKER */
                        int play_dur = mctx->duration_ms * mctx->quantization / 100;
                        schedule_add_event(sched, current_time, ch, pitch,
                                          mctx->velocity, play_dur);
                    }
                    current_time += mctx->duration_ms;
                }

                /* Play asynchronously (non-blocking) */
                if (sched->count > 0) {
                    schedule_play_async_ctx(sched, mctx);
                }
                schedule_free(sched);
            }
        }
    } else {
        fprintf(stderr, "chan: expected quotation or list, got %s\n",
                p.type == JOY_INTEGER ? "integer" :
                p.type == JOY_FLOAT ? "float" :
                p.type == JOY_SYMBOL ? "symbol" : "unknown");
    }

    mctx->channel = old_channel;
    joy_value_free(&p);
}

/* ============================================================================
 * Ableton Link Primitives
 * ============================================================================ */

/* link-enable - enable Link tempo sync */
void link_enable_(JoyContext* ctx) {
    (void)ctx;
    if (joy_link_enable() == 0) {
        printf("Link enabled (tempo: %.1f BPM, peers: %d)\n",
               joy_link_get_tempo(), joy_link_num_peers());
    } else {
        printf("Failed to enable Link\n");
    }
}

/* link-disable - disable Link */
void link_disable_(JoyContext* ctx) {
    (void)ctx;
    joy_link_disable();
    printf("Link disabled\n");
}

/* link-tempo - get or set Link tempo: BPM link-tempo OR link-tempo */
void link_tempo_(JoyContext* ctx) {
    if (ctx->stack->depth >= 1 && ctx->stack->items[ctx->stack->depth - 1].type == JOY_INTEGER) {
        /* Set tempo */
        JoyValue v = POP();
        double bpm = (double)v.data.integer;
        if (bpm >= 20.0 && bpm <= 999.0) {
            joy_link_set_tempo(bpm);
            printf("Link tempo: %.1f BPM\n", bpm);
        } else {
            printf("Invalid tempo (must be 20-999)\n");
        }
    } else if (ctx->stack->depth >= 1 && ctx->stack->items[ctx->stack->depth - 1].type == JOY_FLOAT) {
        /* Set tempo (float) */
        JoyValue v = POP();
        double bpm = v.data.floating;
        if (bpm >= 20.0 && bpm <= 999.0) {
            joy_link_set_tempo(bpm);
            printf("Link tempo: %.1f BPM\n", bpm);
        } else {
            printf("Invalid tempo (must be 20-999)\n");
        }
    } else {
        /* Get tempo - push onto stack */
        double tempo = joy_link_get_tempo();
        if (tempo > 0) {
            PUSH(joy_float(tempo));
        } else {
            printf("Link not enabled\n");
        }
    }
}

/* link-beat - get current beat position (quantum on stack or default 4) */
void link_beat_(JoyContext* ctx) {
    double quantum = 4.0;
    if (ctx->stack->depth >= 1) {
        JoyValue v = ctx->stack->items[ctx->stack->depth - 1];
        if (v.type == JOY_INTEGER) {
            POP();
            quantum = (double)v.data.integer;
        } else if (v.type == JOY_FLOAT) {
            POP();
            quantum = v.data.floating;
        }
    }
    double beat = joy_link_get_beat(quantum);
    PUSH(joy_float(beat));
}

/* link-phase - get current phase within quantum */
void link_phase_(JoyContext* ctx) {
    double quantum = 4.0;
    if (ctx->stack->depth >= 1) {
        JoyValue v = ctx->stack->items[ctx->stack->depth - 1];
        if (v.type == JOY_INTEGER) {
            POP();
            quantum = (double)v.data.integer;
        } else if (v.type == JOY_FLOAT) {
            POP();
            quantum = v.data.floating;
        }
    }
    double phase = joy_link_get_phase(quantum);
    PUSH(joy_float(phase));
}

/* link-peers - get number of connected peers */
void link_peers_(JoyContext* ctx) {
    int peers = joy_link_num_peers();
    PUSH(joy_integer(peers));
}

/* link-status - print Link status */
void link_status_(JoyContext* ctx) {
    (void)ctx;
    if (joy_link_is_enabled()) {
        printf("Link: enabled, tempo: %.1f BPM, peers: %d, beat: %.2f\n",
               joy_link_get_tempo(), joy_link_num_peers(),
               joy_link_get_beat(4.0));
    } else {
        printf("Link: disabled\n");
    }
}

/* ============================================================================
 * Csound Primitives
 * ============================================================================ */

/* cs-load - load a CSD file: "path.csd" cs-load */
void cs_load_(JoyContext* ctx) {
    if (ctx->stack->depth < 1) {
        printf("cs-load requires a path string\n");
        return;
    }
    JoyValue v = POP();
    if (v.type != JOY_STRING) {
        printf("cs-load requires a string path\n");
        return;
    }
    MusicContext* mctx = (MusicContext*)ctx->user_data;
    if (joy_csound_load(v.data.string) == 0) {
        printf("Csound: Loaded %s\n", v.data.string);
        /* Auto-enable Csound after successful load */
        if (mctx && mctx->shared && joy_csound_enable(mctx->shared) == 0) {
            printf("Csound enabled\n");
        }
    } else {
        const char* err = joy_csound_get_error();
        printf("Csound: Failed to load%s%s\n", err ? ": " : "", err ? err : "");
    }
}

/* cs-enable - enable Csound as audio backend */
void cs_enable_(JoyContext* ctx) {
    MusicContext* mctx = (MusicContext*)ctx->user_data;
    if (mctx && mctx->shared && joy_csound_enable(mctx->shared) == 0) {
        printf("Csound enabled\n");
    } else {
        const char* err = joy_csound_get_error();
        printf("Csound: Failed to enable%s%s\n", err ? ": " : "", err ? err : "");
    }
}

/* cs-disable - disable Csound */
void cs_disable_(JoyContext* ctx) {
    MusicContext* mctx = (MusicContext*)ctx->user_data;
    if (mctx && mctx->shared) {
        joy_csound_disable(mctx->shared);
    }
    printf("Csound disabled\n");
}

/* cs-status - print Csound status */
void cs_status_(JoyContext* ctx) {
    MusicContext* mctx = (MusicContext*)ctx->user_data;
    if (mctx && mctx->shared && joy_csound_is_enabled(mctx->shared)) {
        printf("Csound: enabled\n");
    } else {
        printf("Csound: disabled\n");
    }
}

/* cs-play - play a CSD file (blocking): "path.csd" cs-play */
void cs_play_(JoyContext* ctx) {
    if (ctx->stack->depth < 1) {
        printf("cs-play requires a path string\n");
        return;
    }
    JoyValue v = POP();
    if (v.type != JOY_STRING) {
        printf("cs-play requires a string path\n");
        return;
    }
    printf("Playing %s (Ctrl-C to stop)...\n", v.data.string);
    joy_csound_play_file(v.data.string, 1);
}

/* ============================================================================
 * Initialization / Cleanup
 * ============================================================================ */

/* These are now no-ops - context lifecycle is managed externally */
void midi_init(void) {
    /* Context is set up by REPL/editor before primitives are called */
}

void midi_cleanup(void) {
    /* Context cleanup is handled by REPL/editor */
}

/* ============================================================================
 * Schedule System Implementation
 * ============================================================================ */

/* Scheduling mode state */
static bool g_scheduling_mode = false;
static int g_schedule_channel = 1;
static int g_schedule_time = 0;
static MidiSchedule* g_current_schedule = NULL;

/* Global accumulator state */
static MidiSchedule* g_accumulator = NULL;
static int g_accumulator_offset = 0;

/* Create a new empty schedule */
MidiSchedule* schedule_new(void) {
    MidiSchedule* sched = malloc(sizeof(MidiSchedule));
    sched->events = NULL;
    sched->count = 0;
    sched->capacity = 0;
    sched->total_duration_ms = 0;
    return sched;
}

/* Free a schedule */
void schedule_free(MidiSchedule* sched) {
    if (sched) {
        free(sched->events);
        free(sched);
    }
}

/* Add an event to a schedule */
void schedule_add_event(MidiSchedule* sched, int time_ms, int channel,
                        int pitch, int velocity, int duration_ms) {
    if (!sched) return;

    /* Grow capacity if needed */
    if (sched->count >= sched->capacity) {
        size_t new_cap = sched->capacity == 0 ? 64 : sched->capacity * 2;
        sched->events = realloc(sched->events, new_cap * sizeof(ScheduledEvent));
        sched->capacity = new_cap;
    }

    /* Add the event */
    ScheduledEvent* ev = &sched->events[sched->count++];
    ev->time_ms = time_ms;
    ev->channel = channel;
    ev->pitch = pitch;
    ev->velocity = velocity;
    ev->duration_ms = duration_ms;

    /* Update total duration */
    int end_time = time_ms + duration_ms;
    if (end_time > sched->total_duration_ms) {
        sched->total_duration_ms = end_time;
    }
}

/* Compare events by time for sorting */
static int compare_events(const void* a, const void* b) {
    const ScheduledEvent* ea = (const ScheduledEvent*)a;
    const ScheduledEvent* eb = (const ScheduledEvent*)b;
    return ea->time_ms - eb->time_ms;
}

/* Debug flag - set to true to see scheduled events */
static bool g_schedule_debug = false;

void schedule_set_debug(bool enable) {
    g_schedule_debug = enable;
}

/* midi-debug primitive - toggle debug mode */
void midi_debug_(JoyContext* ctx) {
    (void)ctx;
    g_schedule_debug = !g_schedule_debug;
    printf("Schedule debug: %s\n", g_schedule_debug ? "ON" : "OFF");
}

/* Play a schedule - sorts events and plays them with proper timing */
void schedule_play_ctx(MidiSchedule* sched, MusicContext* mctx) {
    if (!sched || sched->count == 0) return;

    /* Sort events by time */
    qsort(sched->events, sched->count, sizeof(ScheduledEvent), compare_events);

    if (g_schedule_debug) {
        printf("=== Schedule: %zu events, duration %d ms ===\n",
               sched->count, sched->total_duration_ms);
        for (size_t i = 0; i < sched->count && i < 30; i++) {
            ScheduledEvent* ev = &sched->events[i];
            printf("  t=%4d ch=%d pitch=%3d vel=%3d dur=%d\n",
                   ev->time_ms, ev->channel, ev->pitch, ev->velocity, ev->duration_ms);
        }
        if (sched->count > 30) printf("  ... (%zu more)\n", sched->count - 30);
    }

    /* Skip actual playback if no MIDI output */
    if (!music_output_available(mctx)) return;

    /* Track active notes for note-off scheduling */
    typedef struct { int pitch; int channel; int off_time; } ActiveNote;
    ActiveNote* active = malloc(sched->count * sizeof(ActiveNote));
    size_t active_count = 0;

    int current_time = 0;
    size_t event_idx = 0;

    while (event_idx < sched->count || active_count > 0) {
        /* Find next event time */
        int next_event_time = INT_MAX;
        if (event_idx < sched->count) {
            next_event_time = sched->events[event_idx].time_ms;
        }

        /* Find next note-off time */
        int next_off_time = INT_MAX;
        for (size_t i = 0; i < active_count; i++) {
            if (active[i].off_time < next_off_time) {
                next_off_time = active[i].off_time;
            }
        }

        /* Determine what happens next */
        int next_time = (next_event_time < next_off_time) ? next_event_time : next_off_time;
        if (next_time == INT_MAX) break;

        /* Sleep until next event */
        if (next_time > current_time) {
            shared_sleep_ms(mctx->shared, next_time - current_time);
            current_time = next_time;
        }

        /* Process note-offs first */
        for (size_t i = 0; i < active_count; ) {
            if (active[i].off_time <= current_time) {
                /* Send note-off */
                shared_send_note_off(mctx->shared, active[i].channel, active[i].pitch);

                /* Remove from active list */
                active[i] = active[--active_count];
            } else {
                i++;
            }
        }

        /* Process note-ons */
        while (event_idx < sched->count &&
               sched->events[event_idx].time_ms <= current_time) {
            ScheduledEvent* ev = &sched->events[event_idx];

            /* Send note-on */
            shared_send_note_on(mctx->shared, ev->channel, ev->pitch, ev->velocity);

            /* Add to active notes */
            active[active_count].pitch = ev->pitch;
            active[active_count].channel = ev->channel;
            active[active_count].off_time = ev->time_ms + ev->duration_ms;
            active_count++;

            event_idx++;
        }
    }

    free(active);
}

/* Legacy wrapper for backward compatibility */
void schedule_play(MidiSchedule* sched) {
    /* This should not be called in the new architecture */
    (void)sched;
    fprintf(stderr, "schedule_play: use schedule_play_ctx instead\n");
}

/* Async playback wrapper - uses joy_async for non-blocking playback */
int schedule_play_async_ctx(MidiSchedule* sched, MusicContext* mctx) {
    return joy_async_play(sched, mctx);
}

/* Begin scheduling mode for a channel */
void schedule_begin(int channel) {
    g_scheduling_mode = true;
    g_schedule_channel = channel;
    g_schedule_time = 0;
    if (!g_current_schedule) {
        g_current_schedule = schedule_new();
    }
}

/* End scheduling mode */
void schedule_end(void) {
    g_scheduling_mode = false;
}

/* Check if in scheduling mode */
bool is_scheduling(void) {
    return g_scheduling_mode;
}

/* Get current scheduling channel */
int get_schedule_channel(void) {
    return g_schedule_channel;
}

/* Get current time offset in schedule */
int get_schedule_time(void) {
    return g_schedule_time;
}

/* Advance time in current schedule */
void advance_schedule_time(int ms) {
    g_schedule_time += ms;
}

/* Get the current schedule being built */
MidiSchedule* get_current_schedule(void) {
    return g_current_schedule;
}

/* Clear the current schedule (for starting a new part) */
void clear_current_schedule(void) {
    if (g_current_schedule) {
        schedule_free(g_current_schedule);
    }
    g_current_schedule = schedule_new();
    g_schedule_time = 0;
}

/* Initialize the accumulator */
void accumulator_init(void) {
    if (!g_accumulator) {
        g_accumulator = schedule_new();
    }
    g_accumulator_offset = 0;
}

/* Add a schedule to the accumulator (with current offset) */
void accumulator_add_schedule(MidiSchedule* sched) {
    if (!sched || !g_accumulator) return;

    for (size_t i = 0; i < sched->count; i++) {
        ScheduledEvent* ev = &sched->events[i];
        schedule_add_event(g_accumulator,
                          ev->time_ms + g_accumulator_offset,
                          ev->channel, ev->pitch, ev->velocity, ev->duration_ms);
    }
}

/* Flush the accumulator - play asynchronously and clear */
void accumulator_flush_ctx(MusicContext* mctx) {
    if (g_accumulator && g_accumulator->count > 0) {
        /* Use async playback so REPL remains responsive */
        schedule_play_async_ctx(g_accumulator, mctx);
        schedule_free(g_accumulator);
        g_accumulator = NULL;
    }
    g_accumulator_offset = 0;
}

/* Legacy wrapper */
void accumulator_flush(void) {
    fprintf(stderr, "accumulator_flush: use accumulator_flush_ctx instead\n");
}

/* Get current accumulator time offset */
int accumulator_get_offset(void) {
    return g_accumulator_offset;
}

/* Advance accumulator offset for next sequence */
void accumulator_advance(int ms) {
    g_accumulator_offset += ms;
}

/* Execute a sequence definition - called from joy_runtime.c */
void joy_execute_seq(JoyContext* ctx, SeqDefinition* seq) {
    if (!seq || seq->part_count == 0) return;

    /* Initialize accumulator if needed */
    if (!g_accumulator) {
        accumulator_init();
    }

    /* Create a merged schedule for all parts */
    MidiSchedule* merged = schedule_new();

    /* Execute each part in scheduling mode */
    for (size_t i = 0; i < seq->part_count; i++) {
        SeqPart* part = &seq->parts[i];

        /* Clear current schedule and enter scheduling mode */
        clear_current_schedule();
        schedule_begin(part->channel);

        /* Execute the part's quotation */
        joy_execute_quotation(ctx, part->quotation);

        /* If items remain on stack, play them as notes.
         * IMPORTANT: Stack is LIFO, so we must collect all items first
         * and then process in reverse order (bottom to top = original order).
         */
        MidiSchedule* sched = get_current_schedule();
        MusicContext* mctx = (MusicContext*)ctx->user_data;

        if (sched && mctx && ctx->stack->depth > 0) {
            /* Collect all playable items from stack */
            JoyValue* collected = malloc(ctx->stack->depth * sizeof(JoyValue));
            size_t collected_count = 0;

            while (ctx->stack->depth > 0) {
                JoyValue top = joy_stack_peek(ctx->stack);
                if (top.type == JOY_LIST || top.type == JOY_INTEGER) {
                    collected[collected_count++] = joy_stack_pop(ctx->stack);
                } else {
                    break;  /* Unknown type - leave on stack */
                }
            }

            /* Process in reverse order (to restore original sequence) */
            for (size_t j = collected_count; j > 0; j--) {
                JoyValue val = collected[j - 1];
                if (val.type == JOY_LIST) {
                    /* Play the list as sequential notes */
                    for (size_t k = 0; k < val.data.list->length; k++) {
                        if (val.data.list->items[k].type == JOY_INTEGER) {
                            int pitch = (int)val.data.list->items[k].data.integer;
                            int play_dur = mctx->duration_ms * mctx->quantization / 100;
                            /* Skip rests (pitch=-1), just advance time */
                            if (pitch != -1) {
                                schedule_add_event(sched, get_schedule_time(),
                                                  get_schedule_channel(), pitch,
                                                  mctx->velocity, play_dur);
                            }
                            advance_schedule_time(mctx->duration_ms);
                        }
                    }
                    joy_value_free(&val);
                } else if (val.type == JOY_INTEGER) {
                    /* Single note - play it */
                    int pitch = (int)val.data.integer;
                    int play_dur = mctx->duration_ms * mctx->quantization / 100;
                    /* Skip rests (pitch=-1), just advance time */
                    if (pitch != -1) {
                        schedule_add_event(sched, get_schedule_time(),
                                          get_schedule_channel(), pitch,
                                          mctx->velocity, play_dur);
                    }
                    advance_schedule_time(mctx->duration_ms);
                }
            }

            free(collected);
        }

        /* Get the schedule built by this part */
        MidiSchedule* part_sched = get_current_schedule();

        /* Merge into combined schedule (all parts start at time 0) */
        if (part_sched) {
            for (size_t j = 0; j < part_sched->count; j++) {
                ScheduledEvent* ev = &part_sched->events[j];
                schedule_add_event(merged, ev->time_ms, ev->channel,
                                  ev->pitch, ev->velocity, ev->duration_ms);
            }
        }

        schedule_end();
    }

    /* Add merged schedule to accumulator with current offset */
    accumulator_add_schedule(merged);

    /* Advance accumulator offset by this sequence's duration */
    accumulator_advance(merged->total_duration_ms);

    /* Clean up */
    schedule_free(merged);
    clear_current_schedule();
}

/* ============================================================================
 * Parameter System Primitives
 * ============================================================================ */

/* param - Get parameter value: "name" param -> value
 * Pushes the parameter value onto the stack, or 0 if not found */
void param_get_(JoyContext* ctx) {
    REQUIRE(1, "param");
    JoyValue v = POP();

    /* Accept string or symbol */
    const char* name = NULL;
    if (v.type == JOY_STRING) {
        name = v.data.string;
    } else if (v.type == JOY_SYMBOL) {
        name = v.data.symbol;
    } else {
        joy_error_type("param", "string or symbol", v.type);
        return;
    }

    MusicContext* mctx = (MusicContext*)ctx->user_data;
    float value = 0.0f;
    if (mctx && mctx->shared) {
        shared_param_get(mctx->shared, name, &value);
    }

    joy_value_free(&v);
    PUSH(joy_float(value));
}

/* param! - Set parameter value: value "name" param! -> */
void param_set_(JoyContext* ctx) {
    REQUIRE(2, "param!");
    JoyValue name_v = POP();
    JoyValue val_v = POP();

    /* Accept string or symbol for name */
    const char* name = NULL;
    if (name_v.type == JOY_STRING) {
        name = name_v.data.string;
    } else if (name_v.type == JOY_SYMBOL) {
        name = name_v.data.symbol;
    } else {
        joy_value_free(&name_v);
        joy_value_free(&val_v);
        joy_error_type("param!", "string or symbol for name", name_v.type);
        return;
    }

    /* Accept number for value */
    float value;
    if (val_v.type == JOY_INTEGER) {
        value = (float)val_v.data.integer;
    } else if (val_v.type == JOY_FLOAT) {
        value = (float)val_v.data.floating;
    } else {
        joy_value_free(&name_v);
        joy_value_free(&val_v);
        joy_error_type("param!", "integer or float for value", val_v.type);
        return;
    }

    MusicContext* mctx = (MusicContext*)ctx->user_data;
    if (mctx && mctx->shared) {
        if (shared_param_set(mctx->shared, name, value) != 0) {
            fprintf(stderr, "param!: unknown parameter '%s'\n", name);
        }
    }

    joy_value_free(&name_v);
    joy_value_free(&val_v);
}

/* param-list - List all parameters: param-list -> list */
void param_list_(JoyContext* ctx) {
    MusicContext* mctx = (MusicContext*)ctx->user_data;
    if (!mctx || !mctx->shared) {
        PUSH(joy_list_empty());
        return;
    }

    JoyValue list = joy_list_empty();
    for (int i = 0; i < PARAM_MAX_COUNT; i++) {
        const SharedParam* p = shared_param_at(mctx->shared, i);
        if (p) {
            /* Push parameter name as symbol */
            joy_list_push(list.data.list, joy_symbol(p->name));
        }
    }
    PUSH(list);
}

/* ============================================================================
 * MIDI Primitives Registration
 * ============================================================================ */

void joy_midi_register_primitives(JoyContext* ctx) {
    JoyDict* dict = ctx->dictionary;

    /* Port management */
    joy_dict_define_primitive(dict, "midi-list", midi_list_);
    joy_dict_define_primitive(dict, "midi-virtual", midi_virtual_);
    joy_dict_define_primitive(dict, "midi-open", midi_open_);
    joy_dict_define_primitive(dict, "midi-close", midi_close_);

    /* Note operations */
    joy_dict_define_primitive(dict, "midi-note", midi_note_);
    joy_dict_define_primitive(dict, "midi-note-on", midi_note_on_);
    joy_dict_define_primitive(dict, "midi-note-off", midi_note_off_);
    joy_dict_define_primitive(dict, "midi-chord", midi_chord_);

    /* Control messages */
    joy_dict_define_primitive(dict, "midi-cc", midi_cc_);
    joy_dict_define_primitive(dict, "midi-program", midi_program_);
    joy_dict_define_primitive(dict, "midi-panic", midi_panic_);

    /* Utilities */
    joy_dict_define_primitive(dict, "midi-sleep", midi_sleep_);
    joy_dict_define_primitive(dict, "pitch", pitch_);
    joy_dict_define_primitive(dict, "tempo", tempo_);
    joy_dict_define_primitive(dict, "quant", quant_);
    joy_dict_define_primitive(dict, "vol", vol_);

    /* Note durations */
    joy_dict_define_primitive(dict, "whole", whole_);
    joy_dict_define_primitive(dict, "half", half_);
    joy_dict_define_primitive(dict, "quarter", quarter_);
    joy_dict_define_primitive(dict, "eighth", eighth_);
    joy_dict_define_primitive(dict, "8th", eighth_);
    joy_dict_define_primitive(dict, "sixteenth", sixteenth_);
    joy_dict_define_primitive(dict, "16th", sixteenth_);

    /* Music theory */
    joy_dict_define_primitive(dict, "major", major_chord_);
    joy_dict_define_primitive(dict, "minor", minor_chord_);
    joy_dict_define_primitive(dict, "dim", dim_chord_);
    joy_dict_define_primitive(dict, "aug", aug_chord_);
    joy_dict_define_primitive(dict, "dom7", dom7_chord_);
    joy_dict_define_primitive(dict, "maj7", maj7_chord_);
    joy_dict_define_primitive(dict, "min7", min7_chord_);
    joy_dict_define_primitive(dict, "transpose", transpose_);

    /* Musical notation playback */
    joy_dict_define_primitive(dict, "play", music_play_);
    joy_dict_define_primitive(dict, "chord", music_chord_);

    /* Channel operations */
    joy_dict_define_primitive(dict, "channel", channel_);
    joy_dict_define_primitive(dict, "chan", chan_);

    /* Debug */
    joy_dict_define_primitive(dict, "midi-debug", midi_debug_);

    /* Ableton Link */
    joy_dict_define_primitive(dict, "link-enable", link_enable_);
    joy_dict_define_primitive(dict, "link-disable", link_disable_);
    joy_dict_define_primitive(dict, "link-tempo", link_tempo_);
    joy_dict_define_primitive(dict, "link-beat", link_beat_);
    joy_dict_define_primitive(dict, "link-phase", link_phase_);
    joy_dict_define_primitive(dict, "link-peers", link_peers_);
    joy_dict_define_primitive(dict, "link-status", link_status_);

    /* Csound */
    joy_dict_define_primitive(dict, "cs-load", cs_load_);
    joy_dict_define_primitive(dict, "cs-enable", cs_enable_);
    joy_dict_define_primitive(dict, "cs-disable", cs_disable_);
    joy_dict_define_primitive(dict, "cs-status", cs_status_);
    joy_dict_define_primitive(dict, "cs-play", cs_play_);

    /* Parameter system */
    joy_dict_define_primitive(dict, "param", param_get_);
    joy_dict_define_primitive(dict, "param!", param_set_);
    joy_dict_define_primitive(dict, "param-list", param_list_);

    /* Set up post-eval hook for SEQ playback */
    ctx->post_eval_hook = accumulator_flush;
}
