-- Alda Language Definition
-- Syntax highlighting for Alda music notation files (.alda)

loki.register_language({
    name = "Alda",
    extensions = {".alda"},
    keywords = {
        -- General MIDI instruments (melodic)
        "piano", "acoustic-grand-piano", "bright-acoustic-piano",
        "electric-grand-piano", "honky-tonk-piano", "electric-piano-1",
        "electric-piano-2", "harpsichord", "clavinet",
        "celesta", "glockenspiel", "music-box", "vibraphone", "marimba",
        "xylophone", "tubular-bells", "dulcimer",
        "drawbar-organ", "percussive-organ", "rock-organ", "church-organ",
        "reed-organ", "accordion", "harmonica", "tango-accordion",
        "acoustic-guitar-nylon", "acoustic-guitar-steel", "electric-guitar-jazz",
        "electric-guitar-clean", "electric-guitar-muted", "overdriven-guitar",
        "distortion-guitar", "guitar-harmonics",
        "acoustic-bass", "electric-bass-finger", "electric-bass-pick",
        "fretless-bass", "slap-bass-1", "slap-bass-2", "synth-bass-1", "synth-bass-2",
        "violin", "viola", "cello", "contrabass", "tremolo-strings",
        "pizzicato-strings", "orchestral-harp", "timpani",
        "string-ensemble-1", "string-ensemble-2", "synth-strings-1", "synth-strings-2",
        "choir-aahs", "voice-oohs", "synth-choir", "orchestra-hit",
        "trumpet", "trombone", "tuba", "muted-trumpet", "french-horn",
        "brass-section", "synth-brass-1", "synth-brass-2",
        "soprano-sax", "alto-sax", "tenor-sax", "baritone-sax",
        "oboe", "english-horn", "bassoon", "clarinet",
        "piccolo", "flute", "recorder", "pan-flute", "blown-bottle",
        "shakuhachi", "whistle", "ocarina",
        "lead-1-square", "lead-2-sawtooth", "lead-3-calliope", "lead-4-chiff",
        "lead-5-charang", "lead-6-voice", "lead-7-fifths", "lead-8-bass-lead",
        "pad-1-new-age", "pad-2-warm", "pad-3-polysynth", "pad-4-choir",
        "pad-5-bowed", "pad-6-metallic", "pad-7-halo", "pad-8-sweep",
        "fx-1-rain", "fx-2-soundtrack", "fx-3-crystal", "fx-4-atmosphere",
        "fx-5-brightness", "fx-6-goblins", "fx-7-echoes", "fx-8-sci-fi",
        "sitar", "banjo", "shamisen", "koto", "kalimba", "bagpipe", "fiddle", "shanai",
        "tinkle-bell", "agogo", "steel-drums", "woodblock", "taiko-drum",
        "melodic-tom", "synth-drum",
        "reverse-cymbal", "guitar-fret-noise", "breath-noise", "seashore",
        "bird-tweet", "telephone-ring", "helicopter", "applause", "gunshot",
        -- Percussion
        "midi-percussion",
        -- Alda attributes
        "tempo", "volume", "vol", "track-volume", "track-vol",
        "panning", "pan", "quantization", "quant", "quantize",
        "key-signature", "key-sig", "transpose",
        "octave", "set-duration", "set-note-length",
        -- Markers and special
        "marker", "at-marker", "barline", "voice"
    },
    types = {
        -- Note names
        "c", "d", "e", "f", "g", "a", "b", "r",
        -- Octave markers
        "o0", "o1", "o2", "o3", "o4", "o5", "o6", "o7", "o8", "o9"
    },
    line_comment = "#",
    separators = ",.()+-/*=~%<>[]{}:;|'\"",
    highlight_strings = true,
    highlight_numbers = true
})
