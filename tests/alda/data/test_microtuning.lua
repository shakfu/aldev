-- Manual microtuning test with Csound
-- Run from psnd REPL: dofile("../tests/alda/data/test_microtuning.lua")

print("=== Microtuning Test with Csound ===")
print("")

-- Check if Csound is available
if not loki.alda.csound_available() then
    print("ERROR: Csound backend not available")
    print("Rebuild with: cmake .. -DBUILD_CSOUND_BACKEND=ON && make")
    return
end
print("Csound backend available")

-- Initialize alda if needed
if not loki.alda.is_initialized() then
    print("Initializing Alda...")
    loki.alda.init()
end

-- Load the Csound instrument
local csd_path = "../tests/alda/data/microtuning_test.csd"
print("Loading Csound instrument: " .. csd_path)
local ok, err = loki.alda.csound_load(csd_path)
if not ok then
    print("ERROR: Failed to load Csound: " .. (err or "unknown"))
    return
end
print("Csound loaded successfully")

-- Enable Csound backend
loki.alda.set_csound(true)
print("Csound backend enabled")
print("")

-- Load the 12-note just intonation scale
local scl_path = "../tests/alda/data/just_12.scl"
print("Loading scale: " .. scl_path)
ok, err = loki.scala.load(scl_path)
if not ok then
    print("ERROR: Failed to load scale: " .. (err or "unknown"))
    return
end
print("Scale loaded: 12-note Just Intonation (5-limit)")
print("")

-- Create a part and assign the scale
print("Creating piano part with just intonation tuning...")
loki.alda.eval_sync("piano: r")  -- Initialize the part with a rest
ok, err = loki.alda.set_part_scale("piano", 60, 261.6255653)  -- C4 root
if not ok then
    print("ERROR: Failed to set scale: " .. (err or "unknown"))
    return
end
print("Scale assigned to piano (root = C4 @ 261.63 Hz)")
print("")

-- Play comparison: 12-TET vs Just Intonation
print("=== Playing C Major Chord in Just Intonation ===")
print("")
print("Frequencies:")
print("  C4 = 261.63 Hz (1/1)")
print("  E4 = 327.03 Hz (5/4 = 1.25x) - 14 cents FLATTER than 12-TET")
print("  G4 = 392.44 Hz (3/2 = 1.5x)  - 2 cents SHARPER than 12-TET")
print("")

-- Play a C major chord (whole note)
print("Playing chord...")
loki.alda.eval_sync("piano: c1/e/g")

print("")
print("The just major third (5/4) should sound noticeably")
print("smoother than the 12-TET major third (400 cents).")
print("")

-- Play arpeggio
print("Now playing C major arpeggio...")
loki.alda.eval_sync("piano: c4 e4 g4 >c4")

print("")
print("=== Test Complete ===")
print("")
print("Commands to try:")
print("  loki.alda.eval_sync('piano: c d e f g a b >c')  -- Play scale")
print("  loki.alda.clear_part_scale('piano')             -- Return to 12-TET")
print("  loki.alda.csound_stop()                         -- Stop Csound")
