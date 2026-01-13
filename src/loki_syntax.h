/* loki_syntax.h - Syntax highlighting module
 *
 * This module handles all syntax highlighting functionality including:
 * - Token classification (keywords, strings, comments, numbers)
 * - Multi-line comment state tracking
 * - Color formatting for terminal output
 * - Language/syntax selection based on file extension
 */

#ifndef LOKI_SYNTAX_H
#define LOKI_SYNTAX_H

#include "loki_internal.h"

/* Syntax highlighting functions */

/* Update syntax highlighting for a single row.
 * Analyzes the row's rendered content and populates the hl array with
 * syntax highlight types (HL_NORMAL, HL_KEYWORD1, HL_STRING, etc.). */
void syntax_update_row(editor_ctx_t *ctx, t_erow *row);

/* Format a syntax highlight color as an ANSI true color escape sequence.
 * Returns the length of the formatted string written to buf. */
int syntax_format_color(editor_ctx_t *ctx, int hl, char *buf, size_t bufsize);

/* Select syntax highlighting scheme based on filename extension.
 * Searches both built-in HLDB and dynamic language registry. */
void syntax_select_for_filename(editor_ctx_t *ctx, char *filename);

/* Map human-readable style name to HL_* constant.
 * Used by Lua API for color customization. Returns -1 if name unknown. */
int syntax_name_to_code(const char *name);

/* Check if a character is a word separator.
 * Separators are defined per-language in the syntax definition. */
int syntax_is_separator(int c, char *separators);

/* Check if a row has an unclosed multi-line comment.
 * Returns 1 if the row ends with an open comment that continues to next line. */
int syntax_row_has_open_comment(t_erow *row);

/* Initialize default syntax highlighting colors.
 * Sets up RGB color values for all HL_* types with sensible defaults. */
void syntax_init_default_colors(editor_ctx_t *ctx);

#endif /* LOKI_SYNTAX_H */
