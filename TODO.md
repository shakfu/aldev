# Psnd TODO

## High Priority

- [x] Fix line numbers display (not working)
  - `loki.line_numbers(true)` in init.lua doesn't show line numbers
  - Root cause: `buffers_init()` and `buffer_create()` didn't copy `line_numbers` field
  - Fixed in `src/loki/buffers.c`

## Medium Priority

- [x] Add support for Ableton Link

- [x] Add support for midifile

## Low Priority

## Completed

- [x] Lua-based keybinding customization system
  - Added `loki.keymap(modes, key, callback, [description])` API
  - Added `loki.keyunmap(modes, key)` API
  - Supports modes: 'n' (normal), 'i' (insert), 'v' (visual), 'c' (command)
  - Key notation: single chars ('a'), control keys ('<C-a>'), special keys ('<Enter>', '<Esc>', etc.)
  - Lua callbacks are checked before built-in handlers in each mode

- [x] Fix `loki.get_cursor()` returning wrong position
  - Was returning screen position (`ctx->cy`) instead of file position
  - Fixed to return `ctx->rowoff + ctx->cy` (accounts for scroll offset)

- [x] Fix Lua API using stale editor context with multiple buffers
  - `lua_get_editor_context()` was returning a pointer stored at init time
  - Fixed to call `buffer_get_current()` dynamically


## Current

### Consistent Code Highlighting

The repl and the editor have two different styles, and the repl doesn't follow the current theme.

### Windows

The editor uses `termios.h`, `unistd.h`, and `pthread.h` -- all POSIX headers which are directly supported by Windows.

Everything else can work with Windows. So what the alternative solution?

- Windows console code?
- Webeditor using codemirror / websockets?



