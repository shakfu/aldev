/**
 * @file repl.c
 * @brief TR7 Scheme REPL - Interactive R7RS-small Scheme interpreter with music extensions.
 */

#include "repl.h"
#include "version.h"
#include "loki/core.h"
#include "loki/internal.h"
#include "loki/syntax.h"
#include "loki/lua.h"
#include "shared/repl_commands.h"
#include "shared/context.h"

/* TR7 library headers */
#include "tr7.h"
#include "context.h"
#include "midi/midi.h"
#include "audio/audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ============================================================================
 * TR7 Usage and Help
 * ============================================================================ */

static void print_tr7_repl_usage(const char *prog) {
    printf("Usage: %s tr7 [options] [file.scm]\n", prog);
    printf("       %s scheme [options] [file.scm]\n", prog);
    printf("\n");
    printf("TR7 R7RS-small Scheme interpreter with music extensions.\n");
    printf("If no file is provided, starts an interactive REPL.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help        Show this help message\n");
    printf("  -v, --verbose     Enable verbose output\n");
    printf("  -l, --list        List available MIDI ports\n");
    printf("  -p, --port N      Use MIDI port N (0-based index)\n");
    printf("  --virtual NAME    Create virtual MIDI port with NAME\n");
    printf("\n");
    printf("Built-in Synth Options:\n");
    printf("  -sf, --soundfont PATH  Use built-in synth with soundfont (.sf2)\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s tr7                   Start interactive Scheme REPL\n", prog);
    printf("  %s tr7 song.scm          Execute a Scheme file\n", prog);
    printf("  %s tr7 -sf gm.sf2        REPL with built-in synth\n", prog);
    printf("  %s tr7 --virtual TR7Out  REPL with virtual MIDI port\n", prog);
    printf("\n");
}

static void print_tr7_repl_help(void) {
    shared_print_command_help();

    printf("TR7-specific Commands:\n");
    printf("  ,load FILE        Load and execute a Scheme file\n");
    printf("\n");
    printf("Music Primitives:\n");
    printf("  (play-note pitch [vel] [dur])  Play a MIDI note\n");
    printf("  (play-chord '(p1 p2 ...) [vel] [dur])  Play chord\n");
    printf("  (note-on pitch [vel])    Send note-on\n");
    printf("  (note-off pitch)         Send note-off\n");
    printf("  (set-tempo bpm)          Set tempo\n");
    printf("  (set-octave n)           Set default octave (0-9)\n");
    printf("  (set-velocity v)         Set default velocity (0-127)\n");
    printf("  (set-channel ch)         Set MIDI channel (0-15)\n");
    printf("  (note \"c#\" [oct])        Convert note name to pitch\n");
    printf("\n");
    printf("MIDI Control:\n");
    printf("  (midi-list)              List MIDI ports\n");
    printf("  (midi-open port)         Open MIDI port by index\n");
    printf("  (midi-virtual name)      Create virtual MIDI port\n");
    printf("  (midi-panic)             All notes off\n");
    printf("  (program-change prog)    Change instrument\n");
    printf("  (control-change cc val)  Send CC message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  (play-note 60 80 500)    ; Middle C, vel 80, 500ms\n");
    printf("  (play-chord '(60 64 67)) ; C major chord\n");
    printf("  (set-tempo 140)          ; 140 BPM\n");
    printf("\n");
}

/* ============================================================================
 * TR7 Engine State
 * ============================================================================ */

/* TR7 engine state for REPL */
static tr7_engine_t g_tr7_repl_engine = NULL;
static SharedContext *g_tr7_repl_shared = NULL;

/* Music context for TR7 REPL */
typedef struct {
    int octave;
    int velocity;
    int tempo;
    int channel;
    int duration_ms;
} Tr7ReplMusicCtx;

static Tr7ReplMusicCtx g_tr7_music = {4, 80, 120, 0, 500};

/* Sleep helper */
static void tr7_sleep_ms(int ms) {
    if (ms > 0) {
        usleep(ms * 1000);
    }
}

/* ============================================================================
 * Scheme Music Primitives
 * ============================================================================ */

