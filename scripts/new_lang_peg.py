#!/usr/bin/env python3
"""
Generate a minimal music programming language for psnd using PackCC (PEG parser).

This script creates a language with:
- A PEG grammar file (.peg) for the music DSL
- CMake integration that builds packcc and generates the parser automatically
- Runtime that uses the shared backend for realtime MIDI
- REPL and editor integration
- Tests

The generated language supports:
- note C4 [duration] [velocity]   # Play a note (pitch names or MIDI numbers)
- chord C4 E4 G4 [dur:ms] [vel:v] # Play a chord
- rest <duration>                 # Rest for duration ms
- tempo <bpm>                     # Set tempo
- # comments                      # Line comments

Unlike new_lang.py (hand-written recursive descent parser), this script generates
a PackCC PEG grammar that is compiled to C. This demonstrates an alternative
approach using parser generators.

MODULAR ARCHITECTURE:
This script uses the modular language architecture. Creating a new language only
requires creating files under source/langs/<name>/ - NO modifications to shared files
(lang_config.h, lang_dispatch.c, CMakeLists.txt, etc.) are needed.

Workflow:
1. Run this script to generate the language skeleton under source/langs/<name>/
2. Edit the .peg grammar file to experiment with syntax
3. Run 'make' - CMake auto-discovers languages and rebuilds
4. Test your changes

The packcc tool is bundled in source/thirdparty/packcc-2.2.0 and built automatically.
"""

import argparse
import re
import sys
from pathlib import Path
from typing import List


def get_project_root() -> Path:
    """Find the project root (directory containing CMakeLists.txt)."""
    script_dir = Path(__file__).resolve().parent
    root = script_dir.parent
    if not (root / "CMakeLists.txt").exists():
        root = root.parent
    if not (root / "CMakeLists.txt").exists():
        raise RuntimeError("Could not find project root (no CMakeLists.txt found)")
    return root


def to_upper(name: str) -> str:
    return name.upper()


def to_title(name: str) -> str:
    return name.title()


def to_lower(name: str) -> str:
    return name.lower()


# =============================================================================
# PEG Grammar Template
# =============================================================================

PEG_GRAMMAR_TEMPLATE = '''%prefix "{name}_peg"

%value "struct {Title}AstNode*"

%auxil "struct {Title}ParseContext*"

%header {{
#ifndef {NAME}_GRAMMAR_H_INCLUDED
#define {NAME}_GRAMMAR_H_INCLUDED

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

typedef enum {{
    {NAME}_AST_NOTE,
    {NAME}_AST_CHORD,
    {NAME}_AST_REST,
    {NAME}_AST_TEMPO,
    {NAME}_AST_PROGRAM,
    {NAME}_AST_ERROR
}} {Title}AstNodeType;

typedef struct {Title}AstNode {{
    {Title}AstNodeType type;
    union {{
        struct {{ int pitch; int duration_ms; int velocity; }} note;
        struct {{ int pitches[16]; int pitch_count; int duration_ms; int velocity; }} chord;
        struct {{ int duration_ms; }} rest;
        struct {{ int bpm; }} tempo;
        struct {{ struct {Title}AstNode** stmts; int count; int cap; }} program;
        struct {{ char msg[128]; }} error;
    }} d;
}} {Title}AstNode;

typedef struct {Title}ParseContext {{
    const char* input;
    int pos;
}} {Title}ParseContext;

static inline {Title}AstNode* {name}_ast_new({Title}AstNodeType t) {{
    {Title}AstNode* n = ({Title}AstNode*)calloc(1, sizeof({Title}AstNode));
    if (n) n->type = t;
    return n;
}}

static inline void {name}_ast_free({Title}AstNode* n) {{
    if (!n) return;
    if (n->type == {NAME}_AST_PROGRAM) {{
        for (int i = 0; i < n->d.program.count; i++) {name}_ast_free(n->d.program.stmts[i]);
        free(n->d.program.stmts);
    }}
    free(n);
}}

static inline void {name}_prog_add({Title}AstNode* p, {Title}AstNode* s) {{
    if (!p || !s || p->type != {NAME}_AST_PROGRAM) return;
    if (p->d.program.count >= p->d.program.cap) {{
        int nc = p->d.program.cap ? p->d.program.cap * 2 : 8;
        {Title}AstNode** ns = ({Title}AstNode**)realloc(p->d.program.stmts, nc * sizeof({Title}AstNode*));
        if (!ns) return;
        p->d.program.stmts = ns;
        p->d.program.cap = nc;
    }}
    p->d.program.stmts[p->d.program.count++] = s;
}}

static inline int {name}_note_to_midi(const char* s, int len) {{
    if (len < 1) return 60;
    int base;
    char c = s[0];
    if (c >= 'a' && c <= 'g') c -= 32;
    switch (c) {{
        case 'C': base = 0; break;
        case 'D': base = 2; break;
        case 'E': base = 4; break;
        case 'F': base = 5; break;
        case 'G': base = 7; break;
        case 'A': base = 9; break;
        case 'B': base = 11; break;
        default: return 60;
    }}
    int i = 1;
    while (i < len && (s[i] == '#' || s[i] == 'b')) {{
        if (s[i] == '#') base++; else base--;
        i++;
    }}
    int oct = 4;
    if (i < len && s[i] >= '0' && s[i] <= '9') oct = s[i] - '0';
    return 12 * (oct + 1) + base;
}}

#endif
}}

%source {{
#define PCC_GETCHAR(auxil) ((auxil)->input[(auxil)->pos] ? (int)(unsigned char)(auxil)->input[(auxil)->pos++] : -1)

/* Override PCC_ERROR to not exit - just set error flag and return */
#define PCC_ERROR(auxil) ((void)0)
}}

program
    <- p:lines !. {{ $$ = p; }}

lines
    <- s:stmt NL rest:lines {{
        if (s) {name}_prog_add(rest, s);
        $$ = rest;
    }}
    / '#' [^\\n\\r]* NL rest:lines {{
        $$ = rest;
    }}
    / [ \\t]* NL rest:lines {{
        $$ = rest;
    }}
    / s:stmt !. {{
        {Title}AstNode* p = {name}_ast_new({NAME}_AST_PROGRAM);
        if (s) {name}_prog_add(p, s);
        $$ = p;
    }}
    / '#' [^\\n\\r]* !. {{
        $$ = {name}_ast_new({NAME}_AST_PROGRAM);
    }}
    / [ \\t]* !. {{
        $$ = {name}_ast_new({NAME}_AST_PROGRAM);
    }}

stmt
    <- 'note' [ \\t]+ xp:pitch xd:optnum xv:optnum [ \\t]* {{
        {Title}AstNode* node = {name}_ast_new({NAME}_AST_NOTE);
        node->d.note.pitch = (int)(intptr_t)xp;
        node->d.note.duration_ms = xd ? (int)(intptr_t)xd : 250;
        node->d.note.velocity = xv ? (int)(intptr_t)xv : 80;
        $$ = node;
    }}
    / 'chord' [ \\t]+ xps:pitchlist xopts:chordopts [ \\t]* {{
        {Title}AstNode* node = {name}_ast_new({NAME}_AST_CHORD);
        int* pd = (int*)xps;
        if (pd) {{
            node->d.chord.pitch_count = pd[0];
            for (int i = 0; i < pd[0] && i < 16; i++) node->d.chord.pitches[i] = pd[i+1];
            free(pd);
        }}
        int* od = (int*)xopts;
        node->d.chord.duration_ms = od ? od[0] : 250;
        node->d.chord.velocity = od ? od[1] : 80;
        if (od) free(od);
        $$ = node;
    }}
    / 'rest' [ \\t]+ xd:num [ \\t]* {{
        {Title}AstNode* node = {name}_ast_new({NAME}_AST_REST);
        node->d.rest.duration_ms = (int)(intptr_t)xd;
        $$ = node;
    }}
    / 'tempo' [ \\t]+ xb:num [ \\t]* {{
        {Title}AstNode* node = {name}_ast_new({NAME}_AST_TEMPO);
        node->d.tempo.bpm = (int)(intptr_t)xb;
        $$ = node;
    }}

pitch
    <- < [A-Ga-g] [#b]* [0-9] > {{
        $$ = (struct {Title}AstNode*)(intptr_t){name}_note_to_midi($1, $1e - $1s);
    }}
    / n:num {{ $$ = n; }}

pitchlist
    <- p:pitch [ \\t]+ r:pitchlist {{
        int* rd = (int*)r;
        int rc = rd ? rd[0] : 0;
        int* res = (int*)malloc((rc + 2) * sizeof(int));
        res[0] = rc + 1;
        res[1] = (int)(intptr_t)p;
        for (int i = 0; i < rc; i++) res[i+2] = rd[i+1];
        free(rd);
        $$ = (struct {Title}AstNode*)res;
    }}
    / p:pitch {{
        int* res = (int*)malloc(2 * sizeof(int));
        res[0] = 1;
        res[1] = (int)(intptr_t)p;
        $$ = (struct {Title}AstNode*)res;
    }}

chordopts
    <- [ \\t]+ 'dur:' xd:num r:chordopts {{
        int* rd = (int*)r;
        int* res = (int*)malloc(2 * sizeof(int));
        res[0] = (int)(intptr_t)xd;
        res[1] = rd ? rd[1] : 80;
        free(rd);
        $$ = (struct {Title}AstNode*)res;
    }}
    / [ \\t]+ 'vel:' xv:num r:chordopts {{
        int* rd = (int*)r;
        int* res = (int*)malloc(2 * sizeof(int));
        res[0] = rd ? rd[0] : 250;
        res[1] = (int)(intptr_t)xv;
        free(rd);
        $$ = (struct {Title}AstNode*)res;
    }}
    / {{ $$ = NULL; }}

optnum
    <- [ \\t]+ n:num {{ $$ = n; }}
    / {{ $$ = NULL; }}

num
    <- < '-'? [0-9]+ > {{ $$ = (struct {Title}AstNode*)(intptr_t)atoi($1); }}

NL <- '\\r\\n' / '\\r' / '\\n'

%%
'''

