/**
 * @file link.c
 * @brief Shared Ableton Link integration.
 *
 * Provides tempo synchronization with other Link-enabled applications.
 * This is a singleton - one global instance shared by all contexts.
 */

#include "link.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* Ableton Link C wrapper */
#include <abl_link.h>

/* ============================================================================
 * Internal State (global singleton)
 * ============================================================================ */

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

    /* Generic callbacks */
    shared_link_peers_callback_t peers_callback;
    void* peers_userdata;
    shared_link_tempo_callback_t tempo_callback;
    void* tempo_userdata;
    shared_link_transport_callback_t transport_callback;
    void* transport_userdata;
} LinkState;

static LinkState g_link = {0};

/* ============================================================================
 * Link Thread Callbacks
 * ============================================================================ */

static void on_peers_changed(uint64_t num_peers, void* context) {
    (void)context;
    pthread_mutex_lock(&g_link.mutex);
    g_link.pending_peers = num_peers;
    g_link.peers_changed = 1;
    pthread_mutex_unlock(&g_link.mutex);
}

static void on_tempo_changed(double tempo, void* context) {
    (void)context;
    pthread_mutex_lock(&g_link.mutex);
    g_link.pending_tempo = tempo;
    g_link.tempo_changed = 1;
    pthread_mutex_unlock(&g_link.mutex);
}

static void on_start_stop_changed(bool is_playing, void* context) {
    (void)context;
    pthread_mutex_lock(&g_link.mutex);
    g_link.pending_playing = is_playing ? 1 : 0;
    g_link.playing_changed = 1;
    pthread_mutex_unlock(&g_link.mutex);
}

/* ============================================================================
 * Initialization
 * ============================================================================ */

int shared_link_init(double initial_bpm) {
    if (g_link.initialized) {
        return -1;  /* Already initialized */
    }

    /* Clamp tempo to valid range */
    if (initial_bpm < 20.0) initial_bpm = 20.0;
    if (initial_bpm > 999.0) initial_bpm = 999.0;

    /* Initialize mutex */
    if (pthread_mutex_init(&g_link.mutex, NULL) != 0) {
        return -1;
    }

    /* Create Link instance */
    g_link.link = abl_link_create(initial_bpm);

    /* Create session state for capturing/committing */
    g_link.session_state = abl_link_create_session_state();

    /* Register Link callbacks */
    abl_link_set_num_peers_callback(g_link.link, on_peers_changed, NULL);
    abl_link_set_tempo_callback(g_link.link, on_tempo_changed, NULL);
    abl_link_set_start_stop_callback(g_link.link, on_start_stop_changed, NULL);

    /* Initialize last known values */
    g_link.last_peers = 0;
    g_link.last_tempo = initial_bpm;
    g_link.last_playing = 0;

    g_link.initialized = 1;

    return 0;
}

void shared_link_cleanup(void) {
    if (!g_link.initialized) return;

    pthread_mutex_lock(&g_link.mutex);

    /* Disable Link before cleanup */
    abl_link_enable(g_link.link, false);

    /* Destroy session state */
    abl_link_destroy_session_state(g_link.session_state);

    /* Destroy Link instance */
    abl_link_destroy(g_link.link);

    /* Clear callbacks */
    g_link.peers_callback = NULL;
    g_link.peers_userdata = NULL;
    g_link.tempo_callback = NULL;
    g_link.tempo_userdata = NULL;
    g_link.transport_callback = NULL;
    g_link.transport_userdata = NULL;

    g_link.initialized = 0;

    pthread_mutex_unlock(&g_link.mutex);
    pthread_mutex_destroy(&g_link.mutex);
}

int shared_link_is_initialized(void) {
    return g_link.initialized;
}

/* ============================================================================
 * Enable/Disable
 * ============================================================================ */

void shared_link_enable(int enable) {
    if (!g_link.initialized) return;

    abl_link_enable(g_link.link, enable ? true : false);
}

int shared_link_is_enabled(void) {
    if (!g_link.initialized) return 0;

    return abl_link_is_enabled(g_link.link) ? 1 : 0;
}

/* ============================================================================
 * Tempo
 * ============================================================================ */

double shared_link_get_tempo(void) {
    if (!g_link.initialized) return 120.0;

    /* Capture current session state */
    abl_link_capture_app_session_state(g_link.link, g_link.session_state);

    return abl_link_tempo(g_link.session_state);
}

void shared_link_set_tempo(double bpm) {
    if (!g_link.initialized) return;

    /* Clamp tempo to valid range */
    if (bpm < 20.0) bpm = 20.0;
    if (bpm > 999.0) bpm = 999.0;

    /* Capture, modify, commit */
    abl_link_capture_app_session_state(g_link.link, g_link.session_state);
    int64_t now = abl_link_clock_micros(g_link.link);
    abl_link_set_tempo(g_link.session_state, bpm, now);
    abl_link_commit_app_session_state(g_link.link, g_link.session_state);
}

