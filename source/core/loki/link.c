/* loki_link.c - Ableton Link integration for Loki
 *
 * Provides tempo synchronization with other Link-enabled applications
 * on the local network using the abl_link C wrapper.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "loki/link.h"
#include "internal.h"
#include "async_queue.h"

/* Ableton Link C wrapper */
#include <abl_link.h>

/* Lua headers for callbacks */
#include <lua.h>
#include <lauxlib.h>

/* ======================= Internal State ======================= */

typedef struct {
    abl_link link;
    abl_link_session_state session_state;
    int initialized;
    pthread_mutex_t mutex;

    /* Callback state (set by Link thread, polled by main thread) */
    volatile int peers_changed;
    volatile uint64_t pending_peers;
    volatile int tempo_changed;
    volatile double pending_tempo;
    volatile int playing_changed;
    volatile int pending_playing;

    /* Last known values for change detection */
    uint64_t last_peers;
    double last_tempo;
    int last_playing;

    /* Lua callback names (owned strings) */
    char *peers_callback;
    char *tempo_callback;
    char *start_stop_callback;
} LinkState;

static LinkState g_link_state = {0};

/* ======================= Link Callbacks ======================= */

/* Called on Link-managed thread when peer count changes */
static void on_peers_changed(uint64_t num_peers, void *context) {
    (void)context;
    pthread_mutex_lock(&g_link_state.mutex);
    g_link_state.pending_peers = num_peers;
    g_link_state.peers_changed = 1;
    pthread_mutex_unlock(&g_link_state.mutex);

    /* Push event to async queue for unified event handling */
    async_queue_push_link_peers(NULL, num_peers);
}

/* Called on Link-managed thread when tempo changes */
static void on_tempo_changed(double tempo, void *context) {
    (void)context;
    pthread_mutex_lock(&g_link_state.mutex);
    g_link_state.pending_tempo = tempo;
    g_link_state.tempo_changed = 1;
    pthread_mutex_unlock(&g_link_state.mutex);

    /* Push event to async queue for unified event handling */
    async_queue_push_link_tempo(NULL, tempo);
}

/* Called on Link-managed thread when start/stop state changes */
static void on_start_stop_changed(bool is_playing, void *context) {
    (void)context;
    pthread_mutex_lock(&g_link_state.mutex);
    g_link_state.pending_playing = is_playing ? 1 : 0;
    g_link_state.playing_changed = 1;
    pthread_mutex_unlock(&g_link_state.mutex);

    /* Push event to async queue for unified event handling */
    async_queue_push_link_transport(NULL, is_playing ? 1 : 0);
}

/* ======================= Initialization ======================= */

int loki_link_init(editor_ctx_t *ctx, double initial_bpm) {
    (void)ctx;

    if (g_link_state.initialized) {
        return -1;  /* Already initialized */
    }

    /* Clamp tempo to valid range */
    if (initial_bpm < 20.0) initial_bpm = 20.0;
    if (initial_bpm > 999.0) initial_bpm = 999.0;

    /* Initialize mutex */
    if (pthread_mutex_init(&g_link_state.mutex, NULL) != 0) {
        return -1;
    }

    /* Create Link instance */
    g_link_state.link = abl_link_create(initial_bpm);

    /* Create session state for capturing/committing */
    g_link_state.session_state = abl_link_create_session_state();

    /* Register callbacks */
    abl_link_set_num_peers_callback(g_link_state.link, on_peers_changed, NULL);
    abl_link_set_tempo_callback(g_link_state.link, on_tempo_changed, NULL);
    abl_link_set_start_stop_callback(g_link_state.link, on_start_stop_changed, NULL);

    /* Initialize last known values */
    g_link_state.last_peers = 0;
    g_link_state.last_tempo = initial_bpm;
    g_link_state.last_playing = 0;

    g_link_state.initialized = 1;

    return 0;
}

void loki_link_cleanup(editor_ctx_t *ctx) {
    (void)ctx;

    if (!g_link_state.initialized) return;

    pthread_mutex_lock(&g_link_state.mutex);

    /* Disable Link before cleanup */
    abl_link_enable(g_link_state.link, false);

    /* Destroy session state */
    abl_link_destroy_session_state(g_link_state.session_state);

    /* Destroy Link instance */
    abl_link_destroy(g_link_state.link);

    /* Free callback names */
    free(g_link_state.peers_callback);
    free(g_link_state.tempo_callback);
    free(g_link_state.start_stop_callback);

    g_link_state.peers_callback = NULL;
    g_link_state.tempo_callback = NULL;
    g_link_state.start_stop_callback = NULL;

    g_link_state.initialized = 0;

    pthread_mutex_unlock(&g_link_state.mutex);
    pthread_mutex_destroy(&g_link_state.mutex);
}

