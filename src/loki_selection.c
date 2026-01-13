/* loki_selection.c - Text selection and clipboard functionality
 *
 * This module handles visual text selection and clipboard operations using
 * OSC 52 escape sequences. OSC 52 allows terminal-based clipboard access
 * that works over SSH and doesn't require X11 or platform-specific APIs.
 *
 * Features:
 * - Visual selection checking (is position within selection?)
 * - Base64 encoding for OSC 52 clipboard protocol
 * - Copy selection to clipboard using terminal escape sequences
 *
 * OSC 52 Protocol:
 * - Sequence: ESC]52;c;<base64_text>BEL
 * - Supported by: xterm, iTerm2, tmux, screen, kitty, alacritty
 * - Works over SSH without X11 forwarding
 */

#include "loki_selection.h"
#include "loki_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

/* Base64 encoding table for OSC 52 clipboard protocol */
static const char base64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* Check if a position (row, col) is within the current selection.
 * Returns 1 if selected, 0 otherwise.
 * Handles both single-line and multi-line selections. */
int is_selected(editor_ctx_t *ctx, int row, int col) {
    if (!ctx->sel_active) return 0;

    int start_y = ctx->sel_start_y;
    int start_x = ctx->sel_start_x;
    int end_y = ctx->sel_end_y;
    int end_x = ctx->sel_end_x;

    /* Ensure start comes before end */
    if (start_y > end_y || (start_y == end_y && start_x > end_x)) {
        int tmp;
        tmp = start_y; start_y = end_y; end_y = tmp;
        tmp = start_x; start_x = end_x; end_x = tmp;
    }

    /* Check if row is in range */
    if (row < start_y || row > end_y) return 0;

    /* Single line selection */
    if (start_y == end_y) {
        return col >= start_x && col < end_x;
    }

    /* Multi-line selection */
    if (row == start_y) {
        return col >= start_x;
    } else if (row == end_y) {
        return col < end_x;
    } else {
        return 1; /* Entire line selected */
    }
}

/* Base64 encode a string for OSC 52 clipboard protocol.
 * Caller must free the returned string.
 * Returns NULL on allocation failure. */
char *base64_encode(const char *input, size_t len) {
    size_t output_len = 4 * ((len + 2) / 3);
    char *output = malloc(output_len + 1);
    if (!output) return NULL;

    size_t i, j;
    for (i = 0, j = 0; i < len;) {
        uint32_t octet_a = i < len ? (unsigned char)input[i++] : 0;
        uint32_t octet_b = i < len ? (unsigned char)input[i++] : 0;
        uint32_t octet_c = i < len ? (unsigned char)input[i++] : 0;
        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

        output[j++] = base64_table[(triple >> 18) & 0x3F];
        output[j++] = base64_table[(triple >> 12) & 0x3F];
        output[j++] = base64_table[(triple >> 6) & 0x3F];
        output[j++] = base64_table[triple & 0x3F];
    }

    /* Add padding */
    for (i = 0; i < (3 - len % 3) % 3; i++)
        output[output_len - 1 - i] = '=';

    output[output_len] = '\0';
    return output;
}

/* Copy selected text to clipboard using OSC 52 escape sequence.
 * This works over SSH and doesn't require X11 or platform-specific clipboard APIs.
 * Clears the selection after successful copy. */
void copy_selection_to_clipboard(editor_ctx_t *ctx) {
    if (!ctx->sel_active) {
        editor_set_status_msg(ctx, "No selection");
        return;
    }

    /* Ensure start comes before end */
    int start_y = ctx->sel_start_y;
    int start_x = ctx->sel_start_x;
    int end_y = ctx->sel_end_y;
    int end_x = ctx->sel_end_x;

    if (start_y > end_y || (start_y == end_y && start_x > end_x)) {
        int tmp;
        tmp = start_y; start_y = end_y; end_y = tmp;
        tmp = start_x; start_x = end_x; end_x = tmp;
    }

    /* Build selected text */
    char *text = NULL;
    size_t text_len = 0;
    size_t text_capacity = 1024;
    text = malloc(text_capacity);
    if (!text) return;

    for (int y = start_y; y <= end_y && y < ctx->numrows; y++) {
        int x_start = (y == start_y) ? start_x : 0;
        int x_end = (y == end_y) ? end_x : ctx->row[y].size;
        if (x_end > ctx->row[y].size) x_end = ctx->row[y].size;

        int len = x_end - x_start;
        if (len > 0) {
            while (text_len + len + 2 > text_capacity) {
                text_capacity *= 2;
                char *new_text = realloc(text, text_capacity);
                if (!new_text) { free(text); return; }
                text = new_text;
            }
            memcpy(text + text_len, ctx->row[y].chars + x_start, len);
            text_len += len;
        }
        if (y < end_y) {
            text[text_len++] = '\n';
        }
    }
    text[text_len] = '\0';

    /* Base64 encode */
    char *encoded = base64_encode(text, text_len);
    free(text);
    if (!encoded) return;

    /* Send OSC 52 sequence: ESC]52;c;<base64>BEL */
    printf("\033]52;c;%s\007", encoded);
    fflush(stdout);
    free(encoded);

    editor_set_status_msg(ctx, "Copied %d bytes to clipboard", (int)text_len);
    ctx->sel_active = 0;  /* Clear selection after copy */
}

/* Get selected text as a newly allocated string.
 * Caller must free the returned string.
 * Returns NULL if no selection or allocation failure. */
char *get_selection_text(editor_ctx_t *ctx) {
    if (!ctx->sel_active) {
        return NULL;
    }

    /* Ensure start comes before end */
    int start_y = ctx->sel_start_y;
    int start_x = ctx->sel_start_x;
    int end_y = ctx->sel_end_y;
    int end_x = ctx->sel_end_x;

    if (start_y > end_y || (start_y == end_y && start_x > end_x)) {
        int tmp;
        tmp = start_y; start_y = end_y; end_y = tmp;
        tmp = start_x; start_x = end_x; end_x = tmp;
    }

    /* Build selected text */
    char *text = NULL;
    size_t text_len = 0;
    size_t text_capacity = 1024;
    text = malloc(text_capacity);
    if (!text) return NULL;

    for (int y = start_y; y <= end_y && y < ctx->numrows; y++) {
        int x_start = (y == start_y) ? start_x : 0;
        int x_end = (y == end_y) ? end_x : ctx->row[y].size;
        if (x_end > ctx->row[y].size) x_end = ctx->row[y].size;

        int len = x_end - x_start;
        if (len > 0) {
            while (text_len + len + 2 > text_capacity) {
                text_capacity *= 2;
                char *new_text = realloc(text, text_capacity);
                if (!new_text) { free(text); return NULL; }
                text = new_text;
            }
            memcpy(text + text_len, ctx->row[y].chars + x_start, len);
            text_len += len;
        }
        if (y < end_y) {
            text[text_len++] = '\n';
        }
    }
    text[text_len] = '\0';

    return text;
}
