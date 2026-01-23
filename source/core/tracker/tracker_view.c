/**
 * tracker_view.c - Core view lifecycle, rendering, input handling
 */

#include "tracker_view.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/*============================================================================
 * Internal Helpers
 *============================================================================*/

static char* str_dup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* copy = malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}

/*============================================================================
 * View State
 *============================================================================*/

void tracker_view_state_init(TrackerViewState* state) {
    if (!state) return;
    memset(state, 0, sizeof(TrackerViewState));

    state->view_mode = TRACKER_VIEW_MODE_PATTERN;
    state->edit_mode = TRACKER_EDIT_MODE_NAVIGATE;
    state->selection.type = TRACKER_SEL_NONE;
    state->follow_playback = true;
    state->show_row_numbers = true;
    state->show_track_headers = true;
    state->show_transport = true;
    state->show_status_line = true;
    state->highlight_current_row = true;
    state->highlight_beat_rows = true;
    state->beat_highlight_interval = 4;
    state->visible_tracks = 8;
    state->visible_rows = 32;
}

void tracker_view_state_cleanup(TrackerViewState* state) {
    if (!state) return;

    free(state->edit_buffer);
    free(state->command_buffer);
    free(state->error_message);
    free(state->status_message);
    free(state->track_widths);

    if (state->owns_theme && state->theme) {
        tracker_theme_free(state->theme);
    }

    memset(state, 0, sizeof(TrackerViewState));
}

/*============================================================================
 * Lifecycle Functions
 *============================================================================*/

TrackerView* tracker_view_new(const TrackerViewCallbacks* callbacks) {
    TrackerView* view = calloc(1, sizeof(TrackerView));
    if (!view) return NULL;

    if (callbacks) {
        view->callbacks = *callbacks;
    }

    tracker_view_state_init(&view->state);
    tracker_undo_init(&view->undo, 100);  /* 100 undo levels */

    /* Set default theme */
    view->state.theme = (TrackerTheme*)tracker_theme_get("default");
    view->state.owns_theme = false;

    /* Initialize backend if callback provided */
    if (view->callbacks.init) {
        if (!view->callbacks.init(view)) {
            tracker_view_free(view);
            return NULL;
        }
    }

    return view;
}

void tracker_view_free(TrackerView* view) {
    if (!view) return;

    /* Cleanup backend */
    if (view->callbacks.cleanup) {
        view->callbacks.cleanup(view);
    }

    tracker_view_state_cleanup(&view->state);
    tracker_view_clipboard_clear(view);
    tracker_undo_cleanup(&view->undo);

    free(view);
}

void tracker_view_attach(TrackerView* view, TrackerSong* song, TrackerEngine* engine) {
    if (!view) return;

    view->song = song;
    view->engine = engine;

    /* Reset cursor to valid position */
    view->state.cursor_pattern = 0;
    view->state.cursor_track = 0;
    view->state.cursor_row = 0;
    view->state.scroll_track = 0;
    view->state.scroll_row = 0;

    tracker_view_select_clear(view);
    tracker_view_invalidate(view);
}

void tracker_view_detach(TrackerView* view) {
    if (!view) return;

    view->song = NULL;
    view->engine = NULL;

    tracker_view_invalidate(view);
}

/*============================================================================
 * Theme Management
 *============================================================================*/

void tracker_view_set_theme(TrackerView* view, TrackerTheme* theme, bool owns) {
    if (!view) return;

    /* Free old theme if owned */
    if (view->state.owns_theme && view->state.theme) {
        tracker_theme_free(view->state.theme);
    }

    view->state.theme = theme;
    view->state.owns_theme = owns;

    tracker_view_invalidate(view);
}

bool tracker_view_set_theme_by_name(TrackerView* view, const char* name) {
    if (!view) return false;

    const TrackerTheme* theme = tracker_theme_get(name);
    if (!theme) return false;

    /* Free old theme if owned */
    if (view->state.owns_theme && view->state.theme) {
        tracker_theme_free(view->state.theme);
    }

    view->state.theme = (TrackerTheme*)theme;
    view->state.owns_theme = false;

    tracker_view_invalidate(view);
    return true;
}

