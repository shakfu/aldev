/**
 * tracker_view.h - Abstract view interface for tracker UI
 *
 * This defines the interface between the tracker model/engine and UI rendering.
 * Different backends (ncurses, web, GUI) implement this interface.
 *
 * The view is responsible for:
 *   - Rendering the pattern grid, track headers, transport status
 *   - Handling user input and translating to commands
 *   - Managing cursor position, selection, and scroll state
 *   - Visual feedback (playing position, active notes, errors)
 *   - Undo/redo management
 *
 * The view does NOT own the model or engine - it receives pointers to them.
 */

#ifndef TRACKER_VIEW_H
#define TRACKER_VIEW_H

#include "tracker_model.h"
#include "tracker_engine.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

/*============================================================================
 * Forward Declarations
 *============================================================================*/

typedef struct TrackerView TrackerView;
typedef struct TrackerViewState TrackerViewState;
typedef struct TrackerSelection TrackerSelection;
typedef struct TrackerClipboard TrackerClipboard;
typedef struct TrackerViewCallbacks TrackerViewCallbacks;
typedef struct TrackerTheme TrackerTheme;
typedef struct TrackerUndoAction TrackerUndoAction;
typedef struct TrackerUndoStack TrackerUndoStack;

/*============================================================================
 * Color System
 *============================================================================*/

/**
 * Color representation - supports both indexed (terminal) and RGB (web/GUI).
 */
typedef struct {
    enum {
        TRACKER_COLOR_DEFAULT,    /* use terminal/system default */
        TRACKER_COLOR_INDEXED,    /* 0-255 indexed color */
        TRACKER_COLOR_RGB,        /* 24-bit RGB */
    } type;
    union {
        uint8_t index;            /* for INDEXED */
        struct {
            uint8_t r, g, b;
        } rgb;                    /* for RGB */
    } value;
} TrackerColor;

/**
 * Text attributes.
 */
typedef enum {
    TRACKER_ATTR_NONE      = 0,
    TRACKER_ATTR_BOLD      = 1 << 0,
    TRACKER_ATTR_DIM       = 1 << 1,
    TRACKER_ATTR_ITALIC    = 1 << 2,
    TRACKER_ATTR_UNDERLINE = 1 << 3,
    TRACKER_ATTR_BLINK     = 1 << 4,
    TRACKER_ATTR_REVERSE   = 1 << 5,
    TRACKER_ATTR_STRIKE    = 1 << 6,
} TrackerTextAttr;

/**
 * A style combining foreground, background, and attributes.
 */
typedef struct {
    TrackerColor fg;
    TrackerColor bg;
    uint8_t attr;                 /* TrackerTextAttr bitfield */
} TrackerStyle;

/**
 * Theme - complete color/style configuration.
 */
struct TrackerTheme {
    const char* name;
    const char* author;

    /* Base colors */
    TrackerStyle default_style;   /* default text */
    TrackerStyle header_style;    /* track headers, row numbers */
    TrackerStyle status_style;    /* status line */
    TrackerStyle command_style;   /* command line */
    TrackerStyle error_style;     /* error messages */
    TrackerStyle message_style;   /* status messages */

    /* Grid colors */
    TrackerStyle cell_empty;      /* empty cell */
    TrackerStyle cell_note;       /* note content */
    TrackerStyle cell_fx;         /* FX content */
    TrackerStyle cell_off;        /* note-off marker */
    TrackerStyle cell_continuation; /* continuation marker */

    /* Cursor and selection */
    TrackerStyle cursor;          /* cursor position */
    TrackerStyle cursor_edit;     /* cursor in edit mode */
    TrackerStyle selection;       /* selected cells */
    TrackerStyle selection_cursor; /* cursor within selection */

    /* Playback */
    TrackerStyle playing_row;     /* currently playing row */
    TrackerStyle playing_cell;    /* cell being triggered */

    /* Row highlighting */
    TrackerStyle row_beat;        /* beat boundary rows */
    TrackerStyle row_bar;         /* bar boundary rows */
    TrackerStyle row_alternate;   /* alternating row background */

    /* Track states */
    TrackerStyle track_muted;     /* muted track header */
    TrackerStyle track_solo;      /* soloed track header */
    TrackerStyle track_active;    /* track with active notes */

    /* Validation */
    TrackerStyle cell_error;      /* cell with syntax error */
    TrackerStyle cell_warning;    /* cell with warning */

    /* Active notes visualization */
    TrackerStyle note_active;     /* currently sounding note */
    TrackerStyle note_velocity[4]; /* velocity gradient (pp, p, f, ff) */

