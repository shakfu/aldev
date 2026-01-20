# MHS - Micro Haskell MIDI Integration

MHS (Micro Haskell) is a lightweight Haskell implementation integrated into psnd for music programming. It provides a fast-starting, self-contained Haskell environment with MIDI support.

## Overview

The MHS integration supports two modes:

1. **psnd Integration** - MHS embedded into the psnd binary with VFS (Virtual File System)
2. **Standalone Binaries** - Independent mhs-midi executables for development/testing

## Quick Start

```bash
# Start interactive REPL
psnd mhs

# Run a Haskell file
psnd mhs -r myfile.hs

# Compile to executable
psnd mhs -oMyProg myfile.hs

# Show help
psnd mhs --help
```

## Available Modules

The following MIDI modules are available:

| Module | Description |
|--------|-------------|
| `Midi` | Low-level MIDI I/O (ports, note on/off, control change) |
| `Music` | High-level music notation (notes, chords, sequences) |
| `MusicPerform` | Music performance/playback |
| `MidiPerform` | MIDI event scheduling |
| `Async` | Asynchronous operations |

## Build Configuration

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `ENABLE_MHS_INTEGRATION` | ON | Enable MHS in psnd binary |
| `MHS_EMBED_MODE` | PKG_ZSTD | Embedding mode (see below) |
| `MHS_ENABLE_COMPILATION` | ON | Enable -o compilation support |
| `BUILD_MHS_STANDALONE` | ON | Build standalone mhs-midi executables |
| `BUILD_MHS_PKG_VARIANTS` | OFF | Build package-based standalone variants |

### Embedding Modes (MHS_EMBED_MODE)

| Mode | Startup | Binary Size | Description |
|------|---------|-------------|-------------|
| `PKG_ZSTD` | ~2s | ~5.7MB | Precompiled packages + zstd (default) |
| `PKG` | ~2s | ~6MB | Precompiled packages, no compression |
| `SRC_ZSTD` | ~17s | ~4MB | Compressed .hs sources |
| `SRC` | ~17s | ~5MB | Uncompressed .hs sources |

### Compilation Support (MHS_ENABLE_COMPILATION)

When `MHS_ENABLE_COMPILATION=ON` (default):
- `psnd mhs -oMyProg file.hs` compiles to standalone executable
- Binary embeds libremidi.a (~1.3MB compressed)
- Full MHS functionality

When `MHS_ENABLE_COMPILATION=OFF`:
- `psnd mhs -oMyProg file.hs` will fail (only .c output works)
- Binary ~1.3MB smaller (no libremidi)
- REPL and -r modes work normally

### Makefile Targets

The easiest way to build different MHS variants:

| Target | Binary Size | MHS Startup | Compilation | Description |
|--------|-------------|-------------|-------------|-------------|
| `make` | ~5.7MB | ~2s | Yes | Full MHS with PKG_ZSTD + compilation |
| `make mhs-small` | ~4.5MB | ~2s | No | No `-o` executable support |
| `make mhs-src` | ~4.1MB | ~17s | Yes | Source embedding (slower startup) |
| `make mhs-src-small` | ~2.9MB | ~17s | No | Smallest with MHS |
| `make no-mhs` | ~2.1MB | N/A | N/A | MHS disabled entirely |

### CMake Commands

For more control, use CMake directly:

```bash
# Standard psnd build (PKG_ZSTD + compilation)
cmake ..
make

# Smaller binary without compilation support
cmake -DMHS_ENABLE_COMPILATION=OFF ..
make

# Use source embedding (smaller but slower startup)
cmake -DMHS_EMBED_MODE=SRC_ZSTD ..
make

# Smallest binary (source mode, no compilation)
cmake -DMHS_EMBED_MODE=SRC_ZSTD -DMHS_ENABLE_COMPILATION=OFF ..
make

# Build standalone mhs-midi variants
cmake -DBUILD_MHS_PKG_VARIANTS=ON ..
make mhs-midi-all

# Disable MHS integration entirely
cmake -DENABLE_MHS_INTEGRATION=OFF ..
make
```

## Build Variants

