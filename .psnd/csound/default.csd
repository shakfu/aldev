<CsoundSynthesizer>
;; Default Instruments for Psnd
;;
;; Each MIDI channel (1-16) maps to an instrument number (1-16).
;; Instruments receive: p4 = MIDI pitch (0-127), p5 = velocity (0-127)
;;
;; Control channels available:
;;   cc<N>_ch<CH>  - CC value normalized 0-1 (e.g., cc1_ch1 for mod wheel)
;;   bend_ch<CH>   - Pitch bend normalized -1 to 1
;;
<CsOptions>
-d -n -m0 --daemon
</CsOptions>

<CsInstruments>
sr = 44100
ksmps = 64
nchnls = 2
0dbfs = 1

;; Utility: MIDI to frequency with pitch bend
opcode MidiPitch, k, ii
  imidi, ich xin
  kbend chnget sprintf("bend_ch%d", ich)
  kpch = cpsmidinn(imidi + kbend * 2)  ; +/- 2 semitones bend
  xout kpch
endop

;; ----------------------------------------------------------------------------
;; Instrument 1: Subtractive Synth (saw + filter)
;; Good for leads and basses
;; ----------------------------------------------------------------------------
instr 1
  ipch = p4
  ivel = p5 / 127
  ich = 1

  ; Pitch with bend
  kpch MidiPitch ipch, ich

  ; Mod wheel controls filter cutoff
  kmod chnget "cc1_ch1"

  ; Envelope
  kenv madsr 0.01, 0.2, 0.7, 0.3

  ; Two detuned saws
  a1 vco2 0.3, kpch
  a2 vco2 0.15, kpch * 1.003

  ; Low-pass filter
  kcutoff = 500 + kmod * 4000 + kenv * 2000
  afilt moogladder a1 + a2, kcutoff, 0.3

  aout = afilt * kenv * ivel
  outs aout, aout
endin

;; ----------------------------------------------------------------------------
;; Instrument 2: FM Piano
;; Bell-like attack, quick decay
;; ----------------------------------------------------------------------------
instr 2
  ipch = p4
  ivel = p5 / 127
  ich = 2

  kpch MidiPitch ipch, ich
  kenv madsr 0.001, 0.4, 0.2, 0.5

  ; Simple FM synthesis (carrier + modulator)
  kmodidx = 3 * kenv  ; Modulation index decreases with envelope
  amod oscili kmodidx * kpch, kpch * 2.01
  acar oscili 0.4, kpch + amod

  aout = acar * kenv * ivel
  outs aout, aout
endin

;; ----------------------------------------------------------------------------
;; Instrument 3: Pad (slow attack, long release)
;; Ambient, sustained sounds
;; ----------------------------------------------------------------------------
instr 3
  ipch = p4
  ivel = p5 / 127
  ich = 3

  kpch MidiPitch ipch, ich
  kenv madsr 0.5, 0.3, 0.8, 1.0

  ; Super saw with PWM
  a1 vco2 0.2, kpch, 4, 0.5 + lfo(0.3, 0.2)
  a2 vco2 0.1, kpch * 0.998
  a3 vco2 0.1, kpch * 1.002

  ; Gentle filter
  amix = a1 + a2 + a3
  afilt butterlp amix, 3000

  aout = afilt * kenv * ivel
  outs aout, aout
endin

;; ----------------------------------------------------------------------------
;; Instrument 4: Pluck (Karplus-Strong)
;; Guitar/harp-like strings
;; ----------------------------------------------------------------------------
instr 4
  ipch = p4
  ivel = p5 / 127
  ich = 4

  ifreq = cpsmidinn(ipch)

  ; Karplus-Strong plucked string
  apluck pluck ivel * 0.8, ifreq, ifreq, 0, 1

  ; Quick decay envelope
  kenv expsegr 1, 0.01, 0.5, 2, 0.001, 0.2, 0.001

  aout = apluck * kenv
  outs aout, aout
endin

