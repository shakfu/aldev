/**
 * tracker_view_json.c - JSON serialization for web view sync
 */

#include "tracker_view.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*============================================================================
 * JSON Writer Helpers
 *============================================================================*/

static void json_write_raw(TrackerJsonWriter* w, const char* s) {
    if (w->write && s) {
        w->write(w->user_data, s, (int)strlen(s));
    }
}

static void json_write_indent(TrackerJsonWriter* w) {
    if (!w->pretty) return;
    for (int i = 0; i < w->depth * w->indent; i++) {
        w->write(w->user_data, " ", 1);
    }
}

static void json_write_newline(TrackerJsonWriter* w) {
    if (w->pretty) {
        w->write(w->user_data, "\n", 1);
    }
}

static void json_write_string(TrackerJsonWriter* w, const char* s) {
    w->write(w->user_data, "\"", 1);
    if (s) {
        /* Escape special characters */
        const char* p = s;
        while (*p) {
            char c = *p++;
            switch (c) {
                case '"':  w->write(w->user_data, "\\\"", 2); break;
                case '\\': w->write(w->user_data, "\\\\", 2); break;
                case '\b': w->write(w->user_data, "\\b", 2);  break;
                case '\f': w->write(w->user_data, "\\f", 2);  break;
                case '\n': w->write(w->user_data, "\\n", 2);  break;
                case '\r': w->write(w->user_data, "\\r", 2);  break;
                case '\t': w->write(w->user_data, "\\t", 2);  break;
                default:
                    if ((unsigned char)c < 0x20) {
                        char buf[8];
                        snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                        w->write(w->user_data, buf, 6);
                    } else {
                        w->write(w->user_data, &c, 1);
                    }
            }
        }
    }
    w->write(w->user_data, "\"", 1);
}

static void json_write_int(TrackerJsonWriter* w, int64_t n) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", (long long)n);
    json_write_raw(w, buf);
}

static void json_write_double(TrackerJsonWriter* w, double d) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%g", d);
    json_write_raw(w, buf);
}

static void json_write_bool(TrackerJsonWriter* w, bool b) {
    json_write_raw(w, b ? "true" : "false");
}

static void json_write_null(TrackerJsonWriter* w) {
    json_write_raw(w, "null");
}

static void json_begin_object(TrackerJsonWriter* w) {
    json_write_raw(w, "{");
    w->depth++;
    json_write_newline(w);
}

static void json_end_object(TrackerJsonWriter* w) {
    w->depth--;
    json_write_newline(w);
    json_write_indent(w);
    json_write_raw(w, "}");
}

static void json_begin_array(TrackerJsonWriter* w) {
    json_write_raw(w, "[");
    w->depth++;
    json_write_newline(w);
}

static void json_end_array(TrackerJsonWriter* w) {
    w->depth--;
    json_write_newline(w);
    json_write_indent(w);
    json_write_raw(w, "]");
}

static void json_write_key(TrackerJsonWriter* w, const char* key, bool first) {
    if (!first) {
        json_write_raw(w, ",");
        json_write_newline(w);
    }
    json_write_indent(w);
    json_write_string(w, key);
    json_write_raw(w, w->pretty ? ": " : ":");
}

static void json_array_sep(TrackerJsonWriter* w, bool first) {
    if (!first) {
        json_write_raw(w, ",");
        json_write_newline(w);
    }
    json_write_indent(w);
}

/*============================================================================
 * Public API
 *============================================================================*/

void tracker_json_writer_init(TrackerJsonWriter* w, TrackerJsonWriteFn write_fn,
                              void* user_data, bool pretty) {
    if (!w) return;
    w->write = write_fn;
    w->user_data = user_data;
    w->depth = 0;
    w->pretty = pretty;
    w->indent = 2;
}

/*============================================================================
 * Color/Style Serialization
 *============================================================================*/

