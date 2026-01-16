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
#include "lang_bridge.h"

/* Alda library headers */
#include <alda/alda.h>
#include <alda/csound_backend.h>

/* Shared audio backend (for shared_csound_is_available) */
#include "shared/audio/audio.h"

/* Shared MIDI events buffer (for export) */
#include "shared/midi/events.h"

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

/* Per-context alda state */
struct LokiAldaState {
    int initialized;
    AldaContext alda_ctx;        /* The alda interpreter context */
    AldaPlaybackSlot slots[LOKI_ALDA_MAX_SLOTS];
    char last_error[LOKI_ALDA_ERROR_BUFSIZE];  /* Last error message */
    pthread_mutex_t mutex;       /* Protect concurrent access */
};

/* Get alda state from editor context, returning NULL if not initialized */
static LokiAldaState* get_alda_state(editor_ctx_t *ctx) {
    return ctx ? ctx->alda_state : NULL;
}

/* ======================= Helper Functions ======================= */

static void set_state_error(LokiAldaState *state, const char *msg) {
    if (!state) return;
    if (msg) {
        strncpy(state->last_error, msg, sizeof(state->last_error) - 1);
        state->last_error[sizeof(state->last_error) - 1] = '\0';
    } else {
        state->last_error[0] = '\0';
    }
}

static int find_free_slot(LokiAldaState *state) {
    if (!state) return -1;
    for (int i = 0; i < LOKI_ALDA_MAX_SLOTS; i++) {
        if (!state->slots[i].active) {
            return i;
        }
    }
    return -1;
}

static void clear_slot(LokiAldaState *state, int slot_id) {
    if (!state) return;
    if (slot_id < 0 || slot_id >= LOKI_ALDA_MAX_SLOTS) return;

    AldaPlaybackSlot *slot = &state->slots[slot_id];
    free(slot->lua_callback);
    free(slot->error_msg);
    memset(slot, 0, sizeof(AldaPlaybackSlot));
}

/* ======================= Initialization ======================= */

int loki_alda_init(editor_ctx_t *ctx, const char *port_name) {
    if (!ctx) return -1;

    /* Check if already initialized for this context */
    if (ctx->alda_state && ctx->alda_state->initialized) {
        set_state_error(ctx->alda_state, "Alda already initialized");
        return -1;
    }

    /* Allocate state if needed */
    LokiAldaState *state = ctx->alda_state;
    if (!state) {
        state = (LokiAldaState *)calloc(1, sizeof(LokiAldaState));
        if (!state) return -1;
        ctx->alda_state = state;
    }

    /* Initialize mutex */
    if (pthread_mutex_init(&state->mutex, NULL) != 0) {
        set_state_error(state, "Failed to initialize mutex");
        free(state);
        ctx->alda_state = NULL;
        return -1;
    }

    /* Initialize alda context */
    alda_context_init(&state->alda_ctx);

    /* Initialize async system (creates libuv thread) */
    if (alda_async_init() != 0) {
        set_state_error(state, "Failed to initialize async playback");
        alda_context_cleanup(&state->alda_ctx);
        pthread_mutex_destroy(&state->mutex);
        free(state);
        ctx->alda_state = NULL;
        return -1;
    }

    /* Enable concurrent mode for livecoding */
    alda_async_set_concurrent(1);

    /* Open MIDI output */
    const char *name = port_name ? port_name : "Loki";
    if (alda_midi_open_auto(&state->alda_ctx, name) != 0) {
        set_state_error(state, "Failed to open MIDI output");
        alda_async_cleanup();
        alda_context_cleanup(&state->alda_ctx);
        pthread_mutex_destroy(&state->mutex);
        free(state);
        ctx->alda_state = NULL;
        return -1;
    }

    /* Initialize TinySoundFont backend */
    alda_tsf_init();

    /* Clear all slots */
    memset(state->slots, 0, sizeof(state->slots));

    state->initialized = 1;
    set_state_error(state, NULL);

    return 0;
}