int loki_link_is_initialized(editor_ctx_t *ctx) {
    (void)ctx;
    return g_link_state.initialized;
}

/* ======================= Enable/Disable ======================= */

void loki_link_enable(editor_ctx_t *ctx, int enable) {
    (void)ctx;

    if (!g_link_state.initialized) return;

    abl_link_enable(g_link_state.link, enable ? true : false);
}

int loki_link_is_enabled(editor_ctx_t *ctx) {
    (void)ctx;

    if (!g_link_state.initialized) return 0;

    return abl_link_is_enabled(g_link_state.link) ? 1 : 0;
}

/* ======================= Tempo ======================= */

double loki_link_get_tempo(editor_ctx_t *ctx) {
    (void)ctx;

    if (!g_link_state.initialized) return 120.0;

    /* Capture current session state */
    abl_link_capture_app_session_state(g_link_state.link, g_link_state.session_state);

    return abl_link_tempo(g_link_state.session_state);
}

void loki_link_set_tempo(editor_ctx_t *ctx, double bpm) {
    (void)ctx;

    if (!g_link_state.initialized) return;

    /* Clamp tempo to valid range */
    if (bpm < 20.0) bpm = 20.0;
    if (bpm > 999.0) bpm = 999.0;

    /* Capture, modify, commit */
    abl_link_capture_app_session_state(g_link_state.link, g_link_state.session_state);
    int64_t now = abl_link_clock_micros(g_link_state.link);
    abl_link_set_tempo(g_link_state.session_state, bpm, now);
    abl_link_commit_app_session_state(g_link_state.link, g_link_state.session_state);
}

/* ======================= Beat/Phase ======================= */

double loki_link_get_beat(editor_ctx_t *ctx, double quantum) {
    (void)ctx;

    if (!g_link_state.initialized) return 0.0;

    if (quantum <= 0.0) quantum = 4.0;

    abl_link_capture_app_session_state(g_link_state.link, g_link_state.session_state);
    int64_t now = abl_link_clock_micros(g_link_state.link);

    return abl_link_beat_at_time(g_link_state.session_state, now, quantum);
}

double loki_link_get_phase(editor_ctx_t *ctx, double quantum) {
    (void)ctx;

    if (!g_link_state.initialized) return 0.0;

    if (quantum <= 0.0) quantum = 4.0;

    abl_link_capture_app_session_state(g_link_state.link, g_link_state.session_state);
    int64_t now = abl_link_clock_micros(g_link_state.link);

    return abl_link_phase_at_time(g_link_state.session_state, now, quantum);
}

/* ======================= Transport ======================= */

void loki_link_enable_start_stop_sync(editor_ctx_t *ctx, int enable) {
    (void)ctx;

    if (!g_link_state.initialized) return;

    abl_link_enable_start_stop_sync(g_link_state.link, enable ? true : false);
}

int loki_link_is_start_stop_sync_enabled(editor_ctx_t *ctx) {
    (void)ctx;

    if (!g_link_state.initialized) return 0;

    return abl_link_is_start_stop_sync_enabled(g_link_state.link) ? 1 : 0;
}

int loki_link_is_playing(editor_ctx_t *ctx) {
    (void)ctx;

    if (!g_link_state.initialized) return 0;

    abl_link_capture_app_session_state(g_link_state.link, g_link_state.session_state);

    return abl_link_is_playing(g_link_state.session_state) ? 1 : 0;
}

void loki_link_set_playing(editor_ctx_t *ctx, int playing) {
    (void)ctx;

    if (!g_link_state.initialized) return;

    abl_link_capture_app_session_state(g_link_state.link, g_link_state.session_state);
    int64_t now = abl_link_clock_micros(g_link_state.link);
    abl_link_set_is_playing(g_link_state.session_state, playing ? true : false, (uint64_t)now);
    abl_link_commit_app_session_state(g_link_state.link, g_link_state.session_state);
}

/* ======================= Peers ======================= */

uint64_t loki_link_num_peers(editor_ctx_t *ctx) {
    (void)ctx;

    if (!g_link_state.initialized) return 0;

    return abl_link_num_peers(g_link_state.link);
}

