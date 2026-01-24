/**
 * tracker_midi_import.cpp - MIDI file import implementation
 *
 * Uses the midifile library to read Standard MIDI Files and convert
 * them to TrackerSong format.
 */

extern "C" {
#include "tracker_midi_import.h"
#include "music/music_theory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
}

#include <MidiFile.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

/* Last error message */
static std::string g_last_error;

/* Helper: extract filename without path and extension */
static std::string extract_song_name(const char* filename) {
    std::string path(filename);

    /* Find last path separator */
    size_t slash = path.find_last_of("/\\");
    if (slash != std::string::npos) {
        path = path.substr(slash + 1);
    }

    /* Remove extension */
    size_t dot = path.find_last_of('.');
    if (dot != std::string::npos) {
        path = path.substr(0, dot);
    }

    return path.empty() ? "Imported" : path;
}

/* Note event for sorting */
struct NoteEvent {
    int tick;           /* Absolute tick in MIDI file */
    int channel;        /* MIDI channel */
    int pitch;          /* MIDI pitch */
    int velocity;       /* Note-on velocity */
    int duration_ticks; /* Duration in MIDI ticks */
};

extern "C" {

void tracker_midi_import_options_init(TrackerMidiImportOptions* opts) {
    if (!opts) return;

    opts->rows_per_beat = 4;
    opts->ticks_per_row = 6;
    opts->pattern_rows = 64;
    opts->quantize_strength = 100;
    opts->velocity_threshold = 1;
    opts->include_velocity = 1;
    opts->split_by_channel = 1;
    opts->max_tracks = 16;
}

TrackerSong* tracker_midi_import(const char* filename,
                                  const TrackerMidiImportOptions* opts) {
    g_last_error.clear();

    if (!filename || !*filename) {
        g_last_error = "No filename specified";
        return NULL;
    }

    /* Use default options if not provided */
    TrackerMidiImportOptions default_opts;
    if (!opts) {
        tracker_midi_import_options_init(&default_opts);
        opts = &default_opts;
    }

    /* Read MIDI file */
    smf::MidiFile midifile;
    if (!midifile.read(filename)) {
        g_last_error = "Failed to read MIDI file: ";
        g_last_error += filename;
        return NULL;
    }

    /* Convert to absolute ticks and link note-ons/offs */
    midifile.doTimeAnalysis();
    midifile.linkNotePairs();

    int tpq = midifile.getTicksPerQuarterNote();
    if (tpq <= 0) tpq = 480;

    /* Extract tempo (default 120 BPM) */
    int bpm = 120;
    for (int track = 0; track < midifile.getTrackCount(); track++) {
        for (int i = 0; i < midifile[track].size(); i++) {
            if (midifile[track][i].isTempo()) {
                bpm = (int)midifile[track][i].getTempoBPM();
                break;
            }
        }
    }

    /* Collect all note events, grouped by channel */
    std::map<int, std::vector<NoteEvent>> channel_notes;

    for (int track = 0; track < midifile.getTrackCount(); track++) {
        for (int i = 0; i < midifile[track].size(); i++) {
            smf::MidiEvent& evt = midifile[track][i];

            if (evt.isNoteOn() && evt.getVelocity() > 0) {
                NoteEvent note;
                note.tick = evt.tick;
                note.channel = evt.getChannel();
                note.pitch = evt.getKeyNumber();
                note.velocity = evt.getVelocity();

                /* Get duration from linked note-off */
                note.duration_ticks = evt.getTickDuration();

                if (note.velocity >= opts->velocity_threshold) {
                    channel_notes[note.channel].push_back(note);
                }
            }
        }
    }

    if (channel_notes.empty()) {
        g_last_error = "No notes found in MIDI file";
        return NULL;
    }

    /* Sort notes within each channel by tick */
    for (auto& pair : channel_notes) {
        std::sort(pair.second.begin(), pair.second.end(),
            [](const NoteEvent& a, const NoteEvent& b) {
                return a.tick < b.tick;
            });
    }

    /* Calculate timing conversion factors */
    /* MIDI ticks per tracker row */
    double ticks_per_beat = (double)tpq;
    double ticks_per_row = ticks_per_beat / opts->rows_per_beat;

    /* Find the total duration in rows */
    int max_tick = 0;
    for (const auto& pair : channel_notes) {
        for (const auto& note : pair.second) {
            int end_tick = note.tick + note.duration_ticks;
            if (end_tick > max_tick) max_tick = end_tick;
        }
    }

    int total_rows = (int)(max_tick / ticks_per_row) + 1;
    int num_patterns = (total_rows + opts->pattern_rows - 1) / opts->pattern_rows;
    if (num_patterns < 1) num_patterns = 1;

    /* Create song */
    std::string song_name = extract_song_name(filename);
    TrackerSong* song = tracker_song_new(song_name.c_str());
    if (!song) {
        g_last_error = "Failed to create song";
        return NULL;
    }

    song->bpm = bpm;
    song->rows_per_beat = opts->rows_per_beat;
    song->ticks_per_row = opts->ticks_per_row;

    /* Determine number of tracks */
    int num_tracks = opts->split_by_channel ?
        (int)channel_notes.size() : 1;
    if (num_tracks > opts->max_tracks) {
        num_tracks = opts->max_tracks;
    }

    /* Map channels to track indices */
    std::map<int, int> channel_to_track;
    int track_idx = 0;
    for (const auto& pair : channel_notes) {
        if (track_idx >= num_tracks) break;
        channel_to_track[pair.first] = track_idx++;
    }

    /* Create patterns */
    for (int p = 0; p < num_patterns; p++) {
        int rows_in_pattern = opts->pattern_rows;
        if (p == num_patterns - 1) {
            /* Last pattern may be shorter */
            int remaining = total_rows - p * opts->pattern_rows;
            if (remaining > 0 && remaining < opts->pattern_rows) {
                rows_in_pattern = remaining;
            }
        }

        char pattern_name[32];
        snprintf(pattern_name, sizeof(pattern_name), "Pattern %d", p + 1);

        TrackerPattern* pattern = tracker_pattern_new(rows_in_pattern,
                                                       num_tracks, pattern_name);
        if (!pattern) {
            g_last_error = "Failed to create pattern";
            tracker_song_free(song);
            return NULL;
        }

        /* Set track names and channels */
        track_idx = 0;
        for (const auto& pair : channel_notes) {
            if (track_idx >= num_tracks) break;

            TrackerTrack* track = &pattern->tracks[track_idx];
            char track_name[32];
            snprintf(track_name, sizeof(track_name), "Ch %d", pair.first + 1);
            free(track->name);
            track->name = strdup(track_name);
            track->default_channel = pair.first;
            track_idx++;
        }

        tracker_song_add_pattern(song, pattern);
    }

    /* Place notes into cells */
    for (const auto& pair : channel_notes) {
        int channel = pair.first;
        if (channel_to_track.find(channel) == channel_to_track.end()) {
            continue;  /* Skip channels beyond max_tracks */
        }

        int track = channel_to_track[channel];

        for (const auto& note : pair.second) {
            /* Convert MIDI tick to tracker row */
            int row = (int)(note.tick / ticks_per_row);

            /* Apply quantization */
            if (opts->quantize_strength == 100) {
                row = (int)((note.tick + ticks_per_row / 2) / ticks_per_row);
            }

            /* Determine pattern and row within pattern */
            int pattern_idx = row / opts->pattern_rows;
            int row_in_pattern = row % opts->pattern_rows;

            if (pattern_idx >= song->num_patterns) {
                continue;  /* Note beyond song length */
            }

            TrackerPattern* pattern = song->patterns[pattern_idx];
            if (row_in_pattern >= pattern->num_rows) {
                continue;
            }

            TrackerCell* cell = tracker_pattern_get_cell(pattern,
                                                          row_in_pattern, track);
            if (!cell) continue;

            /* Convert pitch to note name */
            char note_name[8];
            if (!music_pitch_to_name(note.pitch, note_name, sizeof(note_name), 1)) {
                continue;
            }

            /* Build expression */
            char expression[64];
            if (opts->include_velocity && note.velocity != 100) {
                snprintf(expression, sizeof(expression), "%s@%d",
                         note_name, note.velocity);
            } else {
                snprintf(expression, sizeof(expression), "%s", note_name);
            }

            /* Calculate gate (duration in rows) */
            int duration_rows = (int)(note.duration_ticks / ticks_per_row);
            if (duration_rows > 1) {
                /* Append gate suffix */
                char gate_str[16];
                snprintf(gate_str, sizeof(gate_str), "~%d", duration_rows);
                strncat(expression, gate_str,
                        sizeof(expression) - strlen(expression) - 1);
            }

            /* Handle polyphony: append to existing cell content */
            if (cell->type == TRACKER_CELL_EXPRESSION && cell->expression) {
                /* Check if there's already content */
                std::string existing(cell->expression);
                if (!existing.empty()) {
                    existing += " ";
                    existing += expression;
                    tracker_cell_set_expression(cell, existing.c_str(), NULL);
                    continue;
                }
            }

            tracker_cell_set_expression(cell, expression, NULL);
        }
    }

    /* Add all patterns to sequence */
    for (int p = 0; p < song->num_patterns; p++) {
        tracker_song_append_to_sequence(song, p, 1);
    }

    return song;
}

const char* tracker_midi_import_error(void) {
    return g_last_error.empty() ? NULL : g_last_error.c_str();
}

}  /* extern "C" */
