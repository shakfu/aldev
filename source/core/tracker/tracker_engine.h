/**
 * tracker_engine.h - Playback engine for the tracker sequencer
 *
 * The engine is responsible for:
 *   - Owning the master clock
 *   - Advancing time and triggering cells
 *   - Evaluating cells and applying FX chains
 *   - Maintaining the event queue (scheduled events)
 *   - Tracking active notes (for note-off and All Notes Off)
 *   - Dispatching MIDI output via callbacks
 *
 * The engine is decoupled from actual MIDI I/O - it uses an output interface
 * that can target hardware MIDI, the shared backend, file recording, etc.
 */

#ifndef TRACKER_ENGINE_H
#define TRACKER_ENGINE_H

#include "tracker_model.h"
#include "tracker_plugin.h"
#include <stdint.h>
#include <stdbool.h>

/*============================================================================
 * Forward Declarations
 *============================================================================*/

typedef struct TrackerEngine TrackerEngine;
typedef struct TrackerEngineConfig TrackerEngineConfig;
typedef struct TrackerOutput TrackerOutput;
typedef struct TrackerPendingEvent TrackerPendingEvent;
typedef struct TrackerActiveNote TrackerActiveNote;

/*============================================================================
 * Constants
 *============================================================================*/

#define TRACKER_ENGINE_MAX_ACTIVE_NOTES   256
#define TRACKER_ENGINE_MAX_PENDING_EVENTS 4096
#define TRACKER_ENGINE_RECENT_ROWS        8     /* rows of history for reactive */

/*============================================================================
 * Engine State
 *============================================================================*/

typedef enum {
    TRACKER_ENGINE_STOPPED,
    TRACKER_ENGINE_PLAYING,
    TRACKER_ENGINE_PAUSED,
    TRACKER_ENGINE_RECORDING,     /* future: record input */
} TrackerEngineState;

typedef enum {
    TRACKER_PLAY_MODE_PATTERN,    /* loop single pattern */
    TRACKER_PLAY_MODE_SONG,       /* play through sequence */
} TrackerPlayMode;

typedef enum {
    TRACKER_SYNC_INTERNAL,        /* engine owns clock */
    TRACKER_SYNC_EXTERNAL_MIDI,   /* sync to MIDI clock */
    TRACKER_SYNC_EXTERNAL_LINK,   /* sync to Ableton Link */
} TrackerSyncMode;

/*============================================================================
 * Output Interface
 *============================================================================*/

/**
 * MIDI output callback signatures.
 * The engine calls these to send MIDI data.
 */
typedef void (*TrackerOutputNoteOnFn)(
    void* user_data,
    uint8_t channel,
    uint8_t note,
    uint8_t velocity
);

typedef void (*TrackerOutputNoteOffFn)(
    void* user_data,
    uint8_t channel,
    uint8_t note,
    uint8_t velocity   /* release velocity, often 0 */
);

typedef void (*TrackerOutputCCFn)(
    void* user_data,
    uint8_t channel,
    uint8_t cc_number,
    uint8_t value
);

typedef void (*TrackerOutputProgramChangeFn)(
    void* user_data,
    uint8_t channel,
    uint8_t program
);

typedef void (*TrackerOutputPitchBendFn)(
    void* user_data,
    uint8_t channel,
    int16_t value      /* -8192 to 8191 */
);

typedef void (*TrackerOutputAftertouchFn)(
    void* user_data,
    uint8_t channel,
    uint8_t pressure
);

typedef void (*TrackerOutputPolyAftertouchFn)(
    void* user_data,
    uint8_t channel,
    uint8_t note,
    uint8_t pressure
);

typedef void (*TrackerOutputAllNotesOffFn)(
    void* user_data,
    uint8_t channel    /* 255 = all channels */
);

typedef void (*TrackerOutputClockFn)(
    void* user_data    /* MIDI clock tick (24 PPQ) */
);

typedef void (*TrackerOutputStartFn)(void* user_data);
typedef void (*TrackerOutputStopFn)(void* user_data);
typedef void (*TrackerOutputContinueFn)(void* user_data);

/**
 * Output interface - collection of callbacks for MIDI output.
 * Set callbacks to NULL if not needed.
 */
struct TrackerOutput {
    void* user_data;              /* passed to all callbacks */

