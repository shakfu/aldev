/* loki_alda.c - Alda music language integration for Loki
 *
 * Integrates the Alda music notation language with Loki editor for livecoding.
 * Uses alda's libuv-based async system with a polling callback mechanism.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#include "alda.h"
#include "internal.h"
#include "loki/link.h"

/* Alda library headers */
#include <alda/alda.h>
#include <alda/csound_backend.h>

/* Lua headers for callbacks */
#include "lua.h"
#include "lauxlib.h"

/* ======================= Internal State ======================= */

/* Playback slot for tracking async operations */
typedef struct {
    int active;                  /* Slot in use */
    int playing;                 /* Currently playing */
    int completed;               /* Playback finished, callback pending */
    LokiAldaStatus status;       /* Final status */
    char *lua_callback;          /* Callback function name (owned) */
    char *error_msg;             /* Error message if any (owned) */
    int events_played;           /* Number of events played */
    int duration_ms;             /* Playback duration */
    time_t start_time;           /* Start timestamp */
} AldaPlaybackSlot;

/* Global alda state (one per editor) */
typedef struct {
    int initialized;
    AldaContext alda_ctx;        /* The alda interpreter context */
    AldaPlaybackSlot slots[LOKI_ALDA_MAX_SLOTS];
    char last_error[256];        /* Last error message */
    pthread_mutex_t mutex;       /* Protect concurrent access */
} LokiAldaState;

/* Static state - could be moved to editor_ctx in the future */
static LokiAldaState g_alda_state = {0};

/* ======================= Helper Functions ======================= */

static void set_error(const char *msg) {
    if (msg) {
        strncpy(g_alda_state.last_error, msg, sizeof(g_alda_state.last_error) - 1);
        g_alda_state.last_error[sizeof(g_alda_state.last_error) - 1] = '\0';
    } else {
        g_alda_state.last_error[0] = '\0';
    }
}

static int find_free_slot(void) {
    for (int i = 0; i < LOKI_ALDA_MAX_SLOTS; i++) {
        if (!g_alda_state.slots[i].active) {
            return i;
        }
    }
    return -1;
}

static void clear_slot(int slot_id) {
    if (slot_id < 0 || slot_id >= LOKI_ALDA_MAX_SLOTS) return;

    AldaPlaybackSlot *slot = &g_alda_state.slots[slot_id];
    free(slot->lua_callback);
    free(slot->error_msg);
    memset(slot, 0, sizeof(AldaPlaybackSlot));
}

/* ======================= Initialization ======================= */

int loki_alda_init(editor_ctx_t *ctx, const char *port_name) {
    (void)ctx;  /* May use editor context in future */

    if (g_alda_state.initialized) {
        set_error("Alda already initialized");
        return -1;
    }

    /* Initialize mutex */
    if (pthread_mutex_init(&g_alda_state.mutex, NULL) != 0) {
        set_error("Failed to initialize mutex");
        return -1;
    }

    /* Initialize alda context */
    alda_context_init(&g_alda_state.alda_ctx);

    /* Initialize async system (creates libuv thread) */
    if (alda_async_init() != 0) {
        set_error("Failed to initialize async playback");
        alda_context_cleanup(&g_alda_state.alda_ctx);
        pthread_mutex_destroy(&g_alda_state.mutex);
        return -1;
    }

    /* Enable concurrent mode for livecoding */
    alda_async_set_concurrent(1);

    /* Open MIDI output */
    const char *name = port_name ? port_name : "Loki";
    if (alda_midi_open_auto(&g_alda_state.alda_ctx, name) != 0) {
        set_error("Failed to open MIDI output");
        alda_async_cleanup();
        alda_context_cleanup(&g_alda_state.alda_ctx);
        pthread_mutex_destroy(&g_alda_state.mutex);
        return -1;
    }

    /* Initialize TinySoundFont backend */
    alda_tsf_init();

    /* Clear all slots */
    memset(g_alda_state.slots, 0, sizeof(g_alda_state.slots));

    g_alda_state.initialized = 1;
    set_error(NULL);

    return 0;
}

