# Psnd TODO

## High Priority

### Web Host Enhancements

The web host is functional with xterm.js terminal emulator. Remaining work:

- [ ] Multiple client support
  - Currently supports single WebSocket connection
  - Add connection management for concurrent clients

- [ ] Session persistence
  - Save/restore editor state across server restarts
  - Optional auto-save of open buffers

- [ ] Authentication
  - Add basic auth or token-based access for remote access
  - Required before exposing to network

### Architecture

- [ ] Centralize SharedContext ownership
  - Multiple `SharedContext` instances created (one per language) can conflict on singleton backends
  - Editor should own single context, languages share it
  - Location: `source/core/shared/context.c`

- [ ] Extract buffer manager to injectable service
  - Remove global `buffer_state` in `buffers.c`
  - Enables multi-editor and better testability

- [ ] Wrap editor core in standalone service process (optional)
  - Small RPC protocol (stdio JSON or gRPC)
  - Commands: load file, save, apply keystroke, get view state
  - Would enable embedding editor in other applications

## Medium Priority

### Testing

- [ ] Add synthesis backend tests
  - `csound_backend.c` - untested
  - `tsf_backend.c` - untested
  - Critical for verifying audio output

- [ ] Add scanner/lexer unit tests for all languages
  - Alda scanner vulnerable to malformed input
  - Joy/Bog/TR7 lexers untested

- [ ] Add fuzzing infrastructure
  - Parser robustness for malformed input
  - Consider AFL or libFuzzer integration

### Ableton Link Integration

- [ ] **Beat-Aligned Start** - Playback quantizes to Link beat grid
  - Use `shared_link_get_phase()` to wait for next beat/bar boundary before starting
  - Add launch quantization option (1 beat, 1 bar, etc.)
  - Align tick 0 with Link's beat grid so notes land on same beats as peers

- [ ] **Full Transport Sync** - Start/stop from any Link peer controls all
  - Wire transport callbacks to actually start/stop playback
  - Requires interruptible playback and a "armed for playback" state
  - Most complex - requires rethinking REPL interaction model

### Editor Features

- [ ] Playback visualization
  - Highlight currently playing region
  - Show playback progress in status bar

- [ ] MIDI port selection from editor
  - Currently only configurable via CLI

- [ ] Tempo tap
  - Tap key to set tempo

- [ ] Metronome toggle

### Refactoring

- [ ] Move to dynamic language registry (`src/lang_dispatch.h:12-14`)
  - Currently limited to 8 languages, 4 commands/extensions each (compiled-in limits)
  - New DSLs require recompilation and tuning macros
  - Makes plugin-style language packs or user modules impossible
  - Fix: Consider dynamically-sized registry backed by Lua extension system

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
  - Couples tests to `/bin/sh`, ignores exit codes in some branches
  - Fix: Use `fork`/`execve` directly for binary invocation, `mkdtemp`/`nftw` for temp directory cleanup

- [ ] Add missing test coverage
  - No tests for: editor bridge, Ableton Link callbacks, shared REPL command processor
  - Pointer/string comparisons can silently truncate in test framework

- [ ] Add `ASSERT_GT`, `ASSERT_LT` macros

- [ ] Add test fixture support (setup/teardown)

- [ ] Add memory leak detection hooks

---

## Low Priority

### Code Consolidation

- [ ] Consolidate dispatch boilerplate
  - 4 files x 26 lines = 104 lines of identical structure
  - Consider macro template in `lang_dispatch.h`

- [ ] Extract shared REPL loop skeleton
  - ~150 lines of help functions still duplicated per language
  - Interactive loop structure still duplicated (could use callback pattern)

- [ ] Centralize platform CMake logic
  - Platform detection repeated in 6+ CMakeLists.txt files
  - Create `psnd_platform.cmake` module

- [ ] Remove Joy music notation duplication
  - 405 lines duplicated from Alda's music theory code
  - Extract to shared module

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

---

## Recently Completed

### Web Editor Implementation (Phases 1-7)
- Eliminated global state and singletons
- Split model from view in `editor_ctx_t`
- Abstracted input handling with `EditorEvent` struct
- Introduced renderer interface with `EditorViewModel`
- Created host-agnostic `EditorSession` API
- Added event queue for async tasks
- Built web front-end with xterm.js terminal emulator
- Embedded xterm.js in binary (optional, via `LOKI_EMBED_XTERM`)
- Added mouse click-to-position support
- Added language switching commands (`:alda`, `:joy`, `:langs`)
- Added first-line directive support (`#alda`, `#joy`)

### Other Completed Items
- Standardized error return conventions
- Unified Lua binding pattern across languages
- Extracted shared REPL helper utilities
- Implemented `:play` command with file-type dispatch
- Wired Ableton Link callbacks for tempo sync
- Added TR7 test suite (38 tests)
