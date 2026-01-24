/**
 * tracker_model.h - Core data structures for the psnd tracker sequencer
 *
 * This defines the tracker's data model, independent of any view or playback engine.
 * Key concepts:
 *   - Song contains Patterns contains Tracks contains Cells
 *   - Cell holds an expression (source) that compiles to a Phrase
 *   - Phrase is a sequence of timed MIDI events (the universal unit)
 *   - Timing is in rows/substeps (compiled to absolute ticks at playback)
 *   - Tracks can have FX chains for post-processing all events on that track
 */

#ifndef TRACKER_MODEL_H
#define TRACKER_MODEL_H

#include <stdint.h>
#include <stdbool.h>

/*============================================================================
 * Constants
 *============================================================================*/

#define TRACKER_MAX_TRACKS          64
#define TRACKER_MAX_PATTERNS        256
#define TRACKER_MAX_PHRASE_EVENTS   1024
#define TRACKER_MAX_FX_CHAIN        16
#define TRACKER_DEFAULT_ROWS        64
#define TRACKER_DEFAULT_BPM         120
#define TRACKER_DEFAULT_RPB         4      /* rows per beat */
#define TRACKER_DEFAULT_TPR         6      /* ticks per row (substeps) */

/*============================================================================
 * Enumerations
 *============================================================================*/

typedef enum {
    TRACKER_EVENT_NOTE_ON,
    TRACKER_EVENT_NOTE_OFF,
    TRACKER_EVENT_CC,
    TRACKER_EVENT_PROGRAM_CHANGE,
    TRACKER_EVENT_PITCH_BEND,
    TRACKER_EVENT_AFTERTOUCH,
    TRACKER_EVENT_POLY_AFTERTOUCH,
} TrackerEventType;

typedef enum {
    TRACKER_SPILLOVER_LAYER,      /* phrases overlap polyphonically */
    TRACKER_SPILLOVER_TRUNCATE,   /* new phrase cuts previous */
    TRACKER_SPILLOVER_LOOP,       /* phrase loops until next trigger */
} TrackerSpilloverMode;

typedef enum {
    TRACKER_CELL_EMPTY,
    TRACKER_CELL_EXPRESSION,      /* contains evaluatable expression */
    TRACKER_CELL_NOTE_OFF,        /* explicit note-off marker */
    TRACKER_CELL_CONTINUATION,    /* visual: phrase continues from above */
} TrackerCellType;

/*============================================================================
 * Event Flags
 *============================================================================*/

typedef enum {
    TRACKER_FLAG_NONE          = 0,
    TRACKER_FLAG_PROBABILITY   = 1 << 0,   /* event has probability < 100% */
    TRACKER_FLAG_HUMANIZE_TIME = 1 << 1,   /* apply timing humanization */
    TRACKER_FLAG_HUMANIZE_VEL  = 1 << 2,   /* apply velocity humanization */
    TRACKER_FLAG_ACCENT        = 1 << 3,   /* velocity boost */
    TRACKER_FLAG_LEGATO        = 1 << 4,   /* don't retrigger, slide to pitch */
    TRACKER_FLAG_SLIDE         = 1 << 5,   /* portamento to this note */
    TRACKER_FLAG_RETRIGGER     = 1 << 6,   /* ratchet/retrigger marker */
    TRACKER_FLAG_MUTE          = 1 << 7,   /* event is muted (for preview/editing) */
} TrackerEventFlags;

/*============================================================================
 * Core Structures
 *============================================================================*/

/**
 * Extended event parameters (used when flags indicate special behavior).
 * Kept separate to avoid bloating the common case.
 */
typedef struct {
    uint8_t probability;          /* 0-100, used when FLAG_PROBABILITY set */
    uint8_t humanize_time_amt;    /* timing variation amount (0-127) */
    uint8_t humanize_vel_amt;     /* velocity variation amount (0-127) */
    uint8_t accent_boost;         /* velocity boost amount (0-127) */
    uint8_t retrigger_count;      /* number of retriggers */
    uint8_t retrigger_rate;       /* retrigger rate (ticks between) */
    uint8_t slide_time;           /* portamento time (0-127) */
    uint8_t reserved;             /* padding / future use */
} TrackerEventParams;

/**
 * A single MIDI event within a phrase.
 * Timing is relative to the cell's trigger point.
 */
typedef struct {
    int16_t offset_rows;          /* rows after trigger (can be 0) */
    int16_t offset_ticks;         /* substep ticks within row (0 to ticks_per_row-1) */
    TrackerEventType type;
    uint8_t channel;              /* MIDI channel 0-15 */
    uint8_t data1;                /* note number, CC number, program number */
    uint8_t data2;                /* velocity, CC value */
    int16_t gate_rows;            /* for NOTE_ON: duration in rows (0 = use explicit OFF) */
    int16_t gate_ticks;           /* sub-row gate precision */
    uint16_t flags;               /* TrackerEventFlags bitfield */
    TrackerEventParams* params;   /* extended params (owned), NULL if not needed */
} TrackerEvent;

