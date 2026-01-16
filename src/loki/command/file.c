/* file.c - File operation commands (:w, :e)
 *
 * Commands for saving and opening files.
 */

#include "command_impl.h"

/* :w, :write - Save file */
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

/* :e, :edit - Open file */
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
