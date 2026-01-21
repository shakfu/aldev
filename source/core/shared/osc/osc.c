/* osc.c - Open Sound Control implementation using liblo
 *
 * Provides OSC server for receiving commands and client for sending events.
 * Uses liblo's threaded server for non-blocking operation.
 */

#ifdef PSND_OSC

#include "osc.h"
#include "../context.h"
#include "../param/param.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>

/* ======================= Rate Limiting ===================================== */

/* Rate limit for MIDI note messages (messages per second, 0 = unlimited) */
static int osc_note_rate_limit = 0;  /* Default: no rate limiting */
static struct timeval osc_last_note_time = {0, 0};
static int osc_note_count_this_second = 0;

/* Get current time in milliseconds */
static long long osc_get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/* Check if we should rate limit a note message */
static int osc_should_rate_limit_note(void) {
    if (osc_note_rate_limit <= 0) return 0;  /* No rate limiting */

    struct timeval now;
    gettimeofday(&now, NULL);

    /* Check if we're in a new second */
    if (now.tv_sec != osc_last_note_time.tv_sec) {
        osc_last_note_time = now;
        osc_note_count_this_second = 1;
        return 0;  /* First message this second, allow it */
    }

    /* Check if we've exceeded the rate limit */
    if (osc_note_count_this_second >= osc_note_rate_limit) {
        return 1;  /* Rate limited */
    }

    osc_note_count_this_second++;
    return 0;  /* Allow message */
}

/* Forward declarations for session/editor access */
struct EditorSession;
struct editor_ctx;

/* ======================= Callback Function Pointers ======================== */

/*
 * These function pointers are set by the editor when OSC is initialized.
 * This avoids linking issues with the shared library being used by tests
 * that don't link against the full editor.
 */
static int (*osc_lang_eval_fn)(struct editor_ctx *ctx, const char *code) = NULL;
static int (*osc_lang_eval_buffer_fn)(struct editor_ctx *ctx) = NULL;
static void (*osc_lang_stop_all_fn)(struct editor_ctx *ctx) = NULL;

/* Query callbacks - set by editor for query/reply handlers */
static int (*osc_lang_is_playing_fn)(struct editor_ctx *ctx) = NULL;
static const char *(*osc_get_filename_fn)(struct editor_ctx *ctx) = NULL;
static void (*osc_get_position_fn)(struct editor_ctx *ctx, int *line, int *col) = NULL;

/* ======================= Session Access ==================================== */

/* Get editor context from SharedContext via osc_user_data (session) */
static struct editor_ctx *osc_get_editor_ctx(SharedContext *ctx) {
    if (!ctx || !ctx->osc_user_data) return NULL;
    /* osc_user_data points to EditorSession, which has ctx as first member */
    /* EditorSession { editor_ctx_t ctx; ... } */
    return (struct editor_ctx *)ctx->osc_user_data;
}

/* ======================= Error Handler ==================================== */

static void osc_error_handler(int num, const char *msg, const char *path) {
    fprintf(stderr, "[OSC] Error %d: %s", num, msg);
    if (path) {
        fprintf(stderr, " (path: %s)", path);
    }
    fprintf(stderr, "\n");
}

/* ======================= Built-in Handlers ================================ */

/* Generic handler that catches all messages for debugging */
static int osc_generic_handler(const char *path, const char *types,
                                lo_arg **argv, int argc,
                                lo_message msg, void *user_data) {
    (void)msg;
    (void)user_data;

    fprintf(stderr, "[OSC] Received: %s [%s]", path, types ? types : "");
    for (int i = 0; i < argc; i++) {
        fprintf(stderr, " ");
        if (types) {
            switch (types[i]) {
                case 'i': fprintf(stderr, "%d", argv[i]->i); break;
                case 'f': fprintf(stderr, "%.2f", argv[i]->f); break;
                case 's': fprintf(stderr, "\"%s\"", &argv[i]->s); break;
                case 'd': fprintf(stderr, "%.4f", argv[i]->d); break;
                default: fprintf(stderr, "?"); break;
            }
        }
    }
    fprintf(stderr, "\n");

    return 1; /* Pass to other handlers */
}

/* /psnd/ping - Reply with pong */
static int osc_ping_handler(const char *path, const char *types,
                            lo_arg **argv, int argc,
                            lo_message msg, void *user_data) {
    (void)path; (void)types; (void)argv; (void)argc; (void)user_data;

    lo_address src = lo_message_get_source(msg);
    if (src) {
        lo_send(src, "/psnd/pong", "");
    }
    return 0;
}