const TrackerTheme* tracker_view_get_theme(TrackerView* view) {
    if (!view) return NULL;
    return view->state.theme;
}

/*============================================================================
 * Rendering
 *============================================================================*/

void tracker_view_invalidate(TrackerView* view) {
    if (!view) return;
    view->dirty_flags = TRACKER_DIRTY_ALL;
}

void tracker_view_invalidate_cell(TrackerView* view, int track, int row) {
    if (!view) return;
    view->dirty_flags |= TRACKER_DIRTY_CELL;
    view->dirty_cell_track = track;
    view->dirty_cell_row = row;
}

void tracker_view_invalidate_row(TrackerView* view, int row) {
    if (!view) return;
    view->dirty_flags |= TRACKER_DIRTY_ROW;
    view->dirty_row = row;
}

void tracker_view_invalidate_track(TrackerView* view, int track) {
    if (!view) return;
    view->dirty_flags |= TRACKER_DIRTY_TRACK;
    view->dirty_track = track;
}

void tracker_view_invalidate_cursor(TrackerView* view) {
    if (!view) return;
    view->dirty_flags |= TRACKER_DIRTY_CURSOR;
}

void tracker_view_invalidate_selection(TrackerView* view) {
    if (!view) return;
    view->dirty_flags |= TRACKER_DIRTY_SELECTION;
}

void tracker_view_invalidate_status(TrackerView* view) {
    if (!view) return;
    view->dirty_flags |= TRACKER_DIRTY_STATUS;
}

void tracker_view_render(TrackerView* view) {
    if (!view) return;

    if (view->dirty_flags == TRACKER_DIRTY_NONE) return;

    if (view->callbacks.render_incremental &&
        view->dirty_flags != TRACKER_DIRTY_ALL) {
        view->callbacks.render_incremental(view, view->dirty_flags);
    } else if (view->callbacks.render) {
        view->callbacks.render(view);
    }

    view->dirty_flags = TRACKER_DIRTY_NONE;
}

void tracker_view_update_playback(TrackerView* view, int pattern, int row) {
    if (!view) return;

    bool changed = (view->state.playback_pattern != pattern ||
                    view->state.playback_row != row);

    view->state.playback_pattern = pattern;
    view->state.playback_row = row;

    if (changed) {
        view->dirty_flags |= TRACKER_DIRTY_PLAYBACK;

        /* Auto-scroll if following playback */
        if (view->state.follow_playback && view->engine &&
            tracker_engine_is_playing(view->engine)) {
            if (pattern == view->state.cursor_pattern) {
                tracker_view_scroll_to_row(view, row);
            }
        }
    }
}

/*============================================================================
 * Input Handling
 *============================================================================*/

bool tracker_view_poll_input(TrackerView* view, int timeout_ms) {
    if (!view || !view->callbacks.poll_input) return false;

    TrackerInputEvent event;
    if (view->callbacks.poll_input(view, timeout_ms, &event)) {
        return tracker_view_handle_input(view, &event);
    }

    return false;
}

