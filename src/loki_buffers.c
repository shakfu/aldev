/* loki_buffers.c - Multiple buffer management implementation
 *
 * Manages multiple editor contexts (buffers) allowing users to edit
 * multiple files simultaneously with tab-like interface.
 */

#include "loki_buffers.h"
#include "loki_terminal.h"
#include "loki_undo.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Forward declarations */
void editor_insert_row(editor_ctx_t *ctx, int at, char *s, size_t len);

/* Buffer entry - wraps an editor context with metadata */
typedef struct {
    editor_ctx_t ctx;       /* Editor context for this buffer */
    int id;                 /* Unique buffer ID */
    int active;             /* 1 if this slot is in use, 0 if free */
    char display_name[64];  /* Cached display name for tabs */
} buffer_entry_t;

/* Global buffer state */
static struct {
    buffer_entry_t buffers[MAX_BUFFERS];  /* Array of buffer slots */
    int current_buffer_id;                /* ID of currently active buffer */
    int next_id;                          /* Next ID to assign */
    int initialized;                      /* 1 if buffers_init() was called */
} buffer_state = {0};

/* ======================== Helper Functions ======================== */

/* Find buffer entry by ID
 * Returns: Pointer to buffer entry, or NULL if not found */
static buffer_entry_t *find_buffer(int buffer_id) {
    for (int i = 0; i < MAX_BUFFERS; i++) {
        if (buffer_state.buffers[i].active && buffer_state.buffers[i].id == buffer_id) {
            return &buffer_state.buffers[i];
        }
    }
    return NULL;
}

/* Find first free buffer slot
 * Returns: Pointer to free slot, or NULL if all slots full */
static buffer_entry_t *find_free_slot(void) {
    for (int i = 0; i < MAX_BUFFERS; i++) {
        if (!buffer_state.buffers[i].active) {
            return &buffer_state.buffers[i];
        }
    }
    return NULL;
}

/* Update display name for buffer */
static void update_display_name(buffer_entry_t *buf) {
    if (!buf) return;

    if (buf->ctx.filename && buf->ctx.filename[0] != '\0') {
        /* Extract basename from path */
        const char *basename = strrchr(buf->ctx.filename, '/');
        basename = basename ? basename + 1 : buf->ctx.filename;

        /* Truncate if too long */
        if (strlen(basename) > 50) {
            snprintf(buf->display_name, sizeof(buf->display_name), "...%s", basename + strlen(basename) - 47);
        } else {
            snprintf(buf->display_name, sizeof(buf->display_name), "%s", basename);
        }
    } else {
        snprintf(buf->display_name, sizeof(buf->display_name), "[No Name]");
    }
}

/* ======================== Initialization ======================== */

int buffers_init(editor_ctx_t *initial_ctx) {
    if (buffer_state.initialized) {
        return -1;  /* Already initialized */
    }

    /* Initialize buffer array */
    memset(buffer_state.buffers, 0, sizeof(buffer_state.buffers));
    buffer_state.current_buffer_id = -1;
    buffer_state.next_id = 1;
    buffer_state.initialized = 1;

    /* Create first buffer using the provided context */
    buffer_entry_t *first = &buffer_state.buffers[0];
    first->active = 1;
    first->id = buffer_state.next_id++;

    /* Initialize fresh context and copy only essential state from initial_ctx */
    editor_ctx_init(&first->ctx);

    /* Copy terminal/display state */
    first->ctx.screencols = initial_ctx->screencols;
    first->ctx.screenrows = initial_ctx->screenrows;
    first->ctx.screenrows_total = initial_ctx->screenrows_total;
    first->ctx.rawmode = initial_ctx->rawmode;
    first->ctx.L = initial_ctx->L;
    memcpy(first->ctx.colors, initial_ctx->colors, sizeof(first->ctx.colors));

    /* Copy buffer content (rows) */
    first->ctx.numrows = initial_ctx->numrows;
    first->ctx.row = initial_ctx->row;
    first->ctx.filename = initial_ctx->filename;
    first->ctx.syntax = initial_ctx->syntax;
    first->ctx.dirty = initial_ctx->dirty;

    /* Copy cursor state */
    first->ctx.cx = initial_ctx->cx;
    first->ctx.cy = initial_ctx->cy;
    first->ctx.rowoff = initial_ctx->rowoff;
    first->ctx.coloff = initial_ctx->coloff;

    update_display_name(first);
    buffer_state.current_buffer_id = first->id;

    return 0;
}

