-- Alda Keybindings
-- Customize Alda-related keyboard shortcuts here
--
-- Default bindings:
--   Ctrl-E  : Evaluate selection or current part
--   Ctrl-P  : Play entire file
--   Ctrl-G  : Stop all playback
--
-- These override the built-in C keybindings when loaded.

local M = {}

-- Helpers
local function is_csd_file()
    local filename = loki.get_filename()
    if not filename then return false end
    return filename:sub(-4):lower() == ".csd"
end

local function get_buffer_contents()
    local lines = {}
    local num_lines = loki.get_lines()
    for i = 0, num_lines - 1 do
        local line = loki.get_line(i)
        if line then
            table.insert(lines, line)
        end
    end
    return table.concat(lines, "\n")
end

-- Check if a line is an Alda part declaration (e.g., "piano:", "trumpet/trombone:")
local function is_part_declaration(line)
    if not line or line == "" then return false end
    -- Skip leading whitespace
    local trimmed = line:match("^%s*(.-)%s*$")
    if not trimmed or trimmed == "" then return false end
    -- Must start with a letter and contain a colon
    if not trimmed:match("^[a-zA-Z]") then return false end
    -- Look for unquoted colon
    local in_quotes = false
    for i = 1, #trimmed do
        local c = trimmed:sub(i, i)
        if c == '"' then
            in_quotes = not in_quotes
        elseif c == ':' and not in_quotes then
            return true
        end
    end
    return false
end

-- Get the Alda part containing the cursor
local function get_current_part()
    local num_lines = loki.get_lines()
    if num_lines == 0 then return nil end

    local cursor_row, cursor_col = loki.get_cursor()
    dbg("get_current_part: cursor_row=" .. cursor_row .. ", num_lines=" .. num_lines)

    -- Show what's at cursor row
    local cursor_line = loki.get_line(cursor_row)
    dbg("  cursor line content: [" .. (cursor_line or "nil") .. "]")

    -- Find start of part: scan backward for part declaration
    local start_row = cursor_row
    while start_row >= 0 do
        local line = loki.get_line(start_row)
        dbg("  scan back row " .. start_row .. ": [" .. (line or "nil") .. "] is_part=" .. tostring(is_part_declaration(line)))
        if is_part_declaration(line) then
            break
        end
        start_row = start_row - 1
    end
    dbg("  start_row=" .. start_row)

    -- If we didn't find a part declaration, we're in the header
    if start_row < 0 then
        dbg("  NO PART FOUND - in header section")
        return nil
    end

    -- Find end of part: scan forward for next part declaration
    local end_row = cursor_row + 1
    while end_row < num_lines do
        local line = loki.get_line(end_row)
        if is_part_declaration(line) then
            dbg("  found end at row " .. end_row .. ": [" .. (line or "nil") .. "]")
            break
        end
        end_row = end_row + 1
    end
    dbg("  end_row=" .. end_row)

    -- Collect lines from start to end (exclusive)
    local lines = {}
    for i = start_row, end_row - 1 do
        local line = loki.get_line(i)
        if line then
            table.insert(lines, line)
        end
    end

    return table.concat(lines, "\n")
end

-- ==============================================================================
-- Keybinding Functions
-- ==============================================================================

