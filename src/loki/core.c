#include "psnd.h"

/* 
loki is based on kilo -- A very simple editor in less than 1000 
lines of code (as counted by "cloc"). Does not depend on libcurses,
directly emits VT100 escapes on the terminal.

Copyright (c) 2016, Salvatore Sanfilippo <antirez at gmail dot com>

see LICENSE.
*/


#ifdef __linux__
#define _POSIX_C_SOURCE 200809L
#endif

#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <strings.h>

#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <signal.h>

#include "loki/editor.h"
#include "internal.h"
#include "selection.h"
#include "search.h"
#include "modal.h"
#include "command.h"
#include "terminal.h"
#include "undo.h"
#include "buffers.h"
#include "syntax.h"
#include "indent.h"
#include "lang_bridge.h"
#include "loki/link.h"

void editor_set_status_msg(editor_ctx_t *ctx, const char *fmt, ...) {
    if (!ctx) return;
    va_list ap;
    va_start(ap,fmt);
    vsnprintf(ctx->statusmsg,sizeof(ctx->statusmsg),fmt,ap);
    va_end(ap);
    ctx->statusmsg_time = time(NULL);
}

/* ======================= Context Management =============================== */

/* Initialize an editor context with default values.
 * This allows creating independent editor contexts for split windows
 * and multiple buffer support. */
void editor_ctx_init(editor_ctx_t *ctx) {
    memset(ctx, 0, sizeof(editor_ctx_t));
    ctx->cx = 0;
    ctx->cy = 0;
    ctx->rowoff = 0;
    ctx->coloff = 0;
    ctx->screenrows = 0;
    ctx->screencols = 0;
    ctx->screenrows_total = 0;
    ctx->numrows = 0;
    /* Note: rawmode now lives in TerminalHost, not per-buffer */
    ctx->row = NULL;
    ctx->dirty = 0;
    ctx->filename = NULL;
    ctx->statusmsg[0] = '\0';
    ctx->statusmsg_time = 0;
    ctx->syntax = NULL;
    ctx->L = NULL;
    memset(&ctx->repl, 0, sizeof(t_lua_repl));
    ctx->mode = MODE_NORMAL;
    ctx->word_wrap = 0;
    ctx->sel_active = 0;
    ctx->sel_start_x = 0;
    ctx->sel_start_y = 0;
    ctx->sel_end_x = 0;
    ctx->sel_end_y = 0;
    /* Note: winsize_changed now lives in TerminalHost, not per-buffer */
    memset(ctx->colors, 0, sizeof(ctx->colors));
    /* Command mode state */
    command_mode_init(ctx);
    /* Undo/redo system (1000 operations, 10MB memory limit) */
    undo_init(ctx, 1000, 10 * 1024 * 1024);
    /* Auto-indent system */
    indent_init(ctx);
}

/* Free all dynamically allocated memory in a context.
 * This should be called when a context is no longer needed. */
void editor_ctx_free(editor_ctx_t *ctx) {
    /* Free all row data */
    for (int i = 0; i < ctx->numrows; i++) {
        free(ctx->row[i].chars);
        free(ctx->row[i].render);
        free(ctx->row[i].hl);
    }
    free(ctx->row);

    /* Free filename */
    free(ctx->filename);

    /* Note: We don't free ctx->L (Lua state) as it's shared across contexts
     * and managed separately by the editor instance */

    /* Free command mode state */
    command_mode_free(ctx);

    /* Free undo/redo state */
    undo_free(ctx);

    /* Free indent configuration */
    free(ctx->indent_config);

    /* Clear the structure */
    memset(ctx, 0, sizeof(editor_ctx_t));
}

/* =========================== Syntax highlights DB =========================
 *
 * Built-in language definitions are in loki_languages.c.
 * Dynamic languages can be registered via loki.register_language() in Lua.
 */

#include "languages.h"

/* Static pointer to editor context for atexit cleanup.
 * Set by init_editor() before registering atexit handler. */
static editor_ctx_t *editor_for_atexit = NULL;

/* Set the context used for atexit cleanup */
void editor_set_atexit_context(editor_ctx_t *ctx) {
    editor_for_atexit = ctx;
}

