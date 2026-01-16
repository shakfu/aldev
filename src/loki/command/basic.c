/* basic.c - Basic editor commands (:q, :wq, :help, :set)
 *
 * Core commands for quitting, help, and settings.
 */

#include "command_impl.h"

/* :q, :quit - Quit editor */
int cmd_quit(editor_ctx_t *ctx, const char *args) {
    (void)args;

    if (ctx->dirty) {
        editor_set_status_msg(ctx, "Unsaved changes! Use :q! to force quit");
        return 0;
    }

    /* Exit program */
    exit(0);
}

/* :q!, :quit! - Force quit without saving */
int cmd_force_quit(editor_ctx_t *ctx, const char *args) {
    (void)ctx;
    (void)args;

    /* Exit without checking dirty flag */
    exit(0);
}

/* :wq, :x - Write and quit */
int cmd_write_quit(editor_ctx_t *ctx, const char *args) {
    /* Save first */
    if (!cmd_write(ctx, args)) {
        return 0;
    }

    /* Then quit */
    exit(0);
}

/* :help, :h - Show help */
int cmd_help(editor_ctx_t *ctx, const char *args) {
    if (!args || !args[0]) {
        /* Show general help */
        editor_set_status_msg(ctx,
            "Commands: :w :q :wq :set :e :help <cmd> | Ctrl-F=find Ctrl-S=save");
        return 1;
    }

    /* Show help for specific command */
    command_def_t *cmd = command_find(args);
    if (cmd) {
        editor_set_status_msg(ctx, ":%s - %s", cmd->name, cmd->help);
        return 1;
    } else {
        editor_set_status_msg(ctx, "Unknown command: %s", args);
        return 0;
    }
}

/* :set - Set editor options */
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
