/**
 * @file joy_async.c
 * @brief Joy async playback - wrapper around shared async service.
 *
 * Provides Joy-specific API that maps to the shared async playback system.
 */

#include "joy_async.h"
#include "midi_primitives.h"
#include "music_context.h"
#include "async/shared_async.h"
#include "link/link.h"
#include <stdlib.h>

/* ============================================================================
 * Public API - Delegates to shared async service
 * ============================================================================ */

int joy_async_init(void) {
    return shared_async_init();
}

void joy_async_cleanup(void) {
    shared_async_cleanup();
}

int joy_async_play(MidiSchedule* sched, MusicContext* mctx) {
    if (!sched || sched->count == 0) return 0;
    if (!mctx || !mctx->shared) return -1;

    /* Calculate tempo scaling factor if Link is enabled */
    double tempo_scale = 1.0;
    if (shared_link_is_enabled() && mctx->tempo > 0) {
        double link_tempo = shared_link_get_tempo();
        if (link_tempo > 0) {
            /* Scale timings: faster tempo = shorter durations */
            tempo_scale = (double)mctx->tempo / link_tempo;
        }
    }

    /* Convert Joy's MidiSchedule to SharedAsyncSchedule */
    SharedAsyncSchedule* async_sched = shared_async_schedule_new();
    if (!async_sched) return -1;

    for (size_t i = 0; i < sched->count; i++) {
        ScheduledEvent* ev = &sched->events[i];
        /* Skip rests (pitch == -1) */
        if (ev->pitch >= 0) {
            /* Scale timings by Link tempo ratio */
            int time_ms = (int)(ev->time_ms * tempo_scale + 0.5);
            int duration_ms = (int)(ev->duration_ms * tempo_scale + 0.5);
            shared_async_schedule_note(async_sched, time_ms, ev->channel,
                                        ev->pitch, ev->velocity, duration_ms);
        }
    }

    /* Play via shared async (returns slot ID or -1) */
    int result = shared_async_play(async_sched, mctx->shared);

    shared_async_schedule_free(async_sched);

    return (result >= 0) ? 0 : -1;
}

void joy_async_stop(void) {
    shared_async_stop_all();
}

int joy_async_is_playing(void) {
    return shared_async_active_count() > 0;
}

int joy_async_wait(int timeout_ms) {
    return shared_async_wait_all(timeout_ms);
}
