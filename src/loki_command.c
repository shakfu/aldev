/* loki_command.c - Vim-like command mode implementation
 *
 * Handles command mode (:w, :q, :set, etc.) for vim-like editing.
 * Commands can be built-in (C functions) or registered from Lua.
 */

#include "loki_command.h"
#include "loki_internal.h"
#include "loki_terminal.h"
#include "loki_buffers.h"
#include <lua.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* Command history storage */
static char *command_history[COMMAND_HISTORY_MAX];
static int command_history_count = 0;

/* Dynamic command registry (for Lua-registered commands) */
#define MAX_DYNAMIC_COMMANDS 100
static command_def_t dynamic_commands[MAX_DYNAMIC_COMMANDS];
static int dynamic_command_count = 0;

/* Built-in command table */
static command_def_t builtin_commands[] = {
    {"w",      cmd_write,       "Write (save) file",              0, 1},
    {"write",  cmd_write,       "Write (save) file",              0, 1},
    {"q",      cmd_quit,        "Quit editor",                    0, 0},
    {"quit",   cmd_quit,        "Quit editor",                    0, 0},
    {"wq",     cmd_write_quit,  "Write and quit",                 0, 1},
    {"x",      cmd_write_quit,  "Write and quit (if modified)",   0, 1},
    {"q!",     cmd_force_quit,  "Quit without saving",            0, 0},
    {"quit!",  cmd_force_quit,  "Quit without saving",            0, 0},
    {"help",   cmd_help,        "Show help",                      0, 1},
    {"h",      cmd_help,        "Show help",                      0, 1},
    {"set",    cmd_set,         "Set option (wrap, etc)",         0, 2},
    {"e",      cmd_edit,        "Edit file",                      1, 1},
    {"edit",   cmd_edit,        "Edit file",                      1, 1},
    {NULL, NULL, NULL, 0, 0}  /* Sentinel */
};

/* ======================== Command State Management ======================== */

void command_mode_init(editor_ctx_t *ctx) {
    /* Command state is part of editor_ctx_t, just reset it */
    memset(ctx->cmd_buffer, 0, sizeof(ctx->cmd_buffer));
    ctx->cmd_length = 0;
    ctx->cmd_cursor_pos = 0;
    ctx->cmd_history_index = 0;
}

void command_mode_free(editor_ctx_t *ctx) {
    /* Nothing to free currently - command buffer is inline */
    (void)ctx;
}

void command_mode_enter(editor_ctx_t *ctx) {
    ctx->mode = MODE_COMMAND;
    ctx->cmd_buffer[0] = ':';
    ctx->cmd_buffer[1] = '\0';
    ctx->cmd_length = 1;
    ctx->cmd_cursor_pos = 1;
    ctx->cmd_history_index = command_history_count;  /* Start at end of history */
    editor_set_status_msg(ctx, ":");
}

void command_mode_exit(editor_ctx_t *ctx) {
    ctx->mode = MODE_NORMAL;
    ctx->cmd_length = 0;
    ctx->cmd_cursor_pos = 0;
    memset(ctx->cmd_buffer, 0, sizeof(ctx->cmd_buffer));
    editor_set_status_msg(ctx, "");
}

/* ======================== Command History ======================== */

static void command_history_add(const char *cmd) {
    /* Don't add empty or duplicate commands */
    if (!cmd || !cmd[0]) return;
    if (command_history_count > 0 &&
        strcmp(command_history[command_history_count - 1], cmd) == 0) {
        return;
    }

    /* Add to history */
    if (command_history_count < COMMAND_HISTORY_MAX) {
        command_history[command_history_count++] = strdup(cmd);
    } else {
        /* Shift and reuse oldest slot */
        free(command_history[0]);
        memmove(command_history, command_history + 1,
                sizeof(char*) * (COMMAND_HISTORY_MAX - 1));
        command_history[COMMAND_HISTORY_MAX - 1] = strdup(cmd);
    }
}

const char* command_history_get(int index) {
    if (index < 0 || index >= command_history_count) return NULL;
    return command_history[index];
}

int command_history_len(void) {
    return command_history_count;
}

/* ======================== Command Parsing ======================== */

