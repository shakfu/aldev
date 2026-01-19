-- ==============================================================================
-- Psnd Editor Configuration
-- ==============================================================================
--
-- This is the main configuration file for the Psnd editor.
-- Loading priority:
--   1. .psnd/init.lua (local, project-specific)
--   2. ~/.psnd/init.lua (global, home directory)
--
-- ==============================================================================

-- Detect execution mode: "editor" or "repl"
MODE = loki.get_lines and "editor" or "repl"

-- ==============================================================================
-- Configure Module Path
-- ==============================================================================

-- Add .psnd/modules and .psnd/keybindings to package.path for require()
package.path = package.path .. ";.psnd/modules/?.lua;.psnd/keybindings/?.lua"

-- Also add global config path if available
-- Note: os library may not be available in sandboxed Lua
local home = os and os.getenv and os.getenv("HOME")
if home then
    package.path = package.path .. ";" .. home .. "/.psnd/modules/?.lua"
    package.path = package.path .. ";" .. home .. "/.psnd/keybindings/?.lua"
end

-- ==============================================================================
-- Load Core Modules
-- ==============================================================================

-- Load language definitions (lazy loading - loads on file open)
languages = require("languages")
local ext_count = languages.init()

-- Load theme utilities
theme = require("theme")

-- Load Alda integration module
alda = require("alda")

-- ==============================================================================
-- Editor Settings
-- ==============================================================================

if MODE == "editor" then
    -- Load a color theme
    local ok, err = theme.load("nord")
    if not ok and err then
        loki.status("Theme: " .. tostring(err))
    end

    -- Enable line numbers (added in latest version)
    loki.line_numbers(true)
end

-- ==============================================================================
-- Ableton Link Configuration
-- ==============================================================================
-- Ableton Link enables tempo synchronization with other Link-enabled apps
-- (Ableton Live, other DAWs, iOS apps, etc.) on the same network.

if MODE == "editor" then
    -- Initialize Link with starting tempo (required before other Link calls)
    -- loki.link.init(120)

    -- Enable Link network synchronization
    -- When enabled, tempo syncs with other Link peers on the network
    -- loki.link.enable(true)

    -- Set session tempo (propagates to all connected peers)
    -- loki.link.set_tempo(120)

    -- Launch Quantization: wait for beat/bar boundary before starting playback
    -- This ensures notes land on the same beats as other Link peers
    -- Values: 0 = immediate (default), 1 = next beat, 4 = next bar (4/4 time)
    -- loki.link.launch_quantize(4)

    -- Enable start/stop sync (transport sync with peers)
    -- When enabled, play/stop commands sync across all Link peers
    -- loki.link.start_stop_sync(true)

    -- Register callbacks for Link events (optional)
    -- These functions will be called when Link state changes:
    --
    -- Peer count changed (num_peers = number of other connected apps):
    -- function on_link_peers(num_peers)
    --     loki.status("Link peers: " .. num_peers)
    -- end
    -- loki.link.on_peers("on_link_peers")
    --
    -- Tempo changed (tempo = new BPM from network):
    -- function on_link_tempo(tempo)
    --     loki.status(string.format("Link tempo: %.1f", tempo))
    -- end
    -- loki.link.on_tempo("on_link_tempo")
    --
    -- Transport state changed (is_playing = true/false):
    -- function on_link_transport(is_playing)
    --     loki.status("Link transport: " .. (is_playing and "playing" or "stopped"))
    -- end
    -- loki.link.on_start_stop("on_link_transport")
end

-- ==============================================================================
-- Load Keybindings
-- ==============================================================================

-- Debug logging (requires -DLUA_SANDBOX=OFF build, writes to /tmp/psnd_debug.log)
-- Note: io and os libraries are disabled by default for security (sandbox mode)
DEBUG_ENABLED = false
local log_file = nil
if DEBUG_ENABLED and io and io.open then
    log_file = io.open("/tmp/psnd_debug.log", "w")
end
function dbg(msg)
    if log_file and os and os.date then
        log_file:write(os.date("%H:%M:%S") .. " " .. tostring(msg) .. "\n")
        log_file:flush()
    end
end

-- Debug helper: show registered keymaps (run via Ctrl-L: show_keymaps())
function show_keymaps()
    if not _loki_keymaps then
        loki.status("No keymaps registered")
        return
    end
    local count = 0
    local keys = {}
    for mode, tbl in pairs(_loki_keymaps) do
        for k, v in pairs(tbl) do
            if type(k) == "number" and type(v) == "function" then
                count = count + 1
                local keyname = k <= 26 and string.format("Ctrl-%c", string.byte('A') + k - 1) or tostring(k)
                table.insert(keys, string.format("%s:%s", mode:sub(1,1), keyname))
            end
        end
    end
    loki.status(string.format("%d keymaps: %s", count, table.concat(keys, ", ")))
end

if MODE == "editor" then
    dbg("=== Editor mode startup ===")

    -- Load Alda keybindings (Ctrl-E, Ctrl-P, Ctrl-G)
    -- Customize in .psnd/keybindings/alda_keys.lua
    dbg("Loading alda_keys module...")
    local alda_keys = require("alda_keys")
    dbg("alda_keys loaded")

    -- To customize keybindings, call setup with options:
    -- alda_keys.setup({
    --     eval_part = '<C-e>',   -- Evaluate selection/part
    --     play_file = '<C-p>',   -- Play entire file
    --     stop = '<C-g>',        -- Stop playback
    --     modes = 'ni',          -- Modes to bind (n=normal, i=insert, v=visual)
    -- })
end

-- ==============================================================================
-- Startup Message
-- ==============================================================================

if MODE == "editor" then
    if ext_count > 0 then
        loki.status(string.format("Psnd ready. %d language extensions available.", ext_count))
    else
        loki.status("Psnd ready. Press Ctrl-L for Lua REPL.")
    end
end
