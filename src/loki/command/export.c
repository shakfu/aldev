/* export.c - MIDI export command (:export)
 *
 * Export Alda compositions to Standard MIDI Files.
 */

#include "command_impl.h"
#include "loki/alda.h"
#include "loki/midi_export.h"

/* :export - Export to MIDI file */
int cmd_export(editor_ctx_t *ctx, const char *args) {
    if (!args || !args[0]) {
        editor_set_status_msg(ctx, "Usage: :export <filename.mid>");
        return 0;
    }

    /* Check if Alda is initialized */
    if (!loki_alda_is_initialized(ctx)) {
        editor_set_status_msg(ctx, "No Alda context (play Alda code first)");
        return 0;
    }

    /* Export to MIDI file */
    if (loki_midi_export(ctx, args) == 0) {
        editor_set_status_msg(ctx, "%s exported", args);
        return 1;
    } else {
        const char *err = loki_midi_export_error();
        editor_set_status_msg(ctx, "Export failed: %s", err ? err : "unknown error");
        return 0;
    }
}