/**
 * A phrase: the universal unit returned by expression evaluation.
 * Can represent atomic (1 event), sequence (N events), or generative output.
 */
typedef struct {
    TrackerEvent* events;
    int count;
    int capacity;
} TrackerPhrase;

/**
 * An FX entry in a chain.
 * Stored as source strings; compiled to function pointers by engine.
 */
typedef struct {
    char* name;                   /* FX name (owned), e.g., "transpose", "ratchet" */
    char* params;                 /* parameter string (owned), can be NULL */
    char* language_id;            /* plugin that provides this FX (owned), NULL = auto-detect */
    bool enabled;                 /* can be toggled without removing */
} TrackerFxEntry;

/**
 * An FX chain (used by both cells and tracks).
 */
typedef struct {
    TrackerFxEntry* entries;      /* array of FX entries (owned) */
    int count;
    int capacity;
} TrackerFxChain;

/**
 * Forward declaration for compiled cell data (defined in tracker_engine.h)
 */
typedef struct CompiledCell CompiledCell;

/**
 * Forward declaration for compiled FX chain (defined in tracker_engine.h)
 */
typedef struct CompiledFxChain CompiledFxChain;

/**
 * A cell in the tracker grid.
 * Stores source expression and cached compiled form.
 */
typedef struct {
    TrackerCellType type;
    char* expression;             /* source expression (owned) */
    char* language_id;            /* plugin identifier (owned), NULL = default */
    TrackerFxChain fx_chain;      /* per-cell FX chain */
    CompiledCell* compiled;       /* cached compiled form (owned), NULL = needs compile */
    bool dirty;                   /* true if expression changed since last compile */
} TrackerCell;

/**
 * A track (column) in a pattern.
 */
typedef struct {
    char* name;                   /* track name (owned), can be NULL */
    uint8_t default_channel;      /* MIDI channel for this track (0-15) */
    uint8_t volume;               /* track volume 0-127 (default 100) */
    int8_t pan;                   /* track pan -64 to +63 (0 = center) */
    bool muted;
    bool solo;
    TrackerFxChain fx_chain;      /* per-track FX chain (post-processes all cells) */
    CompiledFxChain* compiled_fx; /* cached compiled FX chain (owned) */
    TrackerCell* cells;           /* array of cells, length = pattern's num_rows */
} TrackerTrack;

/**
 * A pattern: a grid of tracks x rows.
 */
typedef struct {
    char* name;                   /* pattern name (owned), can be NULL */
    int num_rows;
    int num_tracks;
    TrackerTrack* tracks;         /* array of tracks (owned) */
} TrackerPattern;

/**
 * Pattern sequence entry for song arrangement.
 */
typedef struct {
    int pattern_index;            /* index into song's patterns array */
    int repeat_count;             /* number of times to play (1 = once) */
} TrackerSequenceEntry;

/**
 * A named phrase entry in the phrase library.
 * Allows reusable note patterns referenced by @name in cells.
 */
typedef struct {
    char* name;                   /* phrase name without @ prefix (owned) */
    char* expression;             /* source expression e.g. "C4 E4 G4" (owned) */
    char* language_id;            /* plugin that evaluates this (owned), NULL = default */
} TrackerPhraseEntry;

/**
 * Phrase library - collection of named reusable phrases.
 */
typedef struct {
    TrackerPhraseEntry* entries;  /* array of phrase entries (owned) */
    int count;
    int capacity;
} TrackerPhraseLibrary;

/**
 * Top-level song structure.
 */
typedef struct {
    char* name;                   /* song name (owned) */
    char* author;                 /* author (owned), can be NULL */

    /* Timing */
    int bpm;                      /* beats per minute */
    int rows_per_beat;            /* rows per beat (typically 4) */
    int ticks_per_row;            /* substep resolution (typically 6) */

    /* Patterns */
    TrackerPattern** patterns;    /* array of pattern pointers (owned) */
    int num_patterns;
    int patterns_capacity;

    /* Arrangement */
    TrackerSequenceEntry* sequence;  /* pattern play order (owned) */
    int sequence_length;
    int sequence_capacity;

    /* Global settings */
    TrackerSpilloverMode spillover_mode;
    char* default_language_id;    /* default plugin for cells without explicit language */
    TrackerFxChain master_fx;     /* master FX chain (post-processes all tracks) */
    CompiledFxChain* compiled_master_fx;

    /* Phrase library */
    TrackerPhraseLibrary phrase_library;  /* named reusable phrases */
} TrackerSong;

/*============================================================================
 * Lifecycle Functions
 *============================================================================*/

/* Event Params */
TrackerEventParams* tracker_event_params_new(void);
void tracker_event_params_free(TrackerEventParams* params);
TrackerEventParams* tracker_event_params_clone(const TrackerEventParams* params);