/* Called at exit to avoid remaining in raw mode. */
void editor_atexit(void) {
    /* Restore terminal via TerminalHost */
    terminal_host_disable_raw_mode(g_terminal_host);

    /* Cleanup editor resources */
    if (editor_for_atexit) {
        editor_cleanup_resources(editor_for_atexit);
    }
    cleanup_dynamic_languages();
}

/* ======================= Editor rows implementation ======================= */

/* Update the rendered version and the syntax highlight of a row. */
void editor_update_row(editor_ctx_t *ctx, t_erow *row) {
    unsigned int tabs = 0;
    int j, idx;

   /* Create a version of the row we can directly print on the screen,
     * respecting tabs, substituting non printable characters with '?'. */
    free(row->render);
    for (j = 0; j < row->size; j++)
        if (row->chars[j] == TAB) tabs++;

    unsigned long long allocsize =
        (unsigned long long) row->size + tabs*8 + 1;
    if (allocsize > UINT32_MAX) {
        printf("Some line of the edited file is too long for loki\n");
        exit(1);
    }

    row->render = malloc(row->size + tabs*8 + 1);
    if (row->render == NULL) {
        perror("Out of memory");
        exit(1);
    }
    idx = 0;
    for (j = 0; j < row->size; j++) {
        if (row->chars[j] == TAB) {
            row->render[idx++] = ' ';
            while((idx+1) % 8 != 0) row->render[idx++] = ' ';
        } else {
            row->render[idx++] = row->chars[j];
        }
    }
    row->rsize = idx;
    row->render[idx] = '\0';

    /* Update the syntax highlighting attributes of the row. */
    syntax_update_row(ctx, row);
}

/* Insert a row at the specified position, shifting the other rows on the bottom
 * if required. */
void editor_insert_row(editor_ctx_t *ctx, int at, char *s, size_t len) {
    if (at > ctx->numrows) return;
    /* Check for integer overflow in allocation size calculation */
    if ((size_t)ctx->numrows >= SIZE_MAX / sizeof(t_erow)) {
        fprintf(stderr, "Too many rows, cannot allocate more memory\n");
        exit(1);
    }
    t_erow *new_row = realloc(ctx->row,sizeof(t_erow)*(ctx->numrows+1));
    if (new_row == NULL) {
        perror("Out of memory");
        exit(1);
    }
    ctx->row = new_row;
    if (at != ctx->numrows) {
        memmove(ctx->row+at+1,ctx->row+at,sizeof(ctx->row[0])*(ctx->numrows-at));
        for (int j = at+1; j <= ctx->numrows; j++) ctx->row[j].idx++;
    }
    ctx->row[at].size = len;
    ctx->row[at].chars = malloc(len+1);
    if (ctx->row[at].chars == NULL) {
        perror("Out of memory");
        exit(1);
    }
    memcpy(ctx->row[at].chars,s,len+1);
    ctx->row[at].hl = NULL;
    ctx->row[at].hl_oc = 0;
    ctx->row[at].cb_lang = CB_LANG_NONE;
    ctx->row[at].render = NULL;
    ctx->row[at].rsize = 0;
    ctx->row[at].idx = at;
    editor_update_row(ctx, ctx->row+at);
    ctx->numrows++;
    ctx->dirty++;
}

/* Free row's heap allocated stuff. */
void editor_free_row(t_erow *row) {
    free(row->render);
    free(row->chars);
    free(row->hl);
}

/* Remove the row at the specified position, shifting the remaining on the
 * top. */
void editor_del_row(editor_ctx_t *ctx, int at) {
    t_erow *row;

    if (at >= ctx->numrows) return;
    row = ctx->row+at;
    editor_free_row(row);
    memmove(ctx->row+at,ctx->row+at+1,sizeof(ctx->row[0])*(ctx->numrows-at-1));
    for (int j = at; j < ctx->numrows-1; j++) ctx->row[j].idx++;
    ctx->numrows--;
    ctx->dirty++;
}

/* Turn the editor rows into a single heap-allocated string.
 * Returns the pointer to the heap-allocated string and populate the
 * integer pointed by 'buflen' with the size of the string, excluding
 * the final nulterm. */