/* Parse command line into command name and arguments */
static int parse_command(const char *cmdline, char **cmd_name, char **args) {
    /* Skip leading ':' and whitespace */
    while (*cmdline && (*cmdline == ':' || isspace(*cmdline))) {
        cmdline++;
    }

    if (!*cmdline) return 0;  /* Empty command */

    /* Find command name (up to first space or end) */
    const char *cmd_end = cmdline;
    while (*cmd_end && !isspace(*cmd_end)) {
        cmd_end++;
    }

    size_t cmd_len = cmd_end - cmdline;
    *cmd_name = malloc(cmd_len + 1);
    if (!*cmd_name) return 0;

    memcpy(*cmd_name, cmdline, cmd_len);
    (*cmd_name)[cmd_len] = '\0';

    /* Skip whitespace after command */
    while (*cmd_end && isspace(*cmd_end)) {
        cmd_end++;
    }

    /* Rest is arguments */
    if (*cmd_end) {
        *args = strdup(cmd_end);
    } else {
        *args = NULL;
    }

    return 1;
}

/* Find command definition (builtin or dynamic) */
static command_def_t* find_command(const char *name) {
    /* Check built-in commands */
    for (int i = 0; builtin_commands[i].name != NULL; i++) {
        if (strcmp(builtin_commands[i].name, name) == 0) {
            return &builtin_commands[i];
        }
    }

    /* Check dynamic (Lua-registered) commands */
    for (int i = 0; i < dynamic_command_count; i++) {
        if (strcmp(dynamic_commands[i].name, name) == 0) {
            return &dynamic_commands[i];
        }
    }

    return NULL;
}

/* ======================== Command Execution ======================== */

int command_execute(editor_ctx_t *ctx, const char *cmdline) {
    char *cmd_name = NULL;
    char *args = NULL;

    if (!parse_command(cmdline, &cmd_name, &args)) {
        editor_set_status_msg(ctx, "");
        return 0;
    }

    /* Add to history */
    command_history_add(cmdline + 1);  /* Skip ':' prefix */

    /* Find command handler */
    command_def_t *cmd = find_command(cmd_name);
    if (!cmd) {
        editor_set_status_msg(ctx, "Unknown command: %s", cmd_name);
        free(cmd_name);
        free(args);
        return 0;
    }

    /* Validate argument count (simplified: just check if args exist) */
    int has_args = (args && args[0]) ? 1 : 0;
    if (has_args < cmd->min_args) {
        editor_set_status_msg(ctx, ":%s requires arguments", cmd_name);
        free(cmd_name);
        free(args);
        return 0;
    }

    /* Store command name for Lua handlers (they need to know which command was called) */
    if (ctx->L) {
        lua_State *L = ctx->L;
        lua_pushstring(L, cmd_name);
        lua_setglobal(L, "_loki_ex_command_executing");
    }

    /* Execute command */
    int result = cmd->handler(ctx, args);

    /* Clear the command name */
    if (ctx->L) {
        lua_State *L = ctx->L;
        lua_pushnil(L);
        lua_setglobal(L, "_loki_ex_command_executing");
    }

    free(cmd_name);
    free(args);
    return result;
}

/* ======================== Command Mode Input Handling ======================== */