static tr7_C_return_t repl_scm_play_note(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    if (!g_tr7_repl_shared) {
        return tr7_C_raise_error(tsc, "Music backend not initialized", TR7_NIL, 0);
    }

    int pitch = TR7_TO_INT(values[0]);
    int velocity = (nvalues >= 2) ? TR7_TO_INT(values[1]) : g_tr7_music.velocity;
    int duration = (nvalues >= 3) ? TR7_TO_INT(values[2]) : g_tr7_music.duration_ms;

    if (pitch < 0) pitch = 0;
    if (pitch > 127) pitch = 127;
    if (velocity < 0) velocity = 0;
    if (velocity > 127) velocity = 127;

    shared_send_note_on(g_tr7_repl_shared, g_tr7_music.channel, pitch, velocity);
    tr7_sleep_ms(duration);
    shared_send_note_off(g_tr7_repl_shared, g_tr7_music.channel, pitch);

    return tr7_C_return_single(tsc, TR7_VOID);
}

static tr7_C_return_t repl_scm_note_on(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    if (!g_tr7_repl_shared) {
        return tr7_C_raise_error(tsc, "Music backend not initialized", TR7_NIL, 0);
    }

    int pitch = TR7_TO_INT(values[0]);
    int velocity = (nvalues >= 2) ? TR7_TO_INT(values[1]) : g_tr7_music.velocity;

    if (pitch >= 0 && pitch <= 127 && velocity >= 0 && velocity <= 127) {
        shared_send_note_on(g_tr7_repl_shared, g_tr7_music.channel, pitch, velocity);
    }

    return tr7_C_return_single(tsc, TR7_VOID);
}

static tr7_C_return_t repl_scm_note_off(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    if (!g_tr7_repl_shared) {
        return tr7_C_raise_error(tsc, "Music backend not initialized", TR7_NIL, 0);
    }

    int pitch = TR7_TO_INT(values[0]);
    if (pitch >= 0 && pitch <= 127) {
        shared_send_note_off(g_tr7_repl_shared, g_tr7_music.channel, pitch);
    }

    return tr7_C_return_single(tsc, TR7_VOID);
}

static tr7_C_return_t repl_scm_play_chord(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    if (!g_tr7_repl_shared) {
        return tr7_C_raise_error(tsc, "Music backend not initialized", TR7_NIL, 0);
    }

    if (!TR7_IS_PAIR(values[0]) && !TR7_IS_NIL(values[0])) {
        return tr7_C_raise_error(tsc, "play-chord: expected list of pitches", values[0], 0);
    }

    int velocity = (nvalues >= 2) ? TR7_TO_INT(values[1]) : g_tr7_music.velocity;
    int duration = (nvalues >= 3) ? TR7_TO_INT(values[2]) : g_tr7_music.duration_ms;

    /* Collect pitches */
    int pitches[16];
    int count = 0;
    tr7_t list = values[0];

    while (TR7_IS_PAIR(list) && count < 16) {
        tr7_t note = TR7_CAR(list);
        if (TR7_IS_INT(note)) {
            int p = TR7_TO_INT(note);
            if (p >= 0 && p <= 127) {
                pitches[count++] = p;
            }
        }
        list = TR7_CDR(list);
    }

    /* Play all notes */
    for (int i = 0; i < count; i++) {
        shared_send_note_on(g_tr7_repl_shared, g_tr7_music.channel, pitches[i], velocity);
    }

    tr7_sleep_ms(duration);

    /* Stop all notes */
    for (int i = 0; i < count; i++) {
        shared_send_note_off(g_tr7_repl_shared, g_tr7_music.channel, pitches[i]);
    }

    return tr7_C_return_single(tsc, TR7_VOID);
}

static tr7_C_return_t repl_scm_set_tempo(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    int tempo = TR7_TO_INT(values[0]);
    if (tempo >= 20 && tempo <= 400) {
        g_tr7_music.tempo = tempo;
        g_tr7_music.duration_ms = 60000 / tempo;  /* Quarter note duration */
    }
    return tr7_C_return_single(tsc, TR7_VOID);
}

static tr7_C_return_t repl_scm_set_octave(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    int octave = TR7_TO_INT(values[0]);
    if (octave >= 0 && octave <= 9) {
        g_tr7_music.octave = octave;
    }
    return tr7_C_return_single(tsc, TR7_VOID);
}