char *editor_rows_to_string(editor_ctx_t *ctx, int *buflen) {
    char *buf = NULL, *p;
    int totlen = 0;
    int j;

    /* Compute count of bytes */
    for (j = 0; j < ctx->numrows; j++)
        totlen += ctx->row[j].size+1; /* +1 is for "\n" at end of every row */
    *buflen = totlen;
    totlen++; /* Also make space for nulterm */

    p = buf = malloc(totlen);
    if (buf == NULL) return NULL;
    for (j = 0; j < ctx->numrows; j++) {
        memcpy(p,ctx->row[j].chars,ctx->row[j].size);
        p += ctx->row[j].size;
        *p = '\n';
        p++;
    }
    *p = '\0';
    return buf;
}

/* Insert a character at the specified position in a row, moving the remaining
 * chars on the right if needed. */
void editor_row_insert_char(editor_ctx_t *ctx, t_erow *row, int at, int c) {
    if (!row) return;
    char *new_chars;
    if (at > row->size) {
        /* Pad the string with spaces if the insert location is outside the
         * current length by more than a single character. */
        int padlen = at-row->size;
        /* In the next line +2 means: new char and null term. */
        new_chars = realloc(row->chars,row->size+padlen+2);
        if (new_chars == NULL) {
            perror("Out of memory");
            exit(1);
        }
        row->chars = new_chars;
        memset(row->chars+row->size,' ',padlen);
        row->chars[row->size+padlen+1] = '\0';
        row->size += padlen+1;
    } else {
        /* If we are in the middle of the string just make space for 1 new
         * char plus the (already existing) null term. */
        new_chars = realloc(row->chars,row->size+2);
        if (new_chars == NULL) {
            perror("Out of memory");
            exit(1);
        }
        row->chars = new_chars;
        memmove(row->chars+at+1,row->chars+at,row->size-at+1);
        row->size++;
    }
    row->chars[at] = c;
    editor_update_row(ctx, row);
    ctx->dirty++;
}

/* Append the string 's' at the end of a row */
void editor_row_append_string(editor_ctx_t *ctx, t_erow *row, char *s, size_t len) {
    char *new_chars = realloc(row->chars,row->size+len+1);
    if (new_chars == NULL) {
        perror("Out of memory");
        exit(1);
    }
    row->chars = new_chars;
    memcpy(row->chars+row->size,s,len);
    row->size += len;
    row->chars[row->size] = '\0';
    editor_update_row(ctx, row);
    ctx->dirty++;
}

/* Delete the character at offset 'at' from the specified row. */
void editor_row_del_char(editor_ctx_t *ctx, t_erow *row, int at) {
    if (row->size <= at) return;
    /* Include null terminator in move (+1 for the null byte) */
    memmove(row->chars+at,row->chars+at+1,row->size-at+1);
    row->size--;
    editor_update_row(ctx, row);
    ctx->dirty++;
}

/* Insert the specified char at the current prompt position. */
void editor_insert_char(editor_ctx_t *ctx, int c) {
    int filerow = ctx->rowoff+ctx->cy;
    int filecol = ctx->coloff+ctx->cx;
    t_erow *row = (filerow >= ctx->numrows) ? NULL : &ctx->row[filerow];

    /* If the row where the cursor is currently located does not exist in our
     * logical representaion of the file, add enough empty rows as needed. */
    if (!row) {
        while(ctx->numrows <= filerow)
            editor_insert_row(ctx, ctx->numrows,"",0);
    }
    row = &ctx->row[filerow];
    editor_row_insert_char(ctx, row,filecol,c);

    /* Record undo operation */
    undo_record_insert_char(ctx, filerow, filecol, c);

    if (ctx->cx == ctx->screencols-1)
        ctx->coloff++;
    else
        ctx->cx++;
    /* Note: dirty already incremented by editor_row_insert_char */

    /* Handle electric dedent for closing braces */
    indent_electric_char(ctx, c);
}

/* Inserting a newline is slightly complex as we have to handle inserting a
 * newline in the middle of a line, splitting the line as needed. */