;; ----------------------------------------------------------------------------
;; Instrument 5: Organ
;; Additive harmonics, sustained
;; ----------------------------------------------------------------------------
instr 5
  ipch = p4
  ivel = p5 / 127
  ich = 5

  kpch MidiPitch ipch, ich
  kenv linsegr 0, 0.01, 1, 0.1, 0.8, 0.1, 0

  ; Drawbar organ simulation
  a1 oscili 0.4, kpch         ; Fundamental
  a2 oscili 0.2, kpch * 2     ; 2nd harmonic
  a3 oscili 0.15, kpch * 3    ; 3rd harmonic
  a4 oscili 0.1, kpch * 4     ; 4th harmonic

  aout = (a1 + a2 + a3 + a4) * kenv * ivel
  outs aout, aout
endin

;; ----------------------------------------------------------------------------
;; Instrument 6: Bass
;; Deep, punchy low-end
;; ----------------------------------------------------------------------------
instr 6
  ipch = p4
  ivel = p5 / 127
  ich = 6

  kpch MidiPitch ipch, ich
  kenv madsr 0.005, 0.1, 0.6, 0.15

  ; Sub oscillator + main
  asub oscili 0.4, kpch * 0.5
  amain vco2 0.3, kpch

  ; Filter with envelope
  afilt moogladder asub + amain, 800 + kenv * 1500, 0.2

  aout = afilt * kenv * ivel
  outs aout, aout
endin

;; ----------------------------------------------------------------------------
;; Instrument 7: Strings (ensemble)
;; Slow attack, lush sound
;; ----------------------------------------------------------------------------
instr 7
  ipch = p4
  ivel = p5 / 127
  ich = 7

  kpch MidiPitch ipch, ich
  kenv madsr 0.3, 0.2, 0.9, 0.5

  ; Detuned unison for ensemble effect
  a1 vco2 0.15, kpch * 0.998
  a2 vco2 0.15, kpch
  a3 vco2 0.15, kpch * 1.002

  ; Gentle LPF
  amix = a1 + a2 + a3
  afilt butterlp amix, 4000

  aout = afilt * kenv * ivel
  outs aout, aout
endin

;; ----------------------------------------------------------------------------
;; Instrument 8: Brass
;; Bold attack, filter sweep
;; ----------------------------------------------------------------------------
instr 8
  ipch = p4
  ivel = p5 / 127
  ich = 8

  kpch MidiPitch ipch, ich
  kenv madsr 0.08, 0.15, 0.7, 0.2

  ; Sawtooth
  asaw vco2 0.5, kpch

  ; Filter envelope for brass character
  kfilt expseg 200, 0.1, 3000, 0.5, 1500
  afilt moogladder asaw, kfilt, 0.1

  aout = afilt * kenv * ivel
  outs aout, aout
endin

;; ----------------------------------------------------------------------------
;; Instruments 9-16: Additional channels (copy of 1-8 for now)
;; These can be customized as needed
;; ----------------------------------------------------------------------------
instr 9
  ipch = p4
  ivel = p5 / 127
  kpch MidiPitch ipch, 9
  kenv madsr 0.01, 0.2, 0.7, 0.3
  a1 vco2 0.3, kpch
  afilt moogladder a1, 2000 + kenv * 2000, 0.3
  aout = afilt * kenv * ivel
  outs aout, aout
endin

instr 10  ; Drums channel - simple noise/click
  ipch = p4
  ivel = p5 / 127

  ; Different sounds based on pitch
  if (ipch == 36 || ipch == 35) then  ; Kick
    kenv expon 1, 0.3, 0.001
    aout oscili ivel * 0.8, 60 * kenv
  elseif (ipch == 38 || ipch == 40) then  ; Snare
    kenv expon 1, 0.2, 0.001
    anoise noise 0.5, 0.5
    aout = anoise * kenv * ivel
  elseif (ipch >= 42 && ipch <= 46) then  ; Hi-hat
    kenv expon 1, 0.1, 0.001
    anoise noise 0.3, 0.8
    aout = anoise * kenv * ivel
  else
    aout = 0
  endif

  outs aout, aout
endin

instr 11, 12, 13, 14, 15, 16
  ; Generic synth for remaining channels
  ipch = p4
  ivel = p5 / 127
  ifreq = cpsmidinn(ipch)
  kenv madsr 0.01, 0.2, 0.7, 0.3
  a1 oscili 0.4, ifreq
  aout = a1 * kenv * ivel
  outs aout, aout
endin

</CsInstruments>

<CsScore>
; Run indefinitely (24 hours)
f0 86400
</CsScore>
</CsoundSynthesizer>
