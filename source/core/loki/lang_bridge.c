/* lang_bridge.c - Language-agnostic bridge implementation
 *
 * Manages language registration and provides dispatch functions
 * for core editor code to interact with languages without direct coupling.
 */

#include "lang_bridge.h"
#include "internal.h"  /* For editor_ctx_t full definition */
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>

/* ======================= Internal State ======================= */

static const LokiLangOps *g_languages[LOKI_MAX_LANGUAGES];
static int g_language_count = 0;

/* ======================= Registration ======================= */

int loki_lang_register(const LokiLangOps *ops) {
    if (!ops || !ops->name) {
        fprintf(stderr, "loki_lang: attempted to register NULL or unnamed language\n");
        return -1;
    }
    if (g_language_count >= LOKI_MAX_LANGUAGES) {
        fprintf(stderr, "loki_lang: cannot register '%s' - limit of %d languages reached\n",
                ops->name, LOKI_MAX_LANGUAGES);
        return -1;
    }

    /* Check for duplicate registration */
    for (int i = 0; i < g_language_count; i++) {
        if (strcmp(g_languages[i]->name, ops->name) == 0) {
            return 0;  /* Already registered */
        }
    }

    g_languages[g_language_count++] = ops;
    return 0;
}

/* ======================= Dispatch ======================= */

static const char *get_extension(const char *filename) {
    if (!filename) return NULL;
    const char *dot = strrchr(filename, '.');
    return dot;
}

const LokiLangOps *loki_lang_for_file(const char *filename) {
    const char *ext = get_extension(filename);
    if (!ext) return NULL;

    for (int i = 0; i < g_language_count; i++) {
        const LokiLangOps *ops = g_languages[i];
        for (int j = 0; j < LOKI_MAX_EXTENSIONS && ops->extensions[j]; j++) {
            if (strcmp(ext, ops->extensions[j]) == 0) {
                return ops;
            }
        }
    }
    return NULL;
}

const LokiLangOps *loki_lang_by_name(const char *name) {
    if (!name) return NULL;

    for (int i = 0; i < g_language_count; i++) {
        if (strcmp(g_languages[i]->name, name) == 0) {
            return g_languages[i];
        }
    }
    return NULL;
}

const LokiLangOps **loki_lang_all(int *count) {
    if (count) *count = g_language_count;
    return (const LokiLangOps **)g_languages;
}

/* ======================= Convenience Functions ======================= */

int loki_lang_init_for_file(editor_ctx_t *ctx) {
    if (!ctx) return -1;

    const LokiLangOps *ops = loki_lang_for_file(ctx->model.filename);
    if (!ops) return -1;  /* No language for this file type */

    if (ops->is_initialized && ops->is_initialized(ctx)) {
        return 0;  /* Already initialized */
    }

    if (ops->init) {
        return ops->init(ctx);
    }
    return 0;
}

void loki_lang_cleanup_all(editor_ctx_t *ctx) {
    if (!ctx) return;

    for (int i = 0; i < g_language_count; i++) {
        const LokiLangOps *ops = g_languages[i];
        if (ops->cleanup) {
            ops->cleanup(ctx);
        }
    }
}

void loki_lang_check_callbacks(editor_ctx_t *ctx, lua_State *L) {
    if (!ctx) return;

    for (int i = 0; i < g_language_count; i++) {
        const LokiLangOps *ops = g_languages[i];
        if (ops->check_callbacks) {
            ops->check_callbacks(ctx, L);
        }
    }
}

int loki_lang_eval(editor_ctx_t *ctx, const char *code) {
    if (!ctx || !code) return -1;

    const LokiLangOps *ops = loki_lang_for_file(ctx->model.filename);
    if (!ops) return -1;  /* No language for this file type */

    /* Ensure initialized */
    if (ops->is_initialized && !ops->is_initialized(ctx)) {
        if (ops->init && ops->init(ctx) != 0) {
            return -1;
        }
    }

    if (ops->eval) {
        return ops->eval(ctx, code);
    }
    return -1;
}

int loki_lang_eval_buffer(editor_ctx_t *ctx) {
    if (!ctx || !ctx->model.filename) return -1;

    const LokiLangOps *ops = loki_lang_for_file(ctx->model.filename);
    if (!ops || !ops->eval) return -1;

    /* Ensure initialized */
    if (ops->is_initialized && !ops->is_initialized(ctx)) {
        if (ops->init && ops->init(ctx) != 0) {
            return -1;
        }
    }

    /* Build buffer content string */
    if (ctx->model.numrows == 0) return 0;

    size_t total = 0;
    for (int i = 0; i < ctx->model.numrows; i++) {
        total += ctx->model.row[i].size + 1;  /* +1 for newline */
    }

    char *code = malloc(total + 1);
    if (!code) return -1;

    char *p = code;
    for (int i = 0; i < ctx->model.numrows; i++) {
        memcpy(p, ctx->model.row[i].chars, ctx->model.row[i].size);
        p += ctx->model.row[i].size;
        *p++ = '\n';
    }
    *p = '\0';

    int ret = ops->eval(ctx, code);
    free(code);
    return ret;
}

void loki_lang_stop_all(editor_ctx_t *ctx) {
    if (!ctx) return;

    for (int i = 0; i < g_language_count; i++) {
        const LokiLangOps *ops = g_languages[i];
        if (ops->stop && ops->is_initialized && ops->is_initialized(ctx)) {
            ops->stop(ctx);
        }
    }
}

int loki_lang_is_playing(editor_ctx_t *ctx) {
    if (!ctx) return 0;

    for (int i = 0; i < g_language_count; i++) {
        const LokiLangOps *ops = g_languages[i];
        if (ops->is_playing && ops->is_playing(ctx)) {
            return 1;
        }
    }
    return 0;
}

int loki_lang_has_events(editor_ctx_t *ctx) {
    if (!ctx) return 0;

    for (int i = 0; i < g_language_count; i++) {
        const LokiLangOps *ops = g_languages[i];
        if (ops->has_events && ops->has_events(ctx)) {
            return 1;
        }
    }
    return 0;
}

int loki_lang_populate_shared_buffer(editor_ctx_t *ctx) {
    if (!ctx) return -1;

    for (int i = 0; i < g_language_count; i++) {
        const LokiLangOps *ops = g_languages[i];
        if (ops->has_events && ops->has_events(ctx)) {
            if (ops->populate_shared_buffer) {
                return ops->populate_shared_buffer(ctx);
            }
        }
    }
    return -1;
}

const char *loki_lang_get_error(editor_ctx_t *ctx) {
    if (!ctx) return NULL;

    const LokiLangOps *ops = loki_lang_for_file(ctx->model.filename);
    if (ops && ops->get_error) {
        return ops->get_error(ctx);
    }
    return NULL;
}

int loki_lang_configure_backend(editor_ctx_t *ctx, const char *sf_path, const char *csd_path) {
    if (!ctx) return -1;

    const LokiLangOps *ops = loki_lang_for_file(ctx->model.filename);
    if (!ops) return -1;  /* No language for this file type */

    if (!ops->configure_backend) return -1;  /* Language doesn't support backend config */

    return ops->configure_backend(ctx, sf_path, csd_path);
}

void loki_lang_register_lua_apis(lua_State *L) {
    if (!L) return;

    for (int i = 0; i < g_language_count; i++) {
        const LokiLangOps *ops = g_languages[i];
        if (ops->register_lua_api) {
            ops->register_lua_api(L);
        }
    }
}

void loki_lang_init(void) {
    /* Register all compiled-in languages.
     * Language init functions are declared in src/lang_config.h */
    LOKI_LANG_INIT_ALL()
}
