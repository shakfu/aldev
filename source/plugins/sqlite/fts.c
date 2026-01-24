/**
 * @file fts.c
 * @brief SQLite FTS5 full-text search index implementation.
 */

#include "fts.h"
#include "sqlite3.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>

/* Maximum file size to index (skip large files) */
#define FTS_MAX_FILE_SIZE (1024 * 1024)  /* 1 MB */

/* Maximum path length */
#define FTS_MAX_PATH 4096

/* Index handle */
struct FTSIndex {
    sqlite3 *db;
    char *root_path;      /* Base path for relative paths */
    char error[512];      /* Last error message */
};

/* ----------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------------- */

static void set_error(FTSIndex *idx, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(idx->error, sizeof(idx->error), fmt, args);
    va_end(args);
}

static int is_indexable_extension(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return 0;

    /* Text-based file types we want to index */
    static const char *exts[] = {
        ".lua", ".alda", ".joy", ".scl", ".csd",
        ".md", ".txt", ".json", ".scm", ".lisp",
        ".c", ".h", ".py", ".js", ".ts",
        NULL
    };

    for (int i = 0; exts[i]; i++) {
        if (strcasecmp(ext, exts[i]) == 0) return 1;
    }
    return 0;
}

static int should_skip_dir(const char *name) {
    /* Skip hidden directories except .psnd */
    if (name[0] == '.') {
        if (strcmp(name, ".psnd") == 0) return 0;
        return 1;
    }
    /* Skip common non-content directories */
    if (strcmp(name, "node_modules") == 0) return 1;
    if (strcmp(name, "__pycache__") == 0) return 1;
    if (strcmp(name, "build") == 0) return 1;
    if (strcmp(name, ".git") == 0) return 1;
    return 0;
}

static char *read_file_content(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > FTS_MAX_FILE_SIZE) {
        fclose(f);
        return NULL;
    }

    char *content = malloc(size + 1);
    if (!content) {
        fclose(f);
        return NULL;
    }

    size_t read = fread(content, 1, size, f);
    fclose(f);

    content[read] = '\0';
    if (out_size) *out_size = read;
    return content;
}

static const char *get_filetype(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "unknown";
    ext++; /* Skip the dot */
    return ext;
}

/* Make path relative to root */
static const char *make_relative(const char *path, const char *root) {
    size_t root_len = strlen(root);
    if (strncmp(path, root, root_len) == 0) {
        path += root_len;
        while (*path == '/') path++;
    }
    return path;
}

/* ----------------------------------------------------------------------------
 * Schema initialization
 * ---------------------------------------------------------------------------- */

static const char *SCHEMA_SQL =
    /* FTS5 virtual table with trigram tokenizer for substring matching */
    "CREATE VIRTUAL TABLE IF NOT EXISTS files_fts USING fts5("
    "    path,"
    "    content,"
    "    filetype,"
    "    tokenize='trigram'"
    ");"

    /* Metadata table for incremental indexing */
    "CREATE TABLE IF NOT EXISTS file_meta ("
    "    path TEXT PRIMARY KEY,"
    "    mtime INTEGER NOT NULL,"
    "    size INTEGER NOT NULL"
    ");"

    /* Index statistics */
    "CREATE TABLE IF NOT EXISTS index_info ("
    "    key TEXT PRIMARY KEY,"
    "    value TEXT"
    ");"

    /* Initialize stats if not present */
    "INSERT OR IGNORE INTO index_info (key, value) VALUES ('last_indexed', '0');"
    "INSERT OR IGNORE INTO index_info (key, value) VALUES ('root_path', '');"
;

static int init_schema(FTSIndex *idx) {
    char *err = NULL;
    int rc = sqlite3_exec(idx->db, SCHEMA_SQL, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        set_error(idx, "Schema init failed: %s", err ? err : "unknown");
        sqlite3_free(err);
        return -1;
    }
    return 0;
}

/* ----------------------------------------------------------------------------
 * Lifecycle
 * ---------------------------------------------------------------------------- */