# =============================================================================
# Runtime Implementation
# =============================================================================

RUNTIME_H_TEMPLATE = '''#ifndef {NAME}_RUNTIME_H
#define {NAME}_RUNTIME_H

#include "{name}_grammar.h"

#ifdef __cplusplus
extern "C" {{
#endif

/* Forward declaration */
typedef struct SharedContext SharedContext;

/**
 * {Title} runtime context.
 */
typedef struct {{
    SharedContext *shared;
    int tempo_bpm;
    int default_velocity;
    int default_duration_ms;
    char last_error[256];
}} {Title}Runtime;

/**
 * Initialize the runtime with a shared context.
 */
void {name}_runtime_init({Title}Runtime *rt, SharedContext *shared);

/**
 * Execute a parsed AST node.
 */
int {name}_runtime_exec({Title}Runtime *rt, const {Title}AstNode *node);

/**
 * Parse and evaluate a string of {name} code.
 */
int {name}_runtime_eval({Title}Runtime *rt, const char *code);

/**
 * Stop all playing notes (MIDI panic).
 */
void {name}_runtime_stop({Title}Runtime *rt);

/**
 * Get last error message, or NULL if none.
 */
const char *{name}_runtime_get_error({Title}Runtime *rt);

#ifdef __cplusplus
}}
#endif

#endif /* {NAME}_RUNTIME_H */
'''