void editor_insert_newline(editor_ctx_t *ctx) {
    int filerow = ctx->rowoff+ctx->cy;
    int filecol = ctx->coloff+ctx->cx;
    t_erow *row = (filerow >= ctx->numrows) ? NULL : &ctx->row[filerow];

    if (!row) {
        if (filerow == ctx->numrows) {
            editor_insert_row(ctx, filerow,"",0);
            undo_record_insert_line(ctx, filerow, filecol, "", 0);
            goto fixcursor;
        }
        return;
    }
    /* If the cursor is over the current line size, we want to conceptually
     * think it's just over the last character. */
    if (filecol >= row->size) filecol = row->size;
    if (filecol == 0) {
        editor_insert_row(ctx, filerow,"",0);
        undo_record_insert_line(ctx, filerow, filecol, "", 0);
    } else {
        /* We are in the middle of a line. Split it between two rows. */
        char *split_content = row->chars + filecol;
        int split_length = row->size - filecol;
        editor_insert_row(ctx, filerow+1, split_content, split_length);
        undo_record_insert_line(ctx, filerow, filecol, split_content, split_length);
        row = &ctx->row[filerow];
        row->chars[filecol] = '\0';
        row->size = filecol;
        editor_update_row(ctx, row);
    }
fixcursor:
    if (ctx->cy == ctx->screenrows-1) {
        ctx->rowoff++;
    } else {
        ctx->cy++;
    }
    ctx->cx = 0;
    ctx->coloff = 0;

    /* Apply auto-indentation */
    indent_apply(ctx);
}

/* Delete the char at the current prompt position. */
void editor_del_char(editor_ctx_t *ctx) {
    int filerow = ctx->rowoff+ctx->cy;
    int filecol = ctx->coloff+ctx->cx;
    t_erow *row = (filerow >= ctx->numrows) ? NULL : &ctx->row[filerow];

    if (!row || (filecol == 0 && filerow == 0)) return;
    if (filecol == 0) {
        /* Handle the case of column 0, we need to move the current line
         * on the right of the previous one. */
        filecol = ctx->row[filerow-1].size;
        /* Record deleting the newline (merging lines) */
        undo_record_delete_line(ctx, filerow - 1, filecol, row->chars, row->size);
        editor_row_append_string(ctx, &ctx->row[filerow-1],row->chars,row->size);
        editor_del_row(ctx, filerow);
        row = NULL;
        if (ctx->cy == 0)
            ctx->rowoff--;
        else
            ctx->cy--;
        ctx->cx = filecol;
        if (ctx->cx >= ctx->screencols) {
            int shift = (ctx->cx-ctx->screencols)+1;
            ctx->cx -= shift;
            ctx->coloff += shift;
        }
    } else {
        /* Record deleting the character */
        char deleted_char = row->chars[filecol-1];
        undo_record_delete_char(ctx, filerow, filecol-1, deleted_char);
        editor_row_del_char(ctx, row,filecol-1);
        if (ctx->cx == 0 && ctx->coloff)
            ctx->coloff--;
        else
            ctx->cx--;
    }
    if (row) editor_update_row(ctx, row);
    /* Note: dirty already incremented by editor_row_del_char or editor_del_row */
}

/* Load the specified program in the editor memory and returns 0 on success
 * or 1 on error. */
int editor_open(editor_ctx_t *ctx, char *filename) {
    FILE *fp;

    ctx->dirty = 0;
    free(ctx->filename);
    size_t fnlen = strlen(filename)+1;
    ctx->filename = malloc(fnlen);
    if (ctx->filename == NULL) {
        perror("Out of memory");
        exit(1);
    }
    memcpy(ctx->filename,filename,fnlen);

    fp = fopen(filename,"r");
    if (!fp) {
        if (errno != ENOENT) {
            perror("Opening file");
            exit(1);
        }
        return 1;
    }

    /* Check if file appears to be binary by looking for null bytes in first 1KB */
    char probe[1024];
    size_t probe_len = fread(probe, 1, sizeof(probe), fp);
    for (size_t i = 0; i < probe_len; i++) {
        if (probe[i] == '\0') {
            fclose(fp);
            editor_set_status_msg(ctx, "Cannot open binary file");
            return 1;
        }
    }
    rewind(fp);  /* Go back to start of file to read normally */

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while((linelen = getline(&line,&linecap,fp)) != -1) {
        while (linelen > 0 && (line[linelen-1] == '\n' || line[linelen-1] == '\r'))
            line[--linelen] = '\0';
        editor_insert_row(ctx, ctx->numrows,line,linelen);
    }
    free(line);
    fclose(fp);
    ctx->dirty = 0;

    return 0;
}

