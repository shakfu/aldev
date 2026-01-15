# Psnd TODO

## High Priority

### Testing Gaps

- [ ] Add Alda parser tests
  - Parser handles 27 AST node types
  - Each node type should have dedicated tests
  - Currently only `tests/alda/test_shared_suite.c` exists

- [x] Add Alda interpreter tests
  - Test MIDI event generation (tempo, volume, polyphony, markers, variables)
  - This is the core value proposition - needs coverage

- [ ] Add audio backend tests
  - Smoke tests for MIDI output, TinySoundFont, Csound
  - Would catch regressions in playback

- [ ] Add integration tests
  - End-to-end: parse Alda code and verify MIDI output

### Code Quality

- [ ] Improve parser error recovery
  - More robust error recovery would improve REPL experience
  - Better error messages with context

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

- [ ] Move Alda state to context
  - Refactor `g_alda_state` in `loki_alda.c` to be per-context
  - Enables multiple independent Alda sessions
  - Currently prevents running multiple Alda instances

- [ ] Extract magic numbers to constants
  - Tempo bounds `20` and `400` in `loki_alda.c` should be named constants
  - Other hardcoded limits (64 parts, 16384 events, 256 variables)

### Microtuning

- [ ] Integrate Scala scales with Alda pitch calculation
  - Modify interpreter to use loaded scale for MIDI-to-frequency conversion
  - Currently `loki.scala.midi_to_freq()` is available in Lua but not wired into playback
  - Requires changes to `src/alda/interpreter.c` pitch calculation
  - Consider per-part scale assignment (different instruments in different tunings)

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
