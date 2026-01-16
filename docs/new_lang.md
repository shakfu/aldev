# Adding a New Language to psnd

This guide explains how to add a new music programming language to psnd using the language bridge system.

## Overview

psnd uses a language bridge pattern that allows music languages to integrate with the editor without coupling core code to specific implementations. Each language:

1. Implements the `LokiLangOps` interface
2. Registers itself at startup using a constructor
3. Optionally provides Lua API bindings for scripting

## Quick Start

To add a new language called "example" with file extension `.ex`:

1. Create `src/loki/lang/example.c`
2. Implement the `LokiLangOps` interface
3. Register the language using `__attribute__((constructor))`
4. Add to CMakeLists.txt with a `LANG_EXAMPLE` option
5. Optionally add Lua bindings

## Step 1: Create the Language Source File

Create `src/loki/lang/example.c`:

```c
/* example.c - Example language integration for Loki
 *
 * Integrates the Example music language with Loki editor.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../internal.h"
#include "../lang_bridge.h"
#include "loki/lua.h"    /* For loki_lua_get_editor_context */
#include "lauxlib.h"     /* For luaL_checkstring, etc. */

/* ======================= Internal State ======================= */

/* Per-context state for the language */
typedef struct LokiExampleState {
    int initialized;
    char last_error[256];
    /* Add language-specific state here */
} LokiExampleState;

/* Get state from editor context */
static LokiExampleState* get_example_state(editor_ctx_t *ctx) {
    return ctx ? ctx->example_state : NULL;
}

static void set_error(LokiExampleState *state, const char *msg) {
    if (!state) return;
    if (msg) {
        strncpy(state->last_error, msg, sizeof(state->last_error) - 1);
        state->last_error[sizeof(state->last_error) - 1] = '\0';
    } else {
        state->last_error[0] = '\0';
    }
}
```

## Step 2: Implement LokiLangOps Functions

The `LokiLangOps` struct defines the interface. Here are the key functions:

### Required Functions

```c
/* Initialize the language subsystem */
static int example_init(editor_ctx_t *ctx) {
    if (!ctx) return -1;

    /* Check if already initialized */
    if (ctx->example_state && ctx->example_state->initialized) {
        return 0;
    }

    /* Allocate state */
    LokiExampleState *state = calloc(1, sizeof(LokiExampleState));
    if (!state) return -1;
    ctx->example_state = state;

    /* Initialize your language runtime here */

    state->initialized = 1;
    return 0;
}

/* Cleanup resources */
static void example_cleanup(editor_ctx_t *ctx) {
    if (!ctx) return;

    LokiExampleState *state = get_example_state(ctx);
    if (!state) return;

    /* Cleanup your language runtime here */

    free(state);
    ctx->example_state = NULL;
}

/* Check if initialized */
static int example_is_initialized(editor_ctx_t *ctx) {
    LokiExampleState *state = get_example_state(ctx);
    return state ? state->initialized : 0;
}

/* Evaluate code */
static int example_eval(editor_ctx_t *ctx, const char *code) {
    LokiExampleState *state = get_example_state(ctx);
    if (!state || !state->initialized) {
        if (state) set_error(state, "Not initialized");
        return -1;
    }

    /* Evaluate the code using your language runtime */

    return 0;  /* 0 = success, -1 = error */
}

/* Stop playback */
static void example_stop(editor_ctx_t *ctx) {
    LokiExampleState *state = get_example_state(ctx);
    if (!state || !state->initialized) return;

    /* Stop any active playback */
}

/* Get last error message */
static const char *example_get_error(editor_ctx_t *ctx) {
    LokiExampleState *state = get_example_state(ctx);
    if (!state) return NULL;
    return state->last_error[0] ? state->last_error : NULL;
}
```

### Optional Functions

```c
/* Called in main loop for async callbacks (optional) */
static void example_check_callbacks(editor_ctx_t *ctx, lua_State *L) {
    /* Process any pending callbacks */
}

/* Check if currently playing (optional) */
static int example_is_playing(editor_ctx_t *ctx) {
    return 0;
}

/* Check if there are events for MIDI export (optional) */
static int example_has_events(editor_ctx_t *ctx) {
    return 0;
}

/* Populate shared buffer for MIDI export (optional) */
static int example_populate_shared_buffer(editor_ctx_t *ctx) {
    return -1;
}

/* Configure audio backend (optional) */
static int example_configure_backend(editor_ctx_t *ctx,
                                     const char *sf_path,
                                     const char *csd_path) {
    return 1;  /* 1 = not supported */
}
```

## Step 3: Register the Language

Create the `LokiLangOps` struct and register it:

