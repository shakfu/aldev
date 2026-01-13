-- Dracula Theme for Loki
-- Based on https://draculatheme.com

return function()
    loki.set_theme({
        normal = {r=248, g=248, b=242},      -- Foreground
        nonprint = {r=68, g=71, b=90},       -- Current Line
        comment = {r=98, g=114, b=164},      -- Comment (purple-ish gray)
        mlcomment = {r=98, g=114, b=164},    -- Multi-line comment
        keyword1 = {r=255, g=121, b=198},    -- Pink (keywords)
        keyword2 = {r=189, g=147, b=249},    -- Purple (types/classes)
        string = {r=241, g=250, b=140},      -- Yellow (strings)
        number = {r=189, g=147, b=249},      -- Purple (numbers)
        match = {r=139, g=233, b=253}        -- Cyan (search match)
    })

    if loki.status then
        loki.status("Dracula theme loaded")
    end
end