void tracker_json_write_color(TrackerJsonWriter* w, const TrackerColor* color) {
    if (!w || !color) {
        json_write_null(w);
        return;
    }

    json_begin_object(w);

    json_write_key(w, "type", true);
    switch (color->type) {
        case TRACKER_COLOR_DEFAULT:
            json_write_string(w, "default");
            break;
        case TRACKER_COLOR_INDEXED:
            json_write_string(w, "indexed");
            json_write_key(w, "index", false);
            json_write_int(w, color->value.index);
            break;
        case TRACKER_COLOR_RGB:
            json_write_string(w, "rgb");
            json_write_key(w, "r", false);
            json_write_int(w, color->value.rgb.r);
            json_write_key(w, "g", false);
            json_write_int(w, color->value.rgb.g);
            json_write_key(w, "b", false);
            json_write_int(w, color->value.rgb.b);
            break;
    }

    json_end_object(w);
}

void tracker_json_write_style(TrackerJsonWriter* w, const TrackerStyle* style) {
    if (!w || !style) {
        json_write_null(w);
        return;
    }

    json_begin_object(w);

    json_write_key(w, "fg", true);
    tracker_json_write_color(w, &style->fg);

    json_write_key(w, "bg", false);
    tracker_json_write_color(w, &style->bg);

    json_write_key(w, "attr", false);
    json_write_int(w, style->attr);

    json_end_object(w);
}

/*============================================================================
 * Theme Serialization
 *============================================================================*/

void tracker_json_write_theme(TrackerJsonWriter* w, const TrackerTheme* theme) {
    if (!w || !theme) {
        json_write_null(w);
        return;
    }

    json_begin_object(w);

    json_write_key(w, "name", true);
    json_write_string(w, theme->name);

    json_write_key(w, "author", false);
    json_write_string(w, theme->author);

    /* Base styles */
    json_write_key(w, "default_style", false);
    tracker_json_write_style(w, &theme->default_style);

    json_write_key(w, "header_style", false);
    tracker_json_write_style(w, &theme->header_style);

    json_write_key(w, "status_style", false);
    tracker_json_write_style(w, &theme->status_style);

    json_write_key(w, "command_style", false);
    tracker_json_write_style(w, &theme->command_style);

    json_write_key(w, "error_style", false);
    tracker_json_write_style(w, &theme->error_style);

    json_write_key(w, "message_style", false);
    tracker_json_write_style(w, &theme->message_style);

    /* Grid colors */
    json_write_key(w, "cell_empty", false);
    tracker_json_write_style(w, &theme->cell_empty);

    json_write_key(w, "cell_note", false);
    tracker_json_write_style(w, &theme->cell_note);

    json_write_key(w, "cell_fx", false);
    tracker_json_write_style(w, &theme->cell_fx);

    json_write_key(w, "cell_off", false);
    tracker_json_write_style(w, &theme->cell_off);

    json_write_key(w, "cell_continuation", false);
    tracker_json_write_style(w, &theme->cell_continuation);

    /* Cursor and selection */
    json_write_key(w, "cursor", false);
    tracker_json_write_style(w, &theme->cursor);

    json_write_key(w, "cursor_edit", false);
    tracker_json_write_style(w, &theme->cursor_edit);

    json_write_key(w, "selection", false);
    tracker_json_write_style(w, &theme->selection);

    json_write_key(w, "selection_cursor", false);
    tracker_json_write_style(w, &theme->selection_cursor);

    /* Playback */
    json_write_key(w, "playing_row", false);
    tracker_json_write_style(w, &theme->playing_row);

    json_write_key(w, "playing_cell", false);
    tracker_json_write_style(w, &theme->playing_cell);

    /* Row highlighting */
    json_write_key(w, "row_beat", false);
    tracker_json_write_style(w, &theme->row_beat);

    json_write_key(w, "row_bar", false);
    tracker_json_write_style(w, &theme->row_bar);

    json_write_key(w, "row_alternate", false);
    tracker_json_write_style(w, &theme->row_alternate);

    /* Track states */
    json_write_key(w, "track_muted", false);
    tracker_json_write_style(w, &theme->track_muted);

    json_write_key(w, "track_solo", false);
    tracker_json_write_style(w, &theme->track_solo);

    json_write_key(w, "track_active", false);
    tracker_json_write_style(w, &theme->track_active);

    /* Validation */
    json_write_key(w, "cell_error", false);
    tracker_json_write_style(w, &theme->cell_error);

    json_write_key(w, "cell_warning", false);
    tracker_json_write_style(w, &theme->cell_warning);

    /* Active notes */
    json_write_key(w, "note_active", false);
    tracker_json_write_style(w, &theme->note_active);

    json_write_key(w, "note_velocity", false);
    json_begin_array(w);
    for (int i = 0; i < 4; i++) {
        json_array_sep(w, i == 0);
        tracker_json_write_style(w, &theme->note_velocity[i]);
    }
    json_end_array(w);

    /* Scrollbar */
    json_write_key(w, "scrollbar_track", false);
    tracker_json_write_style(w, &theme->scrollbar_track);

    json_write_key(w, "scrollbar_thumb", false);
    tracker_json_write_style(w, &theme->scrollbar_thumb);

    /* Borders */
    json_write_key(w, "border_color", false);
    tracker_json_write_color(w, &theme->border_color);

    json_write_key(w, "separator_color", false);
    tracker_json_write_color(w, &theme->separator_color);

    /* Border characters */
    json_write_key(w, "border_h", false);
    json_write_string(w, theme->border_h);

    json_write_key(w, "border_v", false);
    json_write_string(w, theme->border_v);

    json_write_key(w, "note_off_marker", false);
    json_write_string(w, theme->note_off_marker);

    json_write_key(w, "continuation_marker", false);
    json_write_string(w, theme->continuation_marker);

    json_write_key(w, "empty_cell", false);
    json_write_string(w, theme->empty_cell);

    json_end_object(w);
}

