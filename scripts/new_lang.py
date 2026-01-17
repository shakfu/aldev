#!/usr/bin/env python3
"""
Generate boilerplate for a new psnd language.

This script creates all necessary files and updates existing configuration
to add a new music programming language to psnd.
"""

import argparse
import os
import re
import sys
from pathlib import Path
from typing import List, Optional


def get_project_root() -> Path:
    """Find the project root (directory containing CMakeLists.txt)."""
    script_dir = Path(__file__).resolve().parent
    # Script is in scripts/, so parent is project root
    root = script_dir.parent
    if not (root / "CMakeLists.txt").exists():
        # Try going up one more level
        root = root.parent
    if not (root / "CMakeLists.txt").exists():
        raise RuntimeError("Could not find project root (no CMakeLists.txt found)")
    return root


def to_upper(name: str) -> str:
    """Convert to uppercase (for macros)."""
    return name.upper()


def to_title(name: str) -> str:
    """Convert to title case."""
    return name.title()


def to_lower(name: str) -> str:
    """Convert to lowercase."""
    return name.lower()


# =============================================================================
# File Templates
# =============================================================================

REGISTER_H_TEMPLATE = '''#ifndef {NAME}_REGISTER_H
#define {NAME}_REGISTER_H

/**
 * Initialize {Title} language registration with the language bridge.
 * Called from loki_lang_init() when LANG_{NAME} is defined.
 */
void {name}_loki_lang_init(void);

#endif /* {NAME}_REGISTER_H */
'''

REGISTER_C_TEMPLATE = '''/**
 * @file register.c
 * @brief {Title} language integration with Loki editor.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "register.h"
#include "psnd.h"
#include "loki/internal.h"
#include "loki/lang_bridge.h"
#include "loki/lua.h"
#include "lauxlib.h"

#include "shared/context.h"

/* ============================================================================
 * Per-Context State
 * ============================================================================ */

struct Loki{Title}State {{
    int initialized;
    SharedContext *shared;
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

    /* Already initialized? */
    if (ctx->model.{name}_state && ctx->model.{name}_state->initialized) {{
        return 0;
    }}

    /* Allocate state */
    struct Loki{Title}State *state = ctx->model.{name}_state;
    if (!state) {{
        state = calloc(1, sizeof(struct Loki{Title}State));
        if (!state) return -1;
        ctx->model.{name}_state = state;
    }}

    /* Create shared context for MIDI/audio */
    state->shared = malloc(sizeof(SharedContext));
    if (!state->shared || shared_context_init(state->shared) != 0) {{
        set_error(state, "Failed to initialize shared context");
        free(state);
        ctx->model.{name}_state = NULL;
        return -1;
    }}

    /* Open virtual MIDI port */
    shared_midi_open_virtual(state->shared, PSND_MIDI_PORT_NAME "-{name}");

    state->initialized = 1;
    set_error(state, NULL);
    return 0;
}}

static void {name}_lang_cleanup(editor_ctx_t *ctx) {{
    if (!ctx) return;

    struct Loki{Title}State *state = get_state(ctx);
    if (!state) return;

    if (state->shared) {{
        shared_midi_panic(state->shared);
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

    /* TODO: Implement evaluation */
    (void)code;
    set_error(state, NULL);
    return 0;
}}

static void {name}_lang_stop(editor_ctx_t *ctx) {{
    struct Loki{Title}State *state = get_state(ctx);
    if (!state || !state->initialized) return;

    if (state->shared) {{
        shared_midi_panic(state->shared);
    }}
}}

static const char *{name}_lang_get_error(editor_ctx_t *ctx) {{
    struct Loki{Title}State *state = get_state(ctx);
    if (!state) return NULL;
    return state->last_error[0] ? state->last_error : NULL;
}}

static int {name}_lang_is_playing(editor_ctx_t *ctx) {{
    struct Loki{Title}State *state = get_state(ctx);
    if (!state || !state->initialized) return 0;
    return 0;  /* TODO: Implement */
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

    /* Lifecycle */
    .init = {name}_lang_init,
    .cleanup = {name}_lang_cleanup,
    .is_initialized = {name}_lang_is_initialized,

    /* Main loop (NULL if not needed) */
    .check_callbacks = NULL,

    /* Playback */
    .eval = {name}_lang_eval,
    .stop = {name}_lang_stop,
    .is_playing = {name}_lang_is_playing,

    /* Export (NULL if not supported) */
    .has_events = NULL,
    .populate_shared_buffer = NULL,

    /* Error */
    .get_error = {name}_lang_get_error,

    /* Backend (NULL if not supported) */
    .configure_backend = NULL,

    /* Lua API */
    .register_lua_api = {name}_register_lua_api,
}};

void {name}_loki_lang_init(void) {{
    loki_lang_register(&{name}_lang_ops);
}}
'''