bool tracker_view_handle_input(TrackerView* view, const TrackerInputEvent* event) {
    if (!view || !event) return false;

    bool handled = true;
    bool shift = (event->modifiers & TRACKER_MOD_SHIFT) != 0;

    /* Handle edit mode input */
    if (view->state.edit_mode == TRACKER_EDIT_MODE_EDIT) {
        switch (event->type) {
            case TRACKER_INPUT_CHAR:
                tracker_view_edit_char(view, event->character);
                break;
            case TRACKER_INPUT_BACKSPACE:
                /* Delete char before cursor */
                if (view->state.edit_cursor_pos > 0 && view->state.edit_buffer) {
                    int len = view->state.edit_buffer_len;
                    int pos = view->state.edit_cursor_pos;
                    memmove(&view->state.edit_buffer[pos - 1],
                            &view->state.edit_buffer[pos], len - pos + 1);
                    view->state.edit_buffer_len--;
                    view->state.edit_cursor_pos--;
                }
                break;
            case TRACKER_INPUT_DELETE:
                /* Delete char at cursor */
                if (view->state.edit_buffer &&
                    view->state.edit_cursor_pos < view->state.edit_buffer_len) {
                    int len = view->state.edit_buffer_len;
                    int pos = view->state.edit_cursor_pos;
                    memmove(&view->state.edit_buffer[pos],
                            &view->state.edit_buffer[pos + 1], len - pos);
                    view->state.edit_buffer_len--;
                }
                break;
            case TRACKER_INPUT_EXIT_EDIT:
                tracker_view_edit_confirm(view);
                break;
            case TRACKER_INPUT_CANCEL:
                tracker_view_edit_cancel(view);
                break;
            case TRACKER_INPUT_CURSOR_LEFT:
                if (view->state.edit_cursor_pos > 0) {
                    view->state.edit_cursor_pos--;
                }
                break;
            case TRACKER_INPUT_CURSOR_RIGHT:
                if (view->state.edit_cursor_pos < view->state.edit_buffer_len) {
                    view->state.edit_cursor_pos++;
                }
                break;
            default:
                handled = false;
                break;
        }
        return handled;
    }

    /* Handle command mode input */
    if (view->state.edit_mode == TRACKER_EDIT_MODE_COMMAND) {
        switch (event->type) {
            case TRACKER_INPUT_CHAR:
                /* Add char to command buffer */
                /* TODO: implement command buffer editing */
                break;
            case TRACKER_INPUT_EXIT_EDIT:
                tracker_view_exit_command(view, true);
                break;
            case TRACKER_INPUT_CANCEL:
                tracker_view_exit_command(view, false);
                break;
            default:
                handled = false;
                break;
        }
        return handled;
    }

    /* Normal mode input handling */
    switch (event->type) {
        /* Navigation */
        case TRACKER_INPUT_CURSOR_UP:
            if (shift && !view->state.selecting) tracker_view_select_start(view);
            tracker_view_cursor_up(view, event->repeat_count ? event->repeat_count : 1);
            if (shift) tracker_view_select_extend(view);
            else if (!shift && view->state.selecting) tracker_view_select_clear(view);
            break;
        case TRACKER_INPUT_CURSOR_DOWN:
            if (shift && !view->state.selecting) tracker_view_select_start(view);
            tracker_view_cursor_down(view, event->repeat_count ? event->repeat_count : 1);
            if (shift) tracker_view_select_extend(view);
            else if (!shift && view->state.selecting) tracker_view_select_clear(view);
            break;
        case TRACKER_INPUT_CURSOR_LEFT:
            if (shift && !view->state.selecting) tracker_view_select_start(view);
            tracker_view_cursor_left(view, event->repeat_count ? event->repeat_count : 1);
            if (shift) tracker_view_select_extend(view);
            else if (!shift && view->state.selecting) tracker_view_select_clear(view);
            break;
        case TRACKER_INPUT_CURSOR_RIGHT:
            if (shift && !view->state.selecting) tracker_view_select_start(view);
            tracker_view_cursor_right(view, event->repeat_count ? event->repeat_count : 1);
            if (shift) tracker_view_select_extend(view);
            else if (!shift && view->state.selecting) tracker_view_select_clear(view);
            break;
        case TRACKER_INPUT_PAGE_UP:
            tracker_view_cursor_page_up(view);
            break;
        case TRACKER_INPUT_PAGE_DOWN:
            tracker_view_cursor_page_down(view);
            break;
        case TRACKER_INPUT_HOME:
            tracker_view_cursor_home(view);
            break;
        case TRACKER_INPUT_END:
            tracker_view_cursor_end(view);
            break;
        case TRACKER_INPUT_PATTERN_START:
            tracker_view_cursor_pattern_start(view);
            break;
        case TRACKER_INPUT_PATTERN_END:
            tracker_view_cursor_pattern_end(view);
            break;
        case TRACKER_INPUT_NEXT_PATTERN:
            if (view->engine) tracker_engine_next_pattern(view->engine);
            break;
        case TRACKER_INPUT_PREV_PATTERN:
            if (view->engine) tracker_engine_prev_pattern(view->engine);
            break;

        /* Editing */
        case TRACKER_INPUT_ENTER_EDIT:
            tracker_view_enter_edit(view);
            break;
        case TRACKER_INPUT_CLEAR_CELL:
            tracker_view_clear_cell(view);
            break;
        case TRACKER_INPUT_DELETE:
            tracker_view_clear_selection(view);
            break;
        case TRACKER_INPUT_INSERT_ROW:
            tracker_view_insert_row(view);
            break;
        case TRACKER_INPUT_DELETE_ROW:
            tracker_view_delete_row(view);
            break;
        case TRACKER_INPUT_DUPLICATE_ROW:
            tracker_view_duplicate_row(view);
            break;

        /* Selection */
        case TRACKER_INPUT_SELECT_START:
            tracker_view_select_start(view);
            break;
        case TRACKER_INPUT_SELECT_ALL:
            tracker_view_select_all(view);
            break;
        case TRACKER_INPUT_SELECT_TRACK:
            tracker_view_select_track(view);
            break;
        case TRACKER_INPUT_SELECT_ROW:
            tracker_view_select_row(view);
            break;
        case TRACKER_INPUT_SELECT_PATTERN:
            tracker_view_select_pattern(view);
            break;

        /* Clipboard */
        case TRACKER_INPUT_CUT:
            tracker_view_cut(view);
            break;
        case TRACKER_INPUT_COPY:
            tracker_view_copy(view);
            break;
        case TRACKER_INPUT_PASTE:
            tracker_view_paste(view);
            break;
        case TRACKER_INPUT_PASTE_INSERT:
            tracker_view_paste_insert(view);
            break;

        /* Transport */
        case TRACKER_INPUT_PLAY:
        case TRACKER_INPUT_PLAY_TOGGLE:
            if (view->engine) tracker_engine_toggle(view->engine);
            break;
        case TRACKER_INPUT_STOP:
            if (view->engine) tracker_engine_stop(view->engine);
            break;
        case TRACKER_INPUT_PAUSE:
            if (view->engine) tracker_engine_pause(view->engine);
            break;
        case TRACKER_INPUT_PLAY_FROM_START:
            if (view->engine) {
                tracker_engine_seek(view->engine, view->state.cursor_pattern, 0);
                tracker_engine_play(view->engine);
            }
            break;
        case TRACKER_INPUT_PLAY_FROM_CURSOR:
            if (view->engine) {
                tracker_engine_seek(view->engine, view->state.cursor_pattern,
                                    view->state.cursor_row);
                tracker_engine_play(view->engine);
            }
            break;
        case TRACKER_INPUT_PLAY_ROW:
            if (view->engine) {
                tracker_engine_trigger_cell(view->engine, view->state.cursor_pattern,
                    view->state.cursor_track, view->state.cursor_row);
            }
            break;

        /* Track control */
        case TRACKER_INPUT_MUTE_TRACK:
            if (view->engine && view->song) {
                TrackerPattern* p = tracker_view_get_current_pattern(view);
                if (p && view->state.cursor_track < p->num_tracks) {
                    bool muted = p->tracks[view->state.cursor_track].muted;
                    tracker_engine_mute_track(view->engine, view->state.cursor_track, !muted);
                    tracker_view_invalidate(view);
                }
            }
            break;
        case TRACKER_INPUT_SOLO_TRACK:
            if (view->engine && view->song) {
                TrackerPattern* p = tracker_view_get_current_pattern(view);
                if (p && view->state.cursor_track < p->num_tracks) {
                    bool solo = p->tracks[view->state.cursor_track].solo;
                    tracker_engine_solo_track(view->engine, view->state.cursor_track, !solo);
                    tracker_view_invalidate(view);
                }
            }
            break;

        /* View modes */
        case TRACKER_INPUT_MODE_PATTERN:
            tracker_view_set_mode(view, TRACKER_VIEW_MODE_PATTERN);
            break;
        case TRACKER_INPUT_MODE_ARRANGE:
            tracker_view_set_mode(view, TRACKER_VIEW_MODE_ARRANGE);
            break;
        case TRACKER_INPUT_MODE_MIXER:
            tracker_view_set_mode(view, TRACKER_VIEW_MODE_MIXER);
            break;
        case TRACKER_INPUT_MODE_HELP:
            tracker_view_set_mode(view, TRACKER_VIEW_MODE_HELP);
            break;
        case TRACKER_INPUT_FOLLOW_TOGGLE:
            view->state.follow_playback = !view->state.follow_playback;
            break;

        /* Undo/Redo */
        case TRACKER_INPUT_UNDO:
            tracker_view_undo(view);
            break;
        case TRACKER_INPUT_REDO:
            tracker_view_redo(view);
            break;

        /* Misc */
        case TRACKER_INPUT_COMMAND_MODE:
            tracker_view_enter_command(view);
            break;
        case TRACKER_INPUT_QUIT:
            tracker_view_request_quit(view);
            break;
        case TRACKER_INPUT_PANIC:
            if (view->engine) tracker_engine_all_notes_off(view->engine);
            break;

        default:
            handled = false;
            break;
    }

    return handled;
}

