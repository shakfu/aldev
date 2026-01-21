# Language Implementation Comparison

This document compares the feature completeness of each language implementation in psnd.

## Feature Matrix

| Feature | Alda | Joy | TR7 | Bog | MHS |
|---------|:----:|:---:|:---:|:---:|:---:|
| **Editor Integration** |  |  |  |  |  |
| Syntax highlighting | Yes | Yes | Yes | Yes | Yes |
| Ctrl-E (eval line) | Yes | Yes | Yes | Yes | Yes |
| Ctrl-P (play file) | Yes | Yes | Yes | Yes | Yes |
| Ctrl-G (stop) | Yes | Yes | Yes | Yes | Yes |
| Lua API (`loki.<lang>.*`) | Yes | Yes | No | Yes | Yes |
| **REPL Features** |  |  |  |  |  |
| Custom REPL loop | Yes | Yes | Yes | Yes | Yes* |
| Syntax highlighting in input | Yes | Yes | Yes | Yes | Yes |
| Shared commands (`:help`, `:list`, etc.) | Yes | Yes | Yes | Yes | Yes |
| Tab completion | Yes | Yes | Yes | Yes | Yes |
| History persistence | Yes | Yes | Yes | Yes | Yes |
| Piped input support | Yes | Yes | Yes | Yes | Yes** |
| Language-specific help | Yes | Yes | Yes | Yes | Yes |
| **CLI Flags** |  |  |  |  |  |
| `--virtual NAME` | Yes | Yes | Yes | Yes | Yes |
| `-sf PATH` (soundfont) | Yes | Yes | Yes | Yes | Yes |
| `-p N` (port select) | Yes | Yes | Yes | Yes | Yes |
| `-v` (verbose) | Yes | Yes | Yes | Yes | Yes |
| `-l` (list ports) | Yes | Yes | Yes | Yes | Yes |
| **Backend Integration** |  |  |  |  |  |
| Ableton Link callbacks | Yes | Yes | Yes | Yes | Yes |
| Async playback | Yes | Yes | Yes | Yes | Partial |
| SharedContext usage | Yes | Yes | Yes | Yes | Yes |

\* MHS uses stdin pipe interposition - psnd's REPL runs in main thread, MicroHs in background thread.
\** Piped input passes directly to MicroHs (no psnd command processing).

## Legend

- **Yes**: Feature is fully implemented
- **No**: Feature is missing
- **Partial**: Feature exists but with limitations

## Architecture Notes

### Alda

- Full-featured reference implementation
- Custom parser, interpreter, and async scheduler
- All REPL and editor features implemented

### Joy

- Stack-based concatenative language
- Full REPL with `repl_readline()` and shared commands
- Complete editor integration with Lua API

### TR7

- R7RS-small Scheme interpreter
- Full REPL features but no editor Lua API
- Music primitives added as Scheme procedures

### Bog

- Prolog-based pattern language
- Full REPL with slot management (`:def`, `:undef`, `:slots`)
- Syntax highlighting for predicates, voices, scales, and chords

### MHS (Micro Haskell)

- Wraps MicroHs interpreter with stdin pipe interposition
- Has editor integration (Lua API)
- Syntax highlighting for Haskell keywords, types, and MIDI primitives
- CLI flags for MIDI setup (`--virtual`, `-sf`, `-p`, `-l`, `-v`)
- Routes MIDI through SharedContext for TSF/Csound/Link support
- Interactive REPL uses psnd's readline with syntax highlighting, completion, history
- MicroHs runs in background thread, receives input via pipe
- Full shared command support (`:help`, `:stop`, `:panic`, etc.)

## Implementation Details

### REPL Loop Comparison

**Joy/TR7/Bog pattern** (full featured):

```c
// Initialize editor context for syntax highlighting
editor_ctx_t ed;
editor_ctx_init(&ed);
syntax_init_default_colors(&ed);

// Set up tab completion
repl_set_completion(ed, completion_callback, user_data);

// Load history
repl_history_load(&ed, history_path);

// Main loop with syntax highlighting
repl_enable_raw_mode();
while ((input = repl_readline(syntax_ctx, &ed, "prompt> "))) {
    if (shared_process_command(input, ctx)) continue;
    // Process language-specific input
    ...
    shared_repl_link_check(ctx);  // Link callbacks
}
repl_disable_raw_mode();
repl_history_save(&ed, history_path);
```

**MHS pattern** (PTY-based stdin interposition):

