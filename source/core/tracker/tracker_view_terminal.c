/**
 * tracker_view_terminal.c - Terminal backend for tracker view
 *
 * Implements VT100-compatible terminal rendering for the tracker.
 */

#include "tracker_view_terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

/*============================================================================
 * Constants
 *============================================================================*/

#define DEFAULT_MIN_TRACK_WIDTH  10
#define DEFAULT_MAX_TRACK_WIDTH  20
#define DEFAULT_ROW_NUM_WIDTH    4
#define DEFAULT_FRAME_RATE       30

/* VT100 escape sequences */
#define ESC "\x1b"
#define CSI ESC "["

/* Cursor control */
#define CURSOR_HIDE       CSI "?25l"
#define CURSOR_SHOW       CSI "?25h"
#define CURSOR_HOME       CSI "H"
#define CURSOR_POS(r,c)   CSI "%d;%dH", (r), (c)

/* Screen control */
#define SCREEN_CLEAR      CSI "2J"
#define SCREEN_ALT_ON     CSI "?1049h"
#define SCREEN_ALT_OFF    CSI "?1049l"
#define LINE_CLEAR        CSI "K"

/* Colors */
#define COLOR_RESET       CSI "0m"
#define COLOR_BOLD        CSI "1m"
#define COLOR_DIM         CSI "2m"
#define COLOR_ITALIC      CSI "3m"
#define COLOR_UNDERLINE   CSI "4m"
#define COLOR_BLINK       CSI "5m"
#define COLOR_REVERSE     CSI "7m"

/* Unicode box-drawing characters */
#define BOX_H      "\xe2\x94\x80"  /* horizontal */
#define BOX_V      "\xe2\x94\x82"  /* vertical */
#define BOX_TL     "\xe2\x94\x8c"  /* top-left */
#define BOX_TR     "\xe2\x94\x90"  /* top-right */
#define BOX_BL     "\xe2\x94\x94"  /* bottom-left */
#define BOX_BR     "\xe2\x94\x98"  /* bottom-right */
#define BOX_T      "\xe2\x94\xac"  /* T down */
#define BOX_B      "\xe2\x94\xb4"  /* T up */
#define BOX_L      "\xe2\x94\x9c"  /* T right */
#define BOX_R      "\xe2\x94\xa4"  /* T left */
#define BOX_X      "\xe2\x94\xbc"  /* cross */

/* ASCII fallback box characters */
#define ASCII_H    "-"
#define ASCII_V    "|"
#define ASCII_TL   "+"
#define ASCII_TR   "+"
#define ASCII_BL   "+"
#define ASCII_BR   "+"
#define ASCII_T    "+"
#define ASCII_B    "+"
#define ASCII_L    "+"
#define ASCII_R    "+"
#define ASCII_X    "+"

/*============================================================================
 * Terminal Backend Data
 *============================================================================*/

typedef struct {
    /* File descriptors */
    int input_fd;
    int output_fd;

    /* Original terminal state */
    struct termios orig_termios;
    bool raw_mode_enabled;

    /* Configuration */
    TrackerTerminalConfig config;

    /* Current dimensions */
    int screen_cols;
    int screen_rows;

    /* Layout cache */
    TrackerTerminalLayout layout;
    bool layout_dirty;

    /* Output buffer for batched writes */
    char* output_buffer;
    int output_buffer_len;
    int output_buffer_capacity;

    /* Input buffer for escape sequences */
    char input_buffer[32];
    int input_buffer_len;

    /* Test mode */
    char* render_target;       /* if set, render to string instead */
    int render_target_capacity;

    /* Injected input for testing */
    char* injected_input;
    int injected_input_pos;
    int injected_input_len;

    /* Box drawing characters (based on config) */
    const char* box_h;
    const char* box_v;
    const char* box_tl;
    const char* box_tr;
    const char* box_bl;
    const char* box_br;
    const char* box_t;
    const char* box_b;
    const char* box_l;
    const char* box_r;
    const char* box_x;

} TerminalBackend;

/*============================================================================
 * Forward Declarations
 *============================================================================*/

static bool terminal_init(TrackerView* view);
static void terminal_cleanup(TrackerView* view);
static void terminal_render(TrackerView* view);
static void terminal_render_incremental(TrackerView* view, uint32_t dirty_flags);
static bool terminal_poll_input(TrackerView* view, int timeout_ms, TrackerInputEvent* out);
static void terminal_get_dimensions(TrackerView* view, int* out_w, int* out_h);
static void terminal_show_message(TrackerView* view, const char* msg);
static void terminal_show_error(TrackerView* view, const char* msg);
static bool terminal_prompt_input(TrackerView* view, const char* prompt,
                                  const char* default_val, char* out, int size);
static bool terminal_prompt_confirm(TrackerView* view, const char* msg);
static void terminal_beep(TrackerView* view);

static void calculate_layout(TrackerView* view);
static void enable_raw_mode(TerminalBackend* tb);
static void disable_raw_mode(TerminalBackend* tb);
static void update_terminal_size(TerminalBackend* tb);
static void output_write(TerminalBackend* tb, const char* data, int len);
static void output_printf(TerminalBackend* tb, const char* fmt, ...);
static void output_flush(TerminalBackend* tb);
static void apply_style(TerminalBackend* tb, const TrackerStyle* style);
static void reset_style(TerminalBackend* tb);
static int read_key(TerminalBackend* tb, int timeout_ms);
static TrackerInputType translate_key(int key, uint32_t* modifiers);

/*============================================================================
 * Creation Functions
 *============================================================================*/

void tracker_terminal_config_init(TrackerTerminalConfig* config) {
    config->min_track_width = DEFAULT_MIN_TRACK_WIDTH;
    config->max_track_width = DEFAULT_MAX_TRACK_WIDTH;
    config->row_number_width = DEFAULT_ROW_NUM_WIDTH;
    config->use_unicode_borders = true;
    config->use_colors = true;
    config->use_256_colors = true;
    config->use_true_color = true;  /* Most modern terminals support 24-bit color */
    config->alternate_screen = true;
    config->mouse_support = false;
    config->frame_rate = DEFAULT_FRAME_RATE;
}

TrackerView* tracker_view_terminal_new(void) {
    TrackerTerminalConfig config;
    tracker_terminal_config_init(&config);
    return tracker_view_terminal_new_with_config(&config);
}

TrackerView* tracker_view_terminal_new_with_fds(int input_fd, int output_fd) {
    TrackerTerminalConfig config;
    tracker_terminal_config_init(&config);

    TrackerView* view = tracker_view_terminal_new_with_config(&config);
    if (view) {
        TerminalBackend* tb = (TerminalBackend*)view->callbacks.backend_data;
        tb->input_fd = input_fd;
        tb->output_fd = output_fd;
    }
    return view;
}

