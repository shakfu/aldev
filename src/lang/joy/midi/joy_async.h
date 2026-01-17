/**
 * @file joy_async.h
 * @brief Asynchronous MIDI playback for Joy interpreter.
 *
 * This module enables non-blocking playback using libuv, allowing the REPL
 * to remain responsive while music plays. Uses the MidiSchedule system from
 * midi_primitives.h with timer-based event dispatch.
 */

#ifndef JOY_ASYNC_H
#define JOY_ASYNC_H

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations - avoid including full headers to minimize dependencies */
struct MidiSchedule;
struct MusicContext;

/* ============================================================================
 * Async Playback API
 * ============================================================================ */

/**
 * @brief Initialize the async playback system.
 *
 * Creates a libuv event loop in a background thread.
 * Must be called before any async playback.
 *
 * @return 0 on success, -1 on error.
 */
int joy_async_init(void);

/**
 * @brief Cleanup the async playback system.
 *
 * Stops all playback and shuts down the background thread.
 */
void joy_async_cleanup(void);

/**
 * @brief Play a schedule asynchronously.
 *
 * Copies the schedule and plays it in the background, returning immediately.
 * The REPL remains responsive while notes play.
 *
 * @param sched MidiSchedule to play (will be deep-copied).
 * @param mctx MusicContext for MIDI output access.
 * @return 0 on success, -1 on error.
 */
int joy_async_play(struct MidiSchedule* sched, struct MusicContext* mctx);

/**
 * @brief Stop all async playback.
 *
 * Sends all-notes-off and stops the timer-based playback.
 */
void joy_async_stop(void);

/**
 * @brief Check if async playback is active.
 * @return Non-zero if events are still playing.
 */
int joy_async_is_playing(void);

/**
 * @brief Wait for current async playback to complete.
 * @param timeout_ms Maximum time to wait in milliseconds, 0 = infinite.
 * @return 0 if playback completed, -1 if timed out.
 */
int joy_async_wait(int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* JOY_ASYNC_H */
