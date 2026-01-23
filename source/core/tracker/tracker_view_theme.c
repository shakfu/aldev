/**
 * tracker_view_theme.c - Theme management and built-in themes
 */

#include "tracker_view.h"
#include <stdlib.h>
#include <string.h>

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
 * Built-in Theme: Default (Dark)
 *============================================================================*/

static const TrackerTheme THEME_DEFAULT = {
    .name = "default",
    .author = "psnd",

    /* Base colors */
    .default_style = {
        .fg = { .type = TRACKER_COLOR_INDEXED, .value.index = 7 },   /* white */
        .bg = { .type = TRACKER_COLOR_INDEXED, .value.index = 0 },   /* black */
        .attr = TRACKER_ATTR_NONE
    },
    .header_style = {
        .fg = { .type = TRACKER_COLOR_INDEXED, .value.index = 6 },   /* cyan */
        .bg = { .type = TRACKER_COLOR_INDEXED, .value.index = 0 },
        .attr = TRACKER_ATTR_BOLD
    },
    .status_style = {
        .fg = { .type = TRACKER_COLOR_INDEXED, .value.index = 0 },   /* black */
        .bg = { .type = TRACKER_COLOR_INDEXED, .value.index = 7 },   /* white */
        .attr = TRACKER_ATTR_NONE
    },
    .command_style = {
        .fg = { .type = TRACKER_COLOR_INDEXED, .value.index = 7 },
        .bg = { .type = TRACKER_COLOR_INDEXED, .value.index = 0 },
        .attr = TRACKER_ATTR_NONE
    },
    .error_style = {
        .fg = { .type = TRACKER_COLOR_INDEXED, .value.index = 1 },   /* red */
        .bg = { .type = TRACKER_COLOR_INDEXED, .value.index = 0 },
        .attr = TRACKER_ATTR_BOLD
    },
    .message_style = {
        .fg = { .type = TRACKER_COLOR_INDEXED, .value.index = 2 },   /* green */
        .bg = { .type = TRACKER_COLOR_INDEXED, .value.index = 0 },
        .attr = TRACKER_ATTR_NONE
    },

    /* Grid colors */
    .cell_empty = {
        .fg = { .type = TRACKER_COLOR_INDEXED, .value.index = 8 },   /* dark gray */
        .bg = { .type = TRACKER_COLOR_INDEXED, .value.index = 0 },
        .attr = TRACKER_ATTR_DIM
    },
    .cell_note = {
        .fg = { .type = TRACKER_COLOR_INDEXED, .value.index = 3 },   /* yellow */
        .bg = { .type = TRACKER_COLOR_INDEXED, .value.index = 0 },
        .attr = TRACKER_ATTR_NONE
    },
    .cell_fx = {
        .fg = { .type = TRACKER_COLOR_INDEXED, .value.index = 5 },   /* magenta */
        .bg = { .type = TRACKER_COLOR_INDEXED, .value.index = 0 },
        .attr = TRACKER_ATTR_NONE
    },
    .cell_off = {
        .fg = { .type = TRACKER_COLOR_INDEXED, .value.index = 1 },   /* red */
        .bg = { .type = TRACKER_COLOR_INDEXED, .value.index = 0 },
        .attr = TRACKER_ATTR_DIM
    },
    .cell_continuation = {
        .fg = { .type = TRACKER_COLOR_INDEXED, .value.index = 8 },
        .bg = { .type = TRACKER_COLOR_INDEXED, .value.index = 0 },
        .attr = TRACKER_ATTR_DIM
    },

    /* Cursor and selection */
    .cursor = {
        .fg = { .type = TRACKER_COLOR_INDEXED, .value.index = 0 },
        .bg = { .type = TRACKER_COLOR_INDEXED, .value.index = 7 },
        .attr = TRACKER_ATTR_NONE
    },
    .cursor_edit = {
        .fg = { .type = TRACKER_COLOR_INDEXED, .value.index = 0 },
        .bg = { .type = TRACKER_COLOR_INDEXED, .value.index = 3 },   /* yellow bg */
        .attr = TRACKER_ATTR_NONE
    },
    .selection = {
        .fg = { .type = TRACKER_COLOR_INDEXED, .value.index = 7 },
        .bg = { .type = TRACKER_COLOR_INDEXED, .value.index = 4 },   /* blue bg */
        .attr = TRACKER_ATTR_NONE
    },
    .selection_cursor = {
        .fg = { .type = TRACKER_COLOR_INDEXED, .value.index = 0 },
        .bg = { .type = TRACKER_COLOR_INDEXED, .value.index = 6 },   /* cyan bg */
        .attr = TRACKER_ATTR_BOLD
    },

    /* Playback */
    .playing_row = {
        .fg = { .type = TRACKER_COLOR_INDEXED, .value.index = 7 },
        .bg = { .type = TRACKER_COLOR_INDEXED, .value.index = 2 },   /* green bg */
        .attr = TRACKER_ATTR_NONE
    },
    .playing_cell = {
        .fg = { .type = TRACKER_COLOR_INDEXED, .value.index = 0 },
        .bg = { .type = TRACKER_COLOR_INDEXED, .value.index = 2 },
        .attr = TRACKER_ATTR_BOLD
    },

    /* Row highlighting */
    .row_beat = {
        .fg = { .type = TRACKER_COLOR_DEFAULT },
        .bg = { .type = TRACKER_COLOR_INDEXED, .value.index = 8 },   /* dark gray */
        .attr = TRACKER_ATTR_NONE
    },
    .row_bar = {
        .fg = { .type = TRACKER_COLOR_DEFAULT },
        .bg = { .type = TRACKER_COLOR_INDEXED, .value.index = 8 },
        .attr = TRACKER_ATTR_BOLD
    },
    .row_alternate = {
        .fg = { .type = TRACKER_COLOR_DEFAULT },
        .bg = { .type = TRACKER_COLOR_DEFAULT },
        .attr = TRACKER_ATTR_NONE
    },

    /* Track states */
    .track_muted = {
        .fg = { .type = TRACKER_COLOR_INDEXED, .value.index = 8 },
        .bg = { .type = TRACKER_COLOR_INDEXED, .value.index = 0 },
        .attr = TRACKER_ATTR_DIM
    },
    .track_solo = {
        .fg = { .type = TRACKER_COLOR_INDEXED, .value.index = 3 },
        .bg = { .type = TRACKER_COLOR_INDEXED, .value.index = 0 },
        .attr = TRACKER_ATTR_BOLD
    },
    .track_active = {
        .fg = { .type = TRACKER_COLOR_INDEXED, .value.index = 2 },
        .bg = { .type = TRACKER_COLOR_INDEXED, .value.index = 0 },
        .attr = TRACKER_ATTR_NONE
    },

    /* Validation */
    .cell_error = {
        .fg = { .type = TRACKER_COLOR_INDEXED, .value.index = 7 },
        .bg = { .type = TRACKER_COLOR_INDEXED, .value.index = 1 },
        .attr = TRACKER_ATTR_NONE
    },
    .cell_warning = {
        .fg = { .type = TRACKER_COLOR_INDEXED, .value.index = 0 },
        .bg = { .type = TRACKER_COLOR_INDEXED, .value.index = 3 },
        .attr = TRACKER_ATTR_NONE
    },

    /* Active notes */
    .note_active = {
        .fg = { .type = TRACKER_COLOR_INDEXED, .value.index = 2 },
        .bg = { .type = TRACKER_COLOR_DEFAULT },
        .attr = TRACKER_ATTR_BOLD
    },
    .note_velocity = {
        { .fg = { .type = TRACKER_COLOR_INDEXED, .value.index = 8 }, .bg = { .type = TRACKER_COLOR_DEFAULT }, .attr = TRACKER_ATTR_NONE },
        { .fg = { .type = TRACKER_COLOR_INDEXED, .value.index = 7 }, .bg = { .type = TRACKER_COLOR_DEFAULT }, .attr = TRACKER_ATTR_NONE },
        { .fg = { .type = TRACKER_COLOR_INDEXED, .value.index = 3 }, .bg = { .type = TRACKER_COLOR_DEFAULT }, .attr = TRACKER_ATTR_NONE },
        { .fg = { .type = TRACKER_COLOR_INDEXED, .value.index = 1 }, .bg = { .type = TRACKER_COLOR_DEFAULT }, .attr = TRACKER_ATTR_BOLD },
    },

    /* Scrollbar */
    .scrollbar_track = {
        .fg = { .type = TRACKER_COLOR_INDEXED, .value.index = 8 },
        .bg = { .type = TRACKER_COLOR_INDEXED, .value.index = 0 },
        .attr = TRACKER_ATTR_NONE
    },
    .scrollbar_thumb = {
        .fg = { .type = TRACKER_COLOR_INDEXED, .value.index = 7 },
        .bg = { .type = TRACKER_COLOR_INDEXED, .value.index = 0 },
        .attr = TRACKER_ATTR_NONE
    },

    /* Borders */
    .border_color = { .type = TRACKER_COLOR_INDEXED, .value.index = 8 },
    .separator_color = { .type = TRACKER_COLOR_INDEXED, .value.index = 8 },

    /* Drawing characters (ASCII fallback) */
    .border_h = "-",
    .border_v = "|",
    .border_corner_tl = "+",
    .border_corner_tr = "+",
    .border_corner_bl = "+",
    .border_corner_br = "+",
    .border_t = "+",
    .border_b = "+",
    .border_l = "+",
    .border_r = "+",
    .border_cross = "+",
    .note_off_marker = "===",
    .continuation_marker = "...",
    .empty_cell = "---",
};

