/**
 * tracker_view_terminal.h - Terminal backend for tracker view
 *
 * Implements TrackerViewCallbacks for VT100-compatible terminals.
 * Renders the tracker grid, handles keyboard input, and manages
 * terminal state (raw mode, alternate screen buffer).
 *
 * Usage:
 *   TrackerView* view = tracker_view_terminal_new();
 *   tracker_view_attach(view, song, engine);
 *   tracker_view_run(view, 30);  // 30 FPS
 *   tracker_view_free(view);
 */

#ifndef TRACKER_VIEW_TERMINAL_H
#define TRACKER_VIEW_TERMINAL_H

#include "tracker_view.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Terminal View Creation
 *============================================================================*/

/**
 * Create a new terminal-based tracker view.
 *
 * Initializes terminal callbacks but does not enter raw mode yet.
 * Call tracker_view_run() to start the UI.
 *
 * @return  New view, or NULL on failure
 */
TrackerView* tracker_view_terminal_new(void);

/**
 * Create terminal view with custom file descriptors.
 *
 * Useful for testing or redirecting I/O.
 *
 * @param input_fd   File descriptor for input (default: STDIN_FILENO)
 * @param output_fd  File descriptor for output (default: STDOUT_FILENO)
 * @return           New view, or NULL on failure
 */
TrackerView* tracker_view_terminal_new_with_fds(int input_fd, int output_fd);

/*============================================================================
 * Terminal Configuration
 *============================================================================*/

/**
 * Terminal view configuration options.
 */
typedef struct {
    int min_track_width;      /* minimum column width for tracks (default: 10) */
    int max_track_width;      /* maximum column width for tracks (default: 20) */
    int row_number_width;     /* width of row number column (default: 4) */
    bool use_unicode_borders; /* use Unicode box-drawing chars (default: true) */
    bool use_colors;          /* use ANSI colors (default: true) */
    bool use_256_colors;      /* use 256-color mode (default: true) */
    bool use_true_color;      /* use 24-bit color (default: false) */
    bool alternate_screen;    /* use alternate screen buffer (default: true) */
    bool mouse_support;       /* enable mouse input (default: false) */
    int frame_rate;           /* target frame rate (default: 30) */
} TrackerTerminalConfig;

/**
 * Get default terminal configuration.
 */
void tracker_terminal_config_init(TrackerTerminalConfig* config);

/**
 * Create terminal view with configuration.
 */
TrackerView* tracker_view_terminal_new_with_config(const TrackerTerminalConfig* config);

/*============================================================================
 * Terminal State Access
 *============================================================================*/

/**
 * Get terminal dimensions.
 *
 * @param view        The terminal view
 * @param out_cols    Output: terminal width in columns
 * @param out_rows    Output: terminal height in rows
 */
void tracker_view_terminal_get_size(TrackerView* view, int* out_cols, int* out_rows);

/**
 * Force terminal size update.
 *
 * Call after receiving SIGWINCH or when terminal may have resized.
 */
void tracker_view_terminal_update_size(TrackerView* view);

/**
 * Check if terminal supports colors.
 */
bool tracker_view_terminal_has_colors(TrackerView* view);

/**
 * Check if terminal supports 256 colors.
 */
bool tracker_view_terminal_has_256_colors(TrackerView* view);

/**
 * Check if terminal supports true color (24-bit).
 */
bool tracker_view_terminal_has_true_color(TrackerView* view);

/*============================================================================
 * Layout Information
 *============================================================================*/

/**
 * Layout metrics for rendering.
 */
typedef struct {
    /* Screen dimensions */
    int screen_cols;
    int screen_rows;

    /* Grid area */
    int grid_start_col;       /* column where grid starts */
    int grid_start_row;       /* row where grid starts */
    int grid_cols;            /* width of grid area */
    int grid_rows;            /* height of grid area (visible rows) */

    /* Track columns */
    int row_num_width;        /* width of row number column */
    int* track_widths;        /* width of each visible track */
    int track_count;          /* number of visible tracks */
    int track_start;          /* first visible track index */

    /* Row range */
    int row_start;            /* first visible row index */
    int row_count;            /* number of visible rows */

    /* Header/footer */
    int header_rows;          /* rows used by header */
    int footer_rows;          /* rows used by footer/status */
    int status_row;           /* row for status line */
    int command_row;          /* row for command input */
} TrackerTerminalLayout;

/**
 * Get current layout metrics.
 *
 * Layout is recalculated when terminal resizes or scroll changes.
 */
const TrackerTerminalLayout* tracker_view_terminal_get_layout(TrackerView* view);

/*============================================================================
 * Debugging / Testing
 *============================================================================*/

/**
 * Render to a string buffer instead of terminal.
 *
 * Useful for testing. Returns heap-allocated string.
 *
 * @param view    The terminal view
 * @param width   Virtual terminal width
 * @param height  Virtual terminal height
 * @return        Rendered output (caller must free)
 */
char* tracker_view_terminal_render_to_string(TrackerView* view, int width, int height);

/**
 * Simulate key input for testing.
 *
 * @param view  The terminal view
 * @param key   Key string (e.g., "j", "k", "\x1b[A" for up arrow)
 */
void tracker_view_terminal_inject_key(TrackerView* view, const char* key);

#ifdef __cplusplus
}
#endif

#endif /* TRACKER_VIEW_TERMINAL_H */