void tracker_view_edit_char(TrackerView* view, uint32_t character) {
    if (!view) return;
    if (view->state.edit_mode != TRACKER_EDIT_MODE_EDIT) return;

    /* Ensure buffer exists */
    if (!view->state.edit_buffer) {
        view->state.edit_buffer_capacity = 256;
        view->state.edit_buffer = calloc(view->state.edit_buffer_capacity, 1);
        if (!view->state.edit_buffer) return;
    }

    /* Grow buffer if needed */
    if (view->state.edit_buffer_len + 4 >= view->state.edit_buffer_capacity) {
        int new_cap = view->state.edit_buffer_capacity * 2;
        char* new_buf = realloc(view->state.edit_buffer, new_cap);
        if (!new_buf) return;
        view->state.edit_buffer = new_buf;
        view->state.edit_buffer_capacity = new_cap;
    }

    /* Insert character at cursor (simplified: ASCII only for now) */
    if (character < 128) {
        int pos = view->state.edit_cursor_pos;
        int len = view->state.edit_buffer_len;

        memmove(&view->state.edit_buffer[pos + 1],
                &view->state.edit_buffer[pos], len - pos + 1);

        view->state.edit_buffer[pos] = (char)character;
        view->state.edit_buffer_len++;
        view->state.edit_cursor_pos++;
    }

    tracker_view_invalidate_cursor(view);
}

