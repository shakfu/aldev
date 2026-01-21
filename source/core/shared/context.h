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
#include "param/param.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum number of MIDI output ports */
#define SHARED_MAX_PORTS 64

/* Forward declarations for OSC types (only defined when PSND_OSC is set) */
#ifdef PSND_OSC
typedef struct lo_server_thread_ *lo_server_thread;
typedef struct lo_address_ *lo_address;
#else
typedef void *lo_server_thread;
typedef void *lo_address;
#endif

/**
 * @brief Shared context for audio/MIDI output.
 *
 * This struct holds per-context state for MIDI output and backend selection.
 * TSF and Csound backends are singletons (global audio devices), but this
 * context tracks whether they're enabled for this particular language context.
 */
typedef struct SharedContext {
    /* Backend enable flags */
    int builtin_synth_enabled;  /* Use built-in synth (TSF or FluidSynth) */
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
    int launch_quantize;    /* Beat quantization (0=immediate, 1=beat, 4=bar) */

    /* Optional: microtuning scale (void* to avoid circular deps) */
    void* scale;

    /* OSC state (Open Sound Control) */
    int osc_enabled;            /* OSC server is running */
    int osc_port;               /* OSC listening port */
    lo_server_thread osc_server;    /* liblo server thread */
    lo_address osc_broadcast;   /* Target for outgoing OSC messages */
    void* osc_user_data;        /* User data for OSC handlers */

    /* Parameter system (named params bound to OSC/MIDI CC) */
    SharedParamStore params;

    /* MIDI input (for CC -> param binding) */
    libremidi_midi_in_handle* midi_in;
    libremidi_midi_in_port* in_ports[SHARED_MAX_PORTS];
    int in_port_count;

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
