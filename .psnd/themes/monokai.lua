-- Monokai Theme for Loki
-- Classic Monokai color scheme

return function()
    loki.set_theme({
        normal = {r=248, g=248, b=240},      -- White-ish
        nonprint = {r=73, g=72, b=62},       -- Dark gray
        comment = {r=117, g=113, b=94},      -- Gray-brown comment
        mlcomment = {r=117, g=113, b=94},    -- Gray-brown
        keyword1 = {r=249, g=38, b=114},     -- Pink (keywords)
        keyword2 = {r=102, g=217, b=239},    -- Cyan (types/classes)
        string = {r=230, g=219, b=116},      -- Yellow (strings)
        number = {r=174, g=129, b=255},      -- Purple (numbers)
        match = {r=166, g=226, b=46}         -- Green (search match)
    })

    if loki.status then
        loki.status("Monokai theme loaded")
    end
end
