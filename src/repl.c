/**
 * @file repl.c
 * @brief Common REPL infrastructure - line editor, terminal handling, history.
 *
 * This file contains the shared infrastructure used by all language REPLs.
 * Language-specific REPL implementations are in src/lang/{alda,joy,tr7}/repl.c
 */

#include "repl.h"
#include "loki/core.h"
#include "loki/terminal.h"
#include "loki/syntax.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>

/* ============================================================================
 * Line Editor State Management
 * ============================================================================ */

void repl_editor_init(ReplLineEditor *ed) {
    memset(ed, 0, sizeof(*ed));
    ed->history_idx = -1;
}

void repl_editor_cleanup(ReplLineEditor *ed) {
    for (int i = 0; i < ed->history_len; i++) {
        free(ed->history[i]);
    }
    memset(ed, 0, sizeof(*ed));
}

void repl_add_history(ReplLineEditor *ed, const char *line) {
    if (!line || !line[0]) return;

    /* Don't add duplicates of the last entry */
    if (ed->history_len > 0 && strcmp(ed->history[ed->history_len - 1], line) == 0) {
        return;
    }

    /* Remove oldest if full */
    if (ed->history_len >= REPL_HISTORY_MAX) {
        free(ed->history[0]);
        memmove(ed->history, ed->history + 1, (REPL_HISTORY_MAX - 1) * sizeof(char *));
        ed->history_len--;
    }

    ed->history[ed->history_len++] = strdup(line);
}

int repl_history_load(ReplLineEditor *ed, const char *filepath) {
    if (!ed || !filepath) return -1;

    FILE *f = fopen(filepath, "r");
    if (!f) return -1;  /* File doesn't exist yet, not an error */

    char line[MAX_INPUT_LENGTH];
    while (fgets(line, sizeof(line), f)) {
        /* Remove trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        /* Skip empty lines */
        if (line[0] != '\0') {
            repl_add_history(ed, line);
        }
    }

    fclose(f);
    return 0;
}

int repl_history_save(ReplLineEditor *ed, const char *filepath) {
    if (!ed || !filepath || !filepath[0]) return -1;

    FILE *f = fopen(filepath, "w");
    if (!f) return -1;

    for (int i = 0; i < ed->history_len; i++) {
        fprintf(f, "%s\n", ed->history[i]);
    }

    fclose(f);
    return 0;
}

/* ============================================================================
 * Terminal Raw Mode
 * ============================================================================ */

/* Original termios for REPL raw mode (separate from editor) */
static struct termios repl_orig_termios;
static int repl_rawmode = 0;

void repl_disable_raw_mode(void) {
    if (repl_rawmode) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &repl_orig_termios);
        repl_rawmode = 0;
    }
}

int repl_enable_raw_mode(void) {
    struct termios raw;

    if (repl_rawmode) return 0;
    if (!isatty(STDIN_FILENO)) return -1;
    if (tcgetattr(STDIN_FILENO, &repl_orig_termios) == -1) return -1;

    raw = repl_orig_termios;
    /* Input modes: no break, no CR to NL, no parity, no strip, no flow ctrl */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* Output modes - keep post processing for proper newline handling */
    /* raw.c_oflag &= ~(OPOST); -- keep OPOST for REPL */
    /* Control modes - 8 bit chars */
    raw.c_cflag |= (CS8);
    /* Local modes - echo off, canonical off, no extended functions
     * Keep ISIG so Ctrl-C still works for interrupt */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    /* Return each byte immediately */
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) return -1;
    repl_rawmode = 1;
    return 0;
}

/* ============================================================================
 * Syntax Highlighting
 * ============================================================================ */

static int repl_is_separator(int c, const char *separators) {
    if (isspace(c)) return 1;
    if (c == '\0') return 1;
    return (separators && strchr(separators, c) != NULL);
}

