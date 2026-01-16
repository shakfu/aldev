# Psnd TODO

## Critical - Functional Regressions

### `psnd play` broken for Joy/TR7 (FIXED)
- [x] Fix `psnd play <file>` for Joy (DONE)
  - Added dedicated `joy_play_main` in `src/lang/joy/repl.c:373`
  - Parses argv starting at index 0 (not 1)
  - Updated dispatch to use `joy_play_main` instead of `joy_repl_main`
- [x] Fix `psnd play <file>` for TR7 (DONE)
  - Added dedicated `tr7_play_main` in `src/lang/tr7/repl.c:826`
  - Same pattern: parses argv starting at index 0
  - Updated dispatch to use `tr7_play_main` instead of `tr7_repl_main`

### SharedContext cleanup leaks global state (FIXED)
- [x] Fix `shared_context_cleanup()` to properly reset backend state (DONE)
  - Now sends panic (all notes off) before cleanup
  - Disables TSF if `ctx->tsf_enabled` was set
  - Disables Csound if `ctx->csound_enabled` was set
  - Disables Link if `ctx->link_enabled` was set
  - Resets all enable flags to prevent stale state

---

## High Priority

### Architecture (Shared Layer)

- [x] Move Csound backend to shared layer (DONE)
  - Real implementation now in `src/shared/audio/csound_backend.c`
  - `src/alda/csound_backend.c` contains thin wrappers calling `shared_csound_*`
  - Joy uses shared backend via alda wrappers
  - Csound synthesis available to all languages

- [x] Move MIDI export to shared layer (DONE)
  - [x] `:export` command is language-agnostic (`src/loki/export.c`)
  - [x] `SharedMidiEvent` format defined in `src/shared/midi/events.h`
  - [x] Shared event buffer API (`shared_midi_events_*`)
  - [x] Export reads from shared buffer (`loki_midi_export_shared`)
  - Alda events converted to shared format at export time
  - Joy uses immediate playback (could add recording in future)

- [x] Modular language selection via CMake options
  - Add `BUILD_ALDA_LANGUAGE` and `BUILD_JOY_LANGUAGE` options (default ON)
  - Conditional library builds and linking in CMakeLists.txt
  - Preprocessor guards in `repl.c` and `main.c` for language-specific code
  - Enables building minimal binaries with only desired languages

### Dispatch System Robustness

- [x] Replace GCC/Clang-specific constructors with explicit init (DONE)
  - Removed `__attribute__((constructor))` from all dispatch.c files
  - Added `lang_dispatch_init()` in `src/lang_dispatch.c` that calls language-specific init functions
  - Each language exports `*_dispatch_init()` (alda, joy, tr7)
  - `main()` calls `lang_dispatch_init()` before any dispatch operations
  - CMake passes `LANG_ALDA`, `LANG_JOY`, `LANG_TR7` defines for conditional compilation

- [x] Fix silent failures when language registration limit is hit (DONE)
  - `lang_dispatch_register()` now returns int (0 success, -1 error) and logs to stderr
  - `loki_lang_register()` now logs to stderr on failure
  - Both report the language name and the limit when registration fails

- [x] Consolidate dispatch systems (DONE)
  - Both `lang_dispatch` (CLI) and `loki_lang_bridge` (editor) now use explicit init pattern
  - Added `loki_lang_init()` in `src/loki/lang_bridge.c` that calls per-language init functions
  - Removed `__attribute__((constructor))` from all `register.c` files
  - Each language exports `*_loki_lang_init()` (alda, joy, tr7)
  - `loki_editor_main()` calls `loki_lang_init()` before any language operations
  - CMake passes `LANG_ALDA`, `LANG_JOY`, `LANG_TR7` defines for conditional compilation
  - Both systems now portable to MSVC (no more GCC/Clang-specific attributes)

---

## Medium Priority

### Refactoring

- [ ] Extract shared REPL launcher for language modules
  - Joy and TR7 duplicate CLI parsing, MIDI/synth setup, Lua bootstrapping, REPL loop
  - Compare `src/lang/joy/repl.c:209-352` with `src/lang/tr7/repl.c:634-720`
  - Divergence already visible: Joy hard-codes virtual port name, TR7 does not
  - Fix: Create `shared_lang_repl_main(struct repl_config*)` so new languages don't duplicate code

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

- [x] Add CLI dispatcher and `psnd play` integration tests (DONE)
  - Added `tests/cli/test_play_command.c` with 15 integration tests
  - Tests Joy, TR7/Scheme, and Alda play commands
  - Tests error cases (missing file, no argument, unknown extension)
  - Also fixed bug in `tr7_play_main` where success returned exit code 1

- [ ] Add `ASSERT_GT`, `ASSERT_LT` macros

- [ ] Add test fixture support (setup/teardown)

- [ ] Add memory leak detection hooks

---

## Low Priority

### Platform Support

- [ ] Windows support
  - Editor uses POSIX headers: `termios.h`, `unistd.h`, `pthread.h`
  - Language registration uses `__attribute__((constructor))` (see High Priority dispatch fix)
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