RUNTIME_C_TEMPLATE = '''/**
 * @file {name}_runtime.c
 * @brief {Title} language runtime - executes parsed AST from PackCC parser.
 */

#include "{name}_runtime.h"
#include "{name}_grammar.h"
#include "context.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define SLEEP_MS(ms) Sleep(ms)
#else
#include <unistd.h>
#define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

void {name}_runtime_init({Title}Runtime *rt, SharedContext *shared) {{
    memset(rt, 0, sizeof(*rt));
    rt->shared = shared;
    rt->tempo_bpm = 120;
    rt->default_velocity = 80;
    rt->default_duration_ms = 250;
}}

static void set_error({Title}Runtime *rt, const char *msg) {{
    if (msg) {{
        strncpy(rt->last_error, msg, sizeof(rt->last_error) - 1);
        rt->last_error[sizeof(rt->last_error) - 1] = '\\0';
    }} else {{
        rt->last_error[0] = '\\0';
    }}
}}

int {name}_runtime_exec({Title}Runtime *rt, const {Title}AstNode *node) {{
    if (!rt || !node) return -1;

    switch (node->type) {{
    case {NAME}_AST_NOTE: {{
        int pitch = node->d.note.pitch;
        int velocity = node->d.note.velocity;
        int duration = node->d.note.duration_ms;
        int channel = rt->shared ? rt->shared->default_channel : 0;

        if (rt->shared) {{
            shared_send_note_on(rt->shared, channel, pitch, velocity);
            SLEEP_MS(duration);
            shared_send_note_off(rt->shared, channel, pitch);
        }}
        break;
    }}

    case {NAME}_AST_CHORD: {{
        int velocity = node->d.chord.velocity;
        int duration = node->d.chord.duration_ms;
        int channel = rt->shared ? rt->shared->default_channel : 0;

        if (rt->shared) {{
            for (int i = 0; i < node->d.chord.pitch_count; i++) {{
                shared_send_note_on(rt->shared, channel, node->d.chord.pitches[i], velocity);
            }}
            SLEEP_MS(duration);
            for (int i = 0; i < node->d.chord.pitch_count; i++) {{
                shared_send_note_off(rt->shared, channel, node->d.chord.pitches[i]);
            }}
        }}
        break;
    }}

    case {NAME}_AST_REST: {{
        SLEEP_MS(node->d.rest.duration_ms);
        break;
    }}

    case {NAME}_AST_TEMPO: {{
        rt->tempo_bpm = node->d.tempo.bpm;
        if (rt->shared) {{
            rt->shared->tempo = node->d.tempo.bpm;
        }}
        break;
    }}

    case {NAME}_AST_PROGRAM: {{
        for (int i = 0; i < node->d.program.count; i++) {{
            {Title}AstNode *stmt = node->d.program.stmts[i];
            if (stmt) {{
                int result = {name}_runtime_exec(rt, stmt);
                if (result != 0) return result;
            }}
        }}
        break;
    }}

    case {NAME}_AST_ERROR: {{
        set_error(rt, node->d.error.msg);
        return -1;
    }}
    }}

    set_error(rt, NULL);
    return 0;
}}

int {name}_runtime_eval({Title}Runtime *rt, const char *code) {{
    if (!rt || !code) return -1;

    {Title}ParseContext pctx = {{0}};
    pctx.input = code;
    pctx.pos = 0;

    {name}_peg_context_t *ctx = {name}_peg_create(&pctx);
    if (!ctx) {{
        set_error(rt, "Failed to create parser context");
        return -1;
    }}

    {Title}AstNode *ast = NULL;
    int result = {name}_peg_parse(ctx, &ast);

    if (result == 0 || !ast) {{
        set_error(rt, "Parse error");
        {name}_peg_destroy(ctx);
        return -1;
    }}

    int exec_result = {name}_runtime_exec(rt, ast);

    {name}_ast_free(ast);
    {name}_peg_destroy(ctx);

    return exec_result;
}}

void {name}_runtime_stop({Title}Runtime *rt) {{
    if (rt && rt->shared) {{
        shared_send_panic(rt->shared);
    }}
}}

const char *{name}_runtime_get_error({Title}Runtime *rt) {{
    if (!rt) return NULL;
    return rt->last_error[0] ? rt->last_error : NULL;
}}
'''

# =============================================================================
# Register/REPL/Dispatch Templates
# =============================================================================

REGISTER_H_TEMPLATE = '''#ifndef {NAME}_REGISTER_H
#define {NAME}_REGISTER_H

#ifdef __cplusplus
extern "C" {{
#endif

/* Forward declarations */
struct editor_ctx;
typedef struct editor_ctx editor_ctx_t;
struct lua_State;
typedef struct lua_State lua_State;

/**
 * Initialize {Title} language registration with the language bridge.
 * Called automatically during editor initialization.
 */
void {name}_loki_lang_init(void);

#ifdef __cplusplus
}}
#endif

#endif /* {NAME}_REGISTER_H */
'''

REGISTER_C_TEMPLATE = '''/**
 * @file register.c
 * @brief {Title} language integration with Loki editor.
 *
 * Uses a PackCC-generated PEG parser for parsing.
 */

#include "register.h"
#include "psnd.h"
#include "loki/internal.h"
#include "loki/lang_bridge.h"
#include "loki/lua.h"
#include "lauxlib.h"

#include "context.h"
#include "shared/midi/midi.h"
#include "{name}_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Per-Context State
 * ============================================================================ */

struct Loki{Title}State {{
    int initialized;
    SharedContext *shared;
    {Title}Runtime runtime;
    char last_error[256];
}};

static struct Loki{Title}State *get_state(editor_ctx_t *ctx) {{
    return ctx ? ctx->model.{name}_state : NULL;
}}

static void set_error(struct Loki{Title}State *state, const char *msg) {{
    if (!state) return;
    if (msg) {{
        strncpy(state->last_error, msg, sizeof(state->last_error) - 1);
        state->last_error[sizeof(state->last_error) - 1] = '\\0';
    }} else {{
        state->last_error[0] = '\\0';
    }}
}}

/* ============================================================================
 * LokiLangOps Implementation
 * ============================================================================ */

static int {name}_lang_init(editor_ctx_t *ctx) {{
    if (!ctx) return -1;

    if (ctx->model.{name}_state && ctx->model.{name}_state->initialized) {{
        return 0;
    }}

    struct Loki{Title}State *state = ctx->model.{name}_state;
    if (!state) {{
        state = calloc(1, sizeof(struct Loki{Title}State));
        if (!state) return -1;
        ctx->model.{name}_state = state;
    }}

    state->shared = malloc(sizeof(SharedContext));
    if (!state->shared || shared_context_init(state->shared) != 0) {{
        set_error(state, "Failed to initialize shared context");
        free(state);
        ctx->model.{name}_state = NULL;
        return -1;
    }}

    shared_midi_open_virtual(state->shared, PSND_MIDI_PORT_NAME "-{name}");
    {name}_runtime_init(&state->runtime, state->shared);

    state->initialized = 1;
    set_error(state, NULL);
    return 0;
}}

static void {name}_lang_cleanup(editor_ctx_t *ctx) {{
    if (!ctx) return;

    struct Loki{Title}State *state = get_state(ctx);
    if (!state) return;

    if (state->shared) {{
        shared_send_panic(state->shared);
        shared_context_cleanup(state->shared);
        free(state->shared);
    }}

    free(state);
    ctx->model.{name}_state = NULL;
}}

static int {name}_lang_is_initialized(editor_ctx_t *ctx) {{
    struct Loki{Title}State *state = get_state(ctx);
    return state ? state->initialized : 0;
}}

static int {name}_lang_eval(editor_ctx_t *ctx, const char *code) {{
    struct Loki{Title}State *state = get_state(ctx);
    if (!state || !state->initialized) {{
        if (state) set_error(state, "Not initialized");
        return -1;
    }}

    int result = {name}_runtime_eval(&state->runtime, code);
    if (result != 0) {{
        const char *err = {name}_runtime_get_error(&state->runtime);
        set_error(state, err ? err : "Evaluation failed");
        return -1;
    }}

    set_error(state, NULL);
    return 0;
}}

static void {name}_lang_stop(editor_ctx_t *ctx) {{
    struct Loki{Title}State *state = get_state(ctx);
    if (!state || !state->initialized) return;
    {name}_runtime_stop(&state->runtime);
}}

static const char *{name}_lang_get_error(editor_ctx_t *ctx) {{
    struct Loki{Title}State *state = get_state(ctx);
    if (!state) return NULL;
    return state->last_error[0] ? state->last_error : NULL;
}}

static int {name}_lang_is_playing(editor_ctx_t *ctx) {{
    (void)ctx;
    return 0;  /* Synchronous execution */
}}

/* ============================================================================
 * Lua API
 * ============================================================================ */

static int lua_{name}_init(lua_State *L) {{
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    int result = {name}_lang_init(ctx);
    lua_pushboolean(L, result == 0);
    return 1;
}}

static int lua_{name}_eval(lua_State *L) {{
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    const char *code = luaL_checkstring(L, 1);

    int result = {name}_lang_eval(ctx, code);
    if (result != 0) {{
        lua_pushnil(L);
        lua_pushstring(L, {name}_lang_get_error(ctx));
        return 2;
    }}

    lua_pushboolean(L, 1);
    return 1;
}}

static int lua_{name}_stop(lua_State *L) {{
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    {name}_lang_stop(ctx);
    return 0;
}}

static void {name}_register_lua_api(lua_State *L) {{
    lua_getglobal(L, "loki");
    if (!lua_istable(L, -1)) {{
        lua_pop(L, 1);
        return;
    }}

    lua_newtable(L);

    lua_pushcfunction(L, lua_{name}_init);
    lua_setfield(L, -2, "init");

    lua_pushcfunction(L, lua_{name}_eval);
    lua_setfield(L, -2, "eval");

    lua_pushcfunction(L, lua_{name}_stop);
    lua_setfield(L, -2, "stop");

    lua_setfield(L, -2, "{name}");
    lua_pop(L, 1);
}}

/* ============================================================================
 * Language Registration
 * ============================================================================ */

static const LokiLangOps {name}_lang_ops = {{
    .name = "{name}",
    .extensions = {{{extensions_c}}},

    .init = {name}_lang_init,
    .cleanup = {name}_lang_cleanup,
    .is_initialized = {name}_lang_is_initialized,
    .check_callbacks = NULL,

    .eval = {name}_lang_eval,
    .stop = {name}_lang_stop,
    .is_playing = {name}_lang_is_playing,

    .has_events = NULL,
    .populate_shared_buffer = NULL,

    .get_error = {name}_lang_get_error,
    .configure_backend = NULL,
    .register_lua_api = {name}_register_lua_api,
}};

void {name}_loki_lang_init(void) {{
    loki_lang_register(&{name}_lang_ops);
}}
'''

