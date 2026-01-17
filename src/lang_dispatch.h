/**
 * @file lang_dispatch.h
 * @brief Language dispatch system for main.c - decouples from specific languages.
 *
 * Each language registers its command names, file extensions, and entry points.
 * main.c uses this to dispatch to the appropriate language without #ifdef blocks.
 */

#ifndef PSND_LANG_DISPATCH_H
#define PSND_LANG_DISPATCH_H

#define LANG_DISPATCH_MAX_COMMANDS 4
#define LANG_DISPATCH_MAX_EXTENSIONS 4
#define LANG_DISPATCH_MAX_LANGS 8

/**
 * @brief Language dispatch entry.
 *
 * Each supported language registers one of these to enable command-line
 * dispatch without hardcoding language names in main.c.
 */
typedef struct {
    /* Command names that invoke this language's REPL (e.g., "alda", "joy") */
    const char *commands[LANG_DISPATCH_MAX_COMMANDS];
    int command_count;

    /* File extensions this language handles (e.g., ".alda", ".joy") */
    const char *extensions[LANG_DISPATCH_MAX_EXTENSIONS];
    int extension_count;

    /* Display name for help text */
    const char *display_name;

    /* Short description for help text */
    const char *description;

    /* REPL entry point: int repl_main(int argc, char **argv) */
    int (*repl_main)(int argc, char **argv);

    /* Play entry point (optional): int play_main(int argc, char **argv) */
    int (*play_main)(int argc, char **argv);
} LangDispatchEntry;

/**
 * @brief Register a language for dispatch.
 *
 * Called by lang_dispatch_init() for each compiled-in language.
 *
 * @param entry Pointer to static dispatch entry (must remain valid).
 * @return 0 on success, -1 on error (NULL entry or limit reached).
 *         Errors are logged to stderr.
 */
int lang_dispatch_register(const LangDispatchEntry *entry);

/**
 * @brief Find a language by command name.
 *
 * @param command Command string (e.g., "alda", "joy", "tr7").
 * @return Dispatch entry or NULL if not found.
 */
const LangDispatchEntry *lang_dispatch_find_by_command(const char *command);

/**
 * @brief Find a language by file extension.
 *
 * @param path File path to check extension of.
 * @return Dispatch entry or NULL if no language handles this extension.
 */
const LangDispatchEntry *lang_dispatch_find_by_extension(const char *path);

/**
 * @brief Check if a path has a supported file extension.
 *
 * @param path File path to check.
 * @return 1 if supported, 0 otherwise.
 */
int lang_dispatch_has_supported_extension(const char *path);

/**
 * @brief Get all registered languages.
 *
 * @param count Output: number of registered languages.
 * @return Array of dispatch entries.
 */
const LangDispatchEntry **lang_dispatch_get_all(int *count);

/**
 * @brief Print language help section.
 *
 * Prints registered languages and their descriptions for --help output.
 */
void lang_dispatch_print_help(void);

/**
 * @brief Initialize the language dispatch system.
 *
 * Registers all compiled-in languages. Must be called before any
 * dispatch operations (find_by_command, find_by_extension, etc.).
 *
 * This replaces the previous __attribute__((constructor)) approach
 * which is not portable to MSVC.
 */
void lang_dispatch_init(void);

/* Language-specific init functions (called by lang_dispatch_init) */
#ifdef LANG_ALDA
void alda_dispatch_init(void);
#endif
#ifdef LANG_JOY
void joy_dispatch_init(void);
#endif
#ifdef LANG_TR7
void tr7_dispatch_init(void);
#endif
#ifdef LANG_BOG
void bog_dispatch_init(void);
#endif

#endif /* PSND_LANG_DISPATCH_H */
