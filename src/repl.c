/**
 * @file alda_repl.c
 * @brief Alda REPL - Interactive music composition terminal with syntax highlighting.
 *
 * Direct Alda notation input with real-time syntax highlighting.
 * Uses custom line editor with terminal raw mode for colored input.
 */

/* Alda language headers */
#ifdef LANG_ALDA
#include "alda/alda.h"
#include "alda/context.h"
#include "alda/midi_backend.h"
#include "alda/tsf_backend.h"
#include "alda/csound_backend.h"
#include "alda/scheduler.h"
#include "alda/interpreter.h"
#include "alda/async.h"
#endif

#include "version.h"
#include "loki/core.h"
#include "loki/internal.h"
#include "loki/terminal.h"
#include "loki/syntax.h"
#include "loki/lua.h"

/* Shared REPL commands */
#include "shared/repl_commands.h"

/* Joy language headers */
#ifdef LANG_JOY
#include "joy_runtime.h"
#include "joy_parser.h"
#include "joy_midi_backend.h"
#include "music_notation.h"
#include "music_context.h"
#include "midi_primitives.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>

#ifndef _WIN32
#include <getopt.h>
#endif

#define MAX_INPUT_LENGTH 1024
#define REPL_HISTORY_MAX 64

/* ============================================================================
 * Syntax-Highlighting Line Editor
 * ============================================================================ */

/* Control key definitions (not defined in loki_internal.h) */
#ifndef CTRL_A
#define CTRL_A 1
#endif
#ifndef CTRL_K
#define CTRL_K 11
#endif

typedef struct {
    char buf[MAX_INPUT_LENGTH];      /* Input buffer */
    int len;                         /* Current length */
    int pos;                         /* Cursor position */
    char *history[REPL_HISTORY_MAX]; /* History entries */
    int history_len;                 /* Number of history entries */
    int history_idx;                 /* Current history index (-1 = current input) */
    char saved_buf[MAX_INPUT_LENGTH];/* Saved current input when browsing history */
    int saved_len;                   /* Saved length */
    unsigned char hl[MAX_INPUT_LENGTH]; /* Highlight types per character */
} ReplLineEditor;

static void repl_editor_init(ReplLineEditor *ed) {
    memset(ed, 0, sizeof(*ed));
    ed->history_idx = -1;
}

static void repl_editor_cleanup(ReplLineEditor *ed) {
    for (int i = 0; i < ed->history_len; i++) {
        free(ed->history[i]);
    }
    memset(ed, 0, sizeof(*ed));
}

static void repl_add_history(ReplLineEditor *ed, const char *line) {
    if (!line || !line[0]) return;

    /* Don't add duplicates of the last entry */
    if (ed->history_len > 0 && strcmp(ed->history[ed->history_len - 1], line) == 0) {
        return;
    }

    /* Remove oldest if full */
    if (ed->history_len >= REPL_HISTORY_MAX) {
        free(ed->history[0]);
        memmove(ed->history, ed->history + 1, (REPL_HISTORY_MAX - 1) * sizeof(char *));
        ed->history_len--;
    }

    ed->history[ed->history_len++] = strdup(line);
}

/* Original termios for REPL raw mode (separate from editor) */
static struct termios repl_orig_termios;
static int repl_rawmode = 0;

static void repl_disable_raw_mode(void) {
    if (repl_rawmode) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &repl_orig_termios);
        repl_rawmode = 0;
    }
}

static int repl_enable_raw_mode(void) {
    struct termios raw;

    if (repl_rawmode) return 0;
    if (!isatty(STDIN_FILENO)) return -1;
    if (tcgetattr(STDIN_FILENO, &repl_orig_termios) == -1) return -1;

    raw = repl_orig_termios;
    /* Input modes: no break, no CR to NL, no parity, no strip, no flow ctrl */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* Output modes - keep post processing for proper newline handling */
    /* raw.c_oflag &= ~(OPOST); -- keep OPOST for REPL */
    /* Control modes - 8 bit chars */
    raw.c_cflag |= (CS8);
    /* Local modes - echo off, canonical off, no extended functions
     * Keep ISIG so Ctrl-C still works for interrupt */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    /* Return each byte immediately */
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) return -1;
    repl_rawmode = 1;
    return 0;
}

