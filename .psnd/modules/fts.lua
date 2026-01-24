-- fts.lua - Full-text search helpers for psnd
--
-- This module provides convenient wrappers around loki.fts for searching
-- the .psnd configuration directory and project files.
--
-- Usage:
--   local fts = require('fts')
--   fts.search('chord major')
--   fts.find('*.alda')
--   fts.rebuild()

local M = {}

-- Check if FTS plugin is available
function M.available()
    return loki and loki.fts ~= nil
end

-- Search indexed content and print results
-- @param query FTS5 query string
-- @param limit Maximum results (default 20)
function M.search(query, limit)
    if not M.available() then
        loki.message('FTS plugin not available')
        return nil
    end

    local results, err = loki.fts.search(query, limit or 20)
    if not results then
        loki.message('Search failed: ' .. (err or 'unknown error'))
        return nil
    end

    if #results == 0 then
        loki.message('No results for: ' .. query)
        return {}
    end

    -- Print results
    for i, r in ipairs(results) do
        local snippet = r.snippet or ''
        -- Clean up snippet markers
        snippet = snippet:gsub('>>>', '['):gsub('<<<', ']')
        loki.message(string.format('%d. %s', i, r.path))
        if snippet ~= '' then
            loki.message('   ' .. snippet)
        end
    end

    loki.message(string.format('Found %d result(s)', #results))
    return results
end

-- Search file paths using glob pattern
-- @param pattern Glob pattern (e.g., '*.alda', '*theme*')
-- @param limit Maximum results (default 50)
function M.find(pattern, limit)
    if not M.available() then
        loki.message('FTS plugin not available')
        return nil
    end

    local results, err = loki.fts.find(pattern, limit or 50)
    if not results then
        loki.message('Find failed: ' .. (err or 'unknown error'))
        return nil
    end

    if #results == 0 then
        loki.message('No files matching: ' .. pattern)
        return {}
    end

    for i, r in ipairs(results) do
        loki.message(string.format('%d. %s', i, r.path))
    end

    loki.message(string.format('Found %d file(s)', #results))
    return results
end

-- Index directory (incremental by default)
-- @param path Directory to index (default ~/.psnd)
-- @param full If true, do full reindex instead of incremental
function M.index(path, full)
    if not M.available() then
        loki.message('FTS plugin not available')
        return nil
    end

    loki.message('Indexing...')
    local count, err = loki.fts.index(path, not full)
    if not count then
        loki.message('Index failed: ' .. (err or 'unknown error'))
        return nil
    end

    loki.message(string.format('Indexed %d file(s)', count))
    return count
end

-- Full rebuild of the index
function M.rebuild()
    if not M.available() then
        loki.message('FTS plugin not available')
        return nil
    end

    loki.message('Rebuilding index...')
    local count, err = loki.fts.rebuild()
    if not count then
        loki.message('Rebuild failed: ' .. (err or 'unknown error'))
        return nil
    end

    loki.message(string.format('Rebuilt index with %d file(s)', count))
    return count
end

-- Show index statistics
function M.stats()
    if not M.available() then
        loki.message('FTS plugin not available')
        return nil
    end

    local stats, err = loki.fts.stats()
    if not stats then
        loki.message('Cannot get stats: ' .. (err or 'unknown error'))
        return nil
    end

    loki.message('FTS Index Statistics:')
    loki.message(string.format('  Files: %d', stats.file_count))
    loki.message(string.format('  Size: %.1f KB', stats.total_bytes / 1024))
    loki.message(string.format('  Index: %.1f KB', stats.index_size / 1024))
    if stats.last_indexed_str then
        loki.message(string.format('  Last indexed: %s', stats.last_indexed_str))
    end

    return stats
end

-- Register ex-commands if in editor context
if loki and loki.register_ex_command then
    loki.register_ex_command('search', function(args)
        if not args or args == '' then
            loki.message('Usage: :search <query>')
            return
        end
        M.search(args)
    end, 'Full-text search in indexed files')

    loki.register_ex_command('find', function(args)
        if not args or args == '' then
            loki.message('Usage: :find <pattern>')
            return
        end
        M.find(args)
    end, 'Find files by path pattern')

    loki.register_ex_command('reindex', function()
        M.index()
    end, 'Update search index (incremental)')

    loki.register_ex_command('rebuild-index', function()
        M.rebuild()
    end, 'Rebuild search index from scratch')

    loki.register_ex_command('index-stats', function()
        M.stats()
    end, 'Show search index statistics')
end

return M