void tracker_view_edit_confirm(TrackerView* view) {
    if (!view || !view->song) return;
    if (view->state.edit_mode != TRACKER_EDIT_MODE_EDIT) return;

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    if (!pattern) return;

    TrackerCell* cell = tracker_pattern_get_cell(pattern,
        view->state.cursor_row, view->state.cursor_track);
    if (!cell) return;

    /* Record undo */
    TrackerCell old_cell;
    tracker_cell_clone(&old_cell, cell);

    /* Update cell */
    tracker_cell_set_expression(cell, view->state.edit_buffer,
                                cell->language_id);

    /* Record undo */
    tracker_undo_record_cell_edit(&view->undo, view,
        view->state.cursor_pattern, view->state.cursor_track, view->state.cursor_row,
        &old_cell, cell);

    tracker_cell_clear(&old_cell);

    /* Exit edit mode */
    view->state.edit_mode = TRACKER_EDIT_MODE_NAVIGATE;
    tracker_view_invalidate(view);
}

void tracker_view_edit_cancel(TrackerView* view) {
    if (!view) return;

    view->state.edit_mode = TRACKER_EDIT_MODE_NAVIGATE;
    tracker_view_invalidate_cursor(view);
}

/*============================================================================
 * Cursor Movement
 *============================================================================*/

void tracker_view_cursor_up(TrackerView* view, int count) {
    if (!view) return;

    view->state.cursor_row -= count;
    tracker_view_clamp_cursor(view);
    tracker_view_ensure_visible(view);
    tracker_view_invalidate_cursor(view);
}

void tracker_view_cursor_down(TrackerView* view, int count) {
    if (!view) return;

    view->state.cursor_row += count;
    tracker_view_clamp_cursor(view);
    tracker_view_ensure_visible(view);
    tracker_view_invalidate_cursor(view);
}

void tracker_view_cursor_left(TrackerView* view, int count) {
    if (!view) return;

    view->state.cursor_track -= count;
    tracker_view_clamp_cursor(view);
    tracker_view_ensure_visible(view);
    tracker_view_invalidate_cursor(view);
}