/*============================================================================
 * Built-in Theme: Retro (FastTracker-inspired)
 *============================================================================*/

static const TrackerTheme THEME_RETRO = {
    .name = "retro",
    .author = "psnd",

    .default_style = {
        .fg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 170, 170, 170 } },
        .bg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 0, 0, 85 } },
        .attr = TRACKER_ATTR_NONE
    },
    .header_style = {
        .fg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 255, 255, 85 } },
        .bg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 0, 0, 85 } },
        .attr = TRACKER_ATTR_BOLD
    },
    .status_style = {
        .fg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 0, 0, 0 } },
        .bg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 170, 170, 170 } },
        .attr = TRACKER_ATTR_NONE
    },
    .command_style = {
        .fg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 170, 170, 170 } },
        .bg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 0, 0, 85 } },
        .attr = TRACKER_ATTR_NONE
    },
    .error_style = {
        .fg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 255, 85, 85 } },
        .bg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 0, 0, 85 } },
        .attr = TRACKER_ATTR_BOLD
    },
    .message_style = {
        .fg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 85, 255, 85 } },
        .bg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 0, 0, 85 } },
        .attr = TRACKER_ATTR_NONE
    },

    .cell_empty = {
        .fg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 85, 85, 85 } },
        .bg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 0, 0, 85 } },
        .attr = TRACKER_ATTR_NONE
    },
    .cell_note = {
        .fg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 255, 255, 255 } },
        .bg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 0, 0, 85 } },
        .attr = TRACKER_ATTR_NONE
    },
    .cell_fx = {
        .fg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 85, 255, 255 } },
        .bg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 0, 0, 85 } },
        .attr = TRACKER_ATTR_NONE
    },
    .cell_off = {
        .fg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 255, 85, 85 } },
        .bg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 0, 0, 85 } },
        .attr = TRACKER_ATTR_NONE
    },
    .cell_continuation = {
        .fg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 85, 85, 85 } },
        .bg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 0, 0, 85 } },
        .attr = TRACKER_ATTR_NONE
    },

    .cursor = {
        .fg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 0, 0, 0 } },
        .bg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 170, 170, 170 } },
        .attr = TRACKER_ATTR_NONE
    },
    .cursor_edit = {
        .fg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 0, 0, 0 } },
        .bg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 255, 255, 85 } },
        .attr = TRACKER_ATTR_NONE
    },
    .selection = {
        .fg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 255, 255, 255 } },
        .bg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 85, 85, 170 } },
        .attr = TRACKER_ATTR_NONE
    },
    .selection_cursor = {
        .fg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 0, 0, 0 } },
        .bg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 85, 255, 255 } },
        .attr = TRACKER_ATTR_NONE
    },

    .playing_row = {
        .fg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 255, 255, 255 } },
        .bg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 0, 85, 0 } },
        .attr = TRACKER_ATTR_NONE
    },
    .playing_cell = {
        .fg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 255, 255, 255 } },
        .bg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 0, 170, 0 } },
        .attr = TRACKER_ATTR_BOLD
    },

    .row_beat = {
        .fg = { .type = TRACKER_COLOR_DEFAULT },
        .bg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 0, 0, 120 } },
        .attr = TRACKER_ATTR_NONE
    },
    .row_bar = {
        .fg = { .type = TRACKER_COLOR_DEFAULT },
        .bg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 0, 0, 140 } },
        .attr = TRACKER_ATTR_NONE
    },
    .row_alternate = {
        .fg = { .type = TRACKER_COLOR_DEFAULT },
        .bg = { .type = TRACKER_COLOR_DEFAULT },
        .attr = TRACKER_ATTR_NONE
    },

    .track_muted = {
        .fg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 85, 85, 85 } },
        .bg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 0, 0, 85 } },
        .attr = TRACKER_ATTR_DIM
    },
    .track_solo = {
        .fg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 255, 255, 85 } },
        .bg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 0, 0, 85 } },
        .attr = TRACKER_ATTR_BOLD
    },
    .track_active = {
        .fg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 85, 255, 85 } },
        .bg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 0, 0, 85 } },
        .attr = TRACKER_ATTR_NONE
    },

    .cell_error = {
        .fg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 255, 255, 255 } },
        .bg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 170, 0, 0 } },
        .attr = TRACKER_ATTR_NONE
    },
    .cell_warning = {
        .fg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 0, 0, 0 } },
        .bg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 255, 255, 85 } },
        .attr = TRACKER_ATTR_NONE
    },

    .note_active = {
        .fg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 85, 255, 85 } },
        .bg = { .type = TRACKER_COLOR_DEFAULT },
        .attr = TRACKER_ATTR_BOLD
    },
    .note_velocity = {
        { .fg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 85, 85, 85 } }, .bg = { .type = TRACKER_COLOR_DEFAULT }, .attr = TRACKER_ATTR_NONE },
        { .fg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 170, 170, 170 } }, .bg = { .type = TRACKER_COLOR_DEFAULT }, .attr = TRACKER_ATTR_NONE },
        { .fg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 255, 255, 85 } }, .bg = { .type = TRACKER_COLOR_DEFAULT }, .attr = TRACKER_ATTR_NONE },
        { .fg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 255, 85, 85 } }, .bg = { .type = TRACKER_COLOR_DEFAULT }, .attr = TRACKER_ATTR_BOLD },
    },

    .scrollbar_track = {
        .fg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 85, 85, 85 } },
        .bg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 0, 0, 85 } },
        .attr = TRACKER_ATTR_NONE
    },
    .scrollbar_thumb = {
        .fg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 170, 170, 170 } },
        .bg = { .type = TRACKER_COLOR_RGB, .value.rgb = { 0, 0, 85 } },
        .attr = TRACKER_ATTR_NONE
    },

    .border_color = { .type = TRACKER_COLOR_RGB, .value.rgb = { 85, 85, 170 } },
    .separator_color = { .type = TRACKER_COLOR_RGB, .value.rgb = { 85, 85, 170 } },

    .border_h = "-",
    .border_v = "|",
    .border_corner_tl = "+",
    .border_corner_tr = "+",
    .border_corner_bl = "+",
    .border_corner_br = "+",
    .border_t = "+",
    .border_b = "+",
    .border_l = "+",
    .border_r = "+",
    .border_cross = "+",
    .note_off_marker = "===",
    .continuation_marker = "...",
    .empty_cell = "...",
};

