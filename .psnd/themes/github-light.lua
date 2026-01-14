-- GitHub Light Theme for Loki
-- Inspired by GitHub's light theme

return function()
    loki.set_theme({
        normal = {r=36, g=41, b=46},         -- Dark gray text
        nonprint = {r=149, g=157, b=165},    -- Gray
        comment = {r=106, g=115, b=125},     -- Medium gray
        mlcomment = {r=106, g=115, b=125},   -- Medium gray
        keyword1 = {r=215, g=58, b=73},      -- Red (keywords)
        keyword2 = {r=111, g=66, b=193},     -- Purple (types/classes)
        string = {r=3, g=47, b=98},          -- Dark blue (strings)
        number = {r=0, g=92, b=197},         -- Blue (numbers)
        match = {r=255, g=223, b=0}          -- Yellow (search match)
    })

    if loki.status then
        loki.status("GitHub Light theme loaded")
    end
end
