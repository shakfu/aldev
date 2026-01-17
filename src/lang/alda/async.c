/**
 * @file async.c
 * @brief Asynchronous event playback for Alda interpreter.
 *
 * This module is a thin wrapper around the shared async playback service,
 * providing Alda-specific API and converting AldaScheduledEvents to the
 * shared format.
 */

#include "alda/async.h"
#include "alda/scheduler.h"
#include "alda/context.h"
#include "async/shared_async.h"
#include "link/link.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Module State
 * ============================================================================ */

static int g_concurrent_mode = 0;

/* ============================================================================
 * Public API - Delegates to shared async service
 * ============================================================================ */

int alda_async_init(void) {
    return shared_async_init();
}

void alda_async_cleanup(void) {
    shared_async_cleanup();
}

int alda_events_play_async(AldaContext* ctx) {
    if (!ctx) return -1;

    if (ctx->event_count == 0) {
        return 0;  /* Nothing to play */
    }

    /* Check if shared context has output */
    if (!ctx->shared) {
        fprintf(stderr, "No shared context available\n");
        return -1;
    }

    /* In sequential mode, wait for previous playback to complete */
    if (!g_concurrent_mode) {
        shared_async_wait_all(0);
    }

    /* Sort events first */
    alda_events_sort(ctx);

    /* Create shared schedule in tick mode */
    SharedAsyncSchedule* sched = shared_async_schedule_new();
    if (!sched) {
        fprintf(stderr, "Failed to allocate schedule\n");
        return -1;
    }

    /* Set tick mode with initial tempo - use Link tempo if enabled */
    int local_tempo = ctx->global_tempo > 0 ? ctx->global_tempo : ALDA_DEFAULT_TEMPO;
    int tempo = (int)(shared_link_effective_tempo((double)local_tempo) + 0.5);
    shared_async_schedule_set_tick_mode(sched, tempo);

    /* Convert Alda events to shared events */
    /* Note: Alda uses 0-based channels (0-15), shared service uses 1-based (1-16) */
    for (int i = 0; i < ctx->event_count; i++) {
        AldaScheduledEvent* evt = &ctx->events[i];
        int channel = evt->channel + 1;  /* Convert to 1-based */

        switch (evt->type) {
            case ALDA_EVT_NOTE_ON:
                shared_async_schedule_note_on_tick(sched, evt->tick,
                    channel, evt->data1, evt->data2);
                break;

            case ALDA_EVT_NOTE_OFF:
                shared_async_schedule_note_off_tick(sched, evt->tick,
                    channel, evt->data1);
                break;

            case ALDA_EVT_PROGRAM:
                shared_async_schedule_program_tick(sched, evt->tick,
                    channel, evt->data1);
                break;

            case ALDA_EVT_CC:
                shared_async_schedule_cc_tick(sched, evt->tick,
                    channel, evt->data1, evt->data2);
                break;

            case ALDA_EVT_PAN:
                /* Pan is CC 10 */
                shared_async_schedule_cc_tick(sched, evt->tick,
                    channel, 10, evt->data1);
                break;

            case ALDA_EVT_TEMPO:
                shared_async_schedule_tempo(sched, evt->tick, evt->data1);
                break;
        }
    }

    /* Play via shared async */
    int result = shared_async_play(sched, ctx->shared);

    shared_async_schedule_free(sched);

    return (result >= 0) ? 0 : -1;
}

void alda_async_stop(void) {
    shared_async_stop_all();
}

int alda_async_is_playing(void) {
    return shared_async_active_count() > 0;
}

int alda_async_active_count(void) {
    return shared_async_active_count();
}

int alda_async_wait(int timeout_ms) {
    return shared_async_wait_all(timeout_ms);
}

void alda_async_set_concurrent(int enabled) {
    g_concurrent_mode = enabled ? 1 : 0;
}

int alda_async_get_concurrent(void) {
    return g_concurrent_mode;
}