/* /psnd/tempo f:bpm - Set tempo */
static int osc_tempo_handler(const char *path, const char *types,
                             lo_arg **argv, int argc,
                             lo_message msg, void *user_data) {
    (void)path; (void)types; (void)msg;
    SharedContext *ctx = (SharedContext *)user_data;

    if (argc >= 1 && ctx) {
        float tempo = argv[0]->f;
        if (tempo > 0 && tempo < 1000) {
            ctx->tempo = (int)tempo;
            fprintf(stderr, "[OSC] Tempo set to %.1f BPM\n", tempo);
            /* Broadcast tempo change if enabled */
            shared_osc_send_tempo(ctx, tempo);
        }
    }
    return 0;
}

/* /psnd/note iii:pitch,velocity,duration or iiii:channel,pitch,velocity,duration */
static int osc_note_handler(const char *path, const char *types,
                            lo_arg **argv, int argc,
                            lo_message msg, void *user_data) {
    (void)path; (void)types; (void)msg;
    SharedContext *ctx = (SharedContext *)user_data;

    if (!ctx) return 0;

    int channel = ctx->default_channel;
    int pitch, velocity, duration;

    if (argc >= 4) {
        /* /psnd/note iiii: channel, pitch, velocity, duration */
        channel = argv[0]->i;
        pitch = argv[1]->i;
        velocity = argv[2]->i;
        duration = argv[3]->i;
    } else if (argc >= 3) {
        /* /psnd/note iii: pitch, velocity, duration */
        pitch = argv[0]->i;
        velocity = argv[1]->i;
        duration = argv[2]->i;
    } else {
        return 0;
    }

    /* Validate ranges */
    if (pitch < 0 || pitch > 127) return 0;
    if (velocity < 0 || velocity > 127) return 0;
    if (duration < 0 || duration > 60000) return 0;
    if (channel < 0 || channel > 15) channel = 0;

    fprintf(stderr, "[OSC] Note: ch=%d pitch=%d vel=%d dur=%d\n",
            channel, pitch, velocity, duration);

    /* Send note on */
    shared_send_note_on(ctx, channel + 1, pitch, velocity);

    /* Note: For proper note-off scheduling, we'd need async support.
     * For now, this just sends note-on. Use noteon/noteoff for control. */
    (void)duration;

    return 0;
}

/* /psnd/noteon ii:pitch,velocity or iii:channel,pitch,velocity */
static int osc_noteon_handler(const char *path, const char *types,
                              lo_arg **argv, int argc,
                              lo_message msg, void *user_data) {
    (void)path; (void)types; (void)msg;
    SharedContext *ctx = (SharedContext *)user_data;

    if (!ctx) return 0;

    int channel = ctx->default_channel;
    int pitch, velocity;

    if (argc >= 3) {
        channel = argv[0]->i;
        pitch = argv[1]->i;
        velocity = argv[2]->i;
    } else if (argc >= 2) {
        pitch = argv[0]->i;
        velocity = argv[1]->i;
    } else {
        return 0;
    }

    if (pitch < 0 || pitch > 127) return 0;
    if (velocity < 0 || velocity > 127) return 0;
    if (channel < 0 || channel > 15) channel = 0;

    fprintf(stderr, "[OSC] NoteOn: ch=%d pitch=%d vel=%d\n", channel, pitch, velocity);

    /* Send note on (channel is 1-based in shared API) */
    shared_send_note_on(ctx, channel + 1, pitch, velocity);

    /* Broadcast if enabled */
    shared_osc_send_note(ctx, channel, pitch, velocity);

    return 0;
}

/* /psnd/noteoff i:pitch or ii:channel,pitch */
static int osc_noteoff_handler(const char *path, const char *types,
                               lo_arg **argv, int argc,
                               lo_message msg, void *user_data) {
    (void)path; (void)types; (void)msg;
    SharedContext *ctx = (SharedContext *)user_data;

    if (!ctx) return 0;

    int channel = ctx->default_channel;
    int pitch;

    if (argc >= 2) {
        channel = argv[0]->i;
        pitch = argv[1]->i;
    } else if (argc >= 1) {
        pitch = argv[0]->i;
    } else {
        return 0;
    }

    if (pitch < 0 || pitch > 127) return 0;
    if (channel < 0 || channel > 15) channel = 0;

    fprintf(stderr, "[OSC] NoteOff: ch=%d pitch=%d\n", channel, pitch);

    /* Send note off (channel is 1-based in shared API) */
    shared_send_note_off(ctx, channel + 1, pitch);

    return 0;
}

