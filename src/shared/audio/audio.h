/**
 * @file audio.h
 * @brief Shared audio backend API (TinySoundFont, Csound).
 *
 * These backends are singletons - only one instance exists globally.
 * The enable flags in SharedContext determine whether to route events
 * to each backend.
 */

#ifndef SHARED_AUDIO_H
#define SHARED_AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * TinySoundFont Backend
 * ============================================================================ */

/**
 * @brief Initialize the TSF backend.
 * @return 0 on success, -1 on error.
 */
int shared_tsf_init(void);

/**
 * @brief Cleanup TSF resources.
 */
void shared_tsf_cleanup(void);

/**
 * @brief Load a SoundFont file (.sf2).
 * @param path Path to the .sf2 file.
 * @return 0 on success, -1 on error.
 */
int shared_tsf_load_soundfont(const char* path);

/**
 * @brief Check if a soundfont is loaded.
 * @return Non-zero if loaded, 0 if not.
 */
int shared_tsf_has_soundfont(void);

/**
 * @brief Get the number of presets in the loaded soundfont.
 * @return Number of presets, or 0 if no soundfont loaded.
 */
int shared_tsf_get_preset_count(void);

/**
 * @brief Get the name of a preset by index.
 * @param index Preset index (0 to preset_count-1).
 * @return Preset name, or NULL if invalid index.
 */
const char* shared_tsf_get_preset_name(int index);

/**
 * @brief Enable the TSF synth (starts audio output).
 * @return 0 on success, -1 on error.
 */
int shared_tsf_enable(void);

/**
 * @brief Disable the TSF synth (stops audio output).
 */
void shared_tsf_disable(void);

/**
 * @brief Check if TSF is enabled.
 * @return Non-zero if enabled, 0 if disabled.
 */
int shared_tsf_is_enabled(void);

/**
 * @brief Send a note-on message to TSF.
 * @param channel MIDI channel (1-16).
 * @param pitch Note pitch (0-127).
 * @param velocity Note velocity (0-127).
 */
void shared_tsf_send_note_on(int channel, int pitch, int velocity);

/**
 * @brief Send a note-off message to TSF.
 * @param channel MIDI channel (1-16).
 * @param pitch Note pitch (0-127).
 */
void shared_tsf_send_note_off(int channel, int pitch);

/**
 * @brief Send a program change to TSF.
 * @param channel MIDI channel (1-16).
 * @param program GM program number (0-127).
 */
void shared_tsf_send_program(int channel, int program);

/**
 * @brief Send a control change to TSF.
 * @param channel MIDI channel (1-16).
 * @param cc Controller number (0-127).
 * @param value Controller value (0-127).
 */
void shared_tsf_send_cc(int channel, int cc, int value);

/**
 * @brief Send all notes off to TSF.
 */
void shared_tsf_all_notes_off(void);

/* ============================================================================
 * Csound Backend
 * ============================================================================ */

/**
 * @brief Initialize the Csound backend.
 * @return 0 on success, -1 on error.
 */
int shared_csound_init(void);

/**
 * @brief Cleanup Csound resources.
 */
void shared_csound_cleanup(void);

/**
 * @brief Load a Csound orchestra file (.csd or .orc).
 * @param path Path to the Csound file.
 * @return 0 on success, -1 on error.
 */
int shared_csound_load(const char* path);

/**
 * @brief Enable the Csound synth.
 * @return 0 on success, -1 on error.
 */
int shared_csound_enable(void);

/**
 * @brief Disable the Csound synth.
 */
void shared_csound_disable(void);

/**
 * @brief Check if Csound is enabled.
 * @return Non-zero if enabled, 0 if disabled.
 */
int shared_csound_is_enabled(void);

/**
 * @brief Send a note-on message to Csound.
 * @param channel MIDI channel (1-16).
 * @param pitch Note pitch (0-127).
 * @param velocity Note velocity (0-127).
 */
void shared_csound_send_note_on(int channel, int pitch, int velocity);

/**
 * @brief Send a note-on with frequency to Csound (for microtuning).
 * @param channel MIDI channel (1-16).
 * @param freq Frequency in Hz.
 * @param velocity Note velocity (0-127).
 * @param midi_pitch Original MIDI pitch.
 */
void shared_csound_send_note_on_freq(int channel, double freq, int velocity, int midi_pitch);

/**
 * @brief Send a note-off message to Csound.
 * @param channel MIDI channel (1-16).
 * @param pitch Note pitch (0-127).
 */
void shared_csound_send_note_off(int channel, int pitch);

/**
 * @brief Send a program change to Csound.
 * @param channel MIDI channel (1-16).
 * @param program Program number (0-127).
 */
void shared_csound_send_program(int channel, int program);

/**
 * @brief Send a control change to Csound.
 * @param channel MIDI channel (1-16).
 * @param cc Controller number (0-127).
 * @param value Controller value (0-127).
 */
void shared_csound_send_cc(int channel, int cc, int value);

/**
 * @brief Send all notes off to Csound.
 */
void shared_csound_all_notes_off(void);

#ifdef __cplusplus
}
#endif

#endif /* SHARED_AUDIO_H */
