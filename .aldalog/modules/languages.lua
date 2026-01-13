-- Language Loading Module - Lazy Loading Implementation
-- Only loads language definitions when needed (on file open)
--
-- Usage:
--   languages = require("languages")
--   languages.init()  -- Set up lazy loading (call once at startup)
--
-- Languages are loaded automatically when opening files:
--   ./aldalog test.alda  → Automatically loads alda.lua
--   ./aldalog init.lua   → Automatically loads lua.lua

local M = {}

-- Extension → language file mapping
local extension_map = {}

-- Cache of loaded languages (filepath → true)
local loaded_languages = {}

-- Configuration
local config = {
    lang_dir = ".aldalog/languages",
    fallback_dirs = {
        (os.getenv("HOME") or "") .. "/.aldalog/languages",
    },
    default_syntax = nil,  -- No default fallback language
    enable_status = true,  -- Show "Loaded X" messages
}

local MODE = nil

-- Detect execution mode
local function get_mode()
    if MODE then return MODE end
    MODE = loki.get_lines and "editor" or "repl"
    return MODE
end

-- Status message helper (mode-aware)
local function status(msg)
    if not config.enable_status then return end

    if get_mode() == "editor" then
        loki.status(msg)
    else
        print(msg)
    end
end

-- Check if file exists
local function file_exists(path)
    local f = io.open(path, "r")
    if f then
        f:close()
        return true
    end
    return false
end

-- Find language file in search paths
local function find_language_file(lang_name)
    -- Try primary directory first
    local filepath = config.lang_dir .. "/" .. lang_name .. ".lua"
    if file_exists(filepath) then
        return filepath
    end

    -- Try fallback directories
    for _, dir in ipairs(config.fallback_dirs) do
        filepath = dir .. "/" .. lang_name .. ".lua"
        if file_exists(filepath) then
            return filepath
        end
    end

    return nil
end

-- Quick scan of language file to extract extensions (without loading)
-- Reads first 500 bytes to find: extensions = {".py", ".pyw"}
local function get_language_extensions(filepath)
    local f = io.open(filepath, "r")
    if not f then return {} end

    local header = f:read(500)  -- First 500 bytes should contain extensions
    f:close()

    if not header then return {} end

    -- Look for: extensions = {".py", ".pyw", ...}
    local exts = {}
    for ext in header:gmatch('"(%.[^"]+)"') do
        table.insert(exts, ext)
    end

    return exts
end

-- Build extension registry by scanning language directory
local function build_extension_registry(lang_dir)
    local handle = io.popen("ls " .. lang_dir .. "/*.lua 2>/dev/null")
    if not handle then
        return 0
    end

    local count = 0
    for filepath in handle:lines() do
        -- Extract language name from filename
        local lang_name = filepath:match("([^/]+)%.lua$")
        if lang_name then
            -- Quick scan to get extensions
            local extensions = get_language_extensions(filepath)

            -- Map each extension → language file
            for _, ext in ipairs(extensions) do
                extension_map[ext] = filepath
                count = count + 1
            end
        end
    end
    handle:close()

    return count
end

-- Load a specific language definition file
function M.load_file(filepath)
    -- Check if already loaded
    if loaded_languages[filepath] then
        return true  -- Already loaded
    end

    -- Load and execute language definition file
    local ok, result = pcall(dofile, filepath)
    if not ok then
        status("Error loading " .. filepath .. ": " .. tostring(result))
        return false
    end

    -- Mark as loaded
    loaded_languages[filepath] = true

    -- Extract language name for status message
    local lang_name = filepath:match("([^/]+)%.lua$") or filepath
    status("Loaded " .. lang_name)

    return true
end

-- Load language definition by name (e.g., "python", "javascript")
function M.load(lang_name)
    local filepath = find_language_file(lang_name)
    if not filepath then
        status("Language definition not found: " .. lang_name)
        return false
    end

    return M.load_file(filepath)
end

-- Load language definition for a specific file extension
-- This is called automatically when opening files
function M.load_for_extension(ext)
    -- Normalize extension (ensure it starts with .)
    if not ext:match("^%.") then
        ext = "." .. ext
    end

    -- Check if we have a language for this extension
    local lang_file = extension_map[ext]
    if not lang_file then
        -- No language definition for this extension
        return false
    end

    -- Load language definition (checks if already loaded)
    return M.load_file(lang_file)
end

-- Initialize lazy loading system
-- Builds extension registry and loads default/fallback languages
function M.init(user_config)
    -- Merge user configuration
    if user_config then
        for k, v in pairs(user_config) do
            config[k] = v
        end
    end

    -- Build extension registry by scanning language directory
    local ext_count = build_extension_registry(config.lang_dir)

    -- Also scan fallback directories
    for _, dir in ipairs(config.fallback_dirs) do
        ext_count = ext_count + build_extension_registry(dir)
    end

    -- Status message
    if ext_count > 0 then
        status(string.format("Language registry: %d extensions mapped", ext_count))
    end

    -- Load default language if configured
    if config.default_syntax then
        M.load(config.default_syntax)
    end

    return ext_count
end

-- Backwards compatibility: load_all() for users who want eager loading
function M.load_all(lang_dir)
    lang_dir = lang_dir or config.lang_dir

    -- Build registry first
    local ext_count = build_extension_registry(lang_dir)

    -- Load all registered languages immediately
    local loaded_count = 0
    local loaded_files = {}

    for _, filepath in pairs(extension_map) do
        -- Avoid loading same file multiple times
        if not loaded_files[filepath] then
            if M.load_file(filepath) then
                loaded_count = loaded_count + 1
                loaded_files[filepath] = true
            end
        end
    end

    status(string.format("Loaded %d languages (%d extensions)", loaded_count, ext_count))
    return loaded_count
end

-- Get list of available languages (from extension registry)
function M.list()
    local languages = {}
    local seen = {}

    for _, filepath in pairs(extension_map) do
        local lang_name = filepath:match("([^/]+)%.lua$")
        if lang_name and not seen[lang_name] then
            table.insert(languages, lang_name)
            seen[lang_name] = true
        end
    end

    table.sort(languages)
    return languages
end

-- Get extensions supported by a language
function M.get_extensions(lang_name)
    local extensions = {}

    for ext, filepath in pairs(extension_map) do
        local file_lang = filepath:match("([^/]+)%.lua$")
        if file_lang == lang_name then
            table.insert(extensions, ext)
        end
    end

    table.sort(extensions)
    return extensions
end

-- Reload a specific language (hot-reload)
function M.reload(lang_name)
    local filepath = find_language_file(lang_name)
    if not filepath then
        status("Language definition not found: " .. lang_name)
        return false
    end

    -- Force reload by removing from cache
    loaded_languages[filepath] = nil

    -- Reload
    return M.load_file(filepath)
end

-- Get statistics
function M.stats()
    local total_extensions = 0
    for _ in pairs(extension_map) do
        total_extensions = total_extensions + 1
    end

    local total_loaded = 0
    for _ in pairs(loaded_languages) do
        total_loaded = total_loaded + 1
    end

    return {
        extensions = total_extensions,
        loaded = total_loaded,
        unloaded = total_extensions - total_loaded,
    }
end

-- Register with REPL help
if loki.repl and loki.repl.register then
    loki.repl.register("languages.list", "List available language definitions")
    loki.repl.register("languages.load", "Load a language: languages.load('alda')")
    loki.repl.register("languages.reload", "Reload a language: languages.reload('alda')")
    loki.repl.register("languages.stats", "Show language loading statistics")
    loki.repl.register("languages.get_extensions", "Get extensions for language: languages.get_extensions('alda')")
end

return M
