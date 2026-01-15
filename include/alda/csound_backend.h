/**
 * @file csound_backend.h
 * @brief Csound synthesis backend for Alda.
 *
 * Provides advanced synthesis capabilities using embedded Csound.
 * Audio is rendered to buffers for integration with the host audio system
 * (miniaudio). MIDI events are translated to Csound score events.
 *
 * Build with -DBUILD_CSOUND_BACKEND=ON to enable.
 */

#ifndef ALDA_CSOUND_BACKEND_H
#define ALDA_CSOUND_BACKEND_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Initialization and Cleanup
 * ============================================================================ */

/**
 * @brief Initialize the Csound backend.
 *
 * Creates a Csound instance configured for host-implemented audio I/O.
 * Must be called before any other csound_backend functions.
 *
 * @return 0 on success, -1 on error.
 */
int alda_csound_init(void);

/**
 * @brief Cleanup Csound resources.
 *
 * Stops performance and destroys the Csound instance.
 * Safe to call even if not initialized.
 */
void alda_csound_cleanup(void);

/* ============================================================================
 * Instrument Loading
 * ============================================================================ */

/**
 * @brief Load instruments from a CSD file.
 *
 * Compiles the CSD and prepares Csound for performance.
 * This replaces any previously loaded instruments.
 *
 * @param path Path to the .csd file.
 * @return 0 on success, -1 on error.
 */
int alda_csound_load_csd(const char* path);

/**
 * @brief Compile orchestra code directly.
 *
 * Compiles additional orchestra code into the running Csound instance.
 * Can be used for hot-reloading instruments during performance.
 *
 * @param orc Csound orchestra code string.
 * @return 0 on success, -1 on error.
 */
int alda_csound_compile_orc(const char* orc);

/**
 * @brief Check if a CSD/orchestra has been loaded.
 * @return Non-zero if instruments are loaded, 0 if not.
 */
int alda_csound_has_instruments(void);

/* ============================================================================
 * Enable/Disable
 * ============================================================================ */

/**
 * @brief Enable the Csound backend (starts performance).
 *
 * Csound must have instruments loaded before enabling.
 *
 * @return 0 on success, -1 on error (e.g., no instruments loaded).
 */
int alda_csound_enable(void);

/**
 * @brief Disable the Csound backend (pauses performance).
 *
 * Audio rendering stops but state is preserved.
 * Can be re-enabled without reloading instruments.
 */
void alda_csound_disable(void);

/**
 * @brief Check if the Csound backend is enabled.
 * @return Non-zero if enabled, 0 if disabled.
 */
int alda_csound_is_enabled(void);

/* ============================================================================
 * MIDI Message Sending
 * ============================================================================ */

/**
 * @brief Send a note-on message.
 *
 * Triggers a Csound instrument corresponding to the MIDI channel.
 * Instrument number = channel (1-16).
 *
 * @param channel MIDI channel (1-16).
 * @param pitch Note pitch (0-127).
 * @param velocity Note velocity (0-127).
 */
void alda_csound_send_note_on(int channel, int pitch, int velocity);

/**
 * @brief Send a note-on message with frequency (for microtuning).
 *
 * Like alda_csound_send_note_on but accepts frequency in Hz instead of
 * MIDI pitch. Used for Scala scale microtuning support.
 *
 * The Csound instrument receives frequency as p4 (instead of MIDI pitch).
 * Instruments should check if p4 > 200 to distinguish frequency from pitch.
 *
 * @param channel MIDI channel (1-16).
 * @param freq Frequency in Hz.
 * @param velocity Note velocity (0-127).
 * @param midi_pitch Original MIDI pitch (for note tracking/release).
 */
void alda_csound_send_note_on_freq(int channel, double freq, int velocity, int midi_pitch);

/**
 * @brief Send a note-off message.
 *
 * Turns off the note with the specified pitch on the channel.
 *
 * @param channel MIDI channel (1-16).
 * @param pitch Note pitch (0-127).
 */
void alda_csound_send_note_off(int channel, int pitch);

/**
 * @brief Send a program change message.
 *
 * Sets the instrument for subsequent notes on the channel.
 * Implementation may use massign or switch instrument numbers.
 *
 * @param channel MIDI channel (1-16).
 * @param program GM program number (0-127).
 */
void alda_csound_send_program(int channel, int program);

/**
 * @brief Send a control change message.
 *
 * Updates a control channel in Csound.
 * Instruments can read this via chnget.
 *
 * @param channel MIDI channel (1-16).
 * @param cc Controller number (0-127).
 * @param value Controller value (0-127).
 */
void alda_csound_send_cc(int channel, int cc, int value);

/**
 * @brief Send pitch bend message.
 *
 * @param channel MIDI channel (1-16).
 * @param bend Pitch bend value (-8192 to 8191, 0 = center).
 */
void alda_csound_send_pitch_bend(int channel, int bend);

/**
 * @brief Send all notes off on all channels.
 *
 * Immediately silences all playing notes.
 */
void alda_csound_all_notes_off(void);

/* ============================================================================
 * Audio Rendering
 * ============================================================================ */

/**
 * @brief Get the sample rate configured in Csound.
 * @return Sample rate in Hz, or 0 if not initialized.
 */
int alda_csound_get_sample_rate(void);

/**
 * @brief Get the number of output channels.
 * @return Number of channels (typically 2), or 0 if not initialized.
 */
int alda_csound_get_channels(void);

/**
 * @brief Render audio samples.
 *
 * Called by the audio callback (e.g., miniaudio) to get audio data.
 * Renders the specified number of frames of interleaved stereo float samples.
 *
 * @param output Buffer to write interleaved float samples.
 * @param frames Number of frames to render.
 */
void alda_csound_render(float* output, int frames);

/* ============================================================================
 * Error Handling
 * ============================================================================ */

/**
 * @brief Get the last error message.
 * @return Error message string, or NULL if no error.
 */
const char* alda_csound_get_error(void);

/* ============================================================================
 * Standalone Playback
 * ============================================================================ */

/**
 * @brief Play a CSD file with its own score section (blocking).
 *
 * Unlike the MIDI-driven mode (load_csd + enable), this plays the CSD file's
 * embedded score section directly. The function blocks until playback completes
 * or an error occurs.
 *
 * @param path Path to the .csd file.
 * @param verbose If non-zero, print progress messages to stdout.
 * @return 0 on success, -1 on error.
 */
int alda_csound_play_file(const char* path, int verbose);

/**
 * @brief Start playing a CSD file asynchronously (non-blocking).
 *
 * Starts playback and returns immediately. Use alda_csound_playback_active()
 * to check if playback is still running, and alda_csound_stop_playback() to stop.
 *
 * @param path Path to the .csd file.
 * @return 0 on success, -1 on error.
 */
int alda_csound_play_file_async(const char* path);

/**
 * @brief Check if async playback is active.
 * @return Non-zero if playback is running, 0 if stopped or not started.
 */
int alda_csound_playback_active(void);

/**
 * @brief Stop async playback.
 *
 * Stops any currently running async playback and cleans up resources.
 */
void alda_csound_stop_playback(void);

#ifdef __cplusplus
}
#endif

#endif /* ALDA_CSOUND_BACKEND_H */