/* Phrase */
TrackerPhrase* tracker_phrase_new(int initial_capacity);
void tracker_phrase_free(TrackerPhrase* phrase);
void tracker_phrase_clear(TrackerPhrase* phrase);
bool tracker_phrase_add_event(TrackerPhrase* phrase, const TrackerEvent* event);
TrackerPhrase* tracker_phrase_clone(const TrackerPhrase* phrase);

/* FX Chain */
void tracker_fx_chain_init(TrackerFxChain* chain);
void tracker_fx_chain_clear(TrackerFxChain* chain);
bool tracker_fx_chain_append(TrackerFxChain* chain, const char* name,
                             const char* params, const char* lang_id);
bool tracker_fx_chain_insert(TrackerFxChain* chain, int index, const char* name,
                             const char* params, const char* lang_id);
bool tracker_fx_chain_remove(TrackerFxChain* chain, int index);
bool tracker_fx_chain_move(TrackerFxChain* chain, int from_index, int to_index);
void tracker_fx_chain_set_enabled(TrackerFxChain* chain, int index, bool enabled);
TrackerFxEntry* tracker_fx_chain_get(TrackerFxChain* chain, int index);
bool tracker_fx_chain_clone(TrackerFxChain* dest, const TrackerFxChain* src);

/* Cell */
void tracker_cell_init(TrackerCell* cell);
void tracker_cell_clear(TrackerCell* cell);
void tracker_cell_set_expression(TrackerCell* cell, const char* expr, const char* lang_id);
void tracker_cell_mark_dirty(TrackerCell* cell);
bool tracker_cell_clone(TrackerCell* dest, const TrackerCell* src);

/* Track */
TrackerTrack* tracker_track_new(int num_rows, const char* name, uint8_t channel);
void tracker_track_free(TrackerTrack* track, int num_rows);
void tracker_track_resize(TrackerTrack* track, int old_rows, int new_rows);

/* Pattern */
TrackerPattern* tracker_pattern_new(int num_rows, int num_tracks, const char* name);
void tracker_pattern_free(TrackerPattern* pattern);
TrackerCell* tracker_pattern_get_cell(TrackerPattern* pattern, int row, int track);
bool tracker_pattern_add_track(TrackerPattern* pattern, const char* name, uint8_t channel);
bool tracker_pattern_remove_track(TrackerPattern* pattern, int track_index);
void tracker_pattern_set_rows(TrackerPattern* pattern, int new_num_rows);

/* Song */
TrackerSong* tracker_song_new(const char* name);
void tracker_song_free(TrackerSong* song);
int tracker_song_add_pattern(TrackerSong* song, TrackerPattern* pattern);
bool tracker_song_remove_pattern(TrackerSong* song, int pattern_index);
TrackerPattern* tracker_song_get_pattern(TrackerSong* song, int index);
bool tracker_song_append_to_sequence(TrackerSong* song, int pattern_index, int repeat_count);

/* Phrase Library */
void tracker_phrase_library_init(TrackerPhraseLibrary* lib);
void tracker_phrase_library_clear(TrackerPhraseLibrary* lib);
bool tracker_phrase_library_add(TrackerPhraseLibrary* lib, const char* name,
                                 const char* expression, const char* language_id);
bool tracker_phrase_library_remove(TrackerPhraseLibrary* lib, const char* name);
TrackerPhraseEntry* tracker_phrase_library_get(TrackerPhraseLibrary* lib, const char* name);
int tracker_phrase_library_find(TrackerPhraseLibrary* lib, const char* name);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * Calculate absolute tick from row + substep tick.
 */
static inline int64_t tracker_calc_absolute_tick(
    int row,
    int tick,
    int ticks_per_row
) {
    return (int64_t)row * ticks_per_row + tick;
}

/**
 * Calculate milliseconds from absolute tick.
 */
static inline double tracker_tick_to_ms(
    int64_t tick,
    int bpm,
    int rows_per_beat,
    int ticks_per_row
) {
    double ticks_per_beat = (double)rows_per_beat * ticks_per_row;
    double ms_per_beat = 60000.0 / bpm;
    return (tick / ticks_per_beat) * ms_per_beat;
}

/**
 * Check if a cell has content (not empty or continuation).
 */
static inline bool tracker_cell_has_content(const TrackerCell* cell) {
    return cell->type == TRACKER_CELL_EXPRESSION ||
           cell->type == TRACKER_CELL_NOTE_OFF;
}

/**
 * Check if event has any flags set.
 */
static inline bool tracker_event_has_flags(const TrackerEvent* event) {
    return event->flags != TRACKER_FLAG_NONE;
}

/**
 * Check if event needs extended params allocated.
 */
static inline bool tracker_event_needs_params(uint16_t flags) {
    return flags & (TRACKER_FLAG_PROBABILITY |
                    TRACKER_FLAG_HUMANIZE_TIME |
                    TRACKER_FLAG_HUMANIZE_VEL |
                    TRACKER_FLAG_ACCENT |
                    TRACKER_FLAG_RETRIGGER |
                    TRACKER_FLAG_SLIDE);
}

#endif /* TRACKER_MODEL_H */
