# Shared Audio Backend Refactor Plan

## Overview

Extract a language-agnostic audio/MIDI backend layer from the current Alda-specific implementation. This allows Joy (and future languages) to use TinySoundFont, Csound, Ableton Link, and MIDI output without duplicating code.

## Current Architecture Problems

1. **Alda-specific naming**: `AldaContext`, `alda_tsf_*`, `alda_csound_*` couples backends to one language
2. **Joy has separate MIDI**: `joy_midi_backend.c` duplicates libremidi integration
3. **Link requires editor context**: `loki_link_*` functions take `editor_ctx_t*` but use global state
4. **No shared abstraction**: Each language must implement its own routing logic

## Proposed Architecture

```
src/
  shared/                   # NEW: Language-agnostic shared layer
    audio/                  # Audio synthesis backends
      audio.h               # Public API for audio backends
      audio.c               # Backend selection and routing
      tsf_backend.c         # TinySoundFont synthesizer
      csound_backend.c      # Csound synthesis (optional)
    midi/                   # MIDI I/O
      midi.h                # Public API for MIDI
      midi.c                # Port management, message sending
      midi_context.h        # MidiContext struct
    link/                   # Ableton Link tempo sync
      link.h                # Public API for Link
      link.c                # Link integration
    context.h               # Shared AudioContext (combines all)
    context.c               # Context lifecycle
  alda/                     # Alda-specific code
    ...                     # Parser, interpreter (unchanged)
  joy/                      # Joy-specific code
    ...                     # Runtime, primitives (unchanged)
    midi_primitives.c       # Modified to use shared backend
  loki/
    alda.c                  # Uses shared context
    joy.c                   # Uses shared context
```

## Core Abstraction: SharedContext

```c
// src/shared/context.h

typedef struct SharedContext {
    /* Audio backend state */
    int tsf_enabled;
    int csound_enabled;
    int link_enabled;

    /* MIDI output (libremidi) */
    libremidi_midi_out_handle* midi_out;
    libremidi_midi_observer_handle* observer;

    /* Port enumeration */
    libremidi_midi_out_port* out_ports[64];
    int out_port_count;

    /* Global tempo (BPM) - Link can override */
    int tempo;

    /* Optional: microtuning scale */
    void* scale;  /* Scala scale if loaded */
} SharedContext;

/* Lifecycle - src/shared/context.c */
int shared_context_init(SharedContext* ctx);
void shared_context_cleanup(SharedContext* ctx);

/* Event dispatch - routes to active backend */
void shared_send_note_on(SharedContext* ctx, int channel, int pitch, int velocity);
void shared_send_note_off(SharedContext* ctx, int channel, int pitch);
void shared_send_note_on_freq(SharedContext* ctx, int channel, double freq, int velocity, int midi_pitch);
void shared_send_program(SharedContext* ctx, int channel, int program);
void shared_send_cc(SharedContext* ctx, int channel, int cc, int value);
void shared_send_panic(SharedContext* ctx);

/* MIDI port management - src/shared/midi/midi.h */
int shared_midi_open_port(SharedContext* ctx, int port_idx);
int shared_midi_open_virtual(SharedContext* ctx, const char* name);
void shared_midi_close(SharedContext* ctx);
void shared_midi_list_ports(SharedContext* ctx);

/* Audio backend control - src/shared/audio/audio.h */
int shared_audio_tsf_load(const char* soundfont_path);
void shared_audio_tsf_enable(SharedContext* ctx, int enable);
int shared_audio_csound_load(const char* csd_path);
void shared_audio_csound_enable(SharedContext* ctx, int enable);

/* Link integration - src/shared/link/link.h */
int shared_link_init(double initial_bpm);
void shared_link_cleanup(void);
void shared_link_enable(int enable);
double shared_link_get_tempo(void);
double shared_link_effective_tempo(SharedContext* ctx);  /* Returns Link tempo if enabled, else ctx->tempo */

/* Timing utilities */
int shared_ticks_to_ms(int ticks, int tempo);
void shared_sleep_ms(int ms);
```

## Implementation Steps

### Step 1: Create src/shared/ directory structure

```bash
mkdir -p src/shared/audio src/shared/midi src/shared/link
```

Create header files:
- `src/shared/context.h` - SharedContext struct
- `src/shared/audio/audio.h` - Audio backend API
- `src/shared/midi/midi.h` - MIDI I/O API
- `src/shared/link/link.h` - Ableton Link API

### Step 2: Move and rename backend files

| From | To | Changes |
|------|-----|---------|
| `src/alda/tsf_backend.c` | `src/shared/audio/tsf_backend.c` | Rename `alda_tsf_*` to `shared_audio_tsf_*` |
| `src/alda/csound_backend.c` | `src/shared/audio/csound_backend.c` | Rename `alda_csound_*` to `shared_audio_csound_*` |
| `src/alda/midi_backend.c` | `src/shared/midi/midi.c` | Extract MIDI-only parts, rename to `shared_midi_*` |
| `src/loki/link.c` | `src/shared/link/link.c` | Remove editor_ctx dependency, rename to `shared_link_*` |

### Step 3: Create src/shared/context.c

Implement context lifecycle and priority routing:

```c
// src/shared/context.c

void shared_send_note_on(SharedContext* ctx, int ch, int pitch, int vel) {
    /* Priority 1: Csound */
    if (ctx->csound_enabled && shared_audio_csound_is_enabled()) {
        shared_audio_csound_send_note_on(ch, pitch, vel);
        return;
    }
    /* Priority 2: TSF */
    if (ctx->tsf_enabled && shared_audio_tsf_is_enabled()) {
        shared_audio_tsf_send_note_on(ch, pitch, vel);
        return;
    }
    /* Priority 3: MIDI */
    if (ctx->midi_out) {
        shared_midi_send_note_on(ctx, ch, pitch, vel);
    }
}
```

