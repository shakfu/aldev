/**
 * @file shared_async.c
 * @brief Shared asynchronous MIDI playback implementation.
 *
 * Uses libuv for timer-based event dispatch in a background thread.
 * Supports multiple concurrent playback slots for polyphonic layering.
 */

#include "shared_async.h"
#include "context.h"
#include <uv.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Active Note Tracking
 * ============================================================================ */

typedef struct {
    int pitch;
    int channel;
    int off_time_ms;
} ActiveNote;

#define MAX_ACTIVE_NOTES 128

/* ============================================================================
 * Playback Slot
 * ============================================================================ */

typedef struct {
    int active;
    int stop_requested;

    /* Event data (deep copy) */
    SharedAsyncEvent* events;
    size_t event_count;
    size_t event_index;

    /* Timing state */
    int current_time_ms;
    int current_tick;       /* For tick mode */
    int use_ticks;          /* Non-zero if using tick-based timing */
    int tempo;              /* Current tempo in BPM (for tick mode) */

    /* Active note tracking */
    ActiveNote active_notes[MAX_ACTIVE_NOTES];
    int active_note_count;

    /* For tick mode: store note-off times in ticks */
    int active_note_off_ticks[MAX_ACTIVE_NOTES];

    /* Output context (borrowed) */
    SharedContext* ctx;

    /* libuv handles */
    uv_timer_t timer;
    uv_async_t stop_async;
} AsyncSlot;

/* ============================================================================
 * Async System State
 * ============================================================================ */

typedef struct {
    uv_loop_t* loop;
    uv_thread_t thread;
    uv_async_t wake_async;
    uv_mutex_t mutex;

    int running;
    int shutdown_requested;

    AsyncSlot slots[SHARED_ASYNC_MAX_SLOTS];
    int active_count;
} AsyncSystem;

static AsyncSystem g_async = {0};

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void on_timer(uv_timer_t* handle);
static void schedule_next_timer(AsyncSlot* slot);
static void finalize_slot(AsyncSlot* slot);
static void send_event(AsyncSlot* slot, SharedAsyncEvent* evt);

/* ============================================================================
 * Active Note Helpers
 * ============================================================================ */

static void add_active_note(AsyncSlot* slot, int channel, int pitch, int off_time) {
    if (slot->active_note_count >= MAX_ACTIVE_NOTES) {
        /* Overflow - send note-off for oldest */
        if (slot->ctx) {
            shared_send_note_off(slot->ctx, slot->active_notes[0].channel,
                                 slot->active_notes[0].pitch);
        }
        memmove(&slot->active_notes[0], &slot->active_notes[1],
                (MAX_ACTIVE_NOTES - 1) * sizeof(ActiveNote));
        slot->active_note_count = MAX_ACTIVE_NOTES - 1;
    }

    slot->active_notes[slot->active_note_count].pitch = pitch;
    slot->active_notes[slot->active_note_count].channel = channel;
    slot->active_notes[slot->active_note_count].off_time_ms = off_time;
    slot->active_note_count++;
}

static void process_note_offs(AsyncSlot* slot, int up_to_time) {
    if (!slot->ctx) return;

    for (int i = 0; i < slot->active_note_count; ) {
        if (slot->active_notes[i].off_time_ms <= up_to_time) {
            shared_send_note_off(slot->ctx, slot->active_notes[i].channel,
                                 slot->active_notes[i].pitch);
            memmove(&slot->active_notes[i], &slot->active_notes[i + 1],
                    (slot->active_note_count - i - 1) * sizeof(ActiveNote));
            slot->active_note_count--;
        } else {
            i++;
        }
    }
}

static void send_all_note_offs(AsyncSlot* slot) {
    if (!slot->ctx) return;

    for (int i = 0; i < slot->active_note_count; i++) {
        shared_send_note_off(slot->ctx, slot->active_notes[i].channel,
                             slot->active_notes[i].pitch);
    }
    slot->active_note_count = 0;
}

static int find_earliest_note_off(AsyncSlot* slot) {
    if (slot->active_note_count == 0) return -1;

    int earliest = slot->active_notes[0].off_time_ms;
    for (int i = 1; i < slot->active_note_count; i++) {
        if (slot->active_notes[i].off_time_ms < earliest) {
            earliest = slot->active_notes[i].off_time_ms;
        }
    }
    return earliest;
}

/* ============================================================================
 * Event Dispatch
 * ============================================================================ */