TrackerView* tracker_view_terminal_new_with_config(const TrackerTerminalConfig* config) {
    /* Allocate backend data */
    TerminalBackend* tb = calloc(1, sizeof(TerminalBackend));
    if (!tb) return NULL;

    tb->input_fd = STDIN_FILENO;
    tb->output_fd = STDOUT_FILENO;
    tb->config = *config;
    tb->layout_dirty = true;

    /* Allocate output buffer */
    tb->output_buffer_capacity = 16384;
    tb->output_buffer = malloc(tb->output_buffer_capacity);
    if (!tb->output_buffer) {
        free(tb);
        return NULL;
    }

    /* Set box drawing characters */
    if (config->use_unicode_borders) {
        tb->box_h = BOX_H;
        tb->box_v = BOX_V;
        tb->box_tl = BOX_TL;
        tb->box_tr = BOX_TR;
        tb->box_bl = BOX_BL;
        tb->box_br = BOX_BR;
        tb->box_t = BOX_T;
        tb->box_b = BOX_B;
        tb->box_l = BOX_L;
        tb->box_r = BOX_R;
        tb->box_x = BOX_X;
    } else {
        tb->box_h = ASCII_H;
        tb->box_v = ASCII_V;
        tb->box_tl = ASCII_TL;
        tb->box_tr = ASCII_TR;
        tb->box_bl = ASCII_BL;
        tb->box_br = ASCII_BR;
        tb->box_t = ASCII_T;
        tb->box_b = ASCII_B;
        tb->box_l = ASCII_L;
        tb->box_r = ASCII_R;
        tb->box_x = ASCII_X;
    }

    /* Set up callbacks */
    TrackerViewCallbacks callbacks = {
        .init = terminal_init,
        .cleanup = terminal_cleanup,
        .render = terminal_render,
        .render_incremental = terminal_render_incremental,
        .poll_input = terminal_poll_input,
        .get_dimensions = terminal_get_dimensions,
        .show_message = terminal_show_message,
        .show_error = terminal_show_error,
        .prompt_input = terminal_prompt_input,
        .prompt_confirm = terminal_prompt_confirm,
        .beep = terminal_beep,
        .backend_data = tb,
    };

    TrackerView* view = tracker_view_new(&callbacks);
    if (!view) {
        free(tb->output_buffer);
        free(tb);
        return NULL;
    }

    return view;
}

/*============================================================================
 * Lifecycle Callbacks
 *============================================================================*/

static bool terminal_init(TrackerView* view) {
    TerminalBackend* tb = (TerminalBackend*)view->callbacks.backend_data;

    /* Get terminal size */
    update_terminal_size(tb);

    /* Enable raw mode */
    enable_raw_mode(tb);

    /* Switch to alternate screen buffer */
    if (tb->config.alternate_screen) {
        output_write(tb, SCREEN_ALT_ON, -1);
    }

    /* Hide cursor, clear screen */
    output_write(tb, CURSOR_HIDE, -1);
    output_write(tb, SCREEN_CLEAR, -1);
    output_write(tb, CURSOR_HOME, -1);
    output_flush(tb);

    return true;
}

static void terminal_cleanup(TrackerView* view) {
    TerminalBackend* tb = (TerminalBackend*)view->callbacks.backend_data;

    /* Show cursor */
    output_write(tb, CURSOR_SHOW, -1);

    /* Reset colors */
    output_write(tb, COLOR_RESET, -1);

    /* Return to normal screen buffer */
    if (tb->config.alternate_screen) {
        output_write(tb, SCREEN_ALT_OFF, -1);
    }

    output_flush(tb);

    /* Restore terminal mode */
    disable_raw_mode(tb);

    /* Free layout track widths */
    free(tb->layout.track_widths);

    /* Free buffers */
    free(tb->output_buffer);
    free(tb->render_target);
    free(tb->injected_input);

    free(tb);
}

/*============================================================================
 * Terminal Mode Control
 *============================================================================*/

static void enable_raw_mode(TerminalBackend* tb) {
    if (tb->raw_mode_enabled) return;
    if (!isatty(tb->input_fd)) return;

    tcgetattr(tb->input_fd, &tb->orig_termios);

    struct termios raw = tb->orig_termios;

    /* Input modes: no break, no CR to NL, no parity check, no strip char,
     * no start/stop output control */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

    /* Output modes: disable post processing */
    raw.c_oflag &= ~(OPOST);

    /* Control modes: set 8 bit chars */
    raw.c_cflag |= (CS8);

    /* Local modes: no echo, no canonical, no extended functions, no signals */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    /* Control chars: return immediately with 0 bytes */
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    tcsetattr(tb->input_fd, TCSAFLUSH, &raw);
    tb->raw_mode_enabled = true;
}

static void disable_raw_mode(TerminalBackend* tb) {
    if (!tb->raw_mode_enabled) return;
    tcsetattr(tb->input_fd, TCSAFLUSH, &tb->orig_termios);
    tb->raw_mode_enabled = false;
}

static void update_terminal_size(TerminalBackend* tb) {
    struct winsize ws;
    if (ioctl(tb->output_fd, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        tb->screen_cols = 80;
        tb->screen_rows = 24;
    } else {
        tb->screen_cols = ws.ws_col;
        tb->screen_rows = ws.ws_row;
    }
    tb->layout_dirty = true;
}

/*============================================================================
 * Output Buffering
 *============================================================================*/

static void output_write(TerminalBackend* tb, const char* data, int len) {
    if (len < 0) len = strlen(data);

    /* Grow buffer if needed */
    while (tb->output_buffer_len + len > tb->output_buffer_capacity) {
        tb->output_buffer_capacity *= 2;
        tb->output_buffer = realloc(tb->output_buffer, tb->output_buffer_capacity);
    }

    memcpy(tb->output_buffer + tb->output_buffer_len, data, len);
    tb->output_buffer_len += len;
}

static void output_printf(TerminalBackend* tb, const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len > 0) {
        output_write(tb, buf, len < (int)sizeof(buf) ? len : (int)sizeof(buf) - 1);
    }
}

static void output_flush(TerminalBackend* tb) {
    if (tb->render_target) {
        /* Rendering to string buffer */
        if (tb->output_buffer_len < tb->render_target_capacity - 1) {
            memcpy(tb->render_target, tb->output_buffer, tb->output_buffer_len);
            tb->render_target[tb->output_buffer_len] = '\0';
        }
    } else {
        /* Write to terminal */
        write(tb->output_fd, tb->output_buffer, tb->output_buffer_len);
    }
    tb->output_buffer_len = 0;
}

/*============================================================================
 * Color/Style Application
 *============================================================================*/