```c
/* Language operations */
static const LokiLangOps example_lang_ops = {
    .name = "example",
    .extensions = {".ex", ".example", NULL},  /* Up to 4 extensions */

    /* Lifecycle */
    .init = example_init,
    .cleanup = example_cleanup,
    .is_initialized = example_is_initialized,

    /* Main loop (NULL if not needed) */
    .check_callbacks = NULL,

    /* Playback */
    .eval = example_eval,
    .stop = example_stop,
    .is_playing = NULL,

    /* Export (NULL if not supported) */
    .has_events = NULL,
    .populate_shared_buffer = NULL,

    /* Error */
    .get_error = example_get_error,

    /* Backend (NULL if not supported) */
    .configure_backend = NULL,

    /* Lua API (NULL if not provided) */
    .register_lua_api = NULL,
};

/* Auto-register at startup */
__attribute__((constructor))
static void example_register_language(void) {
    loki_lang_register(&example_lang_ops);
}
```

## Step 4: Add Editor Context State

Add your state pointer to `src/loki/internal.h`:

```c
typedef struct editor_ctx {
    /* ... existing fields ... */

    /* Language-specific state */
    struct LokiAldaState *alda_state;
    struct LokiJoyState *joy_state;
    struct LokiExampleState *example_state;  /* Add this */

    /* ... */
} editor_ctx_t;
```

## Step 5: Add Lua API Bindings (Optional)

If you want to expose your language to Lua scripts:

```c
/* ======================= Lua API Bindings ======================= */

static int lua_example_init(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);

    int result = example_init(ctx);
    if (result != 0) {
        lua_pushnil(L);
        const char *err = example_get_error(ctx);
        lua_pushstring(L, err ? err : "Failed to initialize");
        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}

static int lua_example_eval(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    const char *code = luaL_checkstring(L, 1);

    int result = example_eval(ctx, code);
    if (result != 0) {
        lua_pushnil(L);
        const char *err = example_get_error(ctx);
        lua_pushstring(L, err ? err : "Evaluation failed");
        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}

static int lua_example_stop(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    example_stop(ctx);
    return 0;
}

/* Register as loki.example subtable */
static void example_register_lua_api(lua_State *L) {
    /* Get loki global table */
    lua_getglobal(L, "loki");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    /* Create example subtable */
    lua_newtable(L);

    lua_pushcfunction(L, lua_example_init);
    lua_setfield(L, -2, "init");

    lua_pushcfunction(L, lua_example_eval);
    lua_setfield(L, -2, "eval");

    lua_pushcfunction(L, lua_example_stop);
    lua_setfield(L, -2, "stop");

    /* Add more functions as needed */

    lua_setfield(L, -2, "example");  /* Set as loki.example */
    lua_pop(L, 1);  /* Pop loki table */
}
```

Then update the `LokiLangOps` struct:

```c
static const LokiLangOps example_lang_ops = {
    /* ... */
    .register_lua_api = example_register_lua_api,
};
```

## Step 6: Update CMakeLists.txt

Add a CMake option for your language in `scripts/cmake/psnd_loki_library.cmake`:

```cmake
# Language-specific sources (in src/loki/lang/)
set(LOKI_LANG_SOURCES)
if(LANG_ALDA)
    list(APPEND LOKI_LANG_SOURCES ${PSND_ROOT_DIR}/src/loki/lang/alda.c)
endif()
if(LANG_JOY)
    list(APPEND LOKI_LANG_SOURCES ${PSND_ROOT_DIR}/src/loki/lang/joy.c)
endif()
if(LANG_EXAMPLE)
    list(APPEND LOKI_LANG_SOURCES ${PSND_ROOT_DIR}/src/loki/lang/example.c)
endif()

# ... later in the file ...

if(LANG_EXAMPLE)
    list(APPEND LOKI_PUBLIC_LIBS example)  # If you have a separate library
    target_compile_definitions(libloki PUBLIC LANG_EXAMPLE=1)
endif()
```

Also add the language option in the main `CMakeLists.txt`:

```cmake
option(LANG_EXAMPLE "Include the Example language" ON)
```

## Step 7: Add Syntax Highlighting (Optional)

Create a language definition in `.psnd/languages/example.lua`:

```lua
loki.register_language({
    name = "Example",
    extensions = {".ex", ".example"},
    keywords = {"play", "note", "rest", "tempo"},
    types = {"int", "float", "string"},
    line_comment = "//",
    block_comment_start = "/*",
    block_comment_end = "*/",
    string_delimiters = {'"', "'"},
})
```

## Step 8: Testing

Create tests in `tests/example/`:

```c
/* tests/example/test_example.c */
#include "../test_framework.h"

TEST(test_example_init) {
    /* Test initialization */
    ASSERT_TRUE(1);
}

TEST_SUITE(example_tests) {
    RUN_TEST(test_example_init);
}

int main(void) {
    RUN_SUITE(example_tests);
    return TEST_REPORT();
}
```

Add to `tests/example/CMakeLists.txt`:

```cmake
add_executable(test_example test_example.c ../test_framework.c)
target_link_libraries(test_example PRIVATE libloki)
add_test(NAME example_tests COMMAND test_example)
```