static void send_event(AsyncSlot* slot, SharedAsyncEvent* evt) {
    if (!slot->ctx) return;

    switch (evt->type) {
        case SHARED_ASYNC_NOTE:
            shared_send_note_on(slot->ctx, evt->channel, evt->data1, evt->data2);
            if (slot->use_ticks) {
                if (evt->duration_ticks > 0) {
                    /* For tick mode, note-offs are scheduled separately by Alda */
                    /* so we don't add active notes here */
                }
            } else if (evt->duration_ms > 0) {
                add_active_note(slot, evt->channel, evt->data1,
                               evt->time_ms + evt->duration_ms);
            }
            break;

        case SHARED_ASYNC_NOTE_ON:
            shared_send_note_on(slot->ctx, evt->channel, evt->data1, evt->data2);
            break;

        case SHARED_ASYNC_NOTE_OFF:
            shared_send_note_off(slot->ctx, evt->channel, evt->data1);
            break;

        case SHARED_ASYNC_CC:
            shared_send_cc(slot->ctx, evt->channel, evt->data1, evt->data2);
            break;

        case SHARED_ASYNC_PROGRAM:
            shared_send_program(slot->ctx, evt->channel, evt->data1);
            break;

        case SHARED_ASYNC_TEMPO:
            /* Update tempo for subsequent tick-to-ms calculations */
            if (evt->data1 > 0) {
                slot->tempo = evt->data1;
            }
            break;
    }
}

/* ============================================================================
 * Timer Callback
 * ============================================================================ */

static void on_timer(uv_timer_t* handle) {
    AsyncSlot* slot = (AsyncSlot*)handle->data;

    if (slot->stop_requested) {
        send_all_note_offs(slot);
        finalize_slot(slot);
        return;
    }

    if (slot->use_ticks) {
        /* Tick-based timing */
        while (slot->event_index < slot->event_count) {
            SharedAsyncEvent* evt = &slot->events[slot->event_index];

            if (evt->tick > slot->current_tick) {
                break;  /* Future event */
            }

            send_event(slot, evt);
            slot->event_index++;
        }

        /* Check completion */
        if (slot->event_index >= slot->event_count) {
            finalize_slot(slot);
            return;
        }

        schedule_next_timer(slot);
    } else {
        /* Millisecond-based timing */
        /* Process pending note-offs */
        process_note_offs(slot, slot->current_time_ms);

        /* Process events at current time */
        while (slot->event_index < slot->event_count) {
            SharedAsyncEvent* evt = &slot->events[slot->event_index];

            if (evt->time_ms > slot->current_time_ms) {
                break;  /* Future event */
            }

            send_event(slot, evt);
            slot->event_index++;
        }

        /* Check completion */
        if (slot->event_index >= slot->event_count && slot->active_note_count == 0) {
            finalize_slot(slot);
            return;
        }

        schedule_next_timer(slot);
    }
}

static void schedule_next_timer(AsyncSlot* slot) {
    if (slot->stop_requested) {
        send_all_note_offs(slot);
        finalize_slot(slot);
        return;
    }

    if (slot->use_ticks) {
        /* Tick-based timing */
        if (slot->event_index >= slot->event_count) {
            finalize_slot(slot);
            return;
        }

        SharedAsyncEvent* next_evt = &slot->events[slot->event_index];
        int delta_ticks = next_evt->tick - slot->current_tick;
        int delay_ms = shared_async_ticks_to_ms(delta_ticks, slot->tempo);
        if (delay_ms < 0) delay_ms = 0;

        slot->current_tick = next_evt->tick;
        uv_timer_start(&slot->timer, on_timer, delay_ms, 0);
    } else {
        /* Millisecond-based timing */
        int next_time = -1;

        /* Next event time */
        if (slot->event_index < slot->event_count) {
            next_time = slot->events[slot->event_index].time_ms;
        }

        /* Earliest note-off */
        int earliest_off = find_earliest_note_off(slot);
        if (earliest_off >= 0) {
            if (next_time < 0 || earliest_off < next_time) {
                next_time = earliest_off;
            }
        }

        if (next_time < 0) {
            finalize_slot(slot);
            return;
        }

        int delay_ms = next_time - slot->current_time_ms;
        if (delay_ms < 0) delay_ms = 0;

        slot->current_time_ms = next_time;
        uv_timer_start(&slot->timer, on_timer, delay_ms, 0);
    }
}

