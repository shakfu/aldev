/**
 * @file repl.c
 * @brief Joy REPL - Interactive stack-based music composition terminal.
 */

#include "repl.h"
#include "psnd.h"
#include "loki/core.h"
#include "loki/internal.h"
#include "loki/syntax.h"
#include "loki/lua.h"
#include "loki/repl_launcher.h"
#include "shared/repl_commands.h"

/* Joy library headers */
#include "joy_runtime.h"
#include "joy_parser.h"
#include "joy_midi_backend.h"
#include "music_notation.h"
#include "music_context.h"
#include "midi_primitives.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <setjmp.h>

/* ============================================================================
 * Joy Usage and Help
 * ============================================================================ */

static void print_joy_repl_usage(const char *prog) {
    printf("Usage: %s joy [options] [file.joy]\n", prog);
    printf("\n");
    printf("Joy concatenative music language interpreter with MIDI output.\n");
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
    printf("  %s joy                   Start interactive Joy REPL\n", prog);
    printf("  %s joy song.joy          Execute a Joy file\n", prog);
    printf("  %s joy -sf gm.sf2        REPL with built-in synth\n", prog);
    printf("  %s joy --virtual JoyOut  REPL with virtual MIDI port\n", prog);
    printf("\n");
}

static void print_joy_repl_help(void) {
    shared_print_command_help();

    printf("Joy-specific Commands:\n");
    printf("  .               Print stack\n");
    printf("\n");
    printf("Joy Syntax:\n");
    printf("  c d e f g a b   Note names (octave 4 by default)\n");
    printf("  c5 d3 e6        Notes with explicit octave\n");
    printf("  c+ c-           Sharps and flats\n");
    printf("  [c d e] play    Play notes sequentially\n");
    printf("  [c e g] chord   Play notes as chord\n");
    printf("  c major chord   Build and play C major chord\n");
    printf("  120 tempo       Set tempo to 120 BPM\n");
    printf("  80 vol          Set volume to 80%%\n");
    printf("\n");
    printf("Combinators:\n");
    printf("  [1 2 3] [2 *] map   -> [2 4 6]\n");
    printf("  [c d e] [12 +] map  -> transpose up octave\n");
    printf("  5 [c e g] times     -> repeat 5 times\n");
    printf("\n");
}

/* ============================================================================
 * Joy REPL Loop
 * ============================================================================ */

/* Stop callback for Joy REPL */
static void joy_stop_playback(void) {
    joy_midi_panic();
}

/* Process a Joy REPL command. Returns: 0=continue, 1=quit, 2=evaluate as Joy code */
static int joy_process_command(const char* input) {
    /* Try shared commands first */
    SharedContext* ctx = joy_get_shared_context();
    int result = shared_process_command(ctx, input, joy_stop_playback);
    if (result == REPL_CMD_QUIT) {
        return 1; /* quit */
    }
    if (result == REPL_CMD_HANDLED) {
        return 0;
    }

    /* Handle Joy-specific commands */
    const char *cmd = input;
    if (cmd[0] == ':')
        cmd++;

    /* Help - add Joy-specific help */
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0 || strcmp(cmd, "?") == 0) {
        print_joy_repl_help();
        return 0;
    }

    /* Print stack */
    if (strcmp(input, ".") == 0) {
        /* Print stack - handled by caller */
        return 2;
    }

    return 2; /* evaluate as Joy code */
}

/* Non-interactive Joy REPL loop for piped input */
static void joy_repl_loop_pipe(JoyContext *ctx) {
    char line[MAX_INPUT_LENGTH];
    jmp_buf error_recovery;

    ctx->error_jmp = &error_recovery;
    joy_set_current_context(ctx);

    while (fgets(line, sizeof(line), stdin) != NULL) {
        /* Strip trailing newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }

        if (len == 0) continue;

        int result = joy_process_command(line);
        if (result == 1) break; /* quit */
        if (result == 0) continue; /* command handled */

        /* Set up error recovery and evaluate */
        if (setjmp(error_recovery) != 0) {
            continue;
        }
        joy_eval_line(ctx, line);
    }
}

static void joy_repl_loop(JoyContext *ctx, editor_ctx_t *syntax_ctx) {
    ReplLineEditor ed;
    char *input;
    jmp_buf error_recovery;

    /* Use non-interactive mode for piped input */
    if (!isatty(STDIN_FILENO)) {
        joy_repl_loop_pipe(ctx);
        return;
    }

    repl_editor_init(&ed);

    /* Set up error recovery */
    ctx->error_jmp = &error_recovery;
    joy_set_current_context(ctx);

    printf("Joy REPL %s (type help for help, quit to exit)\n", PSND_VERSION);

    /* Enable raw mode for syntax-highlighted input */
    repl_enable_raw_mode();

    while (1) {
        input = repl_readline(syntax_ctx, &ed, "joy> ");

        if (input == NULL) {
            /* EOF - exit cleanly */
            break;
        }

        if (input[0] == '\0') {
            continue;
        }

        repl_add_history(&ed, input);

        /* Process command */
        int result = joy_process_command(input);
        if (result == 1) break;      /* quit */
        if (result == 0) continue;   /* command handled */

        /* Set up error recovery point */
        if (setjmp(error_recovery) != 0) {
            /* Error occurred during eval - continue REPL */
            continue;
        }

        /* Evaluate Joy code */
        joy_eval_line(ctx, input);
    }

    /* Disable raw mode before exit */
    repl_disable_raw_mode();
    repl_editor_cleanup(&ed);
}