## Architecture Reference

### LokiLangOps Interface

```c
typedef struct LokiLangOps {
    /* Identification */
    const char *name;                              /* e.g., "example" */
    const char *extensions[LOKI_MAX_EXTENSIONS];   /* e.g., {".ex", NULL} */

    /* Lifecycle */
    int (*init)(editor_ctx_t *ctx);
    void (*cleanup)(editor_ctx_t *ctx);
    int (*is_initialized)(editor_ctx_t *ctx);

    /* Main loop integration */
    void (*check_callbacks)(editor_ctx_t *ctx, lua_State *L);

    /* Playback */
    int (*eval)(editor_ctx_t *ctx, const char *code);
    void (*stop)(editor_ctx_t *ctx);
    int (*is_playing)(editor_ctx_t *ctx);

    /* Export */
    int (*has_events)(editor_ctx_t *ctx);
    int (*populate_shared_buffer)(editor_ctx_t *ctx);

    /* Error handling */
    const char *(*get_error)(editor_ctx_t *ctx);

    /* Backend configuration */
    int (*configure_backend)(editor_ctx_t *ctx,
                            const char *sf_path,
                            const char *csd_path);

    /* Lua API */
    void (*register_lua_api)(lua_State *L);
} LokiLangOps;
```

### Key Files

| File | Purpose |
|------|---------|
| `src/loki/lang_bridge.h` | Language bridge interface definitions |
| `src/loki/lang_bridge.c` | Bridge implementation and dispatch |
| `src/loki/internal.h` | Editor context with language state pointers |
| `src/loki/lang/alda.c` | Reference implementation (Alda language) |
| `src/loki/lang/joy.c` | Reference implementation (Joy language) |

### Dispatch Functions

The bridge provides convenience functions for core editor code:

```c
/* Get language by file extension */
const LokiLangOps *loki_lang_for_file(const char *filename);

/* Get language by name */
const LokiLangOps *loki_lang_by_name(const char *name);

/* Initialize language for current file */
int loki_lang_init_for_file(editor_ctx_t *ctx);

/* Evaluate code with appropriate language */
int loki_lang_eval(editor_ctx_t *ctx, const char *code);

/* Stop all languages */
void loki_lang_stop_all(editor_ctx_t *ctx);

/* Check if any language is playing */
int loki_lang_is_playing(editor_ctx_t *ctx);

/* Register Lua APIs for all languages */
void loki_lang_register_lua_apis(lua_State *L);
```

## Example: Complete Minimal Language

Here's a complete minimal example that plays a beep:

```c
/* src/loki/lang/beep.c - Minimal beep language */

#include <stdlib.h>
#include <string.h>
#include "../internal.h"
#include "../lang_bridge.h"

typedef struct {
    int initialized;
    char error[64];
} BeepState;

static int beep_init(editor_ctx_t *ctx) {
    if (!ctx) return -1;
    if (ctx->beep_state) return 0;

    ctx->beep_state = calloc(1, sizeof(BeepState));
    if (!ctx->beep_state) return -1;

    ctx->beep_state->initialized = 1;
    return 0;
}

static void beep_cleanup(editor_ctx_t *ctx) {
    if (ctx && ctx->beep_state) {
        free(ctx->beep_state);
        ctx->beep_state = NULL;
    }
}

static int beep_is_initialized(editor_ctx_t *ctx) {
    return ctx && ctx->beep_state && ctx->beep_state->initialized;
}

static int beep_eval(editor_ctx_t *ctx, const char *code) {
    if (!beep_is_initialized(ctx)) return -1;
    /* Parse code and play beeps */
    printf("\a");  /* System beep */
    return 0;
}

static void beep_stop(editor_ctx_t *ctx) {
    (void)ctx;
}

static const char *beep_get_error(editor_ctx_t *ctx) {
    if (!ctx || !ctx->beep_state) return NULL;
    return ctx->beep_state->error[0] ? ctx->beep_state->error : NULL;
}

static const LokiLangOps beep_ops = {
    .name = "beep",
    .extensions = {".beep", NULL},
    .init = beep_init,
    .cleanup = beep_cleanup,
    .is_initialized = beep_is_initialized,
    .eval = beep_eval,
    .stop = beep_stop,
    .get_error = beep_get_error,
};

__attribute__((constructor))
static void beep_register(void) {
    loki_lang_register(&beep_ops);
}
```

## Tips

1. **Use existing languages as reference** - `src/loki/lang/alda.c` and `src/loki/lang/joy.c` are well-documented examples
2. **Keep state per-context** - Store language state in the editor context, not globals
3. **Handle errors gracefully** - Always set meaningful error messages
4. **Null-check everything** - Function pointers can be NULL, check before calling
5. **Use the shared backend** - For audio output, use the shared services in `src/shared/`
