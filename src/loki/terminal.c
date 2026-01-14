/* loki_terminal.c - Terminal I/O implementation
 *
 * Platform-specific terminal operations for POSIX systems.
 * Uses termios for raw mode, ioctl for window size, and VT100
 * escape sequences for advanced features.
 */

#include "terminal.h"
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ======================= Static State ===================================== */

/* Original terminal state (saved before entering raw mode) */
static struct termios orig_termios;

/* Global pointer to current editor context for signal handler access.
 * POSIX signal handlers cannot reliably access per-context data, so we
 * maintain a global pointer to the "active" context. This means only one
 * editor instance can properly handle window resize signals at a time.
 * For most use cases (single editor instance), this is not a limitation. */
static editor_ctx_t *signal_context = NULL;

/* ======================= Terminal Mode Management ========================= */

void terminal_disable_raw_mode(editor_ctx_t *ctx, int fd) {
    /* Don't even check the return value as it's too late. */
    if (ctx && ctx->rawmode) {
        /* Exit alternate screen buffer (restores original terminal content)
         * Only if stdout is a terminal (not a pipe or file) */
        if (isatty(STDOUT_FILENO)) {
            (void)write(STDOUT_FILENO, "\x1b[?1049l", 8);
        }
        tcsetattr(fd, TCSAFLUSH, &orig_termios);
        ctx->rawmode = 0;
    }
}

/* Raw mode: 1960 magic shit. */
int terminal_enable_raw_mode(editor_ctx_t *ctx, int fd) {
    struct termios raw;

    if (!ctx) return -1; /* Need valid context */
    if (ctx->rawmode) return 0; /* Already enabled. */
    if (!isatty(STDIN_FILENO)) goto fatal;
    if (tcgetattr(fd, &orig_termios) == -1) goto fatal;

    raw = orig_termios;  /* modify the original mode */
    /* input modes: no break, no CR to NL, no parity check, no strip char,
     * no start/stop output control. */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* output modes - disable post processing */
    raw.c_oflag &= ~(OPOST);
    /* control modes - set 8 bit chars */
    raw.c_cflag |= (CS8);
    /* local modes - choing off, canonical off, no extended functions,
     * no signal chars (^Z,^C) */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /* control chars - set return condition: min number of bytes and timer. */
    raw.c_cc[VMIN] = 0; /* Return each byte, or zero for timeout. */
    raw.c_cc[VTIME] = 1; /* 100 ms timeout (unit is tens of second). */

    /* put terminal in raw mode after flushing */
    if (tcsetattr(fd, TCSAFLUSH, &raw) < 0) goto fatal;
    ctx->rawmode = 1;

    /* Enter alternate screen buffer (saves current screen, gives clean slate)
     * Only if stdout is a terminal (not a pipe or file) */
    if (isatty(STDOUT_FILENO)) {
        (void)write(STDOUT_FILENO, "\x1b[?1049h", 8);
    }

    /* Register this context for signal handling */
    signal_context = ctx;

    return 0;

fatal:
    errno = ENOTTY;
    return -1;
}

/* ======================= Input Reading ==================================== */

/* Read a key from the terminal put in raw mode, trying to handle
 * escape sequences. */
int terminal_read_key(int fd) {
    int nread;
    char c, seq[6];
    int retries = 0;
    /* Wait for input with timeout. If we get too many consecutive
     * zero-byte reads, stdin may be closed. */
    while ((nread = read(fd, &c, 1)) == 0) {
        if (++retries > 1000) {
            /* After ~100 seconds of no input, assume stdin is closed */
            fprintf(stderr, "\nNo input received, exiting.\n");
            exit(0);
        }
    }
    if (nread == -1) exit(1);

    while(1) {
        switch(c) {
        case ESC:    /* escape sequence */
            /* If this is just an ESC, we'll timeout here. */
            if (read(fd, seq, 1) == 0) return ESC;
            if (read(fd, seq+1, 1) == 0) return ESC;

            /* ESC [ sequences. */
            if (seq[0] == '[') {
                if (seq[1] >= '0' && seq[1] <= '9') {
                    /* Extended escape, read additional byte. */
                    if (read(fd, seq+2, 1) == 0) return ESC;
                    if (seq[2] == '~') {
                        switch(seq[1]) {
                        case '3': return DEL_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        }
                    } else if (seq[2] == ';') {
                        /* ESC[1;2X for Shift+Arrow */
                        if (read(fd, seq+3, 1) == 0) return ESC;
                        if (read(fd, seq+4, 1) == 0) return ESC;
                        if (seq[1] == '1' && seq[3] == '2') {
                            switch(seq[4]) {
                            case 'A': return SHIFT_ARROW_UP;
                            case 'B': return SHIFT_ARROW_DOWN;
                            case 'C': return SHIFT_ARROW_RIGHT;
                            case 'D': return SHIFT_ARROW_LEFT;
                            }
                        }
                    } else if (seq[1] == '1' && seq[2] == '3') {
                        /* ESC[13;2u - Shift+Return (kitty keyboard protocol) */
                        if (read(fd, seq+3, 1) == 0) return ESC;
                        if (seq[3] == ';') {
                            if (read(fd, seq+4, 1) == 0) return ESC;
                            if (read(fd, seq+5, 1) == 0) return ESC;
                            if (seq[4] == '2' && seq[5] == 'u') {
                                return SHIFT_RETURN;
                            }
                        }
                    }
                } else {
                    switch(seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                    }
                }
            }

            /* ESC O sequences. */
            else if (seq[0] == 'O') {
                switch(seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
                }
            }
            break;
        default:
            return c;
        }
    }
}

