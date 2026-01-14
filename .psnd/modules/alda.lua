-- Alda Module - Lua wrapper for the loki.alda C API
--
-- This module provides convenience functions and documentation for the
-- loki.alda subsystem which handles Alda music notation parsing and
-- MIDI playback via the built-in TinySoundFont synthesizer.
--
-- The underlying C API (loki.alda.*) is automatically available when
-- the editor starts. This module adds higher-level helpers.
--
-- ==============================================================================
-- loki.alda C API Reference
-- ==============================================================================
--
-- Initialization:
--   loki.alda.init([port_name])      Initialize the Alda subsystem
--                                    port_name: optional MIDI port name (default: "Loki")
--                                    Returns: true on success, nil + error on failure
--
--   loki.alda.cleanup()              Cleanup and release all resources
--                                    Stops all playback, releases MIDI port
--
--   loki.alda.is_initialized()       Check if Alda is ready
--                                    Returns: boolean
--
-- Playback:
--   loki.alda.eval(code, [callback]) Evaluate and play Alda code asynchronously
--                                    code: Alda notation string (e.g., "piano: c d e f g")
--                                    callback: optional Lua function name to call on completion
--                                    Returns: slot ID (0-7) on success, nil + error on failure
--
--   loki.alda.eval_sync(code)        Evaluate and play Alda code synchronously (blocking)
--                                    code: Alda notation string
--                                    Returns: true on success, nil + error on failure
--
--   loki.alda.stop([slot])           Stop playback
--                                    slot: slot ID (0-7), or nil/-1 to stop all
--
--   loki.alda.stop_all()             Stop all active playback
--
-- Status:
--   loki.alda.is_playing()           Check if any slot is playing
--                                    Returns: boolean
--
--   loki.alda.active_count()         Get number of active playback slots
--                                    Returns: integer (0-8)
--
-- Configuration:
--   loki.alda.set_tempo(bpm)         Set global tempo (20-400 BPM)
--                                    bpm: beats per minute
--
--   loki.alda.get_tempo()            Get current tempo
--                                    Returns: integer BPM
--
--   loki.alda.set_synth(enabled)     Enable/disable built-in synthesizer
--                                    enabled: boolean
--                                    Returns: true on success, nil + error on failure
--
--   loki.alda.load_soundfont(path)   Load a SoundFont file for synthesis
--                                    path: path to .sf2 file
--                                    Returns: true on success, nil + error on failure
--
-- Error Handling:
--   loki.alda.get_error()            Get last error message
--                                    Returns: string or nil
--
-- ==============================================================================
-- Playback Slots
-- ==============================================================================
--
-- The Alda subsystem supports up to 8 concurrent playback slots (0-7).
-- Each call to loki.alda.eval() allocates a slot that is freed when
-- playback completes or is stopped.
--
-- ==============================================================================
-- Callback Format
-- ==============================================================================
--
-- When using async eval with a callback, the callback receives a result table:
--   function on_playback_complete(result)
--       result.status        -- "complete", "stopped", or "error"
--       result.slot_id       -- which slot finished (0-7)
--       result.error_msg     -- error message if status == "error"
--       result.events_played -- number of MIDI events played
--       result.duration_ms   -- total playback duration in milliseconds
--   end
--
-- ==============================================================================

local M = {}

-- Detect execution mode
local MODE = nil
local function get_mode()
    if MODE then return MODE end
    MODE = loki.get_lines and "editor" or "repl"
    return MODE
end

-- Status message helper (mode-aware)
local function status(msg)
    if get_mode() == "editor" then
        loki.status(msg)
    else
        print(msg)
    end
end

-- ==============================================================================
-- Initialization Helpers
-- ==============================================================================

-- Initialize with optional auto-load of default soundfont
function M.setup(opts)
    opts = opts or {}

    -- Initialize alda subsystem
    local ok, err = loki.alda.init(opts.port_name)
    if not ok then
        status("Alda init failed: " .. (err or "unknown"))
        return false, err
    end

    -- Load soundfont if specified
    if opts.soundfont then
        ok, err = loki.alda.load_soundfont(opts.soundfont)
        if not ok then
            status("Soundfont load failed: " .. (err or "unknown"))
            -- Continue anyway, MIDI output still works
        end
    end

    -- Set initial tempo if specified
    if opts.tempo then
        loki.alda.set_tempo(opts.tempo)
    end

    -- Enable synth if soundfont loaded
    if opts.soundfont and opts.synth ~= false then
        loki.alda.set_synth(true)
    end

    status("Alda ready")
    return true
