/* loki_undo.c - Undo/Redo state management
 *
 * Implements undo/redo with operation grouping and memory limits.
 * Operations are grouped by heuristics (time gap, cursor movement, operation type).
 */

#include "loki_undo.h"
#include "loki_internal.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

/* Forward declarations of editor functions we need */
void editor_row_insert_char(editor_ctx_t *ctx, t_erow *row, int at, int c);
void editor_row_del_char(editor_ctx_t *ctx, t_erow *row, int at);
void editor_insert_row(editor_ctx_t *ctx, int at, char *s, size_t len);
void editor_del_row(editor_ctx_t *ctx, int at);

/* Undo grouping heuristics */
#define UNDO_GROUP_TIMEOUT 2         /* 2 seconds gap = new group */
#define UNDO_GROUP_MOVEMENT_GAP 2    /* Cursor moved >2 positions = new group */

/* Undo state (private to this module) */
struct undo_state {
    undo_entry_t *entries;   /* Circular buffer of undo entries */
    int capacity;            /* Max entries (e.g., 1000) */
    int count;               /* Current number of entries */
    int head;                /* Write position (next entry goes here) */
    int current;             /* Current position in undo stack */

    int next_group_id;       /* Next group ID to assign */
    int current_group_id;    /* Current operation group */

    /* Grouping heuristics */
    time_t last_edit_time;   /* Timestamp of last edit */
    int last_edit_row;       /* Row of last edit */
    int last_edit_col;       /* Column of last edit */
    undo_op_type_t last_op;  /* Type of last operation */

    /* Memory tracking */
    size_t memory_used;      /* Bytes used by undo data */
    size_t memory_limit;     /* Max bytes (e.g., 10MB) */
};

/* ======================== Initialization ======================== */

void undo_init(editor_ctx_t *ctx, int capacity, size_t memory_limit) {
    struct undo_state *undo = malloc(sizeof(struct undo_state));
    if (!undo) return;

    undo->entries = calloc(capacity, sizeof(undo_entry_t));
    if (!undo->entries) {
        free(undo);
        return;
    }

    undo->capacity = capacity;
    undo->count = 0;
    undo->head = 0;
    undo->current = 0;
    undo->next_group_id = 1;
    undo->current_group_id = 0;
    undo->last_edit_time = 0;
    undo->last_edit_row = -1;
    undo->last_edit_col = -1;
    undo->last_op = (undo_op_type_t)-1;
    undo->memory_used = 0;
    undo->memory_limit = memory_limit;

    ctx->undo_state = undo;
}

void undo_free(editor_ctx_t *ctx) {
    if (!ctx->undo_state) return;

    struct undo_state *undo = ctx->undo_state;

    /* Free all line_op content strings */
    for (int i = 0; i < undo->count; i++) {
        undo_entry_t *e = &undo->entries[i];
        if ((e->type == UNDO_INSERT_LINE || e->type == UNDO_DELETE_LINE) &&
            e->data.line_op.content) {
            free(e->data.line_op.content);
        }
    }

    free(undo->entries);
    free(undo);
    ctx->undo_state = NULL;
}

/* ======================== Grouping Logic ======================== */

/* Should we start a new undo group for this operation? */
static int should_break_group(struct undo_state *undo, undo_op_type_t op,
                               int row, int col) {
    if (undo->current_group_id == 0) return 1;  /* First operation */

    /* Time gap check */
    time_t now = time(NULL);
    if (now - undo->last_edit_time > UNDO_GROUP_TIMEOUT) {
        return 1;
    }

    /* Operation type change (insert→delete or vice versa) */
    if (undo->last_op == UNDO_INSERT_CHAR && op == UNDO_DELETE_CHAR) return 1;
    if (undo->last_op == UNDO_DELETE_CHAR && op == UNDO_INSERT_CHAR) return 1;

    /* Line operations always break groups */
    if (op == UNDO_INSERT_LINE || op == UNDO_DELETE_LINE) return 1;

    /* Cursor jumped (user moved cursor manually) */
    if (undo->last_edit_row != row) return 1;

    int col_gap = abs(col - undo->last_edit_col);
    if (col_gap > UNDO_GROUP_MOVEMENT_GAP) return 1;

    return 0;  /* Continue current group */
}

