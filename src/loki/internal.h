/* loki_internal.h - Internal structures and function declarations
 *
 * This header contains internal structures and function declarations needed
 * by both loki_core.c and loki_lua.c. It is NOT part of the public API and
 * should only be used within the loki library implementation.
 */

#ifndef LOKI_INTERNAL_H
#define LOKI_INTERNAL_H

#include <stddef.h>
#include <time.h>
#include <signal.h>
#include <lua.h>
#include "loki/core.h"  /* Public API types (EditorMode, etc.) */

/* ======================= Syntax Highlighting Constants ==================== */

#define HL_NORMAL 0
#define HL_NONPRINT 1
#define HL_COMMENT 2   /* Single line comment. */
#define HL_MLCOMMENT 3 /* Multi-line comment. */
#define HL_KEYWORD1 4
#define HL_KEYWORD2 5
#define HL_STRING 6
#define HL_NUMBER 7
#define HL_MATCH 8      /* Search match. */

#define HL_HIGHLIGHT_STRINGS (1<<0)
#define HL_HIGHLIGHT_NUMBERS (1<<1)

#define HL_TYPE_C 0
#define HL_TYPE_MARKDOWN 1

/* Code block language constants (for markdown) */
#define CB_LANG_NONE 0
#define CB_LANG_C 1
#define CB_LANG_PYTHON 2
#define CB_LANG_LUA 3
#define CB_LANG_CYTHON 4

/* ======================= Key Constants =================================== */

enum KEY_ACTION{
        KEY_NULL = 0,       /* NULL */
        CTRL_C = 3,         /* Ctrl-c */
        CTRL_D = 4,         /* Ctrl-d */
        CTRL_E = 5,         /* Ctrl-e (eval Alda) */
        CTRL_F = 6,         /* Ctrl-f */
        CTRL_G = 7,         /* Ctrl-g (stop Alda) */
        CTRL_H = 8,         /* Ctrl-h */
        TAB = 9,            /* Tab */
        CTRL_L = 12,        /* Ctrl+l */
        ENTER = 13,         /* Enter */
        CTRL_P = 16,        /* Ctrl-p (play file) */
        CTRL_Q = 17,        /* Ctrl-q */
        CTRL_S = 19,        /* Ctrl-s */
        CTRL_T = 20,        /* Ctrl-t */
        CTRL_U = 21,        /* Ctrl-u */
        CTRL_W = 23,        /* Ctrl-w */
        CTRL_X = 24,        /* Ctrl-x */
        ESC = 27,           /* Escape */
        BACKSPACE =  127,   /* Backspace */
        /* The following are just soft codes, not really reported by the
         * terminal directly. */
        ARROW_LEFT = 1000,
        ARROW_RIGHT,
        ARROW_UP,
        ARROW_DOWN,
        SHIFT_ARROW_LEFT,
        SHIFT_ARROW_RIGHT,
        SHIFT_ARROW_UP,
        SHIFT_ARROW_DOWN,
        DEL_KEY,
        HOME_KEY,
        END_KEY,
        PAGE_UP,
        PAGE_DOWN,
        SHIFT_RETURN
};

/* ======================= Configuration Constants ========================== */

#define KILO_QUERY_LEN 256
#define STATUS_ROWS 2

#define LUA_REPL_HISTORY_MAX 64
#define LUA_REPL_LOG_MAX 128
#define LUA_REPL_OUTPUT_ROWS 2
#define LUA_REPL_TOTAL_ROWS (LUA_REPL_OUTPUT_ROWS + 1)
#define LUA_REPL_PROMPT ">> "

/* ======================= Forward Declarations ============================= */

/* Undo/redo state - opaque pointer, defined in loki_undo.c */
struct undo_state;

/* ======================= Data Structures ================================== */

/* Syntax highlighting color definition */
typedef struct t_hlcolor {
    int r,g,b;
} t_hlcolor;

/* Syntax highlighting rules per language */
struct t_editor_syntax {
    char **filematch;
    char **keywords;
    char singleline_comment_start[4];   /* Increased to support longer comment syntax */
    char multiline_comment_start[6];    /* Increased for Lua's --[[ */
    char multiline_comment_end[6];      /* Increased for potential longer delimiters */
    char *separators;
    int flags;
    int type;  /* HL_TYPE_* */
};

/* This structure represents a single line of the file we are editing. */
typedef struct t_erow {
    int idx;            /* Row index in the file, zero-based. */
    int size;           /* Size of the row, excluding the null term. */
    int rsize;          /* Size of the rendered row. */
    char *chars;        /* Row content. */
    char *render;       /* Row content "rendered" for screen (for TABs). */
    unsigned char *hl;  /* Syntax highlight type for each character in render.*/
    int hl_oc;          /* Row had open comment at end in last syntax highlight
                           check. */
    int cb_lang;        /* Code block language (for markdown): CB_LANG_* */
} t_erow;

/* Lua REPL state */
typedef struct t_lua_repl {
    char input[KILO_QUERY_LEN+1];
    int input_len;
    int active;
    int history_len;
    int history_index;
    char *history[LUA_REPL_HISTORY_MAX];
    int log_len;
    char *log[LUA_REPL_LOG_MAX];
} t_lua_repl;