/*============================================================================
 * FX Chain Serialization
 *============================================================================*/

void tracker_json_write_fx_chain(TrackerJsonWriter* w, const TrackerFxChain* chain) {
    if (!w || !chain) {
        json_write_null(w);
        return;
    }

    json_begin_object(w);

    json_write_key(w, "count", true);
    json_write_int(w, chain->count);

    json_write_key(w, "entries", false);
    json_begin_array(w);

    for (int i = 0; i < chain->count; i++) {
        const TrackerFxEntry* e = &chain->entries[i];
        json_array_sep(w, i == 0);
        json_begin_object(w);

        json_write_key(w, "name", true);
        json_write_string(w, e->name);

        json_write_key(w, "params", false);
        json_write_string(w, e->params);

        json_write_key(w, "language_id", false);
        json_write_string(w, e->language_id);

        json_write_key(w, "enabled", false);
        json_write_bool(w, e->enabled);

        json_end_object(w);
    }

    json_end_array(w);
    json_end_object(w);
}

/*============================================================================
 * Phrase Serialization
 *============================================================================*/

void tracker_json_write_phrase(TrackerJsonWriter* w, const TrackerPhrase* phrase) {
    if (!w || !phrase) {
        json_write_null(w);
        return;
    }

    json_begin_object(w);

    json_write_key(w, "count", true);
    json_write_int(w, phrase->count);

    json_write_key(w, "capacity", false);
    json_write_int(w, phrase->capacity);

    json_write_key(w, "events", false);
    json_begin_array(w);

    for (int i = 0; i < phrase->count; i++) {
        const TrackerEvent* e = &phrase->events[i];
        json_array_sep(w, i == 0);
        json_begin_object(w);

        json_write_key(w, "type", true);
        switch (e->type) {
            case TRACKER_EVENT_NOTE_ON:         json_write_string(w, "note_on");    break;
            case TRACKER_EVENT_NOTE_OFF:        json_write_string(w, "note_off");   break;
            case TRACKER_EVENT_CC:              json_write_string(w, "cc");         break;
            case TRACKER_EVENT_PITCH_BEND:      json_write_string(w, "pitch_bend"); break;
            case TRACKER_EVENT_PROGRAM_CHANGE:  json_write_string(w, "program");    break;
            case TRACKER_EVENT_AFTERTOUCH:      json_write_string(w, "aftertouch"); break;
            case TRACKER_EVENT_POLY_AFTERTOUCH: json_write_string(w, "poly_at");    break;
            default:                            json_write_string(w, "unknown");    break;
        }

        json_write_key(w, "offset_rows", false);
        json_write_int(w, e->offset_rows);

        json_write_key(w, "offset_ticks", false);
        json_write_int(w, e->offset_ticks);

        json_write_key(w, "channel", false);
        json_write_int(w, e->channel);

        json_write_key(w, "data1", false);
        json_write_int(w, e->data1);

        json_write_key(w, "data2", false);
        json_write_int(w, e->data2);

        json_write_key(w, "gate_rows", false);
        json_write_int(w, e->gate_rows);

        json_write_key(w, "gate_ticks", false);
        json_write_int(w, e->gate_ticks);

        json_write_key(w, "flags", false);
        json_write_int(w, e->flags);

        /* Extended params if present */
        if (e->params) {
            json_write_key(w, "params", false);
            json_begin_object(w);

            json_write_key(w, "probability", true);
            json_write_int(w, e->params->probability);

            json_write_key(w, "humanize_time_amt", false);
            json_write_int(w, e->params->humanize_time_amt);

            json_write_key(w, "humanize_vel_amt", false);
            json_write_int(w, e->params->humanize_vel_amt);

            json_write_key(w, "accent_boost", false);
            json_write_int(w, e->params->accent_boost);

            json_write_key(w, "retrigger_count", false);
            json_write_int(w, e->params->retrigger_count);

            json_write_key(w, "retrigger_rate", false);
            json_write_int(w, e->params->retrigger_rate);

            json_write_key(w, "slide_time", false);
            json_write_int(w, e->params->slide_time);

            json_end_object(w);
        }

        json_end_object(w);
    }

    json_end_array(w);
    json_end_object(w);
}