static void apply_style(TerminalBackend* tb, const TrackerStyle* style) {
    if (!tb->config.use_colors) return;

    /* Reset first */
    output_write(tb, COLOR_RESET, -1);

    /* Apply attributes */
    if (style->attr & TRACKER_ATTR_BOLD) {
        output_write(tb, COLOR_BOLD, -1);
    }
    if (style->attr & TRACKER_ATTR_DIM) {
        output_write(tb, COLOR_DIM, -1);
    }
    if (style->attr & TRACKER_ATTR_ITALIC) {
        output_write(tb, COLOR_ITALIC, -1);
    }
    if (style->attr & TRACKER_ATTR_UNDERLINE) {
        output_write(tb, COLOR_UNDERLINE, -1);
    }
    if (style->attr & TRACKER_ATTR_REVERSE) {
        output_write(tb, COLOR_REVERSE, -1);
    }

    /* Apply foreground color */
    if (style->fg.type == TRACKER_COLOR_INDEXED) {
        if (tb->config.use_256_colors) {
            output_printf(tb, CSI "38;5;%dm", style->fg.value.index);
        } else if (style->fg.value.index < 8) {
            output_printf(tb, CSI "%dm", 30 + style->fg.value.index);
        }
    } else if (style->fg.type == TRACKER_COLOR_RGB && tb->config.use_true_color) {
        output_printf(tb, CSI "38;2;%d;%d;%dm",
                      style->fg.value.rgb.r,
                      style->fg.value.rgb.g,
                      style->fg.value.rgb.b);
    }

    /* Apply background color */
    if (style->bg.type == TRACKER_COLOR_INDEXED) {
        if (tb->config.use_256_colors) {
            output_printf(tb, CSI "48;5;%dm", style->bg.value.index);
        } else if (style->bg.value.index < 8) {
            output_printf(tb, CSI "%dm", 40 + style->bg.value.index);
        }
    } else if (style->bg.type == TRACKER_COLOR_RGB && tb->config.use_true_color) {
        output_printf(tb, CSI "48;2;%d;%d;%dm",
                      style->bg.value.rgb.r,
                      style->bg.value.rgb.g,
                      style->bg.value.rgb.b);
    }
}

static void reset_style(TerminalBackend* tb) {
    output_write(tb, COLOR_RESET, -1);
}

/*============================================================================
 * Layout Calculation
 *============================================================================*/

static void calculate_layout(TrackerView* view) {
    TerminalBackend* tb = (TerminalBackend*)view->callbacks.backend_data;
    TrackerTerminalLayout* layout = &tb->layout;

    if (!tb->layout_dirty) return;

    /* Store screen dimensions */
    layout->screen_cols = tb->screen_cols;
    layout->screen_rows = tb->screen_rows;

    /* Header: pattern name + track headers */
    layout->header_rows = 2;

    /* Footer: status line */
    layout->footer_rows = 1;
    layout->status_row = tb->screen_rows;
    layout->command_row = tb->screen_rows;

    /* Grid area */
    layout->grid_start_col = 1;
    layout->grid_start_row = layout->header_rows + 1;
    layout->grid_rows = tb->screen_rows - layout->header_rows - layout->footer_rows;

    /* Row number column */
    layout->row_num_width = tb->config.row_number_width;

    /* Calculate track widths */
    int available_width = tb->screen_cols - layout->row_num_width - 2;  /* borders */

    /* Get number of tracks from song */
    int total_tracks = 0;
    if (view->song) {
        TrackerPattern* pattern = tracker_song_get_pattern(view->song,
                                                           view->state.cursor_pattern);
        if (pattern) {
            total_tracks = pattern->num_tracks;
        }
    }

    if (total_tracks == 0) total_tracks = 1;

    /* Calculate how many tracks fit */
    int track_width = tb->config.min_track_width;
    int visible_tracks = available_width / (track_width + 1);  /* +1 for separator */
    if (visible_tracks < 1) visible_tracks = 1;
    if (visible_tracks > total_tracks) visible_tracks = total_tracks;

    /* Distribute width evenly */
    if (visible_tracks > 0) {
        track_width = (available_width - visible_tracks + 1) / visible_tracks;
        if (track_width > tb->config.max_track_width) {
            track_width = tb->config.max_track_width;
        }
        if (track_width < tb->config.min_track_width) {
            track_width = tb->config.min_track_width;
        }
    }

    /* Allocate track widths array */
    free(layout->track_widths);
    layout->track_widths = malloc(visible_tracks * sizeof(int));
    for (int i = 0; i < visible_tracks; i++) {
        layout->track_widths[i] = track_width;
    }

    layout->track_count = visible_tracks;
    layout->track_start = view->state.scroll_track;

    /* Clamp scroll position */
    if (layout->track_start + visible_tracks > total_tracks) {
        layout->track_start = total_tracks - visible_tracks;
    }
    if (layout->track_start < 0) layout->track_start = 0;

    /* Row range */
    layout->row_start = view->state.scroll_row;
    layout->row_count = layout->grid_rows;

    layout->grid_cols = layout->row_num_width + 1;
    for (int i = 0; i < visible_tracks; i++) {
        layout->grid_cols += layout->track_widths[i] + 1;
    }

    /* Update view state */
    view->state.visible_tracks = visible_tracks;
    view->state.visible_rows = layout->row_count;

    tb->layout_dirty = false;
}

/*============================================================================
 * Rendering
 *============================================================================*/