REPL_H_TEMPLATE = '''#ifndef {NAME}_REPL_H
#define {NAME}_REPL_H

#ifdef __cplusplus
extern "C" {{
#endif

/**
 * {Title} REPL main entry point.
 * Called when user runs: psnd {name} [options] [file]
 */
int {name}_repl_main(int argc, char **argv);

/**
 * {Title} play mode entry point.
 * Called when user runs: psnd play file.{ext}
 */
int {name}_play_main(int argc, char **argv);

#ifdef __cplusplus
}}
#endif

#endif /* {NAME}_REPL_H */
'''

REPL_C_TEMPLATE = '''/**
 * @file repl.c
 * @brief {Title} language REPL with PackCC-generated PEG parser.
 */

#include "psnd.h"
#include "repl.h"  /* Core REPL infrastructure */
#include "loki/core.h"
#include "loki/internal.h"
#include "loki/syntax.h"
#include "loki/lua.h"
#include "loki/repl_helpers.h"
#include "shared/repl_commands.h"
#include "shared/midi/midi.h"
#include "shared/audio/audio.h"
#include "context.h"
#include "{name}_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* ============================================================================
 * REPL State
 * ============================================================================ */

static SharedContext *g_{name}_shared_ctx = NULL;
static {Title}Runtime g_{name}_runtime;

/* ============================================================================
 * Usage and Help
 * ============================================================================ */

static void print_usage(const char *prog) {{
    printf("Usage: %s [options] [file.{ext}]\\n", prog);
    printf("\\n{Title} music language interpreter (PEG parser).\\n\\n");
    printf("Options:\\n");
    printf("  -h, --help        Show this help message\\n");
    printf("  -l, --list        List available MIDI ports\\n");
    printf("  -p, --port N      Use MIDI port N\\n");
    printf("  --virtual NAME    Create virtual MIDI port\\n");
    printf("  -sf PATH          Use built-in synth with soundfont\\n");
}}

static void print_help(void) {{
    shared_print_command_help();

    printf("{Title} Syntax (PEG-based):\\n");
    printf("  note C4 [dur] [vel]      Play a note (e.g., C4, C#4, Db3, or 60)\\n");
    printf("  chord C4 E4 G4 ...       Play a chord\\n");
    printf("    [dur:ms] [vel:v]       Optional duration/velocity\\n");
    printf("  rest <duration>          Rest for duration ms\\n");
    printf("  tempo <bpm>              Set tempo\\n");
    printf("  # comment                Line comment\\n");
    printf("\\n");
}}

/* ============================================================================
 * Command Processing
 * ============================================================================ */

static void {name}_stop_playback(void) {{
    {name}_runtime_stop(&g_{name}_runtime);
    if (g_{name}_shared_ctx) {{
        shared_send_panic(g_{name}_shared_ctx);
    }}
}}

static int {name}_process_command(const char *input) {{
    int result = shared_process_command(g_{name}_shared_ctx, input, {name}_stop_playback);
    if (result == REPL_CMD_QUIT) return 1;
    if (result == REPL_CMD_HANDLED) return 0;

    const char *cmd = input;
    if (cmd[0] == ':') cmd++;

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0) {{
        print_help();
        return 0;
    }}

    return 2;  /* Interpret as code */
}}

/* ============================================================================
 * Code Evaluation
 * ============================================================================ */

static int evaluate_code(const char *code) {{
    int result = {name}_runtime_eval(&g_{name}_runtime, code);
    if (result != 0) {{
        const char *err = {name}_runtime_get_error(&g_{name}_runtime);
        if (err) {{
            fprintf(stderr, "Error: %s\\n", err);
        }}
        return -1;
    }}
    return 0;
}}

static int evaluate_file(const char *path) {{
    FILE *f = fopen(path, "r");
    if (!f) {{
        fprintf(stderr, "Error: Cannot open file: %s\\n", path);
        return -1;
    }}

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *code = malloc(size + 1);
    if (!code) {{
        fclose(f);
        fprintf(stderr, "Error: Out of memory\\n");
        return -1;
    }}

    size_t read_size = fread(code, 1, size, f);
    code[read_size] = '\\0';
    fclose(f);

    int result = evaluate_code(code);
    free(code);
    return result;
}}

/* ============================================================================
 * REPL Loop
 * ============================================================================ */

static void {name}_repl_loop_pipe(void) {{
    char line[4096];

    while (fgets(line, sizeof(line), stdin) != NULL) {{
        size_t len = repl_strip_newlines(line);
        if (len == 0) continue;

        int result = {name}_process_command(line);
        if (result == 1) break;
        if (result == 0) continue;

        evaluate_code(line);
        fflush(stdout);
    }}
}}

static void {name}_repl_loop(editor_ctx_t *syntax_ctx) {{
    ReplLineEditor ed;
    char *input;
    char history_path[512] = {{0}};

    /* Use non-interactive mode for piped input */
    if (!isatty(STDIN_FILENO)) {{
        {name}_repl_loop_pipe();
        return;
    }}

    repl_editor_init(&ed);

    /* Build history file path and load history */
    if (repl_get_history_path("{name}", history_path, sizeof(history_path))) {{
        repl_history_load(&ed, history_path);
    }}

    printf("{Title} REPL %s (PEG parser). Type :h for help, :q to quit.\\n", PSND_VERSION);

    repl_enable_raw_mode();

    while ((input = repl_readline(syntax_ctx, &ed, "{name}> ")) != NULL) {{
        if (input[0] == '\\0') {{
            continue;
        }}

        repl_add_history(&ed, input);

        int result = {name}_process_command(input);
        if (result == 1) break;
        if (result == 0) {{
            /* Check Link callbacks */
            shared_repl_link_check();
            continue;
        }}

        evaluate_code(input);

        /* Check Link callbacks */
        shared_repl_link_check();
    }}

    repl_disable_raw_mode();

    /* Save history */
    if (history_path[0]) {{
        repl_history_save(&ed, history_path);
    }}

    repl_editor_cleanup(&ed);
}}

/* ============================================================================
 * Main Entry Point
 * ============================================================================ */

int {name}_repl_main(int argc, char **argv) {{
    const char *input_file = NULL;
    const char *soundfont = NULL;
    const char *virtual_name = NULL;
    int port = -1;
    int list_ports = 0;

    for (int i = 1; i < argc; i++) {{
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {{
            print_usage(argv[0]);
            return 0;
        }}
        if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list") == 0) {{
            list_ports = 1;
            continue;
        }}
        if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) && i+1 < argc) {{
            port = atoi(argv[++i]);
            continue;
        }}
        if (strcmp(argv[i], "--virtual") == 0 && i+1 < argc) {{
            virtual_name = argv[++i];
            continue;
        }}
        if ((strcmp(argv[i], "-sf") == 0 || strcmp(argv[i], "--soundfont") == 0) && i+1 < argc) {{
            soundfont = argv[++i];
            continue;
        }}
        if (argv[i][0] != '-') {{
            input_file = argv[i];
        }}
    }}

    g_{name}_shared_ctx = malloc(sizeof(SharedContext));
    if (!g_{name}_shared_ctx || shared_context_init(g_{name}_shared_ctx) != 0) {{
        fprintf(stderr, "Failed to initialize shared context\\n");
        return 1;
    }}

    /* Handle --list after context is initialized */
    if (list_ports) {{
        shared_midi_list_ports(g_{name}_shared_ctx);
        shared_context_cleanup(g_{name}_shared_ctx);
        free(g_{name}_shared_ctx);
        return 0;
    }}

    if (virtual_name) {{
        shared_midi_open_virtual(g_{name}_shared_ctx, virtual_name);
    }} else if (port >= 0) {{
        shared_midi_open_port(g_{name}_shared_ctx, port);
    }}

    if (soundfont) {{
        shared_tsf_load_soundfont(soundfont);
    }}

    {name}_runtime_init(&g_{name}_runtime, g_{name}_shared_ctx);

    /* Initialize Link callbacks */
    shared_repl_link_init_callbacks(g_{name}_shared_ctx);

    if (input_file) {{
        evaluate_file(input_file);
    }}

    if (!input_file || isatty(STDIN_FILENO)) {{
        editor_ctx_t syntax_ctx;
        editor_ctx_init(&syntax_ctx);
        syntax_init_default_colors(&syntax_ctx);
        syntax_select_for_filename(&syntax_ctx, "input.{ext}");

        /* Load Lua for syntax highlighting and themes */
        LuaHost *lua_host = lua_host_create();
        if (lua_host) {{
            syntax_ctx.lua_host = lua_host;
            struct loki_lua_opts lua_opts = {{
                .bind_editor = 1,
                .load_config = 1,
            }};
            lua_host->L = loki_lua_bootstrap(&syntax_ctx, &lua_opts);
        }}

        {name}_repl_loop(&syntax_ctx);

        if (syntax_ctx.lua_host) {{
            lua_host_free(syntax_ctx.lua_host);
            syntax_ctx.lua_host = NULL;
        }}
    }}

    /* Cleanup */
    shared_repl_link_cleanup_callbacks();
    {name}_runtime_stop(&g_{name}_runtime);
    shared_context_cleanup(g_{name}_shared_ctx);
    free(g_{name}_shared_ctx);

    return 0;
}}

int {name}_play_main(int argc, char **argv) {{
    /* Simplified play mode - just execute file and exit */
    const char *soundfont = NULL;
    const char *input_file = NULL;

    for (int i = 0; i < argc; i++) {{
        if ((strcmp(argv[i], "-sf") == 0 || strcmp(argv[i], "--soundfont") == 0) && i+1 < argc) {{
            soundfont = argv[++i];
            continue;
        }}
        if (argv[i][0] != '-') {{
            input_file = argv[i];
        }}
    }}

    if (!input_file) {{
        fprintf(stderr, "Error: No input file specified\\n");
        return 1;
    }}

    g_{name}_shared_ctx = malloc(sizeof(SharedContext));
    if (!g_{name}_shared_ctx || shared_context_init(g_{name}_shared_ctx) != 0) {{
        fprintf(stderr, "Failed to initialize shared context\\n");
        return 1;
    }}

    if (soundfont) {{
        shared_tsf_load_soundfont(soundfont);
    }}

    {name}_runtime_init(&g_{name}_runtime, g_{name}_shared_ctx);

    int result = evaluate_file(input_file);

    {name}_runtime_stop(&g_{name}_runtime);
    shared_context_cleanup(g_{name}_shared_ctx);
    free(g_{name}_shared_ctx);

    return result;
}}
'''

