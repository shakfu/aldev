/**
 * @file repl_launcher.h
 * @brief Shared REPL launcher for music language modules.
 *
 * Provides a common entry point for language REPLs that handles:
 * - CLI argument parsing (--help, -l, -p, --virtual, -sf, -v)
 * - Syntax highlighting setup for REPL mode
 * - Common flow control (file mode vs REPL mode)
 *
 * Languages provide callbacks for their specific initialization,
 * MIDI/audio setup, file execution, and REPL loop implementations.
 */

#ifndef SHARED_REPL_LAUNCHER_H
#define SHARED_REPL_LAUNCHER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
struct editor_ctx;
typedef struct editor_ctx editor_ctx_t;

/**
 * @brief Parsed CLI arguments passed to language callbacks.
 */
typedef struct SharedReplArgs {
    int verbose;              /**< -v, --verbose flag */
    int port_index;           /**< -p, --port N (-1 if not specified) */
    const char *virtual_name; /**< --virtual NAME (NULL if not specified) */
    const char *soundfont_path; /**< -sf, --soundfont PATH (NULL if not specified) */
} SharedReplArgs;

/**
 * @brief Language-specific callbacks for the shared REPL launcher.
 *
 * Languages implement these callbacks to integrate with the shared launcher.
 * The launcher handles CLI parsing and common flow control.
 */
typedef struct SharedReplCallbacks {
    /** Language name for messages (e.g., "joy", "tr7") */
    const char *name;

    /** File extension for syntax highlighting (e.g., ".joy", ".scm") */
    const char *file_ext;

    /** Program name for usage messages (e.g., "psnd"). If NULL, defaults to "psnd". */
    const char *prog_name;

    /**
     * Print language-specific usage/help.
     * @param prog Program name from argv[0]
     */
    void (*print_usage)(const char *prog);

    /**
     * List available MIDI ports.
     * Called for -l, --list option.
     */
    void (*list_ports)(void);

    /**
     * Initialize language context and MIDI/audio.
     * Language is responsible for setting up its own MIDI backend
     * using the provided arguments (soundfont, port, virtual).
     *
     * @param args Parsed CLI arguments
     * @return Opaque language context, or NULL on error
     */
    void *(*init)(const SharedReplArgs *args);

    /**
     * Cleanup language context and MIDI/audio.
     *
     * @param lang_ctx Language context returned by init()
     */
    void (*cleanup)(void *lang_ctx);

    /**
     * Execute a source file.
     *
     * @param lang_ctx Language context
     * @param path File path to execute
     * @param verbose Whether verbose mode is enabled
     * @return 0 on success, non-zero on error
     */
    int (*exec_file)(void *lang_ctx, const char *path, int verbose);

    /**
     * Run the interactive REPL loop.
     * Optional - if NULL, only file mode is supported.
     *
     * @param lang_ctx Language context
     * @param syntax_ctx Editor context for syntax highlighting
     */
    void (*repl_loop)(void *lang_ctx, editor_ctx_t *syntax_ctx);

} SharedReplCallbacks;

/**
 * @brief Shared REPL main entry point.
 *
 * Handles CLI parsing and dispatches to language callbacks.
 *
 * Supported CLI options:
 *   -h, --help         Print usage and exit
 *   -v, --verbose      Enable verbose output
 *   -l, --list         List MIDI ports and exit
 *   -p, --port N       Use MIDI port N
 *   --virtual NAME     Create virtual MIDI port
 *   -sf, --soundfont   Load soundfont for built-in synth
 *   <file>             Execute file instead of REPL
 *
 * @param callbacks Language-specific callbacks (must not be NULL)
 * @param argc Argument count (from main)
 * @param argv Argument vector (from main, argv[0] is language name)
 * @return Exit code (0 on success)
 */
int shared_lang_repl_main(const SharedReplCallbacks *callbacks, int argc, char **argv);

/**
 * @brief Shared play main entry point.
 *
 * Simplified launcher for headless file execution (psnd play <file>).
 * Handles -v and -sf options.
 *
 * @param callbacks Language-specific callbacks
 * @param argc Argument count (argv[0] may be the filename)
 * @param argv Argument vector
 * @return Exit code (0 on success)
 */
int shared_lang_play_main(const SharedReplCallbacks *callbacks, int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* SHARED_REPL_LAUNCHER_H */