static void render_header(TrackerView* view) {
    TerminalBackend* tb = (TerminalBackend*)view->callbacks.backend_data;
    const TrackerTheme* theme = view->state.theme;
    TrackerTerminalLayout* layout = &tb->layout;

    /* Row 1: Pattern name and info */
    output_printf(tb, CSI "%d;%dH", 1, 1);
    apply_style(tb, &theme->header_style);

    TrackerPattern* pattern = NULL;
    if (view->song) {
        pattern = tracker_song_get_pattern(view->song, view->state.cursor_pattern);
    }

    if (pattern && pattern->name) {
        output_printf(tb, " [%s] ", pattern->name);
    } else {
        output_printf(tb, " [Pattern %d] ", view->state.cursor_pattern + 1);
    }

    /* Song info */
    if (view->song) {
        output_printf(tb, "%d BPM  %d/%d",
                      view->song->bpm,
                      view->state.cursor_pattern + 1,
                      view->song->num_patterns);
    }

    output_write(tb, LINE_CLEAR, -1);

    /* Row 2: Track headers */
    output_printf(tb, CSI "%d;%dH", 2, 1);

    /* Row number column header - match the grid row format */
    output_printf(tb, "%*s", layout->row_num_width - 1, "");
    output_write(tb, tb->box_v, -1);

    /* Track names */
    for (int i = 0; i < layout->track_count; i++) {
        int track_idx = layout->track_start + i;
        int width = layout->track_widths[i];

        if (pattern && track_idx < pattern->num_tracks) {
            TrackerTrack* track = &pattern->tracks[track_idx];

            /* Check track state for styling */
            if (track->muted) {
                apply_style(tb, &theme->track_muted);
            } else if (track->solo) {
                apply_style(tb, &theme->track_solo);
            } else {
                apply_style(tb, &theme->header_style);
            }

            /* Track name or number with mute/solo indicator */
            char header[64];
            const char* indicator = "";
            if (track->muted && track->solo) {
                indicator = "[MS]";
            } else if (track->muted) {
                indicator = "[M]";
            } else if (track->solo) {
                indicator = "[S]";
            }

            if (track->name) {
                snprintf(header, sizeof(header), "%s%s", track->name, indicator);
            } else {
                snprintf(header, sizeof(header), "Track %d%s", track_idx + 1, indicator);
            }

            /* Center the header */
            int name_len = strlen(header);
            if (name_len > width - 2) name_len = width - 2;
            int pad_left = (width - name_len) / 2;
            int pad_right = width - name_len - pad_left;

            output_printf(tb, "%*s%.*s%*s",
                          pad_left, "",
                          name_len, header,
                          pad_right, "");
        } else {
            output_printf(tb, "%*s", width, "");
        }

        output_write(tb, tb->box_v, -1);
    }

    reset_style(tb);
    output_write(tb, LINE_CLEAR, -1);
}

static void render_separator(TrackerView* view, int row) {
    TerminalBackend* tb = (TerminalBackend*)view->callbacks.backend_data;
    TrackerTerminalLayout* layout = &tb->layout;

    output_printf(tb, CSI "%d;%dH", row, 1);

    /* Row number area - no left edge, just horizontal lines */
    for (int i = 0; i < layout->row_num_width - 1; i++) {
        output_write(tb, tb->box_h, -1);
    }
    output_write(tb, tb->box_x, -1);

    /* Track columns */
    for (int i = 0; i < layout->track_count; i++) {
        int width = layout->track_widths[i];
        for (int j = 0; j < width; j++) {
            output_write(tb, tb->box_h, -1);
        }
        if (i < layout->track_count - 1) {
            output_write(tb, tb->box_x, -1);
        }
    }

    output_write(tb, LINE_CLEAR, -1);
}

static void render_grid_row(TrackerView* view, int screen_row, int pattern_row) {
    TerminalBackend* tb = (TerminalBackend*)view->callbacks.backend_data;
    const TrackerTheme* theme = view->state.theme;
    TrackerTerminalLayout* layout = &tb->layout;

    TrackerPattern* pattern = NULL;
    if (view->song) {
        pattern = tracker_song_get_pattern(view->song, view->state.cursor_pattern);
    }

    output_printf(tb, CSI "%d;%dH", screen_row, 1);

    /* Determine row highlighting */
    bool is_playing_row = view->state.is_playing &&
                          pattern_row == view->state.playback_row;
    bool is_beat_row = (pattern_row % view->state.beat_highlight_interval) == 0;
    bool is_cursor_row = pattern_row == view->state.cursor_row;

    /* Row number */
    if (is_playing_row) {
        apply_style(tb, &theme->playing_row);
    } else if (is_beat_row) {
        apply_style(tb, &theme->row_beat);
    } else {
        apply_style(tb, &theme->header_style);
    }

    output_printf(tb, "%*d", layout->row_num_width - 1, pattern_row);
    reset_style(tb);
    output_write(tb, tb->box_v, -1);

    /* Cells */
    for (int i = 0; i < layout->track_count; i++) {
        int track_idx = layout->track_start + i;
        int width = layout->track_widths[i];

        bool is_cursor_cell = is_cursor_row && track_idx == view->state.cursor_track;
        bool is_selected = tracker_view_is_selected(view, track_idx, pattern_row);

        /* Get cell content */
        /* Note: tracker_pattern_get_cell(pattern, row, track) */
        TrackerCell* cell = NULL;
        if (pattern && track_idx < pattern->num_tracks &&
            pattern_row < pattern->num_rows) {
            cell = tracker_pattern_get_cell(pattern, pattern_row, track_idx);
        }

        /* Determine cell style */
        const TrackerStyle* cell_style;
        if (is_cursor_cell) {
            if (view->state.edit_mode == TRACKER_EDIT_MODE_EDIT) {
                cell_style = &theme->cursor_edit;
            } else {
                cell_style = &theme->cursor;
            }
        } else if (is_selected) {
            cell_style = &theme->selection;
        } else if (is_playing_row) {
            cell_style = &theme->playing_row;
        } else if (is_beat_row) {
            cell_style = &theme->row_beat;
        } else if (cell && cell->type == TRACKER_CELL_EMPTY) {
            cell_style = &theme->cell_empty;
        } else if (cell && cell->type == TRACKER_CELL_NOTE_OFF) {
            cell_style = &theme->cell_off;
        } else if (cell && cell->type == TRACKER_CELL_CONTINUATION) {
            cell_style = &theme->cell_continuation;
        } else {
            cell_style = &theme->cell_note;
        }

        apply_style(tb, cell_style);

        /* Render cell content */
        char content[64] = "";

        /* In edit mode, show edit buffer for cursor cell */
        if (is_cursor_cell && view->state.edit_mode == TRACKER_EDIT_MODE_EDIT) {
            if (view->state.edit_buffer && view->state.edit_buffer_len > 0) {
                snprintf(content, sizeof(content), "%s", view->state.edit_buffer);
            } else {
                content[0] = '\0';  /* Empty during edit */
            }
        } else if (cell) {
            switch (cell->type) {
                case TRACKER_CELL_EMPTY:
                    snprintf(content, sizeof(content), "%s",
                             theme->empty_cell ? theme->empty_cell : "---");
                    break;
                case TRACKER_CELL_EXPRESSION:
                    if (cell->expression) {
                        snprintf(content, sizeof(content), "%s", cell->expression);
                    }
                    break;
                case TRACKER_CELL_NOTE_OFF:
                    snprintf(content, sizeof(content), "%s",
                             theme->note_off_marker ? theme->note_off_marker : "OFF");
                    break;
                case TRACKER_CELL_CONTINUATION:
                    snprintf(content, sizeof(content), "%s",
                             theme->continuation_marker ? theme->continuation_marker : "...");
                    break;
            }
        } else {
            snprintf(content, sizeof(content), "---");
        }

        /* Truncate/pad to width */
        int content_len = strlen(content);
        if (content_len > width) content_len = width;

        output_printf(tb, "%.*s", content_len, content);
        int remaining = width - content_len;
        if (remaining > 0) {
            output_printf(tb, "%*s", remaining, "");
        }

        reset_style(tb);
        output_write(tb, tb->box_v, -1);
    }

    output_write(tb, LINE_CLEAR, -1);
}

