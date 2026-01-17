# Bog: Architecture Overview and MIDI Backend Guide

Bog is a C implementation of a Prolog-based live coding system for music. It allows defining musical events using logic programming, where patterns emerge from the declarative rules rather than imperative sequencing.

## Architecture Overview

### Core Components

```
+------------------+     +------------------+     +------------------+
|     Parser       | --> |    Resolution    | --> |    Scheduler     |
|  (bog.c)         |     |    (bog.c)       |     |  (scheduler.c)   |
+------------------+     +------------------+     +------------------+
        |                        |                        |
        v                        v                        v
+------------------+     +------------------+     +------------------+
|  Term/AST        |     |    Builtins      |     | Audio/MIDI       |
|  (bog.h)         |     |  (builtins.c)    |     | Callbacks        |
+------------------+     +------------------+     +------------------+
```

### 1. Term Representation (`bog.h`)

All data is represented as `BogTerm` structures:

```c
typedef enum {
    CPROLOG_TERM_NUM,      // Numbers: 36, 0.5, 120.0
    CPROLOG_TERM_ATOM,     // Atoms: kick, snare, ionian
    CPROLOG_TERM_VAR,      // Variables: X, Pitch, T
    CPROLOG_TERM_COMPOUND, // Compound terms: event(kick, 36, 0.9, T)
    CPROLOG_TERM_LIST,     // Lists: [60, 64, 67]
    CPROLOG_TERM_EXPR      // Expressions: 2 + 3, X * 4
} BogTermType;
```

### 2. Parser (`bog.c`)

Recursive descent parser that handles:
- Facts: `event(kick, 36, 0.9, 0).`
- Rules: `event(kick, 36, 0.9, T) :- euc(T, 4, 16, 4, 0).`
- Infix operators: `X is 2 + 3`, `X < Y`, `A =:= B`
- Lists with head/tail: `[H|T]`
- Comments: `% this is ignored`

```c
BogProgram* bog_parse_program(const char* src, BogArena* arena, char** error);
```

### 3. Unification (`bog.c`)

Pattern matching between terms. Two terms unify if they can be made identical through variable substitution.

```c
bool bog_unify(BogTerm* a, BogTerm* b, BogEnv* env, BogArena* arena);
```

Examples:
- `foo(X, 2)` unifies with `foo(1, Y)` producing `{X=1, Y=2}`
- `[H|T]` unifies with `[1,2,3]` producing `{H=1, T=[2,3]}`

### 4. Resolution Engine (`bog.c`)

SLD resolution with backtracking. Given a query and a program, finds all solutions.

```c
void bog_resolve(const BogGoalList* goals, BogEnv* env,
                 const BogProgram* program, const BogContext* ctx,
                 const BogBuiltins* builtins,
                 BogSolutions* solutions, BogArena* arena);
```

The resolver:
1. Takes a goal (e.g., `event(Voice, Pitch, Vel, 0)`)
2. Finds matching clauses in the program
3. Recursively resolves body goals
4. Collects all successful variable bindings

### 5. Builtins (`builtins.c`)

32 built-in predicates for music and logic:

| Category | Predicates |
|----------|------------|
| Timing | `beat/2`, `every/2`, `phase/4` |
| Rhythm | `euc/5` (Euclidean), `pulse/3` |
| Pitch | `scale/5`, `chord/4`, `inv/3` |
| Selection | `choose/2`, `seq/2`, `shuffle/2`, `wrand/2` |
| Arithmetic | `is/2`, `mod/3`, `abs/2`, `round/2`, `floor/2`, `ceil/2` |
| Comparison | `=/2`, `=:=/2`, `=\=/2`, `</2`, `>/2`, `=</2`, `>=/2` |
| Control | `trigger/3`, `chance/2` |

### 6. State Manager (`scheduler.c`)

Maintains state across scheduler ticks:
- **Cycle state**: For `seq/2` to cycle through lists
- **Trigger state**: For `trigger/3` to enforce minimum gaps

```c
BogStateManager* bog_state_manager_create(void);
size_t bog_state_manager_increment_cycle(BogStateManager* mgr,
                                         const char* key, size_t length);
bool bog_state_manager_can_trigger(const BogStateManager* mgr,
                                   const char* key, double now, double gap);
```

### 7. Scheduler (`scheduler.c`)

The heart of the runtime. Queries for events and dispatches to callbacks.

```c
BogScheduler* bog_scheduler_create(const BogAudioCallbacks* audio,
                                   const BogBuiltins* builtins,
                                   BogStateManager* state_manager);
void bog_scheduler_tick(BogScheduler* scheduler);
```

