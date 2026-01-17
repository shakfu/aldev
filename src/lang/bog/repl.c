/**
 * @file repl.c
 * @brief Bog REPL - Interactive Prolog-based music live coding environment.
 *
 * Provides a standalone REPL for the Bog language with:
 * - Live code evaluation with quantized transitions
 * - MIDI output via SharedContext
 * - Command-line interface similar to other psnd languages
 */

#include "bog_repl.h"
#include "repl.h"
#include "psnd.h"
#include "loki/core.h"
#include "loki/internal.h"
#include "loki/syntax.h"
#include "loki/lua.h"
#include "loki/repl_launcher.h"
#include "shared/repl_commands.h"
#include "shared/context.h"
#include "shared/midi/midi.h"
#include "shared/audio/audio.h"

/* Bog library headers */
#include "bog.h"
#include "scheduler.h"
#include "livecoding.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

/* ============================================================================
 * Bog Usage and Help
 * ============================================================================ */

static void print_bog_repl_usage(const char *prog) {
    printf("Usage: %s bog [options] [file.bog]\n", prog);
    printf("\n");
    printf("Bog - Prolog-based music live coding language.\n");
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
    printf("  %s bog                   Start interactive Bog REPL\n", prog);
    printf("  %s bog song.bog          Execute a Bog file\n", prog);
    printf("  %s bog -sf gm.sf2        REPL with built-in synth\n", prog);
    printf("  %s bog --virtual BogOut  REPL with virtual MIDI port\n", prog);
    printf("\n");
}

static void print_bog_repl_help(void) {
    shared_print_command_help();

    printf("Bog-specific Commands:\n");
    printf("  :play FILE        Load and execute a Bog file\n");
    printf("  :tempo BPM        Set tempo (default: 120)\n");
    printf("  :swing AMOUNT     Set swing (0.0-1.0, default: 0.0)\n");
    printf("\n");
    printf("Bog Syntax:\n");
    printf("  event(Voice, Pitch, Vel, T) :- beat(T, N).     Trigger on beats\n");
    printf("  event(kick, 36, 0.9, T) :- every(T, 0.5).      Every 0.5 beats\n");
    printf("  event(snare, 38, 0.8, T) :- beat(T, 2).        On beat 2\n");
    printf("  event(hat, 42, 0.6, T) :- every(T, 0.25).      Every quarter beat\n");
    printf("  event(sine, Note, Vel, T) :- pattern(T, Note). Melodic patterns\n");
    printf("\n");
    printf("Available Voices:\n");
    printf("  kick, snare, hat, clap, noise   (drums, channel 10)\n");
    printf("  sine, square, triangle          (melodic, channel 1)\n");
    printf("\n");
    printf("Examples:\n");
    printf("  event(kick, 36, 0.9, T) :- beat(T, 1).   ; Kick on beat 1\n");
    printf("  event(hat, 42, 0.5, T) :- every(T, 0.25). ; Hi-hat 16ths\n");
    printf("\n");
}

/* ============================================================================
 * Bog REPL State
 * ============================================================================ */

/* MIDI note mappings for drum sounds (GM drums, channel 10) */
#define BOG_MIDI_KICK  36
#define BOG_MIDI_SNARE 38
#define BOG_MIDI_HAT   42
#define BOG_MIDI_CLAP  39
#define BOG_MIDI_NOISE 46

/* MIDI channels */
#define BOG_DRUM_CHANNEL  10
#define BOG_SYNTH_CHANNEL 1

/* Bog REPL state */
static BogArena *g_bog_repl_arena = NULL;
static BogBuiltins *g_bog_repl_builtins = NULL;
static BogStateManager *g_bog_repl_state_manager = NULL;
static BogScheduler *g_bog_repl_scheduler = NULL;
static BogTransitionManager *g_bog_repl_transition = NULL;
static BogLiveEvaluator *g_bog_repl_evaluator = NULL;
static SharedContext *g_bog_repl_shared = NULL;
static int g_bog_repl_running = 0;
static double g_bog_repl_tempo = 120.0;
static double g_bog_repl_swing = 0.0;

/* ============================================================================
 * Audio Callbacks
 * ============================================================================ */

static int vel_to_midi(double velocity) {
    int v = (int)(velocity * 127.0);
    if (v < 0) v = 0;
    if (v > 127) v = 127;
    return v;
}

static void repl_audio_init(void *userdata) {
    (void)userdata;
}

