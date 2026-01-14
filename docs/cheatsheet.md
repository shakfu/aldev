# psnd

## Commandline API

```sh
# repl mode
psnd                                 # Start REPL
psnd -sf gm.sf2                      # REPL with built-in synth

# editor mode
psnd song.alda                       # Open alda file in editor
psnd song.csd                        # Open csound file in editor
psnd -sf gm.sf2 song.alda            # Editor with TinySoundFont synth
psnd -cs instruments.csd song.alda   # Editor with Csound synthesis

# play mode (headless)
psnd play song.alda                  # Play alda file and exit
psnd play song.csd                   # Play csound file and exit
psnd play -sf gm.sf2 song.alda       # Play with built-in synth
```
