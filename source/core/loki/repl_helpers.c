/**
 * @file repl_helpers.c
 * @brief Implementation of shared REPL helper utilities.
 */

#include "repl_helpers.h"
#include "psnd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>

/* Maximum input line length for piped input */
#ifndef MAX_INPUT_LENGTH
#define MAX_INPUT_LENGTH 4096
#endif

/* ============================================================================
 * String Utilities
 * ============================================================================ */

int repl_starts_with(const char *str, const char *prefix) {
    if (!str || !prefix) return 0;
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

size_t repl_strip_newlines(char *line) {
    if (!line) return 0;

    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[--len] = '\0';
    }
    return len;
}

const char *repl_skip_whitespace(const char *str) {
    if (!str) return str;
    while (*str && isspace((unsigned char)*str)) {
        str++;
    }
    return str;
}

char *repl_trim_trailing(char *str) {
    if (!str) return str;

    size_t len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        str[--len] = '\0';
    }
    return str;
}

/* ============================================================================
 * History File Management
 * ============================================================================ */

int repl_get_history_path(const char *lang_name, char *buf, size_t buf_size) {
    if (!lang_name || !buf || buf_size == 0) {
        if (buf && buf_size > 0) buf[0] = '\0';
        return 0;
    }

    struct stat st;

    /* Check for local .psnd/ directory first */
    if (stat(PSND_CONFIG_DIR, &st) == 0 && S_ISDIR(st.st_mode)) {
        snprintf(buf, buf_size, "%s/%s_history", PSND_CONFIG_DIR, lang_name);
        return 1;
    }

    /* Fall back to ~/.psnd/ if it exists */
    const char *home = getenv("HOME");
    if (home) {
        char global_psnd[512];
        snprintf(global_psnd, sizeof(global_psnd), "%s/%s", home, PSND_CONFIG_DIR);
        if (stat(global_psnd, &st) == 0 && S_ISDIR(st.st_mode)) {
            snprintf(buf, buf_size, "%s/%s_history", global_psnd, lang_name);
            return 1;
        }
    }

    /* No history location available */
    buf[0] = '\0';
    return 0;
}

/* ============================================================================
 * REPL Loop Helpers
 * ============================================================================ */

void repl_pipe_loop(int (*process_fn)(void *ctx, const char *line),
                    void (*eval_fn)(void *ctx, const char *line),
                    void *ctx) {
    if (!process_fn) return;

    char line[MAX_INPUT_LENGTH];

    while (fgets(line, sizeof(line), stdin) != NULL) {
        /* Strip trailing newline */
        size_t len = repl_strip_newlines(line);

        /* Skip empty lines */
        if (len == 0) continue;

        /* Process command */
        int result = process_fn(ctx, line);

        if (result == 1) {
            /* Quit */
            break;
        }

        if (result == 0) {
            /* Command handled */
            continue;
        }

        /* result == 2: evaluate as code */
        if (eval_fn) {
            eval_fn(ctx, line);
        }

        fflush(stdout);
    }
}
