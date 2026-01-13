/* loki_editor.c - Integration layer between editor core and Lua
 *
 * This file contains:
 * - Lua state management
 * - REPL state and functions
 * - Main editor loop with Lua integration
 * - Functions that bridge between pure C core and Lua bindings
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>

/* Lua headers */
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

/* Loki headers */
#include "loki/version.h"
#include "loki/editor.h"
#include "loki/core.h"
#include "loki/lua.h"
#include "loki_internal.h"
#include "loki_terminal.h"
#include "loki_buffers.h"
#include "loki_syntax.h"
#include "loki_alda.h"

/* ======================== Main Editor Instance ============================ */

/* Static editor instance for the main editor.
 * All functions now use explicit context passing (editor_ctx_t *ctx),
 * enabling future support for multiple editor windows/buffers.
 * This static instance is only used by main() as the primary editor instance.
 */
static editor_ctx_t E;

/* ======================== Helper Functions =============================== */

/* Lua status reporter - reports Lua errors to editor status bar */
static void loki_lua_status_reporter(const char *message, void *userdata) {
    editor_ctx_t *ctx = (editor_ctx_t *)userdata;
    if (message && message[0] != '\0' && ctx) {
        editor_set_status_msg(ctx, "%s", message);
    }
}

/* Update REPL layout when active/inactive state changes */
void editor_update_repl_layout(editor_ctx_t *ctx) {
    if (!ctx) return;
    int reserved = ctx->repl.active ? LUA_REPL_TOTAL_ROWS : 0;
    int available = ctx->screenrows_total;
    if (available > reserved) {
        ctx->screenrows = available - reserved;
    } else {
        ctx->screenrows = 1;
    }
    if (ctx->screenrows < 1) ctx->screenrows = 1;

    if (ctx->cy >= ctx->screenrows) {
        ctx->cy = ctx->screenrows - 1;
        if (ctx->cy < 0) ctx->cy = 0;
    }

    if (ctx->numrows > ctx->screenrows && ctx->rowoff > ctx->numrows - ctx->screenrows) {
        ctx->rowoff = ctx->numrows - ctx->screenrows;
    }
    if (ctx->numrows <= ctx->screenrows) {
        ctx->rowoff = 0;
    }
}

/* Toggle the Lua REPL focus */
static void exec_lua_command(editor_ctx_t *ctx, int fd) {
    (void)fd;
    if (!ctx || !ctx->L) {
        editor_set_status_msg(ctx, "Lua not available");
        return;
    }
    int was_active = ctx->repl.active;
    ctx->repl.active = !ctx->repl.active;
    editor_update_repl_layout(ctx);
    if (ctx->repl.active) {
        ctx->repl.history_index = -1;
        editor_set_status_msg(ctx, 
            "Lua REPL: Enter runs, ESC exits, Up/Down history, type 'help'");
        if (ctx->repl.log_len == 0) {
            lua_repl_append_log(ctx, "Type 'help' for built-in commands");
        }
    } else {
        if (was_active) {
            editor_set_status_msg(ctx, "Lua REPL closed");
        }
    }
}