static void finalize_slot(AsyncSlot* slot) {
    uv_mutex_lock(&g_async.mutex);
    if (slot->active) {
        slot->active = 0;
        g_async.active_count--;
    }
    uv_mutex_unlock(&g_async.mutex);
}

/* ============================================================================
 * Stop Handler
 * ============================================================================ */

static void on_stop_signal(uv_async_t* handle) {
    AsyncSlot* slot = (AsyncSlot*)handle->data;
    slot->stop_requested = 1;
    uv_timer_stop(&slot->timer);
    send_all_note_offs(slot);

    uv_mutex_lock(&g_async.mutex);
    if (slot->active) {
        slot->active = 0;
        g_async.active_count--;
    }
    uv_mutex_unlock(&g_async.mutex);
}

/* ============================================================================
 * Wake Handler
 * ============================================================================ */

static void on_wake(uv_async_t* handle) {
    (void)handle;
    if (g_async.shutdown_requested) {
        uv_stop(g_async.loop);
    }
}

/* ============================================================================
 * Event Loop Thread
 * ============================================================================ */

static void async_thread_fn(void* arg) {
    (void)arg;

    while (!g_async.shutdown_requested) {
        uv_run(g_async.loop, UV_RUN_DEFAULT);

        if (!g_async.shutdown_requested) {
            uv_sleep(10);
        }
    }

    g_async.running = 0;
}

/* ============================================================================
 * Event Comparison for Sorting
 * ============================================================================ */

/* Flag to indicate sorting mode - set before qsort call */
static int g_sort_by_ticks = 0;

static int compare_events(const void* a, const void* b) {
    const SharedAsyncEvent* ea = (const SharedAsyncEvent*)a;
    const SharedAsyncEvent* eb = (const SharedAsyncEvent*)b;

    int time_diff;
    if (g_sort_by_ticks) {
        time_diff = ea->tick - eb->tick;
    } else {
        time_diff = ea->time_ms - eb->time_ms;
    }

    if (time_diff != 0) return time_diff;

    /* At same time: note-offs before note-ons */
    int type_a = (ea->type == SHARED_ASYNC_NOTE_OFF) ? 0 : 1;
    int type_b = (eb->type == SHARED_ASYNC_NOTE_OFF) ? 0 : 1;
    return type_a - type_b;
}

/* ============================================================================
 * Schedule Management
 * ============================================================================ */

SharedAsyncSchedule* shared_async_schedule_new(void) {
    SharedAsyncSchedule* sched = malloc(sizeof(SharedAsyncSchedule));
    if (!sched) return NULL;

    sched->events = NULL;
    sched->count = 0;
    sched->capacity = 0;
    sched->total_duration_ms = 0;
    sched->use_ticks = 0;
    sched->initial_tempo = SHARED_ASYNC_DEFAULT_TEMPO;
    return sched;
}

void shared_async_schedule_free(SharedAsyncSchedule* sched) {
    if (sched) {
        free(sched->events);
        free(sched);
    }
}

static void schedule_grow(SharedAsyncSchedule* sched) {
    if (sched->count >= sched->capacity) {
        size_t new_cap = sched->capacity == 0 ? 64 : sched->capacity * 2;
        sched->events = realloc(sched->events, new_cap * sizeof(SharedAsyncEvent));
        sched->capacity = new_cap;
    }
}

static void schedule_update_duration(SharedAsyncSchedule* sched, int end_time) {
    if (end_time > sched->total_duration_ms) {
        sched->total_duration_ms = end_time;
    }
}

void shared_async_schedule_note(SharedAsyncSchedule* sched, int time_ms,
                                 int channel, int pitch, int velocity,
                                 int duration_ms) {
    if (!sched) return;
    schedule_grow(sched);

    SharedAsyncEvent* evt = &sched->events[sched->count++];
    evt->time_ms = time_ms;
    evt->tick = 0;
    evt->type = SHARED_ASYNC_NOTE;
    evt->channel = channel;
    evt->data1 = pitch;
    evt->data2 = velocity;
    evt->duration_ms = duration_ms;
    evt->duration_ticks = 0;

    schedule_update_duration(sched, time_ms + duration_ms);
}