static int repl_is_separator(int c, const char *separators) {
    if (isspace(c)) return 1;
    if (c == '\0') return 1;
    return (separators && strchr(separators, c) != NULL);
}

static void repl_highlight_line(editor_ctx_t *syntax_ctx, ReplLineEditor *ed) {
    struct t_editor_syntax *syn = syntax_ctx->syntax;

    memset(ed->hl, HL_NORMAL, ed->len);

    if (!syn || ed->len == 0) return;

    char **keywords = syn->keywords;
    char *scs = syn->singleline_comment_start;
    char *separators = syn->separators;
    int highlight_strings = syn->flags & HL_HIGHLIGHT_STRINGS;
    int highlight_numbers = syn->flags & HL_HIGHLIGHT_NUMBERS;

    int i = 0;
    int prev_sep = 1;
    int in_string = 0;
    char *p = ed->buf;

    /* Skip leading whitespace */
    while (*p && isspace(*p) && i < ed->len) {
        p++;
        i++;
    }

    while (*p && i < ed->len) {
        /* Handle single-line comments */
        if (prev_sep && scs && scs[0] && *p == scs[0] &&
            (scs[1] == '\0' || (i < ed->len - 1 && *(p+1) == scs[1]))) {
            memset(ed->hl + i, HL_COMMENT, ed->len - i);
            break;
        }

        /* Handle strings */
        if (in_string) {
            ed->hl[i] = HL_STRING;
            if (*p == '\\' && i < ed->len - 1) {
                ed->hl[i+1] = HL_STRING;
                p += 2;
                i += 2;
                continue;
            }
            if (*p == in_string) in_string = 0;
            p++;
            i++;
            prev_sep = 1;
            continue;
        } else if (highlight_strings && (*p == '"' || *p == '\'')) {
            in_string = *p;
            ed->hl[i] = HL_STRING;
            p++;
            i++;
            prev_sep = 0;
            continue;
        }

        /* Handle numbers */
        if (highlight_numbers && (isdigit(*p) || (*p == '.' && i < ed->len - 1 && isdigit(*(p+1)))) && prev_sep) {
            while (i < ed->len && (isdigit(*p) || *p == '.')) {
                ed->hl[i] = HL_NUMBER;
                p++;
                i++;
            }
            prev_sep = 0;
            continue;
        }

        /* Handle keywords */
        if (prev_sep && keywords) {
            int keyword_found = 0;
            for (int j = 0; keywords[j]; j++) {
                size_t klen = strlen(keywords[j]);
                int kw2 = (klen > 0 && keywords[j][klen-1] == '|');
                if (kw2) klen--;

                /* Safety: skip zero-length keywords */
                if (klen == 0) continue;

                if ((int)(i + klen) <= ed->len &&
                    memcmp(p, keywords[j], klen) == 0 &&
                    repl_is_separator(p[klen], separators)) {
                    int hl_type = kw2 ? HL_KEYWORD2 : HL_KEYWORD1;
                    memset(ed->hl + i, hl_type, klen);
                    p += klen;
                    i += (int)klen;
                    prev_sep = 0;
                    keyword_found = 1;
                    break;
                }
            }
            if (keyword_found) continue;
        }

        prev_sep = repl_is_separator(*p, separators);
        p++;
        i++;
    }
}

