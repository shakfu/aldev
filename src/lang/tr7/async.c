/**
 * @file tr7_async.c
 * @brief TR7 async playback - wrapper around shared async service.
 *
 * Provides non-blocking note and chord playback for the TR7 Scheme REPL
 * by delegating to the shared async playback system.
 */

#include "async.h"
#include "async/shared_async.h"

/* ============================================================================
 * Public API - Delegates to shared async service
 * ============================================================================ */

int tr7_async_init(void) {
    return shared_async_init();
}

void tr7_async_cleanup(void) {
    shared_async_cleanup();
}

int tr7_async_play_note(SharedContext* shared, int channel, int pitch,
                        int velocity, int duration_ms) {
    if (!shared) return -1;
    if (pitch < 0 || pitch > 127) return -1;
    if (velocity < 0) velocity = 0;
    if (velocity > 127) velocity = 127;

    SharedAsyncSchedule* sched = shared_async_schedule_new();
    if (!sched) return -1;

    /* Schedule note at time 0 with specified duration */
    shared_async_schedule_note(sched, 0, channel, pitch, velocity, duration_ms);

    int result = shared_async_play(sched, shared);
    shared_async_schedule_free(sched);

    return (result >= 0) ? 0 : -1;
}

int tr7_async_play_chord(SharedContext* shared, int channel,
                         const int* pitches, int count,
                         int velocity, int duration_ms) {
    if (!shared || !pitches || count <= 0) return -1;
    if (velocity < 0) velocity = 0;
    if (velocity > 127) velocity = 127;

    SharedAsyncSchedule* sched = shared_async_schedule_new();
    if (!sched) return -1;

    /* Schedule all notes at time 0 with same duration */
    for (int i = 0; i < count; i++) {
        int pitch = pitches[i];
        if (pitch >= 0 && pitch <= 127) {
            shared_async_schedule_note(sched, 0, channel, pitch, velocity, duration_ms);
        }
    }

    int result = shared_async_play(sched, shared);
    shared_async_schedule_free(sched);

    return (result >= 0) ? 0 : -1;
}

int tr7_async_play_sequence(SharedContext* shared, int channel,
                            const int* pitches, int count,
                            int velocity, int duration_ms) {
    if (!shared || !pitches || count <= 0) return -1;
    if (velocity < 0) velocity = 0;
    if (velocity > 127) velocity = 127;

    SharedAsyncSchedule* sched = shared_async_schedule_new();
    if (!sched) return -1;

    /* Schedule notes sequentially, each starting after the previous ends */
    int time_ms = 0;
    for (int i = 0; i < count; i++) {
        int pitch = pitches[i];
        if (pitch >= 0 && pitch <= 127) {
            shared_async_schedule_note(sched, time_ms, channel, pitch, velocity, duration_ms);
        }
        time_ms += duration_ms;
    }

    int result = shared_async_play(sched, shared);
    shared_async_schedule_free(sched);

    return (result >= 0) ? 0 : -1;
}

void tr7_async_stop(void) {
    shared_async_stop_all();
}

int tr7_async_is_playing(void) {
    return shared_async_active_count() > 0;
}

int tr7_async_wait(int timeout_ms) {
    return shared_async_wait_all(timeout_ms);
}