/* Save the current file on disk. Return 0 on success, 1 on error. */
int editor_save(editor_ctx_t *ctx) {
    int len;
    char *buf = editor_rows_to_string(ctx, &len);
    if (buf == NULL) {
        editor_set_status_msg(ctx, "Can't save! Out of memory");
        return 1;
    }

    /* Check if buffer has a filename */
    if (ctx->filename == NULL) {
        free(buf);
        editor_set_status_msg(ctx, "No file name (use :w <filename> to save)");
        return 1;
    }

    int fd = open(ctx->filename,O_RDWR|O_CREAT,0644);
    if (fd == -1) goto writeerr;

    /* Use truncate + a single write(2) call in order to make saving
     * a bit safer, under the limits of what we can do in a small editor. */
    if (ftruncate(fd,len) == -1) goto writeerr;
    if (write(fd,buf,len) != len) goto writeerr;

    close(fd);
    free(buf);
    ctx->dirty = 0;
    editor_set_status_msg(ctx, "%d bytes written on disk", len);
    return 0;

writeerr:
    free(buf);
    if (fd != -1) close(fd);
    editor_set_status_msg(ctx, "Can't save! I/O error: %s",strerror(errno));
    return 1;
}

/* ============================= Terminal update ============================ */

/* Screen buffer functions are now in loki_terminal.c */

/* This function writes the whole screen using VT100 escape characters
 * starting from the logical state of the editor in the global state 'E'. */