static tr7_C_return_t repl_scm_set_velocity(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    int velocity = TR7_TO_INT(values[0]);
    if (velocity >= 0 && velocity <= 127) {
        g_tr7_music.velocity = velocity;
    }
    return tr7_C_return_single(tsc, TR7_VOID);
}

static tr7_C_return_t repl_scm_set_channel(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    int channel = TR7_TO_INT(values[0]);
    if (channel >= 0 && channel <= 15) {
        g_tr7_music.channel = channel;
    }
    return tr7_C_return_single(tsc, TR7_VOID);
}

static tr7_C_return_t repl_scm_tempo(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    (void)values;
    return tr7_C_return_single(tsc, TR7_FROM_INT(g_tr7_music.tempo));
}

static tr7_C_return_t repl_scm_octave(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    (void)values;
    return tr7_C_return_single(tsc, TR7_FROM_INT(g_tr7_music.octave));
}

static tr7_C_return_t repl_scm_velocity(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    (void)values;
    return tr7_C_return_single(tsc, TR7_FROM_INT(g_tr7_music.velocity));
}

static tr7_C_return_t repl_scm_channel(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    (void)values;
    return tr7_C_return_single(tsc, TR7_FROM_INT(g_tr7_music.channel));
}

static tr7_C_return_t repl_scm_midi_list(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    (void)values;
    shared_midi_list_ports(g_tr7_repl_shared);
    return tr7_C_return_single(tsc, TR7_VOID);
}

static tr7_C_return_t repl_scm_midi_open(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    if (!g_tr7_repl_shared) {
        return tr7_C_raise_error(tsc, "Music backend not initialized", TR7_NIL, 0);
    }

    int port = TR7_TO_INT(values[0]);
    int result = shared_midi_open_port(g_tr7_repl_shared, port);

    if (result == 0) {
        printf("TR7: Opened MIDI port %d\n", port);
    }

    return tr7_C_return_single(tsc, result == 0 ? TR7_TRUE : TR7_FALSE);
}

static tr7_C_return_t repl_scm_midi_virtual(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    if (!g_tr7_repl_shared) {
        return tr7_C_raise_error(tsc, "Music backend not initialized", TR7_NIL, 0);
    }

    if (!TR7_IS_STRING(values[0])) {
        return tr7_C_raise_error(tsc, "midi-virtual: expected string name", values[0], 0);
    }

    const char *name = tr7_string_buffer(values[0]);
    int result = shared_midi_open_virtual(g_tr7_repl_shared, name);

    if (result == 0) {
        printf("TR7: Created virtual MIDI port: %s\n", name);
    }

    return tr7_C_return_single(tsc, result == 0 ? TR7_TRUE : TR7_FALSE);
}

static tr7_C_return_t repl_scm_midi_panic(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    (void)values;
    if (g_tr7_repl_shared) {
        shared_send_panic(g_tr7_repl_shared);
    }
    return tr7_C_return_single(tsc, TR7_VOID);
}

static tr7_C_return_t repl_scm_tsf_load(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;

    if (!TR7_IS_STRING(values[0])) {
        return tr7_C_raise_error(tsc, "tsf-load: expected string path", values[0], 0);
    }

    const char *path = tr7_string_buffer(values[0]);
    int result = shared_tsf_load_soundfont(path);

    if (result == 0) {
        printf("TR7: Loaded SoundFont: %s\n", path);
        if (g_tr7_repl_shared) {
            g_tr7_repl_shared->tsf_enabled = 1;
        }
    }

    return tr7_C_return_single(tsc, result == 0 ? TR7_TRUE : TR7_FALSE);
}

static tr7_C_return_t repl_scm_sleep_ms(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    int ms = TR7_TO_INT(values[0]);
    tr7_sleep_ms(ms);
    return tr7_C_return_single(tsc, TR7_VOID);
}

static tr7_C_return_t repl_scm_program_change(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    if (!g_tr7_repl_shared) {
        return tr7_C_raise_error(tsc, "Music backend not initialized", TR7_NIL, 0);
    }

    int program = TR7_TO_INT(values[0]);
    if (program < 0) program = 0;
    if (program > 127) program = 127;

    shared_send_program(g_tr7_repl_shared, g_tr7_music.channel, program);

    return tr7_C_return_single(tsc, TR7_VOID);
}

