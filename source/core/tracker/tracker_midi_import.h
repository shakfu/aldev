/**
 * tracker_midi_import.h - Import Standard MIDI Files into tracker songs
 *
 * Converts MIDI files to TrackerSong format with automatic:
 * - Channel to track mapping
 * - Note quantization to rows
 * - Velocity preservation
 * - Tempo extraction
 */

#ifndef TRACKER_MIDI_IMPORT_H
#define TRACKER_MIDI_IMPORT_H

#include "tracker_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Import options for MIDI file conversion.
 */
typedef struct {
    int rows_per_beat;        /* Rows per beat for quantization (default: 4) */
    int ticks_per_row;        /* Ticks per row (default: 6) */
    int pattern_rows;         /* Max rows per pattern (default: 64) */
    int quantize_strength;    /* 0-100: 0=no quantize, 100=hard quantize (default: 100) */
    int velocity_threshold;   /* Minimum velocity to include (default: 1) */
    int include_velocity;     /* Include velocity in expressions (default: 1) */
    int split_by_channel;     /* Create separate tracks per channel (default: 1) */
    int max_tracks;           /* Maximum tracks to import (default: 16) */
} TrackerMidiImportOptions;

/**
 * Initialize import options with defaults.
 */
void tracker_midi_import_options_init(TrackerMidiImportOptions* opts);

/**
 * Import a MIDI file into a new TrackerSong.
 *
 * Parameters:
 *   filename - Path to the MIDI file
 *   opts     - Import options (NULL for defaults)
 *
 * Returns: New TrackerSong on success, NULL on error.
 *          Caller owns the returned song.
 */
TrackerSong* tracker_midi_import(const char* filename,
                                  const TrackerMidiImportOptions* opts);

/**
 * Get the last import error message.
 *
 * Returns: Error string or NULL if no error.
 */
const char* tracker_midi_import_error(void);

#ifdef __cplusplus
}
#endif

#endif /* TRACKER_MIDI_IMPORT_H */