void loki_alda_cleanup(editor_ctx_t *ctx) {
    if (!ctx) return;

    LokiAldaState *state = get_alda_state(ctx);
    if (!state || !state->initialized) return;

    /* Stop all playback */
    alda_async_stop();

    /* Clean up TinySoundFont */
    alda_tsf_cleanup();

    /* Clean up async system */
    alda_async_cleanup();

    /* Clean up MIDI */
    alda_midi_cleanup(&state->alda_ctx);

    /* Clean up context */
    alda_context_cleanup(&state->alda_ctx);

    /* Clear all slots */
    for (int i = 0; i < LOKI_ALDA_MAX_SLOTS; i++) {
        clear_slot(state, i);
    }

    /* Destroy mutex */
    pthread_mutex_destroy(&state->mutex);

    state->initialized = 0;

    /* Free the state structure */
    free(state);
    ctx->alda_state = NULL;
}

int loki_alda_is_initialized(editor_ctx_t *ctx) {
    LokiAldaState *state = get_alda_state(ctx);
    return state ? state->initialized : 0;
}

/* ======================= Playback Control ======================= */

int loki_alda_eval_async(editor_ctx_t *ctx, const char *code, const char *lua_callback) {
    LokiAldaState *state = get_alda_state(ctx);

    if (!state || !state->initialized) {
        if (state) set_state_error(state, "Alda not initialized");
        return -1;
    }

    if (!code || !*code) {
        set_state_error(state, "Empty code");
        return -2;
    }

    pthread_mutex_lock(&state->mutex);

    /* Find a free slot */
    int slot_id = find_free_slot(state);
    if (slot_id < 0) {
        pthread_mutex_unlock(&state->mutex);
        set_state_error(state, "No free playback slots");
        return -1;
    }

    AldaPlaybackSlot *slot = &state->slots[slot_id];

    /* Reset context for new evaluation (keeps MIDI connection) */
    alda_context_reset(&state->alda_ctx);

    /* Parse and interpret the code */
    if (alda_interpret_string(&state->alda_ctx, code, "<loki>") != 0) {
        pthread_mutex_unlock(&state->mutex);
        set_state_error(state, "Parse error in Alda code");
        return -2;
    }

    /* Check if we have any events to play */
    if (state->alda_ctx.event_count == 0) {
        pthread_mutex_unlock(&state->mutex);
        set_state_error(state, "No events generated");
        return -2;
    }

    /* Sort events for playback */
    alda_events_sort(&state->alda_ctx);

    /* Use Link tempo if enabled */
    double effective_tempo = loki_link_effective_tempo(
        ctx, (double)state->alda_ctx.global_tempo);
    state->alda_ctx.global_tempo = (int)effective_tempo;

    /* Set up slot */
    slot->active = 1;
    slot->playing = 1;
    slot->completed = 0;
    slot->status = LOKI_ALDA_STATUS_PLAYING;
    slot->lua_callback = lua_callback ? strdup(lua_callback) : NULL;
    slot->error_msg = NULL;
    slot->events_played = state->alda_ctx.event_count;
    slot->start_time = time(NULL);

    /* Start async playback */
    if (alda_events_play_async(&state->alda_ctx) != 0) {
        slot->active = 0;
        slot->playing = 0;
        free(slot->lua_callback);
        slot->lua_callback = NULL;
        pthread_mutex_unlock(&state->mutex);
        set_state_error(state, "Failed to start playback");
        return -1;
    }

    pthread_mutex_unlock(&state->mutex);
    set_state_error(state, NULL);

    return slot_id;
}

int loki_alda_eval_sync(editor_ctx_t *ctx, const char *code) {
    LokiAldaState *state = get_alda_state(ctx);

    if (!state || !state->initialized) {
        if (state) set_state_error(state, "Alda not initialized");
        return -1;
    }

    if (!code || !*code) {
        set_state_error(state, "Empty code");
        return -1;
    }

    pthread_mutex_lock(&state->mutex);

    /* Reset context for new evaluation */
    alda_context_reset(&state->alda_ctx);

    /* Parse and interpret */
    if (alda_interpret_string(&state->alda_ctx, code, "<loki>") != 0) {
        pthread_mutex_unlock(&state->mutex);
        set_state_error(state, "Parse error in Alda code");
        return -1;
    }

    /* Sort events */
    alda_events_sort(&state->alda_ctx);

    /* Use Link tempo if enabled */
    double effective_tempo = loki_link_effective_tempo(
        ctx, (double)state->alda_ctx.global_tempo);
    state->alda_ctx.global_tempo = (int)effective_tempo;

    /* Play */
    int result = alda_events_play(&state->alda_ctx);

    pthread_mutex_unlock(&state->mutex);

    if (result != 0) {
        set_state_error(state, "Playback error");
        return -1;
    }

    set_state_error(state, NULL);
    return 0;
}