/* /psnd/cc ii:cc,value or iii:channel,cc,value */
static int osc_cc_handler(const char *path, const char *types,
                          lo_arg **argv, int argc,
                          lo_message msg, void *user_data) {
    (void)path; (void)types; (void)msg;
    SharedContext *ctx = (SharedContext *)user_data;

    if (!ctx) return 0;

    int channel = ctx->default_channel;
    int cc, value;

    if (argc >= 3) {
        channel = argv[0]->i;
        cc = argv[1]->i;
        value = argv[2]->i;
    } else if (argc >= 2) {
        cc = argv[0]->i;
        value = argv[1]->i;
    } else {
        return 0;
    }

    if (cc < 0 || cc > 127) return 0;
    if (value < 0 || value > 127) return 0;
    if (channel < 0 || channel > 15) channel = 0;

    fprintf(stderr, "[OSC] CC: ch=%d cc=%d val=%d\n", channel, cc, value);

    /* Send control change (channel is 1-based in shared API) */
    shared_send_cc(ctx, channel + 1, cc, value);

    return 0;
}

/* /psnd/pc i:program or ii:channel,program - Program change */
static int osc_pc_handler(const char *path, const char *types,
                          lo_arg **argv, int argc,
                          lo_message msg, void *user_data) {
    (void)path; (void)types; (void)msg;
    SharedContext *ctx = (SharedContext *)user_data;

    if (!ctx) return 0;

    int channel = ctx->default_channel;
    int program;

    if (argc >= 2) {
        channel = argv[0]->i;
        program = argv[1]->i;
    } else if (argc >= 1) {
        program = argv[0]->i;
    } else {
        return 0;
    }

    if (program < 0 || program > 127) return 0;
    if (channel < 0 || channel > 15) channel = 0;

    fprintf(stderr, "[OSC] Program Change: ch=%d prog=%d\n", channel, program);

    /* Send program change (channel is 1-based in shared API) */
    shared_send_program(ctx, channel + 1, program);

    return 0;
}

/* /psnd/bend i:value or ii:channel,value - Pitch bend */
static int osc_bend_handler(const char *path, const char *types,
                            lo_arg **argv, int argc,
                            lo_message msg, void *user_data) {
    (void)path; (void)types; (void)msg;
    SharedContext *ctx = (SharedContext *)user_data;

    if (!ctx) return 0;

    int channel = ctx->default_channel;
    int value;

    if (argc >= 2) {
        channel = argv[0]->i;
        value = argv[1]->i;
    } else if (argc >= 1) {
        value = argv[0]->i;
    } else {
        return 0;
    }

    /* Pitch bend value: -8192 to 8191 (center = 0) */
    if (value < -8192) value = -8192;
    if (value > 8191) value = 8191;
    if (channel < 0 || channel > 15) channel = 0;

    fprintf(stderr, "[OSC] Pitch Bend: ch=%d val=%d\n", channel, value);

    /* Convert to 14-bit MIDI pitch bend (0-16383, center = 8192) */
    int midi_bend = value + 8192;
    int lsb = midi_bend & 0x7F;
    int msb = (midi_bend >> 7) & 0x7F;

    /* Send pitch bend via MIDI (channel is 1-based in shared API) */
    if (ctx->midi_out) {
        unsigned char bend_msg[3] = {
            (unsigned char)(0xE0 | channel),
            (unsigned char)lsb,
            (unsigned char)msb
        };
        libremidi_midi_out_send_message(ctx->midi_out, bend_msg, 3);
    }

    return 0;
}

/* /psnd/panic - All notes off */
static int osc_panic_handler(const char *path, const char *types,
                             lo_arg **argv, int argc,
                             lo_message msg, void *user_data) {
    (void)path; (void)types; (void)argv; (void)argc; (void)msg;
    SharedContext *ctx = (SharedContext *)user_data;

    if (!ctx) return 0;

    fprintf(stderr, "[OSC] Panic - all notes off\n");

    /* Send panic to all backends */
    shared_send_panic(ctx);

    return 0;
}

/* /psnd/play - Play entire file */
static int osc_play_handler(const char *path, const char *types,
                            lo_arg **argv, int argc,
                            lo_message msg, void *user_data) {
    (void)path; (void)types; (void)argv; (void)argc; (void)msg;
    SharedContext *ctx = (SharedContext *)user_data;

    if (!ctx) return 0;

    struct editor_ctx *editor = osc_get_editor_ctx(ctx);
    if (!editor || !osc_lang_eval_buffer_fn) {
        fprintf(stderr, "[OSC] Play: no editor context or callback available\n");
        return 0;
    }

    fprintf(stderr, "[OSC] Play: evaluating entire buffer\n");

    /* Evaluate the entire buffer */
    int result = osc_lang_eval_buffer_fn(editor);
    if (result == 0) {
        shared_osc_send_playing(ctx, 1);
    }

    return 0;
}

