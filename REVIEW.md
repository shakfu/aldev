# Psnd Project Review

## Executive Summary

**Psnd** is a self-contained, single-binary polyglot editor, REPL, and playback environment for music programming languages. The project has evolved from a single-language Alda editor to a platform designed to support multiple music DSLs. The architecture is clean, modular, and production-ready for its current Alda-focused use case, with a clear path toward polyglot expansion.

**Key Strengths:**
- Well-architected separation of concerns (editor vs. music engine)
- Comprehensive feature set for live-coding workflows
- No external runtime dependencies (fully self-contained)
- Multiple audio backends (MIDI, TinySoundFont, Csound)
- Extensible via Lua scripting

**Areas for Improvement:**
- Limited test coverage for the Alda interpreter and Csound backend
- Windows support not yet implemented

---

## Architecture Overview

### High-Level Structure

```
psnd (single binary)
    |
    +-- main.c (mode dispatcher)
    |
    +-- Editor (src/loki/)          +-- Music Engine (src/alda/)
    |   - Vim-like modal editing    |   - Alda parser/interpreter
    |   - Lua scripting             |   - MIDI event generation
    |   - Syntax highlighting       |   - Multiple audio backends
    |   - Multiple buffers          |   - Async playback
    |   - Undo/redo                 |
    +---------------------------+---+
                                |
                                v
                        Audio Output Layer
                        - libremidi (MIDI devices)
                        - TinySoundFont (software synth)
                        - Csound (optional, advanced synthesis)
```

### Mode Dispatch

The unified binary operates in three modes:

| Invocation | Mode | Purpose |
|------------|------|---------|
| `psnd` | REPL | Interactive Alda composition |
| `psnd file.alda` | Editor | Live-coding with file persistence |
| `psnd play file` | Play | Headless batch playback |

This design reduces user confusion and simplifies distribution to a single binary.

### Module Decomposition

**Editor Subsystem (src/loki/)** - 15 source files:

| Module | Responsibility |
|--------|----------------|
| `core.c` | Buffer/row operations, file I/O |
| `editor.c` | Main loop, screen refresh |
| `modal.c` | Vim modes (NORMAL/INSERT/VISUAL/COMMAND) |
| `terminal.c` | Raw terminal I/O, VT100 escape sequences |
| `syntax.c` | Extensible syntax highlighting |
| `undo.c` | Multi-level undo/redo with operation grouping |
| `buffers.c` | Multiple buffer management |
| `command.c` | Ex commands (`:q`, `:w`, etc.) |
| `search.c` | Incremental search |
| `selection.c` | Visual selection |
| `languages.c` | Dynamic language registration |
| `lua.c` | Lua VM integration and APIs |
| `indent.c` | Auto-indentation |
| `alda.c` | Editor-Alda integration glue |
| `link.c` | Ableton Link integration |

**Music Engine Subsystem (src/alda/)** - 14 source files:

| Module | Responsibility |
|--------|----------------|
| `scanner.c` | Character-level lexical analysis |
| `tokens.c` | Token stream generation |
| `parser.c` | AST construction (27 node types) |
| `ast.c` | AST node management |
| `interpreter.c` | AST traversal, MIDI event generation |
| `context.c` | Execution state (parts, tempo, events) |
| `scheduler.c` | Event queue management |
| `attributes.c` | Musical attributes (tempo, volume, pan) |
| `instruments.c` | General MIDI instrument definitions |
| `async.c` | libuv-based async playback |
| `midi_backend.c` | MIDI output via libremidi |
| `tsf_backend.c` | TinySoundFont software synth |
| `csound_backend.c` | Csound synthesis (optional) |
| `error.c` | Error reporting with source locations |

### Data Flow

```
Source Text (Alda notation)
    |
    v
Scanner (character stream -> lexemes)
    |
    v
Tokenizer (lexemes -> typed tokens)
    |
    v
Parser (tokens -> AST with 27 node types)
    |
    v
Interpreter (AST -> scheduled MIDI events)
    |
    v
Scheduler (event queue sorted by tick)
    |
    v
Async Playback Engine (libuv event loop)
    |
    +-- MIDI Backend (external devices)
    +-- TinySoundFont (software synth)
    +-- Csound (advanced synthesis)
```

---

## Code Quality Assessment

### Strengths

