/* loki_buffers.h - Multiple buffer management interface
 *
 * This module implements multiple buffer support, allowing users to edit
 * multiple files simultaneously with tab-like switching between buffers.
 *
 * Features:
 * - Create new buffers (empty or from file)
 * - Switch between buffers
 * - Close buffers (with unsaved changes warning)
 * - Tab-like status bar display showing all open buffers
 *
 * Keybindings:
 * - Ctrl-T: Create new empty buffer
 * - Ctrl-W: Close current buffer
 * - Ctrl-Tab: Switch to next buffer
 * - Ctrl-Shift-Tab: Switch to previous buffer (if supported by terminal)
 */

#ifndef LOKI_BUFFERS_H
#define LOKI_BUFFERS_H

#include "loki/core.h"
#include "loki_internal.h"

/* Maximum number of simultaneous buffers */
#define MAX_BUFFERS 16

/* Buffer state - opaque structure defined in loki_buffers.c */
struct buffer_state;

/* ======================== Public API ======================== */

/* Initialize buffer system
 * Creates the initial buffer and sets up buffer management structures
 * Returns: 0 on success, -1 on failure */
int buffers_init(editor_ctx_t *initial_ctx);

/* Free buffer system resources
 * Cleans up all buffers and associated memory */
void buffers_free(void);

/* Create a new buffer
 * filename: File to open (NULL for empty buffer)
 * Returns: Buffer ID on success, -1 on failure */
int buffer_create(const char *filename);

/* Close a buffer by ID
 * buffer_id: ID of buffer to close
 * force: If 0, warn about unsaved changes; if 1, close regardless
 * Returns: 0 on success, -1 if buffer not found, 1 if unsaved changes and not forced */
int buffer_close(int buffer_id, int force);

/* Switch to buffer by ID
 * buffer_id: ID of buffer to switch to
 * Returns: 0 on success, -1 if buffer not found */
int buffer_switch(int buffer_id);

/* Switch to next buffer (circular)
 * Returns: New buffer ID, or -1 if no buffers */
int buffer_next(void);

/* Switch to previous buffer (circular)
 * Returns: New buffer ID, or -1 if no buffers */
int buffer_prev(void);

/* Get current buffer context
 * Returns: Pointer to current editor context, or NULL if no buffers */
editor_ctx_t *buffer_get_current(void);

/* Get buffer context by ID
 * buffer_id: ID of buffer to get
 * Returns: Pointer to editor context, or NULL if not found */
editor_ctx_t *buffer_get(int buffer_id);

/* Get current buffer ID
 * Returns: Current buffer ID, or -1 if no buffers */
int buffer_get_current_id(void);

/* Get number of open buffers
 * Returns: Number of open buffers */
int buffer_count(void);

/* Get list of buffer IDs
 * ids: Array to fill with buffer IDs (must be at least MAX_BUFFERS in size)
 * Returns: Number of buffers (IDs filled in array) */
int buffer_get_list(int *ids);

/* Get buffer display name (filename or "[No Name]")
 * buffer_id: ID of buffer
 * Returns: Display name (pointer to internal buffer, do not free) */
const char *buffer_get_display_name(int buffer_id);

/* Check if buffer has unsaved changes
 * buffer_id: ID of buffer to check
 * Returns: 1 if modified, 0 if not, -1 if buffer not found */
int buffer_is_modified(int buffer_id);

/* Render buffer tabs for status bar
 * ab: Append buffer to write tab display into
 * max_width: Maximum width available for tabs (screen width)
 * This displays: [1:file.c*] [2:test.py] [3:[No Name]*]
 * Where * indicates modified buffer and highlighted tab is current */
void buffers_render_tabs(struct abuf *ab, int max_width);

/* Save current buffer context state
 * Called before switching buffers to preserve cursor position, etc. */
void buffers_save_current_state(void);

/* Restore buffer context state
 * Called after switching buffers to restore cursor position, etc. */
void buffers_restore_state(editor_ctx_t *ctx);

/* Update buffer display name (call after changing filename)
 * buffer_id: ID of buffer to update
 * This refreshes the cached display name based on the current filename */
void buffer_update_display_name(int buffer_id);

#endif /* LOKI_BUFFERS_H */