static void repl_render_line(editor_ctx_t *syntax_ctx, ReplLineEditor *ed, const char *prompt) {
    struct abuf ab = ABUF_INIT;

    /* Move to start of line, clear it */
    terminal_buffer_append(&ab, "\r\x1b[K", 4);

    /* Output prompt (no highlighting) */
    terminal_buffer_append(&ab, prompt, strlen(prompt));

    /* Highlight the input */
    repl_highlight_line(syntax_ctx, ed);

    /* Output highlighted text */
    int current_hl = -1;
    for (int i = 0; i < ed->len; i++) {
        if (ed->hl[i] != current_hl) {
            char color[32];
            int clen = syntax_format_color(syntax_ctx, ed->hl[i], color, sizeof(color));
            terminal_buffer_append(&ab, color, clen);
            current_hl = ed->hl[i];
        }
        terminal_buffer_append(&ab, &ed->buf[i], 1);
    }

    /* Reset color */
    terminal_buffer_append(&ab, "\x1b[39m", 5);

    /* Position cursor */
    if (ed->pos < ed->len) {
        /* Move cursor back from end */
        char pos[32];
        int plen = snprintf(pos, sizeof(pos), "\x1b[%dD", ed->len - ed->pos);
        terminal_buffer_append(&ab, pos, plen);
    }

    write(STDOUT_FILENO, ab.b, ab.len);
    terminal_buffer_free(&ab);
}