/* /psnd/play/line i:line - Play specific line */
static int osc_play_line_handler(const char *path, const char *types,
                                 lo_arg **argv, int argc,
                                 lo_message msg, void *user_data) {
    (void)path; (void)types; (void)msg;
    SharedContext *ctx = (SharedContext *)user_data;

    if (!ctx || argc < 1) return 0;

    /* TODO: Implement line-specific playback when we have access to buffer rows */
    int line = argv[0]->i;
    fprintf(stderr, "[OSC] Play line %d: not yet implemented\n", line);

    return 0;
}

/* /psnd/stop - Stop all playback */
static int osc_stop_handler(const char *path, const char *types,
                            lo_arg **argv, int argc,
                            lo_message msg, void *user_data) {
    (void)path; (void)types; (void)argv; (void)argc; (void)msg;
    SharedContext *ctx = (SharedContext *)user_data;

    if (!ctx) return 0;

    struct editor_ctx *editor = osc_get_editor_ctx(ctx);

    fprintf(stderr, "[OSC] Stop: stopping all playback\n");

    /* Stop all languages */
    if (editor && osc_lang_stop_all_fn) {
        osc_lang_stop_all_fn(editor);
    }

    /* Also send panic */
    shared_send_panic(ctx);

    /* Broadcast stopped state */
    shared_osc_send_playing(ctx, 0);

    return 0;
}

/* /psnd/eval s:code - Evaluate code string */
static int osc_eval_handler(const char *path, const char *types,
                            lo_arg **argv, int argc,
                            lo_message msg, void *user_data) {
    (void)path; (void)types; (void)msg;
    SharedContext *ctx = (SharedContext *)user_data;

    if (!ctx || argc < 1) return 0;

    struct editor_ctx *editor = osc_get_editor_ctx(ctx);
    if (!editor || !osc_lang_eval_fn) {
        fprintf(stderr, "[OSC] Eval: no editor context or callback available\n");
        return 0;
    }

    const char *code = &argv[0]->s;
    fprintf(stderr, "[OSC] Eval: %s\n", code);

    /* Evaluate the code */
    int result = osc_lang_eval_fn(editor, code);
    if (result == 0) {
        shared_osc_send_playing(ctx, 1);
    }

    return 0;
}

/* ======================= Query Handlers ==================================== */

/* /psnd/query/tempo - Reply with current tempo */
static int osc_query_tempo_handler(const char *path, const char *types,
                                   lo_arg **argv, int argc,
                                   lo_message msg, void *user_data) {
    (void)path; (void)types; (void)argv; (void)argc;
    SharedContext *ctx = (SharedContext *)user_data;

    lo_address src = lo_message_get_source(msg);
    if (src && ctx) {
        float tempo = (float)ctx->tempo;
        lo_send(src, "/psnd/reply/tempo", "f", tempo);
        fprintf(stderr, "[OSC] Query tempo: %.1f BPM\n", tempo);
    }
    return 0;
}

/* /psnd/query/playing - Reply with playback state */
static int osc_query_playing_handler(const char *path, const char *types,
                                     lo_arg **argv, int argc,
                                     lo_message msg, void *user_data) {
    (void)path; (void)types; (void)argv; (void)argc;
    SharedContext *ctx = (SharedContext *)user_data;

    lo_address src = lo_message_get_source(msg);
    if (src && ctx) {
        int playing = 0;
        if (osc_lang_is_playing_fn) {
            struct editor_ctx *editor = osc_get_editor_ctx(ctx);
            if (editor) {
                playing = osc_lang_is_playing_fn(editor);
            }
        }
        lo_send(src, "/psnd/reply/playing", "i", playing);
        fprintf(stderr, "[OSC] Query playing: %d\n", playing);
    }
    return 0;
}

/* /psnd/query/file - Reply with current filename */
static int osc_query_file_handler(const char *path, const char *types,
                                  lo_arg **argv, int argc,
                                  lo_message msg, void *user_data) {
    (void)path; (void)types; (void)argv; (void)argc;
    SharedContext *ctx = (SharedContext *)user_data;

    lo_address src = lo_message_get_source(msg);
    if (src && ctx) {
        const char *filename = "";
        if (osc_get_filename_fn) {
            struct editor_ctx *editor = osc_get_editor_ctx(ctx);
            if (editor) {
                const char *f = osc_get_filename_fn(editor);
                if (f) filename = f;
            }
        }
        lo_send(src, "/psnd/reply/file", "s", filename);
        fprintf(stderr, "[OSC] Query file: %s\n", filename);
    }
    return 0;
}

