#ifndef LOKI_CORE_H
#define LOKI_CORE_H

#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Loki Editor Core API
 *
 * This header defines the C API for the Loki editor core.
 * Lua bindings and other language wrappers should use these functions
 * to interact with the editor.
 * ============================================================================ */

/* Forward declarations */
struct lua_State;
struct t_erow;
struct t_hlcolor;
struct t_editor_syntax;
struct editor_ctx;
typedef struct editor_ctx editor_ctx_t;

/* Editor modes */
typedef enum {
    MODE_NORMAL,
    MODE_INSERT,
    MODE_VISUAL,
    MODE_COMMAND
} EditorMode;

/* ============================================================================
 * Status and Messages
 * ============================================================================ */

/* Set status bar message (printf-style) */
void editor_set_status_msg(editor_ctx_t *ctx, const char *fmt, ...);

/* ============================================================================
 * Buffer Access (Read-only)
 * ============================================================================ */

/* Get total number of lines in buffer */
int editor_get_num_rows(void);

/* Get line content at row (0-indexed), returns NULL if out of bounds */
const char *editor_get_line(int row);

/* Get row structure (for advanced access), returns NULL if out of bounds */
const struct t_erow *editor_get_row(int row);

/* Get current filename, returns NULL if unsaved */
const char *editor_get_filename(void);

/* Check if file has unsaved changes */
int editor_file_was_modified(void);

/* ============================================================================
 * Cursor Position
 * ============================================================================ */

/* Get cursor position (screen coordinates) */
void editor_get_cursor(int *row, int *col);

/* Get cursor position (file coordinates - with offset) */
void editor_get_cursor_file_pos(int *row, int *col);

/* ============================================================================
 * Buffer Modification
 * ============================================================================ */

/* Insert a single character at cursor position */
void editor_insert_char(editor_ctx_t *ctx, int c);

/* Delete character at cursor position */
void editor_del_char(editor_ctx_t *ctx);

/* Insert newline at cursor position */
void editor_insert_newline(editor_ctx_t *ctx);

/* ============================================================================
 * File Operations
 * ============================================================================ */

/* Save current buffer to file (returns 0 on success, 1 on error) */
int editor_save(editor_ctx_t *ctx);

/* Open a file */
int editor_open(editor_ctx_t *ctx, char *filename);

/* ============================================================================
 * Display and Rendering
 * ============================================================================ */

/* Refresh the screen display */
void editor_refresh_screen(editor_ctx_t *ctx);

/* ============================================================================
 * Modal Editing
 * ============================================================================ */

/* Get current editor mode */
EditorMode editor_get_mode(void);

/* Set editor mode */
void editor_set_mode(EditorMode mode);

/* ============================================================================
 * Color and Theming
 * ============================================================================ */

/* Set syntax highlight color by index (0-8) */
void editor_set_color(int hl_index, int r, int g, int b);

/* Get syntax highlight color by index */
const struct t_hlcolor *editor_get_color(int hl_index);

/* Syntax highlight constants */
#define HL_NORMAL 0
#define HL_NONPRINT 1
#define HL_COMMENT 2
#define HL_MLCOMMENT 3
#define HL_KEYWORD1 4
#define HL_KEYWORD2 5
#define HL_STRING 6
#define HL_NUMBER 7
#define HL_MATCH 8

/* ============================================================================
 * Syntax Highlighting
 * ============================================================================ */

/* Language configuration for syntax highlighting */
struct editor_language_config {
    const char *name;
    const char **extensions;
    int num_extensions;
    const char **keywords;
    int num_keywords;
    const char **types;
    int num_types;
    const char *line_comment;
    const char *block_comment_start;
    const char *block_comment_end;
    const char *separators;
    int highlight_strings;
    int highlight_numbers;
};

/* Register a new language for syntax highlighting */
int editor_register_language(const struct editor_language_config *config);

/* Select syntax highlighting for current file */
void editor_select_syntax_highlight(editor_ctx_t *ctx, char *filename);

/* ============================================================================
 * Selection
 * ============================================================================ */

/* Check if selection is active */
int editor_selection_active(void);

/* Get selection bounds (returns 0 if no selection) */
int editor_get_selection(int *start_x, int *start_y, int *end_x, int *end_y);

/* Set selection */
void editor_set_selection(int start_x, int start_y, int end_x, int end_y);

/* Clear selection */
void editor_clear_selection(void);

/* ============================================================================
 * Async HTTP (for Lua integration)
 * ============================================================================ */

/* Start an async HTTP request
 * Returns request ID on success, -1 on failure
 * The callback is a Lua function name (global) */
int editor_start_async_http(
    const char *url,
    const char *method,
    const char *body,
    const char **headers,
    int num_headers,
    const char *lua_callback
);

/* Poll async HTTP requests (called from main loop) */
void editor_poll_async_http(struct lua_State *L);

/* ============================================================================
 * Initialization and Cleanup
 * ============================================================================ */

/* Initialize the editor */
void init_editor(editor_ctx_t *ctx);

/* Cleanup function (called at exit) */
void editor_atexit(void);

/* ============================================================================
 * Screen Dimensions
 * ============================================================================ */

/* Get screen dimensions */
void editor_get_screen_size(int *rows, int *cols);

#ifdef __cplusplus
}
#endif

#endif /* LOKI_CORE_H */
