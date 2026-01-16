/* loki_midi_export.cpp - MIDI file export implementation
 *
 * Exports MIDI events to Standard MIDI Files using midifile library.
 * Supports both:
 *   - Shared event buffer (loki_midi_export_shared)
 *   - Legacy Alda events (loki_midi_export)
 */

extern "C" {
#include "loki/midi_export.h"
#include "alda.h"
#include "midi/events.h"
#include <alda/context.h>
}

/* midifile library */
#include <MidiFile.h>

#include <string>
#include <set>
#include <map>

/* Last error message (static storage) */
static std::string g_last_error;

extern "C" {

/* ============================================================================
 * Export from Shared Event Buffer
 * ============================================================================ */

int loki_midi_export_shared(const char *filename) {
    g_last_error.clear();

    /* Validate filename */
    if (!filename || !*filename) {
        g_last_error = "No filename specified";
        return -1;
    }

    /* Get events from shared buffer */
    int event_count = 0;
    const SharedMidiEvent *events = shared_midi_events_get(&event_count);

    if (!events || event_count == 0) {
        g_last_error = "No events to export";
        return -1;
    }

    int ticks_per_quarter = shared_midi_events_ticks_per_quarter();
    if (ticks_per_quarter <= 0) {
        ticks_per_quarter = 480;  /* Default fallback */
    }

    /* Determine unique channels to decide Type 0 vs Type 1 */
    std::set<int> channels_used;
    for (int i = 0; i < event_count; i++) {
        if (events[i].type == SHARED_MIDI_NOTE_ON ||
            events[i].type == SHARED_MIDI_NOTE_OFF ||
            events[i].type == SHARED_MIDI_PROGRAM ||
            events[i].type == SHARED_MIDI_CC) {
            channels_used.insert(events[i].channel);
        }
    }

    int num_channels = static_cast<int>(channels_used.size());
    if (num_channels == 0) {
        g_last_error = "No channel events to export";
        return -1;
    }

    /* Create MIDI file */
    smf::MidiFile midifile;
    midifile.setTicksPerQuarterNote(ticks_per_quarter);

    /* Type 0: single track, Type 1: one track per channel + conductor */
    bool use_type0 = (num_channels == 1);

    if (!use_type0) {
        /* Type 1: Need track 0 (conductor) + one track per channel */
        /* MidiFile starts with 1 track (index 0), add one more per channel */
        midifile.addTracks(num_channels);
    }

    /* Map channel -> track index for Type 1 */
    std::map<int, int> channel_to_track;
    if (!use_type0) {
        int track_idx = 1;
        for (int ch : channels_used) {
            channel_to_track[ch] = track_idx++;
        }
    }

    /* Always add default tempo to ensure track 0 has content */
    midifile.addTempo(0, 0, 120.0);

    /* Convert events */
    for (int i = 0; i < event_count; i++) {
        const SharedMidiEvent &evt = events[i];
        int tick = evt.tick;
        int channel = evt.channel;

        /* Determine target track */
        int track = 0;
        if (!use_type0 && channel_to_track.count(channel)) {
            track = channel_to_track[channel];
        }

        switch (evt.type) {
            case SHARED_MIDI_NOTE_ON:
                midifile.addNoteOn(track, tick, channel, evt.data1, evt.data2);
                break;

            case SHARED_MIDI_NOTE_OFF:
                midifile.addNoteOff(track, tick, channel, evt.data1, 0);
                break;

            case SHARED_MIDI_PROGRAM:
                midifile.addPatchChange(track, tick, channel, evt.data1);
                break;

            case SHARED_MIDI_CC:
                midifile.addController(track, tick, channel, evt.data1, evt.data2);
                break;

            case SHARED_MIDI_TEMPO:
                /* Tempo events go to track 0 (conductor track) */
                midifile.addTempo(0, tick, static_cast<double>(evt.data1));
                break;
        }
    }

    /* Sort tracks (ensures events are in correct order) */
    midifile.sortTracks();

    /* Write the file */
    if (!midifile.write(filename)) {
        g_last_error = "Failed to write MIDI file";
        return -1;
    }

    return 0;
}

/* ============================================================================
 * Legacy: Export from Alda Events
 * ============================================================================ */

int loki_midi_export(editor_ctx_t *ctx, const char *filename) {
    g_last_error.clear();

    /* Validate filename */
    if (!filename || !*filename) {
        g_last_error = "No filename specified";
        return -1;
    }

    /* Get events from Alda context */
    int event_count = 0;
    const AldaScheduledEvent *events = loki_alda_get_events(ctx, &event_count);

    if (!events || event_count == 0) {
        g_last_error = "No events to export (play Alda code first)";
        return -1;
    }

    /* Determine unique channels to decide Type 0 vs Type 1 */
    std::set<int> channels_used;
    for (int i = 0; i < event_count; i++) {
        if (events[i].type == ALDA_EVT_NOTE_ON ||
            events[i].type == ALDA_EVT_NOTE_OFF ||
            events[i].type == ALDA_EVT_PROGRAM ||
            events[i].type == ALDA_EVT_CC ||
            events[i].type == ALDA_EVT_PAN) {
            channels_used.insert(events[i].channel);
        }
    }

    int num_channels = static_cast<int>(channels_used.size());
    if (num_channels == 0) {
        g_last_error = "No channel events to export";
        return -1;
    }

    /* Create MIDI file */
    smf::MidiFile midifile;
    midifile.setTicksPerQuarterNote(ALDA_TICKS_PER_QUARTER);

    /* Type 0: single track, Type 1: one track per channel */
    bool use_type0 = (num_channels == 1);

    if (!use_type0) {
        for (int i = 0; i < num_channels; i++) {
            midifile.addTrack();
        }
    }

    /* Map channel -> track index for Type 1 */
    std::map<int, int> channel_to_track;
    if (!use_type0) {
        int track_idx = 1;
        for (int ch : channels_used) {
            channel_to_track[ch] = track_idx++;
        }
    }

    /* Get initial tempo */
    int tempo = loki_alda_get_tempo(ctx);
    midifile.addTempo(0, 0, static_cast<double>(tempo));

    /* Convert events */
    for (int i = 0; i < event_count; i++) {
        const AldaScheduledEvent &evt = events[i];
        int tick = evt.tick;
        int channel = evt.channel;

        int track = 0;
        if (!use_type0 && channel_to_track.count(channel)) {
            track = channel_to_track[channel];
        }

        switch (evt.type) {
            case ALDA_EVT_NOTE_ON:
                midifile.addNoteOn(track, tick, channel, evt.data1, evt.data2);
                break;

            case ALDA_EVT_NOTE_OFF:
                midifile.addNoteOff(track, tick, channel, evt.data1, evt.data2);
                break;

            case ALDA_EVT_PROGRAM:
                midifile.addPatchChange(track, tick, channel, evt.data1);
                break;

            case ALDA_EVT_CC:
                midifile.addController(track, tick, channel, evt.data1, evt.data2);
                break;

            case ALDA_EVT_PAN:
                midifile.addController(track, tick, channel, 10, evt.data1);
                break;

            case ALDA_EVT_TEMPO:
                midifile.addTempo(0, tick, static_cast<double>(evt.data1));
                break;

            default:
                break;
        }
    }

    midifile.sortTracks();

    if (!midifile.write(filename)) {
        g_last_error = "Failed to write MIDI file";
        return -1;
    }

    return 0;
}

const char *loki_midi_export_error(void) {
    return g_last_error.empty() ? nullptr : g_last_error.c_str();
}

}  /* extern "C" */