/* /psnd/query/position - Reply with cursor position (line, column) */
static int osc_query_position_handler(const char *path, const char *types,
                                      lo_arg **argv, int argc,
                                      lo_message msg, void *user_data) {
    (void)path; (void)types; (void)argv; (void)argc;
    SharedContext *ctx = (SharedContext *)user_data;

    lo_address src = lo_message_get_source(msg);
    if (src && ctx) {
        int line = 0, col = 0;
        if (osc_get_position_fn) {
            struct editor_ctx *editor = osc_get_editor_ctx(ctx);
            if (editor) {
                osc_get_position_fn(editor, &line, &col);
            }
        }
        /* Return 1-based line and column for user-facing consistency */
        lo_send(src, "/psnd/reply/position", "ii", line + 1, col + 1);
        fprintf(stderr, "[OSC] Query position: line %d, col %d\n", line + 1, col + 1);
    }
    return 0;
}

/* ======================= Parameter Handlers ================================ */

/* /psnd/param/set sf:name,value - Set parameter by name */
static int osc_param_set_handler(const char *path, const char *types,
                                  lo_arg **argv, int argc,
                                  lo_message msg, void *user_data) {
    (void)path; (void)types; (void)msg;
    SharedContext *ctx = (SharedContext *)user_data;

    if (!ctx || argc < 2) return 0;

    const char *name = &argv[0]->s;
    float value = argv[1]->f;

    if (shared_param_set(ctx, name, value) == 0) {
        fprintf(stderr, "[OSC] Param set: %s = %.4f\n", name, value);
    } else {
        fprintf(stderr, "[OSC] Param set: unknown parameter '%s'\n", name);
    }

    return 0;
}

/* /psnd/param/get s:name - Query parameter, reply with /psnd/param/value */
static int osc_param_get_handler(const char *path, const char *types,
                                  lo_arg **argv, int argc,
                                  lo_message msg, void *user_data) {
    (void)path; (void)types;
    SharedContext *ctx = (SharedContext *)user_data;

    if (!ctx || argc < 1) return 0;

    const char *name = &argv[0]->s;
    float value = 0.0f;

    lo_address src = lo_message_get_source(msg);
    if (src) {
        if (shared_param_get(ctx, name, &value) == 0) {
            lo_send(src, "/psnd/param/value", "sf", name, value);
            fprintf(stderr, "[OSC] Param get: %s = %.4f\n", name, value);
        } else {
            lo_send(src, "/psnd/param/error", "ss", name, "not found");
            fprintf(stderr, "[OSC] Param get: unknown parameter '%s'\n", name);
        }
    }

    return 0;
}

/* /psnd/param/list - Query all parameters, reply with /psnd/param/info for each */
static int osc_param_list_handler(const char *path, const char *types,
                                   lo_arg **argv, int argc,
                                   lo_message msg, void *user_data) {
    (void)path; (void)types; (void)argv; (void)argc;
    SharedContext *ctx = (SharedContext *)user_data;

    if (!ctx) return 0;

    lo_address src = lo_message_get_source(msg);
    if (!src) return 0;

    /* Send info for each defined parameter */
    for (int i = 0; i < PARAM_MAX_COUNT; i++) {
        const SharedParam *p = shared_param_at(ctx, i);
        if (p) {
            float val = shared_param_get_idx(ctx, i);
            lo_send(src, "/psnd/param/info", "sffff",
                    p->name, val, p->min_val, p->max_val, p->default_val);
        }
    }

    /* Send end marker */
    lo_send(src, "/psnd/param/list/end", "i", shared_param_count(ctx));

    return 0;
}

/* Wildcard handler for bound OSC paths.
 * Called for any message - checks if path matches a bound parameter.
 * Returns 1 to pass to other handlers, 0 to consume. */
static int osc_param_wildcard_handler(const char *path, const char *types,
                                       lo_arg **argv, int argc,
                                       lo_message msg, void *user_data) {
    (void)types; (void)msg;
    SharedContext *ctx = (SharedContext *)user_data;

    if (!ctx || !path || argc < 1) return 1;  /* Pass to other handlers */

    /* Look up parameter by OSC path */
    int idx = shared_param_find_by_osc_path(ctx, path);
    if (idx < 0) return 1;  /* Not bound, pass to other handlers */

    /* Get value from first argument (support float or int) */
    float value;
    if (types && types[0] == 'f') {
        value = argv[0]->f;
    } else if (types && types[0] == 'i') {
        value = (float)argv[0]->i;
    } else {
        return 1;  /* Unknown type, pass to other handlers */
    }

    shared_param_set_idx(ctx, idx, value);

    const SharedParam *p = shared_param_at(ctx, idx);
    if (p) {
        fprintf(stderr, "[OSC] Param bound: %s (%s) = %.4f\n", path, p->name, value);
    }

    return 0;  /* Consumed */
}

