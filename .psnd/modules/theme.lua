-- Theme Loading Module
-- Provides utilities for loading and managing color themes

local M = {}

-- Add themes directory to package.path
local function setup_theme_path()
    local theme_paths = {
        ".psnd/themes/?.lua",
        (os.getenv("HOME") or "") .. "/.psnd/themes/?.lua"
    }

    for _, path in ipairs(theme_paths) do
        if not package.path:find(path, 1, true) then
            package.path = package.path .. ";" .. path
        end
    end
end

setup_theme_path()

-- Load a theme from the themes directory
-- Returns true on success, nil and error message on failure
function M.load(theme_name)
    -- Clear from cache if previously loaded
    package.loaded[theme_name] = nil

    local success, theme = pcall(require, theme_name)

    if not success then
        return nil, "Failed to load theme '" .. theme_name .. "': " .. tostring(theme)
    end

    -- If theme returns a function, call it to apply the theme
    if type(theme) == "function" then
        theme()
    end

    return true
end

-- Try to load a theme, silently fail if not found
function M.try_load(theme_name)
    local success, err = M.load(theme_name)
    if not success and err then
        local MODE = loki.get_lines and "editor" or "repl"
        if MODE == "editor" then
            loki.status(err)
        else
            print(err)
        end
    end
    return success
end

-- List available themes in the themes directory
function M.list()
    local themes = {}
    local handle = io.popen("ls .psnd/themes/*.lua 2>/dev/null")

    if handle then
        for filepath in handle:lines() do
            local theme_name = filepath:match("([^/]+)%.lua$")
            if theme_name then
                table.insert(themes, theme_name)
            end
        end
        handle:close()
    end

    return themes
end

-- Print available themes (for REPL)
function M.print_available()
    local themes = M.list()
    if #themes > 0 then
        print("Available themes:")
        for _, theme in ipairs(themes) do
            print("  - " .. theme)
        end
        print("\nLoad with: theme.load('" .. themes[1] .. "')")
    else
        print("No themes found in .psnd/themes/")
    end
end

return M