void undo_break_group(editor_ctx_t *ctx) {
    if (!ctx->undo_state) return;

    struct undo_state *undo = ctx->undo_state;
    if (undo->count > 0) {
        int last_idx = (undo->head - 1 + undo->capacity) % undo->capacity;
        undo->entries[last_idx].group_break = 1;
    }
    undo->current_group_id = 0;  /* Force new group on next operation */
}

/* ======================== Recording Operations ======================== */

static void free_entry_data(undo_entry_t *entry, struct undo_state *undo) {
    if ((entry->type == UNDO_INSERT_LINE || entry->type == UNDO_DELETE_LINE) &&
        entry->data.line_op.content) {
        undo->memory_used -= entry->data.line_op.length;
        free(entry->data.line_op.content);
        entry->data.line_op.content = NULL;
    }
}

static void record_operation(editor_ctx_t *ctx, undo_entry_t *entry) {
    struct undo_state *undo = ctx->undo_state;
    if (!undo) return;

    /* Check if we should start new group */
    if (should_break_group(undo, entry->type, entry->row, entry->col)) {
        undo->current_group_id = undo->next_group_id++;
    }

    entry->group_id = undo->current_group_id;
    entry->group_break = 0;

    /* Update grouping heuristics */
    undo->last_edit_time = time(NULL);
    undo->last_edit_row = entry->row;
    undo->last_edit_col = entry->col;
    undo->last_op = entry->type;

    /* If we've undone operations, discard redo history */
    if (undo->current < undo->count) {
        /* Free any line content in discarded entries */
        for (int i = undo->current; i < undo->count; i++) {
            int idx = (undo->head - undo->count + i + undo->capacity) % undo->capacity;
            free_entry_data(&undo->entries[idx], undo);
        }
        undo->count = undo->current;
    }

    /* Add entry to circular buffer */
    if (undo->count == undo->capacity) {
        /* Buffer full - evict oldest entry */
        int evict_idx = undo->head;
        free_entry_data(&undo->entries[evict_idx], undo);
    } else {
        undo->count++;
    }

    /* Write entry */
    undo->entries[undo->head] = *entry;
    undo->head = (undo->head + 1) % undo->capacity;
    undo->current = undo->count;

    /* Track memory for line operations */
    if (entry->type == UNDO_INSERT_LINE || entry->type == UNDO_DELETE_LINE) {
        undo->memory_used += entry->data.line_op.length;
    }
}

void undo_record_insert_char(editor_ctx_t *ctx, int row, int col, char ch) {
    if (!ctx->undo_state) return;

    undo_entry_t entry = {
        .type = UNDO_INSERT_CHAR,
        .row = row,
        .col = col,
        .data.char_op.ch = ch,
        .cursor_row = ctx->cy,
        .cursor_col = ctx->cx,
        .cursor_rowoff = ctx->rowoff,
        .cursor_coloff = ctx->coloff
    };

    record_operation(ctx, &entry);
}

void undo_record_delete_char(editor_ctx_t *ctx, int row, int col, char ch) {
    if (!ctx->undo_state) return;

    undo_entry_t entry = {
        .type = UNDO_DELETE_CHAR,
        .row = row,
        .col = col,
        .data.char_op.ch = ch,
        .cursor_row = ctx->cy,
        .cursor_col = ctx->cx,
        .cursor_rowoff = ctx->rowoff,
        .cursor_coloff = ctx->coloff
    };

    record_operation(ctx, &entry);
}

