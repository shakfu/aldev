/* csd.c - Csound synthesis command (:csd)
 *
 * Toggle Csound synthesis backend.
 */

#include "command_impl.h"
#include "loki/alda.h"

/* :csd - Toggle Csound synthesis */
int cmd_csd(editor_ctx_t *ctx, const char *args) {
    /* Check if Csound backend is available */
    if (!loki_alda_csound_is_available()) {
        editor_set_status_msg(ctx, "Csound not available (build with -DBUILD_CSOUND_BACKEND=ON)");
        return 0;
    }

    /* Initialize Alda if needed */
    if (!loki_alda_is_initialized(ctx)) {
        if (loki_alda_init(ctx, NULL) != 0) {
            editor_set_status_msg(ctx, "Failed to initialize Alda");
            return 0;
        }
    }

    if (!args || !args[0]) {
        /* Toggle Csound */
        int csound_enabled = loki_alda_csound_is_enabled(ctx);
        if (csound_enabled) {
            /* Switch to TSF */
            loki_alda_csound_set_enabled(ctx, 0);
            loki_alda_set_synth_enabled(ctx, 1);
            editor_set_status_msg(ctx, "Switched to TinySoundFont");
        } else {
            /* Switch to Csound */
            if (loki_alda_csound_set_enabled(ctx, 1) == 0) {
                editor_set_status_msg(ctx, "Switched to Csound");
            } else {
                editor_set_status_msg(ctx, "Failed to enable Csound (load .csd first)");
                return 0;
            }
        }
        return 1;
    }

    /* Parse argument */
    int enable = -1;
    if (strcmp(args, "1") == 0 || strcasecmp(args, "on") == 0) {
        enable = 1;
    } else if (strcmp(args, "0") == 0 || strcasecmp(args, "off") == 0) {
        enable = 0;
    } else {
        editor_set_status_msg(ctx, "Usage: :csd [on|off|1|0]");
        return 0;
    }

    if (enable) {
        if (loki_alda_csound_set_enabled(ctx, 1) == 0) {
            editor_set_status_msg(ctx, "Csound enabled");
            return 1;
        } else {
            editor_set_status_msg(ctx, "Failed to enable Csound (load .csd first)");
            return 0;
        }
    } else {
        loki_alda_csound_set_enabled(ctx, 0);
        loki_alda_set_synth_enabled(ctx, 1);
        editor_set_status_msg(ctx, "Csound disabled, using TinySoundFont");
        return 1;
    }
}