/* Apply Lua-based highlighting spans to a row */
static int lua_apply_span_table(editor_ctx_t *ctx, t_erow *row, int table_index) {
    if (!ctx || !ctx->L) return 0;
    lua_State *L = ctx->L;
    if (!lua_istable(L, table_index)) return 0;

    int applied = 0;
    size_t entries = lua_rawlen(L, table_index);

    for (size_t i = 1; i <= entries; i++) {
        lua_rawgeti(L, table_index, (lua_Integer)i);
        if (lua_type(L, -1) == LUA_TTABLE) {
            int start = 0;
            int stop = 0;
            int length = 0;
            int style = -1;

            lua_getfield(L, -1, "start");
            if (lua_isnumber(L, -1)) start = (int)lua_tointeger(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "stop");
            if (lua_isnumber(L, -1)) stop = (int)lua_tointeger(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "end");
            if (lua_isnumber(L, -1)) stop = (int)lua_tointeger(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "length");
            if (lua_isnumber(L, -1)) length = (int)lua_tointeger(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "style");
            if (lua_isstring(L, -1)) {
                style = syntax_name_to_code(lua_tostring(L, -1));
            } else if (lua_isnumber(L, -1)) {
                style = (int)lua_tointeger(L, -1);
            }
            lua_pop(L, 1);

            if (style < 0) {
                lua_getfield(L, -1, "type");
                if (lua_isstring(L, -1)) {
                    style = syntax_name_to_code(lua_tostring(L, -1));
                } else if (lua_isnumber(L, -1)) {
                    style = (int)lua_tointeger(L, -1);
                }
                lua_pop(L, 1);
            }

            if (start <= 0) start = 1;
            if (length > 0 && stop <= 0) stop = start + length - 1;
            if (stop <= 0) stop = start;

            if (style >= 0 && row->rsize > 0) {
                if (start > stop) {
                    int tmp = start;
                    start = stop;
                    stop = tmp;
                }
                if (start < 1) start = 1;
                if (stop > row->rsize) stop = row->rsize;
                for (int pos = start - 1; pos < stop && pos < row->rsize; pos++) {
                    row->hl[pos] = style;
                }
                applied = 1;
            } else if (style >= 0 && row->rsize == 0) {
                applied = 1;
            }
        }
        lua_pop(L, 1);
    }

    return applied;
}

/* Apply Lua custom highlighting to a row */
static void lua_apply_highlight_row(editor_ctx_t *ctx, t_erow *row, int default_ran) {
    if (!ctx || !ctx->L || row == NULL || row->render == NULL) return;
    lua_State *L = ctx->L;
    int top = lua_gettop(L);

    lua_getglobal(L, "loki");
    if (!lua_istable(L, -1)) {
        lua_settop(L, top);
        return;
    }

    lua_getfield(L, -1, "highlight_row");
    if (!lua_isfunction(L, -1)) {
        lua_settop(L, top);
        return;
    }

    lua_pushinteger(L, row->idx);
    lua_pushlstring(L, row->chars ? row->chars : "", (size_t)row->size);
    lua_pushlstring(L, row->render ? row->render : "", (size_t)row->rsize);
    if (ctx->syntax) {
        lua_pushinteger(L, ctx->syntax->type);
    } else {
        lua_pushnil(L);
    }
    lua_pushboolean(L, default_ran);

    if (lua_pcall(L, 5, 1, 0) != LUA_OK) {
        const char *err = lua_tostring(L, -1);
        editor_set_status_msg(ctx, "Lua highlight error: %s", err ? err : "unknown");
        lua_settop(L, top);
        return;
    }

    if (!lua_istable(L, -1)) {
        lua_settop(L, top);
        return;
    }

    int table_index = lua_gettop(L);
    int replace = 0;

    lua_getfield(L, table_index, "replace");
    if (lua_isboolean(L, -1)) replace = lua_toboolean(L, -1);
    lua_pop(L, 1);

    int spans_index = table_index;
    int has_spans_field = 0;

    lua_getfield(L, table_index, "spans");
    if (lua_istable(L, -1)) {
        spans_index = lua_gettop(L);
        has_spans_field = 1;
    } else {
        lua_pop(L, 1);
    }

    if (replace) {
        memset(row->hl, HL_NORMAL, row->rsize);
    }

    lua_apply_span_table(ctx, row, spans_index);

    if (has_spans_field) {
        lua_pop(L, 1);
    }

    lua_settop(L, top);
}

/* ======================== Main Editor Function =========================== */

static void print_usage(void) {
    printf("Usage: loki [options] <filename>\n");
    printf("\nOptions:\n");
    printf("  --help              Show this help message\n");
    printf("  --version           Show version information\n");
    printf("\nInteractive mode (default):\n");
    printf("  loki <filename>     Open file in interactive editor\n");
    printf("\nKeybindings in interactive mode:\n");
    printf("  Ctrl-S    Save file\n");
    printf("  Ctrl-Q    Quit\n");
    printf("  Ctrl-F    Find\n");
    printf("  Ctrl-L    Toggle Lua REPL\n");
}

