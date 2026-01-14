/* loki_modal.c - Modal editing (vim-like modes)
 *
 * This module implements vim-like modal editing with three modes:
 * - NORMAL mode: Navigation and commands (default)
 * - INSERT mode: Text insertion
 * - VISUAL mode: Text selection
 *
 * Modal editing separates navigation from text insertion, allowing
 * efficient keyboard-only editing without modifier keys.
 *
 * Keybindings:
 * NORMAL mode:
 *   h/j/k/l - Move cursor left/down/up/right
 *   i - Enter INSERT mode
 *   a - Enter INSERT mode after cursor
 *   o/O - Insert line below/above and enter INSERT mode
 *   v - Enter VISUAL mode (selection)
 *   x - Delete character
 *   {/} - Paragraph motion (move to prev/next empty line)
 *
 * INSERT mode:
 *   ESC - Return to NORMAL mode
 *   Normal typing inserts characters
 *   Arrow keys move cursor
 *
 * VISUAL mode:
 *   h/j/k/l - Extend selection
 *   y - Yank (copy) selection
 *   ESC - Return to NORMAL mode
 */

#include "modal.h"
#include "internal.h"
#include "selection.h"
#include "search.h"
#include "command.h"
#include "terminal.h"
#include "undo.h"
#include "buffers.h"
#include "alda.h"
#include <stdlib.h>
#include <string.h>

/* Lua headers for keymap dispatch */
#include "lua.h"
#include "lauxlib.h"

/* Control key definition (if not already defined) */
#ifndef CTRL_R
#define CTRL_R 18
#endif

/* Number of times CTRL-Q must be pressed before actually quitting */
#define KILO_QUIT_TIMES 3

/* Try to dispatch a keypress to a Lua keymap callback.
 * Checks _loki_keymaps.{mode}[keycode] for a registered function.
 * Returns 1 if handled by Lua, 0 if not (fall through to built-in). */
static int try_lua_keymap(editor_ctx_t *ctx, const char *mode, int key) {
    lua_State *L = ctx->L;
    if (!L) return 0;

    /* Get _loki_keymaps global table */
    lua_getglobal(L, "_loki_keymaps");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return 0;
    }

    /* Get mode subtable (e.g., _loki_keymaps.normal) */
    lua_getfield(L, -1, mode);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 2);
        return 0;
    }

    /* Get callback function at mode_table[keycode] */
    lua_pushinteger(L, key);
    lua_gettable(L, -2);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 3);  /* Pop nil/value, mode_table, _loki_keymaps */
        return 0;
    }

    /* Found a Lua keymap - call it */
    int pcall_result = lua_pcall(L, 0, 0, 0);
    if (pcall_result != LUA_OK) {
        const char *err = lua_tostring(L, -1);
        editor_set_status_msg(ctx, "Lua error: %s", err ? err : "(no message)");
        lua_pop(L, 1);  /* Pop error message */
    }

    lua_pop(L, 2);  /* Pop mode_table and _loki_keymaps */
    return 1;  /* Handled by Lua */
}

/* Helper: Check if a line is empty (blank or whitespace only) */
static int is_empty_line(editor_ctx_t *ctx, int row) {
    if (row < 0 || row >= ctx->numrows) return 1;
    t_erow *line = &ctx->row[row];
    for (int i = 0; i < line->size; i++) {
        if (line->chars[i] != ' ' && line->chars[i] != '\t') {
            return 0;
        }
    }
    return 1;
}