/*============================================================================
 * Cell Serialization
 *============================================================================*/

void tracker_json_write_cell(TrackerJsonWriter* w, const TrackerCell* cell) {
    if (!w || !cell) {
        json_write_null(w);
        return;
    }

    json_begin_object(w);

    json_write_key(w, "type", true);
    switch (cell->type) {
        case TRACKER_CELL_EMPTY:        json_write_string(w, "empty");        break;
        case TRACKER_CELL_EXPRESSION:   json_write_string(w, "expression");   break;
        case TRACKER_CELL_NOTE_OFF:     json_write_string(w, "note_off");     break;
        case TRACKER_CELL_CONTINUATION: json_write_string(w, "continuation"); break;
        default:                        json_write_string(w, "unknown");      break;
    }

    json_write_key(w, "expression", false);
    json_write_string(w, cell->expression);

    json_write_key(w, "language_id", false);
    json_write_string(w, cell->language_id);

    json_write_key(w, "dirty", false);
    json_write_bool(w, cell->dirty);

    /* FX chain */
    json_write_key(w, "fx_chain", false);
    tracker_json_write_fx_chain(w, &cell->fx_chain);

    json_end_object(w);
}

/*============================================================================
 * Track Serialization
 *============================================================================*/

void tracker_json_write_track(TrackerJsonWriter* w, const TrackerTrack* track, int num_rows) {
    if (!w || !track) {
        json_write_null(w);
        return;
    }

    json_begin_object(w);

    json_write_key(w, "name", true);
    json_write_string(w, track->name);

    json_write_key(w, "default_channel", false);
    json_write_int(w, track->default_channel);

    json_write_key(w, "muted", false);
    json_write_bool(w, track->muted);

    json_write_key(w, "solo", false);
    json_write_bool(w, track->solo);

    json_write_key(w, "fx_chain", false);
    tracker_json_write_fx_chain(w, &track->fx_chain);

    json_write_key(w, "cells", false);
    json_begin_array(w);

    for (int r = 0; r < num_rows; r++) {
        json_array_sep(w, r == 0);
        tracker_json_write_cell(w, &track->cells[r]);
    }

    json_end_array(w);
    json_end_object(w);
}

/*============================================================================
 * Pattern Serialization
 *============================================================================*/

void tracker_json_write_pattern(TrackerJsonWriter* w, const TrackerPattern* pattern) {
    if (!w || !pattern) {
        json_write_null(w);
        return;
    }

    json_begin_object(w);

    json_write_key(w, "name", true);
    json_write_string(w, pattern->name);

    json_write_key(w, "num_rows", false);
    json_write_int(w, pattern->num_rows);

    json_write_key(w, "num_tracks", false);
    json_write_int(w, pattern->num_tracks);

    json_write_key(w, "tracks", false);
    json_begin_array(w);

    for (int t = 0; t < pattern->num_tracks; t++) {
        json_array_sep(w, t == 0);
        tracker_json_write_track(w, &pattern->tracks[t], pattern->num_rows);
    }

    json_end_array(w);
    json_end_object(w);
}

