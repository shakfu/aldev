/* link.c - Ableton Link command (:link)
 *
 * Toggle and control Ableton Link tempo synchronization.
 */

#include "command_impl.h"
#include "loki/link.h"

/* :link - Toggle Ableton Link */
int cmd_link(editor_ctx_t *ctx, const char *args) {
    /* Initialize Link if not already done */
    if (!loki_link_is_initialized(ctx)) {
        if (loki_link_init(ctx, 120.0) != 0) {
            editor_set_status_msg(ctx, "Failed to initialize Link");
            return 0;
        }
    }

    if (!args || !args[0]) {
        /* Toggle Link */
        int enabled = loki_link_is_enabled(ctx);
        loki_link_enable(ctx, !enabled);
        editor_set_status_msg(ctx, "Link %s (%.1f BPM, %lu peers)",
            !enabled ? "enabled" : "disabled",
            loki_link_get_tempo(ctx),
            (unsigned long)loki_link_num_peers(ctx));
        return 1;
    }

    /* Parse argument */
    int enable = -1;
    if (strcmp(args, "1") == 0 || strcasecmp(args, "on") == 0) {
        enable = 1;
    } else if (strcmp(args, "0") == 0 || strcasecmp(args, "off") == 0) {
        enable = 0;
    } else {
        editor_set_status_msg(ctx, "Usage: :link [on|off|1|0]");
        return 0;
    }

    loki_link_enable(ctx, enable);
    editor_set_status_msg(ctx, "Link %s (%.1f BPM, %lu peers)",
        enable ? "enabled" : "disabled",
        loki_link_get_tempo(ctx),
        (unsigned long)loki_link_num_peers(ctx));
    return 1;
}