static tr7_C_return_t repl_scm_control_change(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    (void)nvalues;
    if (!g_tr7_repl_shared) {
        return tr7_C_raise_error(tsc, "Music backend not initialized", TR7_NIL, 0);
    }

    int cc = TR7_TO_INT(values[0]);
    int value = TR7_TO_INT(values[1]);

    if (cc < 0) cc = 0;
    if (cc > 127) cc = 127;
    if (value < 0) value = 0;
    if (value > 127) value = 127;

    shared_send_cc(g_tr7_repl_shared, g_tr7_music.channel, cc, value);

    return tr7_C_return_single(tsc, TR7_VOID);
}

/* Note name to MIDI pitch helper */
static int tr7_note_to_pitch(const char *name, int octave) {
    static const int base[] = {9, 11, 0, 2, 4, 5, 7};  /* a, b, c, d, e, f, g */

    if (!name || !*name) return -1;

    char note = name[0];
    if (note >= 'A' && note <= 'G') note = note - 'A' + 'a';
    if (note < 'a' || note > 'g') return -1;

    int pitch = base[note - 'a'];
    const char *p = name + 1;

    /* Handle accidentals */
    while (*p) {
        if (*p == '#' || *p == '+') pitch++;
        else if (*p == 'b' || *p == '-') pitch--;
        p++;
    }

    /* Calculate final pitch */
    pitch = (octave + 1) * 12 + pitch;

    return pitch;
}

static tr7_C_return_t repl_scm_note(tr7_engine_t tsc, int nvalues, const tr7_t *values, void *closure) {
    (void)closure;
    int octave = (nvalues >= 2) ? TR7_TO_INT(values[1]) : g_tr7_music.octave;

    if (TR7_IS_STRING(values[0])) {
        const char *name = tr7_string_buffer(values[0]);
        int pitch = tr7_note_to_pitch(name, octave);
        if (pitch >= 0 && pitch <= 127) {
            return tr7_C_return_single(tsc, TR7_FROM_INT(pitch));
        }
    } else if (TR7_IS_SYMBOL(values[0])) {
        const char *name = tr7_symbol_string(values[0]);
        int pitch = tr7_note_to_pitch(name, octave);
        if (pitch >= 0 && pitch <= 127) {
            return tr7_C_return_single(tsc, TR7_FROM_INT(pitch));
        }
    }

    return tr7_C_raise_error(tsc, "note: invalid note name", values[0], 0);
}

/* Register all music primitives with TR7 engine */
static const tr7_C_func_def_t tr7_repl_music_funcs[] = {
    /* Note playing */
    {"play-note", repl_scm_play_note, NULL, TR7ARG_INTEGER, 1, 3},
    {"note-on", repl_scm_note_on, NULL, TR7ARG_INTEGER, 1, 2},
    {"note-off", repl_scm_note_off, NULL, TR7ARG_INTEGER, 1, 1},
    {"play-chord", repl_scm_play_chord, NULL, TR7ARG_PROPER_LIST, 1, 3},

    /* State setters */
    {"set-tempo", repl_scm_set_tempo, NULL, TR7ARG_INTEGER, 1, 1},
    {"set-octave", repl_scm_set_octave, NULL, TR7ARG_INTEGER, 1, 1},
    {"set-velocity", repl_scm_set_velocity, NULL, TR7ARG_INTEGER, 1, 1},
    {"set-channel", repl_scm_set_channel, NULL, TR7ARG_INTEGER, 1, 1},

    /* State getters */
    {"tempo", repl_scm_tempo, NULL, NULL, 0, 0},
    {"octave", repl_scm_octave, NULL, NULL, 0, 0},
    {"velocity", repl_scm_velocity, NULL, NULL, 0, 0},
    {"channel", repl_scm_channel, NULL, NULL, 0, 0},

    /* MIDI control */
    {"midi-list", repl_scm_midi_list, NULL, NULL, 0, 0},
    {"midi-open", repl_scm_midi_open, NULL, TR7ARG_INTEGER, 1, 1},
    {"midi-virtual", repl_scm_midi_virtual, NULL, TR7ARG_STRING, 1, 1},
    {"midi-panic", repl_scm_midi_panic, NULL, NULL, 0, 0},

    /* Utilities */
    {"tsf-load", repl_scm_tsf_load, NULL, TR7ARG_STRING, 1, 1},
    {"sleep-ms", repl_scm_sleep_ms, NULL, TR7ARG_INTEGER, 1, 1},
    {"program-change", repl_scm_program_change, NULL, TR7ARG_INTEGER, 1, 1},
    {"control-change", repl_scm_control_change, NULL, TR7ARG_INTEGER, 2, 2},
    {"note", repl_scm_note, NULL, NULL, 1, 2},

    {NULL, NULL, NULL, NULL, 0, 0}  /* Sentinel */
};