static void render_status(TrackerView* view) {
    TerminalBackend* tb = (TerminalBackend*)view->callbacks.backend_data;
    const TrackerTheme* theme = view->state.theme;

    output_printf(tb, CSI "%d;%dH", tb->screen_rows, 1);
    apply_style(tb, &theme->status_style);

    /* Transport state */
    const char* transport = view->state.is_playing ? "[PLAY]" : "[STOP]";
    output_printf(tb, " %s", transport);

    /* Record indicator */
    if (view->state.is_recording) {
        apply_style(tb, &theme->error_style);  /* Red for record */
        output_printf(tb, " [REC]");
        apply_style(tb, &theme->status_style);
    }

    /* Loop indicator */
    if (view->engine && view->engine->loop_enabled) {
        output_printf(tb, " [LOOP]");
    }

    /* Play mode indicator */
    if (view->engine) {
        if (view->engine->play_mode == TRACKER_PLAY_MODE_SONG) {
            output_printf(tb, " [SONG]");
        } else {
            output_printf(tb, " [PAT]");
        }
    }

    /* BPM */
    if (view->song) {
        output_printf(tb, " %d BPM", view->song->bpm);
    }

    /* Position */
    TrackerPattern* pattern = NULL;
    if (view->song) {
        pattern = tracker_song_get_pattern(view->song, view->state.cursor_pattern);
    }
    int total_rows = pattern ? pattern->num_rows : 0;
    output_printf(tb, " | Row %d/%d",
                  view->state.cursor_row + 1,
                  total_rows);

    /* Pattern */
    if (view->song) {
        output_printf(tb, " | Pattern %d/%d",
                      view->state.cursor_pattern + 1,
                      view->song->num_patterns);
    }

    /* Mode indicator */
    const char* mode = "";
    if (view->state.selecting) {
        mode = "VISUAL";
    } else {
        switch (view->state.edit_mode) {
            case TRACKER_EDIT_MODE_NAVIGATE: mode = "NAV"; break;
            case TRACKER_EDIT_MODE_EDIT: mode = "EDIT"; break;
            case TRACKER_EDIT_MODE_SELECT: mode = "SEL"; break;
            case TRACKER_EDIT_MODE_COMMAND: mode = "CMD"; break;
        }
    }
    output_printf(tb, " | %s", mode);

    /* Step size and octave */
    output_printf(tb, " | Oct:%d Step:%d",
                  view->state.default_octave, view->state.step_size);

    /* Command line or error/status message */
    if (view->state.edit_mode == TRACKER_EDIT_MODE_COMMAND) {
        /* Show command line */
        reset_style(tb);
        output_write(tb, LINE_CLEAR, -1);
        output_printf(tb, CSI "%d;%dH", tb->screen_rows, 1);
        apply_style(tb, &theme->command_style);
        output_printf(tb, ":%s", view->state.command_buffer ? view->state.command_buffer : "");
        output_write(tb, LINE_CLEAR, -1);
        /* Position cursor after the command text */
        output_printf(tb, CSI "%d;%dH", tb->screen_rows, 2 + view->state.command_cursor_pos);
    } else {
        /* Show error or status message */
        if (view->state.error_message) {
            apply_style(tb, &theme->error_style);
            output_printf(tb, " | %s", view->state.error_message);
        } else if (view->state.status_message) {
            output_printf(tb, " | %s", view->state.status_message);
        }
        reset_style(tb);
        output_write(tb, LINE_CLEAR, -1);
    }
}

static void render_help(TrackerView* view) {
    TerminalBackend* tb = (TerminalBackend*)view->callbacks.backend_data;
    const TrackerTheme* theme = view->state.theme;

    output_write(tb, CURSOR_HOME, -1);
    output_write(tb, SCREEN_CLEAR, -1);

    apply_style(tb, &theme->header_style);
    output_printf(tb, "  TRACKER HELP - Press any key to return\n\n");
    reset_style(tb);

    apply_style(tb, &theme->default_style);

    output_printf(tb, "  NAVIGATION\n");
    output_printf(tb, "    h/j/k/l, Arrows  Move cursor\n");
    output_printf(tb, "    g / G            Go to start / end of pattern\n");
    output_printf(tb, "    [ / ]            Previous / next pattern\n");
    output_printf(tb, "    PgUp / PgDn      Page up / down\n\n");

    output_printf(tb, "  EDITING\n");
    output_printf(tb, "    i, Enter         Enter edit mode\n");
    output_printf(tb, "    Escape           Exit edit mode / clear selection\n");
    output_printf(tb, "    x                Clear cell\n");
    output_printf(tb, "    o / O            Insert row / duplicate row\n");
    output_printf(tb, "    X                Delete row\n\n");

    output_printf(tb, "  SELECTION & CLIPBOARD\n");
    output_printf(tb, "    v                Visual selection mode\n");
    output_printf(tb, "    y                Copy (yank)\n");
    output_printf(tb, "    d                Cut (delete)\n");
    output_printf(tb, "    p                Paste\n\n");

    output_printf(tb, "  TRACKS\n");
    output_printf(tb, "    m / S            Mute / Solo track\n");
    output_printf(tb, "    a / A            Add / remove track\n\n");

    output_printf(tb, "  PATTERNS\n");
    output_printf(tb, "    n                New pattern\n");
    output_printf(tb, "    c                Clone pattern\n");
    output_printf(tb, "    D                Delete pattern\n\n");

    output_printf(tb, "  PLAYBACK\n");
    output_printf(tb, "    Space            Play / Stop\n");
    output_printf(tb, "    P                Toggle play mode (PAT/SONG)\n");
    output_printf(tb, "    Ctrl+R           Toggle record mode\n");
    output_printf(tb, "    f                Toggle follow mode\n");
    output_printf(tb, "    L                Toggle loop mode\n");
    output_printf(tb, "    { / }            Decrease / increase BPM\n\n");

    output_printf(tb, "  SETTINGS\n");
    output_printf(tb, "    + / -            Increase / decrease step size\n");
    output_printf(tb, "    > / <  (. / ,)   Increase / decrease octave\n");
    output_printf(tb, "    T                Cycle theme\n\n");

    output_printf(tb, "  ARRANGE (press 'r' to enter)\n");
    output_printf(tb, "    j/k, Arrows      Move in sequence\n");
    output_printf(tb, "    a                Add pattern to sequence\n");
    output_printf(tb, "    x                Remove from sequence\n");
    output_printf(tb, "    K / J            Move entry up / down\n");
    output_printf(tb, "    Enter            Jump to pattern\n\n");

    output_printf(tb, "  COMMANDS (press ':' to enter)\n");
    output_printf(tb, "    :w               Save\n");
    output_printf(tb, "    :q               Quit\n");
    output_printf(tb, "    :wq              Save and quit\n");
    output_printf(tb, "    :bpm N           Set tempo\n");
    output_printf(tb, "    :rows N          Set pattern length\n");
    output_printf(tb, "    :export [file]   Export to MIDI\n");
    output_printf(tb, "    :set step N      Set step size\n");
    output_printf(tb, "    :set octave N    Set default octave\n");
    output_printf(tb, "    :set swing N     Set swing (0-100)\n");
    output_printf(tb, "    :name [text]     Set pattern name\n\n");

    output_printf(tb, "  FILE\n");
    output_printf(tb, "    Ctrl+S           Save\n");
    output_printf(tb, "    E, Ctrl+E        Export MIDI\n");
    output_printf(tb, "    q                Quit\n");

    reset_style(tb);
}