void editor_refresh_screen(editor_ctx_t *ctx) {
    int y;
    t_erow *r;
    char buf[32];
    struct abuf ab = ABUF_INIT;

    terminal_buffer_append(&ab,"\x1b[?25l",6); /* Hide cursor. */
    terminal_buffer_append(&ab,"\x1b[H",3); /* Go home. */

    /* Render buffer tabs at top if multiple buffers open */
    int tabs_showing = (buffer_count() > 1) ? 1 : 0;
    buffers_render_tabs(&ab, ctx->screencols);

    /* Reduce available rows if tabs are showing */
    int available_rows = ctx->screenrows - tabs_showing;

    /* Calculate gutter width for line numbers */
    int gutter_width = 0;
    if (ctx->line_numbers && ctx->numrows > 0) {
        /* Width = digits needed for max line number + 1 for separator */
        int max_line = ctx->numrows;
        gutter_width = 1; /* At least 1 digit */
        while (max_line >= 10) {
            gutter_width++;
            max_line /= 10;
        }
        gutter_width += 1; /* Space separator after number */
    }

    /* Available cols for text after gutter */
    int text_cols = ctx->screencols - gutter_width;
    if (text_cols < 1) text_cols = 1;

    for (y = 0; y < available_rows; y++) {
        int filerow = ctx->rowoff+y;

        if (filerow >= ctx->numrows) {
            if (ctx->numrows == 0 && y == available_rows/3) {
                char welcome[80];
                int welcomelen = snprintf(welcome,sizeof(welcome),
                    "Loki editor -- version %s\x1b[0K\r\n", PSND_VERSION);
                int padding = (ctx->screencols-welcomelen)/2;
                if (padding) {
                    terminal_buffer_append(&ab,"~",1);
                    padding--;
                }
                while(padding--) terminal_buffer_append(&ab," ",1);
                terminal_buffer_append(&ab,welcome,welcomelen);
            } else {
                /* Empty lines: show gutter filler if line numbers enabled */
                if (ctx->line_numbers && gutter_width > 0) {
                    terminal_buffer_append(&ab,"\x1b[90m",5); /* Dark gray */
                    for (int i = 0; i < gutter_width - 1; i++)
                        terminal_buffer_append(&ab," ",1);
                    terminal_buffer_append(&ab,"~",1);
                    terminal_buffer_append(&ab,"\x1b[39m",5); /* Reset */
                } else {
                    terminal_buffer_append(&ab,"~",1);
                }
                terminal_buffer_append(&ab,"\x1b[0K\r\n",6);
            }
            continue;
        }

        /* Render line number gutter */
        if (ctx->line_numbers && gutter_width > 0) {
            char line_num_buf[16];
            int line_num_len = snprintf(line_num_buf, sizeof(line_num_buf),
                "%*d ", gutter_width - 1, filerow + 1);
            terminal_buffer_append(&ab,"\x1b[90m",5); /* Dark gray for line numbers */
            terminal_buffer_append(&ab, line_num_buf, line_num_len);
            terminal_buffer_append(&ab,"\x1b[39m",5); /* Reset foreground */
        }

        r = &ctx->row[filerow];

        int len = r->rsize - ctx->coloff;
        int current_color = -1;

        /* Word wrap: clamp to screen width and find word boundary */
        if (ctx->word_wrap && len > text_cols && r->cb_lang == CB_LANG_NONE) {
            len = text_cols;
            /* Find last space/separator to break at word boundary */
            int last_space = -1;
            for (int k = 0; k < len; k++) {
                if (isspace(r->render[ctx->coloff + k])) {
                    last_space = k;
                }
            }
            if (last_space > 0 && last_space > len / 2) {
                len = last_space + 1; /* Include the space */
            }
        }

        if (len > 0) {
            if (len > text_cols) len = text_cols;
            char *c = r->render+ctx->coloff;
            unsigned char *hl = r->hl+ctx->coloff;
            int j;
            for (j = 0; j < len; j++) {
                int selected = is_selected(ctx, filerow, ctx->coloff + j);

                /* Apply selection background */
                if (selected) {
                    terminal_buffer_append(&ab,"\x1b[7m",4); /* Reverse video */
                }

                if (hl[j] == HL_NONPRINT) {
                    char sym;
                    if (!selected) terminal_buffer_append(&ab,"\x1b[7m",4);
                    if (c[j] <= 26)
                        sym = '@'+c[j];
                    else
                        sym = '?';
                    terminal_buffer_append(&ab,&sym,1);
                    terminal_buffer_append(&ab,"\x1b[0m",4);
                    if (current_color != -1) {
                        char buf[32];
                        int clen = syntax_format_color(ctx, current_color, buf, sizeof(buf));
                        terminal_buffer_append(&ab,buf,clen);
                    }
                } else if (hl[j] == HL_NORMAL) {
                    if (current_color != -1) {
                        terminal_buffer_append(&ab,"\x1b[39m",5);
                        current_color = -1;
                    }
                    terminal_buffer_append(&ab,c+j,1);
                    if (selected) {
                        terminal_buffer_append(&ab,"\x1b[0m",4); /* Reset */
                    }
                } else {
                    int color = hl[j];
                    if (color != current_color) {
                        char buf[32];
                        int clen = syntax_format_color(ctx, color, buf, sizeof(buf));
                        current_color = color;
                        terminal_buffer_append(&ab,buf,clen);
                    }
                    terminal_buffer_append(&ab,c+j,1);
                    if (selected) {
                        terminal_buffer_append(&ab,"\x1b[0m",4); /* Reset */
                        if (current_color != -1) {
                            char buf[32];
                            int clen = syntax_format_color(ctx, current_color, buf, sizeof(buf));
                            terminal_buffer_append(&ab,buf,clen);
                        }
                    }
                }
            }
        }
        terminal_buffer_append(&ab,"\x1b[39m",5);
        terminal_buffer_append(&ab,"\x1b[0K",4);
        terminal_buffer_append(&ab,"\r\n",2);
    }

    /* Create a two rows status. First row: */
    terminal_buffer_append(&ab,"\x1b[0K",4);
    terminal_buffer_append(&ab,"\x1b[7m",4);
    char status[80], rstatus[80];

    /* Get mode indicator */
    const char *mode_str = "";
    int link_active = loki_link_is_initialized(ctx) && loki_link_is_enabled(ctx);

    switch(ctx->mode) {
        case MODE_NORMAL: mode_str = link_active ? "LINK" : "NORMAL"; break;
        case MODE_INSERT: mode_str = "INSERT"; break;
        case MODE_VISUAL: mode_str = "VISUAL"; break;
        case MODE_COMMAND: mode_str = "COMMAND"; break;
    }

    /* Show language indicator if a language is active for this file */
    const LokiLangOps *lang = loki_lang_for_file(ctx->filename);
    const char *lang_str = "";
    char lang_buf[16] = "";
    if (lang && lang->is_initialized && lang->is_initialized(ctx)) {
        snprintf(lang_buf, sizeof(lang_buf), "%s ", lang->name);
        /* Uppercase the language name */
        for (int i = 0; lang_buf[i] && i < 15; i++) {
            if (lang_buf[i] >= 'a' && lang_buf[i] <= 'z') {
                lang_buf[i] -= 32;
            }
        }
        lang_str = lang_buf;
    }

    int len = snprintf(status, sizeof(status), " %s%s  %.20s - %d lines %s",
        lang_str, mode_str, ctx->filename, ctx->numrows, ctx->dirty ? "(modified)" : "");

    /* Show playing indicator if any language is playing */
    const char *playing = loki_lang_is_playing(ctx) ? "[PLAYING] " : "";
    int rlen = snprintf(rstatus, sizeof(rstatus),
        "%s%d/%d", playing, ctx->rowoff+ctx->cy+1, ctx->numrows);
    if (len > ctx->screencols) len = ctx->screencols;
    terminal_buffer_append(&ab,status,len);
    while(len < ctx->screencols) {
        if (ctx->screencols - len == rlen) {
            terminal_buffer_append(&ab,rstatus,rlen);
            break;
        } else {
            terminal_buffer_append(&ab," ",1);
            len++;
        }
    }
    terminal_buffer_append(&ab,"\x1b[0m\r\n",6);

    /* Render buffer tabs if multiple buffers are open */
    /* Note: buffer tabs are rendered in a separate line above the status bar */
    /* This is done by the buffers module */

    /* Second row depends on ctx->statusmsg and the status message update time. */
    terminal_buffer_append(&ab,"\x1b[0K",4);
    int msglen = strlen(ctx->statusmsg);
    if (msglen && time(NULL)-ctx->statusmsg_time < 5)
        terminal_buffer_append(&ab,ctx->statusmsg,msglen <= ctx->screencols ? msglen : ctx->screencols);

    /* Render REPL if active */
    if (ctx->repl.active) lua_repl_render(ctx, &ab);

    /* Put cursor at its current position. Note that the horizontal position
     * at which the cursor is displayed may be different compared to 'ctx->cx'
     * because of TABs. */
    int cursor_row = 1;
    int cursor_col = 1;

    /* Calculate cursor position - different for REPL vs editor mode */
    if (ctx->repl.active) {
        /* REPL mode: cursor is on the REPL prompt line */
        int prompt_len = (int)strlen(LUA_REPL_PROMPT);
        int visible = ctx->repl.input_len;
        if (prompt_len + visible >= ctx->screencols) {
            visible = ctx->screencols > prompt_len ? ctx->screencols - prompt_len : 0;
        }
        cursor_row = ctx->screenrows + STATUS_ROWS + LUA_REPL_OUTPUT_ROWS + 1;
        cursor_col = prompt_len + visible + 1;
        if (cursor_col < 1) cursor_col = 1;
        if (cursor_col > ctx->screencols) cursor_col = ctx->screencols;
    } else {
        /* Editor mode: cursor is in the text area */
        int cx = 1;
        int filerow = ctx->rowoff+ctx->cy;
        t_erow *row = (filerow >= ctx->numrows) ? NULL : &ctx->row[filerow];
        if (row) {
            for (int j = ctx->coloff; j < (ctx->cx+ctx->coloff); j++) {
                if (j < row->size && row->chars[j] == TAB)
                    cx += 7-((cx)%8);
                cx++;
            }
        }
        /* Account for line numbers gutter */
        if (ctx->line_numbers && ctx->numrows > 0) {
            int gw = 1;
            int max_ln = ctx->numrows;
            while (max_ln >= 10) { gw++; max_ln /= 10; }
            gw += 1; /* Space separator */
            cx += gw;
        }
        /* Account for tab bar at top if multiple buffers are open */
        int tab_offset = (buffer_count() > 1) ? 1 : 0;
        cursor_row = ctx->cy + 1 + tab_offset;
        cursor_col = cx;
        if (cursor_col > ctx->screencols) cursor_col = ctx->screencols;
    }

    snprintf(buf,sizeof(buf),"\x1b[%d;%dH",cursor_row,cursor_col);
    terminal_buffer_append(&ab,buf,strlen(buf));
    terminal_buffer_append(&ab,"\x1b[?25h",6); /* Show cursor. */
    write(STDOUT_FILENO,ab.b,ab.len);
    terminal_buffer_free(&ab);
}