Each tick:
1. Gets current time from callbacks
2. Queries `event(Voice, Pitch, Vel, T)` for upcoming time slots
3. Extracts Voice, Pitch, Velocity from solutions
4. Applies swing timing adjustment
5. Dispatches to appropriate voice callback

### 8. Transition Manager (`scheduler.c`)

Handles live code updates with quantization:

```c
BogTransitionManager* bog_transition_manager_create(BogScheduler* scheduler,
                                                    double quantization_beats);
void bog_transition_manager_schedule(BogTransitionManager* mgr,
                                     const BogProgram* program);
```

New programs take effect at the next quantization boundary (e.g., next bar).

### 9. Memory Management

Arena allocator for AST/term memory:

```c
BogArena* bog_arena_create(void);
void* bog_arena_alloc(BogArena* arena, size_t size);
void bog_arena_destroy(BogArena* arena);  // Frees everything at once
```

Environments (`BogEnv`) use standard malloc/free for flexibility during resolution.

---

## MIDI Backend Implementation Guide

### Overview

The scheduler dispatches events through `BogAudioCallbacks`. To implement MIDI output, you provide callbacks that send MIDI messages instead of triggering audio.

### Step 1: Choose a MIDI Library

Options for cross-platform MIDI:
- **RtMidi** (C++) - Popular, well-tested
- **PortMidi** (C) - Pure C, simpler API
- **CoreMIDI** (macOS only)
- **ALSA** (Linux only)

### Step 2: Define Your MIDI Context

```c
#include <portmidi.h>

typedef struct {
    PmStream* stream;
    PmDeviceID device_id;
    double start_time;        // For relative timing
    int drum_channel;         // Typically channel 10 (9 zero-indexed)
    int synth_channel;        // For melodic voices
} MidiContext;
```

### Step 3: Implement Timing

The scheduler needs a time source:

```c
double midi_get_time(void* userdata) {
    MidiContext* ctx = (MidiContext*)userdata;
    // Return time in seconds since start
    return (Pt_Time() - ctx->start_time) / 1000.0;
}
```

### Step 4: Implement Voice Callbacks

Map bog voices to MIDI:

```c
// Helper to send MIDI note
static void send_note(MidiContext* ctx, int channel, int note,
                      int velocity, double time_sec) {
    // Convert to milliseconds for PortMidi timestamp
    PmTimestamp timestamp = (PmTimestamp)(time_sec * 1000);

    // Note On
    PmEvent event;
    event.timestamp = timestamp;
    event.message = Pm_Message(0x90 | channel, note, velocity);
    Pm_Write(ctx->stream, &event, 1);

    // Schedule Note Off (e.g., 100ms later)
    event.timestamp = timestamp + 100;
    event.message = Pm_Message(0x80 | channel, note, 0);
    Pm_Write(ctx->stream, &event, 1);
}

// Drum voices - use GM drum map on channel 10
void midi_kick(void* userdata, double time, double velocity) {
    MidiContext* ctx = (MidiContext*)userdata;
    send_note(ctx, 9, 36, (int)(velocity * 127), time);  // GM kick
}

void midi_snare(void* userdata, double time, double velocity) {
    MidiContext* ctx = (MidiContext*)userdata;
    send_note(ctx, 9, 38, (int)(velocity * 127), time);  // GM snare
}

void midi_hat(void* userdata, double time, double velocity) {
    MidiContext* ctx = (MidiContext*)userdata;
    send_note(ctx, 9, 42, (int)(velocity * 127), time);  // Closed hi-hat
}

void midi_clap(void* userdata, double time, double velocity) {
    MidiContext* ctx = (MidiContext*)userdata;
    send_note(ctx, 9, 39, (int)(velocity * 127), time);  // Hand clap
}

// Melodic voices - pitch comes from the event
void midi_sine(void* userdata, double time, double midi_note, double velocity) {
    MidiContext* ctx = (MidiContext*)userdata;
    send_note(ctx, ctx->synth_channel, (int)midi_note,
              (int)(velocity * 127), time);
}

// Map other synth voices to different channels or same channel
void midi_square(void* userdata, double time, double midi, double velocity) {
    midi_sine(userdata, time, midi, velocity);  // Or use different channel
}
```

### Step 5: Initialize and Run

