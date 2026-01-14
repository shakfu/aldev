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


## Current

### Extend the Commandline API

The current commandline api is as follows:

```sh
aldalog                           # Start REPL
aldalog -sf gm.sf2                # REPL with built-in synth
# editor mode
aldalog song.alda                       # Open file in editor
aldalog -sf gm.sf2 song.alda            # Editor with TinySoundFont synth
aldalog -cs instruments.csd song.alda   # Editor with Csound synthesis
# play mode (headless)
aldalog play song.alda                # Play file and exit
aldalog play -sf gm.sf2 song.alda     # Play with built-in synth
```

I would like to add the following to make the recent csound integration more usable:

```sh
# editor mode
aldalog song.csd                        # Open csound file in editor
# play mode (headless)
aldalog play song.csd                 # Play file and exit
```

### Consistent Code Highlighting

The repl and the editor have two different styles, and the repl doesn't follow the current theme.

### Windows

The editor uses `termios.h`, `unistd.h`, and `pthread.h` -- all POSIX headers which are directly supported by Windows.

Everything else can work with Windows. So what the alternative solution?

- Windows console code?
- Webeditor using codemirror / websockets?



