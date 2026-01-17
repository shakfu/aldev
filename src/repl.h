/**
 * @file repl.h
 * @brief Common REPL infrastructure - types and functions shared by all language REPLs.
 */

#ifndef PSND_REPL_H
#define PSND_REPL_H

#include "loki/internal.h"

#define MAX_INPUT_LENGTH 1024
#define REPL_HISTORY_MAX 64

/* Control key definitions */
#ifndef CTRL_A
#define CTRL_A 1
#endif
#ifndef CTRL_K
#define CTRL_K 11
#endif

/* Line editor state for syntax-highlighted REPL input */
typedef struct {
    char buf[MAX_INPUT_LENGTH];      /* Input buffer */
    int len;                         /* Current length */
    int pos;                         /* Cursor position */
    char *history[REPL_HISTORY_MAX]; /* History entries */
    int history_len;                 /* Number of history entries */
    int history_idx;                 /* Current history index (-1 = current input) */
    char saved_buf[MAX_INPUT_LENGTH];/* Saved current input when browsing history */
    int saved_len;                   /* Saved length */
    unsigned char hl[MAX_INPUT_LENGTH]; /* Highlight types per character */
} ReplLineEditor;

/* Initialize line editor state */
void repl_editor_init(ReplLineEditor *ed);

/* Cleanup line editor (free history) */
void repl_editor_cleanup(ReplLineEditor *ed);

/* Add a line to history */
void repl_add_history(ReplLineEditor *ed, const char *line);

/* Load history from file (one entry per line) */
int repl_history_load(ReplLineEditor *ed, const char *filepath);

/* Save history to file (one entry per line) */
int repl_history_save(ReplLineEditor *ed, const char *filepath);

/* Enable/disable terminal raw mode for REPL */
int repl_enable_raw_mode(void);
void repl_disable_raw_mode(void);

/* Read a line with syntax highlighting */
char *repl_readline(editor_ctx_t *syntax_ctx, ReplLineEditor *ed, const char *prompt);

/* Highlight the current line buffer */
void repl_highlight_line(editor_ctx_t *syntax_ctx, ReplLineEditor *ed);

/* Render the current line with highlighting */
void repl_render_line(editor_ctx_t *syntax_ctx, ReplLineEditor *ed, const char *prompt);

#endif /* PSND_REPL_H */