```c
// Create PTY and fork child process
int master_fd;
pid_t child = forkpty(&master_fd, NULL, NULL, NULL);

if (child == 0) {
    // Child process: initialize MIDI after fork (handles don't survive fork)
    setup_midi_for_repl(args);
    // Run MicroHs with PTY as terminal
    mhs_main(argc, argv);
    _exit(0);
}

// Parent process: psnd REPL loop
while (g_mhs_running) {
    input = repl_readline(syntax_ctx, &ed, "mhs> ");
    if (shared_process_command(ctx, input, ...)) continue;
    // Forward to MicroHs via PTY
    write(master_fd, input, strlen(input));
    write(master_fd, "\n", 1);
    mhs_read_output();  // Read and display MicroHs output
    shared_repl_link_check();
}
```

### Shared REPL Commands

All languages support these commands via `shared_process_command()`:

| Command | Description |
|---------|-------------|
| `:q` `:quit` `:exit` | Exit REPL |
| `:h` `:help` `:?` | Show help |
| `:l` `:list` | List MIDI ports |
| `:s` `:stop` | Stop playback |
| `:p` `:panic` | All notes off |
| `:sf PATH` | Load soundfont |
| `:synth` `:builtin` | Switch to built-in synth |
| `:midi` | Switch to MIDI output |
| `:virtual [NAME]` | Create virtual MIDI port |
| `:link [on\|off]` | Toggle Ableton Link |
| `:link-tempo BPM` | Set Link tempo |
| `:link-status` | Show Link status |
| `:cs PATH` | Load CSD file |
| `:csound` | Enable Csound backend |

### CLI Flag Parsing

Joy/TR7/Bog use `SharedReplArgs` structure via `shared_lang_repl_main()`:

```c
typedef struct {
    const char* soundfont_path;
    const char* virtual_name;
    const char* csound_path;
    int port_index;
    bool verbose;
    // ...
} SharedReplArgs;
```

MHS uses its own `MhsReplArgs` structure with similar fields:

```c
typedef struct {
    const char *virtual_name;
    const char *soundfont_path;
    int port_index;
    int list_ports;
    int verbose;
    int show_help;
    int mhs_argc;      /* Remaining args for MicroHs */
    char **mhs_argv;
} MhsReplArgs;
```

MHS separates psnd flags from MicroHs flags, initializes SharedContext
and MIDI based on the psnd flags, then passes remaining flags to MicroHs.

## MHS Implementation Status

All major features have been implemented:

1. **Completed**
   - ~~Add syntax highlighting (`lang_haskell.h`)~~ **DONE**
   - ~~Add CLI flags (`--virtual`, `-sf`, `-p`, `-l`, `-v`)~~ **DONE**
   - ~~SharedContext integration for TSF/Csound/Link~~ **DONE**
   - ~~Implement custom REPL loop with `repl_readline()`~~ **DONE** (PTY-based interposition)
   - ~~Add shared command processing~~ **DONE**
   - ~~Add tab completion for Haskell keywords~~ **DONE** (80+ keywords)
   - ~~Add history persistence~~ **DONE** (`~/.psnd/mhs_history`)
   - ~~Integrate Ableton Link callbacks~~ **DONE**
   - ~~Add language-specific help text~~ **DONE**

2. **Remaining Limitations**
   - Piped input (non-TTY) passes directly to MicroHs without psnd command processing
   - MicroHs commands (`:type`, `:kind`, `:browse`) work but aren't intercepted by psnd

## MHS REPL Architecture

### Background

MicroHs's REPL is fundamentally different from Joy/TR7/Bog:

| Aspect | Joy/TR7/Bog | MHS (MicroHs) |
|--------|-------------|---------------|
| REPL Implementation | C code | Haskell (`Interactive.hs`) |
| Input Method | `repl_readline()` (line-based) | `GETRAW` FFI (char-by-char) |
| Line Editing | psnd's editor API | `SimpleReadline.hs` in Haskell |
| Command Processing | C `shared_process_command()` | Haskell pattern matching |

### How MicroHs REPL Works

```text
+---------------+       +------------------+       +-------------+
| User Terminal | <---> | SimpleReadline.hs| <---> | GETRAW (C)  |
+---------------+       | (Haskell)        |       | unix/extra.c|
                        +------------------+       +-------------+
                               |
                               v
                        +------------------+
                        | Interactive.hs   |
                        | (Haskell REPL)   |
                        +------------------+
```

1. `GETRAW` (C function) reads single characters in raw terminal mode
2. `SimpleReadline.hs` accumulates characters, handles backspace/arrow keys
3. `Interactive.hs` receives complete lines and evaluates them

### Options Analyzed

#### Option A: Stdin Pipe Interposition (Attempted, Failed)

Replace stdin with a pipe before calling `mhs_main()`:

