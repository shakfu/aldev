/**
 * @file context.h
 * @brief Shared audio/MIDI/Link context for all music languages.
 *
 * This provides a language-agnostic interface for audio output that can be
 * shared between Alda, Joy, and future music DSLs. It routes events to the
 * appropriate backend (Csound > TSF > MIDI) based on what's enabled.
 */

#ifndef SHARED_CONTEXT_H
#define SHARED_CONTEXT_H

#include <libremidi/libremidi-c.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum number of MIDI output ports */
#define SHARED_MAX_PORTS 64

/**
 * @brief Shared context for audio/MIDI output.
 *
 * This struct holds per-context state for MIDI output and backend selection.
 * TSF and Csound backends are singletons (global audio devices), but this
 * context tracks whether they're enabled for this particular language context.
 */
typedef struct SharedContext {
    /* Backend enable flags */
    int tsf_enabled;        /* Use TinySoundFont if available */
    int csound_enabled;     /* Use Csound if available */
    int link_enabled;       /* Use Ableton Link for tempo */

    /* MIDI output (libremidi) */
    libremidi_midi_out_handle* midi_out;
    libremidi_midi_observer_handle* observer;

    /* Port enumeration */
    libremidi_midi_out_port* out_ports[SHARED_MAX_PORTS];
    int out_port_count;

    /* Musical state */
    int tempo;              /* Current tempo in BPM (Link can override) */
    int default_channel;    /* Default MIDI channel (1-16) */

    /* Optional: microtuning scale (void* to avoid circular deps) */
    void* scale;

    /* Test mode flag */
    int no_sleep_mode;      /* Skip sleeps for testing */
} SharedContext;

/* ============================================================================
 * Context Lifecycle
 * ============================================================================ */

/**
 * @brief Initialize a shared context.
 * @param ctx Context to initialize.
 * @return 0 on success, -1 on error.
 */
int shared_context_init(SharedContext* ctx);

/**
 * @brief Cleanup a shared context.
 * @param ctx Context to cleanup.
 */
void shared_context_cleanup(SharedContext* ctx);

/* ============================================================================
 * Event Dispatch (routes to active backend)
 * ============================================================================ */

/**
 * @brief Send a note-on event.
 * Routes to Csound, TSF, or MIDI based on what's enabled.
 *
 * @param ctx Shared context.
 * @param channel MIDI channel (1-16).
 * @param pitch Note pitch (0-127).
 * @param velocity Note velocity (0-127).
 */
void shared_send_note_on(SharedContext* ctx, int channel, int pitch, int velocity);

/**
 * @brief Send a note-on event with frequency (for microtuning).
 *
 * @param ctx Shared context.
 * @param channel MIDI channel (1-16).
 * @param freq Frequency in Hz.
 * @param velocity Note velocity (0-127).
 * @param midi_pitch Original MIDI pitch (for backends that need it).
 */
void shared_send_note_on_freq(SharedContext* ctx, int channel, double freq,
                              int velocity, int midi_pitch);

/**
 * @brief Send a note-off event.
 *
 * @param ctx Shared context.
 * @param channel MIDI channel (1-16).
 * @param pitch Note pitch (0-127).
 */
void shared_send_note_off(SharedContext* ctx, int channel, int pitch);

/**
 * @brief Send a program change event.
 *
 * @param ctx Shared context.
 * @param channel MIDI channel (1-16).
 * @param program Program number (0-127).
 */
void shared_send_program(SharedContext* ctx, int channel, int program);

/**
 * @brief Send a control change event.
 *
 * @param ctx Shared context.
 * @param channel MIDI channel (1-16).
 * @param cc Controller number (0-127).
 * @param value Controller value (0-127).
 */
void shared_send_cc(SharedContext* ctx, int channel, int cc, int value);

/**
 * @brief Send all notes off (panic).
 *
 * @param ctx Shared context.
 */
void shared_send_panic(SharedContext* ctx);

/* ============================================================================
 * Timing Utilities
 * ============================================================================ */

/**
 * @brief Convert ticks to milliseconds.
 *
 * @param ticks Number of ticks.
 * @param tempo Tempo in BPM.
 * @return Duration in milliseconds.
 */
int shared_ticks_to_ms(int ticks, int tempo);

/**
 * @brief Sleep for specified milliseconds.
 * Respects no_sleep_mode for testing.
 *
 * @param ctx Shared context.
 * @param ms Milliseconds to sleep.
 */
void shared_sleep_ms(SharedContext* ctx, int ms);

/**
 * @brief Get effective tempo (Link tempo if enabled, else context tempo).
 *
 * @param ctx Shared context.
 * @return Tempo in BPM.
 */
int shared_effective_tempo(SharedContext* ctx);

#ifdef __cplusplus
}
#endif

#endif /* SHARED_CONTEXT_H */