void loki_alda_stop(editor_ctx_t *ctx, int slot_id) {
    LokiAldaState *state = get_alda_state(ctx);
    if (!state || !state->initialized) return;

    if (slot_id < 0) {
        loki_alda_stop_all(ctx);
        return;
    }

    if (slot_id >= LOKI_ALDA_MAX_SLOTS) return;

    pthread_mutex_lock(&state->mutex);

    AldaPlaybackSlot *slot = &state->slots[slot_id];
    if (slot->active && slot->playing) {
        alda_async_stop();
        slot->playing = 0;
        slot->completed = 1;
        slot->status = LOKI_ALDA_STATUS_STOPPED;
    }

    pthread_mutex_unlock(&state->mutex);
}

void loki_alda_stop_all(editor_ctx_t *ctx) {
    LokiAldaState *state = get_alda_state(ctx);
    if (!state || !state->initialized) return;

    pthread_mutex_lock(&state->mutex);

    alda_async_stop();
    alda_midi_all_notes_off(&state->alda_ctx);

    for (int i = 0; i < LOKI_ALDA_MAX_SLOTS; i++) {
        AldaPlaybackSlot *slot = &state->slots[i];
        if (slot->active && slot->playing) {
            slot->playing = 0;
            slot->completed = 1;
            slot->status = LOKI_ALDA_STATUS_STOPPED;
        }
    }

    pthread_mutex_unlock(&state->mutex);
}

/* ======================= Status Queries ======================= */

LokiAldaStatus loki_alda_get_status(editor_ctx_t *ctx, int slot_id) {
    LokiAldaState *state = get_alda_state(ctx);
    if (!state || !state->initialized) return LOKI_ALDA_STATUS_IDLE;
    if (slot_id < 0 || slot_id >= LOKI_ALDA_MAX_SLOTS) return LOKI_ALDA_STATUS_IDLE;

    return state->slots[slot_id].status;
}

int loki_alda_is_playing(editor_ctx_t *ctx) {
    LokiAldaState *state = get_alda_state(ctx);
    if (!state || !state->initialized) return 0;
    return alda_async_is_playing();
}

int loki_alda_active_count(editor_ctx_t *ctx) {
    LokiAldaState *state = get_alda_state(ctx);
    if (!state || !state->initialized) return 0;
    return alda_async_active_count();
}

/* ======================= Configuration ======================= */

void loki_alda_set_tempo(editor_ctx_t *ctx, int bpm) {
    LokiAldaState *state = get_alda_state(ctx);
    if (!state || !state->initialized) return;

    if (bpm < LOKI_ALDA_TEMPO_MIN) bpm = LOKI_ALDA_TEMPO_MIN;
    if (bpm > LOKI_ALDA_TEMPO_MAX) bpm = LOKI_ALDA_TEMPO_MAX;

    state->alda_ctx.global_tempo = bpm;
}

int loki_alda_get_tempo(editor_ctx_t *ctx) {
    LokiAldaState *state = get_alda_state(ctx);
    if (!state || !state->initialized) return LOKI_ALDA_TEMPO_DEFAULT;
    return state->alda_ctx.global_tempo;
}

/* ======================= MIDI Export Support ======================= */

const AldaScheduledEvent* loki_alda_get_events(editor_ctx_t *ctx, int *count) {
    LokiAldaState *state = get_alda_state(ctx);
    if (!state || !state->initialized) {
        if (count) *count = 0;
        return NULL;
    }

    if (count) *count = state->alda_ctx.event_count;
    return state->alda_ctx.events;
}

