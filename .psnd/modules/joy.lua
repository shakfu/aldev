-- Joy Module - Lua wrapper for the loki.joy C API
--
-- This module provides convenience functions and documentation for the
-- loki.joy subsystem which handles Joy stack-based music programming
-- and MIDI playback via the built-in TinySoundFont synthesizer.
--
-- The underlying C API (loki.joy.*) is automatically available when
-- the editor starts. This module adds higher-level helpers.
--
-- ==============================================================================
-- loki.joy C API Reference
-- ==============================================================================
--
-- Initialization:
--   loki.joy.init()               Initialize the Joy subsystem
--                                 Returns: true on success, nil + error on failure
--
--   loki.joy.cleanup()            Cleanup and release all resources
--                                 Stops all playback, releases MIDI port
--
--   loki.joy.is_initialized()     Check if Joy is ready
--                                 Returns: boolean
--
-- Evaluation:
--   loki.joy.eval(code)           Evaluate Joy code synchronously
--                                 code: Joy notation string (e.g., "c d e play")
--                                 Returns: true on success, nil + error on failure
--
--   loki.joy.load(path)           Load and evaluate a Joy source file
--                                 path: path to .joy file
--                                 Returns: true on success, nil + error on failure
--
--   loki.joy.define(name, body)   Define a new Joy word
--                                 name: word name (e.g., "my-chord")
--                                 body: Joy code for the word body
--                                 Returns: true on success, nil + error on failure
--
-- Playback:
--   loki.joy.stop()               Stop all MIDI playback and send panic
--
-- MIDI Ports:
--   loki.joy.list_ports()         List available MIDI output ports
--                                 Returns: table of port names
--
--   loki.joy.open_port(index)     Open a MIDI output port by index (0-based)
--                                 Returns: true on success, nil + error on failure
--
--   loki.joy.open_virtual(name)   Create a virtual MIDI output port
--                                 name: port name (default: "PSND_MIDI")
--                                 Returns: true on success, nil + error on failure
--
-- Stack Operations:
--   loki.joy.push(value)          Push a value onto the Joy stack
--                                 value: number or string
--
--   loki.joy.stack_depth()        Get the current stack depth
--                                 Returns: integer
--
--   loki.joy.stack_clear()        Clear the Joy stack
--
--   loki.joy.stack_print()        Print the Joy stack (for debugging)
--
-- Error Handling:
--   loki.joy.get_error()          Get last error message
--                                 Returns: string or nil
--
-- Extension:
--   loki.joy.register_primitive(name, callback)
--                                 Register a Lua function as a Joy word
--                                 name: word name (e.g., "lua-double")
--                                 callback: function(stack) -> stack or nil, error
--                                   stack: Lua array (index 1 = bottom, #stack = top)
--                                   Return: modified stack, or nil + error string
--                                 Returns: true on success, nil + error on failure
--
--   Stack values in callbacks:
--     integers, floats, booleans, strings -> Lua native types
--     lists -> Lua arrays
--     quotations -> {type="quotation", value={...terms...}}
--     symbols -> {type="symbol", value="name"}
--     sets -> {type="set", value=number}
--
--   Example:
--     loki.joy.register_primitive("lua-double", function(stack)
--         if #stack < 1 then return nil, "stack underflow" end
--         local top = table.remove(stack)
--         table.insert(stack, top * 2)
--         return stack
--     end)
--
-- ==============================================================================
-- Joy Music Primitives (partial list)
-- ==============================================================================
--
-- Notes:
--   c d e f g a b                 Note names (pushes MIDI note number)
--   c# db                         Sharps and flats
--   c4 c5                         Octave specification
--   note                          Play note: velocity duration note-num -> plays note
--   play                          Play stack contents as chord
--
-- Rhythm:
--   q h w                         Quarter, half, whole note durations
--   e s                           Eighth, sixteenth note durations
--
-- Chords:
--   major minor dom7 maj7         Chord constructors
--   dim aug sus4 sus2             More chord types
--
-- Control:
--   tempo                         Set tempo: bpm -> sets tempo
--   channel                       Set MIDI channel: ch -> sets channel
--   velocity                      Set velocity: vel -> sets velocity
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

-- Initialize with options
function M.setup(opts)
    opts = opts or {}

    -- Initialize Joy subsystem
    local ok, err = loki.joy.init()
    if not ok then
        status("Joy init failed: " .. (err or "unknown"))
        return false, err
    end

    -- Open MIDI port if specified
    if opts.port_name then
        ok, err = loki.joy.open_virtual(opts.port_name)
        if not ok then
            status("MIDI port failed: " .. (err or "unknown"))
        end
    elseif opts.port_index then
        ok, err = loki.joy.open_port(opts.port_index)
        if not ok then
            status("MIDI port failed: " .. (err or "unknown"))
        end
    end

    status("Joy ready")
    return true
end

-- ==============================================================================
-- Evaluation Helpers
-- ==============================================================================

-- Evaluate Joy code with error handling
function M.eval(code)
    if not loki.joy.is_initialized() then
        status("Joy not initialized")
        return false
    end

    if not code or code == "" then
        status("No code to evaluate")
        return false
    end

    local ok, err = loki.joy.eval(code)
    if not ok then
        status("Eval failed: " .. (err or "unknown"))
        return false
    end

    return true
end

-- Evaluate current buffer (editor mode)
function M.eval_file()
    if get_mode() ~= "editor" then
        print("joy.eval_file() requires editor mode")
        return false
    end

    if not loki.joy.is_initialized() then
        status("Joy not initialized")
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

    local ok, err = loki.joy.eval(code)
    if not ok then
        status("Eval failed: " .. (err or "unknown"))
        return false
    end

    status("Evaluated")
    return true
end

-- Evaluate current line (editor mode)
function M.eval_line()
    if get_mode() ~= "editor" then
        print("joy.eval_line() requires editor mode")
        return false
    end

    if not loki.joy.is_initialized() then
        status("Joy not initialized")
        return false
    end

    local row, _ = loki.get_cursor()
    local line = loki.get_line(row)

    if not line or line == "" then
        status("Empty line")
        return false
    end

    local ok, err = loki.joy.eval(line)
    if not ok then
        status("Eval failed: " .. (err or "unknown"))
        return false
    end

    status("Evaluated line")
    return true
end

-- Define a new word
function M.define(name, body)
    if not loki.joy.is_initialized() then
        status("Joy not initialized")
        return false
    end

    local ok, err = loki.joy.define(name, body)
    if not ok then
        status("Define failed: " .. (err or "unknown"))
        return false
    end

    status("Defined: " .. name)
    return true
end

-- Stop all playback
function M.stop()
    loki.joy.stop()
    status("Stopped")
end

-- ==============================================================================
-- Stack Helpers
-- ==============================================================================

-- Push multiple values
function M.push_all(...)
    for _, v in ipairs({...}) do
        loki.joy.push(v)
    end
end

-- Get stack info
function M.stack_info()
    return {
        depth = loki.joy.stack_depth()
    }
end

-- ==============================================================================
-- Status Helpers
-- ==============================================================================

-- Print full status info
function M.info()
    local initialized = loki.joy.is_initialized()
    print("Joy Status:")
    print("  Initialized: " .. (initialized and "yes" or "no"))

    if initialized then
        print("  Stack depth: " .. loki.joy.stack_depth())

        local err = loki.joy.get_error()
        if err then
            print("  Last error:  " .. err)
        end
    end
end

-- List available MIDI ports
function M.ports()
    local ports = loki.joy.list_ports()
    if not ports or #ports == 0 then
        print("No MIDI ports available")
        return
    end

    print("Available MIDI ports:")
    for i, name in ipairs(ports) do
        print(string.format("  %d: %s", i - 1, name))
    end
end

-- ==============================================================================
-- Extension Helpers
-- ==============================================================================

-- Register a Lua function as a Joy primitive
-- Example: joy.primitive("double", function(stack) ... return stack end)
function M.primitive(name, callback)
    if not loki.joy.is_initialized() then
        status("Joy not initialized")
        return false
    end

    if type(name) ~= "string" or name == "" then
        status("Primitive name must be a non-empty string")
        return false
    end

    if type(callback) ~= "function" then
        status("Callback must be a function")
        return false
    end

    local ok, err = loki.joy.register_primitive(name, callback)
    if not ok then
        status("Register failed: " .. (err or "unknown"))
        return false
    end

    status("Registered: " .. name)
    return true
end

-- ==============================================================================
-- Register with REPL help
-- ==============================================================================

if loki.repl and loki.repl.register then
    loki.repl.register("joy.setup", "Initialize Joy: joy.setup({port_name='PSND_MIDI'})")
    loki.repl.register("joy.eval", "Evaluate Joy code: joy.eval('c d e play')")
    loki.repl.register("joy.eval_file", "Evaluate current buffer (editor mode)")
    loki.repl.register("joy.eval_line", "Evaluate current line (editor mode)")
    loki.repl.register("joy.define", "Define a word: joy.define('my-chord', 'c e g')")
    loki.repl.register("joy.primitive", "Register Lua primitive: joy.primitive('name', function(stack) ... end)")
    loki.repl.register("joy.stop", "Stop all playback")
    loki.repl.register("joy.info", "Show Joy status info")
    loki.repl.register("joy.ports", "List available MIDI ports")
end

return M
