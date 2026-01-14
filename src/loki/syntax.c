/* loki_syntax.c - Syntax highlighting implementation
 *
 * This module implements syntax highlighting for the loki editor.
 * It uses a simple token-based approach with support for:
 * - Keywords (primary and type keywords)
 * - String literals (with escape sequence handling)
 * - Single-line and multi-line comments
 * - Numeric literals
 * - Non-printable character visualization
 *
 * The highlighting is performed on the "rendered" version of each row
 * (after tab expansion) and stores highlight types in the row->hl array.
 */

#include "syntax.h"
#include "languages.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* ====================== Syntax highlight color scheme  ==================== */

int syntax_is_separator(int c, char *separators) {
    return c == '\0' || isspace(c) || strchr(separators, c) != NULL;
}

/* Return true if the specified row last char is part of a multi line comment
 * that starts at this row or at one before, and does not end at the end
 * of the row but spawns to the next row. */
int syntax_row_has_open_comment(t_erow *row) {
    if (row->hl && row->rsize && row->hl[row->rsize-1] == HL_MLCOMMENT &&
        (row->rsize < 2 || (row->render[row->rsize-2] != '*' ||
                            row->render[row->rsize-1] != '/'))) return 1;
    return 0;
}

/* Forward declaration for markdown highlighter */
void editor_update_syntax_markdown(editor_ctx_t *ctx, t_erow *row);

/* Map human-readable style names to HL_* constants */
int syntax_name_to_code(const char *name) {
    if (name == NULL) return -1;
    if (strcasecmp(name, "normal") == 0) return HL_NORMAL;
    if (strcasecmp(name, "nonprint") == 0) return HL_NONPRINT;
    if (strcasecmp(name, "comment") == 0) return HL_COMMENT;
    if (strcasecmp(name, "mlcomment") == 0) return HL_MLCOMMENT;
    if (strcasecmp(name, "keyword1") == 0) return HL_KEYWORD1;
    if (strcasecmp(name, "keyword2") == 0) return HL_KEYWORD2;
    if (strcasecmp(name, "string") == 0) return HL_STRING;
    if (strcasecmp(name, "number") == 0) return HL_NUMBER;
    if (strcasecmp(name, "match") == 0) return HL_MATCH;
    return -1;
}

/* Set every byte of row->hl (that corresponds to every character in the line)
 * to the right syntax highlight type (HL_* defines). */
void syntax_update_row(editor_ctx_t *ctx, t_erow *row) {
    unsigned char *new_hl = realloc(row->hl,row->rsize);
    if (new_hl == NULL) return; /* Out of memory, keep old highlighting */
    row->hl = new_hl;
    memset(row->hl,HL_NORMAL,row->rsize);

    int default_ran = 0;

    if (ctx->syntax != NULL) {
        if (ctx->syntax->type == HL_TYPE_MARKDOWN) {
            editor_update_syntax_markdown(ctx, row);
            default_ran = 1;
        } else {
            int i, prev_sep, in_string, in_comment;
            char *p;
            char **keywords = ctx->syntax->keywords;
            char *scs = ctx->syntax->singleline_comment_start;
            char *mcs = ctx->syntax->multiline_comment_start;
            char *mce = ctx->syntax->multiline_comment_end;
            char *separators = ctx->syntax->separators;

            /* Point to the first non-space char. */
            p = row->render;
            i = 0; /* Current char offset */
            while(*p && isspace(*p)) {
                p++;
                i++;
            }
            prev_sep = 1; /* Tell the parser if 'i' points to start of word. */
            in_string = 0; /* Are we inside "" or '' ? */
            in_comment = 0; /* Are we inside multi-line comment? */

            /* If the previous line has an open comment, this line starts
             * with an open comment state. */
            if (row->idx > 0 && syntax_row_has_open_comment(&ctx->row[row->idx-1]))
                in_comment = 1;

            while(*p) {
                /* Handle single-line comments (e.g., //, #, --) */
                if (prev_sep && scs[0] && *p == scs[0] &&
                    (scs[1] == '\0' || (i < row->rsize - 1 && *(p+1) == scs[1]))) {
                    /* From here to end is a comment */
                    memset(row->hl+i,HL_COMMENT,row->rsize-i);
                    break;
                }

                /* Handle multi line comments. */
                if (in_comment) {
                    row->hl[i] = HL_MLCOMMENT;
                    if (i < row->rsize - 1 && *p == mce[0] && *(p+1) == mce[1]) {
                        row->hl[i+1] = HL_MLCOMMENT;
                        p += 2; i += 2;
                        in_comment = 0;
                        prev_sep = 1;
                        continue;
                    } else {
                        prev_sep = 0;
                        p++; i++;
                        continue;
                    }
                } else if (i < row->rsize - 1 && *p == mcs[0] && *(p+1) == mcs[1]) {
                    row->hl[i] = HL_MLCOMMENT;
                    row->hl[i+1] = HL_MLCOMMENT;
                    p += 2; i += 2;
                    in_comment = 1;
                    prev_sep = 0;
                    continue;
                }

                /* Handle "" and '' */
                if (in_string) {
                    row->hl[i] = HL_STRING;
                    if (i < row->rsize - 1 && *p == '\\') {
                        row->hl[i+1] = HL_STRING;
                        p += 2; i += 2;
                        prev_sep = 0;
                        continue;
                    }
                    if (*p == in_string) in_string = 0;
                    p++; i++;
                    continue;
                } else {
                    if (*p == '"' || *p == '\'') {
                        in_string = *p;
                        row->hl[i] = HL_STRING;
                        p++; i++;
                        prev_sep = 0;
                        continue;
                    }
                }

                /* Handle non printable chars. */
                if (!isprint(*p)) {
                    row->hl[i] = HL_NONPRINT;
                    p++; i++;
                    prev_sep = 0;
                    continue;
                }

                /* Handle numbers */
                if ((isdigit(*p) && (prev_sep || row->hl[i-1] == HL_NUMBER)) ||
                    (*p == '.' && i > 0 && row->hl[i-1] == HL_NUMBER &&
                     i < row->rsize - 1 && isdigit(*(p+1)))) {
                    row->hl[i] = HL_NUMBER;
                    p++; i++;
                    prev_sep = 0;
                    continue;
                }

                /* Handle keywords and lib calls */
                if (prev_sep) {
                    int j;
                    for (j = 0; keywords[j]; j++) {
                        int klen = strlen(keywords[j]);
                        int kw2 = keywords[j][klen-1] == '|';
                        if (kw2) klen--;

                        if (i + klen <= row->rsize &&
                            !memcmp(p,keywords[j],klen) &&
                            (i + klen == row->rsize || syntax_is_separator(*(p+klen), separators)))
                        {
                            /* Keyword */
                            memset(row->hl+i,kw2 ? HL_KEYWORD2 : HL_KEYWORD1,klen);
                            p += klen;
                            i += klen;
                            break;
                        }
                    }
                    if (keywords[j] != NULL) {
                        prev_sep = 0;
                        continue; /* We had a keyword match */
                    }
                }

                /* Not special chars */
                prev_sep = syntax_is_separator(*p, separators);
                p++; i++;
            }

            default_ran = 1;
        }
    }

    /* Lua custom highlighting is in loki_editor.c */
    (void)default_ran; /* Suppress unused variable warning */

    /* Propagate syntax change to the next row if the open comment
     * state changed. This may recursively affect all the following rows
     * in the file. */
    int oc = syntax_row_has_open_comment(row);
    if (row->hl_oc != oc && row->idx+1 < ctx->numrows)
        syntax_update_row(ctx, &ctx->row[row->idx+1]);
    row->hl_oc = oc;
}

