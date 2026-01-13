# Aldalog TODO

## High Priority

- [ ] Fix line numbers display (not working)
  - `loki.line_numbers(true)` in init.lua doesn't show line numbers
  - Investigate `src/loki_core.c` line number rendering

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