void tracker_view_cursor_right(TrackerView* view, int count) {
    if (!view) return;

    view->state.cursor_track += count;
    tracker_view_clamp_cursor(view);
    tracker_view_ensure_visible(view);
    tracker_view_invalidate_cursor(view);
}

void tracker_view_cursor_page_up(TrackerView* view) {
    if (!view) return;
    tracker_view_cursor_up(view, view->state.visible_rows);
}

void tracker_view_cursor_page_down(TrackerView* view) {
    if (!view) return;
    tracker_view_cursor_down(view, view->state.visible_rows);
}

void tracker_view_cursor_home(TrackerView* view) {
    if (!view) return;
    view->state.cursor_track = 0;
    tracker_view_ensure_visible(view);
    tracker_view_invalidate_cursor(view);
}

void tracker_view_cursor_end(TrackerView* view) {
    if (!view || !view->song) return;

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    if (pattern) {
        view->state.cursor_track = pattern->num_tracks - 1;
    }
    tracker_view_ensure_visible(view);
    tracker_view_invalidate_cursor(view);
}

void tracker_view_cursor_pattern_start(TrackerView* view) {
    if (!view) return;
    view->state.cursor_row = 0;
    tracker_view_ensure_visible(view);
    tracker_view_invalidate_cursor(view);
}

void tracker_view_cursor_pattern_end(TrackerView* view) {
    if (!view || !view->song) return;

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    if (pattern) {
        view->state.cursor_row = pattern->num_rows - 1;
    }
    tracker_view_ensure_visible(view);
    tracker_view_invalidate_cursor(view);
}

void tracker_view_cursor_goto(TrackerView* view, int pattern, int track, int row) {
    if (!view) return;

    view->state.cursor_pattern = pattern;
    view->state.cursor_track = track;
    view->state.cursor_row = row;

    tracker_view_clamp_cursor(view);
    tracker_view_ensure_visible(view);
    tracker_view_invalidate_cursor(view);
}

void tracker_view_ensure_visible(TrackerView* view) {
    if (!view) return;

    /* Adjust scroll to keep cursor visible */
    if (view->state.cursor_row < view->state.scroll_row) {
        view->state.scroll_row = view->state.cursor_row;
        view->dirty_flags |= TRACKER_DIRTY_SCROLL;
    } else if (view->state.cursor_row >= view->state.scroll_row + view->state.visible_rows) {
        view->state.scroll_row = view->state.cursor_row - view->state.visible_rows + 1;
        view->dirty_flags |= TRACKER_DIRTY_SCROLL;
    }

    if (view->state.cursor_track < view->state.scroll_track) {
        view->state.scroll_track = view->state.cursor_track;
        view->dirty_flags |= TRACKER_DIRTY_SCROLL;
    } else if (view->state.cursor_track >= view->state.scroll_track + view->state.visible_tracks) {
        view->state.scroll_track = view->state.cursor_track - view->state.visible_tracks + 1;
        view->dirty_flags |= TRACKER_DIRTY_SCROLL;
    }
}

/*============================================================================
 * Mode Switching
 *============================================================================*/

void tracker_view_set_mode(TrackerView* view, TrackerViewMode mode) {
    if (!view) return;

    if (view->state.view_mode != mode) {
        view->state.view_mode = mode;
        tracker_view_invalidate(view);
    }
}

void tracker_view_enter_edit(TrackerView* view) {
    if (!view || !view->song) return;

    TrackerCell* cell = tracker_view_get_cursor_cell(view);
    if (!cell) return;

    /* Initialize edit buffer with current cell content */
    free(view->state.edit_buffer);
    view->state.edit_buffer = str_dup(cell->expression ? cell->expression : "");
    view->state.edit_buffer_len = view->state.edit_buffer ?
        (int)strlen(view->state.edit_buffer) : 0;
    view->state.edit_buffer_capacity = view->state.edit_buffer_len + 256;
    view->state.edit_cursor_pos = view->state.edit_buffer_len;

    view->state.edit_mode = TRACKER_EDIT_MODE_EDIT;
    tracker_view_invalidate_cursor(view);
}

void tracker_view_exit_edit(TrackerView* view, bool confirm) {
    if (!view) return;

    if (confirm) {
        tracker_view_edit_confirm(view);
    } else {
        tracker_view_edit_cancel(view);
    }
}