REPL_H_TEMPLATE = '''#ifndef {NAME}_REPL_H
#define {NAME}_REPL_H

int {name}_repl_main(int argc, char **argv);

#endif /* {NAME}_REPL_H */
'''

REPL_C_TEMPLATE = '''/**
 * @file repl.c
 * @brief {Title} language REPL with shared command handling.
 */

#include "repl.h"
#include "psnd.h"
#include "loki/core.h"
#include "loki/internal.h"
#include "loki/repl_launcher.h"
#include "shared/repl_commands.h"
#include "shared/context.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ============================================================================
 * REPL State
 * ============================================================================ */

static SharedContext *g_shared_ctx = NULL;

/* ============================================================================
 * Usage and Help
 * ============================================================================ */

static void print_usage(const char *prog) {{
    printf("Usage: %s [options] [file.{ext}]\\n", prog);
    printf("\\n{Title} music language interpreter.\\n\\n");
    printf("Options:\\n");
    printf("  -h, --help        Show this help message\\n");
    printf("  -l, --list        List available MIDI ports\\n");
    printf("  -p, --port N      Use MIDI port N\\n");
    printf("  --virtual NAME    Create virtual MIDI port\\n");
    printf("  -sf PATH          Use built-in synth with soundfont\\n");
}}

static void print_help(void) {{
    shared_print_command_help();

    printf("{Title}-specific Commands:\\n");
    printf("  (none yet)\\n");
    printf("\\n");
}}

/* ============================================================================
 * Command Processing
 * ============================================================================ */

static void {name}_stop_playback(void) {{
    if (g_shared_ctx) {{
        shared_midi_panic(g_shared_ctx);
    }}
}}

static int process_command(const char *input) {{
    int result = shared_process_command(g_shared_ctx, input, {name}_stop_playback);
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
 * REPL Loop
 * ============================================================================ */

static void repl_loop(editor_ctx_t *syntax_ctx) {{
    ReplLineEditor ed;
    char *input;

    if (!isatty(STDIN_FILENO)) {{
        char line[4096];
        while (fgets(line, sizeof(line), stdin)) {{
            size_t len = strlen(line);
            while (len > 0 && (line[len-1] == '\\n' || line[len-1] == '\\r'))
                line[--len] = '\\0';
            if (len == 0) continue;

            int result = process_command(line);
            if (result == 1) break;
            if (result == 0) continue;

            /* TODO: Evaluate code */
            printf("eval: %s\\n", line);
        }}
        return;
    }}

    repl_editor_init(&ed);

    printf("{Title} REPL. Type :help for commands, :quit to exit.\\n");

    while ((input = repl_editor_readline(&ed, "{name}> ", syntax_ctx)) != NULL) {{
        if (input[0] == '\\0') {{
            free(input);
            continue;
        }}

        repl_editor_add_history(&ed, input);

        int result = process_command(input);
        if (result == 1) {{
            free(input);
            break;
        }}
        if (result == 0) {{
            free(input);
            continue;
        }}

        /* TODO: Evaluate code */
        printf("eval: %s\\n", input);

        free(input);
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

    for (int i = 1; i < argc; i++) {{
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {{
            print_usage(argv[0]);
            return 0;
        }}
        if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list") == 0) {{
            shared_midi_list_ports();
            return 0;
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

    g_shared_ctx = malloc(sizeof(SharedContext));
    if (!g_shared_ctx || shared_context_init(g_shared_ctx) != 0) {{
        fprintf(stderr, "Failed to initialize shared context\\n");
        return 1;
    }}

    if (virtual_name) {{
        shared_midi_open_virtual(g_shared_ctx, virtual_name);
    }} else if (port >= 0) {{
        shared_midi_open_port(g_shared_ctx, port);
    }}

    if (soundfont) {{
        shared_audio_tsf_load(g_shared_ctx, soundfont);
    }}

    if (input_file) {{
        /* TODO: Evaluate file */
        printf("Would evaluate file: %s\\n", input_file);
    }}

    if (!input_file || isatty(STDIN_FILENO)) {{
        editor_ctx_t syntax_ctx = {{0}};
        repl_loop(&syntax_ctx);
    }}

    shared_midi_panic(g_shared_ctx);
    shared_context_cleanup(g_shared_ctx);
    free(g_shared_ctx);

    return 0;
}}
'''