static char *repl_readline(editor_ctx_t *syntax_ctx, ReplLineEditor *ed, const char *prompt) {
    /* Reset editor state for new line */
    ed->buf[0] = '\0';
    ed->len = 0;
    ed->pos = 0;
    ed->history_idx = -1;

    /* Initial render */
    repl_render_line(syntax_ctx, ed, prompt);

    while (1) {
        fflush(stdout); /* Ensure output is flushed before blocking read */
        int c = terminal_read_key(STDIN_FILENO);

        if (c == ENTER) {
            /* Submit line */
            write(STDOUT_FILENO, "\r\n", 2);
            ed->buf[ed->len] = '\0';
            return ed->buf;
        }

        if (c == CTRL_C) {
            /* Cancel current line */
            ed->buf[0] = '\0';
            ed->len = 0;
            ed->pos = 0;
            write(STDOUT_FILENO, "^C\r\n", 4);
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == CTRL_D) {
            /* EOF on empty line */
            if (ed->len == 0) {
                write(STDOUT_FILENO, "\r\n", 2);
                return NULL;
            }
            /* Delete char at cursor */
            if (ed->pos < ed->len) {
                memmove(&ed->buf[ed->pos], &ed->buf[ed->pos + 1], ed->len - ed->pos);
                ed->len--;
            }
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == BACKSPACE || c == CTRL_H) {
            /* Delete char before cursor */
            if (ed->pos > 0) {
                memmove(&ed->buf[ed->pos - 1], &ed->buf[ed->pos], ed->len - ed->pos + 1);
                ed->pos--;
                ed->len--;
            }
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == DEL_KEY) {
            /* Delete char at cursor */
            if (ed->pos < ed->len) {
                memmove(&ed->buf[ed->pos], &ed->buf[ed->pos + 1], ed->len - ed->pos);
                ed->len--;
            }
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == ARROW_LEFT) {
            if (ed->pos > 0) ed->pos--;
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == ARROW_RIGHT) {
            if (ed->pos < ed->len) ed->pos++;
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == HOME_KEY || c == CTRL_A) {
            ed->pos = 0;
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == END_KEY || c == CTRL_E) {
            ed->pos = ed->len;
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == ARROW_UP) {
            /* Previous history */
            if (ed->history_len == 0) continue;

            if (ed->history_idx == -1) {
                /* Save current input */
                memcpy(ed->saved_buf, ed->buf, ed->len + 1);
                ed->saved_len = ed->len;
                ed->history_idx = ed->history_len - 1;
            } else if (ed->history_idx > 0) {
                ed->history_idx--;
            } else {
                continue;
            }

            /* Load history entry */
            strcpy(ed->buf, ed->history[ed->history_idx]);
            ed->len = strlen(ed->buf);
            ed->pos = ed->len;
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == ARROW_DOWN) {
            /* Next history */
            if (ed->history_idx == -1) continue;

            if (ed->history_idx < ed->history_len - 1) {
                ed->history_idx++;
                strcpy(ed->buf, ed->history[ed->history_idx]);
                ed->len = strlen(ed->buf);
                ed->pos = ed->len;
            } else {
                /* Restore saved input */
                ed->history_idx = -1;
                memcpy(ed->buf, ed->saved_buf, ed->saved_len + 1);
                ed->len = ed->saved_len;
                ed->pos = ed->len;
            }
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == CTRL_U) {
            /* Clear line */
            ed->buf[0] = '\0';
            ed->len = 0;
            ed->pos = 0;
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == CTRL_K) {
            /* Kill to end of line */
            ed->len = ed->pos;
            ed->buf[ed->len] = '\0';
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        /* Insert printable character */
        if (c >= 32 && c < 127 && ed->len < MAX_INPUT_LENGTH - 1) {
            memmove(&ed->buf[ed->pos + 1], &ed->buf[ed->pos], ed->len - ed->pos + 1);
            ed->buf[ed->pos] = c;
            ed->pos++;
            ed->len++;
            repl_render_line(syntax_ctx, ed, prompt);
        }
    }
}

#ifdef LANG_ALDA
/* ============================================================================
 * Alda Usage and Help
 * ============================================================================ */

static void print_repl_usage(const char *prog) {
    printf("Usage: %s [options] [file.alda]\n", prog);
    printf("\n");
    printf("Alda music language interpreter with MIDI output.\n");
    printf("If no file is provided, starts an interactive REPL.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help        Show this help message\n");
    printf("  -v, --verbose     Enable verbose output\n");
    printf("  -l, --list        List available MIDI ports\n");
    printf("  -p, --port N      Use MIDI port N (0-based index)\n");
    printf("  -o, --output NAME Use MIDI port matching NAME\n");
    printf("  --virtual NAME    Create virtual MIDI port with NAME\n");
    printf("  -s, --sequential  Use sequential playback mode\n");
    printf("\n");
    printf("Built-in Synth Options:\n");
    printf("  -sf, --soundfont PATH  Use built-in synth with soundfont (.sf2)\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s                      Start interactive REPL\n", prog);
    printf("  %s song.alda            Play an Alda file\n", prog);
    printf("  %s -sf gm.sf2           REPL with built-in synth\n", prog);
    printf("  %s -sf gm.sf2 song.alda Play with built-in synth\n", prog);
    printf("\n");
}

static void print_repl_help(void) {
    shared_print_command_help();

    printf("Alda-specific Commands:\n");
    printf("  :sequential       Wait for each input to complete\n");
    printf("  :concurrent       Enable polyphonic playback (default)\n");
    printf("\n");
    printf("Alda Syntax Examples:\n");
    printf("  piano:            Select piano instrument\n");
    printf("  c d e f g         Play notes C D E F G\n");
    printf("  c4 d8 e8 f4       Quarter, eighths, quarter\n");
    printf("  c/e/g             Play C major chord\n");
    printf("  (tempo 140)       Set tempo to 140 BPM\n");
    printf("  o5 c d e          Octave 5, then notes\n");
    printf("\n");
}

/* ============================================================================
 * REPL Loop
 * ============================================================================ */

/* Process an Alda REPL command. Returns: 0=continue, 1=quit, 2=interpret as Alda */
static int alda_process_command(AldaContext *ctx, const char *input) {
    /* Try shared commands first */
    int result = shared_process_command(ctx->shared, input, alda_async_stop);
    if (result == REPL_CMD_QUIT) {
        return 1; /* quit */
    }
    if (result == REPL_CMD_HANDLED) {
        /* Sync tsf_enabled flag from shared context */
        if (ctx->shared) {
            ctx->tsf_enabled = ctx->shared->tsf_enabled;
            ctx->csound_enabled = ctx->shared->csound_enabled;
        }
        return 0;
    }

    /* Handle Alda-specific commands */
    const char *cmd = input;
    if (cmd[0] == ':')
        cmd++;

    /* Help - add Alda-specific help */
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0 || strcmp(cmd, "?") == 0) {
        print_repl_help();
        return 0;
    }

    if (strcmp(cmd, "concurrent") == 0) {
        alda_async_set_concurrent(1);
        printf("Concurrent mode enabled (polyphony)\n");
        return 0;
    }

    if (strcmp(cmd, "sequential") == 0) {
        alda_async_set_concurrent(0);
        printf("Sequential mode enabled\n");
        return 0;
    }

    return 2; /* interpret as Alda */
}