int loki_editor_main(int argc, char **argv) {
    /* Register cleanup handler early to ensure terminal is always restored */
    atexit(editor_atexit);

    /* Parse command-line arguments */
    if (argc < 2) {
        print_usage();
        exit(1);
    }

    /* Check for --help flag */
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_usage();
        exit(0);
    }

    /* Check for --version flag */
    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
        printf("loki %s\n", LOKI_VERSION);
        exit(0);
    }

    /* Check for unknown options */
    if (argv[1][0] == '-') {
        fprintf(stderr, "Error: Unknown option: %s\n", argv[1]);
        print_usage();
        exit(1);
    }

    /* Default: interactive mode */
    if (argc != 2) {
        fprintf(stderr, "Error: Too many arguments\n");
        print_usage();
        exit(1);
    }

    /* Initialize editor core */
    init_editor(&E);
    syntax_select_for_filename(&E, argv[1]);
    editor_open(&E, argv[1]);

    /* Initialize Lua */
    struct loki_lua_opts opts = {
        .bind_editor = 1,
        .bind_http = 0,
        .load_config = 1,
        .config_override = NULL,
        .project_root = NULL,
        .extra_lua_path = NULL,
        .reporter = loki_lua_status_reporter,
        .reporter_userdata = NULL
    };

    E.L = loki_lua_bootstrap(&E, &opts);
    if (!E.L) {
        fprintf(stderr, "Warning: Failed to initialize Lua runtime (%s)\n", loki_lua_runtime());
    }

    /* Re-select syntax now that Lua has registered dynamic languages */
    if (!E.syntax && E.filename) {
        syntax_select_for_filename(&E, E.filename);
        /* If syntax was found, refresh highlighting for all rows */
        if (E.syntax) {
            for (int i = 0; i < E.numrows; i++) {
                syntax_update_row(&E, &E.row[i]);
            }
        }
    }

    /* Initialize REPL */
    lua_repl_init(&E.repl);

    /* Initialize buffer management with the initial editor context */
    if (buffers_init(&E) != 0) {
        fprintf(stderr, "Error: Failed to initialize buffer management\n");
        exit(1);
    }

    /* Auto-initialize Alda for .alda files (must be after buffers_init) */
    {
        editor_ctx_t *ctx = buffer_get_current();
        size_t flen = strlen(argv[1]);
        if (ctx && flen >= 5 && strcmp(argv[1] + flen - 5, ".alda") == 0) {
            int ret = loki_alda_init(ctx, NULL);
            if (ret == 0) {
                ctx->alda_mode = 1;
                editor_set_status_msg(ctx, "ALDA: Ctrl-E part, Ctrl-P file, Ctrl-G stop");
            } else {
                const char *err = loki_alda_get_error(ctx);
                editor_set_status_msg(ctx, "Alda init failed: %s", err ? err : "unknown error");
            }
        }
    }

    /* Enable terminal raw mode and start main loop */
    terminal_enable_raw_mode(&E, STDIN_FILENO);
    editor_set_status_msg(&E,
        "HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find | Ctrl-T = new buf | Ctrl-X n/p/k = buf nav");

    while(1) {
        /* Get current buffer context */
        editor_ctx_t *ctx = buffer_get_current();
        if (!ctx) {
            fprintf(stderr, "Error: No active buffer\n");
            exit(1);
        }

        terminal_handle_resize(ctx);

        /* Process any pending alda playback callbacks */
        if (ctx->L) {
            loki_alda_check_callbacks(ctx, ctx->L);
        }

        editor_refresh_screen(ctx);
        editor_process_keypress(ctx, STDIN_FILENO);
    }

    return 0;
}

/* Clean up editor resources (called from editor_atexit in loki_core.c) */
void editor_cleanup_resources(editor_ctx_t *ctx) {
    if (!ctx) return;

    /* Clean up alda subsystem (stops all playback) */
    loki_alda_cleanup(ctx);

    /* Clean up Lua REPL */
    lua_repl_free(&ctx->repl);

    /* Clean up Lua state */
    if (ctx->L) {
        lua_close(ctx->L);
        ctx->L = NULL;
    }

}
