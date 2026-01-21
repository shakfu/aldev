# PSND OSC Integration Design

## Overview

This document describes the design for integrating [liblo](http://liblo.sourceforge.net/) (Open Sound Control library) into psnd, enabling OSC communication across all editor/REPL modes.

OSC (Open Sound Control) is a protocol for communication between computers, sound synthesizers, and other multimedia devices. It's widely used in music software for remote control and inter-application communication.

## Motivation

OSC integration enables:

1. **Remote Control** - Control psnd from external applications (TouchOSC, Max/MSP, SuperCollider, Lemur)
2. **Hardware Controllers** - Receive input from OSC-enabled hardware (tablets, custom controllers)
3. **Inter-Application Communication** - Integrate psnd into larger music production setups
4. **Live Performance** - Trigger playback, change tempo, send notes from stage controllers
5. **Monitoring/Logging** - Forward events to external visualizers or loggers

## Architecture

### Module Location

```
source/core/shared/osc/
  osc.h             # Public API
  osc.c             # Implementation using liblo
```

### Integration with SharedContext

The `SharedContext` structure gains OSC-related fields:

```c
typedef struct SharedContext {
    // ... existing fields ...

    // OSC state
    int osc_enabled;
    int osc_port;
    lo_server_thread osc_server;
    lo_address osc_broadcast;       // Optional broadcast target
    void *osc_user_data;            // For callbacks
} SharedContext;
```

### Thread Model

liblo's `lo_server_thread` runs OSC reception in a background thread, invoking registered handlers when messages arrive. This is non-blocking and works alongside any host (terminal, webview, web server).

## CLI Interface

```bash
# Enable OSC server on default port (7770)
psnd --osc song.alda
psnd alda --osc

# Specify custom port
psnd --osc --osc-port 9000 song.alda

# Broadcast events to external listener
psnd --osc --osc-send localhost:8000 song.alda

# Combined with other modes
psnd --native --osc song.alda       # Native webview + OSC
psnd --web --osc song.alda          # Web server + OSC
```

### CLI Arguments

| Flag | Description |
|------|-------------|
| `--osc` | Enable OSC server on default port (7770) |
| `--osc-port PORT` | Set OSC server port (implies `--osc`) |
| `--osc-send HOST:PORT` | Send events to external OSC listener |

## OSC Address Namespace

### Incoming Messages (psnd receives)

#### Playback Control

| Address | Types | Description |
|---------|-------|-------------|
| `/psnd/play` | (none) | Play entire file |
| `/psnd/play/line` | `i` | Play specific line number |
| `/psnd/play/selection` | (none) | Play current selection |
| `/psnd/eval` | `s` | Evaluate code string |
| `/psnd/stop` | (none) | Stop all playback |
| `/psnd/panic` | (none) | All notes off |

#### Tempo & Transport

| Address | Types | Description |
|---------|-------|-------------|
| `/psnd/tempo` | `f` | Set tempo in BPM |
| `/psnd/tempo/tap` | (none) | Tap tempo |

#### MIDI Control

| Address | Types | Description |
|---------|-------|-------------|
| `/psnd/note` | `iii` | Play note (pitch, velocity, duration_ms) |
| `/psnd/note` | `iiii` | Play note on channel (ch, pitch, vel, dur) |
| `/psnd/noteon` | `ii` | Note on (pitch, velocity) |
| `/psnd/noteon` | `iii` | Note on (channel, pitch, velocity) |
| `/psnd/noteoff` | `i` | Note off (pitch) |
| `/psnd/noteoff` | `ii` | Note off (channel, pitch) |
| `/psnd/cc` | `ii` | Control change (cc, value) |
| `/psnd/cc` | `iii` | Control change (channel, cc, value) |
| `/psnd/pc` | `i` | Program change (program) |
| `/psnd/pc` | `ii` | Program change (channel, program) |
| `/psnd/bend` | `i` | Pitch bend (-8192 to 8191) |
| `/psnd/bend` | `ii` | Pitch bend (channel, value) |

#### Editor Control

| Address | Types | Description |
|---------|-------|-------------|
| `/psnd/file/open` | `s` | Open file by path |
| `/psnd/file/save` | (none) | Save current file |
| `/psnd/goto` | `i` | Go to line number |
| `/psnd/insert` | `s` | Insert text at cursor |

#### Backend Control

| Address | Types | Description |
|---------|-------|-------------|
| `/psnd/backend` | `s` | Set backend ("tsf", "midi", "csound") |
| `/psnd/soundfont` | `s` | Load soundfont file |
| `/psnd/link` | `i` | Enable/disable Ableton Link (0/1) |
| `/psnd/link/tempo` | `f` | Set Link tempo |

#### Query (request state)

| Address | Types | Description |
|---------|-------|-------------|
| `/psnd/query/tempo` | (none) | Request current tempo (replies to sender) |
| `/psnd/query/playing` | (none) | Request playback state |
| `/psnd/query/file` | (none) | Request current filename |
| `/psnd/query/position` | (none) | Request cursor position |

### Outgoing Messages (psnd sends)

When `--osc-send` is specified, psnd broadcasts state changes:

| Address | Types | Description |
|---------|-------|-------------|
| `/psnd/status/playing` | `i` | Playback started (1) or stopped (0) |
| `/psnd/status/tempo` | `f` | Tempo changed |
| `/psnd/status/file` | `s` | File opened |
| `/psnd/status/position` | `ii` | Cursor moved (line, col) |
| `/psnd/status/mode` | `s` | Editor mode changed |
| `/psnd/midi/note` | `iii` | MIDI note forwarded (ch, pitch, vel) |
| `/psnd/midi/cc` | `iii` | MIDI CC forwarded (ch, cc, val) |
| `/psnd/link/beat` | `f` | Link beat position |
| `/psnd/link/peers` | `i` | Link peer count changed |

## C API

### Initialization

```c
/**
 * Initialize OSC server on specified port.
 * @param ctx SharedContext to store OSC state
 * @param port Port number (use 0 for default 7770)
 * @return 0 on success, -1 on error
 */
int shared_osc_init(SharedContext *ctx, int port);

/**
 * Set broadcast target for outgoing messages.
 * @param ctx SharedContext
 * @param host Target hostname or IP
 * @param port Target port
 * @return 0 on success, -1 on error
 */
int shared_osc_set_broadcast(SharedContext *ctx, const char *host, const char *port);

/**
 * Start OSC server thread.
 * @param ctx SharedContext
 * @return 0 on success, -1 on error
 */
int shared_osc_start(SharedContext *ctx);

/**
 * Stop OSC server and clean up.
 * @param ctx SharedContext
 */
void shared_osc_cleanup(SharedContext *ctx);
```

### Sending Messages

```c
/**
 * Send OSC message to broadcast target.
 * @param ctx SharedContext
 * @param path OSC address path
 * @param types Type string (e.g., "ifs" for int, float, string)
 * @param ... Arguments matching types
 * @return 0 on success, -1 on error
 */
int shared_osc_send(SharedContext *ctx, const char *path, const char *types, ...);

/* Convenience functions */
void shared_osc_send_playing(SharedContext *ctx, int playing);
void shared_osc_send_tempo(SharedContext *ctx, float tempo);
void shared_osc_send_note(SharedContext *ctx, int channel, int pitch, int velocity);
```

### Callback Registration

```c
/**
 * OSC message handler callback type.
 * @param path OSC address path
 * @param types Type string of arguments
 * @param argv Array of argument pointers
 * @param argc Argument count
 * @param user_data User data pointer
 * @return 0 if handled, 1 to pass to next handler
 */
typedef int (*osc_handler_t)(const char *path, const char *types,
                              lo_arg **argv, int argc, void *user_data);

/**
 * Register custom OSC handler.
 * @param ctx SharedContext
 * @param path OSC path pattern (supports wildcards)
 * @param types Expected type string (NULL for any)
 * @param handler Callback function
 * @param user_data User data for callback
 */
void shared_osc_add_handler(SharedContext *ctx, const char *path,
                            const char *types, osc_handler_t handler,
                            void *user_data);
```

## Lua API

### Module: `loki.osc`

```lua
-- Initialize and start OSC server
loki.osc.init(port)          -- port optional, default 7770
loki.osc.start()
loki.osc.stop()

-- Check status
loki.osc.enabled()           -- returns true/false
loki.osc.port()              -- returns port number

-- Send messages (requires broadcast target)
loki.osc.send(path, ...)     -- auto-detect types
loki.osc.send_to(host, port, path, ...)

-- Set broadcast target
loki.osc.broadcast(host, port)

-- Register handlers
loki.osc.on(path, function(...)
    -- arguments passed based on message types
end)

-- Remove handler
loki.osc.off(path)
```

### Lua Examples

```lua
-- Basic setup in init.lua
loki.osc.init(7770)
loki.osc.start()

-- Handle incoming play command
loki.osc.on("/psnd/play", function()
    loki.alda.play_file()
end)

-- Handle note input
loki.osc.on("/psnd/note", function(pitch, velocity, duration)
    local code = string.format("piano: o4 %s", pitch_to_note(pitch))
    loki.alda.eval_sync(code)
end)

-- Handle eval with code string
loki.osc.on("/psnd/eval", function(code)
    loki.alda.eval_sync(code)
end)

-- Forward MIDI to another app
loki.osc.broadcast("localhost", 8000)
loki.alda.on_note(function(ch, pitch, vel)
    loki.osc.send("/midi/note", ch, pitch, vel)
end)
```

## Build Configuration

### CMake Option

```cmake
option(BUILD_OSC "Build with OSC support via liblo" OFF)

if(BUILD_OSC)
    set(LIBLO_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(LIBLO_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(LIBLO_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(WITH_STATIC ON CACHE BOOL "" FORCE)
    add_subdirectory(${PSND_ROOT_DIR}/source/thirdparty/liblo/cmake liblo)

    target_compile_definitions(libloki PUBLIC PSND_OSC)
    target_link_libraries(libloki PRIVATE liblo_static)
endif()
```

### Build Commands

```bash
# Build with OSC support
cmake -B build -DBUILD_OSC=ON
make

# Or use make target (when added)
make osc
```

## Integration Points

### 1. Session Initialization

In `session.c`, after SharedContext creation:

```c
if (config->osc_enabled) {
    if (shared_osc_init(ctx->model.shared, config->osc_port) == 0) {
        if (config->osc_broadcast_host) {
            shared_osc_set_broadcast(ctx->model.shared,
                                     config->osc_broadcast_host,
                                     config->osc_broadcast_port);
        }
        shared_osc_start(ctx->model.shared);
    }
}
```

### 2. Event Handling

OSC handlers queue events to the editor's event queue:

```c
static int osc_play_handler(const char *path, const char *types,
                            lo_arg **argv, int argc, void *user_data) {
    EditorSession *session = (EditorSession *)user_data;

    // Queue a "play file" command
    EditorEvent event = {0};
    event.type = EVENT_COMMAND;
    event.data.command.type = CMD_PLAY_FILE;
    editor_session_queue_event(session, &event);

    return 0;
}
```

### 3. State Change Notifications

When state changes, broadcast to listeners:

```c
// In playback code
void on_playback_start(SharedContext *ctx) {
    shared_osc_send_playing(ctx, 1);
}

void on_tempo_change(SharedContext *ctx, float bpm) {
    shared_osc_send_tempo(ctx, bpm);
}
```

## Security Considerations

1. **Network Binding** - By default, bind to localhost only. Add `--osc-any` flag for network-wide access.

2. **Input Validation** - Validate all incoming OSC data before use (path lengths, string sizes, numeric ranges).

3. **Rate Limiting** - Consider limiting message rate to prevent DoS from malicious clients.

4. **File Access** - Restrict `/psnd/file/open` to configured directories or disable in sensitive deployments.

## Testing

### Manual Testing with oscsend

```bash
# Start psnd with OSC
./build/psnd --osc song.alda

# In another terminal, send commands
oscsend localhost 7770 /psnd/play
oscsend localhost 7770 /psnd/tempo f 140.0
oscsend localhost 7770 /psnd/note iii 60 80 500
oscsend localhost 7770 /psnd/eval s "piano: c d e f g"
oscsend localhost 7770 /psnd/stop
```

### Unit Tests

```c
// test_osc.c
void test_osc_init(void) {
    SharedContext ctx = {0};
    ASSERT_EQ(shared_osc_init(&ctx, 7770), 0);
    ASSERT_EQ(ctx.osc_enabled, 1);
    shared_osc_cleanup(&ctx);
}

void test_osc_send(void) {
    // Set up receiver, send message, verify received
}
```

## Example Use Cases

### 1. TouchOSC Control Surface

Create a TouchOSC layout with:
- Play/Stop buttons sending `/psnd/play` and `/psnd/stop`
- Tempo fader sending `/psnd/tempo f {value}`
- Piano keys sending `/psnd/note iii {pitch} 100 500`

### 2. SuperCollider Integration

```supercollider
// Send note from SuperCollider to psnd
n = NetAddr("localhost", 7770);
n.sendMsg("/psnd/eval", "piano: c d e f g");

// Receive psnd events
OSCdef(\psndNote, { |msg|
    msg[1..3].postln;  // channel, pitch, velocity
}, '/psnd/midi/note');
```

### 3. Max/MSP Patch

```
[udpsend localhost 7770]
|
[pack /psnd/note i i i]
|
[60 80 500]  <- pitch, velocity, duration
```

### 4. Live Performance Setup

```bash
# Start psnd with OSC, broadcasting to visualizer
psnd --osc --osc-send visualizer.local:9000 set.alda

# Performer uses tablet controller to:
# - Trigger sections with /psnd/eval
# - Control tempo with /psnd/tempo
# - Trigger notes with /psnd/note
```

## Implementation Phases

### Phase 1: Core Infrastructure
- Add liblo to build system
- Create `shared/osc/` module
- Implement `shared_osc_init()`, `shared_osc_cleanup()`
- Add CLI flags `--osc`, `--osc-port`

### Phase 2: Basic Handlers
- Implement `/psnd/play`, `/psnd/stop`, `/psnd/eval`
- Implement `/psnd/tempo`
- Implement `/psnd/note`, `/psnd/noteon`, `/psnd/noteoff`

### Phase 3: Lua API
- Expose `loki.osc` module
- Implement `loki.osc.on()` for custom handlers
- Implement `loki.osc.send()`

### Phase 4: Broadcasting
- Implement `--osc-send` flag
- Add state change notifications
- MIDI event forwarding

### Phase 5: Advanced Features
- Query/reply mechanism
- Rate limiting
- Network interface selection
- Multicast support

## References

- [liblo Documentation](http://liblo.sourceforge.net/docs/)
- [OSC Specification](http://opensoundcontrol.org/spec-1_0)
- [TouchOSC](https://hexler.net/touchosc)
- [OSC in SuperCollider](https://doc.sccode.org/Guides/OSC_communication.html)