/*============================================================================
 * Song Serialization
 *============================================================================*/

void tracker_json_write_song(TrackerJsonWriter* w, const TrackerSong* song) {
    if (!w || !song) {
        json_write_null(w);
        return;
    }

    json_begin_object(w);

    json_write_key(w, "name", true);
    json_write_string(w, song->name);

    json_write_key(w, "author", false);
    json_write_string(w, song->author);

    json_write_key(w, "bpm", false);
    json_write_int(w, song->bpm);

    json_write_key(w, "rows_per_beat", false);
    json_write_int(w, song->rows_per_beat);

    json_write_key(w, "ticks_per_row", false);
    json_write_int(w, song->ticks_per_row);

    json_write_key(w, "spillover_mode", false);
    switch (song->spillover_mode) {
        case TRACKER_SPILLOVER_LAYER:    json_write_string(w, "layer");    break;
        case TRACKER_SPILLOVER_TRUNCATE: json_write_string(w, "truncate"); break;
        case TRACKER_SPILLOVER_LOOP:     json_write_string(w, "loop");     break;
        default:                         json_write_string(w, "layer");    break;
    }

    json_write_key(w, "default_language_id", false);
    json_write_string(w, song->default_language_id);

    json_write_key(w, "master_fx", false);
    tracker_json_write_fx_chain(w, &song->master_fx);

    json_write_key(w, "num_patterns", false);
    json_write_int(w, song->num_patterns);

    json_write_key(w, "patterns", false);
    json_begin_array(w);

    for (int p = 0; p < song->num_patterns; p++) {
        json_array_sep(w, p == 0);
        tracker_json_write_pattern(w, song->patterns[p]);
    }

    json_end_array(w);

    /* Sequence (arrangement) */
    json_write_key(w, "sequence_length", false);
    json_write_int(w, song->sequence_length);

    json_write_key(w, "sequence", false);
    json_begin_array(w);

    for (int i = 0; i < song->sequence_length; i++) {
        const TrackerSequenceEntry* e = &song->sequence[i];
        json_array_sep(w, i == 0);
        json_begin_object(w);

        json_write_key(w, "pattern_index", true);
        json_write_int(w, e->pattern_index);

        json_write_key(w, "repeat_count", false);
        json_write_int(w, e->repeat_count);

        json_end_object(w);
    }

    json_end_array(w);
    json_end_object(w);
}

/*============================================================================
 * Selection Serialization
 *============================================================================*/

void tracker_json_write_selection(TrackerJsonWriter* w, const TrackerSelection* sel) {
    if (!w || !sel) {
        json_write_null(w);
        return;
    }

    json_begin_object(w);

    json_write_key(w, "type", true);
    switch (sel->type) {
        case TRACKER_SEL_NONE:    json_write_string(w, "none");    break;
        case TRACKER_SEL_CELL:    json_write_string(w, "cell");    break;
        case TRACKER_SEL_RANGE:   json_write_string(w, "range");   break;
        case TRACKER_SEL_TRACK:   json_write_string(w, "track");   break;
        case TRACKER_SEL_ROW:     json_write_string(w, "row");     break;
        case TRACKER_SEL_PATTERN: json_write_string(w, "pattern"); break;
        default:                  json_write_string(w, "none");    break;
    }

    json_write_key(w, "anchor_track", false);
    json_write_int(w, sel->anchor_track);

    json_write_key(w, "anchor_row", false);
    json_write_int(w, sel->anchor_row);

    json_write_key(w, "start_track", false);
    json_write_int(w, sel->start_track);

    json_write_key(w, "end_track", false);
    json_write_int(w, sel->end_track);

    json_write_key(w, "start_row", false);
    json_write_int(w, sel->start_row);

    json_write_key(w, "end_row", false);
    json_write_int(w, sel->end_row);

    json_write_key(w, "start_pattern", false);
    json_write_int(w, sel->start_pattern);

    json_write_key(w, "end_pattern", false);
    json_write_int(w, sel->end_pattern);

    json_end_object(w);
}

/*============================================================================
 * View State Serialization
 *============================================================================*/

