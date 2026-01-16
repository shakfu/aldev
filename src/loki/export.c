/* export.c - Editor-level MIDI export control
 *
 * Language-agnostic MIDI export implementation.
 * Converts language-specific events to SharedMidiEvent format,
 * then calls the shared exporter.
 */

#include "export.h"
#include "loki/midi_export.h"
#include "midi/events.h"
#include <stddef.h>  /* NULL */

#ifdef LANG_ALDA
#include "alda.h"
#include <alda/context.h>  /* For AldaScheduledEvent, ALDA_TICKS_PER_QUARTER */
#endif

static const char *g_export_error = NULL;

#ifdef LANG_ALDA
/* Convert Alda events to shared MIDI event buffer */
static int populate_from_alda(editor_ctx_t *ctx) {
    int event_count = 0;
    const AldaScheduledEvent *events = loki_alda_get_events(ctx, &event_count);

    if (!events || event_count == 0) {
        return -1;
    }

    /* Initialize shared buffer with Alda's ticks per quarter */
    if (shared_midi_events_init(ALDA_TICKS_PER_QUARTER) != 0) {
        return -1;
    }

    shared_midi_events_clear();

    /* Add initial tempo */
    int tempo = loki_alda_get_tempo(ctx);
    shared_midi_events_tempo(0, tempo);

    /* Convert each Alda event to shared format */
    for (int i = 0; i < event_count; i++) {
        const AldaScheduledEvent *evt = &events[i];

        switch (evt->type) {
            case ALDA_EVT_NOTE_ON:
                shared_midi_events_note_on(evt->tick, evt->channel,
                                           evt->data1, evt->data2);
                break;

            case ALDA_EVT_NOTE_OFF:
                shared_midi_events_note_off(evt->tick, evt->channel, evt->data1);
                break;

            case ALDA_EVT_PROGRAM:
                shared_midi_events_program(evt->tick, evt->channel, evt->data1);
                break;

            case ALDA_EVT_CC:
                shared_midi_events_cc(evt->tick, evt->channel,
                                      evt->data1, evt->data2);
                break;

            case ALDA_EVT_PAN:
                /* Pan is CC #10 */
                shared_midi_events_cc(evt->tick, evt->channel, 10, evt->data1);
                break;

            case ALDA_EVT_TEMPO:
                shared_midi_events_tempo(evt->tick, evt->data1);
                break;
        }
    }

    shared_midi_events_sort();
    return 0;
}
#endif /* LANG_ALDA */

int loki_export_available(editor_ctx_t *ctx) {
#ifdef LANG_ALDA
    /* Check Alda - has event-based model */
    if (loki_alda_is_initialized(ctx)) {
        int event_count = 0;
        const void *events = loki_alda_get_events(ctx, &event_count);
        if (events && event_count > 0) {
            return 1;
        }
    }
#else
    (void)ctx;
#endif

    /* Check shared buffer - might have events from other sources */
    if (shared_midi_events_count() > 0) {
        return 1;
    }

    /* Joy uses immediate playback - no exportable events */
    /* Future languages could be checked here */

    return 0;
}

int loki_export_midi(editor_ctx_t *ctx, const char *filename) {
    g_export_error = NULL;

    if (!filename || !*filename) {
        g_export_error = "No filename specified";
        return -1;
    }

#ifdef LANG_ALDA
    /* Try to populate from Alda if available */
    if (loki_alda_is_initialized(ctx)) {
        int event_count = 0;
        const void *events = loki_alda_get_events(ctx, &event_count);
        if (events && event_count > 0) {
            if (populate_from_alda(ctx) != 0) {
                g_export_error = "Failed to convert events";
                return -1;
            }
        }
    }
#else
    (void)ctx;
#endif

    /* Check if we have events to export */
    if (shared_midi_events_count() == 0) {
        g_export_error = "No events to export (play music code first)";
        return -1;
    }

    /* Export from shared buffer */
    int result = loki_midi_export_shared(filename);
    if (result != 0) {
        g_export_error = loki_midi_export_error();
    }
    return result;
}

const char *loki_export_error(void) {
    return g_export_error;
}
