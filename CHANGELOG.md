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