void command_mode_handle_key(editor_ctx_t *ctx, int fd, int key) {
    (void)fd;  /* fd parameter for future use */

    switch (key) {
        case ESC:
            command_mode_exit(ctx);
            break;

        case ENTER:
            /* Execute command */
            if (ctx->cmd_length > 1) {  /* More than just ':' */
                command_execute(ctx, ctx->cmd_buffer);
            }
            command_mode_exit(ctx);
            break;

        case BACKSPACE:
        case CTRL_H:
        case DEL_KEY:
            if (ctx->cmd_cursor_pos > 1) {  /* Can't delete ':' */
                /* Remove character before cursor */
                memmove(ctx->cmd_buffer + ctx->cmd_cursor_pos - 1,
                        ctx->cmd_buffer + ctx->cmd_cursor_pos,
                        ctx->cmd_length - ctx->cmd_cursor_pos + 1);
                ctx->cmd_cursor_pos--;
                ctx->cmd_length--;
                editor_set_status_msg(ctx, "%s", ctx->cmd_buffer);
            } else {
                /* Backspace on empty command exits command mode */
                command_mode_exit(ctx);
            }
            break;

        case ARROW_LEFT:
            if (ctx->cmd_cursor_pos > 1) {
                ctx->cmd_cursor_pos--;
            }
            break;

        case ARROW_RIGHT:
            if (ctx->cmd_cursor_pos < ctx->cmd_length) {
                ctx->cmd_cursor_pos++;
            }
            break;

        case ARROW_UP:
            /* Previous command in history */
            if (ctx->cmd_history_index > 0) {
                ctx->cmd_history_index--;
                const char *hist = command_history_get(ctx->cmd_history_index);
                if (hist) {
                    ctx->cmd_buffer[0] = ':';
                    strncpy(ctx->cmd_buffer + 1, hist,
                            sizeof(ctx->cmd_buffer) - 2);
                    ctx->cmd_buffer[sizeof(ctx->cmd_buffer) - 1] = '\0';
                    ctx->cmd_length = strlen(ctx->cmd_buffer);
                    ctx->cmd_cursor_pos = ctx->cmd_length;
                    editor_set_status_msg(ctx, "%s", ctx->cmd_buffer);
                }
            }
            break;

        case ARROW_DOWN:
            /* Next command in history */
            if (ctx->cmd_history_index < command_history_count - 1) {
                ctx->cmd_history_index++;
                const char *hist = command_history_get(ctx->cmd_history_index);
                if (hist) {
                    ctx->cmd_buffer[0] = ':';
                    strncpy(ctx->cmd_buffer + 1, hist,
                            sizeof(ctx->cmd_buffer) - 2);
                    ctx->cmd_buffer[sizeof(ctx->cmd_buffer) - 1] = '\0';
                    ctx->cmd_length = strlen(ctx->cmd_buffer);
                    ctx->cmd_cursor_pos = ctx->cmd_length;
                    editor_set_status_msg(ctx, "%s", ctx->cmd_buffer);
                }
            } else {
                /* At end of history, clear command */
                ctx->cmd_buffer[0] = ':';
                ctx->cmd_buffer[1] = '\0';
                ctx->cmd_length = 1;
                ctx->cmd_cursor_pos = 1;
                ctx->cmd_history_index = command_history_count;
                editor_set_status_msg(ctx, ":");
            }
            break;

        case CTRL_U:
            /* Clear command line */
            ctx->cmd_buffer[0] = ':';
            ctx->cmd_buffer[1] = '\0';
            ctx->cmd_length = 1;
            ctx->cmd_cursor_pos = 1;
            editor_set_status_msg(ctx, ":");
            break;

        default:
            /* Regular character input */
            if (isprint(key) && ctx->cmd_length < COMMAND_BUFFER_SIZE - 1) {
                /* Insert character at cursor */
                memmove(ctx->cmd_buffer + ctx->cmd_cursor_pos + 1,
                        ctx->cmd_buffer + ctx->cmd_cursor_pos,
                        ctx->cmd_length - ctx->cmd_cursor_pos + 1);
                ctx->cmd_buffer[ctx->cmd_cursor_pos] = key;
                ctx->cmd_cursor_pos++;
                ctx->cmd_length++;
                editor_set_status_msg(ctx, "%s", ctx->cmd_buffer);
            }
            break;
    }
}

/* ======================== Built-in Command Implementations ======================== */

int cmd_write(editor_ctx_t *ctx, const char *args) {
    /* Use provided filename or current filename */
    if (args && args[0]) {
        /* Save to new filename */
        if (ctx->filename) {
            free(ctx->filename);
        }
        ctx->filename = strdup(args);

        /* Update buffer display name */
        buffer_update_display_name(buffer_get_current_id());
    }

    if (!ctx->filename) {
        editor_set_status_msg(ctx, "No filename");
        return 0;
    }

    /* Save file using existing editor_save() */
    int len = editor_save(ctx);
    if (len >= 0) {
        editor_set_status_msg(ctx, "\"%s\" %dL written",
                             ctx->filename, ctx->numrows);
        ctx->dirty = 0;
        return 1;
    } else {
        editor_set_status_msg(ctx, "Error writing file");
        return 0;
    }
}