### Step 4: Update Alda to use SharedContext

Modify `src/alda/` to use the shared layer:
- `AldaContext` keeps parser/interpreter state
- Add `SharedContext* shared` field to `AldaContext`
- Replace `alda_midi_send_*` calls with `shared_send_*`

```c
// In interpreter.c or wherever events are dispatched
shared_send_note_on(ctx->shared, channel, pitch, velocity);
```

### Step 5: Update Joy to use SharedContext

Modify `src/joy/midi_primitives.c`:
- Remove direct libremidi calls
- Add `SharedContext*` to `JoyContext`
- Route through shared backend

```c
// Before:
joy_midi_note_on(pitch, velocity);  // Direct libremidi

// After:
shared_send_note_on(ctx->shared, ctx->channel, pitch, velocity);
```

### Step 6: Update Loki bridges

Modify `src/loki/alda.c` and `src/loki/joy.c`:
- Initialize shared `SharedContext` in editor context
- Both languages share the same context when in editor
- REPL mode creates standalone `SharedContext`

```c
// In editor_ctx_t (internal.h)
typedef struct editor_ctx {
    // ... existing fields
    SharedContext* shared;  // Shared audio/MIDI/Link context
    LokiAldaState* alda_state;
    LokiJoyState* joy_state;
} editor_ctx_t;
```

### Step 7: Update CMakeLists.txt

```cmake
# Shared library (audio, MIDI, Link)
set(SHARED_SOURCES
    src/shared/context.c
    src/shared/audio/audio.c
    src/shared/audio/tsf_backend.c
    src/shared/midi/midi.c
    src/shared/link/link.c
)

if(BUILD_CSOUND_BACKEND)
    list(APPEND SHARED_SOURCES src/shared/audio/csound_backend.c)
endif()

add_library(shared STATIC ${SHARED_SOURCES})
target_include_directories(shared PUBLIC ${CMAKE_SOURCE_DIR}/src/shared)
target_link_libraries(shared libremidi miniaudio abl_link)

# Link shared to both alda and loki
target_link_libraries(alda shared)
target_link_libraries(libloki shared)
```

### Step 8: Update Joy REPL

Modify `src/repl.c` (joy_repl_main):
- Create `SharedContext` instead of using `joy_midi_*` directly
- Pass to Joy context for playback

## Files to Create

| File | Purpose |
|------|---------|
| `src/shared/context.h` | SharedContext struct and main API |
| `src/shared/context.c` | Context lifecycle and event routing |
| `src/shared/audio/audio.h` | Audio backend public API |
| `src/shared/audio/audio.c` | Audio backend selection |
| `src/shared/midi/midi.h` | MIDI I/O public API |
| `src/shared/link/link.h` | Ableton Link public API |

## Files to Move/Rename

| From | To |
|------|-----|
| `src/alda/tsf_backend.c` | `src/shared/audio/tsf_backend.c` |
| `src/alda/csound_backend.c` | `src/shared/audio/csound_backend.c` |
| `src/alda/midi_backend.c` | `src/shared/midi/midi.c` |
| `src/loki/link.c` | `src/shared/link/link.c` |
| `include/alda/tsf_backend.h` | `src/shared/audio/tsf_backend.h` |
| `include/loki/link.h` | `src/shared/link/link.h` |

## Files to Modify

| File | Changes |
|------|---------|
| `src/alda/async.c` | Use `shared_send_*` instead of direct backend calls |
| `src/alda/context.c` | Add `SharedContext*` field to AldaContext |
| `include/alda/alda.h` | Add `SharedContext*` to AldaContext struct |
| `src/joy/midi_primitives.c` | Use `shared_send_*` instead of `joy_midi_*` |
| `src/loki/alda.c` | Initialize SharedContext, pass to Alda |
| `src/loki/joy.c` | Use SharedContext from editor or create new |
| `src/loki/internal.h` | Add `SharedContext*` to editor_ctx_t |
| `src/repl.c` | Create SharedContext for REPL modes |
| `CMakeLists.txt` | Add shared library, update dependencies |

## Files to Delete

| File | Reason |
|------|--------|
| `src/joy/joy_midi_backend.c` | Replaced by `src/shared/midi/midi.c` |
| `src/joy/joy_midi_backend.h` | Replaced by `src/shared/context.h` |

## Verification

1. **Build**: `make clean && make` - should compile without errors
2. **Tests**: `make test` - all 25 tests pass
3. **Alda REPL**: `psnd` then `piano: c d e f g` - plays via TSF/MIDI
4. **Joy REPL**: `psnd joy` then `[c d e] play` - plays via same backends
5. **Alda + TSF**: `psnd -sf gm.sf2 song.alda` - TinySoundFont works
6. **Joy + TSF**: `psnd joy -sf gm.sf2` then `[c d e] play` - TSF works for Joy
7. **Link**: Enable Link in Alda, verify Joy also uses Link tempo
8. **Editor**: Open .joy file, Ctrl-E plays through shared backend

## Migration Strategy

1. Create `src/audio/` with new files first (additive)
2. Keep old code working while building new layer
3. Update Alda to use new layer, verify tests pass
4. Update Joy to use new layer, verify tests pass
5. Remove old `joy_midi_backend.c`
6. Clean up any remaining `alda_*` naming in audio layer

## Risk Assessment

- **Medium risk**: Significant refactor touching many files
- **Mitigation**: Incremental migration with tests at each step
- **Rollback**: Git allows easy revert if issues arise