/*============================================================================
 * Theme Registry
 *============================================================================*/

static const TrackerTheme* BUILTIN_THEMES[] = {
    &THEME_DEFAULT,
    &THEME_RETRO,
};

static const char* BUILTIN_THEME_NAMES[] = {
    "default",
    "retro",
};

#define NUM_BUILTIN_THEMES (sizeof(BUILTIN_THEMES) / sizeof(BUILTIN_THEMES[0]))

/*============================================================================
 * Theme Functions
 *============================================================================*/

const TrackerTheme* tracker_theme_get(const char* name) {
    if (!name) return &THEME_DEFAULT;

    for (size_t i = 0; i < NUM_BUILTIN_THEMES; i++) {
        if (strcmp(BUILTIN_THEME_NAMES[i], name) == 0) {
            return BUILTIN_THEMES[i];
        }
    }

    return NULL;
}

const char** tracker_theme_list(int* count) {
    if (count) *count = NUM_BUILTIN_THEMES;
    return BUILTIN_THEME_NAMES;
}

void tracker_theme_init_default(TrackerTheme* theme) {
    if (!theme) return;
    memcpy(theme, &THEME_DEFAULT, sizeof(TrackerTheme));
}

TrackerTheme* tracker_theme_clone(const TrackerTheme* theme) {
    if (!theme) return NULL;

    TrackerTheme* copy = malloc(sizeof(TrackerTheme));
    if (!copy) return NULL;

    memcpy(copy, theme, sizeof(TrackerTheme));

    /* Deep copy strings */
    copy->name = str_dup(theme->name);
    copy->author = str_dup(theme->author);
    copy->border_h = str_dup(theme->border_h);
    copy->border_v = str_dup(theme->border_v);
    copy->border_corner_tl = str_dup(theme->border_corner_tl);
    copy->border_corner_tr = str_dup(theme->border_corner_tr);
    copy->border_corner_bl = str_dup(theme->border_corner_bl);
    copy->border_corner_br = str_dup(theme->border_corner_br);
    copy->border_t = str_dup(theme->border_t);
    copy->border_b = str_dup(theme->border_b);
    copy->border_l = str_dup(theme->border_l);
    copy->border_r = str_dup(theme->border_r);
    copy->border_cross = str_dup(theme->border_cross);
    copy->note_off_marker = str_dup(theme->note_off_marker);
    copy->continuation_marker = str_dup(theme->continuation_marker);
    copy->empty_cell = str_dup(theme->empty_cell);

    return copy;
}

void tracker_theme_free(TrackerTheme* theme) {
    if (!theme) return;

    /* Only free if it's a cloned theme (has heap-allocated strings) */
    /* Check if name is not pointing to static memory */
    bool is_builtin = false;
    for (size_t i = 0; i < NUM_BUILTIN_THEMES; i++) {
        if (theme == BUILTIN_THEMES[i]) {
            is_builtin = true;
            break;
        }
    }

    if (!is_builtin) {
        free((void*)theme->name);
        free((void*)theme->author);
        free((void*)theme->border_h);
        free((void*)theme->border_v);
        free((void*)theme->border_corner_tl);
        free((void*)theme->border_corner_tr);
        free((void*)theme->border_corner_bl);
        free((void*)theme->border_corner_br);
        free((void*)theme->border_t);
        free((void*)theme->border_b);
        free((void*)theme->border_l);
        free((void*)theme->border_r);
        free((void*)theme->border_cross);
        free((void*)theme->note_off_marker);
        free((void*)theme->continuation_marker);
        free((void*)theme->empty_cell);
        free(theme);
    }
}