1. **Clean Separation**: The editor (loki) and music engine (alda) are independent modules with well-defined interfaces. Each can evolve independently.

2. **Modular C Design**: Despite being C99, the codebase follows modern practices:
   - Context structs (`editor_ctx_t`, `AldaContext`) encapsulate state
   - Clear module boundaries with public headers in `include/`
   - Internal headers (`internal.h`) hide implementation details

3. **Comprehensive Documentation**: 26 markdown files covering language reference, API docs, and examples. Inline code comments explain non-obvious logic.

4. **Self-Contained Build**: All dependencies bundled in `thirdparty/`. No package manager required. CMake build is straightforward.

5. **Reasonable Binary Size**: 1.6MB (TinySoundFont) or 4.4MB (with Csound) - compact for the feature set.

### Code Metrics

| Component | Source Files | Lines (approx) |
|-----------|-------------|----------------|
| Editor (loki) | 15 | ~9,000 |
| Music Engine (alda) | 14 | ~7,500 |
| Tests | 16 | ~7,000 |
| Configuration (Lua) | 15 | ~1,500 |
| **Total** | 60 | ~25,000 |

### Areas for Improvement

1. **Test Coverage Gaps**: The Alda interpreter and Csound backend lack comprehensive tests. The `tests/alda/` directory contains only `test_shared_suite.c`. Given the instruction "All major functionality must be tested - no exceptions," this is a significant gap.

2. **Hardcoded Limits**: Constants like `64` parts, `16384` events, and `256` variables are reasonable but could be configurable for edge cases.

3. **Error Recovery**: The parser has improved (fixed infinite loop bug), but more robust error recovery could improve the REPL experience.

4. **Memory Management**: While generally careful, some code paths could benefit from additional NULL checks and bounds validation.

---

## Feature Analysis

### Currently Implemented

| Feature | Status | Notes |
|---------|--------|-------|
| Vim-like modal editing | Complete | NORMAL/INSERT/VISUAL/COMMAND modes |
| Alda language support | Complete | Full parser, interpreter, 128 GM instruments |
| TinySoundFont synthesis | Complete | SoundFont loading, real-time playback |
| MIDI output | Complete | Cross-platform via libremidi |
| Csound backend | Complete | Optional, requires separate build |
| Ableton Link | Complete | Tempo sync with DAWs and peers |
| MIDI file export | Complete | Type 0/1 MIDI files |
| Lua scripting | Complete | Full API for editor and playback |
| Syntax highlighting | Complete | Built-in Alda, extensible via Lua |
| Async playback | Complete | 8 concurrent slots, libuv event loop |
| Multiple buffers | Complete | `:e file` to switch |
| Undo/redo | Complete | Smart operation grouping |
| Custom keybindings | Complete | Lua-based customization |

### Known Issues (from TODO.md)

- Windows not supported (POSIX terminal dependencies)

### Polyglot Roadmap

The architecture is explicitly designed for multiple languages:

1. **Language Registration API**: `languages.c` provides dynamic registration
2. **Shared Audio Stack**: All backends are language-agnostic
3. **Planned Integrations**: midi-langs mini-DSLs (sister project)

The current single-language implementation proves the architecture; adding languages requires parser/interpreter modules per DSL.

---

## Dependency Analysis

### Third-Party Libraries (10 total)

| Library | Purpose | License | Risk Assessment |
|---------|---------|---------|-----------------|
| Lua 5.5.0 | Scripting | MIT | Low - stable, well-maintained |
| libuv | Async I/O | MIT | Low - battle-tested (Node.js core) |
| libremidi | MIDI I/O | Boost | Low - active development |
| miniaudio | Audio I/O | Public Domain | Low - header-only, mature |
| TinySoundFont | SoundFont synth | MIT | Low - header-only |
| Ableton Link | Tempo sync | GPL v2+ | Medium - GPL implications |
| midifile | MIDI files | BSD-2-Clause | Low - stable |
| Csound | Synthesis | LGPL | Medium - large dependency (~3MB) |
| libsndfile | Audio files | LGPL | Low - standard audio library |

**License Compliance**: The project is GPL-3, which is compatible with all dependencies. The GPL v2+ (Link) and LGPL (Csound) components require the overall project to remain GPL.

### Build Dependencies

- CMake 3.18+
- C99 compiler
- C++17 compiler (for libremidi)
- No external package manager required