DISPATCH_C_TEMPLATE = '''/**
 * @file dispatch.c
 * @brief {Title} language dispatch registration.
 */

#include "lang_dispatch.h"

/* Forward declarations from repl.c */
extern int {name}_repl_main(int argc, char **argv);
extern int {name}_play_main(int argc, char **argv);

static const LangDispatchEntry {name}_dispatch = {{
    .commands = {{"{name}"}},
    .command_count = 1,
    .extensions = {{{extensions_dispatch}}},
    .extension_count = {ext_count},
    .display_name = "{Title}",
    .description = "PEG-based music DSL",
    .repl_main = {name}_repl_main,
    .play_main = {name}_play_main,
}};

void {name}_dispatch_init(void) {{
    lang_dispatch_register(&{name}_dispatch);
}}
'''

# =============================================================================
# CMake Template (New Modular Architecture)
# =============================================================================

CMAKE_LANG_TEMPLATE = '''# {Title} language - PEG-based music DSL
#
# This CMakeLists.txt is processed by psnd_languages.cmake auto-discovery.
# No modifications to other files are needed to add/remove this language.

# ==============================================================================
# Build packcc tool from thirdparty (if not already available)
# ==============================================================================
if(NOT TARGET packcc_tool)
    set(PACKCC_SOURCE_DIR "${{PSND_ROOT_DIR}}/source/thirdparty/packcc-2.2.0")
    set(PACKCC_BINARY "${{CMAKE_BINARY_DIR}}/tools/packcc")

    add_custom_command(
        OUTPUT "${{PACKCC_BINARY}}"
        COMMAND ${{CMAKE_COMMAND}} -E make_directory "${{CMAKE_BINARY_DIR}}/tools"
        COMMAND ${{CMAKE_C_COMPILER}} -O2 -o "${{PACKCC_BINARY}}"
                "${{PACKCC_SOURCE_DIR}}/src/packcc.c"
        DEPENDS "${{PACKCC_SOURCE_DIR}}/src/packcc.c"
        COMMENT "Building packcc parser generator"
        VERBATIM
    )

    add_custom_target(packcc_tool DEPENDS "${{PACKCC_BINARY}}")
endif()

# ==============================================================================
# Generate parser from PEG grammar
# ==============================================================================
set({NAME}_PEG_FILE "${{CMAKE_CURRENT_SOURCE_DIR}}/impl/{name}_grammar.peg")
set({NAME}_GENERATED_C "${{CMAKE_BINARY_DIR}}/generated/{name}/{name}_grammar.c")
set({NAME}_GENERATED_H "${{CMAKE_BINARY_DIR}}/generated/{name}/{name}_grammar.h")

add_custom_command(
    OUTPUT "${{{NAME}_GENERATED_C}}" "${{{NAME}_GENERATED_H}}"
    COMMAND ${{CMAKE_COMMAND}} -E make_directory "${{CMAKE_BINARY_DIR}}/generated/{name}"
    COMMAND "${{CMAKE_BINARY_DIR}}/tools/packcc" -o "${{CMAKE_BINARY_DIR}}/generated/{name}/{name}_grammar" "${{{NAME}_PEG_FILE}}"
    DEPENDS "${{{NAME}_PEG_FILE}}" packcc_tool
    COMMENT "Generating {name} parser from PEG grammar"
    VERBATIM
)

add_custom_target({name}_parser_gen DEPENDS "${{{NAME}_GENERATED_C}}" "${{{NAME}_GENERATED_H}}")

# ==============================================================================
# Library Sources
# ==============================================================================
set({NAME}_LIB_SOURCES
    "${{{NAME}_GENERATED_C}}"
    "${{CMAKE_CURRENT_SOURCE_DIR}}/impl/{name}_runtime.c"
)

# ==============================================================================
# Build the Language Library
# ==============================================================================
add_library({name} STATIC ${{{NAME}_LIB_SOURCES}})
add_library({name}::{name} ALIAS {name})
add_dependencies({name} {name}_parser_gen)

target_include_directories({name}
    PUBLIC
        ${{PSND_ROOT_DIR}}/source/core/include
        ${{CMAKE_CURRENT_SOURCE_DIR}}/impl
        ${{CMAKE_BINARY_DIR}}/generated/{name}
    PRIVATE
        ${{PSND_ROOT_DIR}}/source/core
        ${{PSND_ROOT_DIR}}/source/core/shared
)

target_link_libraries({name} PUBLIC shared)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options({name} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ==============================================================================
# Register with psnd Language System
# ==============================================================================
psnd_register_language(
    NAME {name}
    DISPLAY_NAME "{Title}"
    DESCRIPTION "PEG-based music DSL"
    COMMANDS {name}
    EXTENSIONS {extensions_space}
    SOURCES ${{{NAME}_LIB_SOURCES}}
    INCLUDE_DIRS
        ${{CMAKE_CURRENT_SOURCE_DIR}}/impl
        ${{CMAKE_BINARY_DIR}}/generated/{name}
    REPL_SOURCES
        ${{CMAKE_CURRENT_SOURCE_DIR}}/repl.c
        ${{CMAKE_CURRENT_SOURCE_DIR}}/dispatch.c
    REGISTER_SOURCES
        ${{CMAKE_CURRENT_SOURCE_DIR}}/register.c
    LINK_LIBRARIES {name}
)
'''