static void tr7_repl_register_music_funcs(tr7_engine_t engine) {
    /* Register in tr7/foreigns library and import it */
    tr7_lib_register_C_func_list(engine, TR7_FOREIGNS_LIBNAME, tr7_repl_music_funcs);
    tr7_import_lib(engine, TR7_FOREIGNS_LIBNAME);
}

/* ============================================================================
 * TR7 REPL Loop
 * ============================================================================ */

/* Stop callback for TR7 REPL */
static void tr7_stop_playback(void) {
    if (g_tr7_repl_shared) {
        shared_send_panic(g_tr7_repl_shared);
    }
}

/* Process a TR7 REPL command. Returns: 0=continue, 1=quit, 2=evaluate as Scheme */
static int tr7_process_command(const char *input) {
    /* Try shared commands first */
    int result = shared_process_command(g_tr7_repl_shared, input, tr7_stop_playback);
    if (result == REPL_CMD_QUIT) {
        return 1; /* quit */
    }
    if (result == REPL_CMD_HANDLED) {
        return 0;
    }

    /* Handle TR7-specific commands */
    const char *cmd = input;
    if (cmd[0] == ':')
        cmd++;

    /* Help */
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0 || strcmp(cmd, "?") == 0) {
        print_tr7_repl_help();
        return 0;
    }

    /* ,load command */
    if (strncmp(input, ",load ", 6) == 0) {
        const char *filename = input + 6;
        while (*filename == ' ') filename++;

        if (*filename) {
            FILE *f = fopen(filename, "r");
            if (!f) {
                printf("Error: Cannot open file: %s\n", filename);
            } else {
                int status = tr7_run_file(g_tr7_repl_engine, f, filename);
                fclose(f);
                if (status != 0) {
                    tr7_t err = tr7_get_last_value(g_tr7_repl_engine);
                    if (tr7_is_error(err)) {
                        tr7_t msg = tr7_error_message(err);
                        if (TR7_IS_STRING(msg)) {
                            printf("Error loading %s: %s\n", filename, tr7_string_buffer(msg));
                        } else {
                            printf("Error loading %s\n", filename);
                        }
                    }
                } else {
                    printf("Loaded: %s\n", filename);
                }
            }
        }
        return 0;
    }

    return 2; /* evaluate as Scheme */
}

/* Non-interactive TR7 REPL loop for piped input */
static void tr7_repl_loop_pipe(void) {
    char line[MAX_INPUT_LENGTH];

    while (fgets(line, sizeof(line), stdin) != NULL) {
        /* Strip trailing newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }

        if (len == 0) continue;

        int result = tr7_process_command(line);
        if (result == 1) break;      /* quit */
        if (result == 0) continue;   /* command handled */

        /* Evaluate as Scheme - use tr7_play_string which handles result printing */
        tr7_play_string(g_tr7_repl_engine, line,
            Tr7_Play_Show_Result | Tr7_Play_Show_Errors | Tr7_Play_Keep_Playing);
        fflush(stdout);
    }
}

