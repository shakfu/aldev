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

- **Stripped Binary**: Binary is now stripped by default, reducing size significantly
- **Simplified Configuration**: Renamed `.loki/` to `.aldev/` configuration directory
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

- **Parser Infinite Loop on Invalid Syntax**: Fixed hang when entering invalid Alda syntax in REPL
  - `tempo 120` (missing parentheses) now shows error instead of hanging
  - Parser now properly advances past unexpected tokens
  - Correct syntax `(tempo 120)` continues to work as expected

---

## [0.1.1]

### Added

- **Unified Binary**: Merged editor, REPL, and playback into single `aldev` binary
  - `aldev` (no args) - Interactive REPL for direct Alda notation input
  - `aldev file.alda` - Editor mode with live-coding
  - `aldev play file.alda` - Headless playback
  - `aldev -sf soundfont.sf2` - REPL with built-in synthesizer
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

- **Eager Language Loading**: Changed from lazy to eager loading of language definitions in `.aldev/init.lua`
  - Ensures syntax highlighting works immediately when opening files
  - All languages in `.aldev/languages/` are now loaded at startup

- **Self-Contained Lua Build**: Switched from system Lua to local Lua 5.5.0 in thirdparty/
  - Project now builds without requiring system Lua installation
  - Added `thirdparty/lua-5.5.0/CMakeLists.txt` to build Lua as static library
  - Updated `thirdparty/CMakeLists.txt` to include Lua subdirectory
  - Updated main `CMakeLists.txt` to use local Lua instead of `find_package(Lua)`
  - Lua 5.5.0 features available (coroutine improvements, new warnings system)
  - Binary remains self-contained with no external Lua dependency

### Removed

- **Separate Binaries**: Removed `alda-editor` and `alda-repl` as separate executables
  - Replaced by unified `aldev` binary with mode dispatch
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
  - **Syntax Highlighting** (`.aldev/languages/alda.lua`):
    - All General MIDI instruments (piano, violin, trumpet, etc.)
    - Alda attributes (tempo, volume, pan, quantization, etc.)
    - Note names (c, d, e, f, g, a, b) and rests (r)
    - Octave markers (o0-o9)
    - Line comments (#)
  - **Lua Helper Module** (`.aldev/modules/alda.lua`):
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
    - `.aldev/languages/alda.lua` - Syntax highlighting
    - `.aldev/modules/alda.lua` - Lua helper module
  - **Files Modified**:
    - `src/loki_internal.h` - Added `CTRL_P`, `CTRL_G` keys and `alda_mode` flag
    - `src/loki_modal.c` - Added keybinding handlers
    - `src/loki_core.c` - Added status bar indicators
    - `src/loki_editor.c` - Added auto-initialization for .alda files
    - `src/loki_lua.c` - Added Lua bindings
    - `.aldev/init.lua` - Load alda module
    - `CMakeLists.txt` - Build integration

## [0.1.0] - Initial Release

- Project created