/* Non-interactive Alda REPL loop for piped input */
static void alda_repl_loop_pipe(AldaContext *ctx) {
    char line[MAX_INPUT_LENGTH];

    while (fgets(line, sizeof(line), stdin) != NULL) {
        /* Strip trailing newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }

        if (len == 0) continue;

        int result = alda_process_command(ctx, line);
        if (result == 1) break;      /* quit */
        if (result == 0) continue;   /* command handled */

        /* Interpret as Alda */
        alda_events_clear(ctx);
        int parse_result = alda_interpret_string(ctx, line, "<pipe>");
        if (parse_result < 0) continue;

        if (ctx->event_count > 0) {
            if (ctx->verbose_mode) {
                printf("Playing %d events...\n", ctx->event_count);
            }
            alda_events_play_async(ctx);
        }
    }
}

static void repl_loop(AldaContext *ctx, editor_ctx_t *syntax_ctx) {
    ReplLineEditor ed;
    char *input;

    /* Use non-interactive mode for piped input */
    if (!isatty(STDIN_FILENO)) {
        alda_repl_loop_pipe(ctx);
        return;
    }

    repl_editor_init(&ed);

    printf("Alda REPL %s (type :h for help, :q to quit)\n", PSND_VERSION);
    if (!alda_async_get_concurrent()) {
        printf("Mode: sequential\n");
    }

    /* Enable raw mode for syntax-highlighted input */
    repl_enable_raw_mode();

    while (1) {
        input = repl_readline(syntax_ctx, &ed, "alda> ");

        if (input == NULL) {
            /* EOF - exit cleanly */
            break;
        }

        if (input[0] == '\0') {
            continue;
        }

        repl_add_history(&ed, input);

        /* Process command */
        int result = alda_process_command(ctx, input);
        if (result == 1) break;      /* quit */
        if (result == 0) continue;   /* command handled */

        /* Alda interpretation */
        alda_events_clear(ctx);

        int parse_result = alda_interpret_string(ctx, input, "<repl>");
        if (parse_result < 0) {
            continue;
        }

        if (ctx->event_count > 0) {
            if (ctx->verbose_mode) {
                printf("Playing %d events...\n", ctx->event_count);
            }
            alda_events_play_async(ctx);
        }
    }

    /* Disable raw mode before exit */
    repl_disable_raw_mode();
    repl_editor_cleanup(&ed);
}

/* ============================================================================
 * File Playback (headless)
 * ============================================================================ */

/* Helper function to check for .csd extension */
static int is_csd_file(const char *path) {
    if (!path) return 0;
    size_t len = strlen(path);
    if (len < 4) return 0;
    return strcmp(path + len - 4, ".csd") == 0;
}