static double repl_audio_time(void *userdata) {
    (void)userdata;
    if (!g_bog_repl_scheduler) return 0.0;
    return bog_scheduler_now(g_bog_repl_scheduler);
}

static void repl_audio_kick(void *userdata, double time, double velocity) {
    (void)userdata;
    (void)time;
    if (!g_bog_repl_shared) return;
    shared_send_note_on(g_bog_repl_shared, BOG_DRUM_CHANNEL, BOG_MIDI_KICK, vel_to_midi(velocity));
}

static void repl_audio_snare(void *userdata, double time, double velocity) {
    (void)userdata;
    (void)time;
    if (!g_bog_repl_shared) return;
    shared_send_note_on(g_bog_repl_shared, BOG_DRUM_CHANNEL, BOG_MIDI_SNARE, vel_to_midi(velocity));
}

static void repl_audio_hat(void *userdata, double time, double velocity) {
    (void)userdata;
    (void)time;
    if (!g_bog_repl_shared) return;
    shared_send_note_on(g_bog_repl_shared, BOG_DRUM_CHANNEL, BOG_MIDI_HAT, vel_to_midi(velocity));
}

static void repl_audio_clap(void *userdata, double time, double velocity) {
    (void)userdata;
    (void)time;
    if (!g_bog_repl_shared) return;
    shared_send_note_on(g_bog_repl_shared, BOG_DRUM_CHANNEL, BOG_MIDI_CLAP, vel_to_midi(velocity));
}

static void repl_audio_sine(void *userdata, double time, double midi, double velocity) {
    (void)userdata;
    (void)time;
    if (!g_bog_repl_shared) return;
    int pitch = (int)midi;
    if (pitch < 0) pitch = 0;
    if (pitch > 127) pitch = 127;
    shared_send_note_on(g_bog_repl_shared, BOG_SYNTH_CHANNEL, pitch, vel_to_midi(velocity));
}

static void repl_audio_square(void *userdata, double time, double midi, double velocity) {
    (void)userdata;
    (void)time;
    if (!g_bog_repl_shared) return;
    int pitch = (int)midi;
    if (pitch < 0) pitch = 0;
    if (pitch > 127) pitch = 127;
    shared_send_note_on(g_bog_repl_shared, BOG_SYNTH_CHANNEL + 1, pitch, vel_to_midi(velocity));
}

static void repl_audio_triangle(void *userdata, double time, double midi, double velocity) {
    (void)userdata;
    (void)time;
    if (!g_bog_repl_shared) return;
    int pitch = (int)midi;
    if (pitch < 0) pitch = 0;
    if (pitch > 127) pitch = 127;
    shared_send_note_on(g_bog_repl_shared, BOG_SYNTH_CHANNEL + 2, pitch, vel_to_midi(velocity));
}

static void repl_audio_noise(void *userdata, double time, double velocity) {
    (void)userdata;
    (void)time;
    if (!g_bog_repl_shared) return;
    shared_send_note_on(g_bog_repl_shared, BOG_DRUM_CHANNEL, BOG_MIDI_NOISE, vel_to_midi(velocity));
}

/* ============================================================================
 * Bog REPL Loop
 * ============================================================================ */

/* Stop callback for Bog REPL */
static void bog_stop_playback(void) {
    if (g_bog_repl_scheduler && g_bog_repl_running) {
        bog_scheduler_stop(g_bog_repl_scheduler);
        g_bog_repl_running = 0;
    }
    if (g_bog_repl_state_manager) {
        bog_state_manager_reset(g_bog_repl_state_manager);
    }
    if (g_bog_repl_shared) {
        shared_send_panic(g_bog_repl_shared);
    }
}