/* ======================= Public API ======================================= */

/* Internal: register all handlers on the server */
static void osc_register_handlers(SharedContext *ctx) {
    if (!ctx || !ctx->osc_server) return;

    /* Generic handler that catches all messages for debugging */
    lo_server_thread_add_method(ctx->osc_server, NULL, NULL,
                                osc_generic_handler, ctx);

    /* Ping/pong */
    lo_server_thread_add_method(ctx->osc_server, "/psnd/ping", "",
                                osc_ping_handler, ctx);

    /* Tempo */
    lo_server_thread_add_method(ctx->osc_server, "/psnd/tempo", "f",
                                osc_tempo_handler, ctx);

    /* Note with duration */
    lo_server_thread_add_method(ctx->osc_server, "/psnd/note", "iii",
                                osc_note_handler, ctx);
    lo_server_thread_add_method(ctx->osc_server, "/psnd/note", "iiii",
                                osc_note_handler, ctx);

    /* Note on/off */
    lo_server_thread_add_method(ctx->osc_server, "/psnd/noteon", "ii",
                                osc_noteon_handler, ctx);
    lo_server_thread_add_method(ctx->osc_server, "/psnd/noteon", "iii",
                                osc_noteon_handler, ctx);
    lo_server_thread_add_method(ctx->osc_server, "/psnd/noteoff", "i",
                                osc_noteoff_handler, ctx);
    lo_server_thread_add_method(ctx->osc_server, "/psnd/noteoff", "ii",
                                osc_noteoff_handler, ctx);

    /* Control change */
    lo_server_thread_add_method(ctx->osc_server, "/psnd/cc", "ii",
                                osc_cc_handler, ctx);
    lo_server_thread_add_method(ctx->osc_server, "/psnd/cc", "iii",
                                osc_cc_handler, ctx);

    /* Program change */
    lo_server_thread_add_method(ctx->osc_server, "/psnd/pc", "i",
                                osc_pc_handler, ctx);
    lo_server_thread_add_method(ctx->osc_server, "/psnd/pc", "ii",
                                osc_pc_handler, ctx);

    /* Pitch bend */
    lo_server_thread_add_method(ctx->osc_server, "/psnd/bend", "i",
                                osc_bend_handler, ctx);
    lo_server_thread_add_method(ctx->osc_server, "/psnd/bend", "ii",
                                osc_bend_handler, ctx);

    /* Panic */
    lo_server_thread_add_method(ctx->osc_server, "/psnd/panic", "",
                                osc_panic_handler, ctx);

    /* Playback control */
    lo_server_thread_add_method(ctx->osc_server, "/psnd/play", "",
                                osc_play_handler, ctx);
    lo_server_thread_add_method(ctx->osc_server, "/psnd/play/line", "i",
                                osc_play_line_handler, ctx);
    lo_server_thread_add_method(ctx->osc_server, "/psnd/stop", "",
                                osc_stop_handler, ctx);

    /* Eval */
    lo_server_thread_add_method(ctx->osc_server, "/psnd/eval", "s",
                                osc_eval_handler, ctx);

    /* Query handlers */
    lo_server_thread_add_method(ctx->osc_server, "/psnd/query/tempo", "",
                                osc_query_tempo_handler, ctx);
    lo_server_thread_add_method(ctx->osc_server, "/psnd/query/playing", "",
                                osc_query_playing_handler, ctx);
    lo_server_thread_add_method(ctx->osc_server, "/psnd/query/file", "",
                                osc_query_file_handler, ctx);
    lo_server_thread_add_method(ctx->osc_server, "/psnd/query/position", "",
                                osc_query_position_handler, ctx);

    /* Parameter handlers */
    lo_server_thread_add_method(ctx->osc_server, "/psnd/param/set", "sf",
                                osc_param_set_handler, ctx);
    lo_server_thread_add_method(ctx->osc_server, "/psnd/param/get", "s",
                                osc_param_get_handler, ctx);
    lo_server_thread_add_method(ctx->osc_server, "/psnd/param/list", "",
                                osc_param_list_handler, ctx);

    /* Wildcard handler for bound OSC paths (must be registered last) */
    lo_server_thread_add_method(ctx->osc_server, NULL, NULL,
                                osc_param_wildcard_handler, ctx);
}