int alda_play_main(int argc, char **argv) {
    int verbose = 0;
    const char *soundfont_path = NULL;
    const char *input_file = NULL;

    /* Simple argument parsing */
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
        fprintf(stderr, "Usage: psnd play [-v] [-sf soundfont.sf2] <file.alda|file.joy|file.csd>\n");
        return 1;
    }

    /* Handle .csd files with Csound backend */
    if (is_csd_file(input_file)) {
        if (soundfont_path) {
            fprintf(stderr, "Warning: -sf option ignored for .csd files\n");
        }
        int result = alda_csound_play_file(input_file, verbose);
        if (result != 0) {
            const char *err = alda_csound_get_error();
            fprintf(stderr, "Error: %s\n", err ? err : "Failed to play CSD file");
            return 1;
        }
        return 0;
    }

    /* Initialize */
    AldaContext ctx;
    alda_context_init(&ctx);
    ctx.verbose_mode = verbose;

    if (alda_tsf_init() != 0) {
        fprintf(stderr, "Warning: Failed to initialize built-in synth\n");
    }

    alda_midi_init_observer(&ctx);

    /* Setup output */
    if (soundfont_path) {
        if (alda_tsf_load_soundfont(soundfont_path) != 0) {
            fprintf(stderr, "Error: Failed to load soundfont: %s\n", soundfont_path);
            alda_tsf_cleanup();
            alda_midi_cleanup(&ctx);
            alda_context_cleanup(&ctx);
            return 1;
        }
        if (alda_tsf_enable() != 0) {
            fprintf(stderr, "Error: Failed to enable built-in synth\n");
            alda_tsf_cleanup();
            alda_midi_cleanup(&ctx);
            alda_context_cleanup(&ctx);
            return 1;
        }
        ctx.tsf_enabled = 1;
        if (verbose) {
            printf("Using built-in synth: %s\n", soundfont_path);
        }
    } else {
        if (alda_midi_open_auto(&ctx, "Alda") != 0) {
            fprintf(stderr, "Warning: No MIDI output available\n");
            fprintf(stderr, "Hint: Use -sf <soundfont.sf2> for built-in synth\n");
        }
    }

    /* Interpret file */
    if (verbose) {
        printf("Playing: %s\n", input_file);
    }

    int result = alda_interpret_file(&ctx, input_file);
    if (result < 0) {
        fprintf(stderr, "Error: Failed to interpret file\n");
        alda_tsf_cleanup();
        alda_midi_cleanup(&ctx);
        alda_context_cleanup(&ctx);
        return 1;
    }

    if (verbose) {
        printf("Scheduled %d events\n", ctx.event_count);
    }

    /* Play (blocking) */
    result = alda_events_play(&ctx);

    /* Cleanup */
    alda_async_cleanup();
    alda_tsf_cleanup();
    alda_midi_cleanup(&ctx);
    alda_context_cleanup(&ctx);

    return result < 0 ? 1 : 0;
}

/* ============================================================================
 * REPL Main Entry Point
 * ============================================================================ */

int alda_repl_main(int argc, char **argv) {
    int verbose = 0;
    int list_ports = 0;
    int port_index = -1;
    const char *port_name = NULL;
    const char *virtual_name = NULL;
    int sequential = 0;
    const char *input_file = NULL;
    const char *soundfont_path = NULL;

#ifdef _WIN32
    /* Simple argument parsing for Windows */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_repl_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "--list") == 0 || strcmp(argv[i], "-l") == 0) {
            list_ports = 1;
        } else if (strcmp(argv[i], "--sequential") == 0 || strcmp(argv[i], "-s") == 0) {
            sequential = 1;
        } else if ((strcmp(argv[i], "--port") == 0 || strcmp(argv[i], "-p") == 0) &&
                   i + 1 < argc) {
            port_index = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--output") == 0 || strcmp(argv[i], "-o") == 0) &&
                   i + 1 < argc) {
            port_name = argv[++i];
        } else if (strcmp(argv[i], "--virtual") == 0 && i + 1 < argc) {
            virtual_name = argv[++i];
        } else if ((strcmp(argv[i], "--soundfont") == 0 || strcmp(argv[i], "-sf") == 0) &&
                   i + 1 < argc) {
            soundfont_path = argv[++i];
        } else if (argv[i][0] != '-') {
            input_file = argv[i];
        }
    }
#else
    /* Pre-process -sf */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-sf") == 0 && i + 1 < argc) {
            soundfont_path = argv[i + 1];
            argv[i] = "";
            argv[i + 1] = "";
            i++;
        }
    }

    static struct option long_options[] = {{"help", no_argument, 0, 'h'},
                                           {"verbose", no_argument, 0, 'v'},
                                           {"list", no_argument, 0, 'l'},
                                           {"port", required_argument, 0, 'p'},
                                           {"output", required_argument, 0, 'o'},
                                           {"virtual", required_argument, 0, 'V'},
                                           {"sequential", no_argument, 0, 's'},
                                           {"soundfont", required_argument, 0, 'F'},
                                           {0, 0, 0, 0}};

    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "hvlsp:o:", long_options, &option_index)) != -1) {
        switch (opt) {
        case 'h':
            print_repl_usage(argv[0]);
            return 0;
        case 'v':
            verbose = 1;
            break;
        case 'l':
            list_ports = 1;
            break;
        case 'p':
            port_index = atoi(optarg);
            break;
        case 'o':
            port_name = optarg;
            break;
        case 'V':
            virtual_name = optarg;
            break;
        case 's':
            sequential = 1;
            break;
        case 'F':
            soundfont_path = optarg;
            break;
        default:
            print_repl_usage(argv[0]);
            return 1;
        }
    }

    /* Get input file */
    for (int i = optind; i < argc; i++) {
        if (argv[i][0] != '\0') {
            input_file = argv[i];
            break;
        }
    }