void repl_highlight_line(editor_ctx_t *syntax_ctx, ReplLineEditor *ed) {
    struct t_editor_syntax *syn = syntax_ctx->syntax;

    memset(ed->hl, HL_NORMAL, ed->len);

    if (!syn || ed->len == 0) return;

    char **keywords = syn->keywords;
    char *scs = syn->singleline_comment_start;
    char *separators = syn->separators;
    int highlight_strings = syn->flags & HL_HIGHLIGHT_STRINGS;
    int highlight_numbers = syn->flags & HL_HIGHLIGHT_NUMBERS;

    int i = 0;
    int prev_sep = 1;
    int in_string = 0;
    char *p = ed->buf;

    /* Skip leading whitespace */
    while (*p && isspace(*p) && i < ed->len) {
        p++;
        i++;
    }

    while (*p && i < ed->len) {
        /* Handle single-line comments */
        if (prev_sep && scs && scs[0] && *p == scs[0] &&
            (scs[1] == '\0' || (i < ed->len - 1 && *(p+1) == scs[1]))) {
            memset(ed->hl + i, HL_COMMENT, ed->len - i);
            break;
        }

        /* Handle strings */
        if (in_string) {
            ed->hl[i] = HL_STRING;
            if (*p == '\\' && i < ed->len - 1) {
                ed->hl[i+1] = HL_STRING;
                p += 2;
                i += 2;
                continue;
            }
            if (*p == in_string) in_string = 0;
            p++;
            i++;
            prev_sep = 1;
            continue;
        } else if (highlight_strings && (*p == '"' || *p == '\'')) {
            in_string = *p;
            ed->hl[i] = HL_STRING;
            p++;
            i++;
            prev_sep = 0;
            continue;
        }

        /* Handle numbers */
        if (highlight_numbers && (isdigit(*p) || (*p == '.' && i < ed->len - 1 && isdigit(*(p+1)))) && prev_sep) {
            while (i < ed->len && (isdigit(*p) || *p == '.')) {
                ed->hl[i] = HL_NUMBER;
                p++;
                i++;
            }
            prev_sep = 0;
            continue;
        }

        /* Handle keywords */
        if (prev_sep && keywords) {
            int keyword_found = 0;
            for (int j = 0; keywords[j]; j++) {
                size_t klen = strlen(keywords[j]);
                int kw2 = (klen > 0 && keywords[j][klen-1] == '|');
                if (kw2) klen--;

                /* Safety: skip zero-length keywords */
                if (klen == 0) continue;

                if ((int)(i + klen) <= ed->len &&
                    memcmp(p, keywords[j], klen) == 0 &&
                    repl_is_separator(p[klen], separators)) {
                    int hl_type = kw2 ? HL_KEYWORD2 : HL_KEYWORD1;
                    memset(ed->hl + i, hl_type, klen);
                    p += klen;
                    i += (int)klen;
                    prev_sep = 0;
                    keyword_found = 1;
                    break;
                }
            }
            if (keyword_found) continue;
        }

        prev_sep = repl_is_separator(*p, separators);
        p++;
        i++;
    }
}

void repl_render_line(editor_ctx_t *syntax_ctx, ReplLineEditor *ed, const char *prompt) {
    struct abuf ab = ABUF_INIT;

    /* Move to start of line, clear it */
    terminal_buffer_append(&ab, "\r\x1b[K", 4);

    /* Output prompt (no highlighting) */
    terminal_buffer_append(&ab, prompt, strlen(prompt));

    /* Highlight the input */
    repl_highlight_line(syntax_ctx, ed);

    /* Output highlighted text */
    int current_hl = -1;
    for (int i = 0; i < ed->len; i++) {
        if (ed->hl[i] != current_hl) {
            char color[32];
            int clen = syntax_format_color(syntax_ctx, ed->hl[i], color, sizeof(color));
            terminal_buffer_append(&ab, color, clen);
            current_hl = ed->hl[i];
        }
        terminal_buffer_append(&ab, &ed->buf[i], 1);
    }

    /* Reset color */
    terminal_buffer_append(&ab, "\x1b[39m", 5);

    /* Position cursor */
    if (ed->pos < ed->len) {
        /* Move cursor back from end */
        char pos[32];
        int plen = snprintf(pos, sizeof(pos), "\x1b[%dD", ed->len - ed->pos);
        terminal_buffer_append(&ab, pos, plen);
    }

    write(STDOUT_FILENO, ab.b, ab.len);
    terminal_buffer_free(&ab);
}