DISPATCH_C_TEMPLATE = '''/**
 * @file dispatch.c
 * @brief CLI dispatch for {Title} language.
 */

#include "repl.h"

int {name}_dispatch(int argc, char **argv) {{
    return {name}_repl_main(argc, argv);
}}
'''

CMAKE_LIBRARY_TEMPLATE = '''# {Title} language library
include_guard(GLOBAL)

set({NAME}_SOURCES
    # Add implementation sources here
    # ${{PSND_ROOT_DIR}}/src/lang/{name}/impl/{name}_parser.c
    # ${{PSND_ROOT_DIR}}/src/lang/{name}/impl/{name}_runtime.c
)

if({NAME}_SOURCES)
    add_library({name} STATIC ${{{NAME}_SOURCES}})

    target_include_directories({name} PUBLIC
        ${{PSND_ROOT_DIR}}/src/lang/{name}/impl
    )

    target_link_libraries({name} PRIVATE shared)
else()
    # Placeholder library until implementation is added
    add_library({name} INTERFACE)
endif()
'''

TEST_CMAKE_TEMPLATE = '''# {Title} language tests

# add_executable(test_{name}_parser test_parser.c)
# target_link_libraries(test_{name}_parser PRIVATE {name} test_framework)
# target_include_directories(test_{name}_parser PRIVATE
#     ${{PSND_ROOT_DIR}}/src/lang/{name}/impl
#     ${{PSND_ROOT_DIR}}/tests
# )
# add_test(NAME {name}_parser COMMAND test_{name}_parser)
# set_tests_properties({name}_parser PROPERTIES LABELS "unit")
'''

TEST_PARSER_TEMPLATE = '''#include "../test_framework.h"

TEST(test_placeholder) {{
    /* TODO: Add {name} parser tests */
    ASSERT_TRUE(1);
}}

TEST_SUITE({name}_parser_tests) {{
    RUN_TEST(test_placeholder);
}}

int main(void) {{
    RUN_SUITE({name}_parser_tests);
    return TEST_REPORT();
}}
'''

DOC_README_TEMPLATE = '''# {Title} Language

{Title} is a music programming language for psnd.

## Quick Start

```bash
# Start the REPL
psnd {name}

# Or with a virtual MIDI port
psnd {name} --virtual {Title}Out
```

## REPL Commands

| Command | Description |
|---------|-------------|
| `:help` | Show help |
| `:quit` | Exit REPL |
| `:stop` | Stop playback |
| `:midi` | List MIDI ports |
| `:midi N` | Connect to port N |

## Editor Integration

Open a `.{ext}` file in the editor:

```bash
psnd song.{ext}
```

### Keybindings

| Key | Action |
|-----|--------|
| `Ctrl-E` | Evaluate buffer |
| `Ctrl-G` | Stop playback |

## Lua API

```lua
loki.{name}.init()           -- Initialize
loki.{name}.eval(code)       -- Evaluate code
loki.{name}.stop()           -- Stop playback
```

## Syntax

TODO: Document {name} syntax.

## Examples

```{name}
-- TODO: Add examples
```
'''

