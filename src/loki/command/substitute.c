/* substitute.c - Search and replace command (:s/old/new/)
 *
 * Vim-style substitution on current line.
 */

#include "command_impl.h"

/* :s/old/new/[g] - Search and replace on current line */
int cmd_substitute(editor_ctx_t *ctx, const char *pattern) {
    if (!pattern || pattern[0] != 's' || pattern[1] != '/') {
        editor_set_status_msg(ctx, "Usage: :s/old/new/[g]");
        return 0;
    }

    /* Parse s/old/new/[g] pattern */
    const char *p = pattern + 2;  /* Skip "s/" */

    /* Find the "old" string (up to next unescaped /) */
    char old_str[256] = {0};
    int old_len = 0;
    while (*p && *p != '/' && old_len < 255) {
        if (*p == '\\' && *(p + 1)) {
            /* Escaped character */
            p++;
            old_str[old_len++] = *p++;
        } else {
            old_str[old_len++] = *p++;
        }
    }
    old_str[old_len] = '\0';

    if (*p != '/') {
        editor_set_status_msg(ctx, "Invalid substitute pattern");
        return 0;
    }
    p++;  /* Skip middle '/' */

    /* Find the "new" string (up to next unescaped / or end) */
    char new_str[256] = {0};
    int new_len = 0;
    while (*p && *p != '/' && new_len < 255) {
        if (*p == '\\' && *(p + 1)) {
            /* Escaped character */
            p++;
            new_str[new_len++] = *p++;
        } else {
            new_str[new_len++] = *p++;
        }
    }
    new_str[new_len] = '\0';

    /* Check for global flag */
    int global = 0;
    if (*p == '/') {
        p++;
        while (*p) {
            if (*p == 'g') global = 1;
            p++;
        }
    }

    if (old_len == 0) {
        editor_set_status_msg(ctx, "Empty search pattern");
        return 0;
    }

    /* Perform substitution on current line */
    if (ctx->cy >= ctx->numrows) {
        editor_set_status_msg(ctx, "No line to substitute");
        return 0;
    }

    t_erow *row = &ctx->row[ctx->cy];
    char *line = row->chars;
    int line_len = row->size;

    /* Build new line with substitutions */
    char new_line[4096] = {0};
    int new_line_len = 0;
    int count = 0;
    int i = 0;

    while (i < line_len && new_line_len < 4090) {
        /* Check for match at current position */
        if (i + old_len <= line_len && strncmp(line + i, old_str, old_len) == 0) {
            /* Found a match - substitute */
            if (new_line_len + new_len < 4090) {
                memcpy(new_line + new_line_len, new_str, new_len);
                new_line_len += new_len;
                i += old_len;
                count++;
                if (!global) {
                    /* Copy rest of line after first substitution */
                    int remaining = line_len - i;
                    if (new_line_len + remaining < 4096) {
                        memcpy(new_line + new_line_len, line + i, remaining);
                        new_line_len += remaining;
                    }
                    break;
                }
            } else {
                break;  /* Output buffer full */
            }
        } else {
            /* No match - copy character */
            new_line[new_line_len++] = line[i++];
        }
    }
    new_line[new_line_len] = '\0';

    if (count == 0) {
        editor_set_status_msg(ctx, "Pattern not found: %s", old_str);
        return 0;
    }

    /* Update the row */
    free(row->chars);
    row->chars = strdup(new_line);
    row->size = new_line_len;

    /* Update render */
    editor_update_row(ctx, row);

    ctx->dirty++;
    editor_set_status_msg(ctx, "%d substitution%s", count, count > 1 ? "s" : "");
    return 1;
}