/* Move to next empty line (paragraph motion: }) */
static void move_to_next_empty_line(editor_ctx_t *ctx) {
    int filerow = ctx->rowoff + ctx->cy;

    /* Skip current paragraph (non-empty lines) */
    int row = filerow + 1;
    while (row < ctx->numrows && !is_empty_line(ctx, row)) {
        row++;
    }

    /* Skip empty lines to find start of next paragraph or stay at first empty */
    if (row < ctx->numrows) {
        /* Found an empty line - this is where we stop */
        filerow = row;
    } else {
        /* No empty line found, go to end of file */
        filerow = ctx->numrows - 1;
    }

    /* Update cursor position */
    if (filerow < ctx->rowoff) {
        ctx->rowoff = filerow;
        ctx->cy = 0;
    } else if (filerow >= ctx->rowoff + ctx->screenrows) {
        ctx->rowoff = filerow - ctx->screenrows + 1;
        ctx->cy = ctx->screenrows - 1;
    } else {
        ctx->cy = filerow - ctx->rowoff;
    }

    /* Move to start of line */
    ctx->cx = 0;
    ctx->coloff = 0;
}

/* Move to previous empty line (paragraph motion: {) */
static void move_to_prev_empty_line(editor_ctx_t *ctx) {
    int filerow = ctx->rowoff + ctx->cy;

    /* Skip current paragraph (non-empty lines) going backward */
    int row = filerow - 1;
    while (row >= 0 && !is_empty_line(ctx, row)) {
        row--;
    }

    /* Found an empty line - this is where we stop */
    if (row >= 0) {
        filerow = row;
    } else {
        /* No empty line found, go to start of file */
        filerow = 0;
    }

    /* Update cursor position */
    if (filerow < ctx->rowoff) {
        ctx->rowoff = filerow;
        ctx->cy = 0;
    } else if (filerow >= ctx->rowoff + ctx->screenrows) {
        ctx->rowoff = filerow - ctx->screenrows + 1;
        ctx->cy = ctx->screenrows - 1;
    } else {
        ctx->cy = filerow - ctx->rowoff;
    }

    /* Move to start of line */
    ctx->cx = 0;
    ctx->coloff = 0;
}

/* Check if a line is an Alda part declaration (e.g., "piano:", "trumpet/trombone:")
 * Returns 1 if the line contains a part declaration, 0 otherwise.
 * Pattern: optional whitespace, then identifier chars, then ':' not inside quotes */
static int is_part_declaration(const char *line, int len) {
    if (!line || len <= 0) return 0;

    int i = 0;
    /* Skip leading whitespace */
    while (i < len && (line[i] == ' ' || line[i] == '\t')) i++;

    /* Must start with a letter (instrument names start with letters) */
    if (i >= len || !((line[i] >= 'a' && line[i] <= 'z') ||
                      (line[i] >= 'A' && line[i] <= 'Z'))) {
        return 0;
    }

    /* Scan for ':' - valid chars are letters, digits, _-+'()/" and space (for aliases) */
    int in_quotes = 0;
    while (i < len) {
        char c = line[i];
        if (c == '"') {
            in_quotes = !in_quotes;
        } else if (c == ':' && !in_quotes) {
            /* Found unquoted colon - this is a part declaration */
            return 1;
        } else if (!in_quotes) {
            /* Outside quotes: only allow valid instrument/alias chars */
            int valid = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '_' || c == '-' ||
                        c == '+' || c == '\'' || c == '(' || c == ')' ||
                        c == '/' || c == ' ' || c == '.';
            if (!valid) return 0;
        }
        i++;
    }

    return 0;  /* No colon found */
}

/* Get the Alda part containing the cursor position.
 * A part starts at a line with an instrument declaration (e.g., "piano:")
 * and extends until the next part declaration or EOF.
 * Returns newly allocated string, caller must free. Returns NULL on error. */
