/* loki_terminal.h - Terminal I/O abstraction layer
 *
 * This module provides low-level terminal operations including:
 * - Raw mode management (disabling canonical mode, echo, etc.)
 * - Key reading with escape sequence parsing
 * - Window size detection and monitoring
 * - Screen buffer management for efficient rendering
 *
 * These functions are platform-specific (POSIX) and isolate terminal
 * dependencies from the core editor logic.
 */

#ifndef LOKI_TERMINAL_H
#define LOKI_TERMINAL_H

#include "loki_internal.h"  /* For editor_ctx_t, abuf */

/* ======================= Terminal Mode Management ========================= */

/* Enable raw mode on the given file descriptor.
 * Raw mode disables canonical input, echo, and signal generation.
 * Returns 0 on success, -1 on failure (sets errno to ENOTTY). */
int terminal_enable_raw_mode(editor_ctx_t *ctx, int fd);

/* Disable raw mode, restoring terminal to original state.
 * Should be called before exit to avoid leaving terminal in bad state. */
void terminal_disable_raw_mode(editor_ctx_t *ctx, int fd);

/* ======================= Input Reading ==================================== */

/* Read a single key from the terminal, handling escape sequences.
 * Blocks until a key is available or timeout occurs.
 * Returns:
 *   - ASCII value for normal keys (0-127)
 *   - KEY_* constants for special keys (arrows, function keys, etc.)
 *   - Exits on EOF after timeout */
int terminal_read_key(int fd);

/* ======================= Window Size Detection ============================ */

/* Get current terminal window size in rows and columns.
 * First tries ioctl(TIOCGWINSZ), falls back to VT100 cursor queries.
 * Returns 0 on success, -1 on failure. */
int terminal_get_window_size(int ifd, int ofd, int *rows, int *cols);

/* Query cursor position using VT100 escape sequences.
 * Used as fallback when ioctl fails.
 * Returns 0 on success, -1 on failure. */
int terminal_get_cursor_position(int ifd, int ofd, int *rows, int *cols);

/* Update editor context with current window size.
 * Adjusts screenrows/screencols and handles REPL layout.
 * Should be called on initialization and after SIGWINCH. */
void terminal_update_window_size(editor_ctx_t *ctx);

/* Check if window size changed and update if needed.
 * Reads the winsize_changed flag set by signal handler.
 * Safe to call in main loop (signal handler only sets flag). */
void terminal_handle_resize(editor_ctx_t *ctx);

/* ======================= Screen Buffer ==================================== */

/* Append string to screen buffer for efficient rendering.
 * Buffers all VT100 escape sequences and content, then flushes
 * in a single write() call to minimize flicker.
 * Exits on allocation failure after attempting cleanup. */
void terminal_buffer_append(struct abuf *ab, const char *s, int len);

/* Free screen buffer memory. */
void terminal_buffer_free(struct abuf *ab);

/* ======================= Signal Handling ================================== */

/* Signal handler for SIGWINCH (window size change).
 * Async-signal-safe: only sets a flag, actual handling in terminal_handle_resize().
 * Should be registered with signal(SIGWINCH, terminal_sig_winch_handler). */
void terminal_sig_winch_handler(int sig);

#endif /* LOKI_TERMINAL_H */