/* ============================================================================
 * Line Reading
 * ============================================================================ */

char *repl_readline(editor_ctx_t *syntax_ctx, ReplLineEditor *ed, const char *prompt) {
    /* Reset editor state for new line */
    ed->buf[0] = '\0';
    ed->len = 0;
    ed->pos = 0;
    ed->history_idx = -1;

    /* Initial render */
    repl_render_line(syntax_ctx, ed, prompt);

    while (1) {
        fflush(stdout); /* Ensure output is flushed before blocking read */
        int c = terminal_read_key(STDIN_FILENO);

        if (c == ENTER) {
            /* Submit line */
            write(STDOUT_FILENO, "\r\n", 2);
            ed->buf[ed->len] = '\0';
            return ed->buf;
        }

        if (c == CTRL_C) {
            /* Cancel current line */
            ed->buf[0] = '\0';
            ed->len = 0;
            ed->pos = 0;
            write(STDOUT_FILENO, "^C\r\n", 4);
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == CTRL_D) {
            /* EOF on empty line */
            if (ed->len == 0) {
                write(STDOUT_FILENO, "\r\n", 2);
                return NULL;
            }
            /* Delete char at cursor */
            if (ed->pos < ed->len) {
                memmove(&ed->buf[ed->pos], &ed->buf[ed->pos + 1], ed->len - ed->pos);
                ed->len--;
            }
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == BACKSPACE || c == CTRL_H) {
            /* Delete char before cursor */
            if (ed->pos > 0) {
                memmove(&ed->buf[ed->pos - 1], &ed->buf[ed->pos], ed->len - ed->pos + 1);
                ed->pos--;
                ed->len--;
            }
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == DEL_KEY) {
            /* Delete char at cursor */
            if (ed->pos < ed->len) {
                memmove(&ed->buf[ed->pos], &ed->buf[ed->pos + 1], ed->len - ed->pos);
                ed->len--;
            }
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == ARROW_LEFT) {
            if (ed->pos > 0) ed->pos--;
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == ARROW_RIGHT) {
            if (ed->pos < ed->len) ed->pos++;
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == HOME_KEY || c == CTRL_A) {
            ed->pos = 0;
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == END_KEY || c == CTRL_E) {
            ed->pos = ed->len;
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == ARROW_UP) {
            /* Previous history */
            if (ed->history_len == 0) continue;

            if (ed->history_idx == -1) {
                /* Save current input */
                memcpy(ed->saved_buf, ed->buf, ed->len + 1);
                ed->saved_len = ed->len;
                ed->history_idx = ed->history_len - 1;
            } else if (ed->history_idx > 0) {
                ed->history_idx--;
            } else {
                continue;
            }

            /* Load history entry */
            strcpy(ed->buf, ed->history[ed->history_idx]);
            ed->len = strlen(ed->buf);
            ed->pos = ed->len;
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == ARROW_DOWN) {
            /* Next history */
            if (ed->history_idx == -1) continue;

            if (ed->history_idx < ed->history_len - 1) {
                ed->history_idx++;
                strcpy(ed->buf, ed->history[ed->history_idx]);
                ed->len = strlen(ed->buf);
                ed->pos = ed->len;
            } else {
                /* Restore saved input */
                ed->history_idx = -1;
                memcpy(ed->buf, ed->saved_buf, ed->saved_len + 1);
                ed->len = ed->saved_len;
                ed->pos = ed->len;
            }
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == CTRL_U) {
            /* Clear line */
            ed->buf[0] = '\0';
            ed->len = 0;
            ed->pos = 0;
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        if (c == CTRL_K) {
            /* Kill to end of line */
            ed->len = ed->pos;
            ed->buf[ed->len] = '\0';
            repl_render_line(syntax_ctx, ed, prompt);
            continue;
        }

        /* Insert printable character */
        if (c >= 32 && c < 127 && ed->len < MAX_INPUT_LENGTH - 1) {
            memmove(&ed->buf[ed->pos + 1], &ed->buf[ed->pos], ed->len - ed->pos + 1);
            ed->buf[ed->pos] = c;
            ed->pos++;
            ed->len++;
            repl_render_line(syntax_ctx, ed, prompt);
        }
    }
}