void undo_record_insert_line(editor_ctx_t *ctx, int row, int col,
                              const char *content, int length) {
    if (!ctx->undo_state) return;

    undo_entry_t entry = {
        .type = UNDO_INSERT_LINE,
        .row = row,
        .col = col,
        .data.line_op.content = strndup(content, length),
        .data.line_op.length = length,
        .cursor_row = ctx->cy,
        .cursor_col = ctx->cx,
        .cursor_rowoff = ctx->rowoff,
        .cursor_coloff = ctx->coloff
    };

    record_operation(ctx, &entry);
}

void undo_record_delete_line(editor_ctx_t *ctx, int row, int col,
                              const char *content, int length) {
    if (!ctx->undo_state) return;

    undo_entry_t entry = {
        .type = UNDO_DELETE_LINE,
        .row = row,
        .col = col,
        .data.line_op.content = strndup(content, length),
        .data.line_op.length = length,
        .cursor_row = ctx->cy,
        .cursor_col = ctx->cx,
        .cursor_rowoff = ctx->rowoff,
        .cursor_coloff = ctx->coloff
    };

    record_operation(ctx, &entry);
}

/* ======================== Undo/Redo Operations ======================== */

/* Apply single undo operation (reverse the operation) */
static void apply_undo(editor_ctx_t *ctx, undo_entry_t *entry) {
    /* Suppress undo recording while applying undo */
    struct undo_state *saved_state = ctx->undo_state;
    ctx->undo_state = NULL;

    t_erow *row;

    switch (entry->type) {
        case UNDO_INSERT_CHAR:
            /* Undo insert = delete the character */
            if (entry->row >= 0 && entry->row < ctx->numrows) {
                row = &ctx->row[entry->row];
                if (entry->col >= 0 && entry->col < row->size) {
                    editor_row_del_char(ctx, row, entry->col);
                }
            }
            break;

        case UNDO_DELETE_CHAR:
            /* Undo delete = re-insert the character */
            if (entry->row >= 0 && entry->row < ctx->numrows) {
                row = &ctx->row[entry->row];
                editor_row_insert_char(ctx, row, entry->col, entry->data.char_op.ch);
            }
            break;

        case UNDO_INSERT_LINE:
            /* Undo line insert = delete the line (merge with previous) */
            if (entry->row >= 0 && entry->row < ctx->numrows) {
                /* Delete the newline that was inserted */
                editor_del_row(ctx, entry->row + 1);
            }
            break;

        case UNDO_DELETE_LINE:
            /* Undo line delete = re-insert the line (split) */
            if (entry->row >= 0 && entry->row < ctx->numrows) {
                editor_insert_row(ctx, entry->row + 1,
                                 entry->data.line_op.content,
                                 entry->data.line_op.length);
            }
            break;
    }

    /* Restore cursor position from before the operation */
    ctx->cy = entry->cursor_row;
    ctx->cx = entry->cursor_col;
    ctx->rowoff = entry->cursor_rowoff;
    ctx->coloff = entry->cursor_coloff;

    /* Restore undo state */
    ctx->undo_state = saved_state;
}

/* Apply single redo operation (replay the operation) */
static void apply_redo(editor_ctx_t *ctx, undo_entry_t *entry) {
    /* Suppress undo recording while applying redo */
    struct undo_state *saved_state = ctx->undo_state;
    ctx->undo_state = NULL;

    t_erow *row;

    switch (entry->type) {
        case UNDO_INSERT_CHAR:
            /* Redo insert = insert the character again */
            if (entry->row >= 0 && entry->row < ctx->numrows) {
                row = &ctx->row[entry->row];
                editor_row_insert_char(ctx, row, entry->col, entry->data.char_op.ch);
            }
            break;

        case UNDO_DELETE_CHAR:
            /* Redo delete = delete the character again */
            if (entry->row >= 0 && entry->row < ctx->numrows) {
                row = &ctx->row[entry->row];
                if (entry->col >= 0 && entry->col < row->size) {
                    editor_row_del_char(ctx, row, entry->col);
                }
            }
            break;

        case UNDO_INSERT_LINE:
            /* Redo line insert = split line again */
            if (entry->row >= 0 && entry->row < ctx->numrows) {
                editor_insert_row(ctx, entry->row + 1,
                                 entry->data.line_op.content,
                                 entry->data.line_op.length);
            }
            break;

        case UNDO_DELETE_LINE:
            /* Redo line delete = merge lines again */
            if (entry->row >= 0 && entry->row + 1 < ctx->numrows) {
                editor_del_row(ctx, entry->row + 1);
            }
            break;
    }

    /* Restore undo state */
    ctx->undo_state = saved_state;
}

