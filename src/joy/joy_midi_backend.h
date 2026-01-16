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