static char *get_current_part(editor_ctx_t *ctx) {
    if (!ctx || ctx->numrows == 0) return NULL;

    int cursor_row = ctx->rowoff + ctx->cy;
    if (cursor_row >= ctx->numrows) cursor_row = ctx->numrows - 1;

    /* Find start of part: scan backward to find part declaration */
    int start_row = cursor_row;
    while (start_row > 0) {
        t_erow *row = &ctx->row[start_row];
        if (is_part_declaration(row->chars, row->size)) {
            break;  /* Found part declaration */
        }
        start_row--;
    }

    /* Find end of part: scan forward to find next part declaration */
    int end_row = cursor_row + 1;
    while (end_row < ctx->numrows) {
        t_erow *row = &ctx->row[end_row];
        if (is_part_declaration(row->chars, row->size)) {
            break;  /* Found next part */
        }
        end_row++;
    }
    /* end_row is exclusive (first row of next part, or numrows) */

    /* Calculate total length needed */
    size_t total_len = 0;
    for (int i = start_row; i < end_row; i++) {
        total_len += ctx->row[i].size + 1;  /* +1 for newline */
    }

    char *result = malloc(total_len + 1);
    if (!result) return NULL;

    /* Concatenate all lines */
    char *p = result;
    for (int i = start_row; i < end_row; i++) {
        memcpy(p, ctx->row[i].chars, ctx->row[i].size);
        p += ctx->row[i].size;
        *p++ = '\n';
    }
    *p = '\0';

    return result;
}