/* Check if string starts with prefix */
static int starts_with(const char *str, const char *prefix) {
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

/* Process a Bog REPL command. Returns: 0=continue, 1=quit, 2=evaluate as Bog */
static int bog_process_command(const char *input) {
    /* Try shared commands first */
    int result = shared_process_command(g_bog_repl_shared, input, bog_stop_playback);
    if (result == REPL_CMD_QUIT) {
        return 1; /* quit */
    }
    if (result == REPL_CMD_HANDLED) {
        return 0;
    }

    /* Handle Bog-specific commands */
    const char *cmd = input;
    if (cmd[0] == ':')
        cmd++;

    /* Help */
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0 || strcmp(cmd, "?") == 0) {
        print_bog_repl_help();
        return 0;
    }

    /* :tempo BPM */
    if (starts_with(cmd, "tempo ")) {
        double bpm = atof(cmd + 6);
        if (bpm >= 20.0 && bpm <= 400.0) {
            g_bog_repl_tempo = bpm;
            if (g_bog_repl_scheduler) {
                bog_scheduler_configure(g_bog_repl_scheduler, bpm, g_bog_repl_swing, 50.0, 0.25);
            }
            printf("Tempo: %.1f BPM\n", bpm);
        } else {
            printf("Invalid tempo (20-400)\n");
        }
        return 0;
    }

    /* :swing AMOUNT */
    if (starts_with(cmd, "swing ")) {
        double swing = atof(cmd + 6);
        if (swing >= 0.0 && swing <= 1.0) {
            g_bog_repl_swing = swing;
            if (g_bog_repl_scheduler) {
                bog_scheduler_configure(g_bog_repl_scheduler, g_bog_repl_tempo, swing, 50.0, 0.25);
            }
            printf("Swing: %.2f\n", swing);
        } else {
            printf("Invalid swing (0.0-1.0)\n");
        }
        return 0;
    }

    /* :play FILE */
    if (starts_with(cmd, "play ")) {
        const char *path = cmd + 5;
        while (*path == ' ') path++;
        if (*path) {
            FILE *f = fopen(path, "r");
            if (!f) {
                printf("Error: Cannot open file: %s\n", path);
            } else {
                /* Read file contents */
                fseek(f, 0, SEEK_END);
                long size = ftell(f);
                fseek(f, 0, SEEK_SET);

                char *code = malloc(size + 1);
                if (code) {
                    size_t read = fread(code, 1, size, f);
                    code[read] = '\0';

                    /* Evaluate */
                    char *error = NULL;
                    if (g_bog_repl_evaluator) {
                        if (bog_live_evaluator_evaluate(g_bog_repl_evaluator, code, &error)) {
                            printf("Loaded: %s\n", path);
                            if (!g_bog_repl_running && g_bog_repl_scheduler) {
                                bog_scheduler_start(g_bog_repl_scheduler);
                                g_bog_repl_running = 1;
                            }
                        } else {
                            printf("Error: %s\n", error ? error : "Parse error");
                            if (error) free(error);
                        }
                    }
                    free(code);
                }
                fclose(f);
            }
        } else {
            printf("Usage: :play PATH\n");
        }
        return 0;
    }

    return 2; /* evaluate as Bog */
}

/* Tick scheduler and process transitions */
static void bog_repl_tick(void) {
    if (!g_bog_repl_scheduler || !g_bog_repl_running) return;

    bog_scheduler_tick(g_bog_repl_scheduler);

    if (g_bog_repl_transition) {
        double now = bog_scheduler_now(g_bog_repl_scheduler);
        bog_transition_manager_process(g_bog_repl_transition, now);
    }
}

/* Non-interactive Bog REPL loop for piped input */
static void bog_repl_loop_pipe(void) {
    char line[4096];

    while (fgets(line, sizeof(line), stdin) != NULL) {
        /* Strip trailing newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }

        if (len == 0) continue;

        int result = bog_process_command(line);
        if (result == 1) break;      /* quit */
        if (result == 0) {
            bog_repl_tick();
            continue;   /* command handled */
        }

        /* Evaluate as Bog code */
        char *error = NULL;
        if (g_bog_repl_evaluator) {
            if (bog_live_evaluator_evaluate(g_bog_repl_evaluator, line, &error)) {
                printf("ok\n");
                if (!g_bog_repl_running && g_bog_repl_scheduler) {
                    bog_scheduler_start(g_bog_repl_scheduler);
                    g_bog_repl_running = 1;
                }
            } else {
                printf("Error: %s\n", error ? error : "Parse error");
                if (error) free(error);
            }
        }
        bog_repl_tick();
        fflush(stdout);
    }
}

