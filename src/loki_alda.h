/* loki_alda.h - Alda music language integration for Loki
 *
 * Provides livecoding capabilities by integrating the Alda music notation
 * language with the Loki editor. Supports async playback with callbacks,
 * matching the existing loki.async_http pattern.
 *
 * Architecture:
 * - Playback uses alda's built-in libuv-based async system
 * - Callbacks are polled and processed in the main loop
 * - Multiple concurrent playbacks supported via slots
 *
 * Usage from Lua:
 *   loki.alda.init()
 *   loki.alda.eval("piano: c d e f g", "on_complete")
 *   loki.alda.stop()
 *   loki.alda.cleanup()
 */

#ifndef LOKI_ALDA_H
#define LOKI_ALDA_H

/* Forward declarations to avoid including heavy headers */
struct editor_ctx;
typedef struct editor_ctx editor_ctx_t;
struct lua_State;
typedef struct lua_State lua_State;

/* Maximum concurrent playback slots */
#define LOKI_ALDA_MAX_SLOTS 8

/* Playback status codes */
typedef enum {
    LOKI_ALDA_STATUS_IDLE = 0,
    LOKI_ALDA_STATUS_PLAYING,
    LOKI_ALDA_STATUS_STOPPED,
    LOKI_ALDA_STATUS_ERROR,
    LOKI_ALDA_STATUS_COMPLETE
} LokiAldaStatus;

/* Callback result passed to Lua */
typedef struct {
    LokiAldaStatus status;
    int slot_id;
    char *error_msg;      /* NULL if no error */
    int events_played;    /* Number of MIDI events played */
    int duration_ms;      /* Total playback duration */
} LokiAldaResult;

/* ======================= Initialization ======================= */

/**
 * Initialize the Alda subsystem.
 * Must be called before any other alda functions.
 * Creates the playback thread and initializes MIDI output.
 *
 * @param ctx Editor context
 * @param port_name Name for the virtual MIDI port (or NULL for "Loki")
 * @return 0 on success, -1 on error
 */
int loki_alda_init(editor_ctx_t *ctx, const char *port_name);

/**
 * Cleanup the Alda subsystem.
 * Stops all playback and releases resources.
 *
 * @param ctx Editor context
 */
void loki_alda_cleanup(editor_ctx_t *ctx);

/**
 * Check if Alda is initialized.
 *
 * @param ctx Editor context
 * @return 1 if initialized, 0 otherwise
 */
int loki_alda_is_initialized(editor_ctx_t *ctx);

/* ======================= Playback Control ======================= */

/**
 * Evaluate and play Alda code asynchronously.
 * Returns immediately; callback is invoked when playback completes.
 *
 * @param ctx Editor context
 * @param code Alda notation string (e.g., "piano: c d e f g")
 * @param lua_callback Name of Lua function to call on completion (or NULL)
 * @return Slot ID (0-7) on success, -1 if no slots available, -2 on parse error
 */
int loki_alda_eval_async(editor_ctx_t *ctx, const char *code, const char *lua_callback);

/**
 * Evaluate and play Alda code synchronously (blocking).
 * Use for short sequences or when immediate feedback is needed.
 *
 * @param ctx Editor context
 * @param code Alda notation string
 * @return 0 on success, -1 on error
 */
int loki_alda_eval_sync(editor_ctx_t *ctx, const char *code);

/**
 * Stop playback in a specific slot.
 *
 * @param ctx Editor context
 * @param slot_id Slot to stop (0-7), or -1 to stop all
 */
void loki_alda_stop(editor_ctx_t *ctx, int slot_id);

/**
 * Stop all active playback.
 *
 * @param ctx Editor context
 */
void loki_alda_stop_all(editor_ctx_t *ctx);

/* ======================= Status Queries ======================= */

/**
 * Get the status of a playback slot.
 *
 * @param ctx Editor context
 * @param slot_id Slot to query (0-7)
 * @return Status code
 */
LokiAldaStatus loki_alda_get_status(editor_ctx_t *ctx, int slot_id);

/**
 * Check if any slot is currently playing.
 *
 * @param ctx Editor context
 * @return 1 if playing, 0 otherwise
 */
int loki_alda_is_playing(editor_ctx_t *ctx);

/**
 * Get the number of active playback slots.
 *
 * @param ctx Editor context
 * @return Number of slots currently playing
 */
int loki_alda_active_count(editor_ctx_t *ctx);

/* ======================= Configuration ======================= */

/**
 * Set global tempo (BPM).
 *
 * @param ctx Editor context
 * @param bpm Beats per minute (20-400)
 */
void loki_alda_set_tempo(editor_ctx_t *ctx, int bpm);

/**
 * Get current global tempo.
 *
 * @param ctx Editor context
 * @return Current BPM
 */
int loki_alda_get_tempo(editor_ctx_t *ctx);

/**
 * Enable/disable built-in TinySoundFont synthesizer.
 * When disabled, output goes to MIDI port only.
 *
 * @param ctx Editor context
 * @param enable 1 to enable, 0 to disable
 * @return 0 on success, -1 if soundfont not loaded
 */
int loki_alda_set_synth_enabled(editor_ctx_t *ctx, int enable);

/**
 * Load a SoundFont file for the built-in synthesizer.
 *
 * @param ctx Editor context
 * @param path Path to .sf2 file
 * @return 0 on success, -1 on error
 */
int loki_alda_load_soundfont(editor_ctx_t *ctx, const char *path);

/* ======================= Main Loop Integration ======================= */

/**
 * Check for completed async operations and invoke callbacks.
 * Should be called from the editor's main loop.
 *
 * @param ctx Editor context
 * @param L Lua state for callback invocation
 */
void loki_alda_check_callbacks(editor_ctx_t *ctx, lua_State *L);

/* ======================= Utility Functions ======================= */

/**
 * List available MIDI output ports.
 *
 * @param ctx Editor context
 * @param ports Array to fill with port names (caller allocates)
 * @param max_ports Maximum number of ports to return
 * @return Number of ports found
 */
int loki_alda_list_ports(editor_ctx_t *ctx, char **ports, int max_ports);

/**
 * Get the last error message.
 *
 * @param ctx Editor context
 * @return Error message string (do not free)
 */
const char *loki_alda_get_error(editor_ctx_t *ctx);

#endif /* LOKI_ALDA_H */