# =============================================================================
# Test Templates
# =============================================================================

TEST_CMAKE_TEMPLATE = '''# {Title} language tests (PEG parser)

# Helper macro for {name} tests
macro(add_{name}_test TEST_NAME)
    add_executable(test_{name}_${{TEST_NAME}}
        test_${{TEST_NAME}}.c
        ${{PSND_ROOT_DIR}}/source/testing/test_framework.c
    )
    add_dependencies(test_{name}_${{TEST_NAME}} {name}_parser_gen)
    target_link_libraries(test_{name}_${{TEST_NAME}} PRIVATE
        {name}
        shared
        m
    )
    target_include_directories(test_{name}_${{TEST_NAME}} PRIVATE
        ${{PSND_ROOT_DIR}}/source/testing
        ${{CMAKE_CURRENT_SOURCE_DIR}}/../impl
        ${{CMAKE_BINARY_DIR}}/generated/{name}
        ${{PSND_ROOT_DIR}}/source/core/include
    )
    add_test(
        NAME {name}_${{TEST_NAME}}
        COMMAND $<TARGET_FILE:test_{name}_${{TEST_NAME}}>
        WORKING_DIRECTORY ${{CMAKE_BINARY_DIR}}
    )
    set_tests_properties({name}_${{TEST_NAME}} PROPERTIES LABELS "unit")
endmacro()

# Add all {name} tests
add_{name}_test(parser)
'''