void buffers_free(void) {
    if (!buffer_state.initialized) return;

    /* Free all active buffers */
    for (int i = 0; i < MAX_BUFFERS; i++) {
        if (buffer_state.buffers[i].active) {
            editor_ctx_free(&buffer_state.buffers[i].ctx);
            buffer_state.buffers[i].active = 0;
        }
    }

    buffer_state.initialized = 0;
    buffer_state.current_buffer_id = -1;
}

/* ======================== Buffer Operations ======================== */

int buffer_create(const char *filename) {
    if (!buffer_state.initialized) return -1;

    /* Get current buffer context to copy terminal state BEFORE creating new one */
    editor_ctx_t *template_ctx = buffer_get_current();

    /* Find free slot */
    buffer_entry_t *buf = find_free_slot();
    if (!buf) {
        return -1;  /* No free slots */
    }

    /* Initialize new buffer */
    buf->active = 1;
    buf->id = buffer_state.next_id++;

    /* Initialize editor context */
    editor_ctx_init(&buf->ctx);

    /* Copy terminal/display state from template buffer */
    if (template_ctx) {
        buf->ctx.screencols = template_ctx->screencols;
        buf->ctx.screenrows = template_ctx->screenrows;
        buf->ctx.screenrows_total = template_ctx->screenrows_total;
        buf->ctx.rawmode = template_ctx->rawmode;
        buf->ctx.L = template_ctx->L;  /* Share Lua state */
        /* Copy color scheme */
        memcpy(buf->ctx.colors, template_ctx->colors, sizeof(buf->ctx.colors));
    }

    /* Initialize undo system for new buffer */
    undo_init(&buf->ctx, 1000, 10 * 1024 * 1024);  /* 1000 ops, 10MB limit */

    /* Open file if provided */
    if (filename) {
        if (editor_open(&buf->ctx, (char *)filename) != 0) {
            /* Failed to open file - clean up */
            editor_ctx_free(&buf->ctx);
            buf->active = 0;
            return -1;
        }
    } else {
        /* Empty buffer - insert one empty row so it displays properly */
        editor_insert_row(&buf->ctx, 0, "", 0);
        /* Reset dirty flag - empty buffer shouldn't be marked as modified */
        buf->ctx.dirty = 0;
    }

    update_display_name(buf);

    return buf->id;
}

int buffer_close(int buffer_id, int force) {
    if (!buffer_state.initialized) return -1;

    buffer_entry_t *buf = find_buffer(buffer_id);
    if (!buf) return -1;

    /* Check for unsaved changes */
    if (!force && buf->ctx.dirty) {
        return 1;  /* Has unsaved changes */
    }

    /* Can't close last buffer */
    if (buffer_count() <= 1) {
        return -1;
    }

    /* If closing current buffer, switch to another first */
    if (buffer_id == buffer_state.current_buffer_id) {
        /* Try to switch to next buffer */
        int next_id = buffer_next();
        if (next_id == buffer_id) {
            /* Only one buffer left - can't close */
            return -1;
        }
    }

    /* Free buffer resources */
    editor_ctx_free(&buf->ctx);
    buf->active = 0;

    return 0;
}

int buffer_switch(int buffer_id) {
    if (!buffer_state.initialized) return -1;

    buffer_entry_t *buf = find_buffer(buffer_id);
    if (!buf) return -1;

    /* Save current state before switching */
    buffers_save_current_state();

    /* Switch to new buffer */
    buffer_state.current_buffer_id = buffer_id;

    /* Restore new buffer state */
    buffers_restore_state(&buf->ctx);

    return 0;
}

int buffer_next(void) {
    if (!buffer_state.initialized || buffer_count() == 0) return -1;

    int found_current = 0;

    /* Find next active buffer after current */
    for (int i = 0; i < MAX_BUFFERS; i++) {
        if (!buffer_state.buffers[i].active) continue;

        if (found_current) {
            /* Found next buffer */
            buffer_switch(buffer_state.buffers[i].id);
            return buffer_state.buffers[i].id;
        }

        if (buffer_state.buffers[i].id == buffer_state.current_buffer_id) {
            found_current = 1;
        }
    }

    /* Wrap around to first buffer */
    for (int i = 0; i < MAX_BUFFERS; i++) {
        if (buffer_state.buffers[i].active) {
            buffer_switch(buffer_state.buffers[i].id);
            return buffer_state.buffers[i].id;
        }
    }

    return -1;
}