---

## Testing Assessment

### Current State

```
tests/
  test_framework.c       # Custom lightweight test framework
  loki/                  # 14 test files for editor
    test_buffers.c
    test_command.c
    test_core.c
    test_file_io.c
    test_indent.c
    test_lang_registration.c
    test_lua_api.c
    test_modal.c
    test_row_operations.c
    test_search.c
    test_selection.c
    test_syntax.c
    test_terminal.c
    test_undo.c
  alda/                  # 1 test file for music engine
    test_shared_suite.c
```

### Coverage Analysis

| Component | Test Files | Coverage |
|-----------|------------|----------|
| Editor core | 14 | Good |
| Modal editing | 1 | Good |
| Syntax highlighting | 1 | Good |
| Lua API | 1 | Good |
| Alda parser | 1 | **Insufficient** |
| Alda interpreter | 0 | **Missing** |
| Audio backends | 0 | **Missing** |

### Recommendations

1. **Add Alda Parser Tests**: The parser handles complex grammar (27 AST node types). Each node type should have dedicated tests.

2. **Add Interpreter Tests**: MIDI event generation is the core value proposition. Test tempo, volume, polyphony, markers, variables.

3. **Add Backend Tests**: Even minimal smoke tests for MIDI output, TinySoundFont, and Csound would catch regressions.

4. **Integration Tests**: End-to-end tests that parse Alda code and verify MIDI output.

---

## Performance Characteristics

| Operation | Complexity | Notes |
|-----------|-----------|-------|
| File loading | O(n) | n = file size |
| Character insertion | O(m) | m = line length |
| Row insertion | O(r) | r = total rows (shifts array) |
| Syntax highlighting | O(n) per row | Cached, recomputed on change |
| Undo/redo | O(k) | k = operations in group |
| Alda parsing | O(t) | t = tokens |
| Event scheduling | O(e log e) | e = events (sorted queue) |
| Playback | O(e) | Linear event dispatch |

The architecture is suitable for files up to tens of thousands of lines. Row insertion becomes a bottleneck for very large files, but this is acceptable for music notation files.

---

## Security Considerations

1. **File Operations**: Standard file I/O with user-provided paths. No sandboxing, but expected for a terminal editor.

2. **Lua Scripting**: Full Lua VM with filesystem access. Users control their own config files. No remote code execution.

3. **Network (Link)**: Ableton Link uses local network multicast. No internet exposure.

4. **Binary Files**: Editor detects and warns about binary files to prevent corruption.

No significant security concerns for the intended use case (local music composition tool).

---

## Comparison with Alternatives

| Tool | Category | Complexity | Terminal | Polyglot |
|------|----------|------------|----------|----------|
| **Psnd** | Editor+REPL | Low | Yes | Planned |
| SuperCollider | Full IDE | High | No | No |
| Sonic Pi | GUI App | Medium | No | No |
| TidalCycles | Haskell DSL | High | Partial | No |
| Pure Data | Visual | Medium | No | No |

Psnd occupies a unique niche: terminal-native, self-contained, and designed for polyglot expansion. The closest comparison is the Alda CLI itself, but psnd adds live-coding, built-in synthesis, and extensibility.

---

## Recommendations

### Short-Term (Bug Fixes)

1. Add comprehensive Alda interpreter tests

### Medium-Term (Features)

1. Implement the first additional language from midi-langs
2. Add Windows support (or document web-based alternative)
3. Improve parser error recovery and messages

### Long-Term (Architecture)

1. Consider plugin architecture for language modules
2. Evaluate JACK backend for pro audio workflows
3. Document extension API for third-party languages

---

## Conclusion

Psnd is a well-designed, actively maintained project that successfully combines a modal text editor with a music programming environment. The recent transition from single-language (Alda) to polyglot platform is architecturally sound, with clear separation of concerns enabling future language additions.

The codebase demonstrates professional engineering: clean module boundaries, comprehensive documentation, and a focus on user experience (single binary, no dependencies). The main weakness is test coverage for the music engine, which should be addressed given the project's correctness requirements.

For its intended use case - terminal-based live-coding of music DSLs - psnd delivers a polished, practical tool that fills a gap in the music programming ecosystem.

---

*Review generated: 2026-01-15*
*Project version: 0.1.1 (unreleased)*
*Reviewer: Claude Code*