    /* Note events */
    TrackerOutputNoteOnFn note_on;
    TrackerOutputNoteOffFn note_off;

    /* Control events */
    TrackerOutputCCFn cc;
    TrackerOutputProgramChangeFn program_change;
    TrackerOutputPitchBendFn pitch_bend;
    TrackerOutputAftertouchFn aftertouch;
    TrackerOutputPolyAftertouchFn poly_aftertouch;

    /* Panic */
    TrackerOutputAllNotesOffFn all_notes_off;

    /* MIDI clock (for external sync) */
    TrackerOutputClockFn clock;
    TrackerOutputStartFn start;
    TrackerOutputStopFn stop;
    TrackerOutputContinueFn cont;   /* continue */
};

/*============================================================================
 * Event Queue Structures
 *============================================================================*/

/**
 * Source info for a pending event (for spillover tracking).
 */
typedef struct {
    int pattern_index;
    int track_index;
    int row_index;
    int phrase_id;                /* unique ID for the phrase instance */
} TrackerEventSource;

/**
 * A pending event in the queue.
 */
struct TrackerPendingEvent {
    int64_t due_tick;             /* absolute tick when this fires */
    TrackerEvent event;           /* the event data */
    TrackerEventSource source;    /* where it came from */
    TrackerPendingEvent* next;    /* linked list for free list / queue */
};

/**
 * An active (sounding) note - tracked for note-off and panic.
 */
struct TrackerActiveNote {
    uint8_t channel;
    uint8_t note;
    int track_index;              /* which track started this note */
    int phrase_id;                /* which phrase instance */
    int64_t started_tick;         /* when note-on was sent */
    int64_t scheduled_off_tick;   /* when note-off is scheduled (-1 if explicit) */
    bool active;                  /* slot in use */
};

/*============================================================================
 * Engine Configuration
 *============================================================================*/

struct TrackerEngineConfig {
    /* Output */
    TrackerOutput output;

    /* Sync */
    TrackerSyncMode sync_mode;

    /* Playback */
    TrackerPlayMode default_play_mode;
    bool send_midi_clock;         /* emit MIDI clock messages */
    bool auto_recompile;          /* recompile dirty cells on the fly */

    /* Behavior */
    bool chase_notes;             /* when seeking, send note-ons for active notes */
    bool send_all_notes_off_on_stop;
    int lookahead_ms;             /* scheduling lookahead (default 10ms) */

    /* Limits */
    int max_pending_events;       /* 0 = use default */
    int max_active_notes;         /* 0 = use default */
    int recent_events_rows;       /* rows of history for reactive (0 = disable) */
};

/*============================================================================
 * Engine Structure
 *============================================================================*/

struct TrackerEngine {
    /* Configuration */
    TrackerEngineConfig config;

    /* Current song */
    TrackerSong* song;            /* not owned */

    /* State */
    TrackerEngineState state;
    TrackerPlayMode play_mode;

    /* Position */
    int current_pattern;          /* pattern index (in song mode: sequence position) */
    int current_row;
    int64_t current_tick;         /* absolute tick since start */
    double current_time_ms;       /* absolute time in ms */
    int loop_count;               /* times pattern/song has looped */

    /* Timing */
    int bpm;                      /* can differ from song if overridden */
    int rows_per_beat;
    int ticks_per_row;
    double tick_duration_ms;      /* cached: ms per tick */
    double row_duration_ms;       /* cached: ms per row */

    /* Loop points */
    bool loop_enabled;
    int loop_start_row;           /* -1 = pattern start */
    int loop_end_row;             /* -1 = pattern end */

    /* Swing/groove */
    int swing_amount;             /* 0-100: swing percentage (50 = straight) */

    /* Event queue (priority queue as linked list, sorted by due_tick) */
    TrackerPendingEvent* pending_head;
    TrackerPendingEvent* free_list;
    TrackerPendingEvent* event_pool;
    int pending_count;
    int next_phrase_id;           /* counter for unique phrase IDs */

    /* Active notes */
    TrackerActiveNote* active_notes;
    int active_note_count;

    /* Recent events (for reactive composition) */
    TrackerPhrase** recent_by_track;  /* array[num_tracks] of recent phrases */
    int recent_events_rows;