int shared_osc_init(SharedContext *ctx, int port) {
    return shared_osc_init_with_iface(ctx, port, NULL);
}

int shared_osc_init_with_iface(SharedContext *ctx, int port, const char *iface) {
    if (!ctx) return -1;

    if (port <= 0) {
        port = PSND_OSC_DEFAULT_PORT;
    }

    /* Convert port to string */
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    /* Create threaded server
     * Note: liblo doesn't support direct interface binding in its standard API.
     * For network security, rely on firewall rules or run psnd in a container.
     * The iface parameter is logged but currently has no effect.
     */
    if (iface && *iface) {
        fprintf(stderr, "[OSC] Interface binding requested (%s) - not supported by liblo, "
                "binding to all interfaces\n", iface);
    }

    ctx->osc_server = lo_server_thread_new(port_str, osc_error_handler);
    if (!ctx->osc_server) {
        fprintf(stderr, "[OSC] Failed to create server on port %d\n", port);
        return -1;
    }

    ctx->osc_port = lo_server_thread_get_port(ctx->osc_server);
    ctx->osc_enabled = 1;

    /* Register built-in handlers */
    osc_register_handlers(ctx);

    fprintf(stderr, "[OSC] Initialized on port %d\n", ctx->osc_port);
    return 0;
}

int shared_osc_init_multicast(SharedContext *ctx, const char *group, int port) {
    if (!ctx) return -1;

    if (port <= 0) {
        port = PSND_OSC_DEFAULT_PORT;
    }

    /* Convert port to string */
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    /* Create multicast server thread
     * liblo's multicast support joins the specified multicast group
     * Common multicast groups: 224.0.0.1 (all hosts), 239.x.x.x (organization-local)
     */
    ctx->osc_server = lo_server_thread_new_multicast(group, port_str, osc_error_handler);
    if (!ctx->osc_server) {
        fprintf(stderr, "[OSC] Failed to create multicast server on %s:%d\n", group, port);
        return -1;
    }

    ctx->osc_port = lo_server_thread_get_port(ctx->osc_server);
    ctx->osc_enabled = 1;

    /* Register built-in handlers */
    osc_register_handlers(ctx);

    fprintf(stderr, "[OSC] Initialized multicast on %s:%d\n", group, ctx->osc_port);
    return 0;
}

int shared_osc_set_broadcast(SharedContext *ctx, const char *host, const char *port) {
    if (!ctx || !host || !port) return -1;

    /* Free existing broadcast address */
    if (ctx->osc_broadcast) {
        lo_address_free(ctx->osc_broadcast);
        ctx->osc_broadcast = NULL;
    }

    ctx->osc_broadcast = lo_address_new(host, port);
    if (!ctx->osc_broadcast) {
        fprintf(stderr, "[OSC] Failed to create broadcast address %s:%s\n", host, port);
        return -1;
    }

    fprintf(stderr, "[OSC] Broadcasting to %s:%s\n", host, port);
    return 0;
}

int shared_osc_start(SharedContext *ctx) {
    if (!ctx || !ctx->osc_server) return -1;

    int result = lo_server_thread_start(ctx->osc_server);
    if (result < 0) {
        fprintf(stderr, "[OSC] Failed to start server thread\n");
        return -1;
    }

    fprintf(stderr, "[OSC] Server started on port %d\n", ctx->osc_port);
    return 0;
}

void shared_osc_cleanup(SharedContext *ctx) {
    if (!ctx) return;

    if (ctx->osc_server) {
        lo_server_thread_stop(ctx->osc_server);
        lo_server_thread_free(ctx->osc_server);
        ctx->osc_server = NULL;
        fprintf(stderr, "[OSC] Server stopped\n");
    }

    if (ctx->osc_broadcast) {
        lo_address_free(ctx->osc_broadcast);
        ctx->osc_broadcast = NULL;
    }

    ctx->osc_enabled = 0;
    ctx->osc_port = 0;
}

int shared_osc_is_running(SharedContext *ctx) {
    return ctx && ctx->osc_enabled && ctx->osc_server;
}

int shared_osc_get_port(SharedContext *ctx) {
    return ctx ? ctx->osc_port : 0;
}