/* Process normal mode keypresses */
static void process_normal_mode(editor_ctx_t *ctx, int fd, int c) {
    /* Check Lua keymaps first */
    if (try_lua_keymap(ctx, "normal", c)) {
        return;  /* Handled by Lua callback */
    }

    switch(c) {
        case 'h': editor_move_cursor(ctx, ARROW_LEFT); break;
        case 'j': editor_move_cursor(ctx, ARROW_DOWN); break;
        case 'k': editor_move_cursor(ctx, ARROW_UP); break;
        case 'l': editor_move_cursor(ctx, ARROW_RIGHT); break;

        /* Paragraph motion */
        case '{':
            move_to_prev_empty_line(ctx);
            break;
        case '}':
            move_to_next_empty_line(ctx);
            break;

        /* Enter insert mode */
        case 'i':
            undo_break_group(ctx);  /* Break undo group on mode change */
            ctx->mode = MODE_INSERT;
            break;
        case 'a':
            undo_break_group(ctx);  /* Break undo group on mode change */
            editor_move_cursor(ctx, ARROW_RIGHT);
            ctx->mode = MODE_INSERT;
            break;
        case 'o':
            /* Insert line below and enter insert mode */
            if (ctx->numrows > 0) {
                int filerow = ctx->rowoff + ctx->cy;
                if (filerow < ctx->numrows) {
                    ctx->cx = ctx->row[filerow].size; /* Move to end of line */
                }
            }
            editor_insert_newline(ctx);
            ctx->mode = MODE_INSERT;
            break;
        case 'O':
            /* Insert line above and enter insert mode */
            ctx->cx = 0; /* Move to start of line */
            editor_insert_newline(ctx);
            editor_move_cursor(ctx, ARROW_UP);
            ctx->mode = MODE_INSERT;
            break;

        /* Enter visual mode */
        case 'v':
            ctx->mode = MODE_VISUAL;
            ctx->sel_active = 1;
            /* Store selection in file coordinates (not screen coordinates) */
            ctx->sel_start_x = ctx->coloff + ctx->cx;
            ctx->sel_start_y = ctx->rowoff + ctx->cy;
            ctx->sel_end_x = ctx->coloff + ctx->cx;
            ctx->sel_end_y = ctx->rowoff + ctx->cy;
            break;

        /* Enter command mode */
        case ':':
            command_mode_enter(ctx);
            break;

        /* Delete character */
        case 'x':
            editor_del_char(ctx);
            break;

        /* Undo/Redo */
        case 'u':
            if (undo_perform(ctx)) {
                editor_set_status_msg(ctx, "Undo");
            } else {
                editor_set_status_msg(ctx, "Already at oldest change");
            }
            break;
        case CTRL_R:
            if (redo_perform(ctx)) {
                editor_set_status_msg(ctx, "Redo");
            } else {
                editor_set_status_msg(ctx, "Already at newest change");
            }
            break;

        /* Global commands (work in all modes) */
        case CTRL_S: editor_save(ctx); break;
        case CTRL_F: editor_find(ctx, fd); break;
        case CTRL_L:
            /* Toggle REPL */
            ctx->repl.active = !ctx->repl.active;
            editor_update_repl_layout(ctx);
            if (ctx->repl.active) {
                editor_set_status_msg(ctx, "Lua REPL active (Ctrl-L or ESC to close)");
            }
            break;
        case CTRL_E:
            /* Eval selection (or current part) as Alda */
            {
                char *code = get_selection_text(ctx);
                if (!code && ctx->numrows > 0 && ctx->cy < ctx->numrows) {
                    /* No selection - use current part */
                    code = get_current_part(ctx);
                }
                if (code && *code) {
                    if (!loki_alda_is_initialized(ctx)) {
                        /* Auto-init if not initialized */
                        if (loki_alda_init(ctx, NULL) != 0) {
                            editor_set_status_msg(ctx, "Alda init failed: %s",
                                loki_alda_get_error(ctx) ? loki_alda_get_error(ctx) : "unknown");
                            free(code);
                            break;
                        }
                    }
                    int slot = loki_alda_eval_async(ctx, code, NULL);
                    if (slot >= 0) {
                        editor_set_status_msg(ctx, "Alda: playing part (slot %d)", slot);
                    } else {
                        editor_set_status_msg(ctx, "Alda error: %s",
                            loki_alda_get_error(ctx) ? loki_alda_get_error(ctx) : "eval failed");
                    }
                } else {
                    editor_set_status_msg(ctx, "No code to evaluate");
                }
                free(code);
                /* Clear selection after eval */
                ctx->sel_active = 0;
            }
            break;
        case CTRL_P:
            /* Play entire file as Alda */
            {
                if (ctx->numrows == 0) {
                    editor_set_status_msg(ctx, "Empty file");
                    break;
                }
                /* Build full buffer content */
                size_t total_len = 0;
                for (int i = 0; i < ctx->numrows; i++) {
                    total_len += ctx->row[i].size + 1; /* +1 for newline */
                }
                char *code = malloc(total_len + 1);
                if (!code) {
                    editor_set_status_msg(ctx, "Out of memory");
                    break;
                }
                char *p = code;
                for (int i = 0; i < ctx->numrows; i++) {
                    memcpy(p, ctx->row[i].chars, ctx->row[i].size);
                    p += ctx->row[i].size;
                    *p++ = '\n';
                }
                *p = '\0';

                if (!loki_alda_is_initialized(ctx)) {
                    if (loki_alda_init(ctx, NULL) != 0) {
                        editor_set_status_msg(ctx, "Alda init failed: %s",
                            loki_alda_get_error(ctx) ? loki_alda_get_error(ctx) : "unknown");
                        free(code);
                        break;
                    }
                }
                int slot = loki_alda_eval_async(ctx, code, NULL);
                if (slot >= 0) {
                    editor_set_status_msg(ctx, "Playing file (slot %d)", slot);
                } else {
                    editor_set_status_msg(ctx, "Alda error: %s",
                        loki_alda_get_error(ctx) ? loki_alda_get_error(ctx) : "eval failed");
                }
                free(code);
            }
            break;
        case CTRL_G:
            /* Stop Alda playback */
            if (loki_alda_is_initialized(ctx)) {
                loki_alda_stop_all(ctx);
                editor_set_status_msg(ctx, "Stopped");
            }
            break;
        case CTRL_Q:
            /* Handle quit in main function for consistency */
            break;

        /* Arrow keys */
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editor_move_cursor(ctx, c);
            break;

        default:
            /* Beep or show message for unknown command */
            editor_set_status_msg(ctx, "Unknown command");
            break;
    }
}