### psnd Integration (Recommended)

When built with `ENABLE_MHS_INTEGRATION=ON` (default), MHS is embedded into the psnd binary using VFS with zstd compression:

- **Binary size**: ~5.7 MB (includes all MHS libraries)
- **Startup time**: ~2 seconds
- **Self-contained**: No external dependencies required

The psnd binary uses precompiled `.pkg` files with zstd compression for optimal startup time.

### Standalone Variants

For development and testing, standalone binaries can be built:

| Variant | Binary Size | Startup Time | Description |
|---------|-------------|--------------|-------------|
| `mhs-midi` | ~1.0 MB | ~17s | Basic REPL (requires MHSDIR) |
| `mhs-midi-src` | ~2.4 MB | ~17s | Embedded .hs source files |
| `mhs-midi-src-zstd` | ~1.5 MB | ~17s | Compressed .hs sources |
| `mhs-midi-pkg` | ~3.7 MB | ~2s | Precompiled .pkg files |
| `mhs-midi-pkg-zstd` | ~3.1 MB | ~2s | Compressed .pkg files |

**Note**: Package variants (`-pkg`, `-pkg-zstd`) require `BUILD_MHS_PKG_VARIANTS=ON`.

## Directory Structure

```
source/langs/mhs/
  CMakeLists.txt           # Build configuration
  README.md                # This file

  # psnd Integration
  dispatch.c               # CLI command registration
  register.c               # Editor language registration
  repl.c                   # REPL/run/compile entry points
  mhs_context.c/h          # State management for editor integration

  # MIDI FFI Layer
  midi_ffi.c/h             # Haskell FFI bindings for MIDI
  midi_ffi_wrappers.c      # xffi_table for FFI dispatch
  mhs_ffi_override.c       # VFS-aware file operations

  # Virtual File System
  vfs.c/h                  # VFS implementation (serves embedded content)

  # Standalone Entry Points
  mhs_midi_main.c          # Basic REPL (requires MHSDIR)
  mhs_midi_standalone_main.c  # Self-contained with VFS

  # Haskell Libraries
  lib/
    Midi.hs                # MIDI I/O
    Music.hs               # Music notation
    MusicPerform.hs        # Music playback
    MidiPerform.hs         # MIDI scheduling
    Async.hs               # Async operations
    TestMusic.hs           # Test utilities

  # Example Programs
  examples/
    HelloMidi.hs           # Basic MIDI example
    Chords.hs              # Chord progressions
    Melody.hs              # Melody generation
    Arpeggio.hs            # Arpeggios
    ListPorts.hs           # List MIDI ports
    AsyncTest.hs           # Async testing

  # Build Scripts
  scripts/
    mhs-embed.c            # C tool for embedding files
    mhs-embed.py           # Python embedding tool
    mhs-patch-eval.py      # Patch eval.c for VFS
    mhs-patch-xffi.py      # Remove xffi_table from mhs.c
    mhs-pkg.sh             # Build music.pkg

  # MicroHs Runtime (copied from thirdparty)
  impl/
    mhs.c                  # MicroHs compiler (generated)
    eval.c                 # Evaluator
    mhsffi.h               # FFI header
    config.h               # Platform config
```

## VFS (Virtual File System)

The VFS allows embedded files to be served from memory without extracting to disk:

### Compile Definitions

| Definition | Description |
|------------|-------------|
| `VFS_USE_PKG` | Use precompiled .pkg files instead of .hs sources |
| `VFS_USE_ZSTD` | Enable zstd decompression |
| `VFS_DEBUG` | Enable debug logging |

### VFS Modes

```c
// Uncompressed .hs sources (default)
// Includes: mhs_embedded_libs.h

// Compressed .hs sources
#define VFS_USE_ZSTD
// Includes: mhs_embedded_zstd.h

// Precompiled packages
#define VFS_USE_PKG
// Includes: mhs_embedded_pkgs.h

// Compressed packages (psnd default)
#define VFS_USE_PKG
#define VFS_USE_ZSTD
// Includes: mhs_embedded_pkgs_zstd.h
```

### How VFS Works