/* Format RGB color escape sequence for syntax highlighting.
 * Uses true color (24-bit) escape codes: ESC[38;2;R;G;Bm
 * Returns the length of the formatted string. */
int syntax_format_color(editor_ctx_t *ctx, int hl, char *buf, size_t bufsize) {
    if (hl < 0 || hl >= 9) hl = 0;  /* Default to HL_NORMAL */
    t_hlcolor *color = &ctx->colors[hl];
    return snprintf(buf, bufsize, "\x1b[38;2;%d;%d;%dm",
                    color->r, color->g, color->b);
}

/* Select the syntax highlight scheme depending on the filename. */
void syntax_select_for_filename(editor_ctx_t *ctx, char *filename) {
    for (unsigned int j = 0; j < HLDB_ENTRIES; j++) {
        struct t_editor_syntax *s = HLDB+j;
        if (!s->filematch) continue;
        unsigned int i = 0;
        while(s->filematch[i]) {
            char *p;
            int patlen = strlen(s->filematch[i]);
            if ((p = strstr(filename,s->filematch[i])) != NULL) {
                if (s->filematch[i][0] != '.' || p[patlen] == '\0') {
                    ctx->syntax = s;
                    return;
                }
            }
            i++;
        }
    }

    /* Also check dynamic language registry */
    int dynamic_count = get_dynamic_language_count();
    for (int j = 0; j < dynamic_count; j++) {
        struct t_editor_syntax *s = get_dynamic_language(j);
        if (!s || !s->filematch) continue;
        unsigned int i = 0;
        while(s->filematch[i]) {
            char *p;
            int patlen = strlen(s->filematch[i]);
            if ((p = strstr(filename,s->filematch[i])) != NULL) {
                if (s->filematch[i][0] != '.' || p[patlen] == '\0') {
                    ctx->syntax = s;
                    return;
                }
            }
            i++;
        }
    }
}

/* Initialize default syntax highlighting colors.
 * Colors are stored as RGB values and rendered using true color escape codes.
 * These defaults match the visual appearance of the original ANSI color scheme. */
void syntax_init_default_colors(editor_ctx_t *ctx) {
    /* HL_NORMAL */
    ctx->colors[0].r = 200; ctx->colors[0].g = 200; ctx->colors[0].b = 200;
    /* HL_NONPRINT */
    ctx->colors[1].r = 100; ctx->colors[1].g = 100; ctx->colors[1].b = 100;
    /* HL_COMMENT */
    ctx->colors[2].r = 100; ctx->colors[2].g = 100; ctx->colors[2].b = 100;
    /* HL_MLCOMMENT */
    ctx->colors[3].r = 100; ctx->colors[3].g = 100; ctx->colors[3].b = 100;
    /* HL_KEYWORD1 */
    ctx->colors[4].r = 220; ctx->colors[4].g = 100; ctx->colors[4].b = 220;
    /* HL_KEYWORD2 */
    ctx->colors[5].r = 100; ctx->colors[5].g = 220; ctx->colors[5].b = 220;
    /* HL_STRING */
    ctx->colors[6].r = 220; ctx->colors[6].g = 220; ctx->colors[6].b = 100;
    /* HL_NUMBER */
    ctx->colors[7].r = 200; ctx->colors[7].g = 100; ctx->colors[7].b = 200;
    /* HL_MATCH */
    ctx->colors[8].r = 100; ctx->colors[8].g = 150; ctx->colors[8].b = 220;
}