int loki_alda_get_channel_count(editor_ctx_t *ctx) {
    LokiAldaState *state = get_alda_state(ctx);
    if (!state || !state->initialized || state->alda_ctx.event_count == 0) {
        return 0;
    }

    /* Track which channels are used with a bitmask */
    unsigned int channels_used = 0;
    int event_count = state->alda_ctx.event_count;
    const AldaScheduledEvent *events = state->alda_ctx.events;

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

int loki_alda_has_events(editor_ctx_t *ctx) {
    if (!loki_alda_is_initialized(ctx)) {
        return 0;
    }
    int event_count = 0;
    loki_alda_get_events(ctx, &event_count);
    return event_count > 0;
}

int loki_alda_populate_shared_buffer(editor_ctx_t *ctx) {
    if (!loki_alda_has_events(ctx)) {
        return -1;
    }

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

int loki_alda_set_synth_enabled(editor_ctx_t *ctx, int enable) {
    LokiAldaState *state = get_alda_state(ctx);
    if (!state || !state->initialized) return -1;

    if (enable) {
        if (!alda_tsf_has_soundfont()) {
            set_state_error(state, "No soundfont loaded");
            return -1;
        }
        alda_tsf_enable();
        state->alda_ctx.tsf_enabled = 1;
    } else {
        alda_tsf_disable();
        state->alda_ctx.tsf_enabled = 0;
    }

    return 0;
}

int loki_alda_load_soundfont(editor_ctx_t *ctx, const char *path) {
    LokiAldaState *state = get_alda_state(ctx);
    if (!state || !state->initialized) {
        if (state) set_state_error(state, "Alda not initialized");
        return -1;
    }

    if (!path || !*path) {
        set_state_error(state, "Invalid soundfont path");
        return -1;
    }

    if (alda_tsf_load_soundfont(path) != 0) {
        set_state_error(state, "Failed to load soundfont");
        return -1;
    }

    return 0;
}

/* ======================= Microtuning ======================= */

int loki_alda_set_part_scale(editor_ctx_t *ctx, const char *part_name,
                             struct ScalaScale *scale, int root_note, double root_freq) {
    LokiAldaState *state = get_alda_state(ctx);
    if (!state || !state->initialized) {
        return -1;
    }

    if (!part_name) {
        set_state_error(state, "Part name is required");
        return -1;
    }

    /* Find the part by name */
    AldaPartState *part = alda_find_part(&state->alda_ctx, part_name);
    if (!part) {
        set_state_error(state, "Part not found");
        return -1;
    }

    /* Set the scale and tuning parameters */
    part->scale = scale;
    part->scale_root_note = root_note;
    part->scale_root_freq = root_freq;

    return 0;
}

int loki_alda_clear_part_scale(editor_ctx_t *ctx, const char *part_name) {
    return loki_alda_set_part_scale(ctx, part_name, NULL, 60, 261.6255653);
}

/* ======================= Csound Backend ======================= */

int loki_alda_csound_is_available(void) {
    /* Use shared Csound backend availability check */
    return shared_csound_is_available();
}

int loki_alda_csound_is_enabled(editor_ctx_t *ctx) {
    LokiAldaState *state = get_alda_state(ctx);
    return state && state->initialized && state->alda_ctx.csound_enabled;
}

int loki_alda_csound_set_enabled(editor_ctx_t *ctx, int enable) {
    LokiAldaState *state = get_alda_state(ctx);
    if (!state || !state->initialized) {
        if (state) set_state_error(state, "Alda not initialized");
        return -1;
    }

    if (enable) {
        if (!alda_csound_has_instruments()) {
            set_state_error(state, "No Csound instruments loaded");
            return -1;
        }
        /* Disable TSF first if enabled */
        if (state->alda_ctx.tsf_enabled) {
            alda_tsf_disable();
            state->alda_ctx.tsf_enabled = 0;
        }
        if (alda_csound_enable() != 0) {
            set_state_error(state, alda_csound_get_error());
            return -1;
        }
        state->alda_ctx.csound_enabled = 1;
    } else {
        alda_csound_disable();
        state->alda_ctx.csound_enabled = 0;
    }

    return 0;
}

int loki_alda_csound_load_csd(editor_ctx_t *ctx, const char *path) {
    LokiAldaState *state = get_alda_state(ctx);
    if (!state || !state->initialized) {
        if (state) set_state_error(state, "Alda not initialized");
        return -1;
    }

    if (!path || !*path) {
        set_state_error(state, "Invalid CSD path");
        return -1;
    }

    /* Initialize Csound backend if not already */
    if (alda_csound_init() != 0) {
        set_state_error(state, "Csound backend not available");
        return -1;
    }

    if (alda_csound_load_csd(path) != 0) {
        set_state_error(state, alda_csound_get_error());
        return -1;
    }

    return 0;
}

int loki_alda_csound_play_async(const char *path) {
    if (!path || !*path) {
        fprintf(stderr, "loki_alda: Invalid CSD path\n");
        return -1;
    }

    int result = alda_csound_play_file_async(path);
    if (result != 0) {
        const char *err = alda_csound_get_error();
        fprintf(stderr, "loki_alda: %s\n", err ? err : "Failed to start playback");
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
    LokiAldaState *state = get_alda_state(ctx);
    if (!state || !state->initialized) return;
    if (!L) return;

    pthread_mutex_lock(&state->mutex);

    /* Check if async playback has completed */
    int still_playing = alda_async_is_playing();

    for (int i = 0; i < LOKI_ALDA_MAX_SLOTS; i++) {
        AldaPlaybackSlot *slot = &state->slots[i];

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
            clear_slot(state, i);
        }

        /* Clear completed slots without callbacks */
        if (slot->completed && !slot->lua_callback) {
            clear_slot(state, i);
        }
    }

    pthread_mutex_unlock(&state->mutex);
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
    LokiAldaState *state = get_alda_state(ctx);
    if (!state) return NULL;
    return state->last_error[0] ? state->last_error : NULL;
}

/* ======================= Language Bridge Registration ======================= */

/* Wrapper for init (bridge interface doesn't take port_name) */
static int alda_bridge_init(editor_ctx_t *ctx) {
    return loki_alda_init(ctx, NULL);
}

/* Wrapper for eval (bridge uses sync eval) */
static int alda_bridge_eval(editor_ctx_t *ctx, const char *code) {
    return loki_alda_eval_sync(ctx, code);
}

/* Wrapper for stop (bridge doesn't take slot_id) */
static void alda_bridge_stop(editor_ctx_t *ctx) {
    loki_alda_stop_all(ctx);
}

/* Wrapper for backend configuration */
static int alda_bridge_configure_backend(editor_ctx_t *ctx, const char *sf_path, const char *csd_path) {
    /* CSD takes precedence over soundfont */
    if (csd_path && *csd_path) {
        if (loki_alda_csound_is_available()) {
            if (loki_alda_csound_load_csd(ctx, csd_path) == 0) {
                if (loki_alda_csound_set_enabled(ctx, 1) == 0) {
                    return 0;  /* Success with Csound */
                }
            }
        }
        return -1;  /* Csound requested but failed */
    }

    if (sf_path && *sf_path) {
        if (loki_alda_load_soundfont(ctx, sf_path) == 0) {
            if (loki_alda_set_synth_enabled(ctx, 1) == 0) {
                return 0;  /* Success with TSF */
            }
        }
        return -1;  /* Soundfont requested but failed */
    }

    return 1;  /* No backend requested */
}

/* Language operations for Alda */
static const LokiLangOps alda_lang_ops = {
    .name = "alda",
    .extensions = {".alda", NULL},

    /* Lifecycle */
    .init = alda_bridge_init,
    .cleanup = loki_alda_cleanup,
    .is_initialized = loki_alda_is_initialized,

    /* Main loop */
    .check_callbacks = loki_alda_check_callbacks,

    /* Playback */
    .eval = alda_bridge_eval,
    .stop = alda_bridge_stop,
    .is_playing = loki_alda_is_playing,

    /* Export */
    .has_events = loki_alda_has_events,
    .populate_shared_buffer = loki_alda_populate_shared_buffer,

    /* Error */
    .get_error = loki_alda_get_error,

    /* Backend configuration */
    .configure_backend = alda_bridge_configure_backend,
};

/* Register Alda with the language bridge at startup */
__attribute__((constructor))
static void alda_register_language(void) {
    loki_lang_register(&alda_lang_ops);
}