void loki_alda_cleanup(editor_ctx_t *ctx) {
    (void)ctx;

    if (!g_alda_state.initialized) return;

    /* Stop all playback */
    alda_async_stop();

    /* Clean up TinySoundFont */
    alda_tsf_cleanup();

    /* Clean up async system */
    alda_async_cleanup();

    /* Clean up MIDI */
    alda_midi_cleanup(&g_alda_state.alda_ctx);

    /* Clean up context */
    alda_context_cleanup(&g_alda_state.alda_ctx);

    /* Clear all slots */
    for (int i = 0; i < LOKI_ALDA_MAX_SLOTS; i++) {
        clear_slot(i);
    }

    /* Destroy mutex */
    pthread_mutex_destroy(&g_alda_state.mutex);

    g_alda_state.initialized = 0;
}

int loki_alda_is_initialized(editor_ctx_t *ctx) {
    (void)ctx;
    return g_alda_state.initialized;
}

/* ======================= Playback Control ======================= */

int loki_alda_eval_async(editor_ctx_t *ctx, const char *code, const char *lua_callback) {
    (void)ctx;

    if (!g_alda_state.initialized) {
        set_error("Alda not initialized");
        return -1;
    }

    if (!code || !*code) {
        set_error("Empty code");
        return -2;
    }

    pthread_mutex_lock(&g_alda_state.mutex);

    /* Find a free slot */
    int slot_id = find_free_slot();
    if (slot_id < 0) {
        pthread_mutex_unlock(&g_alda_state.mutex);
        set_error("No free playback slots");
        return -1;
    }

    AldaPlaybackSlot *slot = &g_alda_state.slots[slot_id];

    /* Reset context for new evaluation (keeps MIDI connection) */
    alda_context_reset(&g_alda_state.alda_ctx);

    /* Parse and interpret the code */
    if (alda_interpret_string(&g_alda_state.alda_ctx, code, "<loki>") != 0) {
        pthread_mutex_unlock(&g_alda_state.mutex);
        set_error("Parse error in Alda code");
        return -2;
    }

    /* Check if we have any events to play */
    if (g_alda_state.alda_ctx.event_count == 0) {
        pthread_mutex_unlock(&g_alda_state.mutex);
        set_error("No events generated");
        return -2;
    }

    /* Sort events for playback */
    alda_events_sort(&g_alda_state.alda_ctx);

    /* Use Link tempo if enabled */
    double effective_tempo = loki_link_effective_tempo(
        ctx, (double)g_alda_state.alda_ctx.global_tempo);
    g_alda_state.alda_ctx.global_tempo = (int)effective_tempo;

    /* Set up slot */
    slot->active = 1;
    slot->playing = 1;
    slot->completed = 0;
    slot->status = LOKI_ALDA_STATUS_PLAYING;
    slot->lua_callback = lua_callback ? strdup(lua_callback) : NULL;
    slot->error_msg = NULL;
    slot->events_played = g_alda_state.alda_ctx.event_count;
    slot->start_time = time(NULL);

    /* Start async playback */
    if (alda_events_play_async(&g_alda_state.alda_ctx) != 0) {
        slot->active = 0;
        slot->playing = 0;
        free(slot->lua_callback);
        slot->lua_callback = NULL;
        pthread_mutex_unlock(&g_alda_state.mutex);
        set_error("Failed to start playback");
        return -1;
    }

    pthread_mutex_unlock(&g_alda_state.mutex);
    set_error(NULL);

    return slot_id;
}

