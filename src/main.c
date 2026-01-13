/**
 * @file main.c
 * @brief Unified entry point for aldev - editor, REPL, and playback.
 *
 * Dispatch modes:
 *   aldev              -> REPL mode (interactive Alda composition)
 *   aldev file.alda    -> Editor mode (live-coding editor)
 *   aldev play file    -> Play mode (headless playback)
 *   aldev repl         -> REPL mode (explicit)
 */

#include "loki/editor.h"
#include "loki/version.h"
#include <stdio.h>
#include <string.h>

/* External entry points */
extern int alda_repl_main(int argc, char **argv);
extern int alda_play_main(int argc, char **argv);

static void print_unified_help(const char *prog) {
    printf("aldev %s - Music composition editor and REPL\n", LOKI_VERSION);
    printf("\n");
    printf("Usage:\n");
    printf("  %s                     Start interactive REPL\n", prog);
    printf("  %s <file.alda>         Open file in editor\n", prog);
    printf("  %s play <file.alda>    Play file (headless)\n", prog);
    printf("  %s repl [options]      Start REPL with options\n", prog);
    printf("\n");
    printf("Editor Mode:\n");
    printf("  Opens a vim-like modal editor with live-coding support.\n");
    printf("  Ctrl-E: Play current part or selection\n");
    printf("  Ctrl-P: Play entire file\n");
    printf("  Ctrl-G: Stop playback\n");
    printf("\n");
    printf("REPL Mode:\n");
    printf("  Interactive composition - type Alda notation directly.\n");
    printf("  Type 'help' in REPL for commands.\n");
    printf("\n");
    printf("REPL Options:\n");
    printf("  -l, --list             List available MIDI ports\n");
    printf("  -p, --port N           Use MIDI port N\n");
    printf("  -sf, --soundfont PATH  Use built-in synth with soundfont\n");
    printf("  -v, --verbose          Enable verbose output\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s                     Start REPL, type: piano: c d e f g\n", prog);
    printf("  %s -sf gm.sf2          REPL with built-in synth\n", prog);
    printf("  %s song.alda           Edit song.alda\n", prog);
    printf("  %s play song.alda      Play song.alda and exit\n", prog);
    printf("\n");
}

static int has_alda_extension(const char *path) {
    if (!path)
        return 0;
    size_t len = strlen(path);
    if (len < 5)
        return 0;
    return strcmp(path + len - 5, ".alda") == 0;
}

int main(int argc, char **argv) {
    /* No arguments -> REPL mode */
    if (argc == 1) {
        return alda_repl_main(argc, argv);
    }

    const char *first_arg = argv[1];

    /* Help flags */
    if (strcmp(first_arg, "-h") == 0 || strcmp(first_arg, "--help") == 0) {
        print_unified_help(argv[0]);
        return 0;
    }

    /* Version flag */
    if (strcmp(first_arg, "-V") == 0 || strcmp(first_arg, "--version") == 0) {
        printf("aldev %s\n", LOKI_VERSION);
        return 0;
    }

    /* Explicit subcommands */
    if (strcmp(first_arg, "repl") == 0) {
        /* Shift arguments: aldev repl -sf foo -> aldev -sf foo */
        return alda_repl_main(argc - 1, argv + 1);
    }

    if (strcmp(first_arg, "play") == 0) {
        return alda_play_main(argc - 1, argv + 1);
    }

    /* Check if first arg looks like a file (has .alda extension or doesn't start with -) */
    if (has_alda_extension(first_arg)) {
        /* Definitely a file -> editor mode */
        return loki_editor_main(argc, argv);
    }

    /* If first arg starts with -, assume REPL options */
    if (first_arg[0] == '-') {
        return alda_repl_main(argc, argv);
    }

    /* Default: assume it's a file, try editor */
    return loki_editor_main(argc, argv);
}
