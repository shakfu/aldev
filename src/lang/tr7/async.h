/**
 * @file tr7_async.h
 * @brief Asynchronous MIDI playback for TR7 Scheme REPL.
 *
 * Thin wrapper around the shared async playback service, providing
 * non-blocking note and chord playback for the TR7 REPL.
 */

#ifndef ASYNC_H
#define ASYNC_H

#include "shared/context.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Async Playback API
 * ============================================================================ */

/**
 * @brief Initialize the async playback system.
 * @return 0 on success, -1 on error.
 */
int tr7_async_init(void);

/**
 * @brief Cleanup the async playback system.
 */
void tr7_async_cleanup(void);

/**
 * @brief Play a single note asynchronously.
 *
 * If Ableton Link is enabled, the duration is scaled based on Link tempo.
 *
 * @param shared SharedContext for MIDI output.
 * @param channel MIDI channel (0-15).
 * @param pitch MIDI pitch (0-127).
 * @param velocity Note velocity (0-127).
 * @param duration_ms Duration in milliseconds (at local_tempo).
 * @param local_tempo Local tempo in BPM used to calculate duration_ms.
 * @return 0 on success, -1 on error.
 */
int tr7_async_play_note(SharedContext* shared, int channel, int pitch,
                        int velocity, int duration_ms, int local_tempo);

/**
 * @brief Play a chord asynchronously.
 *
 * If Ableton Link is enabled, the duration is scaled based on Link tempo.
 *
 * @param shared SharedContext for MIDI output.
 * @param channel MIDI channel (0-15).
 * @param pitches Array of MIDI pitches.
 * @param count Number of notes in the chord.
 * @param velocity Note velocity (0-127).
 * @param duration_ms Duration in milliseconds (at local_tempo).
 * @param local_tempo Local tempo in BPM used to calculate duration_ms.
 * @return 0 on success, -1 on error.
 */
int tr7_async_play_chord(SharedContext* shared, int channel,
                         const int* pitches, int count,
                         int velocity, int duration_ms, int local_tempo);

/**
 * @brief Play a sequence of notes asynchronously.
 *
 * Notes are played one after another, each starting when the previous ends.
 * If Ableton Link is enabled, the duration is scaled based on Link tempo.
 *
 * @param shared SharedContext for MIDI output.
 * @param channel MIDI channel (0-15).
 * @param pitches Array of MIDI pitches.
 * @param count Number of notes in the sequence.
 * @param velocity Note velocity (0-127).
 * @param duration_ms Duration of each note in milliseconds (at local_tempo).
 * @param local_tempo Local tempo in BPM used to calculate duration_ms.
 * @return 0 on success, -1 on error.
 */
int tr7_async_play_sequence(SharedContext* shared, int channel,
                            const int* pitches, int count,
                            int velocity, int duration_ms, int local_tempo);

/**
 * @brief Stop all async playback.
 */
void tr7_async_stop(void);

/**
 * @brief Check if async playback is active.
 * @return Non-zero if events are still playing.
 */
int tr7_async_is_playing(void);

/**
 * @brief Wait for all async playback to complete.
 * @param timeout_ms Maximum time to wait (0 = infinite).
 * @return 0 if completed, -1 if timed out.
 */
int tr7_async_wait(int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* ASYNC_H */
