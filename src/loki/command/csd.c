/* csd.c - Csound synthesis command (:csd)
 *
 * Toggle Csound synthesis backend.
 *
 * Uses the editor-level loki_csound_* functions which call the shared
 * Csound backend directly. This command is language-agnostic and works
 * regardless of whether Alda or Joy is active.
 */

#include "command_impl.h"
#include "loki/csound.h"

/* :csd - Toggle Csound synthesis */
int cmd_csd(editor_ctx_t *ctx, const char *args) {
    /* Check if Csound backend is available */
    if (!loki_csound_is_available()) {
        editor_set_status_msg(ctx, "Csound not available (build with -DBUILD_CSOUND_BACKEND=ON)");
        return 0;
    }

    if (!args || !args[0]) {
        /* Toggle Csound */
        if (loki_csound_is_enabled()) {
            /* Switch to TSF */
            loki_csound_disable();
            editor_set_status_msg(ctx, "Csound disabled");
        } else {
            /* Switch to Csound */
            if (!loki_csound_has_instruments()) {
                editor_set_status_msg(ctx, "No Csound instruments loaded (use :cs <file.csd> first)");
                return 0;
            }
            if (loki_csound_enable() == 0) {
                editor_set_status_msg(ctx, "Csound enabled");
            } else {
                editor_set_status_msg(ctx, "Failed to enable Csound");
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
        if (!loki_csound_has_instruments()) {
            editor_set_status_msg(ctx, "No Csound instruments loaded (use :cs <file.csd> first)");
            return 0;
        }
        if (loki_csound_enable() == 0) {
            editor_set_status_msg(ctx, "Csound enabled");
            return 1;
        } else {
            editor_set_status_msg(ctx, "Failed to enable Csound");
            return 0;
        }
    } else {
        loki_csound_disable();
        editor_set_status_msg(ctx, "Csound disabled");
        return 1;
    }
}