static void render_arrange(TrackerView* view) {
    TerminalBackend* tb = (TerminalBackend*)view->callbacks.backend_data;
    const TrackerTheme* theme = view->state.theme;

    output_write(tb, CURSOR_HOME, -1);
    output_write(tb, SCREEN_CLEAR, -1);

    /* Header */
    apply_style(tb, &theme->header_style);
    output_printf(tb, "  ARRANGE - Pattern Sequence");
    if (view->song) {
        output_printf(tb, "  (%d entries)", view->song->sequence_length);
    }
    output_printf(tb, "\n");
    reset_style(tb);

    output_printf(tb, "  a=add  x=remove  K/J=move  Enter=goto  Esc=back  ?=help\n\n");

    if (!view->song || view->song->sequence_length == 0) {
        apply_style(tb, &theme->default_style);
        output_printf(tb, "  (empty sequence)\n\n");
        output_printf(tb, "  Press 'a' to add current pattern to sequence\n");
        reset_style(tb);
        return;
    }

    /* Calculate visible range */
    int visible_rows = tb->screen_rows - 8;
    if (visible_rows < 5) visible_rows = 5;

    int cursor = view->state.sequence_cursor;
    int scroll = view->state.sequence_scroll;

    /* Adjust scroll to keep cursor visible */
    if (cursor < scroll) {
        scroll = cursor;
    } else if (cursor >= scroll + visible_rows) {
        scroll = cursor - visible_rows + 1;
    }
    view->state.sequence_scroll = scroll;

    /* Render sequence entries */
    apply_style(tb, &theme->default_style);

    for (int i = 0; i < visible_rows && scroll + i < view->song->sequence_length; i++) {
        int idx = scroll + i;
        TrackerSequenceEntry* entry = &view->song->sequence[idx];

        bool is_cursor = (idx == cursor);

        /* Cursor indicator */
        if (is_cursor) {
            apply_style(tb, &theme->cursor);
            output_printf(tb, " >");
        } else {
            apply_style(tb, &theme->default_style);
            output_printf(tb, "  ");
        }

        /* Entry number */
        output_printf(tb, " %3d: ", idx + 1);

        /* Pattern info */
        TrackerPattern* pattern = tracker_song_get_pattern(view->song, entry->pattern_index);
        if (pattern) {
            output_printf(tb, "Pattern %2d", entry->pattern_index + 1);
            if (pattern->name && pattern->name[0]) {
                output_printf(tb, " \"%s\"", pattern->name);
            }
            output_printf(tb, " (%d rows)", pattern->num_rows);
        } else {
            output_printf(tb, "(invalid pattern %d)", entry->pattern_index);
        }

        /* Repeat count */
        if (entry->repeat_count > 1) {
            output_printf(tb, " x%d", entry->repeat_count);
        }

        if (is_cursor) {
            reset_style(tb);
        }
        output_printf(tb, "\n");
    }

    /* Scroll indicator */
    reset_style(tb);
    if (view->song->sequence_length > visible_rows) {
        output_printf(tb, "\n  [%d-%d of %d]\n",
            scroll + 1,
            (scroll + visible_rows < view->song->sequence_length) ?
                scroll + visible_rows : view->song->sequence_length,
            view->song->sequence_length);
    }
}

static void terminal_render(TrackerView* view) {
    TerminalBackend* tb = (TerminalBackend*)view->callbacks.backend_data;

    /* Check for help mode */
    if (view->state.view_mode == TRACKER_VIEW_MODE_HELP) {
        render_help(view);
        return;
    }

    /* Check for arrange mode */
    if (view->state.view_mode == TRACKER_VIEW_MODE_ARRANGE) {
        render_arrange(view);
        return;
    }

    /* Recalculate layout if needed */
    calculate_layout(view);

    TrackerTerminalLayout* layout = &tb->layout;

    /* Clear screen */
    output_write(tb, CURSOR_HOME, -1);

    /* Header */
    render_header(view);

    /* Separator after header */
    render_separator(view, 3);

    /* Grid rows */
    TrackerPattern* pattern = NULL;
    if (view->song) {
        pattern = tracker_song_get_pattern(view->song, view->state.cursor_pattern);
    }

    int num_rows = pattern ? pattern->num_rows : 0;

    for (int i = 0; i < layout->row_count; i++) {
        int screen_row = layout->grid_start_row + i + 1;  /* +1 for separator */
        int pattern_row = layout->row_start + i;

        if (pattern_row < num_rows) {
            render_grid_row(view, screen_row, pattern_row);
        } else {
            /* Empty row */
            output_printf(tb, CSI "%d;%dH", screen_row, 1);
            output_write(tb, LINE_CLEAR, -1);
        }
    }

    /* Status line */
    render_status(view);

    /* Position cursor */
    if (view->state.edit_mode == TRACKER_EDIT_MODE_EDIT) {
        /* Show cursor in edit cell */
        int cursor_screen_row = layout->grid_start_row + 1 +
                                (view->state.cursor_row - layout->row_start);

        /* Calculate column: row_num_width chars (including separator) + track offsets */
        int cursor_screen_col = layout->row_num_width;  /* After row num + separator */
        for (int i = 0; i < view->state.cursor_track - layout->track_start; i++) {
            cursor_screen_col += layout->track_widths[i] + 1;  /* +1 for separator */
        }
        cursor_screen_col += view->state.edit_cursor_pos + 1;  /* +1 for 1-based column */

        output_printf(tb, CSI "%d;%dH", cursor_screen_row, cursor_screen_col);
        output_write(tb, CURSOR_SHOW, -1);
    } else {
        output_write(tb, CURSOR_HIDE, -1);
    }

    output_flush(tb);

    view->dirty_flags = TRACKER_DIRTY_NONE;
}

