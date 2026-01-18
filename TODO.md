# Psnd TODO

## High Priority

### Web Editor Decoupling (validated from DESIGN_REVIEW.md)

The following tasks enable replacing the terminal editor with a web-based editor.
Order reflects dependency chain - earlier tasks unblock later ones.

```text
┌───────────────────────┬────────────┬───────────┬──────────────┬───────────┐
│         Phase         │ Complexity │  Effort   │ Dependencies │   Risk    │
├───────────────────────┼────────────┼───────────┼──────────────┼───────────┤
│ 3: Input Abstraction  │ Medium     │ 2-3 days  │ None         │ Low       │
├───────────────────────┼────────────┼───────────┼──────────────┼───────────┤
│ 4: Renderer Interface │ High       │ 4-6 days  │ None         │ Medium    │
├───────────────────────┼────────────┼───────────┼──────────────┼───────────┤
│ 5: Session API        │ Medium     │ 2-3 days  │ 3, 4         │ Low       │
├───────────────────────┼────────────┼───────────┼──────────────┼───────────┤
│ 6: RPC Service        │ High       │ 5-8 days  │ 5            │ High      │
├───────────────────────┼────────────┼───────────┼──────────────┼───────────┤
│ 7: Web Front-End      │ Very High  │ 2-4 weeks │ 6            │ Very High │
└───────────────────────┴────────────┴───────────┴──────────────┴───────────┘
```

#### Phase 1: Eliminate Global State & Singletons

- [x] Remove global `signal_context` in `terminal.c:28`
  - Created `TerminalHost` struct in `terminal.h` with `orig_termios`, `winsize_changed`, `rawmode`, `fd`
  - Moved signal handler to use `g_terminal_host->winsize_changed`
  - Removed `rawmode` and `winsize_changed` from `editor_ctx_t`
  - Legacy wrappers maintain API compatibility

- [x] Make `editor.c` static `E` into parameter
  - Changed `static editor_ctx_t E` to local variable in `loki_editor_main()`
  - Added `editor_set_atexit_context()` to explicitly set cleanup context
  - After `buffers_init()`, atexit context points to buffer manager's context

- [x] Decouple buffer manager from terminal state (`buffers.c:98-104`)
  - Removed `rawmode` copying between buffers (now in TerminalHost)
  - Still copies `screencols`, `screenrows`, Lua state (display metrics shared across buffers)

#### Phase 2: Split Model from View in `editor_ctx_t`

- [x] Factor `editor_ctx_t` into `EditorModel` + `ViewAdapter`
  - Model: buffers, syntax, selections, language handles, cursor logical position
  - ViewAdapter: terminal metrics, theme colors, REPL layout
  - See `internal.h:163-219` for current mixed struct

- [x] Provide serialization helpers for the model
  - Enables snapshot/restore, RPC transport, test fixtures

#### Phase 3: Abstract Input Handling

- [ ] Define `EditorEvent` struct for keystrokes, commands, actions
  - Replace raw key codes with structured events
  - Converter from terminal escape sequences to events in CLI build
  - Other transports (WebSocket, tests) inject their own event stream

- [ ] Decouple `modal_process_keypress()` from file descriptor (`modal.c:820-825`)
  - Current signature: `void modal_process_keypress(editor_ctx_t *ctx, int fd)`
  - New approach: `void editor_handle_event(EditorSession*, const EditorEvent*)`

#### Phase 4: Introduce Renderer Interface

- [ ] Replace `editor_refresh_screen()` with renderer callbacks
  - Current code emits VT100 directly (`core.c:544-782`)
  - Define interface: emit rows, gutter, status segments, REPL panes
  - Terminal renderer translates to VT100
  - Web renderer serializes JSON diff to client

- [ ] Route REPL output through renderer abstraction
  - `lua.c:1675-1709` currently emits terminal escapes directly
  - Store REPL logs as plain strings/events, let front-end paint them

- [ ] Abstract OSC-52 clipboard (`selection.c:94-100`)
  - Guard behind `TerminalAdapter` so web host never links against it
  - Web host uses browser clipboard API instead

#### Phase 5: Host-Agnostic Session API