```c
int pipefd[2];
pipe(pipefd);

// Child thread: run MicroHs with stdin = pipefd[0]
dup2(pipefd[0], STDIN_FILENO);
mhs_main(argc, argv);

// Parent: read lines with repl_readline(), filter commands
while ((line = repl_readline(...))) {
    if (shared_process_command(ctx, line, ...)) continue;
    write(pipefd[1], line, strlen(line));
    write(pipefd[1], "\n", 1);
}
```

**Why this doesn't work:**

- MicroHs's `SimpleReadline.hs` calls `tcgetattr()` to configure terminal mode
- Pipes aren't terminals, so `tcgetattr()` fails with errno 25 (ENOTTY)
- Results in "tcgetattr failed: errno=25" and "getRaw failed" errors
- MicroHs crashes instead of starting the REPL

#### Option B: Custom GETRAW Function

Override `GETRAW` macro before including `extra.c`:

```c
#define GETRAW custom_getraw
int custom_getraw(void) {
    // Buffer characters until newline, intercept psnd commands
    // Problem: GETRAW returns ONE character at a time
}
```

**Why this doesn't work:**

- MicroHs expects immediate character returns
- Buffering breaks the character-by-character contract
- Would need to intercept at line level, not char level

#### Option C: PTY Wrapper (Implemented)

Run MicroHs in a pseudo-terminal using `forkpty()`:

```c
int master_fd;
pid_t child = forkpty(&master_fd, NULL, NULL, NULL);
if (child == 0) {
    // Child: MicroHs runs with PTY as its terminal
    setup_midi_for_repl(args);  // MIDI after fork!
    mhs_main(argc, argv);
    _exit(0);
}
// Parent: intercepts I/O through master fd
```

**Pros:** Full terminal emulation, MicroHs's tcgetattr works on PTY
**Cons:** Requires fork (not threads), MIDI must be initialized after fork

#### Option D: Modify MicroHs Haskell Code

Add FFI bindings to psnd's readline in `SimpleReadline.hs`:

```haskell
foreign import ccall "psnd_readline" c_readline :: CString -> IO CString
```

**Pros:** Clean integration, full feature support
**Cons:** Invasive, creates maintenance burden with upstream MicroHs

### Implemented Approach: PTY-based Interposition

The MHS REPL uses Option C (PTY wrapper), implemented in `source/langs/mhs/repl.c`:

1. Detects interactive mode via `isatty(STDIN_FILENO)`
2. Creates PTY and forks child process using `forkpty()`
3. Child process initializes MIDI after fork (libremidi handles don't survive fork)
4. Child runs MicroHs with the PTY slave as its terminal
5. Parent runs psnd's `repl_readline()` for syntax-highlighted input
6. Processes psnd commands locally via `shared_process_command()`
7. Forwards non-commands to MicroHs via PTY master fd
8. Non-blocking read from PTY to display MicroHs output

**Why PTY instead of pipe:** MicroHs's `SimpleReadline.hs` uses `tcgetattr()` for
terminal mode configuration. Pipes fail this check (errno 25: ENOTTY), causing
"tcgetattr failed" and "getRaw failed" errors. A PTY provides a real terminal
that satisfies these requirements.

**Why MIDI after fork:** libremidi handles use internal state that doesn't survive
`fork()`. Initializing MIDI before fork causes the child's MIDI operations to
silently fail. Moving MIDI initialization to the child process ensures working
MIDI output.

Features provided:

- Shared REPL commands (`:help`, `:stop`, `:panic`, `:list`, `:sf`, etc.)
- Syntax-highlighted input (Haskell keywords, types, MIDI primitives)
- Tab completion (80+ Haskell keywords and MIDI functions)
- History persistence (`~/.psnd/mhs_history`)
- Ableton Link callback polling

Trade-off: MicroHs's own line editing is bypassed, but psnd's
`repl_readline()` provides equivalent functionality.

## File Locations

| Language | REPL | Register | Dispatch |
|----------|------|----------|----------|
| Alda | `source/langs/alda/repl.c` | `source/langs/alda/register.c` | `source/langs/alda/dispatch.c` |
| Joy | `source/langs/joy/repl.c` | `source/langs/joy/register.c` | `source/langs/joy/dispatch.c` |
| TR7 | `source/langs/tr7/impl/repl.c` | `source/langs/tr7/impl/register.c` | `source/langs/tr7/dispatch.c` |
| Bog | `source/langs/bog/repl.c` | `source/langs/bog/register.c` | `source/langs/bog/dispatch.c` |
| MHS | `source/langs/mhs/repl.c` | `source/langs/mhs/register.c` | `source/langs/mhs/dispatch.c` |

Syntax definitions: `source/core/loki/syntax/lang_*.h`