/* ======================= Callbacks ======================= */

void loki_link_set_peers_callback(editor_ctx_t *ctx, const char *callback_name) {
    (void)ctx;

    if (!g_link_state.initialized) return;

    pthread_mutex_lock(&g_link_state.mutex);
    free(g_link_state.peers_callback);
    g_link_state.peers_callback = callback_name ? strdup(callback_name) : NULL;
    pthread_mutex_unlock(&g_link_state.mutex);
}

void loki_link_set_tempo_callback(editor_ctx_t *ctx, const char *callback_name) {
    (void)ctx;

    if (!g_link_state.initialized) return;

    pthread_mutex_lock(&g_link_state.mutex);
    free(g_link_state.tempo_callback);
    g_link_state.tempo_callback = callback_name ? strdup(callback_name) : NULL;
    pthread_mutex_unlock(&g_link_state.mutex);
}

void loki_link_set_start_stop_callback(editor_ctx_t *ctx, const char *callback_name) {
    (void)ctx;

    if (!g_link_state.initialized) return;

    pthread_mutex_lock(&g_link_state.mutex);
    free(g_link_state.start_stop_callback);
    g_link_state.start_stop_callback = callback_name ? strdup(callback_name) : NULL;
    pthread_mutex_unlock(&g_link_state.mutex);
}

/* ======================= Main Loop Integration ======================= */

void loki_link_check_callbacks(editor_ctx_t *ctx, lua_State *L) {
    (void)ctx;

    if (!g_link_state.initialized) return;
    if (!L) return;

    pthread_mutex_lock(&g_link_state.mutex);

    /* Check for peer count changes */
    if (g_link_state.peers_changed && g_link_state.peers_callback) {
        uint64_t peers = g_link_state.pending_peers;
        g_link_state.peers_changed = 0;
        g_link_state.last_peers = peers;

        char *callback = g_link_state.peers_callback;
        pthread_mutex_unlock(&g_link_state.mutex);

        lua_getglobal(L, callback);
        if (lua_isfunction(L, -1)) {
            lua_pushinteger(L, (lua_Integer)peers);
            if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
                const char *err = lua_tostring(L, -1);
                fprintf(stderr, "Link peers callback error: %s\n", err ? err : "unknown");
                lua_pop(L, 1);
            }
        } else {
            lua_pop(L, 1);
        }

        pthread_mutex_lock(&g_link_state.mutex);
    }

    /* Check for tempo changes */
    if (g_link_state.tempo_changed && g_link_state.tempo_callback) {
        double tempo = g_link_state.pending_tempo;
        g_link_state.tempo_changed = 0;
        g_link_state.last_tempo = tempo;

        char *callback = g_link_state.tempo_callback;
        pthread_mutex_unlock(&g_link_state.mutex);

        lua_getglobal(L, callback);
        if (lua_isfunction(L, -1)) {
            lua_pushnumber(L, tempo);
            if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
                const char *err = lua_tostring(L, -1);
                fprintf(stderr, "Link tempo callback error: %s\n", err ? err : "unknown");
                lua_pop(L, 1);
            }
        } else {
            lua_pop(L, 1);
        }

        pthread_mutex_lock(&g_link_state.mutex);
    }

    /* Check for start/stop changes */
    if (g_link_state.playing_changed && g_link_state.start_stop_callback) {
        int playing = g_link_state.pending_playing;
        g_link_state.playing_changed = 0;
        g_link_state.last_playing = playing;

        char *callback = g_link_state.start_stop_callback;
        pthread_mutex_unlock(&g_link_state.mutex);

        lua_getglobal(L, callback);
        if (lua_isfunction(L, -1)) {
            lua_pushboolean(L, playing);
            if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
                const char *err = lua_tostring(L, -1);
                fprintf(stderr, "Link start/stop callback error: %s\n", err ? err : "unknown");
                lua_pop(L, 1);
            }
        } else {
            lua_pop(L, 1);
        }

        pthread_mutex_lock(&g_link_state.mutex);
    }

    pthread_mutex_unlock(&g_link_state.mutex);
}

/* ======================= Timing Integration ======================= */

double loki_link_effective_tempo(editor_ctx_t *ctx, double fallback_tempo) {
    (void)ctx;

    if (!g_link_state.initialized) return fallback_tempo;
    if (!abl_link_is_enabled(g_link_state.link)) return fallback_tempo;

    return loki_link_get_tempo(ctx);
}
