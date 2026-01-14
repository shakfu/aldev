-- Lua language definition
return loki.register_language({
    name = "Lua",
    extensions = {".lua"},
    keywords = {
        "and", "break", "do", "else", "elseif", "end", "false", "for",
        "function", "if", "in", "local", "nil", "not", "or", "repeat",
        "return", "then", "true", "until", "while", "goto"
    },
    types = {
        "string", "number", "boolean", "table", "function", "thread",
        "userdata", "self"
    },
    line_comment = "--",
    block_comment_start = "--[[",
    block_comment_end = "]]",
    separators = ",.()+-/*=~%[]{}:;",
    highlight_strings = true,
    highlight_numbers = true
})