    /* Compiled cache */
    CompiledCell*** compiled_patterns;  /* [pattern][track][row] */
    CompiledFxChain** compiled_track_fx; /* [track] per current pattern */
    CompiledFxChain* compiled_master_fx;
    bool* pattern_compiled;        /* which patterns are compiled */

    /* Context template (reused, filled in per-evaluation) */
    TrackerContext ctx_template;

    /* Error state */
    char* last_error;
    int error_pattern;
    int error_track;
    int error_row;

    /* Statistics */
    uint64_t events_fired;
    uint64_t events_scheduled;
    uint64_t notes_on;
    uint64_t notes_off;
    uint64_t underruns;           /* times we fell behind */
};

/*============================================================================
 * Lifecycle Functions
 *============================================================================*/

/**
 * Create a new engine with default configuration.
 */
TrackerEngine* tracker_engine_new(void);

/**
 * Create a new engine with custom configuration.
 */
TrackerEngine* tracker_engine_new_with_config(const TrackerEngineConfig* config);

/**
 * Destroy an engine and free all resources.
 */
void tracker_engine_free(TrackerEngine* engine);

/**
 * Reset engine to initial state (stop, clear queues, reset position).
 */
void tracker_engine_reset(TrackerEngine* engine);

/**
 * Initialize default configuration.
 */
void tracker_engine_config_init(TrackerEngineConfig* config);

/*============================================================================
 * Song Management
 *============================================================================*/

/**
 * Load a song into the engine.
 * Engine does not take ownership of the song.
 * Compiles all patterns if auto_recompile is enabled.
 *
 * @param engine  The engine
 * @param song    Song to load (engine does not own)
 * @return        true on success, false on compilation error
 */
bool tracker_engine_load_song(TrackerEngine* engine, TrackerSong* song);

/**
 * Unload the current song.
 * Stops playback and clears all state.
 */
void tracker_engine_unload_song(TrackerEngine* engine);

/**
 * Recompile a specific pattern.
 */
bool tracker_engine_compile_pattern(TrackerEngine* engine, int pattern_index);

/**
 * Recompile all patterns.
 */
bool tracker_engine_compile_all(TrackerEngine* engine);

/**
 * Mark a cell as dirty (needs recompile).
 */
void tracker_engine_mark_dirty(TrackerEngine* engine, int pattern, int track, int row);

/*============================================================================
 * Transport Controls
 *============================================================================*/

/**
 * Start playback from current position.
 */
bool tracker_engine_play(TrackerEngine* engine);

/**
 * Stop playback.
 * Sends note-offs for all active notes if configured.
 */
void tracker_engine_stop(TrackerEngine* engine);

/**
 * Pause playback (can resume with play).
 */
void tracker_engine_pause(TrackerEngine* engine);

/**
 * Toggle play/pause.
 */
void tracker_engine_toggle(TrackerEngine* engine);

/**
 * Seek to a specific position.
 * Clears pending events and optionally chases notes.
 *
 * @param pattern  Pattern index (or sequence position in song mode)
 * @param row      Row within pattern
 */
void tracker_engine_seek(TrackerEngine* engine, int pattern, int row);

/**
 * Jump to next pattern in sequence.
 */
void tracker_engine_next_pattern(TrackerEngine* engine);

/**
 * Jump to previous pattern in sequence.
 */
void tracker_engine_prev_pattern(TrackerEngine* engine);

/*============================================================================
 * Timing and Advance
 *============================================================================*/

/**
 * Process a time delta.
 * Call this regularly (e.g., from audio callback or timer).
 * Fires all events due within the time window.
 *
 * @param engine    The engine
 * @param delta_ms  Time elapsed since last call (milliseconds)
 * @return          Number of events fired
 */
int tracker_engine_process(TrackerEngine* engine, double delta_ms);

/**
 * Process until a specific absolute time.
 * Alternative to delta-based processing.
 *
 * @param engine       The engine
 * @param target_ms    Target absolute time (ms since start)
 * @return             Number of events fired
 */
int tracker_engine_process_until(TrackerEngine* engine, double target_ms);

/**
 * Advance by one row (for step-based editing/preview).
 * Does not respect real-time - immediately fires all events for that row.
 */
void tracker_engine_step_row(TrackerEngine* engine);