/* REPL layout management, toggle function, and status reporter are in loki_editor.c */

/* Window size functions are now in loki_terminal.c */

/* ========================= Editor events handling  ======================== */

/* Handle cursor position change because arrow keys were pressed. */
void editor_move_cursor(editor_ctx_t *ctx, int key) {
    int filerow = ctx->rowoff+ctx->cy;
    int filecol = ctx->coloff+ctx->cx;
    int rowlen;
    t_erow *row = (filerow >= ctx->numrows) ? NULL : &ctx->row[filerow];

    switch(key) {
    case ARROW_LEFT:
        if (ctx->cx == 0) {
            if (ctx->coloff) {
                ctx->coloff--;
            } else {
                if (filerow > 0) {
                    ctx->cy--;
                    ctx->cx = ctx->row[filerow-1].size;
                    if (ctx->cx > ctx->screencols-1) {
                        ctx->coloff = ctx->cx-ctx->screencols+1;
                        ctx->cx = ctx->screencols-1;
                    }
                }
            }
        } else {
            ctx->cx -= 1;
        }
        break;
    case ARROW_RIGHT:
        if (row && filecol < row->size) {
            if (ctx->cx == ctx->screencols-1) {
                ctx->coloff++;
            } else {
                ctx->cx += 1;
            }
        } else if (row && filecol == row->size) {
            ctx->cx = 0;
            ctx->coloff = 0;
            if (ctx->cy == ctx->screenrows-1) {
                ctx->rowoff++;
            } else {
                ctx->cy += 1;
            }
        }
        break;
    case ARROW_UP:
        if (ctx->cy == 0) {
            if (ctx->rowoff) ctx->rowoff--;
        } else {
            ctx->cy -= 1;
        }
        break;
    case ARROW_DOWN:
        if (filerow < ctx->numrows) {
            if (ctx->cy == ctx->screenrows-1) {
                ctx->rowoff++;
            } else {
                ctx->cy += 1;
            }
        }
        break;
    }
    /* Fix cx if the current line has not enough chars. */
    filerow = ctx->rowoff+ctx->cy;
    filecol = ctx->coloff+ctx->cx;
    row = (filerow >= ctx->numrows) ? NULL : &ctx->row[filerow];
    rowlen = row ? row->size : 0;
    if (filecol > rowlen) {
        ctx->cx -= filecol-rowlen;
        if (ctx->cx < 0) {
            ctx->coloff += ctx->cx;
            ctx->cx = 0;
        }
    }
}

