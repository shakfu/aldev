/* goto.c - Navigation commands (:goto, :<number>)
 *
 * Commands for cursor movement and navigation.
 */

#include "command_impl.h"

/* :goto, :<number> - Go to line number */
int cmd_goto(editor_ctx_t *ctx, const char *args) {
    if (!args || !args[0]) {
        editor_set_status_msg(ctx, "Usage: :<line> or :goto <line>");
        return 0;
    }

    /* Parse line number */
    int line = atoi(args);
    if (line < 1) {
        editor_set_status_msg(ctx, "Invalid line number: %s", args);
        return 0;
    }

    /* Clamp to valid range (1-indexed for user, 0-indexed internally) */
    if (line > ctx->numrows) {
        line = ctx->numrows;
    }

    /* Move cursor to the line (convert to 0-indexed) */
    ctx->cy = line - 1;
    ctx->cx = 0;

    /* Adjust scroll to show the target line */
    if (ctx->cy < ctx->rowoff) {
        ctx->rowoff = ctx->cy;
    } else if (ctx->cy >= ctx->rowoff + ctx->screenrows - 2) {
        ctx->rowoff = ctx->cy - ctx->screenrows / 2;
        if (ctx->rowoff < 0) ctx->rowoff = 0;
    }

    editor_set_status_msg(ctx, "Line %d", line);
    return 1;
}
