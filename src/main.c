/**
 * @file main.c
 * @brief Unified entry point for aldalog - editor, REPL, and playback.
 *
 * Dispatch modes:
 *   aldalog              -> REPL mode (interactive Alda composition)
 *   aldalog file.alda    -> Editor mode (live-coding editor)
 *   aldalog play file    -> Play mode (headless playback)
 *   aldalog repl         -> REPL mode (explicit)
 */

#include "loki/editor.h"
#include "loki/version.h"
#include <stdio.h>
#include <string.h>

/* External entry points */
extern int alda_repl_main(int argc, char **argv);
extern int alda_play_main(int argc, char **argv);

static void print_unified_help(const char *prog) {
    printf("aldalog %s - Music composition editor and REPL\n", LOKI_VERSION);
    printf("\n");
    printf("Usage:\n");
    printf("  %s                     Start interactive REPL\n", prog);
    printf("  %s <file.alda>         Open Alda file in editor\n", prog);
    printf("  %s <file.csd>          Open Csound file in editor\n", prog);
    printf("  %s play <file>         Play file (headless)\n", prog);
    printf("  %s repl [options]      Start REPL with options\n", prog);
    printf("\n");
    printf("Editor Mode:\n");
    printf("  Opens a vim-like modal editor with live-coding support.\n");
    printf("  Ctrl-E: Play current part or selection\n");
    printf("  Ctrl-P: Play entire file\n");
    printf("  Ctrl-G: Stop playback\n");
    printf("\n");
    printf("Editor Options (for .alda files):\n");
    printf("  -sf PATH               Use built-in TinySoundFont synth\n");
    printf("  -cs PATH               Use Csound synthesis with .csd file\n");
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
    printf("  %s song.csd            Edit song.csd (Csound)\n", prog);
    printf("  %s -cs inst.csd song.alda  Edit with Csound synthesis\n", prog);
    printf("  %s play song.alda      Play song.alda and exit\n", prog);
    printf("  %s play song.csd       Play song.csd and exit\n", prog);
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

static int has_csd_extension(const char *path) {
    if (!path)
        return 0;
    size_t len = strlen(path);
    if (len < 4)
        return 0;
    return strcmp(path + len - 4, ".csd") == 0;
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
        printf("aldalog %s\n", LOKI_VERSION);
        return 0;
    }

    /* Explicit subcommands */
    if (strcmp(first_arg, "repl") == 0) {
        /* Shift arguments past 'repl': aldalog repl -sf foo -> -sf foo */
        return alda_repl_main(argc - 2, argv + 2);
    }

    if (strcmp(first_arg, "play") == 0) {
        /* Shift arguments past 'play': aldalog play file.csd -> file.csd */
        return alda_play_main(argc - 2, argv + 2);
    }

    /* Check if first arg looks like a file (has .alda or .csd extension) */
    if (has_alda_extension(first_arg) || has_csd_extension(first_arg)) {
        /* Definitely a file -> editor mode */
        return loki_editor_main(argc, argv);
    }

    /* Check for editor options (-sf, -cs) followed by a .alda file */
    if (strcmp(first_arg, "-sf") == 0 || strcmp(first_arg, "-cs") == 0) {
        /* Look for a .alda file in remaining args */
        for (int i = 2; i < argc; i++) {
            if (has_alda_extension(argv[i])) {
                /* Found .alda file -> editor mode with options */
                return loki_editor_main(argc, argv);
            }
        }
        /* No .alda file found, treat as REPL (only -sf makes sense for REPL) */
        if (strcmp(first_arg, "-sf") == 0) {
            return alda_repl_main(argc, argv);
        }
        /* -cs without .alda file is an error */
        fprintf(stderr, "Error: -cs requires a .alda file\n");
        return 1;
    }

    /* If first arg starts with -, assume REPL options */
    if (first_arg[0] == '-') {
        return alda_repl_main(argc, argv);
    }

    /* Default: assume it's a file, try editor */
    return loki_editor_main(argc, argv);
}