void tracker_json_write_view_state(TrackerJsonWriter* w, const TrackerViewState* state) {
    if (!w || !state) {
        json_write_null(w);
        return;
    }

    json_begin_object(w);

    json_write_key(w, "view_mode", true);
    switch (state->view_mode) {
        case TRACKER_VIEW_MODE_PATTERN:    json_write_string(w, "pattern");    break;
        case TRACKER_VIEW_MODE_ARRANGE:    json_write_string(w, "arrange");    break;
        case TRACKER_VIEW_MODE_MIXER:      json_write_string(w, "mixer");      break;
        case TRACKER_VIEW_MODE_INSTRUMENT: json_write_string(w, "instrument"); break;
        case TRACKER_VIEW_MODE_SONG:       json_write_string(w, "song");       break;
        case TRACKER_VIEW_MODE_HELP:       json_write_string(w, "help");       break;
        default:                           json_write_string(w, "pattern");    break;
    }

    json_write_key(w, "edit_mode", false);
    switch (state->edit_mode) {
        case TRACKER_EDIT_MODE_NAVIGATE: json_write_string(w, "navigate"); break;
        case TRACKER_EDIT_MODE_EDIT:     json_write_string(w, "edit");     break;
        case TRACKER_EDIT_MODE_SELECT:   json_write_string(w, "select");   break;
        case TRACKER_EDIT_MODE_COMMAND:  json_write_string(w, "command");  break;
        default:                         json_write_string(w, "navigate"); break;
    }

    json_write_key(w, "cursor_pattern", false);
    json_write_int(w, state->cursor_pattern);

    json_write_key(w, "cursor_track", false);
    json_write_int(w, state->cursor_track);

    json_write_key(w, "cursor_row", false);
    json_write_int(w, state->cursor_row);

    json_write_key(w, "selection", false);
    tracker_json_write_selection(w, &state->selection);

    json_write_key(w, "selecting", false);
    json_write_bool(w, state->selecting);

    json_write_key(w, "scroll_track", false);
    json_write_int(w, state->scroll_track);

    json_write_key(w, "scroll_row", false);
    json_write_int(w, state->scroll_row);

    json_write_key(w, "visible_tracks", false);
    json_write_int(w, state->visible_tracks);

    json_write_key(w, "visible_rows", false);
    json_write_int(w, state->visible_rows);

    json_write_key(w, "edit_buffer", false);
    json_write_string(w, state->edit_buffer);

    json_write_key(w, "edit_cursor_pos", false);
    json_write_int(w, state->edit_cursor_pos);

    json_write_key(w, "command_buffer", false);
    json_write_string(w, state->command_buffer);

    json_write_key(w, "command_cursor_pos", false);
    json_write_int(w, state->command_cursor_pos);

    /* Display options */
    json_write_key(w, "follow_playback", false);
    json_write_bool(w, state->follow_playback);

    json_write_key(w, "show_row_numbers", false);
    json_write_bool(w, state->show_row_numbers);

    json_write_key(w, "show_track_headers", false);
    json_write_bool(w, state->show_track_headers);

    json_write_key(w, "show_transport", false);
    json_write_bool(w, state->show_transport);

    json_write_key(w, "show_status_line", false);
    json_write_bool(w, state->show_status_line);

    json_write_key(w, "highlight_current_row", false);
    json_write_bool(w, state->highlight_current_row);

    json_write_key(w, "highlight_beat_rows", false);
    json_write_bool(w, state->highlight_beat_rows);

    json_write_key(w, "beat_highlight_interval", false);
    json_write_int(w, state->beat_highlight_interval);

    /* Playback position */
    json_write_key(w, "playback_pattern", false);
    json_write_int(w, state->playback_pattern);

    json_write_key(w, "playback_row", false);
    json_write_int(w, state->playback_row);

    json_write_key(w, "is_playing", false);
    json_write_bool(w, state->is_playing);

    /* Error/status */
    json_write_key(w, "error_message", false);
    json_write_string(w, state->error_message);

    json_write_key(w, "status_message", false);
    json_write_string(w, state->status_message);

    /* Theme */
    if (state->theme) {
        json_write_key(w, "theme", false);
        tracker_json_write_theme(w, state->theme);
    }

    json_end_object(w);
}

/*============================================================================
 * Playback State Serialization
 *============================================================================*/