SYNTAX_LUA_TEMPLATE = '''-- {Title} language syntax highlighting
loki.register_language({{
    name = "{Title}",
    extensions = {{{extensions_lua}}},
    keywords = {{}},
    types = {{}},
    line_comment = "--",
    block_comment_start = nil,
    block_comment_end = nil,
    string_delimiters = {{'"', "'"}},
}})
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


def update_file(path: Path, old: str, new: str, dry_run: bool = False) -> bool:
    """Update a file by replacing old with new. Returns True if updated."""
    if not path.exists():
        print(f"  Warning: {path} does not exist, skipping update")
        return False

    content = path.read_text()
    if old not in content:
        return False

    if dry_run:
        print(f"  [dry-run] Would update: {path}")
        return True

    new_content = content.replace(old, new)
    path.write_text(new_content)
    print(f"  Updated: {path}")
    return True


def append_to_file(path: Path, marker: str, addition: str, dry_run: bool = False) -> bool:
    """Append addition after marker in file. Returns True if updated."""
    if not path.exists():
        print(f"  Warning: {path} does not exist, skipping update")
        return False

    content = path.read_text()
    if marker not in content:
        print(f"  Warning: Marker not found in {path}")
        return False

    if dry_run:
        print(f"  [dry-run] Would update: {path}")
        return True

    new_content = content.replace(marker, marker + addition)
    path.write_text(new_content)
    print(f"  Updated: {path}")
    return True


# =============================================================================
# Main Generation Logic
# =============================================================================

def generate_language(
    name: str,
    extensions: List[str],
    root: Path,
    dry_run: bool = False
) -> None:
    """Generate all files for a new language."""

    name_lower = to_lower(name)
    name_upper = to_upper(name)
    name_title = to_title(name)

    # Format extensions for C code: ".ext", ".ext2", NULL
    ext_c = ", ".join(f'".{e.lstrip(".")}"' for e in extensions) + ", NULL"
    # Format extensions for Lua: ".ext", ".ext2"
    ext_lua = ", ".join(f'".{e.lstrip(".")}"' for e in extensions)
    # Primary extension (without dot)
    primary_ext = extensions[0].lstrip(".")

    # Template substitutions
    subs = {
        "name": name_lower,
        "Name": name_lower,  # for compatibility
        "NAME": name_upper,
        "Title": name_title,
        "extensions_c": ext_c,
        "extensions_lua": ext_lua,
        "ext": primary_ext,
    }

    print(f"\nGenerating language: {name_title}")
    print(f"  Extensions: {extensions}")
    print()

    # === Create new files ===
    print("Creating new files:")

    lang_dir = root / "src" / "lang" / name_lower

    create_file(
        lang_dir / "register.h",
        REGISTER_H_TEMPLATE.format(**subs),
        dry_run
    )
    create_file(
        lang_dir / "register.c",
        REGISTER_C_TEMPLATE.format(**subs),
        dry_run
    )
    create_file(
        lang_dir / "repl.h",
        REPL_H_TEMPLATE.format(**subs),
        dry_run
    )
    create_file(
        lang_dir / "repl.c",
        REPL_C_TEMPLATE.format(**subs),
        dry_run
    )
    create_file(
        lang_dir / "dispatch.c",
        DISPATCH_C_TEMPLATE.format(**subs),
        dry_run
    )

    # Create impl directory
    impl_dir = lang_dir / "impl"
    if not dry_run:
        impl_dir.mkdir(parents=True, exist_ok=True)
        print(f"  Created: {impl_dir}/")
    else:
        print(f"  [dry-run] Would create: {impl_dir}/")

    # CMake library
    create_file(
        root / "scripts" / "cmake" / f"psnd_{name_lower}_library.cmake",
        CMAKE_LIBRARY_TEMPLATE.format(**subs),
        dry_run
    )

    # Tests
    test_dir = root / "tests" / name_lower
    create_file(
        test_dir / "CMakeLists.txt",
        TEST_CMAKE_TEMPLATE.format(**subs),
        dry_run
    )
    create_file(
        test_dir / "test_parser.c",
        TEST_PARSER_TEMPLATE.format(**subs),
        dry_run
    )

    # Documentation
    create_file(
        root / "docs" / name_lower / "README.md",
        DOC_README_TEMPLATE.format(**subs),
        dry_run
    )

    # Syntax highlighting
    create_file(
        root / ".psnd" / "languages" / f"{name_lower}.lua",
        SYNTAX_LUA_TEMPLATE.format(**subs),
        dry_run
    )

    # === Update existing files ===
    print("\nUpdating existing files:")

    # 1. Update src/lang_config.h
    lang_config = root / "src" / "lang_config.h"
    if lang_config.exists():
        content = lang_config.read_text()

        # Check if already added
        if f"LANG_{name_upper}" in content:
            print(f"  Skipping {lang_config} (already contains LANG_{name_upper})")
        else:
            # Find insertion points and add new language
            new_content = content

            # Add helper macro
            helper_marker = "#define IF_LANG_BOG(x)\n#endif"
            helper_addition = f'''