int shared_osc_send(SharedContext *ctx, const char *path, const char *types, ...) {
    if (!ctx || !ctx->osc_broadcast || !path) return -1;

    va_list args;
    va_start(args, types);

    lo_message msg = lo_message_new();
    if (!msg) {
        va_end(args);
        return -1;
    }

    /* Add arguments based on types */
    if (types) {
        for (const char *t = types; *t; t++) {
            switch (*t) {
                case 'i':
                    lo_message_add_int32(msg, va_arg(args, int));
                    break;
                case 'f':
                    lo_message_add_float(msg, (float)va_arg(args, double));
                    break;
                case 'd':
                    lo_message_add_double(msg, va_arg(args, double));
                    break;
                case 's':
                    lo_message_add_string(msg, va_arg(args, const char *));
                    break;
                case 'T':
                    lo_message_add_true(msg);
                    break;
                case 'F':
                    lo_message_add_false(msg);
                    break;
                case 'N':
                    lo_message_add_nil(msg);
                    break;
                default:
                    /* Unknown type, skip */
                    break;
            }
        }
    }

    va_end(args);

    int result = lo_send_message(ctx->osc_broadcast, path, msg);
    lo_message_free(msg);

    return result;
}

int shared_osc_send_to(const char *host, const char *port,
                       const char *path, const char *types, ...) {
    if (!host || !port || !path) return -1;

    lo_address addr = lo_address_new(host, port);
    if (!addr) return -1;

    va_list args;
    va_start(args, types);

    lo_message msg = lo_message_new();
    if (!msg) {
        va_end(args);
        lo_address_free(addr);
        return -1;
    }

    /* Add arguments based on types */
    if (types) {
        for (const char *t = types; *t; t++) {
            switch (*t) {
                case 'i':
                    lo_message_add_int32(msg, va_arg(args, int));
                    break;
                case 'f':
                    lo_message_add_float(msg, (float)va_arg(args, double));
                    break;
                case 'd':
                    lo_message_add_double(msg, va_arg(args, double));
                    break;
                case 's':
                    lo_message_add_string(msg, va_arg(args, const char *));
                    break;
                default:
                    break;
            }
        }
    }

    va_end(args);

    int result = lo_send_message(addr, path, msg);
    lo_message_free(msg);
    lo_address_free(addr);

    return result;
}

void shared_osc_send_playing(SharedContext *ctx, int playing) {
    shared_osc_send(ctx, "/psnd/status/playing", "i", playing ? 1 : 0);
}

void shared_osc_send_tempo(SharedContext *ctx, float tempo) {
    shared_osc_send(ctx, "/psnd/status/tempo", "f", tempo);
}

void shared_osc_send_note(SharedContext *ctx, int channel, int pitch, int velocity) {
    /* Apply rate limiting to note messages */
    if (osc_should_rate_limit_note()) {
        return;  /* Drop message due to rate limit */
    }
    shared_osc_send(ctx, "/psnd/midi/note", "iii", channel, pitch, velocity);
}

int shared_osc_add_handler(SharedContext *ctx, const char *path,
                           const char *types, psnd_osc_handler_t handler,
                           void *user_data) {
    if (!ctx || !ctx->osc_server || !handler) return -1;

    lo_server_thread_add_method(ctx->osc_server, path, types,
                                (lo_method_handler)handler, user_data);
    return 0;
}

void shared_osc_set_user_data(SharedContext *ctx, void *user_data) {
    if (ctx) {
        ctx->osc_user_data = user_data;
    }
}

void shared_osc_set_lang_callbacks(
    int (*eval_fn)(struct editor_ctx *ctx, const char *code),
    int (*eval_buffer_fn)(struct editor_ctx *ctx),
    void (*stop_all_fn)(struct editor_ctx *ctx)
) {
    osc_lang_eval_fn = eval_fn;
    osc_lang_eval_buffer_fn = eval_buffer_fn;
    osc_lang_stop_all_fn = stop_all_fn;
}

void shared_osc_set_query_callbacks(
    int (*is_playing_fn)(struct editor_ctx *ctx),
    const char *(*get_filename_fn)(struct editor_ctx *ctx),
    void (*get_position_fn)(struct editor_ctx *ctx, int *line, int *col)
) {
    osc_lang_is_playing_fn = is_playing_fn;
    osc_get_filename_fn = get_filename_fn;
    osc_get_position_fn = get_position_fn;
}

void shared_osc_set_note_rate_limit(int messages_per_second) {
    osc_note_rate_limit = messages_per_second > 0 ? messages_per_second : 0;
    fprintf(stderr, "[OSC] Note rate limit set to %d msgs/sec (0 = unlimited)\n",
            osc_note_rate_limit);
}

int shared_osc_get_note_rate_limit(void) {
    return osc_note_rate_limit;
}

#endif /* PSND_OSC */
