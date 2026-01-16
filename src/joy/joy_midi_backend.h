/**
 * @file joy_midi_backend.h
 * @brief MIDI backend wrapper for Joy language.
 *
 * Provides the MIDI interface that Joy's primitives expect,
 * backed by psnd's shared MIDI backend.
 */

#ifndef JOY_MIDI_BACKEND_H
#define JOY_MIDI_BACKEND_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize Joy MIDI backend.
 * @return 0 on success, -1 on error.
 */
int joy_midi_init(void);

/**
 * @brief Cleanup Joy MIDI backend.
 */
void joy_midi_cleanup(void);

/**
 * @brief List available MIDI output ports.
 */
void joy_midi_list_ports(void);

/**
 * @brief Open a MIDI output port by index.
 * @param port_idx Port index (0-based).
 * @return 0 on success, -1 on error.
 */
int joy_midi_open_port(int port_idx);

/**
 * @brief Create a virtual MIDI output port.
 * @param name Port name.
 * @return 0 on success, -1 on error.
 */
int joy_midi_open_virtual(const char* name);

/**
 * @brief Close the current MIDI output.
 */
void joy_midi_close(void);

/**
 * @brief Check if MIDI output is open.
 * @return Non-zero if open, 0 if closed.
 */
int joy_midi_is_open(void);

/**
 * @brief Set current MIDI channel.
 * @param channel MIDI channel (1-16).
 */
void joy_midi_set_channel(int channel);

/**
 * @brief Get current MIDI channel.
 * @return Current channel (1-16).
 */
int joy_midi_get_channel(void);

/**
 * @brief Send a note-on message on current channel.
 * @param pitch Note pitch (0-127).
 * @param velocity Note velocity (0-127).
 */
void joy_midi_note_on(int pitch, int velocity);

/**
 * @brief Send a note-off message on current channel.
 * @param pitch Note pitch (0-127).
 */
void joy_midi_note_off(int pitch);

/**
 * @brief Send a note-on message on specific channel.
 * @param channel MIDI channel (1-16).
 * @param pitch Note pitch (0-127).
 * @param velocity Note velocity (0-127).
 */
void joy_midi_note_on_ch(int channel, int pitch, int velocity);

/**
 * @brief Send a note-off message on specific channel.
 * @param channel MIDI channel (1-16).
 * @param pitch Note pitch (0-127).
 */
void joy_midi_note_off_ch(int channel, int pitch);

/**
 * @brief Send a program change message.
 * @param channel MIDI channel (1-16).
 * @param program GM program number (0-127).
 */
void joy_midi_program(int channel, int program);

/**
 * @brief Send a control change message.
 * @param channel MIDI channel (1-16).
 * @param cc Controller number (0-127).
 * @param value Controller value (0-127).
 */
void joy_midi_cc(int channel, int cc, int value);

/**
 * @brief Send all notes off on all channels.
 */
void joy_midi_panic(void);

/**
 * @brief Sleep for a specified number of milliseconds.
 * @param ms Milliseconds to sleep.
 */
void joy_midi_sleep_ms(int ms);

/* ============================================================================
 * TSF Backend Control
 * ============================================================================ */

/**
 * @brief Load a SoundFont file for TSF playback.
 * @param path Path to the .sf2 file.
 * @return 0 on success, -1 on error.
 */
int joy_tsf_load_soundfont(const char* path);

/**
 * @brief Enable TSF synthesis.
 * @return 0 on success, -1 on error.
 */
int joy_tsf_enable(void);

/**
 * @brief Disable TSF synthesis.
 */
void joy_tsf_disable(void);

/**
 * @brief Check if TSF is enabled.
 * @return Non-zero if enabled.
 */
int joy_tsf_is_enabled(void);

/* ============================================================================
 * Csound Backend Control
 * ============================================================================ */

/**
 * @brief Initialize Csound backend.
 * @return 0 on success, -1 on error.
 */
int joy_csound_init(void);

/**
 * @brief Cleanup Csound backend.
 */
void joy_csound_cleanup(void);

/**
 * @brief Load a CSD file for Csound synthesis.
 * @param path Path to the .csd file.
 * @return 0 on success, -1 on error.
 */
int joy_csound_load(const char* path);

/**
 * @brief Enable Csound synthesis.
 * @return 0 on success, -1 on error.
 */
int joy_csound_enable(void);

/**
 * @brief Disable Csound synthesis.
 */
void joy_csound_disable(void);

/**
 * @brief Check if Csound is enabled.
 * @return Non-zero if enabled.
 */
int joy_csound_is_enabled(void);

/**
 * @brief Play a CSD file (blocking).
 * @param path Path to the .csd file.
 * @param verbose Print progress if non-zero.
 * @return 0 on success, -1 on error.
 */
int joy_csound_play_file(const char* path, int verbose);

/**
 * @brief Get last Csound error message.
 * @return Error message or NULL.
 */
const char* joy_csound_get_error(void);

/* ============================================================================
 * Ableton Link Support
 * ============================================================================ */

/**
 * @brief Initialize Link with a starting tempo.
 * @param bpm Initial tempo in beats per minute.
 * @return 0 on success, -1 on error.
 */
int joy_link_init(double bpm);

/**
 * @brief Cleanup Link subsystem.
 */
void joy_link_cleanup(void);

/**
 * @brief Enable Link network synchronization.
 * @return 0 on success, -1 if not initialized.
 */
int joy_link_enable(void);

/**
 * @brief Disable Link network synchronization.
 */
void joy_link_disable(void);

/**
 * @brief Check if Link is enabled.
 * @return Non-zero if enabled.
 */
int joy_link_is_enabled(void);

/**
 * @brief Get current Link tempo.
 * @return Tempo in BPM, or 0 if not initialized.
 */
double joy_link_get_tempo(void);

/**
 * @brief Set Link tempo (propagates to all peers).
 * @param bpm Tempo in beats per minute.
 */
void joy_link_set_tempo(double bpm);

/**
 * @brief Get current beat position.
 * @param quantum Beat subdivision (typically 4.0 for 4/4).
 * @return Beat position (fractional).
 */
double joy_link_get_beat(double quantum);

/**
 * @brief Get current phase within quantum.
 * @param quantum Beat subdivision.
 * @return Phase in range [0, quantum).
 */
double joy_link_get_phase(double quantum);

/**
 * @brief Get number of connected Link peers.
 * @return Number of peers (excluding this instance).
 */
int joy_link_num_peers(void);

/* ============================================================================
 * Shared Context Access
 * ============================================================================ */

/* Forward declaration */
struct SharedContext;

/**
 * @brief Get Joy's shared context.
 * @return Pointer to shared context, or NULL if not initialized.
 */
struct SharedContext* joy_get_shared_context(void);

/**
 * @brief Set Joy's shared context (for editor integration).
 * @param ctx Shared context to use (NULL to release).
 */
void joy_set_shared_context(struct SharedContext* ctx);

#ifdef __cplusplus
}
#endif

#endif /* JOY_MIDI_BACKEND_H */