    /* Scrollbar (if applicable) */
    TrackerStyle scrollbar_track;
    TrackerStyle scrollbar_thumb;

    /* Borders and separators */
    TrackerColor border_color;
    TrackerColor separator_color;

    /* Characters for drawing (UTF-8) */
    const char* border_h;         /* horizontal border */
    const char* border_v;         /* vertical border */
    const char* border_corner_tl;
    const char* border_corner_tr;
    const char* border_corner_bl;
    const char* border_corner_br;
    const char* border_t;         /* T-junction top */
    const char* border_b;         /* T-junction bottom */
    const char* border_l;         /* T-junction left */
    const char* border_r;         /* T-junction right */
    const char* border_cross;     /* cross junction */
    const char* note_off_marker;  /* e.g., "===" or "OFF" */
    const char* continuation_marker; /* e.g., "..." or "|" */
    const char* empty_cell;       /* e.g., "---" or "..." */
};

/*============================================================================
 * Built-in Themes
 *============================================================================*/

/**
 * Get a built-in theme by name.
 * @param name  Theme name ("default", "dark", "light", "retro", etc.)
 * @return      Theme pointer, or NULL if not found
 */
const TrackerTheme* tracker_theme_get(const char* name);

/**
 * List available built-in themes.
 * @param count  Output: number of themes
 * @return       Array of theme names
 */
const char** tracker_theme_list(int* count);

/**
 * Create default theme.
 */
void tracker_theme_init_default(TrackerTheme* theme);

/**
 * Clone a theme.
 */
TrackerTheme* tracker_theme_clone(const TrackerTheme* theme);

/**
 * Free a cloned theme.
 */
void tracker_theme_free(TrackerTheme* theme);

/*============================================================================
 * Color Helpers
 *============================================================================*/

static inline TrackerColor tracker_color_default(void) {
    return (TrackerColor){ .type = TRACKER_COLOR_DEFAULT };
}

static inline TrackerColor tracker_color_indexed(uint8_t index) {
    return (TrackerColor){ .type = TRACKER_COLOR_INDEXED, .value.index = index };
}

static inline TrackerColor tracker_color_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (TrackerColor){ .type = TRACKER_COLOR_RGB, .value.rgb = {r, g, b} };
}