void tracker_view_enter_command(TrackerView* view) {
    if (!view) return;

    free(view->state.command_buffer);
    view->state.command_buffer = calloc(256, 1);
    view->state.command_buffer_len = 0;
    view->state.command_buffer_capacity = 256;
    view->state.command_cursor_pos = 0;

    view->state.edit_mode = TRACKER_EDIT_MODE_COMMAND;
    tracker_view_invalidate_status(view);
}

void tracker_view_exit_command(TrackerView* view, bool execute) {
    if (!view) return;

    if (execute && view->state.command_buffer) {
        /* TODO: implement command execution */
    }

    view->state.edit_mode = TRACKER_EDIT_MODE_NAVIGATE;
    tracker_view_invalidate_status(view);
}

/*============================================================================
 * Scroll Control
 *============================================================================*/

void tracker_view_scroll_to_row(TrackerView* view, int row) {
    if (!view) return;

    /* Center the row if possible */
    int target = row - view->state.visible_rows / 2;
    if (target < 0) target = 0;

    if (view->state.scroll_row != target) {
        view->state.scroll_row = target;
        view->dirty_flags |= TRACKER_DIRTY_SCROLL;
    }
}

void tracker_view_scroll_to_track(TrackerView* view, int track) {
    if (!view) return;

    int target = track - view->state.visible_tracks / 2;
    if (target < 0) target = 0;

    if (view->state.scroll_track != target) {
        view->state.scroll_track = target;
        view->dirty_flags |= TRACKER_DIRTY_SCROLL;
    }
}

void tracker_view_scroll(TrackerView* view, int track_delta, int row_delta) {
    if (!view) return;

    view->state.scroll_track += track_delta;
    view->state.scroll_row += row_delta;

    if (view->state.scroll_track < 0) view->state.scroll_track = 0;
    if (view->state.scroll_row < 0) view->state.scroll_row = 0;

    view->dirty_flags |= TRACKER_DIRTY_SCROLL;
}

void tracker_view_set_follow(TrackerView* view, bool follow) {
    if (!view) return;
    view->state.follow_playback = follow;
}

/*============================================================================
 * Undo/Redo (View Integration)
 *============================================================================*/

bool tracker_view_undo(TrackerView* view) {
    if (!view || !view->song) return false;
    bool result = tracker_undo_undo(&view->undo, view, view->song);
    if (result) tracker_view_invalidate(view);
    return result;
}

bool tracker_view_redo(TrackerView* view) {
    if (!view || !view->song) return false;
    bool result = tracker_undo_redo(&view->undo, view, view->song);
    if (result) tracker_view_invalidate(view);
    return result;
}

void tracker_view_begin_undo_group(TrackerView* view, const char* description) {
    if (!view) return;
    tracker_undo_group_begin(&view->undo, description);
}

void tracker_view_end_undo_group(TrackerView* view) {
    if (!view) return;
    tracker_undo_group_end(&view->undo);
}

/*============================================================================
 * Messages and Status
 *============================================================================*/

void tracker_view_show_status(TrackerView* view, const char* format, ...) {
    if (!view) return;

    va_list args;
    va_start(args, format);

    free(view->state.status_message);
    view->state.status_message = malloc(256);
    if (view->state.status_message) {
        vsnprintf(view->state.status_message, 256, format, args);
    }

    va_end(args);

    view->state.status_display_time = 3.0;  /* 3 seconds */
    tracker_view_invalidate_status(view);

    if (view->callbacks.show_message) {
        view->callbacks.show_message(view, view->state.status_message);
    }
}

void tracker_view_show_error(TrackerView* view, const char* format, ...) {
    if (!view) return;

    va_list args;
    va_start(args, format);

    free(view->state.error_message);
    view->state.error_message = malloc(256);
    if (view->state.error_message) {
        vsnprintf(view->state.error_message, 256, format, args);
    }

    va_end(args);

    view->state.error_display_time = 5.0;  /* 5 seconds */
    tracker_view_invalidate_status(view);

    if (view->callbacks.show_error) {
        view->callbacks.show_error(view, view->state.error_message);
    }
}

