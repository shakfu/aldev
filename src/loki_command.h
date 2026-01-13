/* loki_command.h - Vim-like command mode interface
 *
 * This module implements vim-style command mode (:w, :q, :set, etc.)
 * Provides command parsing, execution, history, and Lua extensibility.
 */

#ifndef LOKI_COMMAND_H
#define LOKI_COMMAND_H

#include "loki/core.h"

/* Command input buffer size */
#define COMMAND_BUFFER_SIZE 256
#define COMMAND_HISTORY_MAX 50

/* Command handler function signature
 * ctx: Editor context
 * args: Command arguments (NULL if no args)
 * Returns: 1 on success, 0 on failure */
typedef int (*command_handler_t)(editor_ctx_t *ctx, const char *args);

/* Command registration entry */
typedef struct {
    const char *name;          /* Command name (without ':') */
    command_handler_t handler; /* Function to execute */
    const char *help;          /* Help text */
    int min_args;              /* Minimum arguments required */
    int max_args;              /* Maximum arguments (-1 = unlimited) */
} command_def_t;

/* ======================== Public API ======================== */

/* Initialize command mode subsystem */
void command_mode_init(editor_ctx_t *ctx);

/* Free command mode resources */
void command_mode_free(editor_ctx_t *ctx);

/* Enter command mode (sets mode to MODE_COMMAND) */
void command_mode_enter(editor_ctx_t *ctx);

/* Exit command mode (returns to MODE_NORMAL) */
void command_mode_exit(editor_ctx_t *ctx);

/* Handle keypress in command mode */
void command_mode_handle_key(editor_ctx_t *ctx, int fd, int key);

/* Execute command line (e.g., ":w file.txt")
 * Returns: 1 on success, 0 on failure */
int command_execute(editor_ctx_t *ctx, const char *cmdline);

/* Register custom command (for Lua integration)
 * Returns: 1 on success, 0 on failure (already exists or registry full) */
int command_register(const char *name, command_handler_t handler,
                     const char *help, int min_args, int max_args);

/* Unregister all dynamic commands (cleanup) */
void command_unregister_all_dynamic(void);

/* Get command history entry by index (0 = oldest)
 * Returns: NULL if index out of bounds */
const char* command_history_get(int index);

/* Get command history length */
int command_history_len(void);

/* ======================== Built-in Command Handlers ======================== */

/* :w [filename] - Write (save) file */
int cmd_write(editor_ctx_t *ctx, const char *args);

/* :q - Quit editor (fails if unsaved changes) */
int cmd_quit(editor_ctx_t *ctx, const char *args);

/* :q! - Force quit without saving */
int cmd_force_quit(editor_ctx_t *ctx, const char *args);

/* :wq [filename] - Write and quit */
int cmd_write_quit(editor_ctx_t *ctx, const char *args);

/* :e <filename> - Edit file */
int cmd_edit(editor_ctx_t *ctx, const char *args);

/* :help [command] - Show help */
int cmd_help(editor_ctx_t *ctx, const char *args);

/* :set <option> [value] - Set editor option */
int cmd_set(editor_ctx_t *ctx, const char *args);

#endif /* LOKI_COMMAND_H */
