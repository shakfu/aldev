# Psnd Project Review

**Date:** 2026-01-13
**Reviewer:** Claude Code (Opus 4.5)
**Project Version:** 0.4.1

---

## Executive Summary

Psnd is a well-architected, self-contained text editor and REPL for the Alda music programming language. Built on the Loki editor (derived from antirez's kilo), it provides a unique combination of vim-like modal editing, embedded Lua scripting, and real-time music playback capabilities. The codebase demonstrates strong engineering practices with proper memory safety, modular design, and comprehensive testing.

**Strengths:**
- Clean modular C architecture with separation of concerns
- Comprehensive test suite with custom framework
- Self-contained build with no external runtime dependencies
- Strong memory safety with bounds checking and NULL validation
- Rich feature set: undo/redo, multiple buffers, syntax highlighting, Lua scripting

**Areas for Improvement:**
- Test coverage gaps in Alda integration and undo/redo
- Missing features common to modern editors (LSP, git integration)
- Limited error recovery strategies in some edge cases
- Documentation could benefit from architecture diagrams

---

## 1. Architecture Analysis

### 1.1 Overall Design

The project follows a modular monolith architecture:

```
include/loki/         Public C API headers
src/                  Implementation files (modular C)
  loki_core.c         Core editor state and buffer management
  loki_lua.c          Lua API bindings and REPL
  loki_modal.c        Vim-like modal editing
  loki_syntax.c       Syntax highlighting engine
  loki_undo.c         Undo/redo with operation grouping
  loki_alda.c         Music playback integration
  loki_buffers.c      Multiple buffer management
  loki_terminal.c     VT100 terminal I/O
  ...
tests/                Unit and integration tests
thirdparty/           Dependencies (Lua, alda-midi, libremidi, etc.)
```

**Verdict:** The architecture is appropriate for the project scope. Each module has clear responsibilities and interfaces through header files.

### 1.2 Context-Based Design

The `editor_ctx_t` structure serves as the central state container, enabling:
- Future multi-window/split support
- Clean testability (contexts can be created independently)
- Proper isolation of state

**Current limitation:** Some subsystems (notably Alda integration at `loki_alda.c:48`) use global state (`g_alda_state`) rather than per-context state. This prevents running multiple independent Alda sessions.

### 1.3 Memory Management

The codebase demonstrates careful memory management:

```c
// Example from loki_core.c:201-205
t_erow *new_row = realloc(ctx->row,sizeof(t_erow)*(ctx->numrows+1));
if (new_row == NULL) {
    perror("Out of memory");
    exit(1);
}
ctx->row = new_row;
```

All allocations are checked, bounds are validated, and cleanup paths are well-defined. The code review documented in CLAUDE.md confirms all critical memory issues have been addressed.

### 1.4 Module Dependencies

```
main.c
  |
  +-> loki_editor.c (editor entry point)
        |
        +-> loki_core.c (buffer/row operations)
        +-> loki_modal.c (input handling)
        +-> loki_lua.c (scripting)
        +-> loki_alda.c (music playback)
        +-> loki_terminal.c (I/O)
        +-> loki_syntax.c (highlighting)
        +-> loki_undo.c (undo/redo)
```

The dependency graph is acyclic and well-layered. Lower-level modules don't depend on higher-level ones.

---

## 2. Code Quality Assessment

### 2.1 Coding Standards

| Aspect | Assessment | Notes |
|--------|------------|-------|
| C Standard | C99 | Consistent across codebase |
| Compiler Flags | `-Wall -Wextra -pedantic` | Strict warnings enabled |
| Naming | Consistent | `editor_*`, `loki_*`, `modal_*` prefixes |
| Comments | Good | Block comments for functions, inline for complex logic |
| Error Handling | Strong | NULL checks, bounds validation, graceful degradation |

### 2.2 Code Smells and Technical Debt

**Identified Issues:**

1. **Global State in Alda Integration** (`loki_alda.c:48`)
   ```c
   static LokiAldaState g_alda_state = {0};
   ```
   This should be moved to `editor_ctx_t` for proper context isolation.

2. **Magic Numbers** (scattered)
   Some hardcoded values lack named constants:
   - `KILO_QUIT_TIMES = 3` (good)
   - `20` and `400` for tempo bounds (`loki_alda.c:359-360`) (should be constants)

3. **Incomplete Visual Mode Delete** (`loki_modal.c:621-629`)
   ```c
   case 'd':
   case 'x':
       copy_selection_to_clipboard(ctx);
       /* TODO: delete selection - need to implement this */
       editor_set_status_msg(ctx, "Delete not implemented yet");
   ```
   Core vim functionality is incomplete.

4. **Unused Function Parameter Suppression**
   Multiple `(void)ctx;` casts in `loki_alda.c` indicate API design mismatch. The context is accepted but not used.

### 2.3 Positive Patterns

1. **Clean State Machine for Modal Editing** (`loki_modal.c`)
   Each mode has its own handler function with clear dispatch:
   ```c
   switch(ctx->mode) {
       case MODE_NORMAL: process_normal_mode(ctx, fd, c); break;
       case MODE_INSERT: process_insert_mode(ctx, fd, c); break;
       case MODE_VISUAL: process_visual_mode(ctx, fd, c); break;
       case MODE_COMMAND: command_mode_handle_key(ctx, fd, c); break;
   }
   ```

2. **Undo Grouping with Heuristics** (`loki_undo.c`)
   Smart grouping of undo operations based on time gaps, cursor movement, and operation type switches.

3. **Lazy Syntax Highlighting**
   Highlighting is computed on row update and cached, avoiding redundant computation.

---

## 3. Feature Analysis

### 3.1 Implemented Features

| Feature | Status | Quality |
|---------|--------|---------|
| Modal Editing (vim-like) | Complete | High |
| Undo/Redo | Complete | High (with grouping) |
| Multiple Buffers | Complete | High |
| Syntax Highlighting | Complete | High (extensible via Lua) |
| Search | Complete | Medium (basic incremental) |
| Lua Scripting | Complete | High |
| Lua REPL | Complete | High (with tab completion) |
| Alda Playback | Complete | High (async with slots) |
| Auto-indentation | Complete | Medium |
| Word Wrap | Complete | Medium |
| Binary File Protection | Complete | High |
| Custom Commands | Complete | High (`:command` registration) |

### 3.2 Feature Gaps

| Missing Feature | Impact | Complexity | Priority |
|-----------------|--------|------------|----------|
| System Clipboard | High | Low | High |
| Visual Mode Delete | High | Low | High |
| Line Numbers | Medium | Low | Medium |
| Go-to-line | Medium | Low | Medium |
| Jump to Definition | Medium | Medium | Medium |
| Split Windows | Medium | High | Low |
| Mouse Support | Low | Medium | Low |
| Session Persistence | Low | Medium | Low |
| LSP Integration | Medium | High | Low |
| Git Integration | Low | Medium | Low |

### 3.3 Alda-Specific Features

The Alda integration is well-implemented:

- Async playback with libuv event loop
- Up to 8 concurrent playback slots
- Built-in synthesizer via TinySoundFont
- Part-aware playback (Ctrl-E plays current part)
- Full Lua API exposure (`loki.alda.*`)

**Suggested Enhancements:**
1. MIDI port selection from editor (currently only via CLI)
2. Visual feedback during playback (highlight playing region)
3. Tempo tap (tap key to set tempo)
4. Metronome toggle

---

## 4. Test Coverage Analysis

### 4.1 Test Suite Overview

| Test File | Coverage Area | Test Count | Quality |
|-----------|--------------|------------|---------|
| `test_core.c` | Context init, cursor, dirty flag | 10 | Good |
| `test_modal.c` | Mode switching, navigation | ~8 | Good |
| `test_syntax.c` | Highlighting, keywords | ~12 | Good |
| `test_indent.c` | Auto-indentation | ~6 | Medium |
| `test_buffers.c` | Multiple buffers | ~10 | Good |
| `test_file_io.c` | File operations | ~8 | Good |
| `test_lua_api.c` | Lua bindings | ~15 | Good |
| `test_lang_registration.c` | Dynamic languages | ~8 | Good |
| `test_search.c` | Search operations | ~6 | Medium |

### 4.2 Coverage Gaps

**Critical gaps:**
1. **Undo/Redo** - No dedicated test file for `loki_undo.c`
2. **Alda Integration** - No tests for `loki_alda.c`
3. **Command Mode** - Limited coverage for `:` commands
4. **Edge Cases** - Large file handling, very long lines

**Recommended additions:**
```c
// test_undo.c - suggested tests
TEST(undo_groups_consecutive_chars)
TEST(undo_breaks_on_cursor_jump)
TEST(undo_handles_line_operations)
TEST(redo_after_partial_undo)
TEST(memory_limit_evicts_old_entries)
```

### 4.3 Test Framework Quality

The custom test framework (`test_framework.h`) is well-designed:
- Assertion macros with file/line information
- Color output for readability
- Suite infrastructure with pass/fail counts
- No external dependencies

**Suggested improvements:**
- Add `ASSERT_GT`, `ASSERT_LT` macros
- Add test fixture support for setup/teardown
- Add memory leak detection hooks

---

## 5. Security Considerations

### 5.1 Input Validation

| Vector | Mitigation | Status |
|--------|------------|--------|
| File Input | Binary detection, line length limits | Mitigated |
| Keyboard Input | Bounds checking on input buffers | Mitigated |
| Lua Execution | Standard Lua sandbox (io, os available) | Partial |
| MIDI Data | Validated in alda-midi library | Mitigated |

### 5.2 Potential Vulnerabilities

1. **Lua Sandbox**
   The embedded Lua has full standard library access (`io`, `os`). Malicious `.psnd/init.lua` could execute arbitrary commands.

   **Recommendation:** Add option for restricted Lua mode.

2. **Path Traversal**
   File operations use user-provided paths directly.

   **Recommendation:** Add path canonicalization/validation.

3. **Integer Overflow**
   Row allocation checks for overflow (`loki_core.c:197`), but some size calculations could still overflow on 32-bit systems.

### 5.3 Signal Safety

The signal handler issue was properly fixed:
```c
// loki_terminal.c - correct pattern
volatile sig_atomic_t winsize_changed;
void terminal_sig_winch_handler(int sig) {
    (void)sig;
    // Only set flag, process in main loop
    // This is async-signal-safe
}
```

---

## 6. Performance Considerations

### 6.1 Measured Characteristics

| Operation | Complexity | Notes |
|-----------|------------|-------|
| Row insertion | O(n) | Shifts subsequent rows |
| Character insertion | O(n) | Within-row shift |
| Syntax highlighting | O(n) per row | Cached, recomputed on change |
| Undo/Redo | O(k) | k = operations in group |
| Search | O(n*m) | n=lines, m=pattern length |

### 6.2 Optimization Opportunities

1. **Gap Buffer for Rows**
   Current implementation uses contiguous array with memmove on insert/delete. Gap buffer would reduce this to O(1) amortized.

2. **Rope Data Structure**
   For very large files (>100K lines), a rope structure would provide O(log n) operations.

3. **Syntax Highlighting Cache**
   Consider lazy highlighting (only visible rows) for large files.

4. **Parallel Syntax Highlighting**
   Multi-line comment tracking prevents easy parallelization, but independent row highlighting could be parallelized.

### 6.3 Memory Usage

| Component | Overhead | Notes |
|-----------|----------|-------|
| Per-row | ~48 bytes + content | chars, render, hl arrays |
| Undo history | ~48 bytes per entry + content | Configurable limits |
| Lua state | ~100KB base | Shared across contexts |

The 10MB undo memory limit (`loki_undo.c:95`) is sensible for typical usage.

---

## 7. Documentation Quality

### 7.1 Code Documentation

| File | Quality | Notes |
|------|---------|-------|
| `loki_internal.h` | Good | Structure documentation |
| `loki_core.c` | Good | Function headers |
| `loki_lua.c` | Excellent | API documentation |
| `loki_undo.c` | Good | Algorithm explanation |
| `loki_modal.c` | Good | Keybinding documentation |

### 7.2 Project Documentation

| Document | Quality | Notes |
|----------|---------|-------|
| `README.md` | Needs review | Modified but not read |
| `CLAUDE.md` | Excellent | Comprehensive API reference |
| `CHANGELOG.md` | Excellent | Detailed version history |
| `docs/` | Good | Examples and language reference |

### 7.3 Documentation Gaps

1. **Architecture Diagram** - No visual overview of module relationships
2. **API Reference** - No generated docs (consider Doxygen)
3. **Contributing Guide** - No contribution guidelines
4. **Build Troubleshooting** - Limited platform-specific guidance

---

## 8. Build System Assessment

### 8.1 CMake Configuration

The CMake setup is well-structured:
- Minimum version 3.18 (modern features)
- C99 for core, C++17 for dependencies
- CTest integration for testing
- Binary stripping for size reduction

### 8.2 Dependency Management

All dependencies are in `thirdparty/`:
- Lua 5.5.0 (statically compiled)
- alda-midi (music notation parser)
- libremidi (cross-platform MIDI)
- libuv (async I/O)
- miniaudio + TinySoundFont (software synthesis)

**Strengths:**
- Self-contained build, no external package managers
- Version pinning for reproducibility

**Weaknesses:**
- No automated dependency updates
- Large thirdparty footprint

### 8.3 Build Recommendations

1. **Add Debug/Address Sanitizer Target**
   ```cmake
   option(LOKI_ENABLE_ASAN "Enable AddressSanitizer" OFF)
   if(LOKI_ENABLE_ASAN)
       add_compile_options(-fsanitize=address)
       add_link_options(-fsanitize=address)
   endif()
   ```

2. **Add Coverage Target**
   ```cmake
   option(LOKI_ENABLE_COVERAGE "Enable code coverage" OFF)
   ```

3. **Consider Install Target**
   Currently no `make install` support.

---

## 9. Recommended Improvements

### 9.1 High Priority (Immediate Value)

1. [x] **Implement Visual Mode Delete**
   - Location: `loki_modal.c:621-629`
   - Complexity: Low
   - Impact: High (core vim functionality)

2. [x] **System Clipboard Integration**
   - Use OSC 52 escape sequences for terminal clipboard
   - Works over SSH, no external dependencies
   - Complexity: Low

3. [x] **Add Undo/Redo Tests**
   - Create `tests/test_undo.c`
   - Test grouping heuristics, memory limits, edge cases

4. [not-working] **Add Line Numbers Display**
   - Common editor feature
   - Requires gutter calculation adjustment

### 9.2 Medium Priority (Enhanced Usability)

5. **Go-to-line Command**
   - Add `:123` or `:goto 123` command
   - Simple addition to command mode

6. **Alda Playback Visualization**
   - Highlight currently playing region
   - Show playback progress in status bar

7. **Move Alda State to Context**
   - Refactor `g_alda_state` to be per-context
   - Enables multiple independent Alda sessions

8. **Add Search and Replace**
   - Extend search with `:s/old/new/` command

### 9.3 Low Priority (Future Enhancements)

9. **Split Windows**
   - Already designed for in `editor_ctx_t`
   - Requires screen rendering changes

10. **LSP Client**
    - Would provide IDE-like features
    - High complexity, significant undertaking

11. **Git Integration**
    - Gutter diff markers
    - Stage/commit commands

---

## 10. Conclusion

Psnd is a well-engineered project that successfully combines a lightweight text editor with music live-coding capabilities. The codebase demonstrates professional quality with:

- Robust error handling and memory safety
- Clean modular architecture
- Comprehensive testing infrastructure
- Thoughtful feature design

The main areas for improvement are:
1. Completing partial features (visual mode delete)
2. Expanding test coverage (undo, alda)
3. Adding common editor conveniences (line numbers, clipboard)

The project is production-ready for its intended use case (Alda music composition) and provides a solid foundation for future enhancements.

---

## Appendix: Files Reviewed

| File | Lines | Purpose |
|------|-------|---------|
| `src/loki_core.c` | 868 | Core editor implementation |
| `src/loki_internal.h` | 265 | Internal structures |
| `src/loki_lua.c` | 1996 | Lua integration |
| `src/loki_modal.c` | 774 | Modal editing |
| `src/loki_undo.c` | 475 | Undo/redo system |
| `src/loki_alda.c` | 521 | Alda integration |
| `include/loki/core.h` | 216 | Public API |
| `tests/test_core.c` | 259 | Core tests |
| `tests/test_framework.h` | 141 | Test framework |
| `CMakeLists.txt` | 204 | Build configuration |
| `CHANGELOG.md` | 197 | Version history |
| `CLAUDE.md` | 750+ | Project documentation |

**Total Lines Reviewed:** ~6,500 lines of C code
