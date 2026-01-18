/**
 * @file repl_helpers.h
 * @brief Shared REPL helper utilities for language modules.
 *
 * Provides common utility functions used across language REPLs to reduce
 * code duplication. These are low-level helpers that don't require
 * restructuring existing REPL implementations.
 */

#ifndef LOKI_REPL_HELPERS_H
#define LOKI_REPL_HELPERS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * String Utilities
 * ============================================================================ */

/**
 * @brief Check if string starts with prefix.
 *
 * @param str The string to check
 * @param prefix The prefix to look for
 * @return 1 if str starts with prefix, 0 otherwise
 */
int repl_starts_with(const char *str, const char *prefix);

/**
 * @brief Strip trailing newline/carriage return characters in place.
 *
 * @param line String to modify (must be writable)
 * @return New length of string after stripping
 */
size_t repl_strip_newlines(char *line);

/**
 * @brief Skip leading whitespace in a string.
 *
 * @param str String to scan
 * @return Pointer to first non-whitespace character, or end of string
 */
const char *repl_skip_whitespace(const char *str);

/**
 * @brief Trim trailing whitespace from a string in place.
 *
 * @param str String to modify (must be writable)
 * @return The input string (for chaining)
 */
char *repl_trim_trailing(char *str);

/* ============================================================================
 * History File Management
 * ============================================================================ */

/**
 * @brief Get the history file path for a language.
 *
 * Determines the appropriate history file location:
 * 1. If local .psnd/ directory exists, use .psnd/<lang>_history
 * 2. Else if ~/.psnd/ exists, use ~/.psnd/<lang>_history
 * 3. Otherwise, buf is set to empty string (no history)
 *
 * @param lang_name Language name (e.g., "joy", "tr7", "alda", "bog")
 * @param buf Output buffer for path
 * @param buf_size Size of output buffer
 * @return 1 if a valid path was found, 0 if no history location available
 */
int repl_get_history_path(const char *lang_name, char *buf, size_t buf_size);

/* ============================================================================
 * REPL Loop Helpers
 * ============================================================================ */

/**
 * @brief Read lines from piped input and process them.
 *
 * Common pattern for non-interactive (piped) REPL input handling.
 * Reads lines from stdin, strips newlines, and calls the process callback.
 *
 * @param process_fn Callback to process each line. Returns:
 *                   0 = continue (command handled)
 *                   1 = quit (exit loop)
 *                   2 = evaluate (line should be evaluated as code)
 * @param eval_fn Callback to evaluate code (called when process_fn returns 2)
 * @param ctx User context passed to callbacks
 */
void repl_pipe_loop(int (*process_fn)(void *ctx, const char *line),
                    void (*eval_fn)(void *ctx, const char *line),
                    void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* LOKI_REPL_HELPERS_H */
