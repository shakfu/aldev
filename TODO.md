# Psnd TODO

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

- [ ] Modular language selection via CMake options
  - Add `BUILD_ALDA_LANGUAGE` and `BUILD_JOY_LANGUAGE` options (default ON)
  - Conditional library builds and linking in CMakeLists.txt
  - Preprocessor guards in `repl.c` and `main.c` for language-specific code
  - Enables building minimal binaries with only desired languages

---

## Medium Priority

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