/* ======================= Window Size Detection ============================ */

/* Use the ESC [6n escape sequence to query the horizontal cursor position
 * and return it. On error -1 is returned, on success the position of the
 * cursor is stored at *rows and *cols and 0 is returned. */
int terminal_get_cursor_position(int ifd, int ofd, int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    /* Report cursor location */
    if (write(ofd, "\x1b[6n", 4) != 4) return -1;

    /* Read the response: ESC [ rows ; cols R */
    while (i < sizeof(buf)-1) {
        if (read(ifd, buf+i, 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    /* Parse it. */
    if (buf[0] != ESC || buf[1] != '[') return -1;
    if (sscanf(buf+2, "%d;%d", rows, cols) != 2) return -1;
    return 0;
}

/* Try to get the number of columns in the current terminal. If the ioctl()
 * call fails the function will try to query the terminal itself.
 * Returns 0 on success, -1 on error. */
int terminal_get_window_size(int ifd, int ofd, int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        /* ioctl() failed. Try to query the terminal itself. */
        int orig_row, orig_col, retval;

        /* Get the initial position so we can restore it later. */
        retval = terminal_get_cursor_position(ifd, ofd, &orig_row, &orig_col);
        if (retval == -1) goto failed;

        /* Go to right/bottom margin and get position. */
        if (write(ofd, "\x1b[999C\x1b[999B", 12) != 12) goto failed;
        retval = terminal_get_cursor_position(ifd, ofd, rows, cols);
        if (retval == -1) goto failed;

        /* Restore position. */
        char seq[32];
        snprintf(seq, 32, "\x1b[%d;%dH", orig_row, orig_col);
        if (write(ofd, seq, strlen(seq)) == -1) {
            /* Can't recover... */
        }
        return 0;
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }

failed:
    return -1;
}

/* Update window size and adjust screen layout */
void terminal_update_window_size(editor_ctx_t *ctx) {
    int rows, cols;
    if (terminal_get_window_size(STDIN_FILENO, STDOUT_FILENO,
                      &rows, &cols) == -1) {
        /* If we can't get terminal size (e.g., non-interactive mode), use defaults */
        rows = 24;
        cols = 80;
    }
    ctx->screencols = cols;
    rows -= STATUS_ROWS;
    if (rows < 1) rows = 1;
    ctx->screenrows_total = rows;
    /* REPL layout update (editor_update_repl_layout) is in loki_editor.c */
    ctx->screenrows = ctx->screenrows_total; /* Without REPL, use all available rows */
}

/* ======================= Signal Handling ================================== */

/* Signal handler for window size changes */
void terminal_sig_winch_handler(int unused __attribute__((unused))) {
    /* Signal handler must be async-signal-safe.
     * Just set a flag in the registered context for main loop to handle. */
    if (signal_context) {
        signal_context->winsize_changed = 1;
    }
}

/* Check and handle window resize */
void terminal_handle_resize(editor_ctx_t *ctx) {
    if (!ctx) return;

    if (ctx->winsize_changed) {
        ctx->winsize_changed = 0;
        terminal_update_window_size(ctx);
        if (ctx->cy > ctx->screenrows) ctx->cy = ctx->screenrows - 1;
        if (ctx->cx > ctx->screencols) ctx->cx = ctx->screencols - 1;
    }
}

/* ======================= Screen Buffer ==================================== */

void terminal_buffer_append(struct abuf *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len + len);

    if (new == NULL) {
        /* Out of memory - attempt to restore terminal and exit cleanly */
        write(STDOUT_FILENO, "\x1b[2J", 4);  /* Clear screen */
        write(STDOUT_FILENO, "\x1b[H", 3);   /* Go home */
        perror("Out of memory during screen refresh");
        exit(1);
    }
    memcpy(new + ab->len, s, len);
    ab->b = new;
    ab->len += len;
}

void terminal_buffer_free(struct abuf *ab) {
    free(ab->b);
}