double shared_link_effective_tempo(double fallback_tempo) {
    if (!g_link.initialized) return fallback_tempo;
    if (!abl_link_is_enabled(g_link.link)) return fallback_tempo;

    return shared_link_get_tempo();
}

/* ============================================================================
 * Beat/Phase
 * ============================================================================ */

double shared_link_get_beat(double quantum) {
    if (!g_link.initialized) return 0.0;

    if (quantum <= 0.0) quantum = 4.0;

    abl_link_capture_app_session_state(g_link.link, g_link.session_state);
    int64_t now = abl_link_clock_micros(g_link.link);

    return abl_link_beat_at_time(g_link.session_state, now, quantum);
}

double shared_link_get_phase(double quantum) {
    if (!g_link.initialized) return 0.0;

    if (quantum <= 0.0) quantum = 4.0;

    abl_link_capture_app_session_state(g_link.link, g_link.session_state);
    int64_t now = abl_link_clock_micros(g_link.link);

    return abl_link_phase_at_time(g_link.session_state, now, quantum);
}

/* ============================================================================
 * Transport (Start/Stop Sync)
 * ============================================================================ */

void shared_link_enable_start_stop_sync(int enable) {
    if (!g_link.initialized) return;

    abl_link_enable_start_stop_sync(g_link.link, enable ? true : false);
}

int shared_link_is_start_stop_sync_enabled(void) {
    if (!g_link.initialized) return 0;

    return abl_link_is_start_stop_sync_enabled(g_link.link) ? 1 : 0;
}

int shared_link_is_playing(void) {
    if (!g_link.initialized) return 0;

    abl_link_capture_app_session_state(g_link.link, g_link.session_state);

    return abl_link_is_playing(g_link.session_state) ? 1 : 0;
}

void shared_link_set_playing(int playing) {
    if (!g_link.initialized) return;

    abl_link_capture_app_session_state(g_link.link, g_link.session_state);
    int64_t now = abl_link_clock_micros(g_link.link);
    abl_link_set_is_playing(g_link.session_state, playing ? true : false, (uint64_t)now);
    abl_link_commit_app_session_state(g_link.link, g_link.session_state);
}

/* ============================================================================
 * Peer Information
 * ============================================================================ */

uint64_t shared_link_num_peers(void) {
    if (!g_link.initialized) return 0;

    return abl_link_num_peers(g_link.link);
}

/* ============================================================================
 * Callbacks
 * ============================================================================ */

void shared_link_set_peers_callback(shared_link_peers_callback_t callback, void* userdata) {
    if (!g_link.initialized) return;

    pthread_mutex_lock(&g_link.mutex);
    g_link.peers_callback = callback;
    g_link.peers_userdata = userdata;
    pthread_mutex_unlock(&g_link.mutex);
}

void shared_link_set_tempo_callback(shared_link_tempo_callback_t callback, void* userdata) {
    if (!g_link.initialized) return;

    pthread_mutex_lock(&g_link.mutex);
    g_link.tempo_callback = callback;
    g_link.tempo_userdata = userdata;
    pthread_mutex_unlock(&g_link.mutex);
}

void shared_link_set_transport_callback(shared_link_transport_callback_t callback, void* userdata) {
    if (!g_link.initialized) return;

    pthread_mutex_lock(&g_link.mutex);
    g_link.transport_callback = callback;
    g_link.transport_userdata = userdata;
    pthread_mutex_unlock(&g_link.mutex);
}

void shared_link_check_callbacks(void) {
    if (!g_link.initialized) return;

    pthread_mutex_lock(&g_link.mutex);

    /* Check for peer count changes */
    if (g_link.peers_changed && g_link.peers_callback) {
        uint64_t peers = g_link.pending_peers;
        shared_link_peers_callback_t callback = g_link.peers_callback;
        void* userdata = g_link.peers_userdata;
        g_link.peers_changed = 0;
        g_link.last_peers = peers;

        pthread_mutex_unlock(&g_link.mutex);
        callback(peers, userdata);
        pthread_mutex_lock(&g_link.mutex);
    }

    /* Check for tempo changes */
    if (g_link.tempo_changed && g_link.tempo_callback) {
        double tempo = g_link.pending_tempo;
        shared_link_tempo_callback_t callback = g_link.tempo_callback;
        void* userdata = g_link.tempo_userdata;
        g_link.tempo_changed = 0;
        g_link.last_tempo = tempo;

        pthread_mutex_unlock(&g_link.mutex);
        callback(tempo, userdata);
        pthread_mutex_lock(&g_link.mutex);
    }

    /* Check for transport changes */
    if (g_link.playing_changed && g_link.transport_callback) {
        int playing = g_link.pending_playing;
        shared_link_transport_callback_t callback = g_link.transport_callback;
        void* userdata = g_link.transport_userdata;
        g_link.playing_changed = 0;
        g_link.last_playing = playing;

        pthread_mutex_unlock(&g_link.mutex);
        callback(playing, userdata);
        pthread_mutex_lock(&g_link.mutex);
    }

    pthread_mutex_unlock(&g_link.mutex);
}
