# aldev

A unified command-line tool for the [Alda](https://alda.io) music programming language featuring a live-coding editor and interactive REPL.

Built on [loki](https://github.com/shakfu/loki), a lightweight text editor with vim-like modal editing and Lua scripting. Uses [libalda](https://github.com/shakfu/midi-langs/tree/main/alda-midi) for Alda parsing and playback.

## Status

Early development. Core functionality works but the API is evolving.

## Features

- **Editor Mode**: Vim-like modal editor with live-coding support
- **REPL Mode**: Interactive Alda composition - type notation directly
- **Play Mode**: Headless playback for scripts and automation
- Built-in software synthesizer (`TinySoundFont`) or MIDI output
- Async playback (non-blocking)
- Lua scripting for editor customization

## Building

```bash
make
```

## Usage

### REPL Mode (Interactive Composition)

```bash
aldev                    # Start REPL
aldev -sf gm.sf2         # REPL with built-in synth
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

### Editor Mode (Live-Coding)

```bash
aldev song.alda          # Open file in editor
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
aldev play song.alda              # Play file and exit
aldev play -sf gm.sf2 song.alda   # Play with built-in synth
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

## Documentation

See the `docs` folder for full technical documentation.

## Credits

- [Alda](https://alda.io) - music programming language by Dave Yarwood
- [kilo](https://github.com/antirez/kilo) by Salvatore Sanfilippo (antirez) - original editor
- [loki](https://github.com/shakfu/loki) - Lua-enhanced fork

## License

MIT

see [docs/licenses](docs/licenses) for dependent licenses

