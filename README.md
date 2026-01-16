# psnd — a polyglot editor & REPL for music programming languages

**psnd** is a self-contained modal editor, REPL, and playback environment aimed at music programming languages. The project is aiming to evolve into a polyglot platform for composing, live-coding, and rendering music DSLs from one binary.

Two languages are currently supported: [Alda](https://alda.io) and [Joy](https://github.com/shakfu/midi-langs/tree/main/projects/joy-midi). Alda is a music notation language; Joy is a concatenative (stack-based) functional language for music. Both are practical for daily live-coding, REPL sketches, and headless playback. The MIDI cores are from the [midi-langs](https://github.com/shakfu/midi-langs) project. Future milestones focus on integrating additional mini MIDI languages so that multiple notations can coexist within psnd. Audio output is handled by the built-in [TinySoundFont](https://github.com/schellingb/TinySoundFont) synthesizer or, optionally, a [Csound](https://csound.com/) backend for advanced synthesis. macOS and Linux are currently supported.

## Features

- **Vim-style editor** with INSERT/NORMAL modes, live evaluation shortcuts, and Lua scripting (built on [loki](https://github.com/shakfu/loki), a fork of [kilo](https://github.com/antirez/kilo))
- **Language-aware REPL** for interactive composition (Alda today; more DSLs on the way)
- **Headless play mode** for batch jobs and automation
- **Asynchronous playback** through [libuv](https://github.com/libuv/libuv)
- **Integrated MIDI routing** powered by [libremidi](https://github.com/celtera/libremidi)
- **MIDI file export** using [midifile](https://github.com/craigsapp/midifile)
- **TinySoundFont synthesizer** built on [miniaudio](https://github.com/mackron/miniaudio)
- **Optional Csound backend** for deeper sound design workflows
- **Ableton Link support** for networked tempo sync
- **Scala .scl import support** for microtuning
- **Lua APIs** for editor automation, playback control, and extensibility

## Status

psnd is in active early development. Alda and Joy are the two fully integrated languages, proving the polyglot architecture works. The internal design allows drop-in DSL integrations so that additional mini MIDI languages from [midi-langs](https://github.com/shakfu/midi-langs) can reuse the same editor, REPL, and audio stack. Expect rapid iteration and occasional breaking changes while polyglot support expands.

## Building

```bash
make              # Standard build with TinySoundFont
make csound       # Build with Csound synthesis backend (larger binary)
```

## Usage

psnd exposes three complementary workflows: REPL mode for interactive sketching, editor mode for live-coding within files, and play mode for headless rendering. Flags (soundfont, csound instruments, etc.) are shared between modes.

### REPL Mode

**Alda REPL**:

```bash
psnd alda               # Start Alda REPL
psnd alda -sf gm.sf2    # REPL with built-in synth
```

Type Alda notation directly:

```
alda> piano: c d e f g
alda> violin: o5 a b > c d e
alda> :stop
alda> :q
```

**Joy REPL**:

```bash
psnd joy                # Start Joy REPL
psnd joy --virtual out  # Joy REPL with named virtual port
psnd joy -p 0           # Joy REPL using MIDI port 0
```

Type Joy code directly:

```
joy> :virtual
joy> 120 tempo
joy> [c d e f g] play
joy> c major chord
joy> [c e g] [d f a] [e g b] each chord
joy> :q
```

**Shared REPL Commands** (work in both Alda and Joy, with or without `:`):

| Command | Action |
|---------|--------|
| `:q` `:quit` `:exit` | Exit REPL |
| `:h` `:help` `:?` | Show help |
| `:l` `:list` | List MIDI ports |
| `:s` `:stop` | Stop playback |
| `:p` `:panic` | All notes off |
| `:sf PATH` | Load soundfont and enable built-in synth |
| `:presets` | List soundfont presets |
| `:midi` | Switch to MIDI output |
| `:synth` `:builtin` | Switch to built-in synth |
| `:virtual [NAME]` | Create virtual MIDI port |
| `:link [on\|off]` | Enable/disable Ableton Link |
| `:link-tempo BPM` | Set Link tempo |
| `:link-status` | Show Link status |
| `:cs PATH` | Load CSD file and enable Csound |
| `:csound` | Enable Csound backend |
| `:cs-disable` | Disable Csound |
| `:cs-status` | Show Csound status |

**Alda-specific commands**:

| Command | Action |
|---------|--------|
| `:sequential` | Wait for each input to complete |
| `:concurrent` | Enable polyphonic playback (default) |
| `:export FILE` | Export to MIDI file |

**Joy-specific commands**:

| Command | Action |
|---------|--------|
| `.` | Print stack |

### Editor Mode

```bash
psnd song.alda                        # Open Alda file in editor
psnd song.joy                         # Open Joy file in editor
psnd song.csd                         # Open Csound file in editor
psnd -sf gm.sf2 song.alda             # Editor with TinySoundFont synth
psnd -cs instruments.csd song.alda    # Editor with Csound synthesis
```

Keybindings:

| Key | Action |
|-----|--------|
| `Ctrl-E` | Play current part/line (or selection) |
| `Ctrl-P` | Play entire file |
| `Ctrl-G` | Stop playback |
| `Ctrl-S` | Save |
| `Ctrl-Q` | Quit |
| `Ctrl-F` | Find |
| `Ctrl-L` | Lua console |
| `i` | Enter INSERT mode |
| `ESC` | Return to NORMAL mode |

### Play Mode

```bash
psnd play song.alda              # Play Alda file and exit
psnd play song.joy               # Play Joy file and exit
psnd play song.csd               # Play Csound file and exit
psnd play -sf gm.sf2 song.alda   # Play Alda with built-in synth
psnd play -v song.csd            # Play with verbose output
```

### Piped Input

Both REPLs support non-interactive piped input for scripting and automation:

```bash
# Alda REPL
echo 'piano: c d e f g' | psnd alda
echo -e 'piano: c d e\n:q' | psnd alda

# Joy REPL
echo '[c d e] play' | psnd joy
printf ':cs synth.csd\n:cs-status\n:q\n' | psnd joy
```

This is useful for testing, CI/CD pipelines, and batch processing.

## Lua Scripting

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

## Joy Language

Joy is a concatenative (stack-based) language for music composition. It provides a different paradigm from Alda's notation-based approach.

### Quick Start

```bash
psnd joy                    # Start Joy REPL
psnd song.joy               # Edit Joy file
psnd play song.joy          # Play Joy file headlessly
```

### Basic Syntax

Joy uses postfix notation where operations follow their arguments:

```joy
\ Comments start with backslash
:virtual                    \ Create virtual MIDI port
120 tempo                   \ Set tempo to 120 BPM
80 vol                      \ Set volume to 80

\ Play notes
c play                      \ Play middle C
[c d e f g] play            \ Play a melody

\ Chords
[c e g] chord               \ Play C major chord
c major chord               \ Same thing using music theory
a minor chord               \ A minor chord
g dom7 chord                \ G dominant 7th

\ Direct MIDI control
60 80 500 midi-note         \ Note 60, velocity 80, 500ms duration
```

### Music Theory Primitives

Joy includes music theory primitives for building chords:

| Primitive | Description | Example |
|-----------|-------------|---------|
| `major` | Major triad | `c major chord` |
| `minor` | Minor triad | `a minor chord` |
| `dom7` | Dominant 7th | `g dom7 chord` |
| `maj7` | Major 7th | `c maj7 chord` |
| `min7` | Minor 7th | `a min7 chord` |
| `dim` | Diminished triad | `b dim chord` |
| `aug` | Augmented triad | `c aug chord` |

### Stack Operations

Joy is stack-based, so values are pushed onto a stack and operations consume them:

```joy
60 dup                      \ Duplicate: [60 60]
60 70 swap                  \ Swap: [70 60]
60 70 pop                   \ Pop: [60]
[1 2 3] [dup *] map         \ Map: [1 4 9]
```

### Lua API

```lua
-- Initialize Joy
loki.joy.init()

-- Evaluate Joy code
loki.joy.eval(":virtual 120 tempo [c d e] play")

-- Define a custom word
loki.joy.define("cmaj", "[c e g] chord")

-- Stop playback
loki.joy.stop()
```

## Ableton Link

psnd supports [Ableton Link](https://www.ableton.com/en/link/) for tempo synchronization with other musicians and applications on the same network.

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
- Other Link-enabled apps (Ableton Live, etc.) share the same tempo

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

Export Alda compositions to Standard MIDI Files (.mid) for use in DAWs and other music software.

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

psnd optionally supports [Csound](https://csound.com/) as an advanced synthesis backend, providing full synthesis capabilities beyond TinySoundFont's sample playback.

### Building with Csound

```bash
make csound       # Build with Csound backend (~4.4MB binary)
```

### Quick Start

**Option 1: Command-line (recommended)**

```bash
psnd -cs .psnd/csound/default.csd song.alda
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
    loki.alda.csound_load(".psnd/csound/default.csd")

    -- Switch to Csound backend
    loki.alda.set_backend("csound")
end
```

### Standalone CSD Playback

You can also open and play `.csd` files directly without using them as MIDI instrument definitions:

```bash
psnd song.csd           # Edit CSD file, Ctrl-P to play
psnd play song.csd      # Headless playback
```

This plays the CSD file's embedded score section using Csound's native playback, not the MIDI-driven synthesis mode.

### Lua API

```lua
-- Check availability
loki.alda.csound_available()      -- true if compiled with Csound

-- Load instruments from .csd file (for MIDI-driven synthesis)
loki.alda.csound_load("instruments.csd")

-- Enable/disable Csound (for MIDI-driven synthesis)
loki.alda.set_csound(true)        -- Enable Csound (disables TSF)
loki.alda.set_csound(false)       -- Disable Csound

-- Unified backend selection
loki.alda.set_backend("csound")   -- Use Csound synthesis
loki.alda.set_backend("tsf")      -- Use TinySoundFont (SoundFont)
loki.alda.set_backend("midi")     -- Use external MIDI only

-- Standalone CSD playback (plays score section)
loki.alda.csound_play("song.csd") -- Play CSD file asynchronously
loki.alda.csound_playing()        -- Check if playback is active
loki.alda.csound_stop()           -- Stop playback
```

### Default Instruments

The included `.psnd/csound/default.csd` provides 16 instruments mapped to MIDI channels, including subtractive synth, FM piano, pad, pluck, organ, bass, strings, brass, and drums.

### Architecture Notes

Csound and TinySoundFont each have independent miniaudio audio devices. When you switch backends, the appropriate audio device is started/stopped. They do not share audio resources, allowing clean separation of concerns.

## Syntax Highlighting

psnd provides built-in syntax highlighting for music programming languages with language-aware features.

### Supported Languages

| Extension | Language | Features |
|-----------|----------|----------|
| `.alda` | Alda | Instruments, attributes, note names, octave markers, comments |
| `.joy` | Joy | Stack ops, combinators, music primitives, note names, comments |
| `.csd` | Csound CSD | Section-aware (orchestra/score/options), opcodes, control flow |
| `.orc` | Csound Orchestra | Full orchestra syntax |
| `.sco` | Csound Score | Score statements, parameters |
| `.scl` | Scala Scale | Comments, numbers (for microtuning definitions) |

### Csound CSD Section Awareness

CSD files contain multiple sections with different syntax. psnd detects these sections and applies appropriate highlighting:

- **`<CsInstruments>`** - Full Csound orchestra highlighting:
  - Control flow (`if`, `then`, `else`, `endif`, `while`, `do`, `od`, `goto`)
  - Structure (`instr`, `endin`, `opcode`, `endop`)
  - Header variables (`sr`, `kr`, `ksmps`, `nchnls`, `0dbfs`, `A4`)
  - Common opcodes (`oscili`, `vco2`, `moogladder`, `pluck`, `reverb`, etc.)
  - Comments (`;` single-line, `/* */` block)
  - Strings and numbers

- **`<CsScore>`** - Score statement highlighting:
  - Statement letters (`i`, `f`, `e`, `s`, `t`, etc.)
  - Numeric parameters
  - Comments

- **`<CsOptions>`** - Command-line flag highlighting

Section tags themselves are highlighted as keywords, and section state is tracked across lines.

## Scala Scale Files (Microtuning)

psnd supports [Scala scale files](https://www.huygens-fokker.org/scala/scl_format.html) (`.scl`) for microtuning and alternative temperaments.

### Loading Scales

```lua
-- Load a scale file
loki.scala.load(".psnd/scales/just.scl")

-- Check if loaded
if loki.scala.loaded() then
    print(loki.scala.description())  -- "5-limit just intonation major"
    print(loki.scala.length())       -- 7
end
```

### Converting MIDI to Frequencies

```lua
-- Convert MIDI note to frequency using loaded scale
-- Arguments: midi_note, root_note (default 60), root_freq (default 261.63)
local freq = loki.scala.midi_to_freq(60)   -- C4 in the scale
local freq = loki.scala.midi_to_freq(67, 60, 261.63)  -- G4 with explicit root

-- Get ratio for a specific degree
local ratio = loki.scala.ratio(4)  -- 4th degree ratio (e.g., 3/2 for perfect fifth)
```

### Generating Csound Pitch Tables

```lua
-- Generate Csound f-table statement for use in .csd files
local ftable = loki.scala.csound_ftable(261.63, 1)
-- Returns: "f1 0 8 -2 261.630000 294.328125 327.031250 ..."
```

### Sample Scales

The `.psnd/scales/` directory includes example scales:

| File | Description |
|------|-------------|
| `12tet.scl` | 12-tone equal temperament (standard Western tuning) |
| `just.scl` | 5-limit just intonation major scale |
| `pythagorean.scl` | Pythagorean 12-tone chromatic scale |

### Lua API Reference

| Function | Description |
|----------|-------------|
| `loki.scala.load(path)` | Load scale file, returns true or nil+error |
| `loki.scala.load_string(content)` | Load from string |
| `loki.scala.unload()` | Unload current scale |
| `loki.scala.loaded()` | Check if scale is loaded |
| `loki.scala.description()` | Get scale description |
| `loki.scala.length()` | Number of degrees (excluding 1/1) |
| `loki.scala.ratio(degree)` | Get frequency ratio for degree |
| `loki.scala.frequency(degree, base)` | Get frequency in Hz |
| `loki.scala.midi_to_freq(note, [root], [freq])` | MIDI note to Hz |
| `loki.scala.degrees()` | Get all degrees as table |
| `loki.scala.csound_ftable([base], [fnum])` | Generate Csound f-table |
| `loki.scala.cents_to_ratio(cents)` | Convert cents to ratio |
| `loki.scala.ratio_to_cents(ratio)` | Convert ratio to cents |

## Roadmap

- Integrate additional MIDI DSLs from [midi-langs](https://github.com/shakfu/midi-langs), giving psnd multiple interchangeable front-ends
- Provide syntax-aware helpers per language (highlighting, evaluation scopes, etc.)
- Experiment with additional backends (JACK, plugin bridges) where it improves workflows

Feedback and experiments are welcome—polyglot support will be guided by real-world usage.

## Project Structure

```
src/
  loki/           # Editor components (core, modal, syntax, lua, etc.)
  alda/           # Alda music library (parser, interpreter, backends)
  joy/            # Joy language runtime (parser, primitives, MIDI)
  main.c          # Entry point
  repl.c          # REPL mode (Alda and Joy)
include/
  loki/           # Public loki headers
  alda/           # Public alda headers
tests/
  loki/           # Editor unit tests
  alda/           # Alda parser tests
  joy/            # Joy parser and MIDI tests
thirdparty/       # External dependencies (lua, libuv, libremidi, etc.)
```

## Documentation

See the `docs` folder for full technical documentation.

## Credits

- [Alda](https://alda.io) - music programming language by Dave Yarwood
- [Joy](https://github.com/shakfu/midi-langs/tree/main/projects/joy-midi) - concatenative music language from midi-langs
- [kilo](https://github.com/antirez/kilo) by Salvatore Sanfilippo (antirez) - original editor
- [loki](https://github.com/shakfu/loki) - Lua-enhanced fork
- [Csound](https://csound.com/) - sound synthesis system (optional)
- [link](https://github.com/Ableton/link) - Ableton Link
- [midifile](https://github.com/craigsapp/midifile) - C++ library for reading/writing Standard MIDI Files
- [libremidi](https://github.com/celtera/libremidi) - Modern C++ MIDI 1 / MIDI 2 real-time & file I/O library
- [TinySoundFont](https://github.com/schellingb/TinySoundFont) - SoundFont2 synthesizer library in a single C/C++ file
- [miniaudio](https://github.com/mackron/miniaudio) - Audio playback and capture library written in C, in a single source file

## License

GPL-3

See `docs/licenses` for dependent licenses.