static void tr7_repl_loop(editor_ctx_t *syntax_ctx) {
    ReplLineEditor ed;
    char *input;

    /* Use non-interactive mode for piped input */
    if (!isatty(STDIN_FILENO)) {
        tr7_repl_loop_pipe();
        return;
    }

    repl_editor_init(&ed);

    printf("TR7 Scheme REPL %s (type :h for help, :q to quit)\n", PSND_VERSION);

    /* Enable raw mode for syntax-highlighted input */
    repl_enable_raw_mode();

    while (1) {
        input = repl_readline(syntax_ctx, &ed, "tr7> ");

        if (input == NULL) {
            /* EOF - exit cleanly */
            break;
        }

        if (input[0] == '\0') {
            continue;
        }

        repl_add_history(&ed, input);

        /* Process command */
        int result = tr7_process_command(input);
        if (result == 1) break;      /* quit */
        if (result == 0) continue;   /* command handled */

        /* Evaluate Scheme code - use tr7_play_string which handles result printing */
        tr7_play_string(g_tr7_repl_engine, input,
            Tr7_Play_Show_Result | Tr7_Play_Show_Errors | Tr7_Play_Keep_Playing);
        fflush(stdout);
    }

    /* Disable raw mode before exit */
    repl_disable_raw_mode();
    repl_editor_cleanup(&ed);
}

/* ============================================================================
 * TR7 REPL Main Entry Point
 * ============================================================================ */

int tr7_repl_main(int argc, char **argv) {
    int verbose = 0;
    int list_ports = 0;
    int port_index = -1;
    const char *virtual_name = NULL;
    const char *input_file = NULL;
    const char *soundfont_path = NULL;

    /* Simple argument parsing */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_tr7_repl_usage("psnd");
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list") == 0) {
            list_ports = 1;
        } else if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) &&
                   i + 1 < argc) {
            port_index = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--virtual") == 0 && i + 1 < argc) {
            virtual_name = argv[++i];
        } else if ((strcmp(argv[i], "-sf") == 0 || strcmp(argv[i], "--soundfont") == 0) &&
                   i + 1 < argc) {
            soundfont_path = argv[++i];
        } else if (argv[i][0] != '-' && input_file == NULL) {
            input_file = argv[i];
        }
    }

    /* Initialize TR7 engine */
    g_tr7_repl_engine = tr7_engine_create(0);
    if (!g_tr7_repl_engine) {
        fprintf(stderr, "Error: Failed to create TR7 engine\n");
        return 1;
    }

    /* Set TR7 search paths to .psnd/lib/scm in current working directory */
    /* Note: tr7_set_string stores pointer, doesn't copy - use static storage */
    static char tr7_lib_path[1100];
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd))) {
        snprintf(tr7_lib_path, sizeof(tr7_lib_path), "%s/.psnd/lib/scm", cwd);
        tr7_set_string(g_tr7_repl_engine, Tr7_StrID_Path, tr7_lib_path);
        tr7_set_string(g_tr7_repl_engine, Tr7_StrID_Library_Path, tr7_lib_path);
        tr7_set_string(g_tr7_repl_engine, Tr7_StrID_Include_Path, tr7_lib_path);
    }

    /* Import standard Scheme libraries */
    tr7_import_lib(g_tr7_repl_engine, "scheme/base");
    tr7_import_lib(g_tr7_repl_engine, "scheme/read");
    tr7_import_lib(g_tr7_repl_engine, "scheme/write");
    tr7_import_lib(g_tr7_repl_engine, "scheme/file");
    tr7_import_lib(g_tr7_repl_engine, "scheme/load");
    tr7_import_lib(g_tr7_repl_engine, "scheme/eval");

    /* Initialize shared context for MIDI/audio */
    g_tr7_repl_shared = calloc(1, sizeof(SharedContext));
    if (!g_tr7_repl_shared) {
        fprintf(stderr, "Error: Failed to create shared context\n");
        tr7_engine_destroy(g_tr7_repl_engine);
        return 1;
    }
    shared_context_init(g_tr7_repl_shared);

    /* Register music primitives */
    tr7_repl_register_music_funcs(g_tr7_repl_engine);

    /* Handle --list */
    if (list_ports) {
        shared_midi_list_ports(g_tr7_repl_shared);
        shared_context_cleanup(g_tr7_repl_shared);
        free(g_tr7_repl_shared);
        tr7_engine_destroy(g_tr7_repl_engine);
        return 0;
    }

    /* Setup output */
    if (soundfont_path) {
        /* Use built-in synth */
        if (shared_tsf_load_soundfont(soundfont_path) != 0) {
            fprintf(stderr, "Error: Failed to load soundfont: %s\n", soundfont_path);
            shared_context_cleanup(g_tr7_repl_shared);
            free(g_tr7_repl_shared);
            tr7_engine_destroy(g_tr7_repl_engine);
            return 1;
        }
        g_tr7_repl_shared->tsf_enabled = 1;
        if (verbose) {
            printf("Using built-in synth: %s\n", soundfont_path);
        }
    } else {
        /* Setup MIDI output */
        int midi_opened = 0;

        if (virtual_name) {
            if (shared_midi_open_virtual(g_tr7_repl_shared, virtual_name) == 0) {
                midi_opened = 1;
                if (verbose) {
                    printf("Created virtual MIDI port: %s\n", virtual_name);
                }
            }
        } else if (port_index >= 0) {
            if (shared_midi_open_port(g_tr7_repl_shared, port_index) == 0) {
                midi_opened = 1;
            }
        } else {
            /* Try to open a virtual port by default */
            if (shared_midi_open_virtual(g_tr7_repl_shared, "TR7MIDI") == 0) {
                midi_opened = 1;
                if (verbose) {
                    printf("Created virtual MIDI output: TR7MIDI\n");
                }
            }
        }

        if (!midi_opened) {
            fprintf(stderr, "Warning: No MIDI output available\n");
            fprintf(stderr, "Hint: Use -sf <soundfont.sf2> for built-in synth\n");
        }
    }

    int result = 0;

    if (input_file) {
        /* File mode - execute Scheme file */
        if (verbose) {
            printf("Executing: %s\n", input_file);
        }
        FILE *f = fopen(input_file, "r");
        if (!f) {
            fprintf(stderr, "Error: Cannot open file: %s\n", input_file);
            result = 1;
        } else {
            int status = tr7_run_file(g_tr7_repl_engine, f, input_file);
            fclose(f);
            if (status != 0) {
                tr7_t val = tr7_get_last_value(g_tr7_repl_engine);
                if (tr7_is_error(val)) {
                    tr7_t msg = tr7_error_message(val);
                    if (TR7_IS_STRING(msg)) {
                        fprintf(stderr, "Error: %s\n", tr7_string_buffer(msg));
                    } else {
                        fprintf(stderr, "Error: Failed to execute file\n");
                    }
                }
                result = 1;
            }
        }
    } else {
        /* REPL mode - initialize syntax highlighting */
        editor_ctx_t syntax_ctx;
        editor_ctx_init(&syntax_ctx);
        syntax_init_default_colors(&syntax_ctx);
        syntax_select_for_filename(&syntax_ctx, "input.scm");

        /* Load Lua and themes for consistent highlighting */
        struct loki_lua_opts lua_opts = {
            .bind_editor = 1,
            .load_config = 1,
            .reporter = NULL
        };
        syntax_ctx.L = loki_lua_bootstrap(&syntax_ctx, &lua_opts);

        tr7_repl_loop(&syntax_ctx);

        /* Cleanup Lua */
        if (syntax_ctx.L) {
            lua_close(syntax_ctx.L);
        }
    }

    /* Wait for audio buffer to drain before cleanup */
    if (g_tr7_repl_shared->tsf_enabled) {
        usleep(300000);  /* 300ms for audio tail */
    }

    /* Cleanup */
    shared_send_panic(g_tr7_repl_shared);
    shared_context_cleanup(g_tr7_repl_shared);
    free(g_tr7_repl_shared);
    g_tr7_repl_shared = NULL;
    tr7_engine_destroy(g_tr7_repl_engine);
    g_tr7_repl_engine = NULL;

    return result;
}