void tracker_json_write_playback_state(TrackerJsonWriter* w, const TrackerEngine* engine) {
    if (!w || !engine) {
        json_write_null(w);
        return;
    }

    json_begin_object(w);

    json_write_key(w, "state", true);
    switch (engine->state) {
        case TRACKER_ENGINE_STOPPED:   json_write_string(w, "stopped");   break;
        case TRACKER_ENGINE_PLAYING:   json_write_string(w, "playing");   break;
        case TRACKER_ENGINE_PAUSED:    json_write_string(w, "paused");    break;
        case TRACKER_ENGINE_RECORDING: json_write_string(w, "recording"); break;
        default:                       json_write_string(w, "stopped");   break;
    }

    json_write_key(w, "play_mode", false);
    switch (engine->play_mode) {
        case TRACKER_PLAY_MODE_PATTERN: json_write_string(w, "pattern"); break;
        case TRACKER_PLAY_MODE_SONG:    json_write_string(w, "song");    break;
        default:                        json_write_string(w, "pattern"); break;
    }

    json_write_key(w, "pattern", false);
    json_write_int(w, engine->current_pattern);

    json_write_key(w, "row", false);
    json_write_int(w, engine->current_row);

    json_write_key(w, "tick", false);
    json_write_int(w, (int64_t)engine->current_tick);

    json_write_key(w, "time_ms", false);
    json_write_double(w, engine->current_time_ms);

    json_write_key(w, "bpm", false);
    json_write_int(w, engine->bpm);

    json_write_key(w, "loop_enabled", false);
    json_write_bool(w, engine->loop_enabled);

    json_write_key(w, "loop_start_row", false);
    json_write_int(w, engine->loop_start_row);

    json_write_key(w, "loop_end_row", false);
    json_write_int(w, engine->loop_end_row);

    json_write_key(w, "loop_count", false);
    json_write_int(w, engine->loop_count);

    json_write_key(w, "pending_count", false);
    json_write_int(w, engine->pending_count);

    json_write_key(w, "active_note_count", false);
    json_write_int(w, engine->active_note_count);

    json_end_object(w);
}

/*============================================================================
 * Incremental Update Serialization
 *============================================================================*/

void tracker_json_write_update(TrackerJsonWriter* w, const TrackerUpdate* update,
                               const TrackerView* view) {
    if (!w || !update) {
        json_write_null(w);
        return;
    }

    json_begin_object(w);

    json_write_key(w, "type", true);
    switch (update->type) {
        case TRACKER_UPDATE_CELL:      json_write_string(w, "cell");      break;
        case TRACKER_UPDATE_ROW:       json_write_string(w, "row");       break;
        case TRACKER_UPDATE_TRACK:     json_write_string(w, "track");     break;
        case TRACKER_UPDATE_CURSOR:    json_write_string(w, "cursor");    break;
        case TRACKER_UPDATE_SELECTION: json_write_string(w, "selection"); break;
        case TRACKER_UPDATE_PLAYBACK:  json_write_string(w, "playback");  break;
        case TRACKER_UPDATE_TRANSPORT: json_write_string(w, "transport"); break;
        case TRACKER_UPDATE_PATTERN:   json_write_string(w, "pattern");   break;
        case TRACKER_UPDATE_SONG:      json_write_string(w, "song");      break;
        default:                       json_write_string(w, "unknown");   break;
    }

    json_write_key(w, "pattern", false);
    json_write_int(w, update->pattern);

    json_write_key(w, "track", false);
    json_write_int(w, update->track);

    json_write_key(w, "row", false);
    json_write_int(w, update->row);

    /* Include relevant data based on update type */
    if (view && view->song) {
        switch (update->type) {
            case TRACKER_UPDATE_CELL:
                if (update->pattern >= 0 && update->pattern < view->song->num_patterns) {
                    TrackerPattern* pattern = view->song->patterns[update->pattern];
                    TrackerCell* cell = tracker_pattern_get_cell(pattern, update->row, update->track);
                    if (cell) {
                        json_write_key(w, "cell", false);
                        tracker_json_write_cell(w, cell);
                    }
                }
                break;

            case TRACKER_UPDATE_CURSOR:
                json_write_key(w, "cursor_pattern", false);
                json_write_int(w, view->state.cursor_pattern);
                json_write_key(w, "cursor_track", false);
                json_write_int(w, view->state.cursor_track);
                json_write_key(w, "cursor_row", false);
                json_write_int(w, view->state.cursor_row);
                break;

            case TRACKER_UPDATE_SELECTION:
                json_write_key(w, "selection", false);
                tracker_json_write_selection(w, &view->state.selection);
                break;

            case TRACKER_UPDATE_PLAYBACK:
                json_write_key(w, "playback_pattern", false);
                json_write_int(w, view->state.playback_pattern);
                json_write_key(w, "playback_row", false);
                json_write_int(w, view->state.playback_row);
                break;

            case TRACKER_UPDATE_TRANSPORT:
                if (view->engine) {
                    json_write_key(w, "engine", false);
                    tracker_json_write_playback_state(w, view->engine);
                }
                break;

            default:
                break;
        }
    }

    json_end_object(w);
}