/* ========================= Modal Key Processing ============================ */

/* Process a single keypress - delegates to modal editing module */
void editor_process_keypress(editor_ctx_t *ctx, int fd) {
    /* All modal editing logic is now in loki_modal.c */
    modal_process_keypress(ctx, fd);
}

void init_editor(editor_ctx_t *ctx) {
    ctx->cx = 0;
    ctx->cy = 0;
    ctx->rowoff = 0;
    ctx->coloff = 0;
    ctx->numrows = 0;
    ctx->row = NULL;
    ctx->dirty = 0;
    ctx->filename = NULL;
    ctx->syntax = NULL;
    ctx->mode = MODE_NORMAL;  /* Start in normal mode (vim-like) */
    ctx->word_wrap = 1;  /* Word wrap enabled by default */
    ctx->sel_active = 0;
    ctx->sel_start_x = ctx->sel_start_y = 0;
    ctx->sel_end_x = ctx->sel_end_y = 0;
    syntax_init_default_colors(ctx);
    /* Lua REPL init and Lua initialization are in loki_editor.c */
    terminal_update_window_size(ctx);
    /* Note: SIGWINCH registration now happens in terminal_host_init() */
    /* Note: editor_for_atexit is set explicitly after buffers_init() */
}

/* Main editor entry point moved to loki_editor.c */
