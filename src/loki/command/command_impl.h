/* command_impl.h - Shared header for command implementations
 *
 * Include this header when implementing new ex-commands.
 * Each command file should:
 *   1. Include this header
 *   2. Implement one or more cmd_* functions
 *   3. Declare functions in this header (extern)
 *   4. Register in builtin_commands[] in command.c
 *
 * Example command implementation (mycommand.c):
 *
 *   #include "command_impl.h"
 *
 *   int cmd_mycommand(editor_ctx_t *ctx, const char *args) {
 *       if (!args || !args[0]) {
 *           editor_set_status_msg(ctx, "Usage: :mycommand <arg>");
 *           return 0;
 *       }
 *       // ... implementation ...
 *       editor_set_status_msg(ctx, "Done!");
 *       return 1;
 *   }
 *
 * Then add to command.c:
 *   - Add to builtin_commands[]: {"mycommand", cmd_mycommand, "Description", min_args, max_args}
 */

#ifndef LOKI_COMMAND_IMPL_H
#define LOKI_COMMAND_IMPL_H

#include "../internal.h"
#include "../command.h"
#include "../buffers.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* ======================== File Commands (file.c) ======================== */

/* :w, :write - Save file */
int cmd_write(editor_ctx_t *ctx, const char *args);

/* :e, :edit - Open file */
int cmd_edit(editor_ctx_t *ctx, const char *args);

/* ======================== Basic Commands (basic.c) ======================== */

/* :q, :quit - Quit editor */
int cmd_quit(editor_ctx_t *ctx, const char *args);

/* :q!, :quit! - Force quit without saving */
int cmd_force_quit(editor_ctx_t *ctx, const char *args);

/* :wq, :x - Write and quit */
int cmd_write_quit(editor_ctx_t *ctx, const char *args);

/* :help, :h - Show help */
int cmd_help(editor_ctx_t *ctx, const char *args);

/* :set - Set editor options */
int cmd_set(editor_ctx_t *ctx, const char *args);

/* ======================== Navigation Commands (goto.c) ======================== */

/* :goto, :<number> - Go to line number */
int cmd_goto(editor_ctx_t *ctx, const char *args);

/* ======================== Edit Commands (substitute.c) ======================== */

/* :s/old/new/[g] - Search and replace on current line */
int cmd_substitute(editor_ctx_t *ctx, const char *args);

/* ======================== Audio Commands (link.c) ======================== */

/* :link - Toggle Ableton Link */
int cmd_link(editor_ctx_t *ctx, const char *args);

/* ======================== Audio Commands (csd.c) ======================== */

/* :csd - Toggle Csound synthesis */
int cmd_csd(editor_ctx_t *ctx, const char *args);

/* ======================== Export Commands (export.c) ======================== */

/* :export - Export to MIDI file */
int cmd_export(editor_ctx_t *ctx, const char *args);

/* ======================== Helper for find_command (used by cmd_help) ======================== */

/* Find a command by name (defined in command.c) */
command_def_t* command_find(const char *name);

#endif /* LOKI_COMMAND_IMPL_H */
