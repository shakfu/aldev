-- Nord Theme for Loki
-- Based on https://www.nordtheme.com

return function()
    loki.set_theme({
        normal = {r=216, g=222, b=233},      -- Snow Storm
        nonprint = {r=76, g=86, b=106},      -- Polar Night 1
        comment = {r=136, g=192, b=208},     -- Frost (cyan comment)
        mlcomment = {r=136, g=192, b=208},   -- Frost
        keyword1 = {r=129, g=161, b=193},    -- Frost (blue keywords)
        keyword2 = {r=136, g=192, b=208},    -- Frost (cyan types)
        string = {r=163, g=190, b=140},      -- Aurora (green strings)
        number = {r=180, g=142, b=173},      -- Aurora (purple numbers)
        match = {r=235, g=203, b=139}        -- Aurora (yellow search)
    })

    if loki.status then
        loki.status("Nord theme loaded")
    end
end