int cmd_quit(editor_ctx_t *ctx, const char *args) {
    (void)args;

    if (ctx->dirty) {
        editor_set_status_msg(ctx, "Unsaved changes! Use :q! to force quit");
        return 0;
    }

    /* Exit program */
    exit(0);
}

int cmd_force_quit(editor_ctx_t *ctx, const char *args) {
    (void)ctx;
    (void)args;

    /* Exit without checking dirty flag */
    exit(0);
}

int cmd_write_quit(editor_ctx_t *ctx, const char *args) {
    /* Save first */
    if (!cmd_write(ctx, args)) {
        return 0;
    }

    /* Then quit */
    exit(0);
}

int cmd_edit(editor_ctx_t *ctx, const char *args) {
    if (!args || !args[0]) {
        editor_set_status_msg(ctx, "Filename required");
        return 0;
    }

    if (ctx->dirty) {
        editor_set_status_msg(ctx, "Unsaved changes! Save first or use :q!");
        return 0;
    }

    /* Load new file */
    editor_open(ctx, (char*)args);  /* Cast away const - editor_open doesn't modify */
    editor_set_status_msg(ctx, "\"%s\" loaded", args);
    return 1;
}

int cmd_help(editor_ctx_t *ctx, const char *args) {
    if (!args || !args[0]) {
        /* Show general help */
        editor_set_status_msg(ctx,
            "Commands: :w :q :wq :set :e :help <cmd> | Ctrl-F=find Ctrl-S=save");
        return 1;
    }

    /* Show help for specific command */
    command_def_t *cmd = find_command(args);
    if (cmd) {
        editor_set_status_msg(ctx, ":%s - %s", cmd->name, cmd->help);
        return 1;
    } else {
        editor_set_status_msg(ctx, "Unknown command: %s", args);
        return 0;
    }
}

int cmd_set(editor_ctx_t *ctx, const char *args) {
    if (!args || !args[0]) {
        /* Show current settings */
        editor_set_status_msg(ctx, "Options: wrap");
        return 1;
    }

    /* Parse "set option" or "set option=value" */
    char option[64] = {0};
    char value[64] = {0};

    if (sscanf(args, "%63s = %63s", option, value) == 2 ||
        sscanf(args, "%63s=%63s", option, value) == 2) {
        /* Set option to value */
        editor_set_status_msg(ctx, "Set %s=%s (not implemented yet)", option, value);
        return 1;
    } else if (sscanf(args, "%63s", option) == 1) {
        /* Toggle boolean option or show value */
        if (strcmp(option, "wrap") == 0) {
            ctx->word_wrap = !ctx->word_wrap;
            editor_set_status_msg(ctx, "Word wrap: %s",
                                 ctx->word_wrap ? "on" : "off");
            return 1;
        } else {
            editor_set_status_msg(ctx, "Unknown option: %s", option);
            return 0;
        }
    }

    return 0;
}

/* ======================== Dynamic Command Registration (for Lua) ======================== */

int command_register(const char *name, command_handler_t handler,
                     const char *help, int min_args, int max_args) {
    if (dynamic_command_count >= MAX_DYNAMIC_COMMANDS) {
        return 0;  /* Registry full */
    }

    /* Check if command already exists */
    if (find_command(name)) {
        return 0;  /* Can't override built-in or existing command */
    }

    /* Register new command */
    dynamic_commands[dynamic_command_count].name = strdup(name);
    dynamic_commands[dynamic_command_count].handler = handler;
    dynamic_commands[dynamic_command_count].help = strdup(help);
    dynamic_commands[dynamic_command_count].min_args = min_args;
    dynamic_commands[dynamic_command_count].max_args = max_args;
    dynamic_command_count++;

    return 1;
}

void command_unregister_all_dynamic(void) {
    for (int i = 0; i < dynamic_command_count; i++) {
        free((char*)dynamic_commands[i].name);
        free((char*)dynamic_commands[i].help);
    }
    dynamic_command_count = 0;
}
