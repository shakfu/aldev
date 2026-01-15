/* loki_link.h - Ableton Link integration for Loki
 *
 * Provides tempo synchronization with other Link-enabled applications
 * on the local network (Ableton Live, hardware devices, etc.).
 *
 * Link is a technology for synchronizing musical beat, tempo, and phase
 * across multiple applications running on one or more devices.
 */

#ifndef LOKI_LINK_H
#define LOKI_LINK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations - conditional to avoid typedef redefinition warnings */
#ifndef LOKI_CORE_H
struct editor_ctx;
typedef struct editor_ctx editor_ctx_t;
#endif

#ifndef lua_h
struct lua_State;
#endif

/* ============================================================================
 * Initialization
 * ============================================================================ */

/**
 * Initialize the Link subsystem.
 * Creates Link instance with specified initial tempo.
 *
 * @param ctx Editor context (may be NULL for standalone use)
 * @param initial_bpm Starting tempo (typically 120.0)
 * @return 0 on success, -1 on error
 */
int loki_link_init(editor_ctx_t *ctx, double initial_bpm);

/**
 * Cleanup the Link subsystem.
 * Disconnects from Link session and releases resources.
 *
 * @param ctx Editor context
 */
void loki_link_cleanup(editor_ctx_t *ctx);

/**
 * Check if Link is initialized.
 *
 * @param ctx Editor context
 * @return 1 if initialized, 0 otherwise
 */
int loki_link_is_initialized(editor_ctx_t *ctx);

/* ============================================================================
 * Enable/Disable
 * ============================================================================ */

/**
 * Enable Link network synchronization.
 * When enabled, begins discovering and connecting to other Link peers.
 *
 * @param ctx Editor context
 * @param enable 1 to enable, 0 to disable
 */
void loki_link_enable(editor_ctx_t *ctx, int enable);

/**
 * Check if Link is currently enabled.
 *
 * @param ctx Editor context
 * @return 1 if enabled, 0 otherwise
 */
int loki_link_is_enabled(editor_ctx_t *ctx);

/* ============================================================================
 * Tempo
 * ============================================================================ */

/**
 * Get current Link session tempo.
 *
 * @param ctx Editor context
 * @return Tempo in BPM
 */
double loki_link_get_tempo(editor_ctx_t *ctx);

/**
 * Set Link session tempo (propagates to all peers).
 *
 * @param ctx Editor context
 * @param bpm Tempo in beats per minute
 */
void loki_link_set_tempo(editor_ctx_t *ctx, double bpm);

/* ============================================================================
 * Beat/Phase
 * ============================================================================ */

/**
 * Get current beat position in Link session.
 *
 * @param ctx Editor context
 * @param quantum Beat subdivision (typically 4 for 4/4 time)
 * @return Beat position (fractional)
 */
double loki_link_get_beat(editor_ctx_t *ctx, double quantum);

/**
 * Get current phase within quantum.
 * Phase is in range [0, quantum).
 *
 * @param ctx Editor context
 * @param quantum Beat subdivision
 * @return Phase position
 */
double loki_link_get_phase(editor_ctx_t *ctx, double quantum);

/* ============================================================================
 * Transport (Start/Stop Sync)
 * ============================================================================ */

/**
 * Enable start/stop synchronization.
 * When enabled, transport state is shared with other peers.
 *
 * @param ctx Editor context
 * @param enable 1 to enable, 0 to disable
 */
void loki_link_enable_start_stop_sync(editor_ctx_t *ctx, int enable);

/**
 * Check if start/stop sync is enabled.
 *
 * @param ctx Editor context
 * @return 1 if enabled, 0 otherwise
 */
int loki_link_is_start_stop_sync_enabled(editor_ctx_t *ctx);

/**
 * Get transport playing state.
 *
 * @param ctx Editor context
 * @return 1 if playing, 0 if stopped
 */
int loki_link_is_playing(editor_ctx_t *ctx);

/**
 * Set transport playing state.
 *
 * @param ctx Editor context
 * @param playing 1 to start, 0 to stop
 */
void loki_link_set_playing(editor_ctx_t *ctx, int playing);

/* ============================================================================
 * Peer Information
 * ============================================================================ */

/**
 * Get number of connected Link peers.
 *
 * @param ctx Editor context
 * @return Number of peers (excluding this instance)
 */
uint64_t loki_link_num_peers(editor_ctx_t *ctx);

/* ============================================================================
 * Callbacks
 * ============================================================================ */

/**
 * Register Lua callback for peer count changes.
 * Callback signature: function(num_peers)
 *
 * @param ctx Editor context
 * @param callback_name Name of global Lua function (or NULL to clear)
 */
void loki_link_set_peers_callback(editor_ctx_t *ctx, const char *callback_name);

/**
 * Register Lua callback for tempo changes.
 * Callback signature: function(tempo)
 *
 * @param ctx Editor context
 * @param callback_name Name of global Lua function (or NULL to clear)
 */
void loki_link_set_tempo_callback(editor_ctx_t *ctx, const char *callback_name);

/**
 * Register Lua callback for transport start/stop changes.
 * Callback signature: function(is_playing)
 *
 * @param ctx Editor context
 * @param callback_name Name of global Lua function (or NULL to clear)
 */
void loki_link_set_start_stop_callback(editor_ctx_t *ctx, const char *callback_name);

/* ============================================================================
 * Main Loop Integration
 * ============================================================================ */

/**
 * Poll Link state and invoke pending callbacks.
 * Should be called from the editor's main loop.
 *
 * @param ctx Editor context
 * @param L Lua state for callback invocation
 */
void loki_link_check_callbacks(editor_ctx_t *ctx, struct lua_State *L);

/* ============================================================================
 * Timing Integration
 * ============================================================================ */

/**
 * Get tempo for playback timing.
 * Returns Link tempo if enabled, otherwise returns the provided fallback.
 * This is the primary integration point with the async playback system.
 *
 * @param ctx Editor context
 * @param fallback_tempo Tempo to use if Link is not enabled
 * @return Tempo in BPM
 */
double loki_link_effective_tempo(editor_ctx_t *ctx, double fallback_tempo);

#ifdef __cplusplus
}
#endif

#endif /* LOKI_LINK_H */