- [ ] Create `EditorSession` opaque handle
  - `editor_session_new(const EditorConfig*)` - create session
  - `editor_session_handle_event(session, const EditorEvent*)` - process input
  - `editor_session_snapshot(session, EditorViewModel*)` - get render state
  - Terminal mode becomes one consumer; web server another

- [ ] Extract CLI parsing + terminal orchestration into thin wrapper
  - Separate from session logic
  - Enables alternate hosts (HTTP server, headless scripting harness)

#### Phase 6: Service Process & RPC

- [ ] Wrap editor core in service process
  - Small RPC protocol (stdio JSON or gRPC)
  - Commands: load file, save, apply keystroke, get view state
  - Terminal binary can talk to it locally (proves abstraction)

- [ ] Add event queue for async tasks
  - Leverage libuv (already a playback dependency)
  - Language callbacks, timers, UI events scheduled without blocking render
  - Web host drives session via async RPC instead of `while(1)` loop

#### Phase 7: Web Front-End

- [ ] Build browser front-end connecting to RPC service
  - Send high-level editor events
  - Render the diff tree (visible rows, selections, status bars)
  - Reuse existing shared audio/MIDI context for playback commands

- [ ] Retire terminal assumptions from shared modules
  - Guard OSC-52, SIGWINCH, alternate screen behind terminal adapter
  - Browser host never links against termios/signal code

## Medium Priority

### Code Quality (from REVIEW.md Phase 1-2)

#### Error Handling
- [x] Standardize error return conventions across critical modules
  - Fixed `lang_bridge.c`: Changed tri-state (0/1/-1) to binary (0=success, -1=error)
  - Fixed `core.c`: Changed `editor_open()` and `editor_save()` from 1=error to -1=error
  - Remaining: `command.c` and `undo.c` use boolean (1=success) consistently - documented as intentional

#### Architecture
- [ ] Centralize SharedContext ownership
  - Multiple `SharedContext` instances created (one per language) can conflict on singleton backends
  - Editor should own single context, languages share it
  - Location: `source/core/shared/context.c`

- [ ] Extract buffer manager to injectable service
  - Remove global `buffer_state` in `buffers.c`
  - Enables multi-editor and better testability

- [ ] Add explicit language selection API
  - Currently uses filename extension lookup only
  - Add `:setlang <name>` command for override
  - Location: `source/core/loki/lang_bridge.c`

### Testing (from REVIEW.md Phase 3)

- [ ] Add synthesis backend tests
  - `csound_backend.c` - untested
  - `tsf_backend.c` - untested
  - Critical for verifying audio output

- [ ] Add scanner/lexer unit tests for all languages
  - Alda scanner vulnerable to malformed input
  - Joy/Bog/TR7 lexers untested

- [x] Add TR7 test suite
  - Added `source/langs/tr7/tests/test_music.c` with 38 tests covering:
    - Engine creation and basic evaluation
    - Scheme arithmetic and list operations
    - MIDI value clamping (velocity, pitch, channel)
    - Duration calculation from tempo
    - Note name to MIDI pitch conversion
    - State management defaults and ranges

- [ ] Add fuzzing infrastructure
  - Parser robustness for malformed input
  - Consider AFL or libFuzzer integration

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

### Code Consolidation (from REVIEW.md Phase 4)

- [x] Unify Lua binding pattern across languages
  - Added `loki_lua_begin_api()`, `loki_lua_add_func()`, `loki_lua_end_api()` helpers
  - Reduced boilerplate from ~80 lines to ~20 lines per language
  - Location: `source/core/loki/lua.c`, `source/core/include/loki/lua.h`

- [ ] Consolidate dispatch boilerplate
  - 4 files x 26 lines = 104 lines of identical structure
  - Consider macro template in `lang_dispatch.h`

- [x] Extract shared REPL helper utilities
  - Added `source/core/loki/repl_helpers.h` with:
    - `repl_starts_with()` - string prefix check
    - `repl_strip_newlines()` - strip trailing newlines
    - `repl_get_history_path()` - determine history file path (~15 lines of duplication removed per REPL)
    - `repl_pipe_loop()` - generic piped input loop helper
  - All four language REPLs (Alda, Joy, Bog, TR7) updated to use helpers
  - Estimated ~80 lines of duplication removed across REPLs

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