static void bog_repl_loop(editor_ctx_t *syntax_ctx) {
    ReplLineEditor ed;
    char *input;
    char history_path[512] = {0};

    /* Use non-interactive mode for piped input */
    if (!isatty(STDIN_FILENO)) {
        bog_repl_loop_pipe();
        return;
    }

    repl_editor_init(&ed);

    /* Build history file path and load history */
    struct stat st;
    if (stat(".psnd", &st) == 0 && S_ISDIR(st.st_mode)) {
        snprintf(history_path, sizeof(history_path), ".psnd/bog_history");
    } else {
        const char *home = getenv("HOME");
        if (home) {
            char global_psnd[512];
            snprintf(global_psnd, sizeof(global_psnd), "%s/.psnd", home);
            if (stat(global_psnd, &st) == 0 && S_ISDIR(st.st_mode)) {
                snprintf(history_path, sizeof(history_path), "%s/bog_history", global_psnd);
            }
        }
    }
    if (history_path[0]) {
        repl_history_load(&ed, history_path);
    }

    printf("Bog REPL %s (type :h for help, :q to quit)\n", PSND_VERSION);

    /* Enable raw mode for syntax-highlighted input */
    repl_enable_raw_mode();

    while (1) {
        /* Tick scheduler while waiting for input */
        bog_repl_tick();

        input = repl_readline(syntax_ctx, &ed, "bog> ");

        if (input == NULL) {
            /* EOF - exit cleanly */
            break;
        }

        if (input[0] == '\0') {
            continue;
        }

        repl_add_history(&ed, input);

        /* Process command */
        int result = bog_process_command(input);
        if (result == 1) break;      /* quit */
        if (result == 0) {
            bog_repl_tick();
            shared_repl_link_check();
            continue;   /* command handled */
        }

        /* Evaluate Bog code */
        char *error = NULL;
        if (g_bog_repl_evaluator) {
            if (bog_live_evaluator_evaluate(g_bog_repl_evaluator, input, &error)) {
                printf("ok\n");
                if (!g_bog_repl_running && g_bog_repl_scheduler) {
                    bog_scheduler_start(g_bog_repl_scheduler);
                    g_bog_repl_running = 1;
                }
            } else {
                printf("Error: %s\n", error ? error : "Parse error");
                if (error) free(error);
            }
        }
        bog_repl_tick();
        shared_repl_link_check();
    }

    /* Disable raw mode before exit */
    repl_disable_raw_mode();

    /* Save history */
    if (history_path[0]) {
        repl_history_save(&ed, history_path);
    }

    repl_editor_cleanup(&ed);
}

/* ============================================================================
 * Shared REPL Launcher Callbacks
 * ============================================================================ */

/* List MIDI ports */
static void bog_cb_list_ports(void) {
    SharedContext tmp;
    shared_context_init(&tmp);
    shared_midi_list_ports(&tmp);
    shared_context_cleanup(&tmp);
}