static inline TrackerColor tracker_color_hex(uint32_t hex) {
    return tracker_color_rgb((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
}

static inline TrackerStyle tracker_style(TrackerColor fg, TrackerColor bg, uint8_t attr) {
    return (TrackerStyle){ .fg = fg, .bg = bg, .attr = attr };
}

/*============================================================================
 * Undo/Redo System
 *============================================================================*/

/**
 * Types of undoable actions.
 */
typedef enum {
    TRACKER_UNDO_CELL_EDIT,       /* single cell content change */
    TRACKER_UNDO_CELL_CLEAR,      /* cell cleared */
    TRACKER_UNDO_CELLS_CHANGE,    /* multiple cells changed */
    TRACKER_UNDO_ROW_INSERT,      /* row inserted */
    TRACKER_UNDO_ROW_DELETE,      /* row deleted */
    TRACKER_UNDO_ROW_DUPLICATE,   /* row duplicated */
    TRACKER_UNDO_ROWS_MOVE,       /* rows moved */
    TRACKER_UNDO_TRACK_ADD,       /* track added */
    TRACKER_UNDO_TRACK_DELETE,    /* track deleted */
    TRACKER_UNDO_TRACK_MOVE,      /* track moved */
    TRACKER_UNDO_PATTERN_ADD,     /* pattern added */
    TRACKER_UNDO_PATTERN_DELETE,  /* pattern deleted */
    TRACKER_UNDO_PATTERN_RESIZE,  /* pattern rows changed */
    TRACKER_UNDO_FX_CHAIN_CHANGE, /* FX chain modified */
    TRACKER_UNDO_SONG_SETTINGS,   /* song settings changed */
    TRACKER_UNDO_PASTE,           /* paste operation */
    TRACKER_UNDO_CUT,             /* cut operation */
    TRACKER_UNDO_GROUP_BEGIN,     /* start of compound action */
    TRACKER_UNDO_GROUP_END,       /* end of compound action */
} TrackerUndoType;

/**
 * Saved cell state for undo.
 */
typedef struct {
    int pattern;
    int track;
    int row;
    TrackerCellType type;
    char* expression;
    char* language_id;
    TrackerFxChain fx_chain;      /* deep copy */
} TrackerUndoCellState;

/**
 * An undoable action.
 */
struct TrackerUndoAction {
    TrackerUndoType type;

    /* Cursor position at time of action (for restore) */
    int cursor_pattern;
    int cursor_track;
    int cursor_row;

    /* Action-specific data */
    union {
        /* CELL_EDIT, CELL_CLEAR */
        struct {
            TrackerUndoCellState before;
            TrackerUndoCellState after;
        } cell;

        /* CELLS_CHANGE, PASTE, CUT */
        struct {
            TrackerUndoCellState* before;
            TrackerUndoCellState* after;
            int count;
            int start_track;
            int end_track;
            int start_row;
            int end_row;
        } cells;

        /* ROW_INSERT, ROW_DELETE, ROW_DUPLICATE */
        struct {
            int pattern;
            int row;
            TrackerUndoCellState* cells;  /* for delete: saved cells */
            int cell_count;
        } row;

        /* ROWS_MOVE */
        struct {
            int pattern;
            int from_row;
            int to_row;
            int count;
        } rows_move;

        /* TRACK_ADD, TRACK_DELETE */
        struct {
            int pattern;
            int track;
            char* name;
            uint8_t channel;
            TrackerUndoCellState* cells;  /* for delete: saved cells */
            int cell_count;
            TrackerFxChain fx_chain;
        } track;

        /* TRACK_MOVE */
        struct {
            int pattern;
            int from_track;
            int to_track;
        } track_move;

        /* PATTERN_ADD, PATTERN_DELETE */
        struct {
            int index;
            TrackerPattern* pattern;      /* for delete: saved pattern */
        } pattern;

        /* PATTERN_RESIZE */
        struct {
            int pattern;
            int old_rows;
            int new_rows;
            TrackerUndoCellState* truncated; /* cells lost in resize */
            int truncated_count;
        } resize;

        /* FX_CHAIN_CHANGE */
        struct {
            enum { FX_CELL, FX_TRACK, FX_MASTER } level;
            int pattern;
            int track;
            int row;
            TrackerFxChain before;
            TrackerFxChain after;
        } fx;

        /* SONG_SETTINGS */
        struct {
            int old_bpm;
            int new_bpm;
            int old_rpb;
            int new_rpb;
            int old_tpr;
            int new_tpr;
            TrackerSpilloverMode old_spillover;
            TrackerSpilloverMode new_spillover;
        } settings;

        /* GROUP_BEGIN */
        struct {
            char* description;
        } group;
    } data;

    TrackerUndoAction* next;      /* for stack linking */
};

/**
 * Undo/redo stack.
 */
struct TrackerUndoStack {
    TrackerUndoAction* undo_head;
    TrackerUndoAction* redo_head;
    int undo_count;
    int redo_count;
    int max_undo;                 /* limit, 0 = unlimited */
    int group_depth;              /* for nested groups */
    bool in_undo;                 /* flag to prevent recording during undo */
};

/*============================================================================
 * Undo/Redo Functions
 *============================================================================*/

/**
 * Initialize undo stack.
 */
void tracker_undo_init(TrackerUndoStack* stack, int max_undo);

/**
 * Cleanup undo stack.
 */
void tracker_undo_cleanup(TrackerUndoStack* stack);

/**
 * Clear all undo/redo history.
 */
void tracker_undo_clear(TrackerUndoStack* stack);

/**
 * Record an action for undo.
 * Clears redo stack.
 */
void tracker_undo_record(TrackerUndoStack* stack, TrackerUndoAction* action);

/**
 * Begin a compound action group.
 * All actions until group_end are undone/redone together.
 */
void tracker_undo_group_begin(TrackerUndoStack* stack, const char* description);

/**
 * End a compound action group.
 */
void tracker_undo_group_end(TrackerUndoStack* stack);

/**
 * Check if undo is available.
 */
bool tracker_undo_can_undo(TrackerUndoStack* stack);

/**
 * Check if redo is available.
 */
bool tracker_undo_can_redo(TrackerUndoStack* stack);

/**
 * Get description of next undo action.
 */
const char* tracker_undo_get_undo_description(TrackerUndoStack* stack);

/**
 * Get description of next redo action.
 */
const char* tracker_undo_get_redo_description(TrackerUndoStack* stack);

/**
 * Perform undo.
 * @param view  View to update cursor position
 * @param song  Song to modify
 * @return      true if undo was performed
 */
bool tracker_undo_undo(TrackerUndoStack* stack, TrackerView* view, TrackerSong* song);

/**
 * Perform redo.
 */
bool tracker_undo_redo(TrackerUndoStack* stack, TrackerView* view, TrackerSong* song);

/**
 * Free an undo action.
 */
void tracker_undo_action_free(TrackerUndoAction* action);

/*============================================================================
 * Convenience Functions for Recording Actions
 *============================================================================*/

/**
 * Record a cell edit.
 */
void tracker_undo_record_cell_edit(
    TrackerUndoStack* stack,
    TrackerView* view,
    int pattern, int track, int row,
    const TrackerCell* old_cell,
    const TrackerCell* new_cell
);

/**
 * Record multiple cell changes.
 */
void tracker_undo_record_cells_change(
    TrackerUndoStack* stack,
    TrackerView* view,
    int pattern,
    int start_track, int end_track,
    int start_row, int end_row,
    TrackerCell** old_cells,
    TrackerCell** new_cells
);

/**
 * Record row insertion.
 */
void tracker_undo_record_row_insert(
    TrackerUndoStack* stack,
    TrackerView* view,
    int pattern, int row
);

/**
 * Record row deletion.
 */
void tracker_undo_record_row_delete(
    TrackerUndoStack* stack,
    TrackerView* view,
    int pattern, int row,
    const TrackerCell* deleted_cells,
    int cell_count
);

/*============================================================================
 * View Modes
 *============================================================================*/

typedef enum {
    TRACKER_VIEW_MODE_PATTERN,    /* pattern editor (main grid) */
    TRACKER_VIEW_MODE_ARRANGE,    /* arrangement/sequence view */
    TRACKER_VIEW_MODE_MIXER,      /* track mixer (levels, FX) */
    TRACKER_VIEW_MODE_INSTRUMENT, /* instrument/plugin editor */
    TRACKER_VIEW_MODE_SONG,       /* song settings */
    TRACKER_VIEW_MODE_HELP,       /* help/documentation */
} TrackerViewMode;

typedef enum {
    TRACKER_EDIT_MODE_NAVIGATE,   /* cursor movement only */
    TRACKER_EDIT_MODE_EDIT,       /* editing cell content */
    TRACKER_EDIT_MODE_SELECT,     /* selection mode */
    TRACKER_EDIT_MODE_COMMAND,    /* command line input */
} TrackerEditMode;

/*============================================================================
 * Input Events
 *============================================================================*/

typedef enum {
    /* Navigation */
    TRACKER_INPUT_CURSOR_UP,
    TRACKER_INPUT_CURSOR_DOWN,
    TRACKER_INPUT_CURSOR_LEFT,
    TRACKER_INPUT_CURSOR_RIGHT,
    TRACKER_INPUT_PAGE_UP,
    TRACKER_INPUT_PAGE_DOWN,
    TRACKER_INPUT_HOME,
    TRACKER_INPUT_END,
    TRACKER_INPUT_PATTERN_START,
    TRACKER_INPUT_PATTERN_END,
    TRACKER_INPUT_NEXT_TRACK,
    TRACKER_INPUT_PREV_TRACK,
    TRACKER_INPUT_NEXT_PATTERN,
    TRACKER_INPUT_PREV_PATTERN,

    /* Editing */
    TRACKER_INPUT_ENTER_EDIT,
    TRACKER_INPUT_EXIT_EDIT,
    TRACKER_INPUT_CANCEL,
    TRACKER_INPUT_BACKSPACE,
    TRACKER_INPUT_DELETE,
    TRACKER_INPUT_CLEAR_CELL,
    TRACKER_INPUT_INSERT_ROW,
    TRACKER_INPUT_DELETE_ROW,
    TRACKER_INPUT_DUPLICATE_ROW,

    /* Selection */
    TRACKER_INPUT_SELECT_START,
    TRACKER_INPUT_SELECT_ALL,
    TRACKER_INPUT_SELECT_TRACK,
    TRACKER_INPUT_SELECT_ROW,
    TRACKER_INPUT_SELECT_PATTERN,

    /* Clipboard */
    TRACKER_INPUT_CUT,
    TRACKER_INPUT_COPY,
    TRACKER_INPUT_PASTE,
    TRACKER_INPUT_PASTE_INSERT,

    /* Transport */
    TRACKER_INPUT_PLAY,
    TRACKER_INPUT_STOP,
    TRACKER_INPUT_PAUSE,
    TRACKER_INPUT_PLAY_TOGGLE,
    TRACKER_INPUT_PLAY_FROM_START,
    TRACKER_INPUT_PLAY_FROM_CURSOR,
    TRACKER_INPUT_PLAY_ROW,
    TRACKER_INPUT_RECORD_TOGGLE,

    /* Pattern/Song */
    TRACKER_INPUT_NEW_PATTERN,
    TRACKER_INPUT_CLONE_PATTERN,
    TRACKER_INPUT_DELETE_PATTERN,
    TRACKER_INPUT_ADD_TRACK,
    TRACKER_INPUT_DELETE_TRACK,

    /* Track control */
    TRACKER_INPUT_MUTE_TRACK,
    TRACKER_INPUT_SOLO_TRACK,
    TRACKER_INPUT_TRACK_FX,

    /* View */
    TRACKER_INPUT_MODE_PATTERN,
    TRACKER_INPUT_MODE_ARRANGE,
    TRACKER_INPUT_MODE_MIXER,
    TRACKER_INPUT_MODE_HELP,
    TRACKER_INPUT_ZOOM_IN,
    TRACKER_INPUT_ZOOM_OUT,
    TRACKER_INPUT_FOLLOW_TOGGLE,

    /* Undo/Redo */
    TRACKER_INPUT_UNDO,
    TRACKER_INPUT_REDO,

    /* File */
    TRACKER_INPUT_SAVE,
    TRACKER_INPUT_SAVE_AS,
    TRACKER_INPUT_OPEN,
    TRACKER_INPUT_EXPORT_MIDI,

    /* Misc */
    TRACKER_INPUT_COMMAND_MODE,
    TRACKER_INPUT_QUIT,
    TRACKER_INPUT_PANIC,

    /* Text input */
    TRACKER_INPUT_CHAR,

    TRACKER_INPUT_COUNT
} TrackerInputType;

typedef enum {
    TRACKER_MOD_NONE  = 0,
    TRACKER_MOD_SHIFT = 1 << 0,
    TRACKER_MOD_CTRL  = 1 << 1,
    TRACKER_MOD_ALT   = 1 << 2,
    TRACKER_MOD_META  = 1 << 3,
} TrackerInputModifiers;

typedef struct {
    TrackerInputType type;
    uint32_t modifiers;
    uint32_t character;
    int repeat_count;
} TrackerInputEvent;

/*============================================================================
 * Selection
 *============================================================================*/

typedef enum {
    TRACKER_SEL_NONE,
    TRACKER_SEL_CELL,
    TRACKER_SEL_RANGE,
    TRACKER_SEL_TRACK,
    TRACKER_SEL_ROW,
    TRACKER_SEL_PATTERN,
} TrackerSelectionType;

struct TrackerSelection {
    TrackerSelectionType type;
    int anchor_track;
    int anchor_row;
    int start_track;
    int end_track;
    int start_row;
    int end_row;
    int start_pattern;
    int end_pattern;
};

/*============================================================================
 * Clipboard
 *============================================================================*/

struct TrackerClipboard {
    TrackerCell* cells;
    int width;
    int height;
    bool owns_cells;
};

/*============================================================================
 * View State
 *============================================================================*/

struct TrackerViewState {
    /* Current mode */
    TrackerViewMode view_mode;
    TrackerEditMode edit_mode;

    /* Cursor position */
    int cursor_pattern;
    int cursor_track;
    int cursor_row;

    /* Selection */
    TrackerSelection selection;
    bool selecting;

    /* Scroll position */
    int scroll_track;
    int scroll_row;
    int visible_tracks;
    int visible_rows;

    /* Edit buffer */
    char* edit_buffer;
    int edit_buffer_len;
    int edit_buffer_capacity;
    int edit_cursor_pos;

    /* Command line */
    char* command_buffer;
    int command_buffer_len;
    int command_buffer_capacity;
    int command_cursor_pos;

    /* Display options */
    bool follow_playback;
    bool show_row_numbers;
    bool show_track_headers;
    bool show_transport;
    bool show_status_line;
    bool highlight_current_row;
    bool highlight_beat_rows;
    int beat_highlight_interval;

    /* Theme */
    TrackerTheme* theme;
    bool owns_theme;

    /* Track column widths */
    int* track_widths;
    int track_widths_count;

    /* Playback position */
    int playback_pattern;
    int playback_row;
    bool is_playing;

    /* Error display */
    char* error_message;
    int error_track;
    int error_row;
    double error_display_time;

    /* Status message */
    char* status_message;
    double status_display_time;
};

/*============================================================================
 * Dirty Flags
 *============================================================================*/

typedef enum {
    TRACKER_DIRTY_NONE           = 0,
    TRACKER_DIRTY_CELL           = 1 << 0,
    TRACKER_DIRTY_ROW            = 1 << 1,
    TRACKER_DIRTY_TRACK          = 1 << 2,
    TRACKER_DIRTY_PATTERN        = 1 << 3,
    TRACKER_DIRTY_CURSOR         = 1 << 4,
    TRACKER_DIRTY_SELECTION      = 1 << 5,
    TRACKER_DIRTY_SCROLL         = 1 << 6,
    TRACKER_DIRTY_PLAYBACK       = 1 << 7,
    TRACKER_DIRTY_STATUS         = 1 << 8,
    TRACKER_DIRTY_HEADER         = 1 << 9,
    TRACKER_DIRTY_ALL            = 0xFFFF,
} TrackerDirtyFlags;

/*============================================================================
 * View Callbacks
 *============================================================================*/

struct TrackerViewCallbacks {
    bool (*init)(TrackerView* view);
    void (*cleanup)(TrackerView* view);
    void (*render)(TrackerView* view);
    void (*render_incremental)(TrackerView* view, uint32_t dirty_flags);
    bool (*poll_input)(TrackerView* view, int timeout_ms, TrackerInputEvent* out_event);
    void (*get_dimensions)(TrackerView* view, int* out_width, int* out_height);
    void (*show_message)(TrackerView* view, const char* message);
    void (*show_error)(TrackerView* view, const char* message);
    bool (*prompt_input)(TrackerView* view, const char* prompt,
                         const char* default_val, char* out_buffer, int buffer_size);
    bool (*prompt_confirm)(TrackerView* view, const char* message);
    void (*beep)(TrackerView* view);
    void* backend_data;
};

/*============================================================================
 * View Structure
 *============================================================================*/

struct TrackerView {
    TrackerViewCallbacks callbacks;
    TrackerSong* song;
    TrackerEngine* engine;
    TrackerViewState state;
    uint32_t dirty_flags;
    int dirty_track;
    int dirty_row;
    int dirty_cell_track;
    int dirty_cell_row;
    TrackerClipboard clipboard;
    TrackerUndoStack undo;
    void* keybindings;
    void* user_data;
    bool quit_requested;
};

/*============================================================================
 * Lifecycle Functions
 *============================================================================*/

TrackerView* tracker_view_new(const TrackerViewCallbacks* callbacks);
void tracker_view_free(TrackerView* view);
void tracker_view_state_init(TrackerViewState* state);
void tracker_view_state_cleanup(TrackerViewState* state);
void tracker_view_attach(TrackerView* view, TrackerSong* song, TrackerEngine* engine);
void tracker_view_detach(TrackerView* view);

/*============================================================================
 * Theme Management
 *============================================================================*/

/**
 * Set view theme.
 * @param theme  Theme to use (view takes ownership if owns=true)
 * @param owns   Whether view should free theme on cleanup
 */
void tracker_view_set_theme(TrackerView* view, TrackerTheme* theme, bool owns);

/**
 * Set theme by name (uses built-in).
 */
bool tracker_view_set_theme_by_name(TrackerView* view, const char* name);

/**
 * Get current theme.
 */
const TrackerTheme* tracker_view_get_theme(TrackerView* view);

/*============================================================================
 * Rendering
 *============================================================================*/

void tracker_view_invalidate(TrackerView* view);
void tracker_view_invalidate_cell(TrackerView* view, int track, int row);
void tracker_view_invalidate_row(TrackerView* view, int row);
void tracker_view_invalidate_track(TrackerView* view, int track);
void tracker_view_invalidate_cursor(TrackerView* view);
void tracker_view_invalidate_selection(TrackerView* view);
void tracker_view_invalidate_status(TrackerView* view);
void tracker_view_render(TrackerView* view);
void tracker_view_update_playback(TrackerView* view, int pattern, int row);

/*============================================================================
 * Input Handling
 *============================================================================*/

bool tracker_view_poll_input(TrackerView* view, int timeout_ms);
bool tracker_view_handle_input(TrackerView* view, const TrackerInputEvent* event);
void tracker_view_edit_char(TrackerView* view, uint32_t character);
void tracker_view_edit_confirm(TrackerView* view);
void tracker_view_edit_cancel(TrackerView* view);

/*============================================================================
 * Cursor Movement
 *============================================================================*/

void tracker_view_cursor_up(TrackerView* view, int count);
void tracker_view_cursor_down(TrackerView* view, int count);
void tracker_view_cursor_left(TrackerView* view, int count);
void tracker_view_cursor_right(TrackerView* view, int count);
void tracker_view_cursor_page_up(TrackerView* view);
void tracker_view_cursor_page_down(TrackerView* view);
void tracker_view_cursor_home(TrackerView* view);
void tracker_view_cursor_end(TrackerView* view);
void tracker_view_cursor_pattern_start(TrackerView* view);
void tracker_view_cursor_pattern_end(TrackerView* view);
void tracker_view_cursor_goto(TrackerView* view, int pattern, int track, int row);
void tracker_view_ensure_visible(TrackerView* view);

/*============================================================================
 * Selection
 *============================================================================*/

void tracker_view_select_start(TrackerView* view);
void tracker_view_select_extend(TrackerView* view);
void tracker_view_select_clear(TrackerView* view);
void tracker_view_select_track(TrackerView* view);
void tracker_view_select_row(TrackerView* view);
void tracker_view_select_pattern(TrackerView* view);
void tracker_view_select_all(TrackerView* view);
bool tracker_view_is_selected(TrackerView* view, int track, int row);
bool tracker_view_get_selection(TrackerView* view,
                                int* out_start_track, int* out_end_track,
                                int* out_start_row, int* out_end_row);

/*============================================================================
 * Clipboard Operations
 *============================================================================*/

bool tracker_view_copy(TrackerView* view);
bool tracker_view_cut(TrackerView* view);
bool tracker_view_paste(TrackerView* view);
bool tracker_view_paste_insert(TrackerView* view);
void tracker_view_clipboard_clear(TrackerView* view);
bool tracker_view_clipboard_has_content(TrackerView* view);

/*============================================================================
 * Cell Operations
 *============================================================================*/

void tracker_view_clear_cell(TrackerView* view);
void tracker_view_clear_selection(TrackerView* view);
void tracker_view_insert_row(TrackerView* view);
void tracker_view_delete_row(TrackerView* view);
void tracker_view_duplicate_row(TrackerView* view);

/*============================================================================
 * Undo/Redo (View Integration)
 *============================================================================*/

/**
 * Undo last action.
 */
bool tracker_view_undo(TrackerView* view);

/**
 * Redo last undone action.
 */
bool tracker_view_redo(TrackerView* view);

/**
 * Begin recording a compound action.
 */
void tracker_view_begin_undo_group(TrackerView* view, const char* description);

/**
 * End recording a compound action.
 */
void tracker_view_end_undo_group(TrackerView* view);

/*============================================================================
 * Mode Switching
 *============================================================================*/

void tracker_view_set_mode(TrackerView* view, TrackerViewMode mode);
void tracker_view_enter_edit(TrackerView* view);
void tracker_view_exit_edit(TrackerView* view, bool confirm);
void tracker_view_enter_command(TrackerView* view);
void tracker_view_exit_command(TrackerView* view, bool execute);

/*============================================================================
 * Scroll Control
 *============================================================================*/

void tracker_view_scroll_to_row(TrackerView* view, int row);
void tracker_view_scroll_to_track(TrackerView* view, int track);
void tracker_view_scroll(TrackerView* view, int track_delta, int row_delta);
void tracker_view_set_follow(TrackerView* view, bool follow);

/*============================================================================
 * Messages and Status
 *============================================================================*/

void tracker_view_show_status(TrackerView* view, const char* format, ...);
void tracker_view_show_error(TrackerView* view, const char* format, ...);
void tracker_view_clear_messages(TrackerView* view);

/*============================================================================
 * Utility Functions
 *============================================================================*/

TrackerCell* tracker_view_get_cursor_cell(TrackerView* view);
TrackerPattern* tracker_view_get_current_pattern(TrackerView* view);
bool tracker_view_cursor_valid(TrackerView* view);
void tracker_view_clamp_cursor(TrackerView* view);
void tracker_view_get_visible_range(TrackerView* view,
                                    int* out_start_track, int* out_end_track,
                                    int* out_start_row, int* out_end_row);

/*============================================================================
 * Main Loop
 *============================================================================*/

void tracker_view_run(TrackerView* view, int frame_rate);
void tracker_view_request_quit(TrackerView* view);

/*============================================================================
 * JSON Serialization (for web view sync)
 *============================================================================*/

/**
 * JSON output callback - called with JSON fragments.
 */
typedef void (*TrackerJsonWriteFn)(void* user_data, const char* json, int len);

/**
 * JSON serialization context.
 */
typedef struct {
    TrackerJsonWriteFn write;
    void* user_data;
    int depth;
    bool pretty;
    int indent;
} TrackerJsonWriter;

/**
 * Initialize JSON writer.
 */
void tracker_json_writer_init(TrackerJsonWriter* w, TrackerJsonWriteFn write_fn,
                              void* user_data, bool pretty);

/*----------------------------------------------------------------------------
 * Model Serialization
 *----------------------------------------------------------------------------*/

/**
 * Serialize entire song to JSON.
 */
void tracker_json_write_song(TrackerJsonWriter* w, const TrackerSong* song);

/**
 * Serialize a pattern to JSON.
 */
void tracker_json_write_pattern(TrackerJsonWriter* w, const TrackerPattern* pattern);

/**
 * Serialize a track to JSON.
 */
void tracker_json_write_track(TrackerJsonWriter* w, const TrackerTrack* track, int num_rows);

/**
 * Serialize a cell to JSON.
 */
void tracker_json_write_cell(TrackerJsonWriter* w, const TrackerCell* cell);

/**
 * Serialize a phrase to JSON.
 */
void tracker_json_write_phrase(TrackerJsonWriter* w, const TrackerPhrase* phrase);

/**
 * Serialize an FX chain to JSON.
 */
void tracker_json_write_fx_chain(TrackerJsonWriter* w, const TrackerFxChain* chain);

/*----------------------------------------------------------------------------
 * View State Serialization
 *----------------------------------------------------------------------------*/

/**
 * Serialize view state to JSON (for client sync).
 */
void tracker_json_write_view_state(TrackerJsonWriter* w, const TrackerViewState* state);

/**
 * Serialize selection to JSON.
 */
void tracker_json_write_selection(TrackerJsonWriter* w, const TrackerSelection* sel);

/**
 * Serialize theme to JSON.
 */
void tracker_json_write_theme(TrackerJsonWriter* w, const TrackerTheme* theme);

/**
 * Serialize color to JSON.
 */
void tracker_json_write_color(TrackerJsonWriter* w, const TrackerColor* color);

/**
 * Serialize style to JSON.
 */
void tracker_json_write_style(TrackerJsonWriter* w, const TrackerStyle* style);

/*----------------------------------------------------------------------------
 * Engine State Serialization
 *----------------------------------------------------------------------------*/

/**
 * Serialize playback state to JSON (for client sync).
 */
void tracker_json_write_playback_state(TrackerJsonWriter* w, const TrackerEngine* engine);

/*----------------------------------------------------------------------------
 * Deserialization
 *----------------------------------------------------------------------------*/

/**
 * Parse a song from JSON string.
 * @param json       JSON string
 * @param len        Length of JSON string (-1 for null-terminated)
 * @param error_msg  Output: error message on failure
 * @return           Parsed song (caller owns), or NULL on error
 */
TrackerSong* tracker_json_parse_song(const char* json, int len, const char** error_msg);

/**
 * Parse a pattern from JSON string.
 */
TrackerPattern* tracker_json_parse_pattern(const char* json, int len, const char** error_msg);

/**
 * Parse view state from JSON string.
 * @param state  State struct to populate
 */
bool tracker_json_parse_view_state(TrackerViewState* state, const char* json,
                                   int len, const char** error_msg);

/**
 * Parse theme from JSON string.
 */
TrackerTheme* tracker_json_parse_theme(const char* json, int len, const char** error_msg);

/*----------------------------------------------------------------------------
 * Incremental Updates (for efficient web sync)
 *----------------------------------------------------------------------------*/

/**
 * Types of incremental updates.
 */
typedef enum {
    TRACKER_UPDATE_CELL,          /* single cell changed */
    TRACKER_UPDATE_ROW,           /* row changed */
    TRACKER_UPDATE_TRACK,         /* track metadata changed */
    TRACKER_UPDATE_CURSOR,        /* cursor moved */
    TRACKER_UPDATE_SELECTION,     /* selection changed */
    TRACKER_UPDATE_PLAYBACK,      /* playback position changed */
    TRACKER_UPDATE_TRANSPORT,     /* play/stop/pause state changed */
    TRACKER_UPDATE_PATTERN,       /* pattern structure changed */
    TRACKER_UPDATE_SONG,          /* song structure changed */
} TrackerUpdateType;

/**
 * An incremental update message.
 */
typedef struct {
    TrackerUpdateType type;
    int pattern;
    int track;
    int row;
    /* Additional data depends on type - serialized inline */
} TrackerUpdate;

/**
 * Serialize an incremental update to JSON.
 */
void tracker_json_write_update(TrackerJsonWriter* w, const TrackerUpdate* update,
                               const TrackerView* view);

/**
 * Apply an incremental update from JSON.
 */
bool tracker_json_apply_update(TrackerView* view, const char* json, int len,
                               const char** error_msg);

/*----------------------------------------------------------------------------
 * String Output Helpers
 *----------------------------------------------------------------------------*/

/**
 * Serialize song to JSON string.
 * @return  Heap-allocated JSON string (caller must free)
 */
char* tracker_json_song_to_string(const TrackerSong* song, bool pretty);

/**
 * Serialize view state to JSON string.
 */
char* tracker_json_view_state_to_string(const TrackerViewState* state, bool pretty);

/**
 * Serialize theme to JSON string.
 */
char* tracker_json_theme_to_string(const TrackerTheme* theme, bool pretty);

#endif /* TRACKER_VIEW_H */
