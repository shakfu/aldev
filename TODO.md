# Psnd TODO

## High Priority

### Architecture (Multi-Context Support)

- [ ] Refactor Joy/TR7 to eliminate global SharedContext (`src/lang/joy/midi/joy_midi_backend.c:29`, `src/lang/tr7/repl.c`)
  - Both modules use static globals (`g_shared`, `g_tr7_repl_shared`)
  - Running two Joy/TR7 sessions simultaneously (multiple buffers in loki or separate REPLs) stomps the same context
  - Cleanup routines (`joy_csound_cleanup()`) can shut down backend even if another language/editor context is active
  - Fix: Each editor/REPL instance should own its own `SharedContext` (or a handle to ref-counted pool)

---

## Medium Priority

### Feature Completeness

- [ ] Implement `:cs-play` command or remove from help (`src/shared/repl_commands.c:301-312`)
  - Help text advertises `:cs-play PATH` but handler prints "not yet implemented"
  - Users enabling Csound backend hit dead-end workflow
  - Fix: Wire command into `shared_csound_play_file()` or hide until supported

- [ ] Wire Ableton Link callbacks (`src/shared/link/link.c`)
  - Link is implemented as singleton, callers must poll `shared_link_check_callbacks` manually
  - No REPL or editor currently calls this, so tempo/peer callbacks are effectively dead
  - Fix: Add polling in REPL/editor main loops or use background thread

### Refactoring

- [ ] Move to dynamic language registry (`src/lang_dispatch.h:12-14`)
  - Currently limited to 8 languages, 4 commands/extensions each (compiled-in limits)
  - New DSLs require recompilation and tuning macros
  - Makes plugin-style language packs or user modules impossible
  - Fix: Consider dynamically-sized registry backed by Lua extension system (see `docs/language-extension-api.md`)

### Editor Features

- [ ] Playback visualization
  - Highlight currently playing region
  - Show playback progress in status bar

- [ ] MIDI port selection from editor
  - Currently only configurable via CLI

- [ ] Tempo tap
  - Tap key to set tempo

- [ ] Metronome toggle

### Build System

- [ ] Add AddressSanitizer build target
  - `option(PSND_ENABLE_ASAN "Enable AddressSanitizer" OFF)`

- [ ] Add code coverage target
  - `option(PSND_ENABLE_COVERAGE "Enable code coverage" OFF)`

- [ ] Add install target
  - Currently no `make install` support

### Test Framework

- [ ] Refactor CLI tests to avoid shell spawning (`tests/cli/test_play_command.c:29-62`)
  - Tests use `system("rm -rf ...")` for cleanup and `system()` to invoke psnd
  - Couples tests to `/bin/sh`, ignores exit codes in some branches, vulnerable to whitespace in paths
  - Fix: Use `fork`/`execve` directly for binary invocation, `mkdtemp`/`nftw` for temp directory cleanup

- [ ] Add missing test coverage
  - No tests for: editor bridge, Ableton Link callbacks, shared REPL command processor
  - Test framework only exposes `ASSERT_EQ/NEQ/TRUE/FALSE` with integer formatting
  - Pointer/string comparisons can silently truncate (`tests/test_framework.h`)

- [ ] Add `ASSERT_GT`, `ASSERT_LT` macros

- [ ] Add test fixture support (setup/teardown)

- [ ] Add memory leak detection hooks

---

## Low Priority

### Platform Support

- [ ] Windows support
  - Editor uses POSIX headers: `termios.h`, `unistd.h`, `pthread.h`
  - Options: Native Windows console API, or web editor using CodeMirror/WebSockets

### Editor Features

- [ ] Split windows
  - Already designed for in `editor_ctx_t`
  - Requires screen rendering changes

- [ ] Tree-sitter integration
  - Would improve syntax highlighting, code handling
  - Significantly more complex than current system

- [ ] LSP client integration
  - Would provide IDE-like features
  - High complexity undertaking

- [ ] Git integration
  - Gutter diff markers
  - Stage/commit commands

### Documentation

- [ ] Architecture diagram
  - Visual overview of module relationships

- [ ] API reference generation
  - Consider Doxygen for generated docs

- [ ] Contributing guide

- [ ] Build troubleshooting
  - Platform-specific guidance

### Future Architecture

- [ ] Plugin architecture for language modules
  - Dynamic loading of language support

- [ ] JACK backend
  - For pro audio workflows

- [ ] Decouple from the loki editor and use a webserver (linky mongoose)

- [ ] Provide a minimal language example

---

## Feature Opportunities

### Preset Browser & Layering
- [ ] Add preset browsing UI to editor/REPL
  - TSF already exposes preset metadata via `shared_tsf_get_preset_name()`
  - No UI for browsing, tagging, or layering presets
  - Let musicians audition instruments and build splits/stacks without editing raw program numbers

### Session Capture & Arrangement
- [ ] Elevate shared MIDI event buffer to first-class timeline (`src/shared/midi/events.h`)
  - Currently only feeds export
  - Capture REPL improvisations into clips, arrange them, re-trigger live
  - Similar to Ableton's Session View but text-driven

### Controller & Automation Mapping
- [ ] Map physical MIDI controllers or OSC sources to language variables
  - Tempo, volume, macro parameters
  - Makes Joy/TR7 live-coding sets more expressive
  - Combine with existing Ableton Link transport hooks

### Cross-Language Patch Sharing
- [ ] Create lightweight messaging bus for Alda, Joy, TR7 to exchange motifs
  - Example: Joy macro emits motif that Alda editor picks up and renders with full notation
  - Showcases polyglot nature, keeps multiple buffers in sync

### Real-Time Visualization
- [ ] Expose playback state in loki status bar or via OSC/WebSocket
  - Current measure, active voices, CPU load
  - Visual confirmation when multiple asynchronous schedulers are active