/* Initialize Bog context and MIDI/audio */
static void *bog_cb_init(const SharedReplArgs *args) {
    /* Create arena */
    g_bog_repl_arena = bog_arena_create();
    if (!g_bog_repl_arena) {
        fprintf(stderr, "Error: Failed to create Bog arena\n");
        return NULL;
    }

    /* Create builtins */
    g_bog_repl_builtins = bog_create_builtins(g_bog_repl_arena);
    if (!g_bog_repl_builtins) {
        fprintf(stderr, "Error: Failed to create Bog builtins\n");
        bog_arena_destroy(g_bog_repl_arena);
        g_bog_repl_arena = NULL;
        return NULL;
    }

    /* Create state manager */
    g_bog_repl_state_manager = bog_state_manager_create();
    if (!g_bog_repl_state_manager) {
        fprintf(stderr, "Error: Failed to create Bog state manager\n");
        bog_arena_destroy(g_bog_repl_arena);
        g_bog_repl_arena = NULL;
        return NULL;
    }

    /* Setup audio callbacks */
    BogAudioCallbacks audio = {
        .userdata = NULL,
        .init = repl_audio_init,
        .time = repl_audio_time,
        .kick = repl_audio_kick,
        .snare = repl_audio_snare,
        .hat = repl_audio_hat,
        .clap = repl_audio_clap,
        .sine = repl_audio_sine,
        .square = repl_audio_square,
        .triangle = repl_audio_triangle,
        .noise = repl_audio_noise,
    };

    /* Create scheduler */
    g_bog_repl_scheduler = bog_scheduler_create(&audio, g_bog_repl_builtins, g_bog_repl_state_manager);
    if (!g_bog_repl_scheduler) {
        fprintf(stderr, "Error: Failed to create Bog scheduler\n");
        bog_state_manager_destroy(g_bog_repl_state_manager);
        bog_arena_destroy(g_bog_repl_arena);
        g_bog_repl_state_manager = NULL;
        g_bog_repl_arena = NULL;
        return NULL;
    }

    /* Configure scheduler */
    bog_scheduler_configure(g_bog_repl_scheduler, g_bog_repl_tempo, g_bog_repl_swing, 50.0, 0.25);

    /* Create transition manager */
    g_bog_repl_transition = bog_transition_manager_create(g_bog_repl_scheduler, 4.0);
    if (!g_bog_repl_transition) {
        fprintf(stderr, "Error: Failed to create Bog transition manager\n");
        bog_scheduler_destroy(g_bog_repl_scheduler);
        bog_state_manager_destroy(g_bog_repl_state_manager);
        bog_arena_destroy(g_bog_repl_arena);
        g_bog_repl_scheduler = NULL;
        g_bog_repl_state_manager = NULL;
        g_bog_repl_arena = NULL;
        return NULL;
    }

    /* Create live evaluator */
    g_bog_repl_evaluator = bog_live_evaluator_create(g_bog_repl_scheduler, 0.1);
    if (!g_bog_repl_evaluator) {
        fprintf(stderr, "Error: Failed to create Bog live evaluator\n");
        bog_transition_manager_destroy(g_bog_repl_transition);
        bog_scheduler_destroy(g_bog_repl_scheduler);
        bog_state_manager_destroy(g_bog_repl_state_manager);
        bog_arena_destroy(g_bog_repl_arena);
        g_bog_repl_transition = NULL;
        g_bog_repl_scheduler = NULL;
        g_bog_repl_state_manager = NULL;
        g_bog_repl_arena = NULL;
        return NULL;
    }

    /* Initialize shared context for MIDI/audio */
    g_bog_repl_shared = calloc(1, sizeof(SharedContext));
    if (!g_bog_repl_shared) {
        fprintf(stderr, "Error: Failed to create shared context\n");
        bog_live_evaluator_destroy(g_bog_repl_evaluator);
        bog_transition_manager_destroy(g_bog_repl_transition);
        bog_scheduler_destroy(g_bog_repl_scheduler);
        bog_state_manager_destroy(g_bog_repl_state_manager);
        bog_arena_destroy(g_bog_repl_arena);
        g_bog_repl_evaluator = NULL;
        g_bog_repl_transition = NULL;
        g_bog_repl_scheduler = NULL;
        g_bog_repl_state_manager = NULL;
        g_bog_repl_arena = NULL;
        return NULL;
    }
    shared_context_init(g_bog_repl_shared);

    /* Setup output */
    if (args->soundfont_path) {
        /* Use built-in synth */
        if (shared_tsf_load_soundfont(args->soundfont_path) != 0) {
            fprintf(stderr, "Error: Failed to load soundfont: %s\n", args->soundfont_path);
            shared_context_cleanup(g_bog_repl_shared);
            free(g_bog_repl_shared);
            bog_live_evaluator_destroy(g_bog_repl_evaluator);
            bog_transition_manager_destroy(g_bog_repl_transition);
            bog_scheduler_destroy(g_bog_repl_scheduler);
            bog_state_manager_destroy(g_bog_repl_state_manager);
            bog_arena_destroy(g_bog_repl_arena);
            g_bog_repl_shared = NULL;
            g_bog_repl_evaluator = NULL;
            g_bog_repl_transition = NULL;
            g_bog_repl_scheduler = NULL;
            g_bog_repl_state_manager = NULL;
            g_bog_repl_arena = NULL;
            return NULL;
        }
        g_bog_repl_shared->tsf_enabled = 1;
        if (args->verbose) {
            printf("Using built-in synth: %s\n", args->soundfont_path);
        }
    } else {
        /* Setup MIDI output */
        int midi_opened = 0;

        if (args->virtual_name) {
            if (shared_midi_open_virtual(g_bog_repl_shared, args->virtual_name) == 0) {
                midi_opened = 1;
                if (args->verbose) {
                    printf("Created virtual MIDI port: %s\n", args->virtual_name);
                }
            }
        } else if (args->port_index >= 0) {
            if (shared_midi_open_port(g_bog_repl_shared, args->port_index) == 0) {
                midi_opened = 1;
            }
        } else {
            /* Try to open a virtual port by default */
            if (shared_midi_open_virtual(g_bog_repl_shared, "BogMIDI") == 0) {
                midi_opened = 1;
                if (args->verbose) {
                    printf("Created virtual MIDI output: BogMIDI\n");
                }
            }
        }

        if (!midi_opened) {
            fprintf(stderr, "Warning: No MIDI output available\n");
            fprintf(stderr, "Hint: Use -sf <soundfont.sf2> for built-in synth\n");
        }
    }

    /* Initialize Link callbacks for REPL notifications */
    shared_repl_link_init_callbacks(g_bog_repl_shared);

    /* Return non-NULL to indicate success */
    return g_bog_repl_scheduler;
}

