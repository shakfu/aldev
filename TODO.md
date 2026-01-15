# Psnd TODO

## High Priority

### Testing Gaps

- [x] Add Alda parser tests
  - Parser handles 27 AST node types
  - Each node type should have dedicated tests
  - `tests/alda/test_parser.c` - 71 tests covering all node types

- [x] Add Alda interpreter tests
  - Test MIDI event generation (tempo, volume, polyphony, markers, variables)
  - This is the core value proposition - needs coverage

- [x] Add audio backend tests
  - Smoke tests for MIDI output, TinySoundFont, Csound
  - `tests/alda/test_backends.c` - 20 smoke tests

- [x] Add integration tests
  - End-to-end: parse Alda code and verify MIDI output
  - `tests/alda/test_integration.c` - 14 integration tests

### Code Quality

- [x] Improve parser error recovery
  - Added synchronization points for error recovery (newlines, delimiters, part declarations)
  - Enhanced error messages with context hints (e.g., "in S-expression", "in cram expression")
  - Added expected/found information to error messages
  - Implemented multiple error collection (up to 10 errors)
  - New APIs: `alda_parser_error_count()`, `alda_parser_all_errors_string()`

---

## Medium Priority

### Editor Features

- [ ] Go-to-line command
  - Add `:123` or `:goto 123` command
  - Simple addition to command mode

- [ ] Search and replace
  - Extend search with `:s/old/new/` command

- [ ] Alda playback visualization
  - Highlight currently playing region
  - Show playback progress in status bar

### Architecture

- [x] Move Alda state to context
  - Refactored `g_alda_state` in `loki_alda.c` to be per-context
  - `LokiAldaState` is now allocated per `editor_ctx_t`
  - Enables multiple independent Alda sessions

- [x] Extract magic numbers to constants
  - Added `LOKI_ALDA_TEMPO_MIN`, `LOKI_ALDA_TEMPO_MAX`, `LOKI_ALDA_TEMPO_DEFAULT` to `alda.h`
  - Added `LOKI_ALDA_ERROR_BUFSIZE` to `alda.h`
  - Alda interpreter limits already defined in `alda/context.h`: `ALDA_MAX_PARTS`, `ALDA_MAX_EVENTS`, `ALDA_MAX_MARKERS`, `ALDA_MAX_VARIABLES`

### Microtuning

- [x] Integrate Scala scales with Alda pitch calculation
  - Added per-part scale fields to `AldaPartState`: `scale`, `scale_root_note`, `scale_root_freq`
  - Modified Csound backend to send frequency (via `alda_csound_send_note_on_freq`)
  - Pitch conversion happens at dispatch in `midi_backend.c` and `async.c`
  - Lua API: `loki.alda.set_part_scale(part_name, [root_note], [root_freq])`
  - Lua API: `loki.alda.clear_part_scale(part_name)` - return to 12-TET
  - Per-part scale assignment enables different instruments in different tunings

### Polyglot Platform

- [ ] Integrate first midi-langs DSL
  - Implement first additional language from midi-langs project
  - Proves polyglot architecture works

- [ ] Document extension API
  - Guide for adding third-party languages
  - Language registration API documentation

---

## Low Priority

### Platform Support

- [ ] Windows support
  - Editor uses POSIX headers: `termios.h`, `unistd.h`, `pthread.h`
  - Options:
    - Native Windows console API
    - Web editor using CodeMirror / WebSockets

### Editor Features

- [ ] Split windows
  - Already designed for in `editor_ctx_t`
  - Requires screen rendering changes

- [ ] LSP client integration
  - Would provide IDE-like features
  - High complexity undertaking

- [ ] Git integration
  - Gutter diff markers
  - Stage/commit commands

- [ ] MIDI port selection from editor
  - Currently only configurable via CLI

- [ ] Tempo tap
  - Tap key to set tempo

- [ ] Metronome toggle

### Build System

- [ ] Add AddressSanitizer build target
  ```cmake
  option(PSND_ENABLE_ASAN "Enable AddressSanitizer" OFF)
  ```

- [ ] Add code coverage target
  ```cmake
  option(PSND_ENABLE_COVERAGE "Enable code coverage" OFF)
  ```

- [ ] Add install target
  - Currently no `make install` support

### Documentation

- [ ] Architecture diagram
  - Visual overview of module relationships

- [ ] API reference generation
  - Consider Doxygen for generated docs

- [ ] Contributing guide
  - Contribution guidelines for external contributors

- [ ] Build troubleshooting
  - Platform-specific guidance

### Test Framework

- [ ] Add `ASSERT_GT`, `ASSERT_LT` macros

- [ ] Add test fixture support (setup/teardown)

- [ ] Add memory leak detection hooks

### Future Architecture

- [ ] Plugin architecture for language modules
  - Dynamic loading of language support

- [ ] JACK backend
  - For pro audio workflows

---

## Completed

- [x] Unify REPL and editor syntax highlighting
  - REPL now loads Lua themes from `.psnd/init.lua`
  - Both use same `editor_ctx_t` colors array
  - Refactored `repl.c` to thread context through rendering

- [x] Fix line numbers display
  - `loki.line_numbers(true)` in init.lua wasn't working
  - Root cause: `buffers_init()` and `buffer_create()` didn't copy `line_numbers` field
  - Fixed in `src/loki/buffers.c`

- [x] Add Ableton Link support
  - Tempo synchronization with DAWs and peers

- [x] Add MIDI file export
  - Export to Standard MIDI Files via midifile library

- [x] Lua-based keybinding customization
  - `loki.keymap()` and `loki.keyunmap()` APIs
  - Supports all modes: normal, insert, visual, command

- [x] Fix `loki.get_cursor()` returning wrong position
  - Was returning screen position instead of file position
  - Fixed to account for scroll offset

- [x] Fix Lua API stale context with multiple buffers
  - Now dynamically calls `buffer_get_current()`

- [x] Visual mode delete
  - Core vim functionality

- [x] System clipboard integration
  - OSC 52 escape sequences for terminal clipboard

- [x] Add undo/redo tests
  - `tests/loki/test_undo.c`