#endif

    /* Initialize context */
    AldaContext ctx;
    alda_context_init(&ctx);
    ctx.verbose_mode = verbose;

    if (alda_tsf_init() != 0) {
        fprintf(stderr, "Warning: Failed to initialize built-in synth\n");
    }

    alda_midi_init_observer(&ctx);

    /* Handle --list */
    if (list_ports) {
        alda_midi_list_ports(&ctx);
        alda_tsf_cleanup();
        alda_midi_cleanup(&ctx);
        alda_context_cleanup(&ctx);
        return 0;
    }

    /* Setup output */
    if (soundfont_path) {
        if (alda_tsf_load_soundfont(soundfont_path) != 0) {
            fprintf(stderr, "Error: Failed to load soundfont: %s\n", soundfont_path);
            alda_tsf_cleanup();
            alda_midi_cleanup(&ctx);
            alda_context_cleanup(&ctx);
            return 1;
        }
        if (alda_tsf_enable() != 0) {
            fprintf(stderr, "Error: Failed to enable built-in synth\n");
            alda_tsf_cleanup();
            alda_midi_cleanup(&ctx);
            alda_context_cleanup(&ctx);
            return 1;
        }
        ctx.tsf_enabled = 1;
        if (verbose) {
            printf("Using built-in synth: %s\n", soundfont_path);
        }
    } else {
        int midi_opened = 0;

        if (virtual_name) {
            if (alda_midi_open_virtual(&ctx, virtual_name) == 0) {
                midi_opened = 1;
                if (verbose) {
                    printf("Created virtual MIDI output: %s\n", virtual_name);
                }
            }
        } else if (port_name) {
            if (alda_midi_open_by_name(&ctx, port_name) == 0) {
                midi_opened = 1;
            }
        } else if (port_index >= 0) {
            if (alda_midi_open_port(&ctx, port_index) == 0) {
                midi_opened = 1;
            }
        } else {
            if (alda_midi_open_auto(&ctx, "Alda") == 0) {
                midi_opened = 1;
            }
        }

        if (!midi_opened) {
            fprintf(stderr, "Warning: No MIDI output available\n");
            fprintf(stderr, "Hint: Use -sf <soundfont.sf2> for built-in synth\n");
        }
    }

    /* Set playback mode */
    if (!sequential) {
        alda_async_set_concurrent(1);
    }
    if (verbose) {
        printf("Playback mode: %s\n", sequential ? "sequential" : "concurrent");
    }

    int result = 0;

    if (input_file) {
        /* File mode */
        if (verbose) {
            printf("Playing: %s\n", input_file);
        }

        result = alda_interpret_file(&ctx, input_file);
        if (result < 0) {
            fprintf(stderr, "Error: Failed to interpret file\n");
            alda_midi_cleanup(&ctx);
            alda_context_cleanup(&ctx);
            return 1;
        }

        if (verbose) {
            printf("Scheduled %d events\n", ctx.event_count);
        }

        result = alda_events_play(&ctx);
    } else {
        /* REPL mode - initialize syntax highlighting with theme support */
        editor_ctx_t syntax_ctx;
        editor_ctx_init(&syntax_ctx);
        syntax_init_default_colors(&syntax_ctx);
        syntax_select_for_filename(&syntax_ctx, "input.alda");

        /* Load Lua and themes for consistent highlighting */
        struct loki_lua_opts lua_opts = {
            .bind_editor = 1,
            .load_config = 1,
            .reporter = NULL
        };
        syntax_ctx.L = loki_lua_bootstrap(&syntax_ctx, &lua_opts);

        repl_loop(&ctx, &syntax_ctx);

        /* Cleanup Lua */
        if (syntax_ctx.L) {
            lua_close(syntax_ctx.L);
        }
    }

    /* Cleanup */
    alda_async_cleanup();
    alda_tsf_cleanup();
    alda_midi_cleanup(&ctx);
    alda_context_cleanup(&ctx);

    return result < 0 ? 1 : 0;
}
#endif /* LANG_ALDA */

