# CHANGELOG

All notable project-wide changes will be documented in this file. Note that each subproject has its own CHANGELOG.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/) and [Commons Changelog](https://common-changelog.org). This project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## Types of Changes

- Added: for new features.
- Changed: for changes in existing functionality.
- Deprecated: for soon-to-be removed features.
- Removed: for now removed features.
- Fixed: for any bug fixes.
- Security: in case of vulnerabilities.

---

## [Unreleased]

### Added

- **REPL Language Switching**: Switch between language REPLs without exiting
  - `:lang NAME` - Switch to another language REPL (e.g., `:lang joy`, `:lang alda`)
  - `:langs` - List available languages
  - Uses `exec` to restart the process with the new language, preserving terminal state
  - Available in all language REPLs (Alda, Joy, TR7, Bog)
  - **Files Modified**: `source/core/shared/repl_commands.c`

- **Ableton Link Beat-Aligned Start**: Playback can quantize to the Link beat grid
  - `loki.link.launch_quantize(quantum)` - Set launch quantization (0=immediate, 1=beat, 4=bar)
  - When Link is enabled and launch_quantize > 0, playback waits for the next beat boundary
  - Ensures notes land on the same beats as other Link peers
  - Works with both Alda and Joy async playback
  - **New API**: `shared_link_ms_to_next_beat(quantum)` - Calculate ms until next boundary
  - **Files Modified**:
    - `source/core/shared/link/link.c`, `source/core/shared/link/link.h`
    - `source/core/shared/async/shared_async.c`, `source/core/shared/async/shared_async.h`
    - `source/core/shared/context.h`
    - `source/core/loki/lua.c`
    - `source/langs/alda/async.c`
    - `source/langs/joy/midi/joy_async.c`

- **Lua Sandbox Mode**: Lua scripting is sandboxed by default for security
  - Disables dangerous libraries: `os` (shell execution), `io` (file access), `debug`
  - Removes `load`, `loadfile`, `dofile` from base library
  - Keeps safe libraries: `table`, `string`, `math`, `utf8`, `coroutine`, `package`
  - Protects against malicious `.psnd/init.lua` in cloned repositories
  - Compile-time option: `-DLUA_SANDBOX=ON` (default) or `-DLUA_SANDBOX=OFF` for full access
  - **Files Modified**:
    - `CMakeLists.txt` - Added `LUA_SANDBOX` option
    - `source/core/CMakeLists.txt` - Passes define to libloki
    - `source/core/loki/lua.c` - Conditional library loading
    - `.psnd/init.lua` - Updated debug logging to handle sandbox

- **Ableton Link Configuration in init.lua**: Added commented configuration options
  - All Link options documented with descriptions (init, enable, tempo, launch_quantize, etc.)
  - Callback registration examples for peers, tempo, and transport changes
  - **Files Modified**: `.psnd/init.lua`

### Changed

- **Unified MIDI Port Name**: All languages now use `PSND_MIDI` as the default virtual MIDI port name
  - Previously each language used its own port name (Alda, Loki, JoyMIDI, TR7MIDI, BogMIDI, etc.)
  - Now all languages share `PSND_MIDI_PORT_NAME` ("PSND_MIDI") for consistency
  - Simplifies DAW setup: connect once, works with all languages
  - Language switching via `:lang` preserves MIDI connection
  - **Files Modified**:
    - `source/langs/alda/repl.c`, `source/langs/alda/register.c`
    - `source/langs/joy/repl.c`, `source/langs/joy/register.c`
    - `source/langs/joy/midi/joy_midi_backend.c`, `source/langs/joy/midi/midi_primitives.c`
    - `source/langs/tr7/impl/repl.c`, `source/langs/tr7/impl/register.c`
    - `source/langs/bog/repl.c`

- **Centralized SharedContext Ownership**: Single SharedContext instance shared across all languages in editor mode
  - Previously each language (Alda, Joy, TR7, Bog) allocated its own SharedContext, causing conflicts on singleton backends (TSF, Csound, Link)
  - `EditorModel` now owns a single `SharedContext*` that all languages share
  - Editor creates SharedContext in `loki_editor_main()` before language initialization
  - Editor cleans up SharedContext in `editor_cleanup_resources()` after language cleanup
  - Each language's `register.c` now uses `ctx->model.shared` instead of allocating its own
  - REPL mode unchanged: each REPL process still owns its own SharedContext (appropriate for standalone processes)
  - Eliminates undefined behavior when switching between language buffers
  - Prevents inconsistent audio routing based on buffer/language initialization order
  - **Files Modified**:
    - `source/core/loki/internal.h` - Added `SharedContext *shared` to `EditorModel`
    - `source/core/loki/editor.c` - SharedContext creation and cleanup
    - `source/langs/alda/impl/context.c` - Removed SharedContext allocation/cleanup
    - `source/langs/alda/register.c` - Use `ctx->model.shared`
    - `source/langs/alda/repl.c` - Create REPL-owned SharedContext
    - `source/langs/joy/register.c` - Use `ctx->model.shared`
    - `source/langs/tr7/impl/register.c` - Use `ctx->model.shared`
    - `source/langs/bog/register.c` - Use `ctx->model.shared`

## [0.1.3]

### Added

- **Renderer Interface**: Abstract rendering layer for platform-agnostic output
  - Decouples editor logic from terminal-specific VT100 escape codes
  - Enables alternative frontends (web, GUI, tests) via renderer callbacks
  - `Renderer` interface with callbacks for:
    - Frame management (`begin_frame`, `end_frame`)
    - Content rendering (`render_tabs`, `render_row`, `render_status`, `render_message`, `render_repl`)
    - Cursor management (`set_cursor`, `show_cursor`, `hide_cursor`)
    - Clipboard operations (`clipboard_copy`)
  - Built-in renderer implementations:
    - `terminal_renderer_create()` - VT100 terminal output
    - `null_renderer_create()` - Discards output (for testing/headless)
  - Structured render data types:
    - `RenderSegment` - Text spans with highlight type and selection state
    - `StatusInfo` - Status bar information (mode, filename, position)
    - `ReplInfo` - REPL pane state (prompt, input, log lines)
    - `HighlightType` - Abstract highlight categories (comment, keyword, string, etc.)
  - `editor_refresh_screen()` now delegates to renderer when available
    - Uses `build_render_segments()` to convert row content to segments
    - Falls back to legacy VT100 code path when no renderer is set
  - OSC-52 clipboard abstracted behind renderer interface
    - `copy_selection_to_clipboard()` uses renderer if available
    - Falls back to direct terminal output for backwards compatibility
  - `editor_ctx_set_renderer()` to set/replace context renderer
  - `buffers_get_tab_info()` / `buffers_free_tab_info()` for tab rendering abstraction
  - **Files Added**: `source/core/loki/renderer.h`, `source/core/loki/renderer.c`
  - **Files Modified**: `source/core/loki/internal.h`, `source/core/loki/core.c`, `source/core/loki/selection.c`, `source/core/loki/buffers.h`, `source/core/loki/buffers.c`, `source/core/CMakeLists.txt`

- **EditorSession API**: Opaque handle for embedding the editor
  - Clean, self-contained API that hides all internal implementation details
  - `EditorSession` opaque handle encapsulates editor state
  - Session lifecycle:
    - `editor_session_new(const EditorConfig*)` - Create session with configuration
    - `editor_session_free(session)` - Free session and resources
  - Event handling:
    - `editor_session_handle_event(session, const EditorEvent*)` - Process input
    - Returns 0 on success, 1 if editor should quit, -1 on error
  - View model (render state snapshot):
    - `editor_session_snapshot(session)` - Get deep copy of render state
    - `editor_viewmodel_free(vm)` - Free view model
    - `EditorViewModel` contains all data for rendering:
      - `EditorRowView` array with segments and owned text
      - `EditorCursor` with screen and file positions
      - `EditorTabInfo` for tab bar
      - `StatusInfo`, `ReplInfo` with owned string copies
  - Configuration via `EditorConfig`:
    - Screen dimensions (rows, cols)
    - Initial filename
    - Line numbers, word wrap flags
    - Lua scripting enable flag
    - Undo limits
  - Convenience accessors:
    - `editor_session_get_mode()` - Get current mode
    - `editor_session_is_dirty()` - Check for unsaved changes
    - `editor_session_get_filename()` - Get current filename
    - `editor_session_resize()` - Update screen dimensions
    - `editor_session_open()` / `editor_session_save()` - File operations
  - Thread-safe view model: snapshot is a deep copy safe to use from any thread
  - **Files Added**: `source/core/loki/session.h`, `source/core/loki/session.c`
  - **Files Modified**: `source/core/CMakeLists.txt`

- **EditorHost Abstraction**: Pluggable host layer for alternate editor environments
  - Separates CLI parsing and terminal orchestration from session logic
  - Enables alternate hosts: HTTP server, headless scripting, test harness
  - `EditorHost` interface with callbacks:
    - `read_event()` - Read next input event (blocking with timeout)
    - `render()` - Render current session state
    - `should_continue()` - Check if host should keep running
    - `destroy()` - Cleanup host resources
  - Optional lifecycle callbacks: `on_start`, `on_tick`, `on_quit`, `on_error`
  - Common entry points:
    - `editor_host_run(host, config)` - Create session and run to completion
    - `editor_host_loop(host, session)` - Run main loop with existing session
  - Built-in host implementations:
    - `editor_host_terminal_create(fd)` - Interactive terminal editing
    - `editor_host_headless_create()` - Scripted/automated editing with event queue
  - Headless host API for automation:
    - `editor_host_headless_queue_event()` - Queue programmatic input
    - `editor_host_headless_quit()` - Signal quit
  - CLI argument parsing extracted to separate module:
    - `EditorCliArgs` struct for parsed arguments
    - `editor_cli_parse()` - Parse argc/argv into config
    - `editor_cli_print_usage()` / `editor_cli_print_version()`
  - **Files Added**: `source/core/loki/host.h`, `source/core/loki/host.c`, `source/core/loki/cli.h`, `source/core/loki/cli.c`
  - **Files Modified**: `source/core/CMakeLists.txt`

- **JSON-RPC Test Harness**: stdio-based command interface for testing editor abstractions
  - Validates EditorSession API without terminal I/O
  - Enables automated integration testing and scripting
  - **Commands**:
    - `{"cmd": "load", "file": "path"}` - Load file into editor
    - `{"cmd": "save"}` - Save current file
    - `{"cmd": "event", "type": "key", "code": N, "modifiers": M}` - Send key event
    - `{"cmd": "event", "type": "resize", "rows": N, "cols": M}` - Send resize event
    - `{"cmd": "event", "type": "quit"}` - Send quit event
    - `{"cmd": "insert", "text": "..."}` - Insert text as key events
    - `{"cmd": "snapshot"}` - Get full viewmodel as JSON
    - `{"cmd": "status"}` - Get editor status (mode, filename, dirty)
    - `{"cmd": "resize", "rows": N, "cols": M}` - Resize screen
    - `{"cmd": "quit"}` - Exit harness
  - **Responses**: `{"ok": true, ...}` or `{"ok": false, "error": "message"}`
  - **Run modes**:
    - `jsonrpc_run_interactive()` - Read commands from stdin until quit/EOF
    - `jsonrpc_run_single()` - Process single command and exit
  - **CLI flags** for invoking JSON-RPC mode:
    - `--json-rpc` - Run in interactive JSON-RPC mode
    - `--json-rpc-single` - Run single JSON-RPC command and exit
    - `--rows N` - Screen rows for headless mode (default: 24)
    - `--cols N` - Screen cols for headless mode (default: 80)
  - Minimal JSON library (no external dependency):
    - `JsonBuilder` - Streaming JSON serialization
    - `json_parse()` - Parse JSON from string
    - `json_object_get_string()`, `json_object_get_int()`, `json_object_get_bool()` - Value accessors
  - `jsonrpc_serialize_viewmodel()` - Full viewmodel to JSON for snapshot command
  - **Files Added**: `source/core/loki/json.h`, `source/core/loki/json.c`, `source/core/loki/jsonrpc.h`, `source/core/loki/jsonrpc.c`
  - **Files Modified**: `source/core/loki/cli.h`, `source/core/loki/cli.c`, `source/core/CMakeLists.txt`

- **Abstract Input Handling Layer**: Structured event abstraction for editor input
  - Replaces raw keycodes with `EditorEvent` objects for cleaner input processing
  - Modifier flags (`MOD_CTRL`, `MOD_SHIFT`, `MOD_ALT`) separated from keycodes
    - `SHIFT_ARROW_UP` becomes `(ARROW_UP, MOD_SHIFT)` internally
  - Event types: `EVENT_KEY`, `EVENT_COMMAND`, `EVENT_ACTION`, `EVENT_RESIZE`, `EVENT_MOUSE`, `EVENT_QUIT`
  - `EventSource` interface for polymorphic input sources:
    - `event_source_terminal(fd)` - wraps `terminal_read_key()` for terminal input
    - `event_source_test()` - queue-based source for unit testing without I/O
  - Backward-compatible conversion functions:
    - `event_from_keycode()` - legacy keycode to event
    - `event_to_keycode()` - event back to legacy keycode
  - New entry point `modal_process_event()` for event-based modal processing
    - Handles Ctrl-X prefix sequences via `pending_prefix` state (no fd required)
    - Handles Ctrl-T (new buffer), Ctrl-Q (quit) directly
  - Decoupled `modal_process_keypress()` from file descriptor dependency
    - Now a thin wrapper that reads from terminal and delegates to `modal_process_event()`
    - Terminal-specific operations (Ctrl-F find) intercepted before delegation
  - Enables test injection without terminal I/O
  - Foundation for future transports (WebSocket, RPC)
  - **Files Added**: `source/core/loki/event.c`, `source/core/loki/event.h`
  - **Files Modified**: `source/core/loki/modal.c`, `source/core/loki/internal.h`, `source/core/CMakeLists.txt`

- **Web Host Mode**: Browser-based editor using xterm.js terminal emulation
  - Access the editor via web browser at `http://localhost:8080`
  - Full terminal emulation with xterm.js
  - WebSocket-based communication for real-time editing
  - **CLI Flags**:
    - `--web` - Start web server (default port 8080)
    - `--web-port N` - Use custom port
  - **Features**:
    - Mouse click-to-position support
    - All editor keybindings work as in terminal mode
    - Language switching commands (`:alda`, `:joy`, `:langs`, `:lang NAME`)
    - First-line directives (`#alda`, `#joy`) for automatic language detection
    - REPL mode with language-aware evaluation
  - **Build Options**:
    - `-DBUILD_WEB_HOST=ON` - Enable web server mode
    - `-DLOKI_EMBED_XTERM=ON` - Embed xterm.js in binary (no CDN dependency)
  - Embedded mode adds ~300KB to binary size but enables offline use
  - **Dependencies**: mongoose (embedded web server), xterm.js (terminal emulator)
  - **Files Added**: `source/core/loki/host_web.c`, `source/core/loki/host_web.h`, `source/core/loki/host_web_xterm.h`

- **Live Loop Feature**: Re-evaluate buffer content on beat boundaries synced to Ableton Link
  - `:loop <beats>` - Start live loop that re-evaluates buffer every N beats (e.g., `:loop 4`)
  - `:unloop` - Stop live loop for current buffer
  - `:loop` (no args) - Show current loop status
  - Requires Ableton Link to be enabled first (`:link on`)
  - Up to 16 concurrent loops across different buffers
  - Enables concurrent multi-language live coding with synchronized playback
  - **Files Added**: `src/loki/live_loop.c`, `src/loki/live_loop.h`, `src/loki/command/loop.c`

- **Playback Ex Commands**: Command-line equivalents for playback control keys
  - `:play` - Play entire buffer (equivalent to Ctrl-P)
  - `:eval [code]` - Evaluate given code or current line (equivalent to Ctrl-E)
  - `:stop` - Stop all playback and live loops (equivalent to Ctrl-G)
  - All commands dispatch through language bridge based on file extension

- **Bog Language Integration**: Prolog-based live coding language for music, inspired by [dogalog](https://github.com/danja/dogalog)
  - **REPL Mode**: `psnd bog` starts interactive Bog REPL with syntax highlighting
    - Declarative event rules: `event(kick, 36, 0.9, T) :- every(T, 1.0).`
    - Named slots for managing multiple patterns: `:def kick ...`, `:undef kick`, `:slots`
    - Mute/unmute/solo controls for live performance: `:mute kick`, `:solo hat`
    - Virtual MIDI port creation: `--virtual NAME`
    - Non-blocking scheduler with ~10ms tick interval
  - **Timing Predicates**: Flexible rhythm generation
    - `every(T, N)` - Fire every N beats
    - `beat(T, N)` - Fire on beat N of bar
    - `euc(T, K, N, B, R)` - Euclidean rhythms (K hits over N steps)
    - `phase(T, P, L, O)` - Phase patterns
  - **Selection Predicates**: Variation and randomness
    - `choose(X, List)` - Random selection
    - `seq(X, List)` - Cycle through list
    - `shuffle(X, List)` - Random permutation
    - `wrand(X, List)` - Weighted random
    - `chance(P, Goal)` - Probabilistic execution
  - **Voice Mapping**: Bog voices map to General MIDI
    - Drums: `kick` (36), `snare` (38), `hat` (42), `clap` (39), `noise` (46) on channel 10
    - Melodic: `sine`, `square`, `triangle` on channel 1
  - **Editor Support**: Full livecoding for `.bog` files
    - `Ctrl-E` - Evaluate current buffer
    - `Ctrl-S` - Stop playback
    - `Ctrl-P` - Panic (all notes off)
  - **Lua API** (`loki.bog` table):
    - `loki.bog.init()` - Initialize Bog subsystem
    - `loki.bog.eval(code)` - Evaluate Bog code
    - `loki.bog.stop()` - Stop playback
    - `loki.bog.is_playing()` - Check if playing
    - `loki.bog.set_tempo(bpm)` - Set tempo
    - `loki.bog.set_swing(amount)` - Set swing
  - **Files Added**:
    - `src/lang/bog/` - Bog language implementation (REPL, dispatch, register, async)
    - `src/lang/bog/impl/` - Core Bog engine (parser, unifier, builtins, scheduler, state manager)
    - `tests/bog/` - Bog test suite (parser, unify, builtins, state manager, resolution, livecoding)
    - `docs/bog/` - Bog documentation (README.md, overview.md)

- **REPL History Persistence**: Command history is now saved between sessions for all REPLs
  - Prefers local `.psnd/` directory if it exists, falls back to `~/.psnd/` if present
  - Joy: `{.psnd}/joy_history`, Alda: `{.psnd}/alda_history`, TR7: `{.psnd}/tr7_history`
  - New shared functions `repl_history_load()` and `repl_history_save()` in `src/repl.c`
  - History limited to 64 entries per REPL

- **Shared Async Playback Service**: Unified non-blocking MIDI playback for all languages
  - New `src/shared/async/shared_async.c/h` - language-agnostic async playback engine
  - Supports both millisecond-based timing (Joy) and tick-based timing with tempo changes (Alda)
  - Up to 8 concurrent playback slots for polyphonic layering
  - libuv-based timer dispatch in background thread
  - Event types: NOTE, NOTE_ON, NOTE_OFF, CC, PROGRAM, TEMPO
  - Tick-based scheduling functions: `shared_async_schedule_*_tick()`
  - `shared_async_ticks_to_ms()` conversion with dynamic tempo tracking

- **Non-blocking Joy REPL**: Joy REPL now remains responsive during MIDI playback
  - `joy_async.c` - thin wrapper around shared async service
  - Commands like `[c d e] play` return immediately while notes play in background
  - Multiple `play` commands layer concurrently instead of blocking sequentially
  - `:stop` command halts all playback

- **Non-blocking TR7 REPL**: TR7 Scheme REPL now remains responsive during MIDI playback
  - `async.c/h` - thin wrapper around shared async service
  - `(play-note pitch [vel] [dur])` returns immediately while note plays in background
  - `(play-chord '(pitches...) [vel] [dur])` plays chord asynchronously
  - `(play-seq '(pitches...) [vel] [dur])` plays notes sequentially without blocking
  - `:stop` command halts all playback

- **Unified `:play` Command**: Generic file playback command that dispatches by extension
  - `:play file.csd` - plays Csound file (blocking)
  - `:play file.alda` - interprets and plays Alda file (from Alda REPL)
  - `:play file.joy` - loads and executes Joy file (from Joy REPL)
  - `:play file.scm` - loads and executes Scheme file (from TR7 REPL)
  - Replaces the previous `:cs-play` command

- **Ableton Link Callbacks in REPLs**: All REPLs now receive Link event notifications
  - Added `shared_repl_link_init_callbacks()`, `shared_repl_link_check()`, `shared_repl_link_cleanup_callbacks()` to `src/shared/repl_commands.c`
  - Joy, Alda, and TR7 REPLs poll for Link events after each command
  - Prints `[Link] Tempo: N BPM`, `[Link] Peers: N`, `[Link] Transport: playing/stopped` to stdout when changes occur
  - Tempo changes automatically sync to the REPL's SharedContext

- **Ableton Link Tempo Sync for Playback**: All languages now use Link tempo when playing
  - Alda: Uses `shared_link_effective_tempo()` to set initial playback tempo
  - Joy: Scales note timings at playback time based on `local_tempo / link_tempo` ratio
  - TR7: Scales note timings at playback time based on `local_tempo / link_tempo` ratio
  - When Link is enabled and connected to peers, playback tempo matches the Link session

### Changed

- **Unified Async Event Queue**: Migrated language playback callbacks to event-driven architecture
  - Replaced polling-based `loki_alda_check_callbacks()` with completion callback mechanism
  - Playback completion now pushes events to unified async queue via `on_alda_playback_complete()`
  - Main loop dispatches all async events through single `async_queue_dispatch_lua()` call
  - Link callbacks, beat boundaries, and language callbacks all use the same event path
  - Added `shared_async_play_ex()` with completion callback parameter
  - Added `alda_events_play_async_ex()` for callback-based playback
  - Extended `async_queue_push_lang_callback()` with events_played, duration_ms, callback name, error
  - Removed `loki_alda_check_callbacks()` - slot clearing now happens in completion callback
  - Marginal efficiency gain (no polling when idle), but main benefit is architectural consistency

- **Makefile Build Targets**: Separated clean and reset functionality
  - `make clean` now uses CMake's clean target (removes compiled objects, keeps cache for faster rebuilds)
  - `make reset` removes the entire build directory (forces full CMake reconfiguration)
  - `make remake` combines reset + build for convenience
  - Use `make reset && make` after adding new languages with `new_lang.py`

- **LuaHost Architecture Refactoring**: Moved Lua state from EditorView to dedicated LuaHost struct
  - Previously `lua_State *L` and `t_lua_repl repl` were embedded in `EditorView`, but Lua is a scripting platform, not a presentation concern
  - New `LuaHost` struct owns both Lua state and REPL state, stored as pointer in `editor_ctx`
  - Accessor macros `ctx_L(ctx)` and `ctx_repl(ctx)` provide indirect access with NULL safety
  - LuaHost lifecycle functions: `lua_host_create()`, `lua_host_free()`, `lua_host_init_repl()`
  - Proper sharing semantics across buffer contexts via pointer assignment
  - Files modified: `internal.h`, `lua.c`, `editor.c`, `buffers.c`, `core.c`, `modal.c`, `command.c`, `repl_launcher.c`, `alda/repl.c`, `bog/repl.c`

- **Alda Async Migrated to Shared Service**: Alda now uses the shared async playback engine
  - Reduced `src/lang/alda/async.c` from ~570 lines to ~135 lines (thin wrapper)
  - Tick-based timing with tempo change support preserved
  - Sequential/concurrent mode flag maintained for backwards compatibility
  - Removed direct `uv_a` dependency (now comes transitively from shared library)

- **Consolidated Dispatch Systems**: Both CLI (`lang_dispatch`) and editor (`loki_lang_bridge`) now use explicit initialization
  - Removed `__attribute__((constructor))` from all `register.c` files (Alda, Joy, TR7)
  - Added `loki_lang_init()` in `src/loki/lang_bridge.c` that calls per-language init functions
  - Each language exports `*_loki_lang_init()` (alda, joy, tr7)
  - `loki_editor_main()` calls `loki_lang_init()` before any language operations
  - CMake passes `LANG_ALDA`, `LANG_JOY`, `LANG_TR7` defines for conditional compilation
  - Both dispatch systems now portable to MSVC (no GCC/Clang-specific attributes)

- **Shared REPL Launcher**: Extracted common REPL startup logic into reusable module
  - New `src/loki/repl_launcher.c/h` with `SharedReplCallbacks` and `SharedReplArgs`
  - Languages provide callbacks for: print_usage, list_ports, init, cleanup, exec_file, repl_loop
  - Shared launcher handles: CLI parsing (`-h`, `-v`, `-l`, `-p`, `--virtual`, `-sf`), syntax highlighting setup, common flow control
  - Joy refactored to use `shared_lang_repl_main()` and `shared_lang_play_main()`
  - TR7 refactored to use the same shared launcher pattern
  - Reduced ~350 lines of duplicate code between Joy and TR7

- **Centralized Constants**: New `include/psnd.h` replaces hardcoded strings throughout codebase
  - `PSND_NAME` - Program name ("psnd")
  - `PSND_VERSION` - Version string ("0.1.2")
  - `PSND_CONFIG_DIR` - Configuration directory (".psnd")
  - `PSND_MIDI_PORT_NAME` - Default virtual MIDI port name ("PSND_MIDI")
  - Updated 12 source files to use these constants
  - Removed `include/version.h` (superseded by `psnd.h`)

- **Shared Csound Backend**: Moved Csound synthesis to shared layer
  - Real implementation now in `src/shared/audio/csound_backend.c`
  - `src/alda/csound_backend.c` provides thin wrappers calling `shared_csound_*` functions
  - Csound synthesis now available to all languages (Alda, Joy) through the shared backend
  - CMakeLists.txt updated to add Csound dependency to shared library

- **Language-agnostic `:csd` Command**: Refactored Csound command to be language-independent
  - New `src/loki/csound.c` provides editor-level Csound control
  - `:csd` command no longer requires Alda initialization
  - Works regardless of whether editing Alda or Joy files

- **Shared MIDI Event Buffer**: Added common event format for MIDI export
  - New `SharedMidiEvent` type in `src/shared/midi/events.h`
  - Shared event buffer API (`shared_midi_events_*`)
  - Languages convert their events to shared format at export time
  - New `loki_midi_export_shared()` reads from shared buffer
  - Enables future languages to support MIDI export

- **Language-agnostic `:export` Command**: Refactored MIDI export command to be language-independent
  - New `src/loki/export.c` provides editor-level export control
  - Converts Alda events to shared format before export
  - Generic error messages (not Alda-specific)

- **Modular Command System**: Refactored editor ex-commands into separate files
  - New `src/loki/command/` directory with one file per command category
  - `command_impl.h` - Shared header with documentation on adding new commands
  - `file.c` - File operations (`:w`, `:e`)
  - `basic.c` - Core commands (`:q`, `:wq`, `:help`, `:set`)
  - `goto.c` - Navigation (`:goto`, `:<number>`)
  - `substitute.c` - Search and replace (`:s/old/new/`)
  - `link.c` - Ableton Link (`:link`)
  - `csd.c` - Csound synthesis (`:csd`)
  - `export.c` - MIDI export (`:export`)
  - Main `command.c` now contains only dispatch logic and command table

- **Unified REPL Command API**: Both Alda and Joy REPLs now share the same command set
  - Common commands work identically in both REPLs (`:help`, `:quit`, `:list`, `:sf`, `:link`, `:cs`, etc.)
  - Commands can be used with or without `:` prefix in both REPLs
  - New shared command processor in `src/shared/repl_commands.c`
  - Language-specific commands remain separate (Alda: `:sequential`/`:concurrent`, Joy: `.` for stack)

- **CLI Normalization**: Simplified command-line interface
  - `psnd` (no args) now shows help and exits with code 1 (was: start Alda REPL)
  - `psnd alda` starts Alda REPL (replaces bare `psnd` and `psnd repl`)
  - `psnd joy` starts Joy REPL (unchanged)
  - Removed implicit REPL fallback for `-sf` without subcommand

### Added

- **Go-to-line Command**: Jump to specific line numbers in the editor
  - `:123` - Jump to line 123
  - `:goto 123` - Same using explicit command name
  - Auto-scrolls viewport to show target line
  - Implemented in `src/loki/command/goto.c`

- **Search and Replace Command**: Vim-style substitution on current line
  - `:s/old/new/` - Replace first occurrence on current line
  - `:s/old/new/g` - Replace all occurrences on current line (global flag)
  - Supports escaped characters (`\/` for literal `/`)
  - Reports number of substitutions made
  - Implemented in `src/loki/command/substitute.c`

- **Shared REPL Commands**: New unified commands available in both Alda and Joy REPLs
  - `:q` `:quit` `:exit` - Exit REPL
  - `:h` `:help` `:?` - Show help
  - `:l` `:list` - List MIDI ports
  - `:s` `:stop` - Stop playback
  - `:p` `:panic` - All notes off
  - `:sf PATH` - Load soundfont and enable built-in synth
  - `:synth` `:builtin` - Switch to built-in synth
  - `:midi` - Switch to MIDI output
  - `:presets` - List soundfont presets
  - `:virtual [NAME]` - Create virtual MIDI port
  - `:link [on|off]` - Enable/disable Ableton Link
  - `:link-tempo BPM` - Set Link tempo
  - `:link-status` - Show Link status
  - `:cs PATH` - Load CSD file and enable Csound
  - `:csound` - Enable Csound backend
  - `:cs-disable` - Disable Csound
  - `:cs-status` - Show Csound status

- **Shared Service Tests**: New test suite for shared backend services (`tests/shared/`)
  - `test_link.c` - 17 tests for Ableton Link (init, enable/disable, tempo, peers, start/stop sync, beat/phase)
  - `test_midi_events.c` - 17 tests for shared MIDI event buffer (recording, retrieval, sorting, capacity)
  - `test_midi_export.c` - 12 tests for MIDI file export (Type 0/Type 1, header validation via "MThd" magic)
  - Link tests excluded from ctest (network discovery can hang); run manually with `./build/tests/shared/test_link`

### Fixed

- **REPL Syntax Highlighting in Generated Languages**: Fixed black text in REPLs created by `new_lang.py`
  - Generated REPLs were missing `editor_ctx_init()` and `syntax_init_default_colors()` calls
  - Without color initialization, all text rendered as black (default zero values)
  - Now properly initializes editor context, default colors, and Lua host for theme loading
  - Updated template in `scripts/new_lang.py` for future language generation

- **PEG Parser Exits on Syntax Error**: Fixed REPL crash on invalid input in languages created by `new_lang_peg.py`
  - PackCC's default `PCC_ERROR` macro calls `exit(1)` on parse failure
  - Added `#define PCC_ERROR(auxil) ((void)0)` to suppress exit behavior
  - Parser now returns failure instead of terminating, REPL continues with error message
  - Updated template in `scripts/new_lang_peg.py` for future language generation

- **Panic Leaves Stuck Notes on Secondary Backends**: Fixed `shared_send_panic()` only silencing the highest-priority backend
  - Previously returned after first active backend (Csound > TSF > MIDI), leaving other backends ringing
  - Now broadcasts "all notes off" to ALL enabled backends regardless of priority
  - Prevents stuck notes when switching backends mid-session or during cleanup

- **Context Cleanup Kills Audio for Other Sessions**: Added reference counting to backend singletons (TSF, Csound, Link)
  - Previously, `shared_context_cleanup()` would unconditionally disable backends that the context had enabled
  - Multiple contexts (REPL, editor, multiple buffers) share the same backend singletons
  - Quitting one REPL would stop audio for all other active sessions
  - Now enable/disable are ref-counted: backend only actually enables on first reference (0->1) and disables on last release (1->0)
  - Files: `src/shared/audio/tsf_backend.c`, `src/shared/audio/csound_backend.c`, `src/shared/link/link.c`

- **Removed Duplicate Alda MIDI Observer**: Eliminated legacy observer that duplicated shared context functionality
  - `alda_midi_init_observer()` was maintaining BOTH a shared observer AND a legacy observer copy
  - This doubled enumeration work, risked memory leaks if one path failed, and complicated cleanup
  - All Alda MIDI operations now delegate entirely to the shared context (`shared_midi_*` functions)
  - Legacy MIDI fields in `AldaContext` (`midi_observer`, `out_ports[]`, `out_port_count`) marked as deprecated
  - The `midi_out` pointer is still synced from `shared->midi_out` for API compatibility
  - Removed ~200 lines of redundant code from `src/lang/alda/backends/midi_backend.c`

- **Joy Multi-Context Support**: Fixed Joy MIDI backend stomping shared context between sessions
  - Previously, Joy's MIDI backend used a global `g_shared` that could be overwritten by multiple callers (REPL vs editor)
  - When one session cleaned up, it would free the context still in use by another session
  - Added ownership tracking (`g_owns_context` flag) to `joy_midi_backend.c`
  - `joy_set_shared_context()` now marks external contexts as not owned (won't free on cleanup)
  - Joy REPL (`repl.c`) now creates and owns its own SharedContext via `g_joy_repl_shared`
  - Joy editor integration (`register.c`) creates per-editor-context SharedContext
  - TR7 already had per-context ownership in both REPL and editor integration

- **MIDI Export Multi-track Crash**: Fixed segfault when exporting multi-channel compositions
  - Track 0 (conductor) was empty when no tempo events existed in the shared buffer
  - Now always adds a default tempo (120 BPM) to ensure track 0 has content

## [0.1.2]

### Added

- **Piped Input Support**: Both Alda and Joy REPLs now support non-interactive piped input
  - `echo ':q' | psnd` - Alda REPL processes piped commands
  - `echo 'quit' | psnd joy` - Joy REPL processes piped commands
  - Detects `!isatty(STDIN_FILENO)` and uses `fgets()` instead of interactive line editor
  - Useful for scripting and automation

- **Joy Csound Backend Integration**: Joy language now supports Csound synthesis
  - **REPL Commands**:
    - `cs-load PATH` - Load a CSD file and auto-enable Csound
    - `cs-enable` - Enable Csound as audio backend
    - `cs-disable` - Disable Csound
    - `cs-status` - Show Csound status
    - `cs-play PATH` - Play a CSD file (blocking)
  - **Joy Primitives**: `cs_load_`, `cs_enable_`, `cs_disable_`, `cs_status_`, `cs_play_`
  - **C API** (`joy_midi_backend.h`):
    - `joy_csound_init()`, `joy_csound_cleanup()`
    - `joy_csound_load(path)`, `joy_csound_enable()`, `joy_csound_disable()`
    - `joy_csound_is_enabled()`, `joy_csound_play_file()`, `joy_csound_get_error()`
  - Routes MIDI events through Alda's Csound backend with priority routing (Csound > TSF > MIDI)
  - Proper cleanup on quit (auto-disables Csound before exit)

- **Joy Language Integration**: Full support for Joy, a concatenative (stack-based) music language from [midi-langs](https://github.com/shakfu/midi-langs)
  - **Joy REPL**: `psnd joy` starts interactive Joy REPL with syntax highlighting
    - Stack-based evaluation: `[c d e] play`, `c major chord`
    - Music theory primitives: `major`, `minor`, `dom7`, `dim`, `aug`
    - MIDI control: `tempo`, `vol`, `pan`, `midi-note`, `midi-cc`
    - Virtual MIDI port creation: `midi-virtual` or `--virtual NAME`
  - **Editor Support**: Full livecoding for `.joy` files
    - `Ctrl-E` - Evaluate current line/selection
    - `Ctrl-P` - Play entire file
    - `Ctrl-G` - Stop playback (MIDI panic)
    - Auto-initialization of Joy context and virtual MIDI port
  - **Play Mode**: `psnd play file.joy` for headless playback
  - **Syntax Highlighting**: Built-in highlighting for Joy files
    - Stack operations (`dup`, `swap`, `pop`, `dip`, `i`, `x`)
    - Combinators (`map`, `fold`, `filter`, `each`)
    - Music primitives (`note`, `chord`, `play`, `rest`)
    - Note names (`c`, `d`, `e`, `f`, `g`, `a`, `b`)
  - **Lua API** (`loki.joy` table):
    - `loki.joy.init()` - Initialize Joy subsystem
    - `loki.joy.eval(code)` - Evaluate Joy code
    - `loki.joy.load(path)` - Load Joy file
    - `loki.joy.define(name, body)` - Define Joy word
    - `loki.joy.stop()` - Stop playback (MIDI panic)
    - `loki.joy.open_port(n)` - Open MIDI port by index
    - `loki.joy.open_virtual(name)` - Create virtual MIDI port
    - `loki.joy.list_ports()` - List available MIDI ports
    - Stack operations: `push_int`, `push_string`, `stack_depth`, `stack_clear`, `stack_print`
  - **Files Added**:
    - `src/joy/` - Joy runtime (parser, primitives, MIDI backend)
    - `src/loki/joy.c`, `include/loki/joy.h` - Loki-Joy bridge
    - `tests/joy/` - Joy test suite (parser, primitives, MIDI)
  - **Files Modified**:
    - `src/loki/modal.c` - Joy keybinding handlers (Ctrl-E, Ctrl-P, Ctrl-G)
    - `src/main.c` - Joy REPL and play mode dispatch
    - `src/repl.c` - Joy REPL implementation

- **Comprehensive Alda Interpreter Tests**: Unit tests for MIDI event generation (`tests/alda/test_interpreter.c`)
  - 44 test cases covering core interpreter functionality
  - Basic notes: pitches, accidentals, octaves, sequences
  - Durations: note lengths, dotted, tied
  - Chords and chord voicings with octave changes
  - Tempo and volume/dynamics attributes
  - Repeats and alternate endings
  - Variables (definition, reference, redefinition)
  - Markers (@marker jumps)
  - Polyphonic voices (V1:, V2:, V0:)
  - Cram expressions (time compression)
  - Key signatures and transposition
  - Pan and quantization
  - Multiple parts and part groups
  - Error handling (undefined variables, markers, missing parts)
  - Unit tests for pitch calculation and duration functions

- **Csound CSD Syntax Highlighting**: Section-aware syntax highlighting for Csound `.csd`, `.orc`, `.sco` files
  - Parses CSD XML structure to detect `<CsOptions>`, `<CsInstruments>`, `<CsScore>` sections
  - **Orchestra section** (`<CsInstruments>`): Full Csound language highlighting
    - Control flow: `if`, `then`, `else`, `endif`, `while`, `do`, `od`, `goto`, etc.
    - Structure: `instr`, `endin`, `opcode`, `endop`
    - Header variables: `sr`, `kr`, `ksmps`, `nchnls`, `0dbfs`, `A4`
    - Common opcodes: `oscili`, `vco2`, `moogladder`, `pluck`, `reverb`, etc.
    - Comments: `;` single-line, `/* */` block comments
    - Strings and numbers
  - **Score section** (`<CsScore>`): Statement-based highlighting
    - Score statements (`i`, `f`, `e`, `s`, etc.) as keywords
    - Numeric parameters
    - `;` comments
  - **Options section** (`<CsOptions>`): Command-line flag highlighting
  - Section tags highlighted as keywords
  - Section state tracked across rows (like markdown code blocks)
  - Keywords extracted from Csound 6.18.1 lexer (`csound_orcparse.h`)
  - **Files Modified**: `internal.h`, `languages.c`, `languages.h`, `syntax.c`

- **Scala Scale File Support**: Parse and use Scala tuning files (`.scl`) for microtuning
  - **C Parser** (`include/alda/scala.h`, `src/alda/scala.c`):
    - `scala_load(path)` - Load .scl file from disk
    - `scala_load_string(buf, len)` - Parse from string buffer
    - `scala_get_ratio(scale, degree)` - Get frequency ratio for scale degree
    - `scala_get_frequency(scale, degree, base)` - Get frequency in Hz
    - `scala_midi_to_freq(scale, midi, root, freq)` - MIDI note to frequency with octave wrapping
    - `scala_cents_to_ratio(cents)` / `scala_ratio_to_cents(ratio)` - Unit conversions
  - **Lua API** (`loki.scala` table):
    - `loki.scala.load(path)` - Load scale file
    - `loki.scala.load_string(content)` - Load from string
    - `loki.scala.unload()` - Unload current scale
    - `loki.scala.loaded()` - Check if scale is loaded
    - `loki.scala.description()` - Get scale name/description
    - `loki.scala.length()` - Number of degrees (excluding implicit 1/1)
    - `loki.scala.ratio(degree)` - Get frequency ratio
    - `loki.scala.frequency(degree, base_freq)` - Get frequency in Hz
    - `loki.scala.midi_to_freq(note, [root], [freq])` - MIDI to Hz with scale
    - `loki.scala.degrees()` - Get all degrees as Lua table
    - `loki.scala.csound_ftable([base_freq], [fnum])` - Generate Csound f-table statement
    - `loki.scala.cents_to_ratio(cents)` / `loki.scala.ratio_to_cents(ratio)` - Utilities
  - **Syntax Highlighting**: `.scl` files with `!` comment highlighting and number highlighting
  - **Sample Scales** (`.psnd/scales/`):
    - `12tet.scl` - 12-tone equal temperament
    - `just.scl` - 5-limit just intonation major
    - `pythagorean.scl` - Pythagorean 12-tone chromatic
  - Format spec: <https://www.huygens-fokker.org/scala/scl_format.html>

- **Standalone CSD File Support**: Edit and play Csound .csd files directly
  - `psnd song.csd` - Open CSD file in editor with syntax highlighting
  - `psnd play song.csd` - Play CSD file headlessly and exit
  - `Ctrl-P` in editor plays the CSD file using Csound's embedded score section
  - `Ctrl-G` stops CSD playback
  - Async playback allows editing while audio plays
  - **Lua API**:
    - `loki.alda.csound_play(path)` - Play a CSD file asynchronously
    - `loki.alda.csound_playing()` - Check if CSD playback is active
    - `loki.alda.csound_stop()` - Stop CSD playback
  - Lua keybindings in `.psnd/keybindings/alda_keys.lua` automatically detect .csd files

- **Csound Synthesis Backend**: Optional advanced synthesis engine as alternative to TinySoundFont
  - Full Csound 6.18.1 integration for powerful synthesis beyond sample playback
  - Independent miniaudio audio device (each backend manages its own audio output)
  - MIDI events translated to Csound score events with fractional instrument IDs
  - Pre-defined instruments in `.psnd/csound/default.csd` (16 GM-compatible instruments)
  - **Build Option**: `make csound` or `-DBUILD_CSOUND_BACKEND=ON`
  - **CLI Options**:
    - `-cs PATH` - Load .csd file and enable Csound when opening an Alda file
    - `-sf PATH` - Load soundfont and enable TinySoundFont when opening an Alda file
  - **Ex Command**: `:csd [on|off|1|0]` - Toggle or set Csound synthesis backend
  - **Lua API** (`loki.alda` extensions):
    - `loki.alda.csound_available()` - Check if Csound is compiled in
    - `loki.alda.csound_load(path)` - Load a .csd instrument file
    - `loki.alda.set_csound(bool)` - Enable/disable Csound synthesis
    - `loki.alda.set_backend(name)` - Unified backend selection ("tsf", "csound", or "midi")
  - **Dependencies**: Csound 6.18.1, libsndfile (both built from source in thirdparty/)
  - **Binary Size**: ~4.4MB with Csound vs ~1.6MB without
  - **Files Added**:
    - `src/alda/csound_backend.c`
    - `include/alda/csound_backend.h`
    - `.psnd/csound/default.csd`

- **Ableton Link Integration**: Tempo synchronization with other Link-enabled applications
  - Sync tempo with Ableton Live, hardware devices, and other Link-compatible software on local network
  - **Ex Command**: `:link [on|off|1|0]` - Toggle or set Link synchronization
  - **Status Bar**: Shows "ALDA LINK" instead of "ALDA NORMAL" when Link is active
  - **Playback Integration**: When Link is enabled, playback uses the Link session tempo
  - **Lua API** (`loki.link` table):
    - `loki.link.init([bpm])` - Initialize Link with optional starting tempo (default 120)
    - `loki.link.cleanup()` - Clean up Link resources
    - `loki.link.enable(bool)` - Enable/disable Link networking
    - `loki.link.is_enabled()` - Check if Link is enabled
    - `loki.link.tempo()` - Get current session tempo
    - `loki.link.set_tempo(bpm)` - Set session tempo (propagates to peers)
    - `loki.link.beat([quantum])` - Get current beat position (default quantum: 4)
    - `loki.link.phase([quantum])` - Get phase within quantum [0, quantum)
    - `loki.link.peers()` - Get number of connected peers
    - `loki.link.start_stop_sync(bool)` - Enable/disable transport sync
    - `loki.link.is_playing()` - Get transport state
    - `loki.link.play()` - Start transport
    - `loki.link.stop()` - Stop transport
    - `loki.link.on_peers(fn)` - Register callback for peer count changes
    - `loki.link.on_tempo(fn)` - Register callback for tempo changes
    - `loki.link.on_start_stop(fn)` - Register callback for transport changes
  - **Files Added**: `src/loki_link.c`, `include/loki/link.h`
  - **Dependencies**: Ableton Link 3.1.5 (GPL v2+)

- **MIDI File Export**: Export Alda compositions to Standard MIDI Files (.mid)
  - **Ex Command**: `:export <filename.mid>` - Export current events to MIDI file
  - **Lua API**: `loki.midi.export(filename)` - Returns true on success, nil + error on failure
  - Exports as Type 0 MIDI (single track) for single-channel compositions
  - Exports as Type 1 MIDI (multi-track) for multi-channel compositions
  - Preserves tempo, program changes, pan, and all MIDI events
  - **Files Added**: `src/loki_midi_export.cpp`, `include/loki/midi_export.h`
  - **Dependencies**: midifile library (BSD-2-Clause)

- **Lua Keybinding Customization System**: User-definable keybindings via Lua
  - `loki.keymap(modes, key, callback, [description])` - Register a keybinding
  - `loki.keyunmap(modes, key)` - Remove a keybinding
  - Supports modes: 'n' (normal), 'i' (insert), 'v' (visual), 'c' (command)
  - Key notation: single chars ('a'), control keys ('<C-a>'), special keys ('<Enter>', '<Esc>', '<Tab>', etc.)
  - Lua callbacks are checked before built-in handlers in each mode
  - Alda keybindings (Ctrl-E, Ctrl-P, Ctrl-G) now customizable via `.psnd/keybindings/alda_keys.lua`

- **REPL Syntax Highlighting**: Real-time Alda syntax highlighting in the REPL as you type
  - Custom line editor with terminal raw mode (no external dependencies)
  - Keywords (tempo, volume, pan, etc.) highlighted in magenta
  - Note names and octave markers highlighted in cyan
  - Numbers highlighted in purple
  - Comments (starting with #) highlighted in gray
  - Full editing support: arrow keys, backspace, delete, home/end
  - Command history with up/down arrows

- **Built-in Alda Syntax**: Alda syntax highlighting now built into the editor
  - No longer requires Lua to load language definition
  - Works immediately in both editor and REPL modes

### Changed

- **Renamed project to psnd instead of aldalog**.

- **Project Restructure**: Reorganized source code for cleaner separation of concerns
  - Moved alda-midi library from `thirdparty/alda-midi/lib/` to `src/alda/` and `include/alda/`
  - Renamed loki source files from `src/loki_*.c` to `src/loki/*.c`
  - Organized tests into `tests/loki/` and `tests/alda/` subdirectories
  - Simplified CMakeLists.txt (~360 lines to ~210 lines)
- **Renamed project to aldalog instead of aldev**.
- **Stripped Binary**: Binary is now stripped by default, reducing size significantly
- **Simplified Configuration**: Renamed `.loki/` to `.psnd/` configuration directory
  - Removed unused modules (ai, editor, markdown, modal, languages, test, example)
  - Removed non-Alda language definitions (13 languages)
  - Kept only Alda-specific files: alda.lua (syntax), alda.lua (module), theme.lua, themes/

### Removed

- **editline/readline Dependency**: Removed external line editing library
  - REPL now uses custom line editor built on terminal raw mode
  - Eliminates dynamic library dependency (libedit.3.dylib)
  - Binary is now fully self-contained

- **libcurl Dependency**: Removed async HTTP and AI integration
  - Removed `loki.async_http()` Lua API
  - Removed `--complete` and `--explain` CLI options
  - Simplifies codebase for Alda-focused use case

- **cmark Dependency**: Removed Markdown parsing library
  - Removed `markdown` Lua module (to_html, parse, validate, etc.)
  - Removed `src/loki_markdown.c` and `src/loki_markdown.h`
  - Binary size reduced from 1.2MB to 1.1MB

### Fixed

- **Alda REPL Cleanup Crash in Pipe Mode**: Fixed segfault during cleanup when using piped input
  - Root cause: Double-free of MIDI output handle due to pointer aliasing
  - `ctx->midi_out` was synced to point to `ctx->shared->midi_out` (same pointer)
  - `alda_midi_cleanup` freed `ctx->midi_out`, then `alda_context_cleanup` tried to free `ctx->shared->midi_out`
  - Fix: Only free `ctx->midi_out` when NOT using shared context; otherwise just clear the pointer

- **Csound Audio Quality**: Fixed poor audio quality in Csound backend
  - Root cause: Audio output was not normalized by Csound's 0dBFS scaling factor
  - CSD files without explicit `0dbfs` setting default to 32768, causing clipping/distortion
  - Now divides all audio samples by `csoundGet0dBFS()` to normalize to -1.0 to 1.0 range
  - Also increased audio buffer size from 512 to 1024 frames to reduce glitches

- **Clean Ctrl-C Handling for CSD Playback**: Fixed messy exit when interrupting `psnd play`
  - Previously required multiple Ctrl-C presses and produced backtrace/crash output
  - Added proper SIGINT signal handler for blocking playback mode
  - Now cleanly stops playback and shows "Stopping playback" message
  - Exits gracefully without error codes or crash output

- **Csound Audio Not Playing**: Fixed Csound synthesis producing no audio output
  - Root cause: `async.c` event dispatcher was routing MIDI events only to TSF, ignoring Csound
  - Added Csound routing to `send_event()` with highest priority
  - Each backend now has its own independent miniaudio device (cleaner architecture)
  - Disproved theory that macOS couldn't handle multiple miniaudio instances

- **Parser Infinite Loop on Invalid Syntax**: Fixed hang when entering invalid Alda syntax in REPL
  - `tempo 120` (missing parentheses) now shows error instead of hanging
  - Parser now properly advances past unexpected tokens
  - Correct syntax `(tempo 120)` continues to work as expected

- **Lua API Cursor Position Bug**: Fixed `loki.get_cursor()` returning screen position instead of file position
  - Was returning `ctx->cy` (screen row) instead of `ctx->rowoff + ctx->cy` (file row)
  - Caused Lua keybindings like Ctrl-E to operate on wrong line when scrolled
  - Also fixed column position to include horizontal scroll offset

- **Lua API Stale Context Bug**: Fixed Lua API using stale editor context with multiple buffers
  - `lua_get_editor_context()` was returning a pointer stored at Lua init time
  - Now dynamically calls `buffer_get_current()` to get the active buffer's context
  - Falls back to registry pointer for backwards compatibility with tests

- **Line Numbers Not Displaying**: Fixed `loki.line_numbers(true)` having no effect
  - Root cause: `buffers_init()` and `buffer_create()` didn't copy the `line_numbers` field
  - Settings from init.lua were applied to initial context but lost when buffer system initialized
  - Now properly copies `line_numbers` setting when creating buffers

- **REPL Syntax Highlighting Now Uses Themes**: REPL now uses the same color theme as the editor
  - Previously: REPL used hardcoded default colors, ignoring `.psnd/init.lua` theme settings
  - Now: REPL initializes Lua and loads themes from `.psnd/init.lua`
  - Both editor and REPL now share consistent syntax highlighting colors
  - Refactored `repl.c` to thread `editor_ctx_t` through rendering functions

---

## [0.1.1]

### Added

- **Unified Binary**: Merged editor, REPL, and playback into single `psnd` binary
  - `psnd` (no args) - Interactive REPL for direct Alda notation input
  - `psnd file.alda` - Editor mode with live-coding
  - `psnd play file.alda` - Headless playback
  - `psnd -sf soundfont.sf2` - REPL with built-in synthesizer
  - Single 1.7MB distributable binary

- **Direct Alda REPL**: New interactive mode for typing Alda notation directly
  - No Lua wrapper required - type `piano: c d e f g` directly
  - Vim-style colon commands (`:q`, `:h`, `:stop`, etc.)
  - Soundfont loading (`:sf PATH`)
  - MIDI port listing (`:list`)
  - Concurrent/sequential playback modes
  - Line editing with history (editline/readline)

### Changed

- **Ctrl-E Plays Part**: `Ctrl-E` now plays the current Alda "part" (instrument declaration and all its notes) instead of just the current line
  - A part starts at an instrument declaration (e.g., `piano:`, `violin "alias":`) and extends until the next part declaration or end of file
  - Selection still takes precedence if text is visually selected
  - More musically meaningful for livecoding workflows

- **Eager Language Loading**: Changed from lazy to eager loading of language definitions in `.psnd/init.lua`
  - Ensures syntax highlighting works immediately when opening files
  - All languages in `.psnd/languages/` are now loaded at startup

- **Self-Contained Lua Build**: Switched from system Lua to local Lua 5.5.0 in thirdparty/
  - Project now builds without requiring system Lua installation
  - Added `thirdparty/lua-5.5.0/CMakeLists.txt` to build Lua as static library
  - Updated `thirdparty/CMakeLists.txt` to include Lua subdirectory
  - Updated main `CMakeLists.txt` to use local Lua instead of `find_package(Lua)`
  - Lua 5.5.0 features available (coroutine improvements, new warnings system)
  - Binary remains self-contained with no external Lua dependency

### Removed

- **Separate Binaries**: Removed `alda-editor` and `alda-repl` as separate executables
  - Replaced by unified `psnd` binary with mode dispatch
  - Deleted `src/main_editor.c` and `src/main_repl.c`

- **Lua REPL**: Removed standalone Lua REPL (`alda-repl`)
  - Lua scripting remains available in editor via `Ctrl-L`
  - New direct Alda REPL is simpler for music composition
  - Reduces user-facing complexity (Lua is internal for extension only)

### Fixed

- **Single-Character Comment Delimiters**: Fixed syntax highlighting for languages using single-character comment delimiters (e.g., `#` for Python, Alda, shell scripts)
  - Previously only two-character delimiters like `//` and `--` worked
  - Now correctly highlights comments starting with `#`

- **Dynamic Language Syntax Highlighting**: Fixed syntax highlighting not working for Lua-registered languages (like Alda)
  - Rows are now re-highlighted after dynamic languages are loaded
  - Syntax selection is re-run after Lua bootstrap completes

- **Keyword Matching Bounds Check**: Fixed potential out-of-bounds read when matching keywords at end of line

### Added

- **Alda Music Language Integration**: Complete livecoding support for the Alda music notation language
  - **Core Integration** (`src/loki_alda.c`, `src/loki_alda.h`):
    - Async playback using libuv event loop (non-blocking, editor remains responsive)
    - Built-in FluidSynth-based synthesizer with SoundFont support
    - Up to 8 concurrent playback slots for layered compositions
    - Polling callback mechanism for Lua integration
  - **Keybindings** (work in both NORMAL and INSERT modes):
    - `Ctrl-E` - Play current part or visual selection as Alda
    - `Ctrl-P` - Play entire file as Alda
    - `Ctrl-G` - Stop all playback
  - **Auto-Initialization**:
    - Automatically initializes Alda when opening `.alda` files
    - Status bar shows "ALDA" indicator when in Alda mode
    - Status bar shows "[PLAYING]" during active playback
  - **Syntax Highlighting** (`.psnd/languages/alda.lua`):
    - All General MIDI instruments (piano, violin, trumpet, etc.)
    - Alda attributes (tempo, volume, pan, quantization, etc.)
    - Note names (c, d, e, f, g, a, b) and rests (r)
    - Octave markers (o0-o9)
    - Line comments (#)
  - **Lua Helper Module** (`.psnd/modules/alda.lua`):
    - `alda.play(code, [callback])` - Play Alda code asynchronously
    - `alda.play_sync(code)` - Play Alda code (blocking)
    - `alda.play_line()` - Play current line (editor only)
    - `alda.play_paragraph()` - Play current paragraph
    - `alda.play_file()` - Play entire buffer
    - `alda.stop()` - Stop all playback
    - `alda.tempo(bpm)` - Set tempo (20-400 BPM)
    - `alda.soundfont(path)` - Load SoundFont (.sf2) file
    - `alda.synth(bool)` - Enable/disable built-in synthesizer
    - `alda.is_playing()` - Check if currently playing
    - `alda.get_tempo()` - Get current tempo
    - `alda.demo()` - Play demo melody
    - `alda.help()` - Show help
  - **Lua API** (`loki.alda` table):
    - `loki.alda.init()` - Initialize Alda subsystem
    - `loki.alda.eval(code, [callback])` - Evaluate Alda code async
    - `loki.alda.eval_sync(code)` - Evaluate Alda code sync
    - `loki.alda.stop_all()` - Stop all playback
    - `loki.alda.is_initialized()` - Check if initialized
    - `loki.alda.is_playing()` - Check if playing
    - `loki.alda.set_tempo(bpm)` - Set tempo
    - `loki.alda.get_tempo()` - Get tempo
    - `loki.alda.load_soundfont(path)` - Load SoundFont
    - `loki.alda.set_synth(bool)` - Enable/disable synth
  - **Dependencies** (in `thirdparty/`):
    - `alda-midi` - Alda parser and MIDI generation library
    - `libuv` - Async I/O for non-blocking playback
    - `libremidi` - Cross-platform MIDI output
  - **Files Added**:
    - `src/loki_alda.c`, `src/loki_alda.h` - Core Alda integration
    - `.psnd/languages/alda.lua` - Syntax highlighting
    - `.psnd/modules/alda.lua` - Lua helper module
  - **Files Modified**:
    - `src/loki_internal.h` - Added `CTRL_P`, `CTRL_G` keys and `alda_mode` flag
    - `src/loki_modal.c` - Added keybinding handlers
    - `src/loki_core.c` - Added status bar indicators
    - `src/loki_editor.c` - Added auto-initialization for .alda files
    - `src/loki_lua.c` - Added Lua bindings
    - `.psnd/init.lua` - Load alda module
    - `CMakeLists.txt` - Build integration

## [0.1.0] - Initial Release

- Project created