void shared_async_schedule_note_on(SharedAsyncSchedule* sched, int time_ms,
                                    int channel, int pitch, int velocity) {
    if (!sched) return;
    schedule_grow(sched);

    SharedAsyncEvent* evt = &sched->events[sched->count++];
    evt->time_ms = time_ms;
    evt->tick = 0;
    evt->type = SHARED_ASYNC_NOTE_ON;
    evt->channel = channel;
    evt->data1 = pitch;
    evt->data2 = velocity;
    evt->duration_ms = 0;
    evt->duration_ticks = 0;

    schedule_update_duration(sched, time_ms);
}

void shared_async_schedule_note_off(SharedAsyncSchedule* sched, int time_ms,
                                     int channel, int pitch) {
    if (!sched) return;
    schedule_grow(sched);

    SharedAsyncEvent* evt = &sched->events[sched->count++];
    evt->time_ms = time_ms;
    evt->tick = 0;
    evt->type = SHARED_ASYNC_NOTE_OFF;
    evt->channel = channel;
    evt->data1 = pitch;
    evt->data2 = 0;
    evt->duration_ms = 0;
    evt->duration_ticks = 0;

    schedule_update_duration(sched, time_ms);
}

void shared_async_schedule_cc(SharedAsyncSchedule* sched, int time_ms,
                               int channel, int cc, int value) {
    if (!sched) return;
    schedule_grow(sched);

    SharedAsyncEvent* evt = &sched->events[sched->count++];
    evt->time_ms = time_ms;
    evt->tick = 0;
    evt->type = SHARED_ASYNC_CC;
    evt->channel = channel;
    evt->data1 = cc;
    evt->data2 = value;
    evt->duration_ms = 0;
    evt->duration_ticks = 0;

    schedule_update_duration(sched, time_ms);
}

void shared_async_schedule_program(SharedAsyncSchedule* sched, int time_ms,
                                    int channel, int program) {
    if (!sched) return;
    schedule_grow(sched);

    SharedAsyncEvent* evt = &sched->events[sched->count++];
    evt->time_ms = time_ms;
    evt->tick = 0;
    evt->type = SHARED_ASYNC_PROGRAM;
    evt->channel = channel;
    evt->data1 = program;
    evt->data2 = 0;
    evt->duration_ms = 0;
    evt->duration_ticks = 0;

    schedule_update_duration(sched, time_ms);
}

/* ============================================================================
 * Tick-Based Schedule Helpers
 * ============================================================================ */

int shared_async_ticks_to_ms(int ticks, int tempo) {
    if (tempo <= 0) tempo = SHARED_ASYNC_DEFAULT_TEMPO;
    /* ms = ticks * (60000 / tempo) / TICKS_PER_QUARTER */
    return (int)((double)ticks * 60000.0 / (double)tempo / SHARED_ASYNC_TICKS_PER_QUARTER);
}

void shared_async_schedule_set_tick_mode(SharedAsyncSchedule* sched, int initial_tempo) {
    if (!sched) return;
    sched->use_ticks = 1;
    sched->initial_tempo = initial_tempo > 0 ? initial_tempo : SHARED_ASYNC_DEFAULT_TEMPO;
}

void shared_async_schedule_note_on_tick(SharedAsyncSchedule* sched, int tick,
                                         int channel, int pitch, int velocity) {
    if (!sched) return;
    schedule_grow(sched);

    SharedAsyncEvent* evt = &sched->events[sched->count++];
    evt->tick = tick;
    evt->time_ms = 0;
    evt->type = SHARED_ASYNC_NOTE_ON;
    evt->channel = channel;
    evt->data1 = pitch;
    evt->data2 = velocity;
    evt->duration_ticks = 0;
    evt->duration_ms = 0;
}

void shared_async_schedule_note_off_tick(SharedAsyncSchedule* sched, int tick,
                                          int channel, int pitch) {
    if (!sched) return;
    schedule_grow(sched);

    SharedAsyncEvent* evt = &sched->events[sched->count++];
    evt->tick = tick;
    evt->time_ms = 0;
    evt->type = SHARED_ASYNC_NOTE_OFF;
    evt->channel = channel;
    evt->data1 = pitch;
    evt->data2 = 0;
    evt->duration_ticks = 0;
    evt->duration_ms = 0;
}

void shared_async_schedule_cc_tick(SharedAsyncSchedule* sched, int tick,
                                    int channel, int cc, int value) {
    if (!sched) return;
    schedule_grow(sched);

    SharedAsyncEvent* evt = &sched->events[sched->count++];
    evt->tick = tick;
    evt->time_ms = 0;
    evt->type = SHARED_ASYNC_CC;
    evt->channel = channel;
    evt->data1 = cc;
    evt->data2 = value;
    evt->duration_ticks = 0;
    evt->duration_ms = 0;
}

