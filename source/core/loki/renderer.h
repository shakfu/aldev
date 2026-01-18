/* renderer.h - Abstract renderer interface
 *
 * This module provides a platform-agnostic rendering abstraction:
 * - Decouples editor logic from terminal-specific VT100 escape codes
 * - Enables alternative frontends (web, GUI, tests)
 * - Provides structured rendering callbacks instead of direct terminal writes
 *
 * The Renderer interface defines callbacks for:
 * - Screen rendering (rows, gutter, status, REPL)
 * - Cursor positioning
 * - Clipboard operations
 * - Screen management (clear, show/hide cursor)
 */

#ifndef LOKI_RENDERER_H
#define LOKI_RENDERER_H

#include <stddef.h>
#include <stdint.h>

/* Forward declarations */
typedef struct editor_ctx editor_ctx_t;
typedef struct Renderer Renderer;

/* ======================= Highlight Types ================================== */

/* Text highlight type for syntax coloring */
typedef enum {
    HL_TYPE_NORMAL = 0,
    HL_TYPE_COMMENT,
    HL_TYPE_KEYWORD1,
    HL_TYPE_KEYWORD2,
    HL_TYPE_STRING,
    HL_TYPE_NUMBER,
    HL_TYPE_MATCH,
    HL_TYPE_NONPRINT,
    HL_TYPE_SELECTION,
} HighlightType;

/* ======================= Render Segment =================================== */

/**
 * RenderSegment - A contiguous span of text with uniform styling.
 *
 * The renderer receives an array of segments for each row, allowing
 * it to apply appropriate styling without knowing the underlying
 * terminal escape codes.
 */
typedef struct {
    const char *text;       /* Text content (not null-terminated) */
    int len;                /* Length of text */
    HighlightType hl_type;  /* Highlight type for coloring */
    int selected;           /* 1 if text is selected, 0 otherwise */
} RenderSegment;

/* ======================= Status Bar Info ================================== */

/**
 * StatusInfo - Information for status bar rendering.
 */
typedef struct {
    const char *mode;           /* Mode string (NORMAL, INSERT, etc.) */
    const char *filename;       /* Current filename */
    const char *lang;           /* Language indicator (ALDA, JOY, etc.) */
    int numrows;                /* Total lines in file */
    int current_row;            /* Current cursor row (1-based) */
    int dirty;                  /* File modified flag */
    int playing;                /* Playback active flag */
    int link_active;            /* Ableton Link active flag */
} StatusInfo;

/* ======================= REPL Info ======================================== */

/**
 * ReplInfo - Information for REPL pane rendering.
 */
typedef struct {
    const char *prompt;         /* REPL prompt string */
    const char *input;          /* Current input line */
    int input_len;              /* Length of input */
    const char **log_lines;     /* Array of log lines (most recent last) */
    int log_count;              /* Number of log lines */
    int max_display_lines;      /* Maximum lines to display */
} ReplInfo;

/* ======================= Renderer Interface =============================== */

/**
 * Renderer - Abstract rendering interface.
 *
 * All rendering operations go through this interface, allowing
 * different backends (terminal, web, GUI) to implement rendering
 * in their own way.
 *
 * Usage:
 *   Renderer *r = terminal_renderer_create();
 *   r->begin_frame(r, cols, rows);
 *   r->render_row(r, ...);
 *   r->end_frame(r);
 */
struct Renderer {
    /* ==================== Frame Management ==================== */

    /**
     * Begin a new frame. Called before rendering content.
     * @param r      Renderer instance
     * @param cols   Screen width in columns
     * @param rows   Screen height in rows
     */
    void (*begin_frame)(Renderer *r, int cols, int rows);

    /**
     * End the current frame. Called after all rendering is complete.
     * Flushes output and restores cursor.
     * @param r      Renderer instance
     */
    void (*end_frame)(Renderer *r);

    /* ==================== Content Rendering ==================== */

    /**
     * Render buffer tabs (when multiple buffers are open).
     * @param r          Renderer instance
     * @param tabs       Array of tab labels
     * @param tab_count  Number of tabs
     * @param active_tab Index of active tab
     * @param width      Available width
     */
    void (*render_tabs)(Renderer *r, const char **tabs, int tab_count,
                        int active_tab, int width);

    /**
     * Render a single row of text content.
     * @param r          Renderer instance
     * @param row_num    Row number (1-based, for gutter display)
     * @param segments   Array of render segments
     * @param seg_count  Number of segments
     * @param gutter_width  Width of line number gutter (0 to disable)
     * @param is_empty   True if this is an empty row (past end of file)
     */
    void (*render_row)(Renderer *r, int row_num, const RenderSegment *segments,
                       int seg_count, int gutter_width, int is_empty);

    /**
     * Render the status bar.
     * @param r      Renderer instance
     * @param info   Status bar information
     * @param width  Available width
     */
    void (*render_status)(Renderer *r, const StatusInfo *info, int width);

    /**
     * Render the message line (below status bar).
     * @param r       Renderer instance
     * @param message Status message (may be NULL or empty)
     * @param width   Available width
     */
    void (*render_message)(Renderer *r, const char *message, int width);

    /**
     * Render the REPL pane.
     * @param r      Renderer instance
     * @param info   REPL information
     * @param width  Available width
     */
    void (*render_repl)(Renderer *r, const ReplInfo *info, int width);

    /* ==================== Cursor Management ==================== */

    /**
     * Position the cursor.
     * @param r    Renderer instance
     * @param row  Row (1-based)
     * @param col  Column (1-based)
     */
    void (*set_cursor)(Renderer *r, int row, int col);

    /**
     * Show the cursor.
     * @param r  Renderer instance
     */
    void (*show_cursor)(Renderer *r);

    /**
     * Hide the cursor.
     * @param r  Renderer instance
     */
    void (*hide_cursor)(Renderer *r);

    /* ==================== Clipboard Operations ==================== */

    /**
     * Copy text to system clipboard.
     * @param r     Renderer instance
     * @param text  Text to copy
     * @param len   Length of text
     * @return 0 on success, -1 on failure
     */
    int (*clipboard_copy)(Renderer *r, const char *text, size_t len);

    /* ==================== Lifecycle ==================== */

    /**
     * Destroy the renderer and free resources.
     * @param r  Renderer instance
     */
    void (*destroy)(Renderer *r);

    /* ==================== Private Data ==================== */

    void *data;  /* Implementation-specific data */
};

/* ======================= Renderer Factory Functions ======================= */

/**
 * Create a terminal renderer using VT100 escape sequences.
 * This is the default renderer for terminal-based editing.
 * @return Renderer instance, or NULL on error
 */
Renderer *terminal_renderer_create(void);

/**
 * Create a null renderer that discards all output.
 * Useful for testing or headless operation.
 * @return Renderer instance, or NULL on error
 */
Renderer *null_renderer_create(void);

/* ======================= Helper Functions ================================= */

/**
 * Convert internal HL_* constant to HighlightType.
 * @param hl_const  Internal highlight constant (from syntax.h)
 * @return HighlightType enum value
 */
HighlightType hl_const_to_type(int hl_const);

#endif /* LOKI_RENDERER_H */