/**
 * Advance by one tick.
 */
void tracker_engine_step_tick(TrackerEngine* engine);

/**
 * Trigger a specific cell immediately (for preview/audition).
 * Does not advance position.
 *
 * @param pattern  Pattern index
 * @param track    Track index
 * @param row      Row index
 */
void tracker_engine_trigger_cell(TrackerEngine* engine, int pattern, int track, int row);

/**
 * Evaluate and play an expression immediately (for REPL/preview).
 *
 * @param expression   Expression to evaluate
 * @param language_id  Language (NULL = default)
 * @param channel      MIDI channel
 * @return             true on success
 */
bool tracker_engine_eval_immediate(
    TrackerEngine* engine,
    const char* expression,
    const char* language_id,
    uint8_t channel
);

/*============================================================================
 * Playback Settings
 *============================================================================*/

/**
 * Set playback mode (pattern vs song).
 */
void tracker_engine_set_play_mode(TrackerEngine* engine, TrackerPlayMode mode);

/**
 * Set tempo (overrides song BPM).
 */
void tracker_engine_set_bpm(TrackerEngine* engine, int bpm);

/**
 * Reset tempo to song's BPM.
 */
void tracker_engine_reset_bpm(TrackerEngine* engine);

/**
 * Enable/disable looping.
 */
void tracker_engine_set_loop(TrackerEngine* engine, bool enabled);

/**
 * Set loop points (within current pattern).
 * Pass -1 for start/end to use pattern boundaries.
 */
void tracker_engine_set_loop_points(TrackerEngine* engine, int start_row, int end_row);

/**
 * Set sync mode.
 */
void tracker_engine_set_sync_mode(TrackerEngine* engine, TrackerSyncMode mode);

/*============================================================================
 * Track Control
 *============================================================================*/

/**
 * Mute a track.
 */
void tracker_engine_mute_track(TrackerEngine* engine, int track, bool muted);

/**
 * Solo a track.
 */
void tracker_engine_solo_track(TrackerEngine* engine, int track, bool solo);

/**
 * Check if any track is soloed.
 */
bool tracker_engine_has_solo(TrackerEngine* engine);

/**
 * Clear all solos.
 */
void tracker_engine_clear_solo(TrackerEngine* engine);

/*============================================================================
 * Event Queue Management
 *============================================================================*/

/**
 * Schedule an event for future playback.
 *
 * @param engine      The engine
 * @param due_tick    Absolute tick when event should fire
 * @param event       Event to schedule
 * @param source      Source info (for spillover tracking)
 * @return            true on success, false if queue full
 */
bool tracker_engine_schedule_event(
    TrackerEngine* engine,
    int64_t due_tick,
    const TrackerEvent* event,
    const TrackerEventSource* source
);

/**
 * Cancel all pending events from a specific phrase.
 * Used for spillover truncate mode.
 */
void tracker_engine_cancel_phrase(TrackerEngine* engine, int phrase_id);

/**
 * Cancel all pending events on a track.
 */
void tracker_engine_cancel_track(TrackerEngine* engine, int track_index);

/**
 * Cancel all pending events.
 */
void tracker_engine_cancel_all(TrackerEngine* engine);

/**
 * Get count of pending events.
 */
int tracker_engine_pending_count(TrackerEngine* engine);

/*============================================================================
 * Active Note Management
 *============================================================================*/

/**
 * Send note-off for all active notes (panic).
 */
void tracker_engine_all_notes_off(TrackerEngine* engine);

/**
 * Send note-off for all active notes on a channel.
 */
void tracker_engine_channel_notes_off(TrackerEngine* engine, uint8_t channel);

/**
 * Send note-off for all active notes on a track.
 */
void tracker_engine_track_notes_off(TrackerEngine* engine, int track_index);

/**
 * Get count of active notes.
 */
int tracker_engine_active_note_count(TrackerEngine* engine);

/*============================================================================
 * Query Functions
 *============================================================================*/

/**
 * Get current playback position.
 */
void tracker_engine_get_position(
    TrackerEngine* engine,
    int* out_pattern,
    int* out_row,
    int* out_tick
);

/**
 * Get current playback time in milliseconds.
 */
double tracker_engine_get_time_ms(TrackerEngine* engine);