int undo_perform(editor_ctx_t *ctx) {
    struct undo_state *undo = ctx->undo_state;
    if (!undo || undo->current == 0) return 0;  /* Nothing to undo */

    /* Find start of current group */
    int target_group = -1;
    int undo_idx = undo->current - 1;

    for (int i = undo->current - 1; i >= 0; i--) {
        int idx = (undo->head - undo->count + i + undo->capacity) % undo->capacity;
        undo_entry_t *e = &undo->entries[idx];

        if (target_group == -1) {
            target_group = e->group_id;
        } else if (e->group_id != target_group || e->group_break) {
            break;  /* Different group or explicit break, stop here */
        }
        undo_idx = i;
    }

    /* Undo all operations in this group (in reverse order) */
    for (int i = undo->current - 1; i >= undo_idx; i--) {
        int idx = (undo->head - undo->count + i + undo->capacity) % undo->capacity;
        apply_undo(ctx, &undo->entries[idx]);
    }

    undo->current = undo_idx;
    ctx->dirty++;
    return 1;
}

int redo_perform(editor_ctx_t *ctx) {
    struct undo_state *undo = ctx->undo_state;
    if (!undo || undo->current >= undo->count) return 0;  /* Nothing to redo */

    /* Find end of next group */
    int idx = (undo->head - undo->count + undo->current + undo->capacity) % undo->capacity;
    int target_group = undo->entries[idx].group_id;
    int redo_end = undo->current;

    for (int i = undo->current; i < undo->count; i++) {
        idx = (undo->head - undo->count + i + undo->capacity) % undo->capacity;
        undo_entry_t *e = &undo->entries[idx];

        redo_end = i + 1;

        if (e->group_break || (i + 1 < undo->count &&
            undo->entries[(undo->head - undo->count + i + 1 + undo->capacity) % undo->capacity].group_id != target_group)) {
            break;  /* End of group */
        }
    }

    /* Redo all operations in this group (in forward order) */
    for (int i = undo->current; i < redo_end; i++) {
        idx = (undo->head - undo->count + i + undo->capacity) % undo->capacity;
        apply_redo(ctx, &undo->entries[idx]);
    }

    undo->current = redo_end;
    ctx->dirty++;
    return 1;
}

/* ======================== Query Functions ======================== */

int undo_can_undo(editor_ctx_t *ctx) {
    struct undo_state *undo = ctx->undo_state;
    return undo && undo->current > 0;
}

int undo_can_redo(editor_ctx_t *ctx) {
    struct undo_state *undo = ctx->undo_state;
    return undo && undo->current < undo->count;
}

void undo_clear(editor_ctx_t *ctx) {
    if (!ctx->undo_state) return;

    struct undo_state *undo = ctx->undo_state;

    /* Free all line content */
    for (int i = 0; i < undo->count; i++) {
        free_entry_data(&undo->entries[i], undo);
    }

    undo->count = 0;
    undo->head = 0;
    undo->current = 0;
    undo->memory_used = 0;
    undo->current_group_id = 0;
}

void undo_get_stats(editor_ctx_t *ctx, int *undo_levels,
                     int *redo_levels, size_t *memory) {
    struct undo_state *undo = ctx->undo_state;
    if (!undo) {
        if (undo_levels) *undo_levels = 0;
        if (redo_levels) *redo_levels = 0;
        if (memory) *memory = 0;
        return;
    }

    if (undo_levels) *undo_levels = undo->current;
    if (redo_levels) *redo_levels = undo->count - undo->current;
    if (memory) *memory = undo->memory_used;
}