#ifdef LANG_JOY
/* ============================================================================
 * Joy REPL
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

int joy_repl_main(int argc, char **argv) {
    int verbose = 0;
    int list_ports = 0;
    int port_index = -1;
    const char *virtual_name = NULL;
    const char *input_file = NULL;
    const char *soundfont_path = NULL;

    /* Simple argument parsing (skip argv[0] which is "joy") */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_joy_repl_usage("psnd");
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

    /* Initialize Joy context */
    JoyContext *ctx = joy_context_new();
    if (!ctx) {
        fprintf(stderr, "Error: Failed to create Joy context\n");
        return 1;
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

    /* Handle --list */
    if (list_ports) {
        joy_midi_list_ports();
        joy_midi_cleanup();
        music_notation_cleanup(ctx);
        joy_context_free(ctx);
        return 0;
    }

    /* Setup output */
    if (soundfont_path) {
        /* Use built-in synth */
        if (joy_tsf_load_soundfont(soundfont_path) != 0) {
            fprintf(stderr, "Error: Failed to load soundfont: %s\n", soundfont_path);
            joy_midi_cleanup();
            music_notation_cleanup(ctx);
            joy_context_free(ctx);
            return 1;
        }
        if (joy_tsf_enable() != 0) {
            fprintf(stderr, "Error: Failed to enable built-in synth\n");
            joy_midi_cleanup();
            music_notation_cleanup(ctx);
            joy_context_free(ctx);
            return 1;
        }
        if (verbose) {
            printf("Using built-in synth: %s\n", soundfont_path);
        }
    } else {
        /* Setup MIDI output */
        int midi_opened = 0;

        if (virtual_name) {
            if (joy_midi_open_virtual(virtual_name) == 0) {
                midi_opened = 1;
                if (verbose) {
                    printf("Created virtual MIDI output: %s\n", virtual_name);
                }
            }
        } else if (port_index >= 0) {
            if (joy_midi_open_port(port_index) == 0) {
                midi_opened = 1;
            }
        } else {
            /* Try to open a virtual port by default */
            if (joy_midi_open_virtual("JoyMIDI") == 0) {
                midi_opened = 1;
                if (verbose) {
                    printf("Created virtual MIDI output: JoyMIDI\n");
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
        /* File mode - execute Joy file */
        if (verbose) {
            printf("Executing: %s\n", input_file);
        }
        result = joy_load_file(ctx, input_file);
        if (result != 0) {
            fprintf(stderr, "Error: Failed to execute file\n");
        }
    } else {
        /* REPL mode - initialize syntax highlighting */
        editor_ctx_t syntax_ctx;
        editor_ctx_init(&syntax_ctx);
        syntax_init_default_colors(&syntax_ctx);
        syntax_select_for_filename(&syntax_ctx, "input.joy");

        /* Load Lua and themes for consistent highlighting */
        struct loki_lua_opts lua_opts = {
            .bind_editor = 1,
            .load_config = 1,
            .reporter = NULL
        };
        syntax_ctx.L = loki_lua_bootstrap(&syntax_ctx, &lua_opts);

        joy_repl_loop(ctx, &syntax_ctx);

        /* Cleanup Lua */
        if (syntax_ctx.L) {
            lua_close(syntax_ctx.L);
        }
    }

    /* Wait for audio buffer to drain before cleanup */
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

    return result;
}
#endif /* LANG_JOY */
