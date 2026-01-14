# aldalog

**aldalog** is a self-contained live-coding text editor and REPL for the [Alda](https://alda.io) music programming language. It’s built on the [loki](https://github.com/shakfu/loki) editor (itself derived from antirez’s [kilo](https://github.com/antirez/kilo)) and integrates [alda-midi](https://github.com/shakfu/midi-langs) to provide a fast, expressive workflow.

It currently only works on macOS and Linux.

## Features

- **Editor Mode**: Vim-style modal editing with live-coding support
- **REPL Mode**: Interactive Alda composition—write notation and hear it immediately
- **Play Mode**: Headless playback for scripts and automation
- **Asynchronous playback** (non-blocking) using [libuv](https://github.com/libuv/libuv)
- **Built-in MIDI output** powered by [libremidi](https://github.com/celtera/libremidi)
- **MIDI file export** powered by [midifile](https://github.com/craigsapp/midifile)
- **Integrated software synthesizer** using [TinySoundFont](https://github.com/schellingb/TinySoundFont) and [miniaudio](https://github.com/mackron/miniaudio)
- **Optional Csound synthesis** for advanced sound design using [Csound](https://csound.com/)
- **Networked tempo synchronization** with DAWs and other performers using [Ableton Link](https://github.com/Ableton/link)
- **Lua scripting** for editor customization.

## Status

Early development. Core functionality works but the API is evolving.

## Building

```bash
make              # Standard build with TinySoundFont
make csound       # Build with Csound synthesis backend (larger binary)
```

## Usage

### REPL Mode (Interactive Composition)

```bash
aldalog                    # Start REPL
aldalog -sf gm.sf2         # REPL with built-in synth
```

Type Alda notation directly:

```
alda> piano: c d e f g
alda> violin: o5 a b > c d e
alda> :stop
alda> :q
```

REPL commands (use with or without `:` prefix):

| Command | Action |
|---------|--------|
| `:q` `:quit` | Exit REPL |
| `:h` `:help` | Show help |
| `:l` `:list` | List MIDI ports |
| `:s` `:stop` | Stop playback |
| `:p` `:panic` | All notes off |
| `:sf PATH` | Load soundfont |
| `:presets` | List soundfont presets |
| `:midi` | Switch to MIDI output |
| `:synth` | Switch to built-in synth |
| `:link [on\|off]` | Toggle Ableton Link sync |
| `:export FILE` | Export to MIDI file |
| `:csd [on\|off]` | Toggle Csound synthesis |

### Editor Mode (Live-Coding)

```bash
aldalog song.alda                        # Open file in editor
aldalog -sf gm.sf2 song.alda             # Editor with TinySoundFont synth
aldalog -cs instruments.csd song.alda    # Editor with Csound synthesis
```

**Keybindings:**

| Key | Action |
|-----|--------|
| `Ctrl-E` | Play current part (or selection) |
| `Ctrl-P` | Play entire file |
| `Ctrl-G` | Stop playback |
| `Ctrl-S` | Save |
| `Ctrl-Q` | Quit |
| `Ctrl-F` | Find |
| `Ctrl-L` | Lua console |
| `i` | Enter INSERT mode |
| `ESC` | Return to NORMAL mode |

### Play Mode (Headless)

```bash
aldalog play song.alda              # Play file and exit
aldalog play -sf gm.sf2 song.alda   # Play with built-in synth
```

## Lua Scripting (Editor)

Press `Ctrl-L` in the editor to access the Lua console:

```lua
-- Play Alda code
loki.alda.eval_sync("piano: c d e f g a b > c")

-- Async playback with callback
loki.alda.eval("piano: c d e f g", "on_done")

-- Stop playback
loki.alda.stop_all()

-- Load soundfont for built-in synth
loki.alda.load_soundfont("path/to/soundfont.sf2")
loki.alda.set_synth(true)
```

## Ableton Link

Aldalog supports [Ableton Link](https://www.ableton.com/en/link/) for tempo synchronization with other musicians and applications on the same network.

### Quick Start

In the editor, use the `:link` command:

```
:link on       # Enable Link
:link off      # Disable Link
:link          # Toggle Link
```

When Link is enabled:
- Status bar shows "ALDA LINK" instead of "ALDA NORMAL"
- Playback tempo syncs with the Link session
- Other Link-enabled apps (Ableton Live, etc.) will share the same tempo

### Lua API

```lua
-- Initialize and enable Link
loki.link.init(120)           -- Initialize with 120 BPM
loki.link.enable(true)        -- Start networking

-- Tempo control
loki.link.tempo()             -- Get session tempo
loki.link.set_tempo(140)      -- Set tempo (syncs to all peers)

-- Session info
loki.link.peers()             -- Number of connected peers
loki.link.beat(4)             -- Current beat (4 beats per bar)
loki.link.phase(4)            -- Phase within bar [0, 4)

-- Transport sync (optional)
loki.link.start_stop_sync(true)
loki.link.play()              -- Start transport
loki.link.stop()              -- Stop transport
loki.link.is_playing()        -- Check transport state

-- Callbacks (called when values change)
loki.link.on_tempo("my_tempo_handler")
loki.link.on_peers("my_peers_handler")
loki.link.on_start_stop("my_transport_handler")

-- Cleanup
loki.link.cleanup()
```

## MIDI Export

Export your Alda compositions to Standard MIDI Files (.mid) for use in DAWs and other music software.

### Quick Start

1. Play some Alda code to generate events (Ctrl-E or Ctrl-P)
2. Run `:export song.mid` to export

### Lua API

```lua
-- Export to MIDI file
local ok, err = loki.midi.export("song.mid")
if not ok then
    loki.status("Export failed: " .. err)
end
```

### Notes

- Single-channel compositions export as Type 0 MIDI (single track)
- Multi-channel compositions export as Type 1 MIDI (multiple tracks)
- All events (notes, program changes, tempo, pan) are preserved

## Csound Synthesis

Aldalog optionally supports [Csound](https://csound.com/) as an advanced synthesis backend, providing full synthesis capabilities beyond TinySoundFont's sample playback.

### Building with Csound

```bash
make csound       # Build with Csound backend (~4.4MB binary)
```

### Quick Start

**Option 1: Command-line (recommended)**

```bash
aldalog -cs .aldalog/csound/default.csd song.alda
```

This loads the Csound instruments and enables Csound synthesis automatically when opening the file.

**Option 2: Ex commands in editor**

In the editor, use the `:csd` command after loading a .csd file:

```
:csd on        # Enable Csound synthesis
:csd off       # Disable Csound, switch to TinySoundFont
:csd           # Toggle between Csound and TSF
```

Or via Lua:

```lua
-- Check if Csound is available
if loki.alda.csound_available() then
    -- Load Csound instruments
    loki.alda.csound_load(".aldalog/csound/default.csd")

    -- Switch to Csound backend
    loki.alda.set_backend("csound")
end
```

### Lua API

```lua
-- Check availability
loki.alda.csound_available()      -- true if compiled with Csound

-- Load instruments from .csd file
loki.alda.csound_load("instruments.csd")

-- Enable/disable Csound
loki.alda.set_csound(true)        -- Enable Csound (disables TSF)
loki.alda.set_csound(false)       -- Disable Csound

-- Unified backend selection
loki.alda.set_backend("csound")   -- Use Csound synthesis
loki.alda.set_backend("tsf")      -- Use TinySoundFont (SoundFont)
loki.alda.set_backend("midi")     -- Use external MIDI only
```

### Default Instruments

The included `.aldalog/csound/default.csd` provides 16 instruments mapped to MIDI channels, including subtractive synth, FM piano, pad, pluck, organ, bass, strings, brass, and drums.

### Architecture Notes

Csound and TinySoundFont each have independent miniaudio audio devices. When you switch backends, the appropriate audio device is started/stopped. They do not share audio resources, allowing clean separation of concerns.

## Project Structure

```
src/
  loki/           # Editor components (core, modal, syntax, lua, etc.)
  alda/           # Alda music library (parser, interpreter, backends)
  main.c          # Entry point
  repl.c          # REPL mode
include/
  loki/           # Public loki headers
  alda/           # Public alda headers
tests/
  loki/           # Editor unit tests
  alda/           # Alda parser tests
thirdparty/       # External dependencies (lua, libuv, libremidi, etc.)
```

## Documentation

See the `docs` folder for full technical documentation.

## Credits

- [Alda](https://alda.io) - music programming language by Dave Yarwood
- [kilo](https://github.com/antirez/kilo) by Salvatore Sanfilippo (antirez) - original editor
- [loki](https://github.com/shakfu/loki) - Lua-enhanced fork
- [Csound](https://csound.com/) - sound synthesis system (optional)
- [link](https://github.com/Ableton/link) - Ableton Link
- [midifile](https://github.com/craigsapp/midifile) - C++ library for reading/writing Standard MIDI Files
- [libremidi](https://github.com/celtera/libremidi) - A modern C++ MIDI 1 / MIDI 2 real-time & file I/O library. Supports Windows, macOS, Linux and WebMIDI.
- [TinySoundFont](https://github.com/schellingb/TinySoundFont) - SoundFont2 synthesizer library in a single C/C++ file
- [miniaudio](https://github.com/mackron/miniaudio) - Audio playback and capture library written in C, in a single source file.

## License

GPL-3

see [docs/licenses](docs/licenses) for dependent licenses