int fts_open(FTSIndex **out_idx, const char *db_path) {
    if (!out_idx || !db_path) return -1;

    /* Initialize SQLite (safe to call multiple times) */
    sqlite3_initialize();

    FTSIndex *idx = calloc(1, sizeof(FTSIndex));
    if (!idx) return -1;

    int rc = sqlite3_open_v2(db_path, &idx->db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX,
        NULL);

    if (rc != SQLITE_OK) {
        set_error(idx, "Cannot open database: %s", sqlite3_errmsg(idx->db));
        sqlite3_close(idx->db);
        free(idx);
        return -1;
    }

    /* Enable WAL mode for better concurrent access */
    sqlite3_exec(idx->db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    sqlite3_exec(idx->db, "PRAGMA synchronous=NORMAL;", NULL, NULL, NULL);

    if (init_schema(idx) != 0) {
        sqlite3_close(idx->db);
        free(idx);
        return -1;
    }

    *out_idx = idx;
    return 0;
}

void fts_close(FTSIndex *idx) {
    if (!idx) return;
    if (idx->db) sqlite3_close(idx->db);
    free(idx->root_path);
    free(idx);
}

/* ----------------------------------------------------------------------------
 * Indexing
 * ---------------------------------------------------------------------------- */

static int file_needs_update(FTSIndex *idx, const char *rel_path, time_t mtime) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT mtime FROM file_meta WHERE path = ?";

    if (sqlite3_prepare_v2(idx->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return 1; /* Error - assume needs update */
    }

    sqlite3_bind_text(stmt, 1, rel_path, -1, SQLITE_STATIC);

    int needs_update = 1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        time_t stored_mtime = sqlite3_column_int64(stmt, 0);
        needs_update = (mtime > stored_mtime);
    }

    sqlite3_finalize(stmt);
    return needs_update;
}

static int index_single_file(FTSIndex *idx, const char *abs_path, const char *rel_path) {
    struct stat st;
    if (stat(abs_path, &st) != 0) {
        return -1;
    }

    size_t content_size;
    char *content = read_file_content(abs_path, &content_size);
    if (!content) {
        /* Skip unreadable files silently */
        return 0;
    }

    const char *filetype = get_filetype(abs_path);

    /* Begin transaction for atomic update */
    sqlite3_exec(idx->db, "BEGIN", NULL, NULL, NULL);

    /* Remove old entry if exists */
    sqlite3_stmt *del_fts;
    sqlite3_prepare_v2(idx->db,
        "DELETE FROM files_fts WHERE path = ?", -1, &del_fts, NULL);
    sqlite3_bind_text(del_fts, 1, rel_path, -1, SQLITE_STATIC);
    sqlite3_step(del_fts);
    sqlite3_finalize(del_fts);

    /* Insert into FTS table */
    sqlite3_stmt *ins_fts;
    sqlite3_prepare_v2(idx->db,
        "INSERT INTO files_fts (path, content, filetype) VALUES (?, ?, ?)",
        -1, &ins_fts, NULL);
    sqlite3_bind_text(ins_fts, 1, rel_path, -1, SQLITE_STATIC);
    sqlite3_bind_text(ins_fts, 2, content, content_size, SQLITE_STATIC);
    sqlite3_bind_text(ins_fts, 3, filetype, -1, SQLITE_STATIC);

    int rc = sqlite3_step(ins_fts);
    sqlite3_finalize(ins_fts);

    if (rc != SQLITE_DONE) {
        set_error(idx, "FTS insert failed: %s", sqlite3_errmsg(idx->db));
        sqlite3_exec(idx->db, "ROLLBACK", NULL, NULL, NULL);
        free(content);
        return -1;
    }

    /* Update metadata */
    sqlite3_stmt *ins_meta;
    sqlite3_prepare_v2(idx->db,
        "INSERT OR REPLACE INTO file_meta (path, mtime, size) VALUES (?, ?, ?)",
        -1, &ins_meta, NULL);
    sqlite3_bind_text(ins_meta, 1, rel_path, -1, SQLITE_STATIC);
    sqlite3_bind_int64(ins_meta, 2, st.st_mtime);
    sqlite3_bind_int64(ins_meta, 3, st.st_size);
    sqlite3_step(ins_meta);
    sqlite3_finalize(ins_meta);

    sqlite3_exec(idx->db, "COMMIT", NULL, NULL, NULL);

    free(content);
    return 0;
}

