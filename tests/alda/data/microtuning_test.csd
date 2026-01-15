<CsoundSynthesizer>
<CsOptions>
-odac -d -m0
</CsOptions>
<CsInstruments>
sr = 44100
ksmps = 32
nchnls = 2
0dbfs = 1

; Simple sine instrument that handles both MIDI pitch and frequency
; If p4 > 200, treat it as frequency in Hz
; If p4 <= 127, treat it as MIDI note number
instr 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
    ipitch = p4
    iamp = p5 / 127  ; velocity to amplitude

    ; Determine if p4 is frequency or MIDI pitch
    if (ipitch > 200) then
        ifreq = ipitch  ; Already frequency
    else
        ifreq = cpsmidinn(ipitch)  ; Convert MIDI to frequency
    endif

    ; Simple ADSR envelope
    kenv madsr 0.01, 0.1, 0.7, 0.2

    ; Generate sine wave
    asig oscili iamp * kenv, ifreq

    ; Stereo output
    outs asig, asig
endin

</CsInstruments>
<CsScore>
; Empty score - events come from real-time input
</CsScore>
</CsoundSynthesizer>