/**
 * Get current effective BPM (may differ from song BPM).
 */
int tracker_engine_get_bpm(TrackerEngine* engine);

/**
 * Check if engine is playing.
 */
static inline bool tracker_engine_is_playing(TrackerEngine* engine) {
    return engine->state == TRACKER_ENGINE_PLAYING;
}

/**
 * Check if engine is paused.
 */
static inline bool tracker_engine_is_paused(TrackerEngine* engine) {
    return engine->state == TRACKER_ENGINE_PAUSED;
}

/**
 * Check if engine is stopped.
 */
static inline bool tracker_engine_is_stopped(TrackerEngine* engine) {
    return engine->state == TRACKER_ENGINE_STOPPED;
}

/**
 * Get last error message.
 */
const char* tracker_engine_get_error(TrackerEngine* engine);

/**
 * Get error location.
 */
void tracker_engine_get_error_location(
    TrackerEngine* engine,
    int* out_pattern,
    int* out_track,
    int* out_row
);

/**
 * Clear error state.
 */
void tracker_engine_clear_error(TrackerEngine* engine);

/*============================================================================
 * Output Configuration
 *============================================================================*/

/**
 * Set output interface.
 */
void tracker_engine_set_output(TrackerEngine* engine, const TrackerOutput* output);

/**
 * Get current output interface.
 */
const TrackerOutput* tracker_engine_get_output(TrackerEngine* engine);

/*============================================================================
 * Statistics
 *============================================================================*/

typedef struct {
    uint64_t events_fired;
    uint64_t events_scheduled;
    uint64_t notes_on;
    uint64_t notes_off;
    uint64_t underruns;
    int pending_events;
    int active_notes;
    double cpu_usage;             /* estimated CPU usage (0.0 - 1.0) */
} TrackerEngineStats;

/**
 * Get engine statistics.
 */
void tracker_engine_get_stats(TrackerEngine* engine, TrackerEngineStats* stats);

/**
 * Reset statistics counters.
 */
void tracker_engine_reset_stats(TrackerEngine* engine);

/*============================================================================
 * External Sync (for TRACKER_SYNC_EXTERNAL_*)
 *============================================================================*/

/**
 * Receive external clock tick.
 * Call this when receiving MIDI clock.
 */
void tracker_engine_external_clock(TrackerEngine* engine);

/**
 * Receive external start command.
 */
void tracker_engine_external_start(TrackerEngine* engine);

/**
 * Receive external stop command.
 */
void tracker_engine_external_stop(TrackerEngine* engine);

/**
 * Receive external continue command.
 */
void tracker_engine_external_continue(TrackerEngine* engine);

/**
 * Receive external position (Song Position Pointer).
 * Position is in MIDI beats (6 MIDI clocks = 1 beat).
 */
void tracker_engine_external_position(TrackerEngine* engine, int position);

/**
 * Update from Ableton Link state.
 * Call this regularly when sync_mode is TRACKER_SYNC_EXTERNAL_LINK.
 *
 * @param beat       Current beat position
 * @param bpm        Current tempo
 * @param is_playing Whether Link session is playing
 */
void tracker_engine_link_update(
    TrackerEngine* engine,
    double beat,
    double bpm,
    bool is_playing
);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * Convert row position to absolute tick.
 */
static inline int64_t tracker_engine_row_to_tick(
    TrackerEngine* engine,
    int pattern,
    int row
) {
    /* Simplified: assumes all patterns have same timing */
    (void)pattern;  /* reserved for future per-pattern timing */
    return (int64_t)row * engine->ticks_per_row;
}

/**
 * Convert absolute tick to row position.
 */
static inline int tracker_engine_tick_to_row(TrackerEngine* engine, int64_t tick) {
    return (int)(tick / engine->ticks_per_row);
}

/**
 * Convert milliseconds to ticks.
 */
static inline int64_t tracker_engine_ms_to_ticks(TrackerEngine* engine, double ms) {
    return (int64_t)(ms / engine->tick_duration_ms);
}

/**
 * Convert ticks to milliseconds.
 */
static inline double tracker_engine_ticks_to_ms(TrackerEngine* engine, int64_t ticks) {
    return ticks * engine->tick_duration_ms;
}

#endif /* TRACKER_ENGINE_H */