-- Evaluate selection or current part as Alda (Ctrl-E)
function M.eval_part()
    dbg("eval_part() called")

    -- Try to get selection first
    local code = loki.get_selection and loki.get_selection()
    dbg("  selection: " .. (code and #code .. " chars" or "nil"))

    -- If no selection, get current part
    if not code or code == "" then
        code = get_current_part()
        dbg("  get_current_part: " .. (code and #code .. " chars" or "nil"))
    end

    if not code or code == "" then
        dbg("  NO CODE - returning")
        loki.status("No code to evaluate")
        return
    end

    dbg("  code ready, " .. #code .. " chars")
    dbg("  code content: [" .. code:gsub("\n", "\\n") .. "]")

    -- Initialize Alda if needed
    local init_ok = loki.alda.is_initialized()
    dbg("  alda initialized: " .. tostring(init_ok))

    if not init_ok then
        dbg("  initializing alda...")
        local ok, err = loki.alda.init()
        if not ok then
            dbg("  init FAILED: " .. (err or "unknown"))
            loki.status("Alda init failed: " .. (err or "unknown"))
            return
        end
        dbg("  init OK")
    end

    dbg("  calling loki.alda.eval...")
    local slot, err = loki.alda.eval(code)
    if slot then
        dbg("  eval OK, slot " .. slot)
        local msg = string.format("Playing part (slot %d)", slot)
        dbg("  setting status: " .. msg)
        loki.status(msg)
    else
        dbg("  eval FAILED: " .. (err or "unknown"))
        local msg = "Alda error: " .. (err or "eval failed")
        dbg("  setting status: " .. msg)
        loki.status(msg)
    end
    dbg("eval_part() done")
end

-- Play entire file as Alda or Csound (Ctrl-P)
function M.play_file()
    -- Check if this is a CSD file
    if is_csd_file() then
        local filename = loki.get_filename()
        if not filename then
            loki.status("No file to play")
            return
        end

        -- Check if Csound is available
        if not loki.alda.csound_available() then
            loki.status("Csound backend not available")
            return
        end

        -- Play the CSD file
        local ok, err = loki.alda.csound_play(filename)
        if ok then
            loki.status("Playing CSD: " .. filename)
        else
            loki.status("Csound error: " .. (err or "playback failed"))
        end
        return
    end

    -- Otherwise, treat as Alda file
    local code = get_buffer_contents()

    if not code or code == "" then
        loki.status("Empty file")
        return
    end

    -- Initialize Alda if needed
    if not loki.alda.is_initialized() then
        local ok, err = loki.alda.init()
        if not ok then
            loki.status("Alda init failed: " .. (err or "unknown"))
            return
        end
    end

    -- Play the file
    local slot, err = loki.alda.eval(code)
    if slot then
        loki.status(string.format("Playing file (slot %d)", slot))
    else
        loki.status("Alda error: " .. (err or "eval failed"))
    end
end

-- Stop all playback (Ctrl-G)
function M.stop()
    -- Stop Csound playback if active
    if loki.alda.csound_playing and loki.alda.csound_playing() then
        loki.alda.csound_stop()
        loki.status("Stopped")
        return
    end

    -- Stop Alda playback
    if loki.alda.is_initialized() then
        loki.alda.stop_all()
        loki.status("Stopped")
    else
        loki.status("Nothing playing")
    end
end

-- ==============================================================================
-- Register Keybindings
-- ==============================================================================

function M.setup(opts)
    dbg("alda_keys.setup() called")
    opts = opts or {}

    -- Default key mappings (can be overridden via opts)
    local keys = {
        eval_part = opts.eval_part or '<C-e>',
        play_file = opts.play_file or '<C-p>',
        stop = opts.stop or '<C-g>',
    }

    -- Modes to bind (default: normal and insert)
    local modes = opts.modes or 'ni'
    dbg("  modes: " .. modes)

    -- Debug: verify loki.keymap exists
    if not loki.keymap then
        dbg("  ERROR: loki.keymap not available!")
        return
    end

    -- Register keybindings
    if keys.eval_part then
        dbg("  registering eval_part: " .. keys.eval_part)
        local ok, err = pcall(loki.keymap, modes, keys.eval_part, M.eval_part, "Evaluate Alda selection/part")
        dbg("    result: " .. (ok and "OK" or "FAILED: " .. tostring(err)))
    end

    if keys.play_file then
        dbg("  registering play_file: " .. keys.play_file)
        local ok, err = pcall(loki.keymap, modes, keys.play_file, M.play_file, "Play entire file as Alda")
        dbg("    result: " .. (ok and "OK" or "FAILED: " .. tostring(err)))
    end

    if keys.stop then
        dbg("  registering stop: " .. keys.stop)
        local ok, err = pcall(loki.keymap, modes, keys.stop, M.stop, "Stop Alda playback")
        dbg("    result: " .. (ok and "OK" or "FAILED: " .. tostring(err)))
    end

    dbg("alda_keys.setup() done")
    loki.status("Alda keybindings loaded")
end

-- Auto-setup with defaults if running in editor mode
if loki.get_lines then
    M.setup()
end

return M