#ifdef LANG_{name_upper}
#define IF_LANG_{name_upper}(x) x
#else
#define IF_LANG_{name_upper}(x)
#endif'''
            new_content = new_content.replace(helper_marker, helper_marker + helper_addition)

            # Add forward declaration
            fwd_marker = "IF_LANG_BOG(struct LokiBogState;)"
            fwd_addition = f"\nIF_LANG_{name_upper}(struct Loki{name_title}State;)"
            new_content = new_content.replace(fwd_marker, fwd_marker + fwd_addition)

            # Add state field
            state_marker = "IF_LANG_BOG(struct LokiBogState *bog_state;)"
            state_addition = f" \\\n    IF_LANG_{name_upper}(struct Loki{name_title}State *{name_lower}_state;)"
            new_content = new_content.replace(state_marker, state_marker + state_addition)

            # Add init declaration
            init_decl_marker = "IF_LANG_BOG(void bog_loki_lang_init(void);)"
            init_decl_addition = f"\nIF_LANG_{name_upper}(void {name_lower}_loki_lang_init(void);)"
            new_content = new_content.replace(init_decl_marker, init_decl_marker + init_decl_addition)

            # Add init call
            init_call_marker = "IF_LANG_BOG(bog_loki_lang_init();)"
            init_call_addition = f" \\\n    IF_LANG_{name_upper}({name_lower}_loki_lang_init();)"
            new_content = new_content.replace(init_call_marker, init_call_marker + init_call_addition)

            if dry_run:
                print(f"  [dry-run] Would update: {lang_config}")
            else:
                lang_config.write_text(new_content)
                print(f"  Updated: {lang_config}")

    # 2. Update src/lang_dispatch.c
    lang_dispatch = root / "src" / "lang_dispatch.c"
    if lang_dispatch.exists():
        content = lang_dispatch.read_text()
        if f'"{name_lower}"' not in content:
            # Add include
            include_marker = "#ifdef LANG_BOG\n#include"
            include_addition = f'''#ifdef LANG_{name_upper}
#include "lang/{name_lower}/repl.h"
#endif

'''
            new_content = content.replace(include_marker, include_addition + include_marker)

            # Add dispatch case - find the return -1 line
            dispatch_marker = "    return -1;  /* Unknown language */"
            dispatch_addition = f'''#ifdef LANG_{name_upper}
    if (strcmp(lang, "{name_lower}") == 0) {{
        return {name_lower}_repl_main(argc, argv);
    }}
#endif

    '''
            new_content = new_content.replace(dispatch_marker, dispatch_addition + dispatch_marker)

            if dry_run:
                print(f"  [dry-run] Would update: {lang_dispatch}")
            else:
                lang_dispatch.write_text(new_content)
                print(f"  Updated: {lang_dispatch}")
        else:
            print(f"  Skipping {lang_dispatch} (already contains {name_lower})")

    # 3. Update CMakeLists.txt
    cmakelists = root / "CMakeLists.txt"
    if cmakelists.exists():
        content = cmakelists.read_text()
        if f"LANG_{name_upper}" not in content:
            # Add option after LANG_BOG
            option_marker = 'option(LANG_BOG "Include the Bog language" ON)'
            option_addition = f'\noption(LANG_{name_upper} "Include the {name_title} language" ON)'
            new_content = content.replace(option_marker, option_marker + option_addition)

            # Add include after bog library include
            include_marker = "if(LANG_BOG)\n    include(psnd_bog_library)\nendif()"
            include_addition = f'''

if(LANG_{name_upper})
    include(psnd_{name_lower}_library)
endif()'''
            new_content = new_content.replace(include_marker, include_marker + include_addition)

            if dry_run:
                print(f"  [dry-run] Would update: {cmakelists}")
            else:
                cmakelists.write_text(new_content)
                print(f"  Updated: {cmakelists}")
        else:
            print(f"  Skipping {cmakelists} (already contains LANG_{name_upper})")

    # 4. Update psnd_loki_library.cmake
    loki_cmake = root / "scripts" / "cmake" / "psnd_loki_library.cmake"
    if loki_cmake.exists():
        content = loki_cmake.read_text()
        if f"LANG_{name_upper}" not in content:
            # Add source
            src_marker = "if(LANG_BOG)\n    list(APPEND LOKI_LANG_SOURCES ${PSND_ROOT_DIR}/src/lang/bog/register.c)\nendif()"
            src_addition = f'''

if(LANG_{name_upper})
    list(APPEND LOKI_LANG_SOURCES ${{PSND_ROOT_DIR}}/src/lang/{name_lower}/register.c)
endif()'''
            new_content = content.replace(src_marker, src_marker + src_addition)

            # Add library link and compile definition
            link_marker = "if(LANG_BOG)\n    list(APPEND LOKI_PUBLIC_LIBS bog)\n    target_compile_definitions(libloki PUBLIC LANG_BOG=1)\nendif()"
            link_addition = f'''

if(LANG_{name_upper})
    list(APPEND LOKI_PUBLIC_LIBS {name_lower})
    target_compile_definitions(libloki PUBLIC LANG_{name_upper}=1)
endif()'''
            new_content = new_content.replace(link_marker, link_marker + link_addition)

            if dry_run:
                print(f"  [dry-run] Would update: {loki_cmake}")
            else:
                loki_cmake.write_text(new_content)
                print(f"  Updated: {loki_cmake}")
        else:
            print(f"  Skipping {loki_cmake} (already contains LANG_{name_upper})")

    # 5. Update psnd_psnd_binary.cmake
    psnd_cmake = root / "scripts" / "cmake" / "psnd_psnd_binary.cmake"
    if psnd_cmake.exists():
        content = psnd_cmake.read_text()
        if f"LANG_{name_upper}" not in content:
            marker = "if(LANG_BOG)\n    list(APPEND PSND_LANG_SOURCES\n        ${PSND_ROOT_DIR}/src/lang/bog/repl.c\n        ${PSND_ROOT_DIR}/src/lang/bog/dispatch.c\n    )\nendif()"
            addition = f'''

if(LANG_{name_upper})
    list(APPEND PSND_LANG_SOURCES
        ${{PSND_ROOT_DIR}}/src/lang/{name_lower}/repl.c
        ${{PSND_ROOT_DIR}}/src/lang/{name_lower}/dispatch.c
    )
endif()'''
            new_content = content.replace(marker, marker + addition)

            if dry_run:
                print(f"  [dry-run] Would update: {psnd_cmake}")
            else:
                psnd_cmake.write_text(new_content)
                print(f"  Updated: {psnd_cmake}")
        else:
            print(f"  Skipping {psnd_cmake} (already contains LANG_{name_upper})")

    # 6. Update psnd_tests.cmake
    tests_cmake = root / "scripts" / "cmake" / "psnd_tests.cmake"
    if tests_cmake.exists():
        content = tests_cmake.read_text()
        if f"tests/{name_lower}" not in content:
            marker = "if(LANG_BOG)\n    add_subdirectory(${PSND_ROOT_DIR}/tests/bog ${CMAKE_BINARY_DIR}/tests/bog)\nendif()"
            addition = f'''

if(LANG_{name_upper})
    add_subdirectory(${{PSND_ROOT_DIR}}/tests/{name_lower} ${{CMAKE_BINARY_DIR}}/tests/{name_lower})
endif()'''
            new_content = content.replace(marker, marker + addition)

            if dry_run:
                print(f"  [dry-run] Would update: {tests_cmake}")
            else:
                tests_cmake.write_text(new_content)
                print(f"  Updated: {tests_cmake}")
        else:
            print(f"  Skipping {tests_cmake} (already contains tests/{name_lower})")

    print("\nDone!")
    if not dry_run:
        print(f"\nNext steps:")
        print(f"  1. Implement your language in src/lang/{name_lower}/impl/")
        print(f"  2. Update scripts/cmake/psnd_{name_lower}_library.cmake with sources")
        print(f"  3. Run 'make clean && make test' to verify")


def main():
    parser = argparse.ArgumentParser(
        description="Generate boilerplate for a new psnd language",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s foo                    # Create language 'foo' with extension .foo
  %(prog)s bar -e .bar .br        # Create 'bar' with extensions .bar and .br
  %(prog)s baz --dry-run          # Preview what would be created
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
        print(f"Error: Language name must be lowercase alphanumeric starting with a letter", file=sys.stderr)
        sys.exit(1)

    # Default extension is .<name>
    extensions = args.extensions if args.extensions else [f".{name}"]

    try:
        root = get_project_root()
    except RuntimeError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

    generate_language(name, extensions, root, args.dry_run)


if __name__ == "__main__":
    main()