static void terminal_render_incremental(TrackerView* view, uint32_t dirty_flags) {
    /* For now, just do full render */
    /* TODO: Optimize to only redraw dirty regions */
    (void)dirty_flags;
    terminal_render(view);
}

/*============================================================================
 * Input Handling
 *============================================================================*/

static int read_key(TerminalBackend* tb, int timeout_ms) {
    /* Check for injected input first */
    if (tb->injected_input && tb->injected_input_pos < tb->injected_input_len) {
        return tb->injected_input[tb->injected_input_pos++];
    }

    /* Set up select for timeout */
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(tb->input_fd, &fds);

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int result = select(tb->input_fd + 1, &fds, NULL, NULL, &tv);
    if (result <= 0) return -1;

    unsigned char c;
    if (read(tb->input_fd, &c, 1) != 1) return -1;

    /* Handle escape sequences */
    if (c == '\x1b') {
        /* Check if more data available */
        FD_ZERO(&fds);
        FD_SET(tb->input_fd, &fds);
        tv.tv_sec = 0;
        tv.tv_usec = 50000;  /* 50ms timeout for escape sequence */

        if (select(tb->input_fd + 1, &fds, NULL, NULL, &tv) > 0) {
            char seq[8];
            int seq_len = 0;

            while (seq_len < 7 && read(tb->input_fd, &seq[seq_len], 1) == 1) {
                seq_len++;

                /* Check for complete sequences */
                if (seq_len == 2 && seq[0] == '[') {
                    switch (seq[1]) {
                        case 'A': return 1000;  /* Up */
                        case 'B': return 1001;  /* Down */
                        case 'C': return 1002;  /* Right */
                        case 'D': return 1003;  /* Left */
                        case 'H': return 1004;  /* Home */
                        case 'F': return 1005;  /* End */
                    }
                }
                if (seq_len == 3 && seq[0] == '[' && seq[2] == '~') {
                    switch (seq[1]) {
                        case '3': return 1006;  /* Delete */
                        case '5': return 1007;  /* Page Up */
                        case '6': return 1008;  /* Page Down */
                    }
                }
            }
        }

        return '\x1b';  /* Just escape */
    }

    return c;
}

static TrackerInputType translate_key(int key, uint32_t* modifiers) {
    *modifiers = TRACKER_MOD_NONE;

    /* Control characters */
    if (key >= 1 && key <= 26) {
        *modifiers = TRACKER_MOD_CTRL;
        switch (key) {
            case 3:  return TRACKER_INPUT_QUIT;         /* Ctrl-C */
            case 5:  return TRACKER_INPUT_EXPORT_MIDI;  /* Ctrl-E */
            case 7:  return TRACKER_INPUT_PANIC;        /* Ctrl-G */
            case 15: return TRACKER_INPUT_OPEN;         /* Ctrl-O */
            case 16: return TRACKER_INPUT_PLAY_TOGGLE;  /* Ctrl-P */
            case 18: return TRACKER_INPUT_RECORD_TOGGLE; /* Ctrl-R */
            case 19: return TRACKER_INPUT_SAVE;         /* Ctrl-S */
            case 25: return TRACKER_INPUT_REDO;         /* Ctrl-Y */
            case 26: return TRACKER_INPUT_UNDO;         /* Ctrl-Z */
        }
    }

    /* Special keys */
    switch (key) {
        case '\r':
        case '\n':   return TRACKER_INPUT_ENTER_EDIT;
        case '\x1b': return TRACKER_INPUT_CANCEL;  /* Escape - cancel in edit, quit in nav */
        case 127:
        case '\b':   return TRACKER_INPUT_BACKSPACE;
        case 1000:   return TRACKER_INPUT_CURSOR_UP;
        case 1001:   return TRACKER_INPUT_CURSOR_DOWN;
        case 1002:   return TRACKER_INPUT_CURSOR_RIGHT;
        case 1003:   return TRACKER_INPUT_CURSOR_LEFT;
        case 1004:   return TRACKER_INPUT_HOME;
        case 1005:   return TRACKER_INPUT_END;
        case 1006:   return TRACKER_INPUT_DELETE;
        case 1007:   return TRACKER_INPUT_PAGE_UP;
        case 1008:   return TRACKER_INPUT_PAGE_DOWN;
    }

    /* Vim-style navigation */
    switch (key) {
        case 'h': return TRACKER_INPUT_CURSOR_LEFT;
        case 'j': return TRACKER_INPUT_CURSOR_DOWN;
        case 'k': return TRACKER_INPUT_CURSOR_UP;
        case 'l': return TRACKER_INPUT_CURSOR_RIGHT;
        case 'g': return TRACKER_INPUT_PATTERN_START;
        case 'G': return TRACKER_INPUT_PATTERN_END;
        case ':': return TRACKER_INPUT_COMMAND_MODE;
        case ' ': return TRACKER_INPUT_PLAY_TOGGLE;
        case 'i': return TRACKER_INPUT_ENTER_EDIT;
        case 'x': return TRACKER_INPUT_CLEAR_CELL;
        case 'y': return TRACKER_INPUT_COPY;
        case 'd': return TRACKER_INPUT_CUT;
        case 'p': return TRACKER_INPUT_PASTE;
        case 'u': return TRACKER_INPUT_UNDO;
        case 'R': return TRACKER_INPUT_REDO;
        case 'v': return TRACKER_INPUT_SELECT_START;
        case 'm': return TRACKER_INPUT_MUTE_TRACK;
        case 'S': return TRACKER_INPUT_SOLO_TRACK;
        case 'T': return TRACKER_INPUT_CYCLE_THEME;
        case 'q': return TRACKER_INPUT_QUIT;
        case 'Q': return TRACKER_INPUT_QUIT;

        /* Pattern management */
        case '[': return TRACKER_INPUT_PREV_PATTERN;
        case ']': return TRACKER_INPUT_NEXT_PATTERN;
        case 'n': return TRACKER_INPUT_NEW_PATTERN;
        case 'D': return TRACKER_INPUT_DELETE_PATTERN;
        case 'c': return TRACKER_INPUT_CLONE_PATTERN;

        /* Row operations */
        case 'o': return TRACKER_INPUT_INSERT_ROW;
        case 'O': return TRACKER_INPUT_DUPLICATE_ROW;
        case 'X': return TRACKER_INPUT_DELETE_ROW;

        /* Track operations */
        case 'a': return TRACKER_INPUT_ADD_TRACK;
        case 'A': return TRACKER_INPUT_DELETE_TRACK;

        /* View/settings */
        case '?': return TRACKER_INPUT_MODE_HELP;
        case 'f': return TRACKER_INPUT_FOLLOW_TOGGLE;
        case '+':
        case '=': return TRACKER_INPUT_STEP_INC;
        case '-':
        case '_': return TRACKER_INPUT_STEP_DEC;
        case '>':
        case '.': return TRACKER_INPUT_OCTAVE_INC;
        case '<':
        case ',': return TRACKER_INPUT_OCTAVE_DEC;

        /* Tempo and loop */
        case '}': return TRACKER_INPUT_BPM_INC;
        case '{': return TRACKER_INPUT_BPM_DEC;
        case 'L': return TRACKER_INPUT_LOOP_TOGGLE;
        case 'P': return TRACKER_INPUT_PLAY_MODE_TOGGLE;

        /* Export */
        case 'E': return TRACKER_INPUT_EXPORT_MIDI;

        /* Arrange mode */
        case 'r': return TRACKER_INPUT_MODE_ARRANGE;

        /* Sequence operations (work in arrange mode) */
        case 'K': return TRACKER_INPUT_SEQ_MOVE_UP;
        case 'J': return TRACKER_INPUT_SEQ_MOVE_DOWN;
    }

    /* Printable character */
    if (key >= 32 && key < 127) {
        return TRACKER_INPUT_CHAR;
    }

    return TRACKER_INPUT_COUNT;  /* Unknown */
}

