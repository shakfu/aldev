# Psnd Color Themes

This directory contains pre-made color themes for the Psnd editor.

## Usage

Load a theme in your `.psnd/init.lua`:

```lua
theme = require("theme")
theme.load("dracula")
```

## Available Themes

- **dracula.lua** - Dark theme with vibrant purple/pink colors
- **github-light.lua** - Light theme inspired by GitHub
- **nord.lua** - Arctic, north-bluish color palette
- **monokai.lua** - Classic Monokai dark theme

## Creating Custom Themes

You can create your own theme by defining RGB colors for each syntax element:

```lua
loki.set_theme({
    normal = {r=200, g=200, b=200},      -- Default text
    nonprint = {r=100, g=100, b=100},    -- Non-printable chars
    comment = {r=100, g=150, b=100},     -- Comments
    mlcomment = {r=100, g=150, b=100},   -- Multi-line comments
    keyword1 = {r=220, g=100, b=220},    -- Keywords (if, while, etc)
    keyword2 = {r=100, g=220, b=220},    -- Types (int, char, etc)
    string = {r=220, g=220, b=100},      -- String literals
    number = {r=200, g=100, b=200},      -- Numbers
    match = {r=100, g=150, b=220}        -- Search matches
})
```

You can also set individual colors:

```lua
loki.set_color("keyword1", {r=255, g=0, b=0})  -- Red keywords
```

## Color Format

All colors use RGB values from 0-255. Loki uses 24-bit true color escape codes, so colors will appear as specified if your terminal supports true color.