/* Process insert mode keypresses */
static void process_insert_mode(editor_ctx_t *ctx, int fd, int c) {
    /* Check Lua keymaps first */
    if (try_lua_keymap(ctx, "insert", c)) {
        return;  /* Handled by Lua callback */
    }

    switch(c) {
        case ESC:
            ctx->mode = MODE_NORMAL;
            /* Move cursor left if not at start of line */
            if (ctx->cx > 0 || ctx->coloff > 0) {
                editor_move_cursor(ctx, ARROW_LEFT);
            }
            break;

        case ENTER:
            editor_insert_newline(ctx);
            break;

        case BACKSPACE:
        case CTRL_H:
        case DEL_KEY:
            editor_del_char(ctx);
            break;

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editor_move_cursor(ctx, c);
            break;

        /* Global commands */
        case CTRL_S: editor_save(ctx); break;
        case CTRL_F: editor_find(ctx, fd); break;
        case CTRL_W:
            ctx->word_wrap = !ctx->word_wrap;
            editor_set_status_msg(ctx, "Word wrap %s", ctx->word_wrap ? "enabled" : "disabled");
            break;
        case CTRL_L:
            /* Toggle REPL */
            ctx->repl.active = !ctx->repl.active;
            editor_update_repl_layout(ctx);
            if (ctx->repl.active) {
                editor_set_status_msg(ctx, "Lua REPL active (Ctrl-L or ESC to close)");
            }
            break;
        case CTRL_C:
            copy_selection_to_clipboard(ctx);
            break;
        case CTRL_E:
        case CTRL_P:
            /* Play entire file as Alda */
            {
                char *code = NULL;
                int play_file = (c == CTRL_P);

                if (play_file) {
                    /* Play entire file */
                    if (ctx->numrows == 0) {
                        editor_set_status_msg(ctx, "Empty file");
                        break;
                    }
                    size_t total_len = 0;
                    for (int i = 0; i < ctx->numrows; i++) {
                        total_len += ctx->row[i].size + 1;
                    }
                    code = malloc(total_len + 1);
                    if (code) {
                        char *p = code;
                        for (int i = 0; i < ctx->numrows; i++) {
                            memcpy(p, ctx->row[i].chars, ctx->row[i].size);
                            p += ctx->row[i].size;
                            *p++ = '\n';
                        }
                        *p = '\0';
                    }
                } else {
                    /* Play selection or current part */
                    code = get_selection_text(ctx);
                    if (!code && ctx->numrows > 0 && ctx->cy < ctx->numrows) {
                        code = get_current_part(ctx);
                    }
                }

                if (code && *code) {
                    if (!loki_alda_is_initialized(ctx)) {
                        if (loki_alda_init(ctx, NULL) != 0) {
                            editor_set_status_msg(ctx, "Alda init failed: %s",
                                loki_alda_get_error(ctx) ? loki_alda_get_error(ctx) : "unknown");
                            free(code);
                            break;
                        }
                    }
                    int slot = loki_alda_eval_async(ctx, code, NULL);
                    if (slot >= 0) {
                        editor_set_status_msg(ctx, "%s (slot %d)",
                            play_file ? "Playing file" : "Playing part", slot);
                    } else {
                        editor_set_status_msg(ctx, "Alda error: %s",
                            loki_alda_get_error(ctx) ? loki_alda_get_error(ctx) : "eval failed");
                    }
                } else {
                    editor_set_status_msg(ctx, "No code to evaluate");
                }
                free(code);
                ctx->sel_active = 0;
            }
            break;
        case CTRL_G:
            /* Stop Alda playback */
            if (loki_alda_is_initialized(ctx)) {
                loki_alda_stop_all(ctx);
                editor_set_status_msg(ctx, "Stopped");
            }
            break;

        case PAGE_UP:
        case PAGE_DOWN:
            if (c == PAGE_UP && ctx->cy != 0)
                ctx->cy = 0;
            else if (c == PAGE_DOWN && ctx->cy != ctx->screenrows-1)
                ctx->cy = ctx->screenrows-1;
            {
                int times = ctx->screenrows;
                while(times--)
                    editor_move_cursor(ctx, c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
            break;

        case SHIFT_ARROW_UP:
        case SHIFT_ARROW_DOWN:
        case SHIFT_ARROW_LEFT:
        case SHIFT_ARROW_RIGHT:
            /* Start selection if not active */
            if (!ctx->sel_active) {
                ctx->sel_active = 1;
                ctx->sel_start_x = ctx->cx;
                ctx->sel_start_y = ctx->cy;
            }
            /* Move cursor */
            if (c == SHIFT_ARROW_UP) editor_move_cursor(ctx, ARROW_UP);
            else if (c == SHIFT_ARROW_DOWN) editor_move_cursor(ctx, ARROW_DOWN);
            else if (c == SHIFT_ARROW_LEFT) editor_move_cursor(ctx, ARROW_LEFT);
            else if (c == SHIFT_ARROW_RIGHT) editor_move_cursor(ctx, ARROW_RIGHT);
            /* Update selection end */
            ctx->sel_end_x = ctx->cx;
            ctx->sel_end_y = ctx->cy;
            break;

        default:
            /* Insert the character */
            editor_insert_char(ctx, c);
            break;
    }
}

/* Process visual mode keypresses */
static void process_visual_mode(editor_ctx_t *ctx, int fd, int c) {
    /* Check Lua keymaps first */
    if (try_lua_keymap(ctx, "visual", c)) {
        return;  /* Handled by Lua callback */
    }

    switch(c) {
        case ESC:
            ctx->mode = MODE_NORMAL;
            ctx->sel_active = 0;
            break;

        /* Movement extends selection */
        case 'h':
        case ARROW_LEFT:
            editor_move_cursor(ctx, ARROW_LEFT);
            /* Update selection end in file coordinates */
            ctx->sel_end_x = ctx->coloff + ctx->cx;
            ctx->sel_end_y = ctx->rowoff + ctx->cy;
            break;

        case 'j':
        case ARROW_DOWN:
            editor_move_cursor(ctx, ARROW_DOWN);
            /* Update selection end in file coordinates */
            ctx->sel_end_x = ctx->coloff + ctx->cx;
            ctx->sel_end_y = ctx->rowoff + ctx->cy;
            break;

        case 'k':
        case ARROW_UP:
            editor_move_cursor(ctx, ARROW_UP);
            /* Update selection end in file coordinates */
            ctx->sel_end_x = ctx->coloff + ctx->cx;
            ctx->sel_end_y = ctx->rowoff + ctx->cy;
            break;

        case 'l':
        case ARROW_RIGHT:
            editor_move_cursor(ctx, ARROW_RIGHT);
            /* Update selection end in file coordinates */
            ctx->sel_end_x = ctx->coloff + ctx->cx;
            ctx->sel_end_y = ctx->rowoff + ctx->cy;
            break;

        /* Copy selection */
        case 'y':
            copy_selection_to_clipboard(ctx);
            ctx->mode = MODE_NORMAL;
            ctx->sel_active = 0;
            editor_set_status_msg(ctx, "Yanked selection");
            break;

        /* Delete selection (yank first for 'd', just delete for 'x') */
        case 'd':
            copy_selection_to_clipboard(ctx); /* Save to clipboard first (yank) */
            {
                int deleted = delete_selection(ctx);
                editor_set_status_msg(ctx, "Deleted %d characters", deleted);
            }
            ctx->mode = MODE_NORMAL;
            break;
        case 'x':
            {
                int deleted = delete_selection(ctx);
                editor_set_status_msg(ctx, "Deleted %d characters", deleted);
            }
            ctx->mode = MODE_NORMAL;
            break;

        /* Global commands */
        case CTRL_C:
            copy_selection_to_clipboard(ctx);
            break;

        default:
            /* Unknown command - beep */
            editor_set_status_msg(ctx, "Unknown visual command");
            break;
    }
    (void)fd; /* Unused */
}

/* Process a single keypress with modal editing support.
 * This is the main entry point for all keyboard input when modal editing is enabled.
 * Dispatches to appropriate mode handler (normal/insert/visual). */
void modal_process_keypress(editor_ctx_t *ctx, int fd) {
    /* When the file is modified, requires Ctrl-q to be pressed N times
     * before actually quitting. */
    static int quit_times = KILO_QUIT_TIMES;

    int c = terminal_read_key(fd);

    /* REPL keypress handling */
    if (ctx->repl.active) {
        lua_repl_handle_keypress(ctx, c);
        return;
    }

    /* Handle quit globally (works in all modes) */
    if (c == CTRL_Q) {
        if (ctx->dirty && quit_times) {
            editor_set_status_msg(ctx, "WARNING!!! File has unsaved changes. "
                "Press Ctrl-Q %d more times to quit.", quit_times);
            quit_times--;
            return;
        }
        exit(0);
    }

    /* Handle buffer operations globally */
    if (c == CTRL_T) {
        /* Create new buffer */
        int new_id = buffer_create(NULL);
        if (new_id >= 0) {
            buffer_switch(new_id);
            editor_set_status_msg(ctx, "Created buffer %d", new_id);
        } else {
            editor_set_status_msg(ctx, "Error: Could not create buffer (max %d buffers)", MAX_BUFFERS);
        }
        quit_times = KILO_QUIT_TIMES;
        return;
    }

    if (c == CTRL_X) {
        /* Ctrl-X prefix - read next key for buffer command */
        int next = terminal_read_key(fd);

        if (next == 'n') {
            /* Next buffer */
            int next_id = buffer_next();
            if (next_id >= 0) {
                editor_set_status_msg(ctx, "Switched to buffer %d", next_id);
            }
        } else if (next == 'p') {
            /* Previous buffer */
            int prev_id = buffer_prev();
            if (prev_id >= 0) {
                editor_set_status_msg(ctx, "Switched to buffer %d", prev_id);
            }
        } else if (next == 'k') {
            /* Close buffer */
            int current_id = buffer_get_current_id();
            int result = buffer_close(current_id, 0);
            if (result == 1) {
                editor_set_status_msg(ctx, "Buffer has unsaved changes! Use Ctrl-X K to force close");
            } else if (result == 0) {
                editor_set_status_msg(ctx, "Closed buffer %d", current_id);
            } else {
                editor_set_status_msg(ctx, "Cannot close last buffer");
            }
        } else if (next == 'K') {
            /* Force close buffer */
            int current_id = buffer_get_current_id();
            int result = buffer_close(current_id, 1);
            if (result == 0) {
                editor_set_status_msg(ctx, "Force closed buffer %d", current_id);
            } else {
                editor_set_status_msg(ctx, "Cannot close last buffer");
            }
        } else if (next >= '1' && next <= '9') {
            /* Switch to buffer by number (1-9) */
            int ids[MAX_BUFFERS];
            int count = buffer_get_list(ids);
            int index = next - '1';
            if (index < count) {
                buffer_switch(ids[index]);
                editor_set_status_msg(ctx, "Switched to buffer %d", ids[index]);
            } else {
                editor_set_status_msg(ctx, "Buffer %d not found", index + 1);
            }
        }
        quit_times = KILO_QUIT_TIMES;
        return;
    }

    /* Dispatch to mode-specific handler */
    switch(ctx->mode) {
        case MODE_NORMAL:
            process_normal_mode(ctx, fd, c);
            break;
        case MODE_INSERT:
            process_insert_mode(ctx, fd, c);
            break;
        case MODE_VISUAL:
            process_visual_mode(ctx, fd, c);
            break;
        case MODE_COMMAND:
            command_mode_handle_key(ctx, fd, c);
            break;
    }

    quit_times = KILO_QUIT_TIMES; /* Reset it to the original value. */
}

/* ============================================================================
 * Test Functions - For unit testing only
 * ============================================================================
 * These functions expose the internal mode handlers for unit testing.
 * They should not be used in production code - only in tests.
 */

void modal_process_normal_mode_key(editor_ctx_t *ctx, int fd, int c) {
    process_normal_mode(ctx, fd, c);
}

void modal_process_insert_mode_key(editor_ctx_t *ctx, int fd, int c) {
    process_insert_mode(ctx, fd, c);
}

void modal_process_visual_mode_key(editor_ctx_t *ctx, int fd, int c) {
    process_visual_mode(ctx, fd, c);
}
