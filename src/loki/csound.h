/* csound.h - Editor-level Csound backend control
 *
 * Language-agnostic Csound control for the editor.
 * Uses the shared Csound backend directly without going through
 * language-specific bridges (Alda, Joy).
 *
 * This allows the :csd command to work regardless of which language
 * is active in the editor.
 */

#ifndef LOKI_CSOUND_H
#define LOKI_CSOUND_H

/**
 * Check if Csound backend is available (compiled in).
 *
 * @return 1 if available, 0 if not compiled
 */
int loki_csound_is_available(void);

/**
 * Load a Csound .csd file.
 *
 * @param path Path to .csd file
 * @return 0 on success, -1 on error
 */
int loki_csound_load(const char *path);

/**
 * Enable Csound synthesis.
 * Csound must have instruments loaded before enabling.
 * Disables TSF when Csound is enabled (mutually exclusive).
 *
 * @return 0 on success, -1 on error
 */
int loki_csound_enable(void);

/**
 * Disable Csound synthesis.
 */
void loki_csound_disable(void);

/**
 * Check if Csound is currently enabled.
 *
 * @return 1 if enabled, 0 if disabled
 */
int loki_csound_is_enabled(void);

/**
 * Check if Csound has instruments loaded.
 *
 * @return 1 if instruments loaded, 0 if not
 */
int loki_csound_has_instruments(void);

/**
 * Play a standalone CSD file asynchronously.
 * This plays the CSD's embedded score section (not MIDI-driven).
 * Returns immediately; use loki_csound_playback_active() to check status.
 *
 * @param path Path to the .csd file
 * @return 0 on success, -1 on error
 */
int loki_csound_play_async(const char *path);

/**
 * Check if async CSD playback is currently active.
 *
 * @return 1 if active, 0 if not
 */
int loki_csound_playback_active(void);

/**
 * Stop async CSD playback.
 */
void loki_csound_stop_playback(void);

/**
 * Get the last error message from Csound backend.
 *
 * @return Error message or NULL if no error
 */
const char *loki_csound_get_error(void);

#endif /* LOKI_CSOUND_H */
