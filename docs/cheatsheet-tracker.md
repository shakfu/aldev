# Tracker Cheatsheet

Quick reference for psnd tracker keybindings and commands.

## Navigation

| Key | Action |
|-----|--------|
| `h` `j` `k` `l` | Move cursor (vim-style) |
| Arrow keys | Move cursor |
| `g` | Go to first row |
| `G` | Go to last row |
| `0` | Go to first track |
| `$` | Go to last track |
| `[` `]` | Previous / next pattern |
| `Ctrl+U` `Ctrl+D` | Page up / down |

## Editing

| Key | Action |
|-----|--------|
| `i` | Enter edit mode |
| `Esc` | Exit edit mode / cancel |
| `Enter` | Confirm edit |
| `x` | Clear cell |
| `o` | Insert row |
| `O` | Duplicate row |
| `X` | Delete row |

## Selection & Clipboard

| Key | Action |
|-----|--------|
| `v` | Visual selection mode |
| `y` | Copy (yank) |
| `d` | Cut (delete) |
| `p` | Paste |
| `Shift+Arrow` | Extend selection |

## Tracks

| Key | Action |
|-----|--------|
| `m` | Mute/unmute track |
| `S` | Solo track |
| `a` | Add track |
| `A` | Remove track |

## Patterns

| Key | Action |
|-----|--------|
| `[` `]` | Previous / next pattern |
| `n` | New pattern |
| `c` | Clone pattern |
| `D` | Delete pattern |

## Playback

| Key | Action |
|-----|--------|
| `Space` | Play / Stop |
| `P` | Toggle play mode (PAT/SONG) |
| `Ctrl+R` | Toggle record mode |
| `f` | Toggle follow mode |
| `L` | Toggle loop mode |
| `{` `}` | Decrease / increase BPM |
| `Ctrl+G` | Panic (all notes off) |

## Settings

| Key | Action |
|-----|--------|
| `+` `-` | Increase / decrease step size |
| `>` `<` (or `.` `,`) | Increase / decrease octave |
| `T` | Cycle theme |

## Arrange Mode

| Key | Action |
|-----|--------|
| `r` | Enter arrange mode |
| `Esc` | Return to pattern view |
| `j` `k` / Arrows | Move in sequence |
| `a` | Add current pattern to sequence |
| `x` | Remove entry from sequence |
| `K` `J` | Move entry up / down |
| `+` `-` | Increase / decrease repeat count |
| `Enter` | Jump to pattern |

## FX Mode

| Key | Action |
|-----|--------|
| `F` | Enter FX mode |
| `Esc` | Return to pattern view |
| `j` `k` / Arrows | Move in FX chain |
| `c` | Edit cell FX |
| `t` | Edit track FX |
| `m` | Edit master FX |
| `a` | Add effect |
| `x` | Remove effect |
| `K` `J` | Move effect up / down |
| `Space` | Toggle effect on/off |

### Available FX

| Name | Description |
|------|-------------|
| `transpose` | Transpose note by semitones |
| `velocity` | Adjust note velocity |
| `arpeggio` | Arpeggiate notes |
| `delay` | Echo/delay effect |
| `ratchet` | Repeat note rapidly |

## File

| Key | Action |
|-----|--------|
| `Ctrl+S` | Save |
| `E` or `Ctrl+E` | Export MIDI |
| `q` | Quit |

## Views

| Key | Action |
|-----|--------|
| `?` | Help screen |
| `r` | Arrange mode |
| `F` | FX mode |

## Undo/Redo

| Key | Action |
|-----|--------|
| `u` or `Ctrl+Z` | Undo |
| `R` or `Ctrl+Y` | Redo |

---

## Command Mode

Press `:` to enter command mode. Press `Enter` to execute, `Esc` to cancel.

### File Commands

| Command | Action |
|---------|--------|
| `:w` | Save |
| `:w filename` | Save as |
| `:q` | Quit |
| `:q!` | Force quit (discard changes) |
| `:wq` | Save and quit |
| `:export` | Export to MIDI |
| `:export file.mid` | Export to specific file |

### Playback Commands

| Command | Action |
|---------|--------|
| `:bpm N` | Set tempo (20-300) |
| `:bpm` | Show current BPM |

### Pattern Commands

| Command | Action |
|---------|--------|
| `:rows N` | Set pattern length (1-256) |
| `:rows` | Show current row count |
| `:name text` | Set pattern name |
| `:name` | Show pattern name |

### Settings Commands

| Command | Action |
|---------|--------|
| `:set` | Show all settings |
| `:set step N` | Set step size (0-16) |
| `:set octave N` | Set default octave (0-9) |
| `:set follow on/off` | Toggle follow mode |
| `:set loop on/off` | Toggle loop mode |
| `:set swing N` | Set swing amount (0-100) |

### Other Commands

| Command | Action |
|---------|--------|
| `:help` | Show help screen |

---

## Status Bar Indicators

| Indicator | Meaning |
|-----------|---------|
| `[PLAY]` | Playback active |
| `[STOP]` | Playback stopped |
| `[REC]` | Record mode active |
| `[LOOP]` | Loop mode enabled |
| `[PAT]` | Pattern play mode |
| `[SONG]` | Song play mode |
| `NAV` | Navigation mode |
| `EDIT` | Edit mode |
| `VISUAL` | Visual selection mode |
| `CMD` | Command mode |

---

## Notes Plugin Syntax

The notes plugin accepts expressions in standard music notation:

```
C4      - Middle C
D#5     - D sharp, octave 5
Bb3     - B flat, octave 3
C4/8    - C4 with 1/8 note duration
C4.     - C4 with dotted duration
C4~     - C4 with tie (sustain)
---     - Note off / rest
```

### Chords

```
Cmaj    - C major chord
Am7     - A minor 7th
G/B     - G chord with B bass
```

### Velocity

```
C4!     - Accent (loud)
C4!!    - Very loud
C4_     - Soft
C4__    - Very soft
```

---

## Quick Tips

1. **Step entry**: Set step size with `+`/`-`, notes advance automatically
2. **Record MIDI**: Enable with `Ctrl+R`, play notes on MIDI controller
3. **Song mode**: Build sequence in arrange mode (`r`), then press `P` to switch
4. **Quick save**: `Ctrl+S` or `:w`
5. **Resize pattern**: `:rows 32` for 32-row pattern
6. **Check settings**: `:set` shows current step, octave, etc.