int loki_alda_eval_sync(editor_ctx_t *ctx, const char *code) {
    (void)ctx;

    if (!g_alda_state.initialized) {
        set_error("Alda not initialized");
        return -1;
    }

    if (!code || !*code) {
        set_error("Empty code");
        return -1;
    }

    pthread_mutex_lock(&g_alda_state.mutex);

    /* Reset context for new evaluation */
    alda_context_reset(&g_alda_state.alda_ctx);

    /* Parse and interpret */
    if (alda_interpret_string(&g_alda_state.alda_ctx, code, "<loki>") != 0) {
        pthread_mutex_unlock(&g_alda_state.mutex);
        set_error("Parse error in Alda code");
        return -1;
    }

    /* Sort events */
    alda_events_sort(&g_alda_state.alda_ctx);

    /* Use Link tempo if enabled */
    double effective_tempo = loki_link_effective_tempo(
        ctx, (double)g_alda_state.alda_ctx.global_tempo);
    g_alda_state.alda_ctx.global_tempo = (int)effective_tempo;

    /* Play */
    int result = alda_events_play(&g_alda_state.alda_ctx);

    pthread_mutex_unlock(&g_alda_state.mutex);

    if (result != 0) {
        set_error("Playback error");
        return -1;
    }

    set_error(NULL);
    return 0;
}

void loki_alda_stop(editor_ctx_t *ctx, int slot_id) {
    (void)ctx;

    if (!g_alda_state.initialized) return;

    if (slot_id < 0) {
        loki_alda_stop_all(ctx);
        return;
    }

    if (slot_id >= LOKI_ALDA_MAX_SLOTS) return;

    pthread_mutex_lock(&g_alda_state.mutex);

    AldaPlaybackSlot *slot = &g_alda_state.slots[slot_id];
    if (slot->active && slot->playing) {
        alda_async_stop();
        slot->playing = 0;
        slot->completed = 1;
        slot->status = LOKI_ALDA_STATUS_STOPPED;
    }

    pthread_mutex_unlock(&g_alda_state.mutex);
}

void loki_alda_stop_all(editor_ctx_t *ctx) {
    (void)ctx;

    if (!g_alda_state.initialized) return;

    pthread_mutex_lock(&g_alda_state.mutex);

    alda_async_stop();
    alda_midi_all_notes_off(&g_alda_state.alda_ctx);

    for (int i = 0; i < LOKI_ALDA_MAX_SLOTS; i++) {
        AldaPlaybackSlot *slot = &g_alda_state.slots[i];
        if (slot->active && slot->playing) {
            slot->playing = 0;
            slot->completed = 1;
            slot->status = LOKI_ALDA_STATUS_STOPPED;
        }
    }

    pthread_mutex_unlock(&g_alda_state.mutex);
}

/* ======================= Status Queries ======================= */

LokiAldaStatus loki_alda_get_status(editor_ctx_t *ctx, int slot_id) {
    (void)ctx;

    if (!g_alda_state.initialized) return LOKI_ALDA_STATUS_IDLE;
    if (slot_id < 0 || slot_id >= LOKI_ALDA_MAX_SLOTS) return LOKI_ALDA_STATUS_IDLE;

    return g_alda_state.slots[slot_id].status;
}

int loki_alda_is_playing(editor_ctx_t *ctx) {
    (void)ctx;

    if (!g_alda_state.initialized) return 0;
    return alda_async_is_playing();
}

int loki_alda_active_count(editor_ctx_t *ctx) {
    (void)ctx;

    if (!g_alda_state.initialized) return 0;
    return alda_async_active_count();
}

/* ======================= Configuration ======================= */

void loki_alda_set_tempo(editor_ctx_t *ctx, int bpm) {
    (void)ctx;

    if (!g_alda_state.initialized) return;
    if (bpm < 20) bpm = 20;
    if (bpm > 400) bpm = 400;

    g_alda_state.alda_ctx.global_tempo = bpm;
}

int loki_alda_get_tempo(editor_ctx_t *ctx) {
    (void)ctx;

    if (!g_alda_state.initialized) return 120;
    return g_alda_state.alda_ctx.global_tempo;
}