int buffer_prev(void) {
    if (!buffer_state.initialized || buffer_count() == 0) return -1;

    int prev_id = -1;

    /* Find previous active buffer before current */
    for (int i = 0; i < MAX_BUFFERS; i++) {
        if (!buffer_state.buffers[i].active) continue;

        if (buffer_state.buffers[i].id == buffer_state.current_buffer_id) {
            /* Found current, prev_id has previous */
            if (prev_id != -1) {
                buffer_switch(prev_id);
                return prev_id;
            }
            break;
        }

        prev_id = buffer_state.buffers[i].id;
    }

    /* Wrap around to last buffer */
    for (int i = MAX_BUFFERS - 1; i >= 0; i--) {
        if (buffer_state.buffers[i].active) {
            buffer_switch(buffer_state.buffers[i].id);
            return buffer_state.buffers[i].id;
        }
    }

    return -1;
}

/* ======================== Query Functions ======================== */

editor_ctx_t *buffer_get_current(void) {
    if (!buffer_state.initialized) return NULL;

    buffer_entry_t *buf = find_buffer(buffer_state.current_buffer_id);
    return buf ? &buf->ctx : NULL;
}

editor_ctx_t *buffer_get(int buffer_id) {
    if (!buffer_state.initialized) return NULL;

    buffer_entry_t *buf = find_buffer(buffer_id);
    return buf ? &buf->ctx : NULL;
}

int buffer_get_current_id(void) {
    return buffer_state.initialized ? buffer_state.current_buffer_id : -1;
}

int buffer_count(void) {
    if (!buffer_state.initialized) return 0;

    int count = 0;
    for (int i = 0; i < MAX_BUFFERS; i++) {
        if (buffer_state.buffers[i].active) {
            count++;
        }
    }
    return count;
}

int buffer_get_list(int *ids) {
    if (!buffer_state.initialized || !ids) return 0;

    int count = 0;
    for (int i = 0; i < MAX_BUFFERS && count < MAX_BUFFERS; i++) {
        if (buffer_state.buffers[i].active) {
            ids[count++] = buffer_state.buffers[i].id;
        }
    }
    return count;
}

const char *buffer_get_display_name(int buffer_id) {
    buffer_entry_t *buf = find_buffer(buffer_id);
    return buf ? buf->display_name : NULL;
}

int buffer_is_modified(int buffer_id) {
    buffer_entry_t *buf = find_buffer(buffer_id);
    if (!buf) return -1;
    return buf->ctx.dirty ? 1 : 0;
}

/* ======================== State Management ======================== */

void buffers_save_current_state(void) {
    /* Current state is already in the buffer context - nothing to do.
     * The editor context contains all necessary state (cursor position, etc.)
     * This function is here for future extensions if needed. */
}

void buffers_restore_state(editor_ctx_t *ctx) {
    /* State is restored by switching the active context pointer.
     * This function is here for future extensions if needed. */
    (void)ctx;  /* Unused for now */
}

/* ======================== Utility Functions ======================== */

void buffer_update_display_name(int buffer_id) {
    buffer_entry_t *buf = find_buffer(buffer_id);
    if (buf) {
        update_display_name(buf);
    }
}

/* ======================== Rendering ======================== */

void buffers_render_tabs(struct abuf *ab, int max_width) {
    if (!buffer_state.initialized || !ab) return;

    int count = buffer_count();
    if (count <= 1) {
        /* Single buffer - don't show tabs */
        return;
    }

    (void)max_width;  /* Not needed for simple numeric tabs */

    /* Render each active buffer as a simple numbered tab: [1] [2] [3] */
    for (int i = 0; i < MAX_BUFFERS; i++) {
        if (!buffer_state.buffers[i].active) continue;

        buffer_entry_t *buf = &buffer_state.buffers[i];
        int is_current = (buf->id == buffer_state.current_buffer_id);

        /* Highlight current tab with reverse video */
        if (is_current) {
            terminal_buffer_append(ab, "\x1b[7m", 4);  /* Reverse video */
        }

        /* Simple tab format: [N] where N is the buffer ID */
        char tab_str[8];
        snprintf(tab_str, sizeof(tab_str), "[%d]", buf->id);
        terminal_buffer_append(ab, tab_str, strlen(tab_str));

        if (is_current) {
            terminal_buffer_append(ab, "\x1b[m", 3);   /* Reset */
        }

        terminal_buffer_append(ab, " ", 1);  /* Space between tabs */
    }

    /* Clear to end of line and add newline to separate tabs from status bar */
    terminal_buffer_append(ab, "\x1b[0K", 4);  /* Clear to end of line */
    terminal_buffer_append(ab, "\r\n", 2);
}
