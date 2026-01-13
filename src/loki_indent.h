/* loki_indent.h - Auto-indentation module
 *
 * This module provides automatic indentation features including:
 * - Copying indentation from previous line when pressing Enter
 * - Electric dedent for closing braces/brackets
 * - Tab/space style detection
 * - Language-aware indentation rules
 */

#ifndef LOKI_INDENT_H
#define LOKI_INDENT_H

#include "loki_internal.h"

/* Indentation style constants */
#define INDENT_STYLE_SPACES 0
#define INDENT_STYLE_TABS   1

/* Indentation configuration stored in editor context */
typedef struct {
    int enabled;            /* Is auto-indent enabled? (default: 1) */
    int style;              /* INDENT_STYLE_SPACES or INDENT_STYLE_TABS */
    int width;              /* Number of spaces per indent level (default: 4) */
    int electric_enabled;   /* Is electric dedent enabled? (default: 1) */
} indent_config_t;

/* Get the indentation level (in spaces) of a given row.
 * Returns the number of leading spaces/tabs (tabs counted as width spaces).
 * Returns 0 for empty lines or lines with no indentation. */
int indent_get_level(editor_ctx_t *ctx, int row);

/* Detect indentation style from the file content.
 * Analyzes the file to determine if it uses tabs or spaces.
 * Returns INDENT_STYLE_SPACES or INDENT_STYLE_TABS.
 * Uses heuristic: count lines with leading tabs vs spaces. */
int indent_detect_style(editor_ctx_t *ctx);

/* Apply indentation to current line based on previous line.
 * Called when user presses Enter - copies indentation from previous line.
 * If previous line ends with opening brace/bracket, adds one level.
 * ctx->cy should point to the newly created line. */
void indent_apply(editor_ctx_t *ctx);

/* Handle electric dedent when typing closing characters.
 * Called when user types }, ], or ).
 * If the line contains only whitespace + closing char, dedents by one level.
 * Returns 1 if dedent was applied, 0 otherwise. */
int indent_electric_char(editor_ctx_t *ctx, int c);

/* Initialize indentation configuration with defaults.
 * Called from editor_ctx_init(). */
void indent_init(editor_ctx_t *ctx);

/* Enable or disable auto-indent feature.
 * Can be called from Lua or command mode. */
void indent_set_enabled(editor_ctx_t *ctx, int enabled);

/* Set indentation width (number of spaces per level).
 * Can be called from Lua or command mode. */
void indent_set_width(editor_ctx_t *ctx, int width);

/* Test-only accessors for indent_config fields (opaque structure) */
int indent_get_width(editor_ctx_t *ctx);
int indent_get_enabled(editor_ctx_t *ctx);
int indent_get_electric_enabled(editor_ctx_t *ctx);

#endif /* LOKI_INDENT_H */