/* ======================= MIDI Export Support ======================= */

const AldaScheduledEvent* loki_alda_get_events(int *count) {
    if (!g_alda_state.initialized) {
        if (count) *count = 0;
        return NULL;
    }

    if (count) *count = g_alda_state.alda_ctx.event_count;
    return g_alda_state.alda_ctx.events;
}

int loki_alda_get_channel_count(void) {
    if (!g_alda_state.initialized || g_alda_state.alda_ctx.event_count == 0) {
        return 0;
    }

    /* Track which channels are used with a bitmask */
    unsigned int channels_used = 0;
    int event_count = g_alda_state.alda_ctx.event_count;
    const AldaScheduledEvent *events = g_alda_state.alda_ctx.events;

    for (int i = 0; i < event_count; i++) {
        if (events[i].channel >= 0 && events[i].channel < 16) {
            channels_used |= (1u << events[i].channel);
        }
    }

    /* Count set bits */
    int count = 0;
    while (channels_used) {
        count += channels_used & 1;
        channels_used >>= 1;
    }

    return count;
}

int loki_alda_set_synth_enabled(editor_ctx_t *ctx, int enable) {
    (void)ctx;

    if (!g_alda_state.initialized) return -1;

    if (enable) {
        if (!alda_tsf_has_soundfont()) {
            set_error("No soundfont loaded");
            return -1;
        }
        alda_tsf_enable();
        g_alda_state.alda_ctx.tsf_enabled = 1;
    } else {
        alda_tsf_disable();
        g_alda_state.alda_ctx.tsf_enabled = 0;
    }

    return 0;
}

int loki_alda_load_soundfont(editor_ctx_t *ctx, const char *path) {
    (void)ctx;

    if (!g_alda_state.initialized) {
        set_error("Alda not initialized");
        return -1;
    }

    if (!path || !*path) {
        set_error("Invalid soundfont path");
        return -1;
    }

    if (alda_tsf_load_soundfont(path) != 0) {
        set_error("Failed to load soundfont");
        return -1;
    }

    return 0;
}

/* ======================= Csound Backend ======================= */

int loki_alda_csound_is_available(void) {
    /* Check if Csound init succeeds (it returns -1 if not compiled) */
    return alda_csound_is_enabled() || alda_csound_has_instruments() ||
           (alda_csound_init() == 0);
}

int loki_alda_csound_is_enabled(editor_ctx_t *ctx) {
    (void)ctx;
    return g_alda_state.initialized && g_alda_state.alda_ctx.csound_enabled;
}

int loki_alda_csound_set_enabled(editor_ctx_t *ctx, int enable) {
    (void)ctx;

    if (!g_alda_state.initialized) {
        set_error("Alda not initialized");
        return -1;
    }

    if (enable) {
        if (!alda_csound_has_instruments()) {
            set_error("No Csound instruments loaded");
            return -1;
        }
        /* Disable TSF first if enabled */
        if (g_alda_state.alda_ctx.tsf_enabled) {
            alda_tsf_disable();
            g_alda_state.alda_ctx.tsf_enabled = 0;
        }
        if (alda_csound_enable() != 0) {
            set_error(alda_csound_get_error());
            return -1;
        }
        g_alda_state.alda_ctx.csound_enabled = 1;
    } else {
        alda_csound_disable();
        g_alda_state.alda_ctx.csound_enabled = 0;
    }

    return 0;
}

int loki_alda_csound_load_csd(editor_ctx_t *ctx, const char *path) {
    (void)ctx;

    if (!g_alda_state.initialized) {
        set_error("Alda not initialized");
        return -1;
    }

    if (!path || !*path) {
        set_error("Invalid CSD path");
        return -1;
    }

    /* Initialize Csound backend if not already */
    if (alda_csound_init() != 0) {
        set_error("Csound backend not available");
        return -1;
    }

    if (alda_csound_load_csd(path) != 0) {
        set_error(alda_csound_get_error());
        return -1;
    }

    return 0;
}

