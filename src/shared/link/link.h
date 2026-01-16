/**
 * @file link.h
 * @brief Shared Ableton Link API for tempo synchronization.
 *
 * Provides tempo sync with other Link-enabled applications.
 * Link is a singleton - one global instance shared by all contexts.
 */

#ifndef SHARED_LINK_H
#define SHARED_LINK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations for optional callback support */
#ifndef lua_h
struct lua_State;
#endif

/* ============================================================================
 * Initialization
 * ============================================================================ */

/**
 * @brief Initialize the Link subsystem.
 * @param initial_bpm Starting tempo (typically 120.0).
 * @return 0 on success, -1 on error.
 */
int shared_link_init(double initial_bpm);

/**
 * @brief Cleanup the Link subsystem.
 */
void shared_link_cleanup(void);

/**
 * @brief Check if Link is initialized.
 * @return Non-zero if initialized, 0 otherwise.
 */
int shared_link_is_initialized(void);

/* ============================================================================
 * Enable/Disable
 * ============================================================================ */

/**
 * @brief Enable or disable Link network synchronization.
 * @param enable 1 to enable, 0 to disable.
 */
void shared_link_enable(int enable);

/**
 * @brief Check if Link is currently enabled.
 * @return Non-zero if enabled, 0 otherwise.
 */
int shared_link_is_enabled(void);

/* ============================================================================
 * Tempo
 * ============================================================================ */

/**
 * @brief Get current Link session tempo.
 * @return Tempo in BPM.
 */
double shared_link_get_tempo(void);

/**
 * @brief Set Link session tempo (propagates to all peers).
 * @param bpm Tempo in beats per minute.
 */
void shared_link_set_tempo(double bpm);

/**
 * @brief Get effective tempo for playback.
 * Returns Link tempo if enabled, otherwise returns the fallback.
 *
 * @param fallback_tempo Tempo to use if Link is not enabled.
 * @return Tempo in BPM.
 */
double shared_link_effective_tempo(double fallback_tempo);

/* ============================================================================
 * Beat/Phase
 * ============================================================================ */

/**
 * @brief Get current beat position in Link session.
 * @param quantum Beat subdivision (typically 4 for 4/4 time).
 * @return Beat position (fractional).
 */
double shared_link_get_beat(double quantum);

/**
 * @brief Get current phase within quantum.
 * Phase is in range [0, quantum).
 *
 * @param quantum Beat subdivision.
 * @return Phase position.
 */
double shared_link_get_phase(double quantum);

/* ============================================================================
 * Transport (Start/Stop Sync)
 * ============================================================================ */

/**
 * @brief Enable or disable start/stop synchronization.
 * @param enable 1 to enable, 0 to disable.
 */
void shared_link_enable_start_stop_sync(int enable);

/**
 * @brief Check if start/stop sync is enabled.
 * @return Non-zero if enabled, 0 otherwise.
 */
int shared_link_is_start_stop_sync_enabled(void);

/**
 * @brief Get transport playing state.
 * @return Non-zero if playing, 0 if stopped.
 */
int shared_link_is_playing(void);

/**
 * @brief Set transport playing state.
 * @param playing 1 to start, 0 to stop.
 */
void shared_link_set_playing(int playing);

/* ============================================================================
 * Peer Information
 * ============================================================================ */

/**
 * @brief Get number of connected Link peers.
 * @return Number of peers (excluding this instance).
 */
uint64_t shared_link_num_peers(void);

/* ============================================================================
 * Callbacks (Optional - for editor integration)
 * ============================================================================ */

/**
 * @brief Callback function type for peer count changes.
 */
typedef void (*shared_link_peers_callback_t)(uint64_t num_peers, void* userdata);

/**
 * @brief Callback function type for tempo changes.
 */
typedef void (*shared_link_tempo_callback_t)(double tempo, void* userdata);

/**
 * @brief Callback function type for transport changes.
 */
typedef void (*shared_link_transport_callback_t)(int is_playing, void* userdata);

/**
 * @brief Set callback for peer count changes.
 * @param callback Function to call, or NULL to clear.
 * @param userdata User data passed to callback.
 */
void shared_link_set_peers_callback(shared_link_peers_callback_t callback, void* userdata);

/**
 * @brief Set callback for tempo changes.
 * @param callback Function to call, or NULL to clear.
 * @param userdata User data passed to callback.
 */
void shared_link_set_tempo_callback(shared_link_tempo_callback_t callback, void* userdata);

/**
 * @brief Set callback for transport changes.
 * @param callback Function to call, or NULL to clear.
 * @param userdata User data passed to callback.
 */
void shared_link_set_transport_callback(shared_link_transport_callback_t callback, void* userdata);

/**
 * @brief Poll Link state and invoke pending callbacks.
 * Should be called from the main loop.
 */
void shared_link_check_callbacks(void);

#ifdef __cplusplus
}
#endif

#endif /* SHARED_LINK_H */