1. `vfs_init()` initializes decompression context and dictionary
2. `mhs_fopen()` (in mhs_ffi_override.c) intercepts file opens
3. Paths starting with `/mhs-embedded` are served from embedded content
4. Other paths fall through to regular filesystem

## Compilation to Executable

When compiling Haskell to a standalone executable, the C compiler needs real files:

```bash
# Output C code only (uses VFS)
psnd mhs -oMyProg.c myfile.hs

# Compile to executable (extracts to temp dir)
psnd mhs -oMyProg myfile.hs
```

For executable output, repl.c:
1. Detects `-o<name>` where name doesn't end in `.c`
2. Extracts VFS contents to a temp directory
3. Sets MHSDIR to the temp directory
4. Adds `-optl` linker flags for MIDI libraries
5. Cleans up temp directory after compilation

### Linker Libraries

When compiling to executable, the following are linked:

**macOS:**
- `libmidi_ffi.a`, `libmusic_theory.a`, `liblibremidi.a`
- Frameworks: CoreMIDI, CoreFoundation, CoreAudio
- `-lc++` (for libremidi C++ runtime)

**Linux:**
- `libmidi_ffi.a`, `libmusic_theory.a`, `liblibremidi.a`
- `-lasound` (ALSA)
- `-lstdc++`, `-lm`

## Package System

MHS uses precompiled `.pkg` files for fast startup:

### Building Packages

```bash
# Build base.pkg (MicroHs standard library)
cd source/thirdparty/MicroHs/lib
mcabal build

# Build music.pkg (MIDI library)
mhs -Pmusic-0.1.0 -ilib Async Midi MidiPerform Music MusicPerform -o music-0.1.0.pkg
```

### Package Loading

In package mode, MHS uses:
- `-a/mhs-embedded` - Package archive search path
- `-pbase` - Preload base package
- `-pmusic` - Preload music package

## Symbol Conflicts

The MHS runtime has symbol conflicts with other psnd components:

| Original | Renamed To | Reason |
|----------|------------|--------|
| `mmalloc` | `mhs_mmalloc` | Conflicts with Csound |
| `midi_init` | `mhs_midi_init` | Conflicts with Joy MIDI |
| `xffi_table` | (removed) | Provided by midi_ffi_wrappers.c |
| `mhs_fopen` | `mhs_fopen_orig` | Overridden for VFS |
| `mhs_opendir` | `mhs_opendir_orig` | Overridden for VFS |

These renames are handled by:
- `scripts/mhs-patch-xffi.py` - Removes xffi_table
- `scripts/mhs-patch-eval.py` - Renames file operations for VFS override

## Example: Hello MIDI

```haskell
-- HelloMidi.hs
import Midi

main :: IO ()
main = do
    midiInit
    n <- midiListPorts
    putStrLn $ "Found " ++ show n ++ " MIDI ports"

    -- Open first port (or virtual port)
    if n > 0
        then midiOpenPort 0
        else midiOpenVirtual "MHS-MIDI"

    -- Play middle C
    midiNoteOn 0 60 100
    midiSleep 500
    midiNoteOff 0 60

    midiCleanup
```

Run with:
```bash
psnd mhs -r HelloMidi.hs
```

## Troubleshooting

### "Package not found: base"
Ensure `-a/mhs-embedded` is passed before `-pbase`. The archive path must be set before loading packages.

### "Module not found: Prelude"
Check that VFS is initialized and MHSDIR is set correctly. Enable `VFS_DEBUG=1` for logging.

### Compilation fails with "no such file"
For executable output, files must be extracted to temp. Check that `vfs_extract_to_temp()` succeeds.

### Slow startup (~17s)
You're using source mode instead of package mode. Ensure `VFS_USE_PKG` is defined and packages are embedded.

## Dependencies

- **MicroHs**: `source/thirdparty/MicroHs/` - Haskell compiler/runtime
- **zstd**: `source/thirdparty/zstd-1.5.7/` - Compression (optional)
- **libremidi**: MIDI I/O library
- **Python 3**: For build scripts

## License

MHS integration follows the psnd project license. MicroHs is BSD-licensed.
