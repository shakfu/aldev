/**
 * @file fts.h
 * @brief SQLite FTS5 full-text search index for psnd.
 *
 * Provides fast full-text search over files in the .psnd/ configuration
 * directory and project files. Uses SQLite's FTS5 extension with trigram
 * tokenization for substring matching.
 *
 * Usage:
 *   FTSIndex *idx;
 *   fts_open(&idx, "~/.psnd/index.db");
 *   fts_index_directory(idx, "~/.psnd", FTS_INDEX_INCREMENTAL);
 *
 *   FTSResult *results;
 *   int count;
 *   fts_search(idx, "chord major", &results, &count, 20);
 *   // ... use results ...
 *   fts_results_free(results, count);
 *   fts_close(idx);
 */

#ifndef PSND_FTS_H
#define PSND_FTS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque index handle */
typedef struct FTSIndex FTSIndex;

/* Search result entry */
typedef struct FTSResult {
    char *path;       /* File path (relative to indexed root) */
    char *snippet;    /* Matching text snippet with context */
    int line;         /* Line number of first match (1-based, 0 if unknown) */
    double rank;      /* BM25 relevance score (lower is better) */
} FTSResult;

/* Index statistics */
typedef struct FTSStats {
    int file_count;        /* Number of indexed files */
    int64_t total_bytes;   /* Total bytes of indexed content */
    int64_t last_indexed;  /* Unix timestamp of last full index */
    int64_t index_size;    /* Size of index database in bytes */
} FTSStats;

/* Indexing flags */
typedef enum FTSIndexFlags {
    FTS_INDEX_FULL = 0,        /* Full reindex (drops existing data) */
    FTS_INDEX_INCREMENTAL = 1, /* Only index changed files (mtime check) */
} FTSIndexFlags;

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

/**
 * @brief Open or create an FTS index database.
 *
 * @param idx      Output: index handle (caller must call fts_close).
 * @param db_path  Path to SQLite database file. Use ":memory:" for in-memory.
 * @return 0 on success, -1 on error.
 */
int fts_open(FTSIndex **idx, const char *db_path);

/**
 * @brief Close the index and free resources.
 *
 * @param idx  Index handle (may be NULL).
 */
void fts_close(FTSIndex *idx);

/* ============================================================================
 * Indexing
 * ============================================================================ */

/**
 * @brief Index all files in a directory tree.
 *
 * Walks the directory recursively, indexing text files (.lua, .alda, .joy,
 * .scl, .csd, .md, .txt, .json). Binary files and hidden directories
 * (except .psnd itself) are skipped.
 *
 * @param idx        Index handle.
 * @param root_path  Directory to index.
 * @param flags      FTS_INDEX_FULL or FTS_INDEX_INCREMENTAL.
 * @return Number of files indexed, or -1 on error.
 */
int fts_index_directory(FTSIndex *idx, const char *root_path, FTSIndexFlags flags);

/**
 * @brief Index a single file.
 *
 * @param idx   Index handle.
 * @param path  Path to file.
 * @return 0 on success, -1 on error.
 */
int fts_index_file(FTSIndex *idx, const char *path);

/**
 * @brief Remove a file from the index.
 *
 * @param idx   Index handle.
 * @param path  Path to file.
 * @return 0 on success, -1 on error.
 */
int fts_remove_file(FTSIndex *idx, const char *path);

/**
 * @brief Clear all indexed data.
 *
 * @param idx  Index handle.
 * @return 0 on success, -1 on error.
 */
int fts_clear(FTSIndex *idx);

/* ============================================================================
 * Search
 * ============================================================================ */

/**
 * @brief Search the index using FTS5 query syntax.
 *
 * Supports:
 *   - Simple terms: "chord" matches files containing "chord"
 *   - Phrases: "\"major chord\"" matches exact phrase
 *   - AND/OR: "chord AND major", "chord OR minor"
 *   - Prefix: "cho*" matches "chord", "chorus", etc.
 *   - Column filter: "path:alda" matches paths containing "alda"
 *
 * Results are ordered by BM25 relevance score.
 *
 * @param idx      Index handle.
 * @param query    FTS5 query string.
 * @param results  Output: array of results (caller must call fts_results_free).
 * @param count    Output: number of results.
 * @param limit    Maximum results to return.
 * @return 0 on success, -1 on error.
 */
int fts_search(FTSIndex *idx, const char *query,
               FTSResult **results, int *count, int limit);

/**
 * @brief Search file paths only (faster than full-text search).
 *
 * @param idx      Index handle.
 * @param pattern  Glob-like pattern (uses SQL LIKE with % wildcards).
 * @param results  Output: array of results.
 * @param count    Output: number of results.
 * @param limit    Maximum results to return.
 * @return 0 on success, -1 on error.
 */
int fts_search_paths(FTSIndex *idx, const char *pattern,
                     FTSResult **results, int *count, int limit);

/**
 * @brief Free search results.
 *
 * @param results  Results array from fts_search or fts_search_paths.
 * @param count    Number of results.
 */
void fts_results_free(FTSResult *results, int count);

/* ============================================================================
 * Metadata
 * ============================================================================ */

/**
 * @brief Get index statistics.
 *
 * @param idx    Index handle.
 * @param stats  Output: statistics structure.
 * @return 0 on success, -1 on error.
 */
int fts_get_stats(FTSIndex *idx, FTSStats *stats);

/**
 * @brief Get the last error message.
 *
 * @param idx  Index handle.
 * @return Error message string (valid until next FTS call), or NULL.
 */
const char *fts_get_error(FTSIndex *idx);

#ifdef __cplusplus
}
#endif

#endif /* PSND_FTS_H */