TEST_PARSER_TEMPLATE = '''#include "test_framework.h"
#include "{name}_grammar.h"
#include "{name}_runtime.h"

static {Title}AstNode* parse_code(const char* input) {{
    {Title}ParseContext pctx = {{0}};
    pctx.input = input;

    {name}_peg_context_t *ctx = {name}_peg_create(&pctx);
    if (!ctx) return NULL;

    {Title}AstNode *ast = NULL;
    int result = {name}_peg_parse(ctx, &ast);
    {name}_peg_destroy(ctx);

    /* PackCC returns non-zero on success */
    if (result == 0) {{
        if (ast) {name}_ast_free(ast);
        return NULL;
    }}

    return ast;
}}

TEST(test_parse_note_midi_number) {{
    {Title}AstNode* ast = parse_code("note 60\\n");
    ASSERT_NOT_NULL(ast);
    ASSERT_EQ(ast->type, {NAME}_AST_PROGRAM);
    ASSERT_EQ(ast->d.program.count, 1);

    {Title}AstNode* note = ast->d.program.stmts[0];
    ASSERT_NOT_NULL(note);
    ASSERT_EQ(note->type, {NAME}_AST_NOTE);
    ASSERT_EQ(note->d.note.pitch, 60);

    {name}_ast_free(ast);
}}

TEST(test_parse_note_name) {{
    {Title}AstNode* ast = parse_code("note C4\\n");
    ASSERT_NOT_NULL(ast);
    ASSERT_EQ(ast->d.program.count, 1);

    {Title}AstNode* note = ast->d.program.stmts[0];
    ASSERT_EQ(note->type, {NAME}_AST_NOTE);
    ASSERT_EQ(note->d.note.pitch, 60);

    {name}_ast_free(ast);
}}

TEST(test_parse_chord) {{
    {Title}AstNode* ast = parse_code("chord C4 E4 G4\\n");
    ASSERT_NOT_NULL(ast);

    {Title}AstNode* chord = ast->d.program.stmts[0];
    ASSERT_EQ(chord->type, {NAME}_AST_CHORD);
    ASSERT_EQ(chord->d.chord.pitch_count, 3);
    ASSERT_EQ(chord->d.chord.pitches[0], 60);
    ASSERT_EQ(chord->d.chord.pitches[1], 64);
    ASSERT_EQ(chord->d.chord.pitches[2], 67);

    {name}_ast_free(ast);
}}

TEST(test_parse_rest) {{
    {Title}AstNode* ast = parse_code("rest 500\\n");
    ASSERT_NOT_NULL(ast);

    {Title}AstNode* rest = ast->d.program.stmts[0];
    ASSERT_EQ(rest->type, {NAME}_AST_REST);
    ASSERT_EQ(rest->d.rest.duration_ms, 500);

    {name}_ast_free(ast);
}}

TEST(test_parse_tempo) {{
    {Title}AstNode* ast = parse_code("tempo 140\\n");
    ASSERT_NOT_NULL(ast);

    {Title}AstNode* tempo = ast->d.program.stmts[0];
    ASSERT_EQ(tempo->type, {NAME}_AST_TEMPO);
    ASSERT_EQ(tempo->d.tempo.bpm, 140);

    {name}_ast_free(ast);
}}

BEGIN_TEST_SUITE("{Title} PEG Parser Tests")
    RUN_TEST(test_parse_note_midi_number);
    RUN_TEST(test_parse_note_name);
    RUN_TEST(test_parse_chord);
    RUN_TEST(test_parse_rest);
    RUN_TEST(test_parse_tempo);
END_TEST_SUITE()
'''

# =============================================================================
# Documentation Templates
# =============================================================================

DOC_README_TEMPLATE = '''# {Title} Language (PEG Parser)

{Title} is a minimal music programming language for psnd, using a PackCC-generated
PEG parser. It supports note names (C4, C#4, Db3) as well as MIDI numbers.

## Quick Start

```bash
# Build (CMake automatically discovers languages and builds packcc)
make clean && make test

# Start the REPL
psnd {name}

# Or with a virtual MIDI port
psnd {name} --virtual {Title}Out

# Play a file
psnd {name} song.{ext}

# With a soundfont
psnd {name} -sf /path/to/soundfont.sf2 song.{ext}
```

## Syntax

### Note

```
note <pitch> [duration_ms] [velocity]
```

Examples:
```{name}
note C4                # Middle C, 250ms, velocity 80
note C#4 500           # C#4, 500ms
note 60 250 100        # MIDI 60, 250ms, velocity 100
```

### Chord

```
chord <pitch1> <pitch2> ... [dur:<ms>] [vel:<velocity>]
```

Examples:
```{name}
chord C4 E4 G4                  # C major chord
chord C4 E4 G4 dur:1000         # C major, 1 second
```

### Rest

```
rest <duration_ms>
```

### Tempo

```
tempo <bpm>
```

### Comments

Lines starting with `#` are comments.

## Modifying the Grammar

The PEG grammar is in `source/langs/{name}/impl/{name}_grammar.peg`. To iterate:

```bash
# Edit the grammar
vim source/langs/{name}/impl/{name}_grammar.peg

# Rebuild - parser regenerates automatically
make
```

## REPL Commands

| Command | Description |
|---------|-------------|
| `:help` or `:h` | Show help |
| `:quit` or `:q` | Exit REPL |
| `:stop` or `:s` | Stop playback |
| `:midi` | List MIDI ports |
| `:midi N` | Connect to port N |

## Lua API

```lua
loki.{name}.init()           -- Initialize
loki.{name}.eval(code)       -- Evaluate code
loki.{name}.stop()           -- Stop playback
```
'''

SYNTAX_LUA_TEMPLATE = '''-- {Title} language syntax highlighting
loki.register_language({{
    name = "{Title}",
    extensions = {{{extensions_lua}}},
    keywords = {{"note", "chord", "rest", "tempo"}},
    types = {{"dur", "vel"}},
    line_comment = "#",
    block_comment_start = nil,
    block_comment_end = nil,
    string_delimiters = {{'"', "'"}},
}})
'''

EXAMPLE_TEMPLATE = '''# Example {Title} program
# Play a simple melody using PEG-parsed syntax

tempo 120

# C major arpeggio (using note names)
note C4 300
note E4 300
note G4 300
note C5 600

rest 200

# C major chord
chord C4 E4 G4 dur:800

rest 100

# Descending with MIDI numbers
note 72 300
note 67 300
note 64 300
note 60 600
'''

# =============================================================================
# File Generation
# =============================================================================

def create_file(path: Path, content: str, dry_run: bool = False) -> None:
    """Create a file with the given content."""
    if dry_run:
        print(f"  [dry-run] Would create: {path}")
        return

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content)
    print(f"  Created: {path}")