/* ============================================================================
 * TR7 Play Main Entry Point (headless file execution)
 * ============================================================================ */

int tr7_play_main(int argc, char **argv) {
    int verbose = 0;
    const char *input_file = NULL;
    const char *soundfont_path = NULL;

    /* Argument parsing - start at 0 since argv[0] may be the filename */
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else if ((strcmp(argv[i], "-sf") == 0 || strcmp(argv[i], "--soundfont") == 0) &&
                   i + 1 < argc) {
            soundfont_path = argv[++i];
        } else if (argv[i][0] != '-' && input_file == NULL) {
            input_file = argv[i];
        }
    }

    if (!input_file) {
        fprintf(stderr, "Usage: psnd play [-v] [-sf soundfont.sf2] <file.scm>\n");
        return 1;
    }

    /* Initialize TR7 engine */
    g_tr7_repl_engine = tr7_engine_create(0);
    if (!g_tr7_repl_engine) {
        fprintf(stderr, "Error: Failed to create TR7 engine\n");
        return 1;
    }

    /* Set TR7 search paths to .psnd/lib/scm in current working directory */
    static char tr7_lib_path[1100];
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd))) {
        snprintf(tr7_lib_path, sizeof(tr7_lib_path), "%s/.psnd/lib/scm", cwd);
        tr7_set_string(g_tr7_repl_engine, Tr7_StrID_Path, tr7_lib_path);
        tr7_set_string(g_tr7_repl_engine, Tr7_StrID_Library_Path, tr7_lib_path);
        tr7_set_string(g_tr7_repl_engine, Tr7_StrID_Include_Path, tr7_lib_path);
    }

    /* Import standard Scheme libraries */
    tr7_import_lib(g_tr7_repl_engine, "scheme/base");
    tr7_import_lib(g_tr7_repl_engine, "scheme/read");
    tr7_import_lib(g_tr7_repl_engine, "scheme/write");
    tr7_import_lib(g_tr7_repl_engine, "scheme/file");
    tr7_import_lib(g_tr7_repl_engine, "scheme/load");
    tr7_import_lib(g_tr7_repl_engine, "scheme/eval");

    /* Initialize shared context for MIDI/audio */
    g_tr7_repl_shared = calloc(1, sizeof(SharedContext));
    if (!g_tr7_repl_shared) {
        fprintf(stderr, "Error: Failed to create shared context\n");
        tr7_engine_destroy(g_tr7_repl_engine);
        return 1;
    }
    shared_context_init(g_tr7_repl_shared);

    /* Register music primitives */
    tr7_repl_register_music_funcs(g_tr7_repl_engine);

    /* Setup output */
    if (soundfont_path) {
        /* Use built-in synth */
        if (shared_tsf_load_soundfont(soundfont_path) != 0) {
            fprintf(stderr, "Error: Failed to load soundfont: %s\n", soundfont_path);
            shared_context_cleanup(g_tr7_repl_shared);
            free(g_tr7_repl_shared);
            tr7_engine_destroy(g_tr7_repl_engine);
            return 1;
        }
        g_tr7_repl_shared->tsf_enabled = 1;
        if (verbose) {
            printf("Using built-in synth: %s\n", soundfont_path);
        }
    } else {
        /* Try to open a virtual MIDI port */
        if (shared_midi_open_virtual(g_tr7_repl_shared, "TR7MIDI") != 0) {
            fprintf(stderr, "Warning: No MIDI output available\n");
            fprintf(stderr, "Hint: Use -sf <soundfont.sf2> for built-in synth\n");
        } else if (verbose) {
            printf("Created virtual MIDI output: TR7MIDI\n");
        }
    }

    /* Execute file */
    int result = 0;
    if (verbose) {
        printf("Executing: %s\n", input_file);
    }
    FILE *f = fopen(input_file, "r");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file: %s\n", input_file);
        result = 1;
    } else {
        int status = tr7_run_file(g_tr7_repl_engine, f, input_file);
        fclose(f);
        if (status == 0) {
            /* tr7_run_file returns 0 on load failure */
            fprintf(stderr, "Error: Failed to load file\n");
            result = 1;
        } else {
            /* Execution happened - check if result was an error */
            tr7_t val = tr7_get_last_value(g_tr7_repl_engine);
            if (tr7_is_error(val)) {
                tr7_t msg = tr7_error_message(val);
                if (TR7_IS_STRING(msg)) {
                    fprintf(stderr, "Error: %s\n", tr7_string_buffer(msg));
                } else {
                    fprintf(stderr, "Error: Failed to execute file\n");
                }
                result = 1;
            }
        }
    }

    /* Wait for audio buffer to drain before cleanup */
    if (g_tr7_repl_shared->tsf_enabled) {
        usleep(300000);  /* 300ms for audio tail */
    }

    /* Cleanup */
    shared_send_panic(g_tr7_repl_shared);
    shared_context_cleanup(g_tr7_repl_shared);
    free(g_tr7_repl_shared);
    g_tr7_repl_shared = NULL;
    tr7_engine_destroy(g_tr7_repl_engine);
    g_tr7_repl_engine = NULL;

    return result;
}