void tracker_view_clear_messages(TrackerView* view) {
    if (!view) return;

    free(view->state.status_message);
    free(view->state.error_message);
    view->state.status_message = NULL;
    view->state.error_message = NULL;
    view->state.status_display_time = 0;
    view->state.error_display_time = 0;

    tracker_view_invalidate_status(view);
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

TrackerCell* tracker_view_get_cursor_cell(TrackerView* view) {
    if (!view || !view->song) return NULL;

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    if (!pattern) return NULL;

    return tracker_pattern_get_cell(pattern,
        view->state.cursor_row, view->state.cursor_track);
}

TrackerPattern* tracker_view_get_current_pattern(TrackerView* view) {
    if (!view || !view->song) return NULL;
    return tracker_song_get_pattern(view->song, view->state.cursor_pattern);
}

bool tracker_view_cursor_valid(TrackerView* view) {
    if (!view || !view->song) return false;

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    if (!pattern) return false;

    return view->state.cursor_track >= 0 &&
           view->state.cursor_track < pattern->num_tracks &&
           view->state.cursor_row >= 0 &&
           view->state.cursor_row < pattern->num_rows;
}

void tracker_view_clamp_cursor(TrackerView* view) {
    if (!view || !view->song) return;

    /* Clamp pattern */
    if (view->state.cursor_pattern < 0) {
        view->state.cursor_pattern = 0;
    }
    if (view->state.cursor_pattern >= view->song->num_patterns) {
        view->state.cursor_pattern = view->song->num_patterns - 1;
    }
    if (view->state.cursor_pattern < 0) {
        view->state.cursor_pattern = 0;
    }

    TrackerPattern* pattern = tracker_view_get_current_pattern(view);
    if (!pattern) return;

    /* Clamp track */
    if (view->state.cursor_track < 0) {
        view->state.cursor_track = 0;
    }
    if (view->state.cursor_track >= pattern->num_tracks) {
        view->state.cursor_track = pattern->num_tracks - 1;
    }

    /* Clamp row */
    if (view->state.cursor_row < 0) {
        view->state.cursor_row = 0;
    }
    if (view->state.cursor_row >= pattern->num_rows) {
        view->state.cursor_row = pattern->num_rows - 1;
    }
}

void tracker_view_get_visible_range(TrackerView* view,
                                    int* out_start_track, int* out_end_track,
                                    int* out_start_row, int* out_end_row) {
    if (!view) return;

    if (out_start_track) *out_start_track = view->state.scroll_track;
    if (out_end_track) *out_end_track = view->state.scroll_track + view->state.visible_tracks - 1;
    if (out_start_row) *out_start_row = view->state.scroll_row;
    if (out_end_row) *out_end_row = view->state.scroll_row + view->state.visible_rows - 1;
}

/*============================================================================
 * Main Loop
 *============================================================================*/

void tracker_view_run(TrackerView* view, int frame_rate) {
    if (!view) return;

    double frame_ms = 1000.0 / frame_rate;
    view->quit_requested = false;

    while (!view->quit_requested) {
        /* Poll input with timeout */
        tracker_view_poll_input(view, (int)frame_ms);

        /* Update playback position if engine is running */
        if (view->engine && tracker_engine_is_playing(view->engine)) {
            int pattern, row, tick;
            tracker_engine_get_position(view->engine, &pattern, &row, &tick);
            tracker_view_update_playback(view, pattern, row);
            view->state.is_playing = true;
        } else {
            view->state.is_playing = false;
        }

        /* Render */
        tracker_view_render(view);

        /* Decay message timers */
        if (view->state.status_display_time > 0) {
            view->state.status_display_time -= frame_ms / 1000.0;
            if (view->state.status_display_time <= 0) {
                free(view->state.status_message);
                view->state.status_message = NULL;
                tracker_view_invalidate_status(view);
            }
        }
        if (view->state.error_display_time > 0) {
            view->state.error_display_time -= frame_ms / 1000.0;
            if (view->state.error_display_time <= 0) {
                free(view->state.error_message);
                view->state.error_message = NULL;
                tracker_view_invalidate_status(view);
            }
        }
    }
}

void tracker_view_request_quit(TrackerView* view) {
    if (!view) return;
    view->quit_requested = true;
}