static int walk_directory(FTSIndex *idx, const char *dir_path,
                          FTSIndexFlags flags, int *file_count) {
    DIR *d = opendir(dir_path);
    if (!d) {
        set_error(idx, "Cannot open directory: %s", strerror(errno));
        return -1;
    }

    struct dirent *entry;
    char path[FTS_MAX_PATH];

    while ((entry = readdir(d)) != NULL) {
        /* Skip . and .. */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            if (!should_skip_dir(entry->d_name)) {
                walk_directory(idx, path, flags, file_count);
            }
        } else if (S_ISREG(st.st_mode) && is_indexable_extension(path)) {
            const char *rel_path = make_relative(path, idx->root_path);

            /* Check if update needed for incremental mode */
            if (flags == FTS_INDEX_INCREMENTAL) {
                if (!file_needs_update(idx, rel_path, st.st_mtime)) {
                    continue;
                }
            }

            if (index_single_file(idx, path, rel_path) == 0) {
                (*file_count)++;
            }
        }
    }

    closedir(d);
    return 0;
}

int fts_index_directory(FTSIndex *idx, const char *root_path, FTSIndexFlags flags) {
    if (!idx || !root_path) return -1;

    /* Store root path for relative path calculation */
    free(idx->root_path);
    idx->root_path = strdup(root_path);

    /* Full reindex: clear existing data */
    if (flags == FTS_INDEX_FULL) {
        fts_clear(idx);
    }

    int file_count = 0;
    if (walk_directory(idx, root_path, flags, &file_count) != 0) {
        return -1;
    }

    /* Update last indexed timestamp */
    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "%ld", (long)time(NULL));

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(idx->db,
        "INSERT OR REPLACE INTO index_info (key, value) VALUES ('last_indexed', ?)",
        -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, timestamp, -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    /* Store root path */
    sqlite3_prepare_v2(idx->db,
        "INSERT OR REPLACE INTO index_info (key, value) VALUES ('root_path', ?)",
        -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, root_path, -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return file_count;
}

int fts_index_file(FTSIndex *idx, const char *path) {
    if (!idx || !path) return -1;

    const char *rel_path = idx->root_path ? make_relative(path, idx->root_path) : path;
    return index_single_file(idx, path, rel_path);
}

int fts_remove_file(FTSIndex *idx, const char *path) {
    if (!idx || !path) return -1;

    const char *rel_path = idx->root_path ? make_relative(path, idx->root_path) : path;

    sqlite3_exec(idx->db, "BEGIN", NULL, NULL, NULL);

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(idx->db, "DELETE FROM files_fts WHERE path = ?", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, rel_path, -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    sqlite3_prepare_v2(idx->db, "DELETE FROM file_meta WHERE path = ?", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, rel_path, -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    sqlite3_exec(idx->db, "COMMIT", NULL, NULL, NULL);
    return 0;
}

int fts_clear(FTSIndex *idx) {
    if (!idx) return -1;

    sqlite3_exec(idx->db, "DELETE FROM files_fts", NULL, NULL, NULL);
    sqlite3_exec(idx->db, "DELETE FROM file_meta", NULL, NULL, NULL);
    return 0;
}

/* ----------------------------------------------------------------------------
 * Search
 * ---------------------------------------------------------------------------- */

int fts_search(FTSIndex *idx, const char *query,
               FTSResult **results, int *count, int limit) {
    if (!idx || !query || !results || !count) return -1;

    *results = NULL;
    *count = 0;

    if (limit <= 0) limit = 100;

    const char *sql =
        "SELECT path, "
        "       snippet(files_fts, 1, '>>>', '<<<', '...', 48), "
        "       bm25(files_fts) "
        "FROM files_fts "
        "WHERE files_fts MATCH ? "
        "ORDER BY bm25(files_fts) "
        "LIMIT ?";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(idx->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        set_error(idx, "Search prepare failed: %s", sqlite3_errmsg(idx->db));
        return -1;
    }

    sqlite3_bind_text(stmt, 1, query, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, limit);

    /* Allocate results array */
    FTSResult *res = calloc(limit, sizeof(FTSResult));
    if (!res) {
        sqlite3_finalize(stmt);
        return -1;
    }

    int n = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && n < limit) {
        const char *path = (const char *)sqlite3_column_text(stmt, 0);
        const char *snippet = (const char *)sqlite3_column_text(stmt, 1);
        double rank = sqlite3_column_double(stmt, 2);

        res[n].path = path ? strdup(path) : NULL;
        res[n].snippet = snippet ? strdup(snippet) : NULL;
        res[n].line = 0;  /* TODO: extract line number from content */
        res[n].rank = rank;
        n++;
    }

    sqlite3_finalize(stmt);

    *results = res;
    *count = n;
    return 0;
}

int fts_search_paths(FTSIndex *idx, const char *pattern,
                     FTSResult **results, int *count, int limit) {
    if (!idx || !pattern || !results || !count) return -1;

    *results = NULL;
    *count = 0;

    if (limit <= 0) limit = 100;

    /* Convert glob pattern to SQL LIKE pattern */
    char like_pattern[FTS_MAX_PATH];
    const char *p = pattern;
    char *out = like_pattern;
    char *end = like_pattern + sizeof(like_pattern) - 2;

    while (*p && out < end) {
        if (*p == '*') {
            *out++ = '%';
        } else if (*p == '?') {
            *out++ = '_';
        } else {
            *out++ = *p;
        }
        p++;
    }
    *out = '\0';

    const char *sql =
        "SELECT DISTINCT path FROM file_meta WHERE path LIKE ? LIMIT ?";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(idx->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        set_error(idx, "Path search failed: %s", sqlite3_errmsg(idx->db));
        return -1;
    }

    sqlite3_bind_text(stmt, 1, like_pattern, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, limit);

    FTSResult *res = calloc(limit, sizeof(FTSResult));
    if (!res) {
        sqlite3_finalize(stmt);
        return -1;
    }

    int n = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && n < limit) {
        const char *path = (const char *)sqlite3_column_text(stmt, 0);
        res[n].path = path ? strdup(path) : NULL;
        res[n].snippet = NULL;
        res[n].line = 0;
        res[n].rank = 0;
        n++;
    }

    sqlite3_finalize(stmt);

    *results = res;
    *count = n;
    return 0;
}

void fts_results_free(FTSResult *results, int count) {
    if (!results) return;
    for (int i = 0; i < count; i++) {
        free(results[i].path);
        free(results[i].snippet);
    }
    free(results);
}

/* ----------------------------------------------------------------------------
 * Metadata
 * ---------------------------------------------------------------------------- */

int fts_get_stats(FTSIndex *idx, FTSStats *stats) {
    if (!idx || !stats) return -1;

    memset(stats, 0, sizeof(FTSStats));

    /* File count */
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(idx->db, "SELECT COUNT(*) FROM file_meta", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        stats->file_count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    /* Total bytes */
    sqlite3_prepare_v2(idx->db, "SELECT SUM(size) FROM file_meta", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        stats->total_bytes = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);

    /* Last indexed */
    sqlite3_prepare_v2(idx->db,
        "SELECT value FROM index_info WHERE key = 'last_indexed'", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *val = (const char *)sqlite3_column_text(stmt, 0);
        if (val) stats->last_indexed = atoll(val);
    }
    sqlite3_finalize(stmt);

    /* Database file size (approximate via page_count * page_size) */
    sqlite3_prepare_v2(idx->db,
        "SELECT page_count * page_size FROM pragma_page_count(), pragma_page_size()",
        -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        stats->index_size = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);

    return 0;
}

const char *fts_get_error(FTSIndex *idx) {
    if (!idx || idx->error[0] == '\0') return NULL;
    return idx->error;
}