static bool terminal_poll_input(TrackerView* view, int timeout_ms,
                                TrackerInputEvent* out) {
    TerminalBackend* tb = (TerminalBackend*)view->callbacks.backend_data;

    int key = read_key(tb, timeout_ms);
    if (key < 0) return false;

    uint32_t modifiers;
    TrackerInputType type = translate_key(key, &modifiers);

    out->type = type;
    out->modifiers = modifiers;
    out->character = (type == TRACKER_INPUT_CHAR) ? key : 0;
    out->repeat_count = 1;

    return true;
}

/*============================================================================
 * Other Callbacks
 *============================================================================*/

static void terminal_get_dimensions(TrackerView* view, int* out_w, int* out_h) {
    TerminalBackend* tb = (TerminalBackend*)view->callbacks.backend_data;
    update_terminal_size(tb);
    *out_w = tb->screen_cols;
    *out_h = tb->screen_rows;
}

static void terminal_show_message(TrackerView* view, const char* msg) {
    free(view->state.status_message);
    view->state.status_message = msg ? strdup(msg) : NULL;
    tracker_view_invalidate_status(view);
}

static void terminal_show_error(TrackerView* view, const char* msg) {
    free(view->state.error_message);
    view->state.error_message = msg ? strdup(msg) : NULL;
    tracker_view_invalidate_status(view);
}

static bool terminal_prompt_input(TrackerView* view, const char* prompt,
                                  const char* default_val, char* out, int size) {
    TerminalBackend* tb = (TerminalBackend*)view->callbacks.backend_data;

    /* Show prompt on status line */
    output_printf(tb, CSI "%d;%dH", tb->screen_rows, 1);
    output_write(tb, LINE_CLEAR, -1);
    output_printf(tb, "%s", prompt);

    if (default_val) {
        strncpy(out, default_val, size - 1);
        out[size - 1] = '\0';
        output_printf(tb, "%s", out);
    } else {
        out[0] = '\0';
    }

    output_write(tb, CURSOR_SHOW, -1);
    output_flush(tb);

    /* Simple line editing */
    int pos = strlen(out);
    while (1) {
        int key = read_key(tb, -1);
        if (key < 0) continue;

        if (key == '\r' || key == '\n') {
            output_write(tb, CURSOR_HIDE, -1);
            output_flush(tb);
            return true;
        }
        if (key == '\x1b') {
            output_write(tb, CURSOR_HIDE, -1);
            output_flush(tb);
            return false;
        }
        if (key == 127 || key == '\b') {
            if (pos > 0) {
                pos--;
                out[pos] = '\0';
                output_write(tb, "\b \b", -1);
                output_flush(tb);
            }
            continue;
        }
        if (key >= 32 && key < 127 && pos < size - 1) {
            out[pos++] = key;
            out[pos] = '\0';
            char c = key;
            output_write(tb, &c, 1);
            output_flush(tb);
        }
    }
}

static bool terminal_prompt_confirm(TrackerView* view, const char* msg) {
    char response[8];
    char prompt[256];
    snprintf(prompt, sizeof(prompt), "%s [y/n]: ", msg);

    if (!terminal_prompt_input(view, prompt, NULL, response, sizeof(response))) {
        return false;
    }

    return response[0] == 'y' || response[0] == 'Y';
}

static void terminal_beep(TrackerView* view) {
    TerminalBackend* tb = (TerminalBackend*)view->callbacks.backend_data;
    output_write(tb, "\a", 1);
    output_flush(tb);
}

/*============================================================================
 * Public Utility Functions
 *============================================================================*/

void tracker_view_terminal_get_size(TrackerView* view, int* out_cols, int* out_rows) {
    terminal_get_dimensions(view, out_cols, out_rows);
}

void tracker_view_terminal_update_size(TrackerView* view) {
    TerminalBackend* tb = (TerminalBackend*)view->callbacks.backend_data;
    update_terminal_size(tb);
}

bool tracker_view_terminal_has_colors(TrackerView* view) {
    TerminalBackend* tb = (TerminalBackend*)view->callbacks.backend_data;
    return tb->config.use_colors;
}

bool tracker_view_terminal_has_256_colors(TrackerView* view) {
    TerminalBackend* tb = (TerminalBackend*)view->callbacks.backend_data;
    return tb->config.use_256_colors;
}

bool tracker_view_terminal_has_true_color(TrackerView* view) {
    TerminalBackend* tb = (TerminalBackend*)view->callbacks.backend_data;
    return tb->config.use_true_color;
}

const TrackerTerminalLayout* tracker_view_terminal_get_layout(TrackerView* view) {
    TerminalBackend* tb = (TerminalBackend*)view->callbacks.backend_data;
    calculate_layout(view);
    return &tb->layout;
}

char* tracker_view_terminal_render_to_string(TrackerView* view, int width, int height) {
    TerminalBackend* tb = (TerminalBackend*)view->callbacks.backend_data;

    /* Set up string rendering mode */
    tb->screen_cols = width;
    tb->screen_rows = height;
    tb->layout_dirty = true;

    tb->render_target_capacity = width * height * 4;  /* worst case with escapes */
    tb->render_target = malloc(tb->render_target_capacity);
    if (!tb->render_target) return NULL;

    /* Render */
    terminal_render(view);

    /* Return result */
    char* result = tb->render_target;
    tb->render_target = NULL;

    return result;
}

void tracker_view_terminal_inject_key(TrackerView* view, const char* key) {
    TerminalBackend* tb = (TerminalBackend*)view->callbacks.backend_data;

    free(tb->injected_input);
    tb->injected_input = strdup(key);
    tb->injected_input_len = strlen(key);
    tb->injected_input_pos = 0;
}