# =============================================================================
# Main Generation Logic
# =============================================================================

def generate_language(
    name: str,
    extensions: List[str],
    root: Path,
    dry_run: bool = False
) -> None:
    """Generate all files for a new PEG-based language using modular architecture."""

    name_lower = to_lower(name)
    name_upper = to_upper(name)
    name_title = to_title(name)

    ext_c = ", ".join(f'".{e.lstrip(".")}"' for e in extensions) + ", NULL"
    ext_dispatch = ", ".join(f'".{e.lstrip(".")}"' for e in extensions)
    ext_lua = ", ".join(f'".{e.lstrip(".")}"' for e in extensions)
    ext_space = " ".join(f'.{e.lstrip(".")}' for e in extensions)
    primary_ext = extensions[0].lstrip(".")

    subs = {
        "name": name_lower,
        "Name": name_lower,
        "NAME": name_upper,
        "Title": name_title,
        "extensions_c": ext_c,
        "extensions_dispatch": ext_dispatch,
        "ext_count": str(len(extensions)),
        "extensions_lua": ext_lua,
        "extensions_space": ext_space,
        "ext": primary_ext,
    }

    print(f"\nGenerating PEG-based language: {name_title}")
    print(f"  Extensions: {extensions}")
    print(f"  Directory: source/langs/{name_lower}/")
    print()

    # Check if language already exists
    lang_dir = root / "source" / "langs" / name_lower
    if lang_dir.exists() and not dry_run:
        print(f"Error: Language directory already exists: {lang_dir}")
        print("Remove it first if you want to regenerate.")
        sys.exit(1)

    # === Create new files in source/langs/<name>/ ===
    print("Creating files:")

    impl_dir = lang_dir / "impl"

    # PEG grammar
    create_file(impl_dir / f"{name_lower}_grammar.peg", PEG_GRAMMAR_TEMPLATE.format(**subs), dry_run)

    # Runtime
    create_file(impl_dir / f"{name_lower}_runtime.h", RUNTIME_H_TEMPLATE.format(**subs), dry_run)
    create_file(impl_dir / f"{name_lower}_runtime.c", RUNTIME_C_TEMPLATE.format(**subs), dry_run)

    # Register header and implementation
    create_file(lang_dir / "register.h", REGISTER_H_TEMPLATE.format(**subs), dry_run)
    create_file(lang_dir / "register.c", REGISTER_C_TEMPLATE.format(**subs), dry_run)

    # REPL (no separate header - uses extern declarations in dispatch.c)
    create_file(lang_dir / "repl.c", REPL_C_TEMPLATE.format(**subs), dry_run)

    # Dispatch
    create_file(lang_dir / "dispatch.c", DISPATCH_C_TEMPLATE.format(**subs), dry_run)

    # CMakeLists.txt for the language
    create_file(lang_dir / "CMakeLists.txt", CMAKE_LANG_TEMPLATE.format(**subs), dry_run)

    # Tests
    test_dir = lang_dir / "tests"
    create_file(test_dir / "CMakeLists.txt", TEST_CMAKE_TEMPLATE.format(**subs), dry_run)
    create_file(test_dir / "test_parser.c", TEST_PARSER_TEMPLATE.format(**subs), dry_run)

    # Documentation
    create_file(lang_dir / "README.md", DOC_README_TEMPLATE.format(**subs), dry_run)

    # Syntax highlighting
    create_file(
        root / ".psnd" / "languages" / f"{name_lower}.lua",
        SYNTAX_LUA_TEMPLATE.format(**subs),
        dry_run
    )

    # Examples directory with sample file
    create_file(
        lang_dir / "examples" / f"melody.{primary_ext}",
        EXAMPLE_TEMPLATE.format(**subs),
        dry_run
    )

    print("\nDone!")
    if not dry_run:
        print(f"\nYour PEG-based language is ready! Next steps:")
        print(f"  1. Build and test: make clean && make test")
        print(f"     (CMake will auto-discover the language and build it)")
        print(f"  2. Start the REPL: ./build/psnd {name_lower}")
        print(f"  3. Try: note C4        # Play middle C")
        print(f"  4. Try: chord C4 E4 G4 # C major chord")
        print(f"")
        print(f"To modify the grammar:")
        print(f"  1. Edit: source/langs/{name_lower}/impl/{name_lower}_grammar.peg")
        print(f"  2. Run: make")
        print(f"     (Parser regenerates automatically when .peg file changes)")
        print(f"")
        print(f"NO modifications to shared files are needed!")
        print(f"The language is auto-discovered from source/langs/{name_lower}/CMakeLists.txt")


def main():
    parser = argparse.ArgumentParser(
        description="Generate a PEG-based music language for psnd using PackCC",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
MODULAR ARCHITECTURE:
This script uses the modular language architecture. Creating a new language
only requires adding files under source/langs/<name>/ - NO modifications to any
shared files (lang_config.h, lang_dispatch.c, CMakeLists.txt, etc.) are needed.

The language is automatically discovered by CMake when you run 'make'.

Examples:
  %(prog)s foo                    # Create 'foo' with extension .foo
  %(prog)s bar -e .bar .br        # Create 'bar' with extensions .bar and .br
  %(prog)s baz --dry-run          # Preview what would be created

Workflow:
  1. %(prog)s mymusic             # Generate language skeleton
  2. make clean && make test      # Build (auto-discovers and builds language)
  3. ./build/psnd mymusic         # Start the REPL
  4. Edit source/langs/mymusic/impl/mymusic_grammar.peg
  5. make                         # Parser regenerates automatically
"""
    )

    parser.add_argument(
        "name",
        help="Language name (lowercase, e.g., 'foo')"
    )

    parser.add_argument(
        "-e", "--extensions",
        nargs="+",
        help="File extensions (default: .<name>)"
    )

    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show what would be created without making changes"
    )

    args = parser.parse_args()

    # Validate name
    name = args.name.lower()
    if not re.match(r'^[a-z][a-z0-9]*$', name):
        print("Error: Language name must be lowercase alphanumeric starting with a letter", file=sys.stderr)
        sys.exit(1)

    # Check for reserved names (existing languages)
    reserved = ["alda", "joy", "bog", "tr7", "scheme"]
    if name in reserved:
        print(f"Error: '{name}' is already a built-in language", file=sys.stderr)
        sys.exit(1)

    extensions = args.extensions if args.extensions else [f".{name}"]

    try:
        root = get_project_root()
    except RuntimeError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

    generate_language(name, extensions, root, args.dry_run)


if __name__ == "__main__":
    main()
