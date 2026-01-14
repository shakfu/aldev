# aldalog

## Commandline API

```sh
# repl mode
aldalog                                 # Start REPL
aldalog -sf gm.sf2                      # REPL with built-in synth

# editor mode
aldalog song.alda                       # Open alda file in editor
aldalog song.csd                        # Open csound file in editor
aldalog -sf gm.sf2 song.alda            # Editor with TinySoundFont synth
aldalog -cs instruments.csd song.alda   # Editor with Csound synthesis

# play mode (headless)
aldalog play song.alda                  # Play alda file and exit
aldalog play song.csd                   # Play csound file and exit
aldalog play -sf gm.sf2 song.alda       # Play with built-in synth
```
