/**
 * @file main.c
 * @brief Unified entry point for psnd - editor, REPL, and playback.
 *
 * Dispatch modes:
 *   psnd              -> REPL mode (interactive Alda composition)
 *   psnd file.alda    -> Editor mode (live-coding editor)
 *   psnd play file    -> Play mode (headless playback)
 *   psnd repl         -> REPL mode (explicit)
 *   psnd joy          -> Joy REPL mode (concatenative music language)
 */

#include "loki/editor.h"
#include "version.h"
#include <stdio.h>
#include <string.h>

/* External entry points - conditionally compiled based on language support */
#ifdef LANG_ALDA
extern int alda_repl_main(int argc, char **argv);
extern int alda_play_main(int argc, char **argv);
#endif

#ifdef LANG_JOY
extern int joy_repl_main(int argc, char **argv);
#endif

static void print_unified_help(const char *prog) {
    printf("psnd %s - Music composition editor and REPL\n", PSND_VERSION);
    printf("\n");
    printf("Usage:\n");
    printf("  %s alda [options]      Start interactive Alda REPL\n", prog);
    printf("  %s joy [options]       Start interactive Joy REPL\n", prog);
    printf("  %s <file.alda>         Open Alda file in editor\n", prog);
    printf("  %s <file.joy>          Open Joy file in editor\n", prog);
    printf("  %s <file.csd>          Open Csound file in editor\n", prog);
    printf("  %s play <file>         Play file (headless, .alda/.joy/.csd)\n", prog);
    printf("\n");
    printf("Languages:\n");
    printf("  Alda - Music notation language\n");
    printf("  Joy  - Concatenative stack-based music language\n");
    printf("\n");
    printf("Editor Mode:\n");
    printf("  Opens a vim-like modal editor with live-coding support.\n");
    printf("  Ctrl-E: Play current part or selection\n");
    printf("  Ctrl-P: Play entire file\n");
    printf("  Ctrl-G: Stop playback\n");
    printf("\n");
    printf("REPL Options:\n");
    printf("  -l, --list             List available MIDI ports\n");
    printf("  -p, --port N           Use MIDI port N\n");
    printf("  -sf, --soundfont PATH  Use built-in synth with soundfont\n");
    printf("  --virtual NAME         Create virtual MIDI port\n");
    printf("  -v, --verbose          Enable verbose output\n");
    printf("\n");
    printf("Editor Options:\n");
    printf("  -sf PATH               Use built-in TinySoundFont synth\n");
    printf("  -cs PATH               Use Csound synthesis with .csd file\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s alda                Start Alda REPL: piano: c d e f g\n", prog);
    printf("  %s joy                 Start Joy REPL: [c d e] play\n", prog);
    printf("  %s alda -sf gm.sf2     Alda REPL with built-in synth\n", prog);
    printf("  %s song.alda           Edit song.alda\n", prog);
    printf("  %s play song.alda      Play song.alda and exit\n", prog);
    printf("\n");
}

#ifdef LANG_ALDA
static int has_alda_extension(const char *path) {
    if (!path)
        return 0;
    size_t len = strlen(path);
    if (len < 5)
        return 0;
    return strcmp(path + len - 5, ".alda") == 0;
}
#endif

static int has_csd_extension(const char *path) {
    if (!path)
        return 0;
    size_t len = strlen(path);
    if (len < 4)
        return 0;
    return strcmp(path + len - 4, ".csd") == 0;
}

#ifdef LANG_JOY
static int has_joy_extension(const char *path) {
    if (!path)
        return 0;
    size_t len = strlen(path);
    if (len < 4)
        return 0;
    return strcmp(path + len - 4, ".joy") == 0;
}
#endif

int main(int argc, char **argv) {
    /* No arguments -> Show help and exit */
    if (argc == 1) {
        print_unified_help(argv[0]);
        return 1;
    }

    const char *first_arg = argv[1];

    /* Help flags */
    if (strcmp(first_arg, "-h") == 0 || strcmp(first_arg, "--help") == 0) {
        print_unified_help(argv[0]);
        return 0;
    }

    /* Version flag */
    if (strcmp(first_arg, "-V") == 0 || strcmp(first_arg, "--version") == 0) {
        printf("psnd %s\n", PSND_VERSION);
        return 0;
    }

    /* Explicit subcommands */
#ifdef LANG_ALDA
    if (strcmp(first_arg, "alda") == 0) {
        /* Alda REPL mode: psnd alda [options] */
        return alda_repl_main(argc - 1, argv + 1);
    }
#endif

#ifdef LANG_JOY
    if (strcmp(first_arg, "joy") == 0) {
        /* Joy REPL mode: psnd joy [options] [file.joy] */
        return joy_repl_main(argc - 1, argv + 1);
    }
#endif

    if (strcmp(first_arg, "play") == 0) {
        /* Shift arguments past 'play': psnd play file.alda -> file.alda */
        if (argc < 3) {
            fprintf(stderr, "Usage: psnd play <file.alda|file.joy|file.csd>\n");
            return 1;
        }
#ifdef LANG_JOY
        /* Check for .joy file - route to Joy REPL with file argument */
        for (int i = 2; i < argc; i++) {
            if (has_joy_extension(argv[i])) {
                /* Create new argv for joy_repl_main: joy_repl_main(1, [file.joy]) */
                const char *joy_argv[2] = {"joy", argv[i]};
                return joy_repl_main(2, (char **)joy_argv);
            }
        }
#endif
#ifdef LANG_ALDA
        /* Not a .joy file - use Alda play */
        return alda_play_main(argc - 2, argv + 2);
#else
        fprintf(stderr, "Error: No playback support compiled in\n");
        return 1;
#endif
    }

    /* Check if first arg looks like a file (has .alda, .csd, or .joy extension) */
    if (
#ifdef LANG_ALDA
        has_alda_extension(first_arg) ||
#endif
        has_csd_extension(first_arg)
#ifdef LANG_JOY
        || has_joy_extension(first_arg)
#endif
    ) {
        /* Definitely a file -> editor mode */
        return loki_editor_main(argc, argv);
    }

    /* Check for editor options (-sf, -cs) followed by a file */
    if (strcmp(first_arg, "-sf") == 0 || strcmp(first_arg, "-cs") == 0) {
        /* Look for a supported file in remaining args */
        for (int i = 2; i < argc; i++) {
            if (
#ifdef LANG_ALDA
                has_alda_extension(argv[i]) ||
#endif
#ifdef LANG_JOY
                has_joy_extension(argv[i]) ||
#endif
                has_csd_extension(argv[i])) {
                /* Found supported file -> editor mode with options */
                return loki_editor_main(argc, argv);
            }
        }
        /* No supported file found - show help */
        fprintf(stderr, "Error: %s requires a .alda, .joy, or .csd file\n", first_arg);
        print_unified_help(argv[0]);
        return 1;
    }

    /* Default: assume it's a file, try editor */
    return loki_editor_main(argc, argv);
}
