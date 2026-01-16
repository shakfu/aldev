/**
 * @file events.h
 * @brief Shared MIDI event buffer for export.
 *
 * Provides a common event format that all languages can use to record
 * MIDI events for export. Languages populate the buffer during playback
 * or evaluation, and the export system reads from it.
 *
 * Usage:
 *   1. Call shared_midi_events_init() before recording
 *   2. Languages call shared_midi_events_add() during playback
 *   3. Export reads events via shared_midi_events_get()
 *   4. Call shared_midi_events_clear() to reset for new recording
 */

#ifndef SHARED_MIDI_EVENTS_H
#define SHARED_MIDI_EVENTS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Event Types
 * ============================================================================ */

typedef enum {
    SHARED_MIDI_NOTE_ON,
    SHARED_MIDI_NOTE_OFF,
    SHARED_MIDI_PROGRAM,
    SHARED_MIDI_CC,
    SHARED_MIDI_TEMPO
} SharedMidiEventType;

/* ============================================================================
 * Event Structure
 * ============================================================================ */

typedef struct {
    int tick;                   /* Absolute tick position */
    SharedMidiEventType type;   /* Event type */
    int channel;                /* MIDI channel (0-15) */
    int data1;                  /* Pitch, CC number, program, or tempo BPM */
    int data2;                  /* Velocity or CC value (0 for others) */
} SharedMidiEvent;

/* ============================================================================
 * Buffer Management
 * ============================================================================ */

/**
 * @brief Initialize the shared event buffer.
 * @param ticks_per_quarter Ticks per quarter note (e.g., 480).
 * @return 0 on success, -1 on error.
 */
int shared_midi_events_init(int ticks_per_quarter);

/**
 * @brief Cleanup the shared event buffer.
 */
void shared_midi_events_cleanup(void);

/**
 * @brief Clear all events from the buffer.
 * Keeps the buffer allocated for reuse.
 */
void shared_midi_events_clear(void);

/**
 * @brief Check if the event buffer is initialized.
 * @return Non-zero if initialized, 0 otherwise.
 */
int shared_midi_events_is_initialized(void);

/* ============================================================================
 * Event Recording
 * ============================================================================ */

/**
 * @brief Add an event to the buffer.
 * @param event Event to add (copied into buffer).
 * @return 0 on success, -1 on error (buffer full or not initialized).
 */
int shared_midi_events_add(const SharedMidiEvent *event);

/**
 * @brief Add a note-on event.
 * @param tick Absolute tick position.
 * @param channel MIDI channel (0-15).
 * @param pitch Note pitch (0-127).
 * @param velocity Note velocity (0-127).
 * @return 0 on success, -1 on error.
 */
int shared_midi_events_note_on(int tick, int channel, int pitch, int velocity);

/**
 * @brief Add a note-off event.
 * @param tick Absolute tick position.
 * @param channel MIDI channel (0-15).
 * @param pitch Note pitch (0-127).
 * @return 0 on success, -1 on error.
 */
int shared_midi_events_note_off(int tick, int channel, int pitch);

/**
 * @brief Add a program change event.
 * @param tick Absolute tick position.
 * @param channel MIDI channel (0-15).
 * @param program Program number (0-127).
 * @return 0 on success, -1 on error.
 */
int shared_midi_events_program(int tick, int channel, int program);

/**
 * @brief Add a control change event.
 * @param tick Absolute tick position.
 * @param channel MIDI channel (0-15).
 * @param cc Controller number (0-127).
 * @param value Controller value (0-127).
 * @return 0 on success, -1 on error.
 */
int shared_midi_events_cc(int tick, int channel, int cc, int value);

/**
 * @brief Add a tempo change event.
 * @param tick Absolute tick position.
 * @param bpm Tempo in beats per minute.
 * @return 0 on success, -1 on error.
 */
int shared_midi_events_tempo(int tick, int bpm);

/* ============================================================================
 * Event Access
 * ============================================================================ */

/**
 * @brief Get pointer to event array.
 * @param count Output: number of events in buffer.
 * @return Pointer to event array, or NULL if not initialized.
 */
const SharedMidiEvent* shared_midi_events_get(int *count);

/**
 * @brief Get the number of events in the buffer.
 * @return Event count, or 0 if not initialized.
 */
int shared_midi_events_count(void);

/**
 * @brief Get ticks per quarter note setting.
 * @return Ticks per quarter, or 0 if not initialized.
 */
int shared_midi_events_ticks_per_quarter(void);

/**
 * @brief Sort events by tick (stable sort preserving order for same tick).
 */
void shared_midi_events_sort(void);

#ifdef __cplusplus
}
#endif

#endif /* SHARED_MIDI_EVENTS_H */