/* Cleanup Bog context and MIDI/audio */
static void bog_cb_cleanup(void *lang_ctx) {
    (void)lang_ctx;

    /* Cleanup Link callbacks */
    shared_repl_link_cleanup_callbacks();

    /* Stop scheduler */
    if (g_bog_repl_scheduler && g_bog_repl_running) {
        bog_scheduler_stop(g_bog_repl_scheduler);
        g_bog_repl_running = 0;
    }

    /* Wait for audio buffer to drain */
    if (g_bog_repl_shared && g_bog_repl_shared->tsf_enabled) {
        usleep(300000);  /* 300ms for audio tail */
    }

    /* Cleanup */
    if (g_bog_repl_shared) {
        shared_send_panic(g_bog_repl_shared);
        shared_context_cleanup(g_bog_repl_shared);
        free(g_bog_repl_shared);
        g_bog_repl_shared = NULL;
    }

    if (g_bog_repl_evaluator) {
        bog_live_evaluator_destroy(g_bog_repl_evaluator);
        g_bog_repl_evaluator = NULL;
    }
    if (g_bog_repl_transition) {
        bog_transition_manager_destroy(g_bog_repl_transition);
        g_bog_repl_transition = NULL;
    }
    if (g_bog_repl_scheduler) {
        bog_scheduler_destroy(g_bog_repl_scheduler);
        g_bog_repl_scheduler = NULL;
    }
    if (g_bog_repl_state_manager) {
        bog_state_manager_destroy(g_bog_repl_state_manager);
        g_bog_repl_state_manager = NULL;
    }
    if (g_bog_repl_arena) {
        bog_arena_destroy(g_bog_repl_arena);
        g_bog_repl_arena = NULL;
    }
}

/* Execute a Bog file */
static int bog_cb_exec_file(void *lang_ctx, const char *path, int verbose) {
    (void)lang_ctx;
    (void)verbose;

    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file: %s\n", path);
        return 1;
    }

    /* Read file contents */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *code = malloc(size + 1);
    if (!code) {
        fclose(f);
        fprintf(stderr, "Error: Out of memory\n");
        return 1;
    }

    size_t read = fread(code, 1, size, f);
    code[read] = '\0';
    fclose(f);

    /* Evaluate */
    char *error = NULL;
    int success = 0;
    if (g_bog_repl_evaluator) {
        success = bog_live_evaluator_evaluate(g_bog_repl_evaluator, code, &error);
    }
    free(code);

    if (!success) {
        fprintf(stderr, "Error: %s\n", error ? error : "Parse error");
        if (error) free(error);
        return 1;
    }

    /* Start scheduler and run until interrupted */
    if (g_bog_repl_scheduler) {
        bog_scheduler_start(g_bog_repl_scheduler);
        g_bog_repl_running = 1;

        /* Run scheduler loop */
        while (g_bog_repl_running) {
            bog_scheduler_tick(g_bog_repl_scheduler);
            if (g_bog_repl_transition) {
                double now = bog_scheduler_now(g_bog_repl_scheduler);
                bog_transition_manager_process(g_bog_repl_transition, now);
            }
            usleep(10000);  /* 10ms tick */
        }
    }

    return 0;
}

/* Run the Bog REPL loop */
static void bog_cb_repl_loop(void *lang_ctx, editor_ctx_t *syntax_ctx) {
    (void)lang_ctx;
    bog_repl_loop(syntax_ctx);
}

/* Bog shared REPL callbacks */
static const SharedReplCallbacks bog_repl_callbacks = {
    .name = "bog",
    .file_ext = ".bog",
    .prog_name = PSND_NAME,
    .print_usage = print_bog_repl_usage,
    .list_ports = bog_cb_list_ports,
    .init = bog_cb_init,
    .cleanup = bog_cb_cleanup,
    .exec_file = bog_cb_exec_file,
    .repl_loop = bog_cb_repl_loop,
};

/* ============================================================================
 * Bog REPL Main Entry Points
 * ============================================================================ */

int bog_repl_main(int argc, char **argv) {
    return shared_lang_repl_main(&bog_repl_callbacks, argc, argv);
}

int bog_play_main(int argc, char **argv) {
    return shared_lang_play_main(&bog_repl_callbacks, argc, argv);
}
