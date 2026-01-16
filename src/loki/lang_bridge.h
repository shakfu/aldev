/* lang_bridge.h - Language-agnostic bridge for music languages
 *
 * Provides a common interface for music languages (Alda, Joy, etc.) to
 * integrate with the Loki editor without coupling core editor code to
 * specific language implementations.
 *
 * Languages register themselves at startup, and core editor code dispatches
 * through this interface based on file extension or explicit language name.
 */

#ifndef LOKI_LANG_BRIDGE_H
#define LOKI_LANG_BRIDGE_H

/* Forward declarations */
#ifndef LOKI_CORE_H
struct editor_ctx;
typedef struct editor_ctx editor_ctx_t;
#endif

#ifndef lua_h
struct lua_State;
typedef struct lua_State lua_State;
#endif

/* Maximum number of registered languages */
#define LOKI_MAX_LANGUAGES 8

/* Maximum file extensions per language */
#define LOKI_MAX_EXTENSIONS 4

/* ======================= Language Operations ======================= */

/**
 * Language operations interface.
 * Each language implements this struct to integrate with Loki.
 * NULL function pointers indicate unsupported operations.
 */
typedef struct LokiLangOps {
    /* Language identification */
    const char *name;                              /* e.g., "alda", "joy" */
    const char *extensions[LOKI_MAX_EXTENSIONS];   /* e.g., {".alda", NULL} */

    /* Lifecycle */
    int (*init)(editor_ctx_t *ctx);                /* Initialize language */
    void (*cleanup)(editor_ctx_t *ctx);            /* Cleanup resources */
    int (*is_initialized)(editor_ctx_t *ctx);      /* Check if initialized */

    /* Main loop integration (optional) */
    void (*check_callbacks)(editor_ctx_t *ctx, lua_State *L);

    /* Playback */
    int (*eval)(editor_ctx_t *ctx, const char *code);   /* Evaluate/play code */
    void (*stop)(editor_ctx_t *ctx);                    /* Stop playback */
    int (*is_playing)(editor_ctx_t *ctx);               /* Check if playing (optional) */

    /* Export support (optional) */
    int (*has_events)(editor_ctx_t *ctx);               /* Has exportable events */
    int (*populate_shared_buffer)(editor_ctx_t *ctx);   /* Fill shared MIDI buffer */

    /* Error handling */
    const char *(*get_error)(editor_ctx_t *ctx);        /* Get last error */

    /* Backend configuration (optional) */
    int (*configure_backend)(editor_ctx_t *ctx, const char *sf_path, const char *csd_path);

    /* Lua API registration (optional) */
    void (*register_lua_api)(lua_State *L);              /* Register language's Lua bindings */
} LokiLangOps;

/* ======================= Registration ======================= */

/**
 * Register a language with the bridge.
 * Called at startup by each language module.
 *
 * @param ops Language operations (must have static lifetime)
 * @return 0 on success, -1 if max languages reached
 */
int loki_lang_register(const LokiLangOps *ops);

/* ======================= Dispatch ======================= */

/**
 * Get language operations by file extension.
 *
 * @param filename Filename to check extension
 * @return Language ops or NULL if no match
 */
const LokiLangOps *loki_lang_for_file(const char *filename);

/**
 * Get language operations by name.
 *
 * @param name Language name (e.g., "alda", "joy")
 * @return Language ops or NULL if not found
 */
const LokiLangOps *loki_lang_by_name(const char *name);

/**
 * Get all registered languages.
 *
 * @param count Output: number of languages
 * @return Array of language ops
 */
const LokiLangOps **loki_lang_all(int *count);

/* ======================= Convenience Functions ======================= */

/**
 * Initialize language for current file.
 * Dispatches to appropriate language based on ctx->filename.
 *
 * @param ctx Editor context
 * @return 0 on success, -1 on error, 1 if no language for file
 */
int loki_lang_init_for_file(editor_ctx_t *ctx);

/**
 * Cleanup all initialized languages.
 *
 * @param ctx Editor context
 */
void loki_lang_cleanup_all(editor_ctx_t *ctx);

/**
 * Check callbacks for all languages (main loop).
 *
 * @param ctx Editor context
 * @param L Lua state
 */
void loki_lang_check_callbacks(editor_ctx_t *ctx, lua_State *L);

/**
 * Evaluate code with language for current file.
 *
 * @param ctx Editor context
 * @param code Code to evaluate
 * @return 0 on success, -1 on error, 1 if no language
 */
int loki_lang_eval(editor_ctx_t *ctx, const char *code);

/**
 * Stop playback for all languages.
 *
 * @param ctx Editor context
 */
void loki_lang_stop_all(editor_ctx_t *ctx);

/**
 * Check if any language is playing.
 *
 * @param ctx Editor context
 * @return 1 if playing, 0 otherwise
 */
int loki_lang_is_playing(editor_ctx_t *ctx);

/**
 * Check if any language has exportable events.
 *
 * @param ctx Editor context
 * @return 1 if events available, 0 otherwise
 */
int loki_lang_has_events(editor_ctx_t *ctx);

/**
 * Populate shared buffer from first language with events.
 *
 * @param ctx Editor context
 * @return 0 on success, -1 if no events
 */
int loki_lang_populate_shared_buffer(editor_ctx_t *ctx);

/**
 * Get error from language for current file.
 *
 * @param ctx Editor context
 * @return Error string or NULL
 */
const char *loki_lang_get_error(editor_ctx_t *ctx);

/**
 * Configure audio backend for language.
 * Dispatches to appropriate language based on ctx->filename.
 *
 * @param ctx Editor context
 * @param sf_path Soundfont path (NULL if not specified)
 * @param csd_path Csound CSD path (NULL if not specified)
 * @return 0 on success, -1 on error, 1 if no language or not supported
 */
int loki_lang_configure_backend(editor_ctx_t *ctx, const char *sf_path, const char *csd_path);

/**
 * Register Lua APIs for all languages.
 * Iterates through registered languages and calls their register_lua_api.
 *
 * @param L Lua state
 */
void loki_lang_register_lua_apis(lua_State *L);

#endif /* LOKI_LANG_BRIDGE_H */
