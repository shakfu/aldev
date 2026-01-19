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

- [ ] Extract buffer manager to injectable service
  - Remove global `buffer_state` in `buffers.c`
  - Enables multi-editor and better testability

- [ ] Wrap editor core in standalone service process (optional)
  - Small RPC protocol (stdio JSON or gRPC)
  - Commands: load file, save, apply keystroke, get view state
  - Would enable embedding editor in other applications

## Medium Priority

### Testing

- [x] Add synthesis backend tests
  - `test_tsf_backend.c` - 19 tests (init, cleanup, loading, enable/disable, MIDI messages, boundaries)
  - `test_csound_backend.c` - 31 tests (availability, init, loading, enable/disable, MIDI, render, playback)
  - Tests verify API behavior without crashing; full audio testing requires manual verification

- [ ] Add scanner/lexer unit tests for all languages
  - Alda scanner vulnerable to malformed input
  - Joy/Bog/TR7 lexers untested

- [ ] Add fuzzing infrastructure
  - Parser robustness for malformed input
  - Consider AFL or libFuzzer integration

### Ableton Link Integration

- [x] **Beat-Aligned Start** - Playback quantizes to Link beat grid
  - Added `shared_link_ms_to_next_beat(quantum)` function
  - Added `launch_quantize` field to SharedContext and SharedAsyncSchedule
  - Added `loki.link.launch_quantize(quantum)` Lua API (0=immediate, 1=beat, 4=bar)
  - Alda and Joy async playback now respects launch quantization

- [ ] **Full Transport Sync** - Start/stop from any Link peer controls all
  - Wire transport callbacks to actually start/stop playback
  - Requires interruptible playback and a "armed for playback" state
  - Most complex - requires rethinking REPL interaction model

### Editor Features

- [ ] Playback visualization
  - Highlight currently playing region
  - Show playback progress in status bar

- [x] MIDI port selection from editor
  - Added `loki.midi.list_ports()`, `loki.midi.port_count()`, `loki.midi.port_name(index)`
  - Added `loki.midi.open_port(index)`, `loki.midi.open_by_name(name)`
  - Added `loki.midi.open_virtual(name)`, `loki.midi.close()`, `loki.midi.is_open()`

- [ ] Tempo tap
  - Tap key to set tempo

- [ ] Metronome toggle

### Refactoring

### Build System

- [x] Add AddressSanitizer build target
  - `cmake -B build -DPSND_ENABLE_ASAN=ON .`

- [x] Add code coverage target
  - `cmake -B build -DPSND_ENABLE_COVERAGE=ON .`
  - Use lcov/gcov to generate reports

- [x] Add install target
  - `cmake --install build [--prefix /usr/local]`
  - Installs binary to `bin/psnd`
  - Installs config to `share/psnd/`

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

- [ ] Extract shared REPL loop skeleton
  - ~150 lines of help functions still duplicated per language
  - Interactive loop structure still duplicated (could use callback pattern)

- [ ] Centralize platform CMake logic
  - Platform detection repeated in 6+ CMakeLists.txt files
  - Create `psnd_platform.cmake` module

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

- [x] Architecture diagram
  - Created `docs/architecture.d2` using D2 language
  - Renders to SVG/PNG: `d2 architecture.d2 architecture.svg`

- [ ] API reference generation
  - Consider Doxygen for generated docs

- [ ] Contributing guide

- [ ] Build troubleshooting
  - Platform-specific guidance

### Future Architecture

- [x] Lua-to-language primitive callbacks (Joy implemented)
  - Allow registering Lua functions as language primitives
  - Enables extending languages with Lua's ecosystem (HTTP, JSON, etc.)

  **Joy** (stack-based) - IMPLEMENTED:
  - API: `loki.joy.register_primitive("name", lua_callback)`
  - Callback receives: `function(stack) ... return modified_stack end`
  - Stack is a Lua array (index 1 = bottom, #stack = top)
  - Values: integers, floats, booleans, strings, tables (for lists/quotations)
  - Quotations represented as `{type="quotation", value={...tokens...}}`
  - Return modified stack or `nil, "error message"` on failure
  - Example:
    ```lua
    loki.joy.register_primitive("double", function(stack)
        if #stack < 1 then return nil, "stack underflow" end
        local top = table.remove(stack)
        table.insert(stack, top * 2)
        return stack
    end)
    ```
  - Files: `source/langs/joy/register.c` (lua_joy_register_primitive, joy_lua_primitive_wrapper)

  **TR7** (Scheme) - NOT YET IMPLEMENTED:
  - API: `loki.tr7.register_primitive("name", lua_callback, min_args, max_args)`
  - Callback receives: `function(arg1, arg2, ...) return result end`
  - Uses TR7's `tr7_C_func_def_t` registration with Lua state as closure
  - Type conversion via `TR7_FROM_*`/`TR7_TO_*` macros
  - Return value becomes Scheme result; `nil, "error"` raises exception
  - Implementation requires:
    - C wrapper `lua_proc_wrapper(tr7_engine_t, int nvalues, tr7_t* values, void* L)`
    - Convert `tr7_t` args to Lua values, call Lua function, convert result back
    - Register via `tr7_register_C_func(engine, &def)`
  - Complexity: Moderate (value conversion, error handling)

  **Bog** (Prolog) - NOT YET IMPLEMENTED:
  - API: `loki.bog.register_predicate("name", arity, lua_callback)`
  - Callback receives: `function(args_table, env_table) return solutions end`
  - `args_table`: array of terms (numbers, atoms, compounds, lists)
  - `env_table`: current variable bindings `{X = value, Y = value}`
  - Return: array of solution environments `{{X=1, Y=2}, {X=3, Y=4}}` or empty for failure
  - Implementation requires:
    - Extend `BogBuiltins` to support dynamic registration
    - C wrapper converting `BogTerm**` to Lua tables and back
    - Thread `lua_State*` through `BogContext` or scheduler
    - Handle non-determinism (multiple solutions)
  - Complexity: High (unification semantics, backtracking, term conversion)

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

### SharedContext Centralization
- EditorModel now owns single SharedContext for all languages
- Languages share context instead of creating separate instances
- Prevents conflicts on singleton backends (TSF, Csound, Link)
- REPLs still own their own SharedContext for standalone mode

### REPL Enhancements
- Added `:lang NAME` command to switch between language REPLs
- Added `:langs` command to list available languages
- Unified MIDI port name to `PSND_MIDI` across all languages

### Other Completed Items
- Standardized error return conventions
- Unified Lua binding pattern across languages
- Extracted shared REPL helper utilities
- Implemented `:play` command with file-type dispatch
- Wired Ableton Link callbacks for tempo sync
- Added TR7 test suite (38 tests)