void shared_async_schedule_program_tick(SharedAsyncSchedule* sched, int tick,
                                         int channel, int program) {
    if (!sched) return;
    schedule_grow(sched);

    SharedAsyncEvent* evt = &sched->events[sched->count++];
    evt->tick = tick;
    evt->time_ms = 0;
    evt->type = SHARED_ASYNC_PROGRAM;
    evt->channel = channel;
    evt->data1 = program;
    evt->data2 = 0;
    evt->duration_ticks = 0;
    evt->duration_ms = 0;
}

void shared_async_schedule_tempo(SharedAsyncSchedule* sched, int tick, int tempo) {
    if (!sched) return;
    schedule_grow(sched);

    SharedAsyncEvent* evt = &sched->events[sched->count++];
    evt->tick = tick;
    evt->time_ms = 0;
    evt->type = SHARED_ASYNC_TEMPO;
    evt->channel = 0;
    evt->data1 = tempo > 0 ? tempo : SHARED_ASYNC_DEFAULT_TEMPO;
    evt->data2 = 0;
    evt->duration_ticks = 0;
    evt->duration_ms = 0;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

int shared_async_init(void) {
    if (g_async.loop != NULL) {
        return 0;  /* Already initialized */
    }

    g_async.loop = malloc(sizeof(uv_loop_t));
    if (!g_async.loop) return -1;

    if (uv_loop_init(g_async.loop) != 0) {
        free(g_async.loop);
        g_async.loop = NULL;
        return -1;
    }

    uv_mutex_init(&g_async.mutex);
    uv_async_init(g_async.loop, &g_async.wake_async, on_wake);

    /* Initialize all slots */
    for (int i = 0; i < SHARED_ASYNC_MAX_SLOTS; i++) {
        AsyncSlot* slot = &g_async.slots[i];
        slot->active = 0;
        slot->events = NULL;
        slot->event_count = 0;
        slot->active_note_count = 0;

        uv_timer_init(g_async.loop, &slot->timer);
        slot->timer.data = slot;

        uv_async_init(g_async.loop, &slot->stop_async, on_stop_signal);
        slot->stop_async.data = slot;
    }

    /* Start thread */
    g_async.running = 1;
    g_async.shutdown_requested = 0;
    g_async.active_count = 0;

    if (uv_thread_create(&g_async.thread, async_thread_fn, NULL) != 0) {
        g_async.running = 0;
        uv_loop_close(g_async.loop);
        free(g_async.loop);
        g_async.loop = NULL;
        return -1;
    }

    return 0;
}

void shared_async_cleanup(void) {
    if (!g_async.loop) return;

    shared_async_stop_all();

    g_async.shutdown_requested = 1;
    uv_async_send(&g_async.wake_async);

    if (g_async.running) {
        uv_thread_join(&g_async.thread);
    }

    for (int i = 0; i < SHARED_ASYNC_MAX_SLOTS; i++) {
        AsyncSlot* slot = &g_async.slots[i];
        uv_close((uv_handle_t*)&slot->timer, NULL);
        uv_close((uv_handle_t*)&slot->stop_async, NULL);

        if (slot->events) {
            free(slot->events);
            slot->events = NULL;
        }
    }
    uv_close((uv_handle_t*)&g_async.wake_async, NULL);

    uv_run(g_async.loop, UV_RUN_DEFAULT);

    uv_mutex_destroy(&g_async.mutex);
    uv_loop_close(g_async.loop);
    free(g_async.loop);
    g_async.loop = NULL;
}

int shared_async_play(SharedAsyncSchedule* sched, SharedContext* ctx) {
    if (!sched || sched->count == 0) return -1;
    if (!ctx) return -1;

    /* Check output availability */
    if (!ctx->midi_out && !ctx->tsf_enabled && !ctx->csound_enabled) {
        return -1;
    }

    /* Initialize if needed */
    if (!g_async.loop) {
        if (shared_async_init() != 0) return -1;
    }

    /* Find free slot */
    uv_mutex_lock(&g_async.mutex);

    int slot_id = -1;
    for (int i = 0; i < SHARED_ASYNC_MAX_SLOTS; i++) {
        if (!g_async.slots[i].active) {
            slot_id = i;
            break;
        }
    }

    if (slot_id < 0) {
        uv_mutex_unlock(&g_async.mutex);
        fprintf(stderr, "shared_async_play: no free slots\n");
        return -1;
    }

    AsyncSlot* slot = &g_async.slots[slot_id];

    /* Free previous events */
    if (slot->events) {
        free(slot->events);
        slot->events = NULL;
    }

    /* Deep copy events */
    slot->events = malloc(sched->count * sizeof(SharedAsyncEvent));
    if (!slot->events) {
        uv_mutex_unlock(&g_async.mutex);
        return -1;
    }

    memcpy(slot->events, sched->events, sched->count * sizeof(SharedAsyncEvent));
    slot->event_count = sched->count;

    /* Set timing mode */
    slot->use_ticks = sched->use_ticks;
    slot->tempo = sched->initial_tempo > 0 ? sched->initial_tempo : SHARED_ASYNC_DEFAULT_TEMPO;

    /* Sort by time (set global flag for comparison function) */
    g_sort_by_ticks = sched->use_ticks;
    qsort(slot->events, slot->event_count, sizeof(SharedAsyncEvent), compare_events);

    /* Initialize state */
    slot->event_index = 0;
    slot->current_time_ms = 0;
    slot->current_tick = 0;
    slot->active_note_count = 0;
    slot->stop_requested = 0;
    slot->ctx = ctx;
    slot->active = 1;
    g_async.active_count++;

    uv_mutex_unlock(&g_async.mutex);

    /* Schedule first timer */
    int first_delay = 0;
    if (slot->event_count > 0) {
        if (slot->use_ticks) {
            int first_tick = slot->events[0].tick;
            if (first_tick > 0) {
                first_delay = shared_async_ticks_to_ms(first_tick, slot->tempo);
                slot->current_tick = first_tick;
            }
        } else {
            if (slot->events[0].time_ms > 0) {
                first_delay = slot->events[0].time_ms;
                slot->current_time_ms = slot->events[0].time_ms;
            }
        }
    }

    uv_timer_start(&slot->timer, on_timer, first_delay, 0);
    uv_async_send(&g_async.wake_async);

    return slot_id;
}

void shared_async_stop(int slot_id) {
    if (!g_async.loop) return;

    if (slot_id < 0 || slot_id >= SHARED_ASYNC_MAX_SLOTS) {
        shared_async_stop_all();
        return;
    }

    AsyncSlot* slot = &g_async.slots[slot_id];
    if (slot->active) {
        slot->stop_requested = 1;
        uv_async_send(&slot->stop_async);
    }
}

void shared_async_stop_all(void) {
    if (!g_async.loop) return;

    for (int i = 0; i < SHARED_ASYNC_MAX_SLOTS; i++) {
        if (g_async.slots[i].active) {
            g_async.slots[i].stop_requested = 1;
            uv_async_send(&g_async.slots[i].stop_async);
        }
    }
}

int shared_async_active_count(void) {
    if (!g_async.loop) return 0;

    uv_mutex_lock(&g_async.mutex);
    int count = g_async.active_count;
    uv_mutex_unlock(&g_async.mutex);

    return count;
}

int shared_async_is_slot_playing(int slot_id) {
    if (!g_async.loop) return 0;
    if (slot_id < 0 || slot_id >= SHARED_ASYNC_MAX_SLOTS) return 0;

    uv_mutex_lock(&g_async.mutex);
    int active = g_async.slots[slot_id].active;
    uv_mutex_unlock(&g_async.mutex);

    return active;
}

int shared_async_wait_all(int timeout_ms) {
    if (!g_async.loop) return 0;

    int waited = 0;
    int interval = 10;

    while (shared_async_active_count() > 0) {
        uv_sleep(interval);
        waited += interval;

        if (timeout_ms > 0 && waited >= timeout_ms) {
            return -1;
        }
    }

    return 0;
}

int shared_async_wait(int slot_id, int timeout_ms) {
    if (!g_async.loop) return 0;
    if (slot_id < 0 || slot_id >= SHARED_ASYNC_MAX_SLOTS) return 0;

    int waited = 0;
    int interval = 10;

    while (shared_async_is_slot_playing(slot_id)) {
        uv_sleep(interval);
        waited += interval;

        if (timeout_ms > 0 && waited >= timeout_ms) {
            return -1;
        }
    }

    return 0;
}