/* Editor context - one instance per editor viewport/buffer.
 * This structure will enable multiple independent editor contexts for future
 * split windows and multiple buffers implementation.
 * The typedef editor_ctx_t is declared in loki/core.h (public API). */
struct editor_ctx {
    int cx,cy;  /* Cursor x and y position in characters */
    int rowoff;     /* Offset of row displayed. */
    int coloff;     /* Offset of column displayed. */
    int screenrows; /* Number of rows that we can show */
    int screencols; /* Number of cols that we can show */
    int screenrows_total; /* Rows available after status bars (before REPL) */
    int numrows;    /* Number of rows */
    int rawmode;    /* Is terminal raw mode enabled? */
    t_erow *row;      /* Rows */
    int dirty;      /* File modified but not saved. */
    char *filename; /* Currently open filename */
    char statusmsg[80];
    time_t statusmsg_time;
    struct t_editor_syntax *syntax;    /* Current syntax highlight, or NULL. */
    lua_State *L;        /* Lua state - managed by loki_editor.c */
    t_lua_repl repl;     /* Lua REPL state - managed by loki_editor.c */
    EditorMode mode; /* Current editor mode (normal/insert/visual/command) */
    int word_wrap;  /* Word wrap enabled flag */
    int sel_active; /* Selection active flag */
    int sel_start_x, sel_start_y; /* Selection start position */
    int sel_end_x, sel_end_y;     /* Selection end position */
    t_hlcolor colors[9]; /* Syntax highlight colors: indexed by HL_* constants */

    /* Window resize state (checked in main loop, set by signal handler) */
    volatile sig_atomic_t winsize_changed; /* Non-zero if window size changed */

    /* Command mode state */
    char cmd_buffer[256];      /* Command input buffer */
    int cmd_length;            /* Length of command */
    int cmd_cursor_pos;        /* Cursor position in command */
    int cmd_history_index;     /* Current history position */

    /* Undo/redo state */
    struct undo_state *undo_state;  /* NULL if undo disabled */

    /* Indentation configuration */
    struct indent_config *indent_config;  /* Auto-indent settings */

    /* Alda mode flag (set when .alda file opened and Alda initialized) */
    int alda_mode;

    /* Line numbers display flag */
    int line_numbers;
};

/* Legacy type name for compatibility during migration.
 * New code should use editor_ctx_t. */
typedef editor_ctx_t loki_editor_instance;

/* ======================= Screen Buffer =================================== */

/* Append buffer for building screen output */
struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL,0}

/* Screen buffer functions are now in loki_terminal.h */

/* ======================= Function Declarations ============================ */

/* Context management (for future split windows and multi-buffer support) */
void editor_ctx_init(editor_ctx_t *ctx);
void editor_ctx_free(editor_ctx_t *ctx);

/* Status message */
void editor_set_status_msg(editor_ctx_t *ctx, const char *fmt, ...);

/* Character insertion (context-aware) */
void editor_insert_char(editor_ctx_t *ctx, int c);
void editor_insert_newline(editor_ctx_t *ctx);
void editor_del_char(editor_ctx_t *ctx);

/* Row management (test helpers) */
void editor_insert_row(editor_ctx_t *ctx, int at, char *s, size_t len);

/* Screen rendering */
void editor_refresh_screen(editor_ctx_t *ctx);

/* Lua REPL functions */
void lua_repl_init(t_lua_repl *repl);
void lua_repl_free(t_lua_repl *repl);
void lua_repl_handle_keypress(editor_ctx_t *ctx, int key);
void lua_repl_render(editor_ctx_t *ctx, struct abuf *ab);
void lua_repl_append_log(editor_ctx_t *ctx, const char *line);
void editor_update_repl_layout(editor_ctx_t *ctx);

/* Editor cleanup */
void editor_cleanup_resources(editor_ctx_t *ctx);

/* Syntax highlighting helpers */
int hl_name_to_code(const char *name);
int is_separator(int c, char *separators);

/* Terminal and input functions are now in loki_terminal.h */
void editor_process_keypress(editor_ctx_t *ctx, int fd);

/* Cursor movement */
void editor_move_cursor(editor_ctx_t *ctx, int key);

/* Modal editing test functions - for testing only
 * These functions expose the internal mode handlers for unit testing.
 * They should not be used in production code. */
void modal_process_normal_mode_key(editor_ctx_t *ctx, int fd, int c);
void modal_process_insert_mode_key(editor_ctx_t *ctx, int fd, int c);
void modal_process_visual_mode_key(editor_ctx_t *ctx, int fd, int c);

/* Syntax highlighting test functions - for testing only
 * These functions expose internal syntax highlighting for unit testing.
 * They should not be used in production code. */
void editor_update_syntax(editor_ctx_t *ctx, t_erow *row);
void editor_update_row(editor_ctx_t *ctx, t_erow *row);

/* Search test functions - for testing only
 * These functions expose internal search logic for unit testing.
 * They should not be used in production code. */
int editor_find_next_match(editor_ctx_t *ctx, const char *query, int start_row, int direction, int *match_offset);

#endif /* LOKI_INTERNAL_H */