```c
int main(void) {
    // Initialize PortMidi
    Pm_Initialize();
    Pt_Start(1, NULL, NULL);  // Start timer with 1ms resolution

    // Open MIDI output
    MidiContext midi_ctx = {0};
    midi_ctx.device_id = Pm_GetDefaultOutputDeviceID();
    midi_ctx.start_time = Pt_Time();
    midi_ctx.drum_channel = 9;
    midi_ctx.synth_channel = 0;

    Pm_OpenOutput(&midi_ctx.stream, midi_ctx.device_id, NULL, 256, NULL, NULL, 0);

    // Set up callbacks
    BogAudioCallbacks callbacks = {
        .userdata = &midi_ctx,
        .time = midi_get_time,
        .kick = midi_kick,
        .snare = midi_snare,
        .hat = midi_hat,
        .clap = midi_clap,
        .sine = midi_sine,
        .square = midi_square,
        .triangle = midi_sine,  // Reuse or differentiate
        .noise = midi_hat,      // Map noise to percussion
    };

    // Create bog components
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    BogStateManager* state_mgr = bog_state_manager_create();
    BogScheduler* scheduler = bog_scheduler_create(&callbacks, builtins, state_mgr);

    // Parse and set program
    char* error = NULL;
    BogProgram* program = bog_parse_program(
        "event(kick, 36, 0.9, T) :- euc(T, 4, 16, 4, 0).\n"
        "event(hat, 42, 0.6, T) :- every(T, 0.25).\n"
        "event(sine, N, 0.7, T) :- every(T, 0.5), choose([60,64,67], N).\n",
        arena, &error);

    bog_scheduler_set_program(scheduler, program);
    bog_scheduler_configure(scheduler, 120.0, 0.0, 80.0, 0.25);
    bog_scheduler_start(scheduler);

    // Main loop
    while (running) {
        bog_scheduler_tick(scheduler);
        usleep(10000);  // 10ms sleep
    }

    // Cleanup
    bog_scheduler_stop(scheduler);
    bog_scheduler_destroy(scheduler);
    bog_state_manager_destroy(state_mgr);
    bog_arena_destroy(arena);

    Pm_Close(midi_ctx.stream);
    Pm_Terminate();

    return 0;
}
```

### Step 6: Handle Note Duration

The current callback API doesn't include note duration. Options:

1. **Fixed duration**: Send Note Off after fixed delay (shown above)
2. **Voice-specific duration**: Drums short, synths longer
3. **Extend the event model**: Add duration parameter to event/5

For option 3, you'd modify the query in `scheduler_query_and_schedule()` to use `event(Voice, Pitch, Vel, Duration, T)`.

### Step 7: Link Against Libraries

Update CMakeLists.txt:

```cmake
find_library(PORTMIDI_LIB portmidi)
find_library(PORTTIME_LIB porttime)

add_executable(bog_midi src/main_midi.c)
target_link_libraries(bog_midi PRIVATE bog ${PORTMIDI_LIB} ${PORTTIME_LIB})
```

### GM Drum Map Reference

| Note | Instrument |
|------|------------|
| 35 | Acoustic Bass Drum |
| 36 | Bass Drum 1 |
| 37 | Side Stick |
| 38 | Acoustic Snare |
| 39 | Hand Clap |
| 42 | Closed Hi-Hat |
| 44 | Pedal Hi-Hat |
| 46 | Open Hi-Hat |
| 49 | Crash Cymbal |
| 51 | Ride Cymbal |

### Alternative: Generic Event Callback

If you want a simpler interface, you could add a generic callback to `BogAudioCallbacks`:

```c
void (*event)(void* userdata, const char* voice, double time,
              double midi, double velocity);
```

Then in `trigger_voice()`, call this for all events, letting the backend decide how to handle each voice. This would require modifying `scheduler.c`.

---

## Example Bog Programs

### Basic Beat

```prolog
event(kick, 36, 0.9, T) :- beat(T, 1).
event(snare, 38, 0.8, T) :- beat(T, 2).
event(hat, 42, 0.5, T) :- every(T, 0.25).
```

### Euclidean Rhythm

```prolog
event(kick, 36, 0.9, T) :- euc(T, 4, 16, 4, 0).
event(snare, 38, 0.8, T) :- euc(T, 3, 8, 3, 0).
```

### Melodic Pattern

```prolog
event(sine, N, 0.7, T) :-
    every(T, 0.5),
    scale(60, minor, 8, T, N).

event(sine, N, 0.6, T) :-
    every(T, 0.25),
    chord(48, minor7, T, N).
```

### Generative Sequence

```prolog
event(sine, N, V, T) :-
    every(T, 0.25),
    seq([60, 64, 67, 72], N),
    wrand([0.3-0.9, 0.7-0.5], V).
```
