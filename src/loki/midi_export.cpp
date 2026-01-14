/* loki_midi_export.cpp - MIDI file export implementation
 *
 * Converts AldaScheduledEvent arrays to Standard MIDI Files using midifile library.
 */

extern "C" {
#include "loki/midi_export.h"
#include "alda.h"
#include <alda/context.h>
}

/* midifile library */
#include <MidiFile.h>

#include <string>
#include <set>
#include <map>

/* Last error message (static storage) */
static std::string g_last_error;

/* MIDI CC number for pan */
#define MIDI_CC_PAN 10

extern "C" {

int loki_midi_export(editor_ctx_t *ctx, const char *filename) {
    (void)ctx;  /* Context passed for future use */

    g_last_error.clear();

    /* Validate filename */
    if (!filename || !*filename) {
        g_last_error = "No filename specified";
        return -1;
    }

    /* Get events from Alda context */
    int event_count = 0;
    const AldaScheduledEvent *events = loki_alda_get_events(&event_count);

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

    /* MidiFile constructor creates track 0 by default.
     * For Type 0: use the existing track 0
     * For Type 1: track 0 is conductor, add tracks 1-N for channels */
    if (!use_type0) {
        /* Type 1: Add one track per channel (track 0 already exists for conductor) */
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
    int tempo = loki_alda_get_tempo(nullptr);

    /* Add initial tempo to track 0 */
    midifile.addTempo(0, 0, static_cast<double>(tempo));

    /* Convert events */
    for (int i = 0; i < event_count; i++) {
        const AldaScheduledEvent &evt = events[i];
        int tick = evt.tick;
        int channel = evt.channel;

        /* Determine target track */
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
                /* Pan is CC #10 */
                midifile.addController(track, tick, channel, MIDI_CC_PAN, evt.data1);
                break;

            case ALDA_EVT_TEMPO:
                /* Tempo events go to track 0 (conductor track) */
                midifile.addTempo(0, tick, static_cast<double>(evt.data1));
                break;

            default:
                /* Unknown event type, skip */
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

const char *loki_midi_export_error(void) {
    return g_last_error.empty() ? nullptr : g_last_error.c_str();
}

}  /* extern "C" */
