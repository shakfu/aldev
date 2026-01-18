/* live_loop.c - Live looping implementation
 *
 * Manages per-buffer loops that re-evaluate on Link beat boundaries.
 */

#include "live_loop.h"
#include "buffers.h"
#include "loki/link.h"
#include "lang_bridge.h"
#include "async_queue.h"
#include "internal.h"
#include <math.h>
#include <string.h>

/* Per-buffer loop state */
typedef struct {
    int buffer_id;        /* Buffer this loop belongs to */
    int active;           /* Loop is running */
    double beat_interval; /* Beats between evaluations */
    double last_beat;     /* Last beat position we fired on */
} LoopEntry;

/* Global loop registry */
static LoopEntry g_loops[LIVE_LOOP_MAX];
static int g_loop_count = 0;
static int g_initialized = 0;

/* Find loop entry for buffer, or NULL if not found */
static LoopEntry *find_loop(int buffer_id) {
    for (int i = 0; i < g_loop_count; i++) {
        if (g_loops[i].buffer_id == buffer_id) {
            return &g_loops[i];
        }
    }
    return NULL;
}

/* Remove loop entry (swap with last and decrement count) */
static void remove_loop(LoopEntry *entry) {
    if (!entry) return;

    int idx = (int)(entry - g_loops);
    if (idx < 0 || idx >= g_loop_count) return;

    /* Swap with last entry if not already last */
    if (idx < g_loop_count - 1) {
        g_loops[idx] = g_loops[g_loop_count - 1];
    }
    g_loop_count--;
}

int live_loop_start(editor_ctx_t *ctx, double beats) {
    if (!ctx || beats <= 0) return -1;

    int buf_id = buffer_get_current_id();
    if (buf_id < 0) return -1;

    /* Check if already looping - update interval */
    LoopEntry *existing = find_loop(buf_id);
    if (existing) {
        existing->beat_interval = beats;
        existing->active = 1;
        /* Reset last_beat to current position to start fresh */
        if (loki_link_is_enabled(ctx)) {
            existing->last_beat = loki_link_get_beat(ctx, beats);
        } else {
            existing->last_beat = 0;
        }
        return 0;
    }

    /* Add new loop */
    if (g_loop_count >= LIVE_LOOP_MAX) return -1;

    LoopEntry *entry = &g_loops[g_loop_count];
    entry->buffer_id = buf_id;
    entry->active = 1;
    entry->beat_interval = beats;

    /* Initialize last_beat to current Link position */
    if (loki_link_is_enabled(ctx)) {
        entry->last_beat = loki_link_get_beat(ctx, beats);
    } else {
        entry->last_beat = 0;
    }

    g_loop_count++;
    g_initialized = 1;

    return 0;
}

void live_loop_stop(editor_ctx_t *ctx) {
    (void)ctx;
    int buf_id = buffer_get_current_id();
    live_loop_stop_buffer(buf_id);
}

void live_loop_stop_buffer(int buffer_id) {
    LoopEntry *entry = find_loop(buffer_id);
    if (entry) {
        remove_loop(entry);
    }
}

int live_loop_is_active(editor_ctx_t *ctx) {
    (void)ctx;
    int buf_id = buffer_get_current_id();
    return live_loop_is_active_buffer(buf_id);
}

int live_loop_is_active_buffer(int buffer_id) {
    LoopEntry *entry = find_loop(buffer_id);
    return (entry && entry->active) ? 1 : 0;
}

double live_loop_get_interval(editor_ctx_t *ctx) {
    (void)ctx;
    int buf_id = buffer_get_current_id();
    LoopEntry *entry = find_loop(buf_id);
    return (entry && entry->active) ? entry->beat_interval : 0.0;
}

void live_loop_tick(void) {
    if (g_loop_count == 0 || !g_initialized) return;

    /* Get any context to check Link state */
    editor_ctx_t *current = buffer_get_current();
    if (!current) return;

    /* Link must be enabled for beat-synced loops */
    if (!loki_link_is_enabled(current)) return;

    /* Process each active loop */
    for (int i = 0; i < g_loop_count; i++) {
        LoopEntry *entry = &g_loops[i];
        if (!entry->active) continue;

        /* Get the buffer context for this loop */
        editor_ctx_t *ctx = buffer_get(entry->buffer_id);
        if (!ctx) {
            /* Buffer was closed, remove loop */
            remove_loop(entry);
            i--;  /* Reprocess this index since we swapped */
            continue;
        }

        double interval = entry->beat_interval;
        double current_beat = loki_link_get_beat(ctx, interval);

        /* Check if we crossed a beat boundary.
         * We fire when floor(current_beat / interval) > floor(last_beat / interval)
         * This triggers once per interval, on the downbeat. */
        double current_cycle = floor(current_beat / interval);
        double last_cycle = floor(entry->last_beat / interval);

        if (current_cycle > last_cycle) {
            /* Push beat boundary event to async queue.
             * The handler will call loki_lang_eval_buffer(). */
            async_queue_push_beat(NULL, current_beat, interval, entry->buffer_id);
        }

        entry->last_beat = current_beat;
    }
}

void live_loop_shutdown(void) {
    g_loop_count = 0;
    g_initialized = 0;
    memset(g_loops, 0, sizeof(g_loops));
}