/* ============================================================================
 * Shared REPL Launcher Callbacks
 * ============================================================================ */

/* List MIDI ports */
static void joy_cb_list_ports(void) {
    if (joy_midi_init() == 0) {
        joy_midi_list_ports();
        joy_midi_cleanup();
    }
}

/* Initialize Joy context and MIDI/audio */
static void *joy_cb_init(const SharedReplArgs *args) {
    /* Initialize Joy context */
    JoyContext *ctx = joy_context_new();
    if (!ctx) {
        fprintf(stderr, "Error: Failed to create Joy context\n");
        return NULL;
    }

    /* Register primitives */
    joy_register_primitives(ctx);
    music_notation_init(ctx);
    joy_midi_register_primitives(ctx);

    /* Set parser dictionary for DEFINE support */
    joy_set_parser_dict(ctx->dictionary);

    /* Initialize MIDI backend */
    if (joy_midi_init() != 0) {
        fprintf(stderr, "Warning: Failed to initialize MIDI backend\n");
    }

    /* Setup output */
    if (args->soundfont_path) {
        /* Use built-in synth */
        if (joy_tsf_load_soundfont(args->soundfont_path) != 0) {
            fprintf(stderr, "Error: Failed to load soundfont: %s\n", args->soundfont_path);
            joy_midi_cleanup();
            music_notation_cleanup(ctx);
            joy_context_free(ctx);
            return NULL;
        }
        if (joy_tsf_enable() != 0) {
            fprintf(stderr, "Error: Failed to enable built-in synth\n");
            joy_midi_cleanup();
            music_notation_cleanup(ctx);
            joy_context_free(ctx);
            return NULL;
        }
        if (args->verbose) {
            printf("Using built-in synth: %s\n", args->soundfont_path);
        }
    } else {
        /* Setup MIDI output */
        int midi_opened = 0;

        if (args->virtual_name) {
            if (joy_midi_open_virtual(args->virtual_name) == 0) {
                midi_opened = 1;
                if (args->verbose) {
                    printf("Created virtual MIDI output: %s\n", args->virtual_name);
                }
            }
        } else if (args->port_index >= 0) {
            if (joy_midi_open_port(args->port_index) == 0) {
                midi_opened = 1;
            }
        } else {
            /* Try to open a virtual port by default */
            if (joy_midi_open_virtual("JoyMIDI") == 0) {
                midi_opened = 1;
                if (args->verbose) {
                    printf("Created virtual MIDI output: JoyMIDI\n");
                }
            }
        }

        if (!midi_opened) {
            fprintf(stderr, "Warning: No MIDI output available\n");
            fprintf(stderr, "Hint: Use -sf <soundfont.sf2> for built-in synth\n");
        }
    }

    return ctx;
}

/* Cleanup Joy context and MIDI/audio */
static void joy_cb_cleanup(void *lang_ctx) {
    JoyContext *ctx = (JoyContext *)lang_ctx;

    /* Wait for audio buffer to drain */
    if (joy_tsf_is_enabled()) {
        usleep(300000);  /* 300ms for audio tail */
    }

    /* Cleanup */
    joy_midi_panic();
    joy_csound_cleanup();
    joy_link_cleanup();
    joy_midi_cleanup();
    music_notation_cleanup(ctx);
    joy_context_free(ctx);
}

/* Execute a Joy file */
static int joy_cb_exec_file(void *lang_ctx, const char *path, int verbose) {
    JoyContext *ctx = (JoyContext *)lang_ctx;
    (void)verbose;

    int result = joy_load_file(ctx, path);
    if (result != 0) {
        fprintf(stderr, "Error: Failed to execute file\n");
    }
    return result;
}

/* Run the Joy REPL loop */
static void joy_cb_repl_loop(void *lang_ctx, editor_ctx_t *syntax_ctx) {
    JoyContext *ctx = (JoyContext *)lang_ctx;
    joy_repl_loop(ctx, syntax_ctx);
}

/* Joy shared REPL callbacks */
static const SharedReplCallbacks joy_repl_callbacks = {
    .name = "joy",
    .file_ext = ".joy",
    .prog_name = PSND_NAME,
    .print_usage = print_joy_repl_usage,
    .list_ports = joy_cb_list_ports,
    .init = joy_cb_init,
    .cleanup = joy_cb_cleanup,
    .exec_file = joy_cb_exec_file,
    .repl_loop = joy_cb_repl_loop,
};

/* ============================================================================
 * Joy REPL Main Entry Point
 * ============================================================================ */

int joy_repl_main(int argc, char **argv) {
    return shared_lang_repl_main(&joy_repl_callbacks, argc, argv);
}

/* ============================================================================
 * Joy Play Main Entry Point (headless file execution)
 * ============================================================================ */

int joy_play_main(int argc, char **argv) {
    return shared_lang_play_main(&joy_repl_callbacks, argc, argv);
}