end

-- ==============================================================================
-- Playback Helpers
-- ==============================================================================

-- Play the current file (editor mode)
function M.play_file()
    if get_mode() ~= "editor" then
        print("alda.play_file() requires editor mode")
        return false
    end

    if not loki.alda.is_initialized() then
        status("Alda not initialized")
        return false
    end

    -- Get entire buffer content
    local lines = {}
    local num_lines = loki.get_lines()
    for i = 0, num_lines - 1 do
        local line = loki.get_line(i)
        if line then
            table.insert(lines, line)
        end
    end
    local code = table.concat(lines, "\n")

    if code == "" then
        status("Empty buffer")
        return false
    end

    local slot, err = loki.alda.eval(code)
    if not slot then
        status("Play failed: " .. (err or "unknown"))
        return false
    end

    status("Playing (slot " .. slot .. ")")
    return true, slot
end

-- Play current line (editor mode)
function M.play_line()
    if get_mode() ~= "editor" then
        print("alda.play_line() requires editor mode")
        return false
    end

    if not loki.alda.is_initialized() then
        status("Alda not initialized")
        return false
    end

    local row, _ = loki.get_cursor()
    local line = loki.get_line(row)

    if not line or line == "" then
        status("Empty line")
        return false
    end

    local slot, err = loki.alda.eval(line)
    if not slot then
        status("Play failed: " .. (err or "unknown"))
        return false
    end

    status("Playing line (slot " .. slot .. ")")
    return true, slot
end

-- Play a string of Alda code
function M.play(code)
    if not loki.alda.is_initialized() then
        status("Alda not initialized")
        return false
    end

    if not code or code == "" then
        status("No code to play")
        return false
    end

    local slot, err = loki.alda.eval(code)
    if not slot then
        status("Play failed: " .. (err or "unknown"))
        return false
    end

    status("Playing (slot " .. slot .. ")")
    return true, slot
end

-- Play synchronously (blocking)
function M.play_sync(code)
    if not loki.alda.is_initialized() then
        status("Alda not initialized")
        return false
    end

    if not code or code == "" then
        status("No code to play")
        return false
    end

    status("Playing...")
    local ok, err = loki.alda.eval_sync(code)
    if not ok then
        status("Play failed: " .. (err or "unknown"))
        return false
    end

    status("Playback complete")
    return true
end

-- Stop all playback
function M.stop()
    loki.alda.stop_all()
    status("Stopped")
end

-- ==============================================================================
-- Status Helpers
-- ==============================================================================

-- Get playback status as string
function M.status_string()
    if not loki.alda.is_initialized() then
        return "not initialized"
    end

    local count = loki.alda.active_count()
    if count == 0 then
        return "idle"
    else
        return string.format("playing (%d slot%s)", count, count > 1 and "s" or "")
    end
end

-- Print full status info
function M.info()
    local initialized = loki.alda.is_initialized()
    print("Alda Status:")
    print("  Initialized: " .. (initialized and "yes" or "no"))

    if initialized then
        print("  Tempo:       " .. loki.alda.get_tempo() .. " BPM")
        print("  Playing:     " .. (loki.alda.is_playing() and "yes" or "no"))
        print("  Active:      " .. loki.alda.active_count() .. " slots")

        local err = loki.alda.get_error()
        if err then
            print("  Last error:  " .. err)
        end
    end
end

-- ==============================================================================
-- Register with REPL help
-- ==============================================================================

if loki.repl and loki.repl.register then
    loki.repl.register("alda.setup", "Initialize Alda: alda.setup({soundfont='path.sf2'})")
    loki.repl.register("alda.play", "Play Alda code: alda.play('piano: c d e')")
    loki.repl.register("alda.play_sync", "Play synchronously: alda.play_sync('piano: c')")
    loki.repl.register("alda.play_file", "Play current buffer (editor mode)")
    loki.repl.register("alda.play_line", "Play current line (editor mode)")
    loki.repl.register("alda.stop", "Stop all playback")
    loki.repl.register("alda.info", "Show Alda status info")
end

return M