/*============================================================================
 * String Buffer Writer
 *============================================================================*/

typedef struct {
    char* buffer;
    int length;
    int capacity;
} StringBuffer;

static void string_buffer_write(void* user_data, const char* json, int len) {
    StringBuffer* sb = (StringBuffer*)user_data;

    /* Grow buffer if needed */
    while (sb->length + len + 1 > sb->capacity) {
        int new_cap = sb->capacity * 2;
        if (new_cap < 256) new_cap = 256;
        char* new_buf = realloc(sb->buffer, new_cap);
        if (!new_buf) return;
        sb->buffer = new_buf;
        sb->capacity = new_cap;
    }

    memcpy(sb->buffer + sb->length, json, len);
    sb->length += len;
    sb->buffer[sb->length] = '\0';
}

/*============================================================================
 * String Output Helpers
 *============================================================================*/

char* tracker_json_song_to_string(const TrackerSong* song, bool pretty) {
    if (!song) return NULL;

    StringBuffer sb = { NULL, 0, 0 };
    TrackerJsonWriter w;
    tracker_json_writer_init(&w, string_buffer_write, &sb, pretty);

    tracker_json_write_song(&w, song);

    return sb.buffer;
}

char* tracker_json_view_state_to_string(const TrackerViewState* state, bool pretty) {
    if (!state) return NULL;

    StringBuffer sb = { NULL, 0, 0 };
    TrackerJsonWriter w;
    tracker_json_writer_init(&w, string_buffer_write, &sb, pretty);

    tracker_json_write_view_state(&w, state);

    return sb.buffer;
}

char* tracker_json_theme_to_string(const TrackerTheme* theme, bool pretty) {
    if (!theme) return NULL;

    StringBuffer sb = { NULL, 0, 0 };
    TrackerJsonWriter w;
    tracker_json_writer_init(&w, string_buffer_write, &sb, pretty);

    tracker_json_write_theme(&w, theme);

    return sb.buffer;
}

/*============================================================================
 * Deserialization Stubs
 * (Full implementation would require a JSON parser - using stubs for now)
 *============================================================================*/

TrackerSong* tracker_json_parse_song(const char* json, int len, const char** error_msg) {
    (void)json;
    (void)len;
    if (error_msg) {
        *error_msg = "JSON parsing not yet implemented";
    }
    return NULL;
}

TrackerPattern* tracker_json_parse_pattern(const char* json, int len, const char** error_msg) {
    (void)json;
    (void)len;
    if (error_msg) {
        *error_msg = "JSON parsing not yet implemented";
    }
    return NULL;
}

bool tracker_json_parse_view_state(TrackerViewState* state, const char* json,
                                   int len, const char** error_msg) {
    (void)state;
    (void)json;
    (void)len;
    if (error_msg) {
        *error_msg = "JSON parsing not yet implemented";
    }
    return false;
}

TrackerTheme* tracker_json_parse_theme(const char* json, int len, const char** error_msg) {
    (void)json;
    (void)len;
    if (error_msg) {
        *error_msg = "JSON parsing not yet implemented";
    }
    return NULL;
}

bool tracker_json_apply_update(TrackerView* view, const char* json, int len,
                               const char** error_msg) {
    (void)view;
    (void)json;
    (void)len;
    if (error_msg) {
        *error_msg = "JSON parsing not yet implemented";
    }
    return false;
}
