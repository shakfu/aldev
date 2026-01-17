# Psnd TODO

## Medium Priority

### Feature Completeness

- [x] Implement `:play` command with file-type dispatch
  - Replaced `:cs-play` with generic `:play PATH` command
  - Dispatches based on file extension: `.csd` -> Csound, `.alda`/`.joy`/`.scm` -> respective REPL
  - Each REPL handles its own file type (Alda, Joy, TR7)

- [x] Wire Ableton Link callbacks (`src/shared/link/link.c`)
  - Added `shared_repl_link_init_callbacks()`, `shared_repl_link_check()`, `shared_repl_link_cleanup_callbacks()` to `src/shared/repl_commands.c`
  - All REPLs (Joy, Alda, TR7) now poll `shared_repl_link_check()` after each command
  - Callbacks print `[Link] Tempo: N BPM`, `[Link] Peers: N`, `[Link] Transport: playing/stopped` to stdout
  - Editor already had Link polling via `loki_link_check_callbacks()` in `src/loki/editor.c`

### Ableton Link Integration

Three levels of Link synchronization with other Link-enabled devices:

- [x] **Level 1: Tempo Sync** - All devices play at the same BPM
  - Alda: Uses `shared_link_effective_tempo()` when setting up tick-based schedule
  - Joy: Scales ms timings by `local_tempo / link_tempo` ratio at playback time
  - TR7: Scales ms timings by `local_tempo / link_tempo` ratio at playback time
  - When Link is enabled, playback matches Link session tempo

- [ ] **Level 2: Beat-Aligned Start** - Playback quantizes to Link beat grid
  - Use `shared_link_get_phase()` to wait for next beat/bar boundary before starting
  - Add launch quantization option (1 beat, 1 bar, etc.)
  - Align tick 0 with Link's beat grid so notes land on same beats as peers

- [ ] **Level 3: Full Transport Sync** - Start/stop from any Link peer controls all
  - Wire transport callbacks to actually start/stop playback
  - Requires interruptible playback and a "armed for playback" state
  - Most complex - requires rethinking REPL interaction model

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
