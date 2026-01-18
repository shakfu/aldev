/**
 * @file main.c
 * @brief Unified entry point for psnd - editor, REPL, and playback.
 *
 * Dispatch modes:
 *   psnd              -> Show help
 *   psnd <lang>       -> REPL mode for that language
 *   psnd file.ext     -> Editor mode (live-coding editor)
 *   psnd play file    -> Play mode (headless playback)
 *
 * Languages are registered via lang_dispatch_init() which is called at
 * startup. This explicit initialization replaces the previous
 * __attribute__((constructor)) approach for MSVC compatibility.
 */

#include "lang_dispatch.h"
#include "loki/editor.h"
#include "loki/cli.h"
#include "loki/host.h"
#include "loki/session.h"
#include "psnd.h"
#include <stdio.h>
#include <string.h>

#ifdef LOKI_WEB_HOST
#include "loki/host_web.h"
#endif

/* Check for .csd extension (Csound - always supported in editor) */
static int has_csd_extension(const char *path) {
    if (!path) return 0;
    size_t len = strlen(path);
    if (len < 4) return 0;
    return strcmp(path + len - 4, ".csd") == 0;
}

static void print_unified_help(const char *prog) {
    int lang_count;
    const LangDispatchEntry **langs = lang_dispatch_get_all(&lang_count);

    printf(PSND_NAME " %s - Music composition editor and REPL\n", PSND_VERSION);
    printf("\n");
    printf("Usage:\n");

    /* Print language-specific REPL commands */
    for (int i = 0; i < lang_count; i++) {
        printf("  %s %-6s [options]    Start interactive %s REPL\n",
               prog, langs[i]->commands[0], langs[i]->display_name);
    }

    /* Print file-based commands */
    printf("  %s <file>             Open file in editor\n", prog);
    printf("  %s play <file>        Play file (headless)\n", prog);
    printf("\n");

    printf("Languages:\n");
    lang_dispatch_print_help();
    printf("\n");

    printf("Supported file extensions:\n");
    for (int i = 0; i < lang_count; i++) {
        printf("  %s:", langs[i]->display_name);
        for (int j = 0; j < langs[i]->extension_count; j++) {
            printf(" %s", langs[i]->extensions[j]);
        }
        printf("\n");
    }
    printf("  Csound: .csd\n");
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
    if (lang_count > 0) {
        printf("  %s %s                Start %s REPL\n",
               prog, langs[0]->commands[0], langs[0]->display_name);
        printf("  %s %s -sf gm.sf2     %s REPL with built-in synth\n",
               prog, langs[0]->commands[0], langs[0]->display_name);
    }
    printf("  %s song.alda           Edit song.alda\n", prog);
    printf("  %s play song.alda      Play song.alda and exit\n", prog);
    printf("\n");
}

int main(int argc, char **argv) {
    /* Initialize language dispatch system */
    lang_dispatch_init();

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
        printf(PSND_NAME " %s\n", PSND_VERSION);
        return 0;
    }

    /* Check if first arg is a language command (e.g., "alda", "joy", "tr7") */
    const LangDispatchEntry *lang = lang_dispatch_find_by_command(first_arg);
    if (lang && lang->repl_main) {
        return lang->repl_main(argc - 1, argv + 1);
    }

    /* Handle "play" subcommand */
    if (strcmp(first_arg, "play") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: " PSND_NAME " play <file>\n");
            return 1;
        }

        /* Find a file argument and its language */
        for (int i = 2; i < argc; i++) {
            const LangDispatchEntry *file_lang = lang_dispatch_find_by_extension(argv[i]);
            if (file_lang && file_lang->play_main) {
                /* Route to language's play function */
                /* Create argv starting from the file */
                return file_lang->play_main(argc - i, argv + i);
            }
        }

        /* No language matched - try first language with play_main as fallback */
        int lang_count;
        const LangDispatchEntry **langs = lang_dispatch_get_all(&lang_count);
        for (int i = 0; i < lang_count; i++) {
            if (langs[i]->play_main) {
                return langs[i]->play_main(argc - 2, argv + 2);
            }
        }

        fprintf(stderr, "Error: No playback support for this file type\n");
        return 1;
    }

    /* Handle --web flag for web server mode */
#ifdef LOKI_WEB_HOST
    if (strcmp(first_arg, "--web") == 0) {
        EditorCliArgs args = {0};
        if (editor_cli_parse(argc, argv, &args) != 0) {
            return 1;
        }
        if (args.show_help) {
            editor_cli_print_usage();
            return 0;
        }
        if (args.show_version) {
            editor_cli_print_version();
            return 0;
        }

        EditorConfig config = {
            .rows = args.rows > 0 ? args.rows : 24,
            .cols = args.cols > 0 ? args.cols : 80,
            .filename = args.filename,
            .line_numbers = args.line_numbers,
            .word_wrap = args.word_wrap,
            .enable_lua = 1
        };

        int port = args.web_port > 0 ? args.web_port : 8080;
        return editor_host_web_run(port, args.web_root, &config);
    }
#endif

    /* Check if first arg looks like a supported file */
    if (lang_dispatch_has_supported_extension(first_arg) || has_csd_extension(first_arg)) {
        return loki_editor_main(argc, argv);
    }

    /* Check for editor options (-sf, -cs) followed by a file */
    if (strcmp(first_arg, "-sf") == 0 || strcmp(first_arg, "-cs") == 0) {
        for (int i = 2; i < argc; i++) {
            if (lang_dispatch_has_supported_extension(argv[i]) || has_csd_extension(argv[i])) {
                return loki_editor_main(argc, argv);
            }
        }
        fprintf(stderr, "Error: %s requires a supported file\n", first_arg);
        print_unified_help(argv[0]);
        return 1;
    }

    /* Default: assume it's a file, try editor */
    return loki_editor_main(argc, argv);
}