int loki_alda_csound_play_async(const char *path) {
    if (!path || !*path) {
        set_error("Invalid CSD path");
        return -1;
    }

    int result = alda_csound_play_file_async(path);
    if (result != 0) {
        const char *err = alda_csound_get_error();
        set_error(err ? err : "Failed to start playback");
        return -1;
    }

    return 0;
}

int loki_alda_csound_playback_active(void) {
    return alda_csound_playback_active();
}

void loki_alda_csound_stop_playback(void) {
    alda_csound_stop_playback();
}

/* ======================= Main Loop Integration ======================= */

void loki_alda_check_callbacks(editor_ctx_t *ctx, lua_State *L) {
    (void)ctx;

    if (!g_alda_state.initialized) return;
    if (!L) return;

    pthread_mutex_lock(&g_alda_state.mutex);

    /* Check if async playback has completed */
    int still_playing = alda_async_is_playing();

    for (int i = 0; i < LOKI_ALDA_MAX_SLOTS; i++) {
        AldaPlaybackSlot *slot = &g_alda_state.slots[i];

        if (!slot->active) continue;

        /* Check if this slot's playback completed */
        if (slot->playing && !still_playing) {
            slot->playing = 0;
            slot->completed = 1;
            slot->status = LOKI_ALDA_STATUS_COMPLETE;
            slot->duration_ms = (int)((time(NULL) - slot->start_time) * 1000);
        }

        /* Invoke callback for completed slots */
        if (slot->completed && slot->lua_callback) {
            /* Get the callback function */
            lua_getglobal(L, slot->lua_callback);
            if (lua_isfunction(L, -1)) {
                /* Create result table */
                lua_newtable(L);

                lua_pushstring(L, "status");
                switch (slot->status) {
                    case LOKI_ALDA_STATUS_COMPLETE:
                        lua_pushstring(L, "complete");
                        break;
                    case LOKI_ALDA_STATUS_STOPPED:
                        lua_pushstring(L, "stopped");
                        break;
                    case LOKI_ALDA_STATUS_ERROR:
                        lua_pushstring(L, "error");
                        break;
                    default:
                        lua_pushstring(L, "unknown");
                        break;
                }
                lua_settable(L, -3);

                lua_pushstring(L, "slot");
                lua_pushinteger(L, i);
                lua_settable(L, -3);

                lua_pushstring(L, "events");
                lua_pushinteger(L, slot->events_played);
                lua_settable(L, -3);

                lua_pushstring(L, "duration_ms");
                lua_pushinteger(L, slot->duration_ms);
                lua_settable(L, -3);

                if (slot->error_msg) {
                    lua_pushstring(L, "error");
                    lua_pushstring(L, slot->error_msg);
                    lua_settable(L, -3);
                }

                /* Call the function with 1 argument (result table) */
                if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
                    const char *err = lua_tostring(L, -1);
                    fprintf(stderr, "Alda callback error: %s\n", err ? err : "unknown");
                    lua_pop(L, 1);
                }
            } else {
                lua_pop(L, 1);  /* Pop non-function value */
            }

            /* Clear the slot after callback */
            clear_slot(i);
        }

        /* Clear completed slots without callbacks */
        if (slot->completed && !slot->lua_callback) {
            clear_slot(i);
        }
    }

    pthread_mutex_unlock(&g_alda_state.mutex);
}

/* ======================= Utility Functions ======================= */

int loki_alda_list_ports(editor_ctx_t *ctx, char **ports, int max_ports) {
    (void)ctx;
    (void)ports;
    (void)max_ports;

    /* TODO: Implement port enumeration */
    /* alda_midi_list_ports only prints to stdout currently */
    return 0;
}

const char *loki_alda_get_error(editor_ctx_t *ctx) {
    (void)ctx;
    return g_alda_state.last_error[0] ? g_alda_state.last_error : NULL;
}
