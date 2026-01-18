/* json.h - Minimal JSON serialization for editor RPC
 *
 * This module provides lightweight JSON handling for the JSON-RPC harness.
 * Not a general-purpose JSON library - only handles the specific types
 * needed for editor commands and view model serialization.
 */

#ifndef LOKI_JSON_H
#define LOKI_JSON_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ======================= JSON Builder ====================================== */

/**
 * JsonBuilder - Accumulates JSON output into a growable buffer.
 */
typedef struct {
    char *buf;          /* Output buffer (owned) */
    size_t len;         /* Current length */
    size_t cap;         /* Allocated capacity */
    int error;          /* Error flag (allocation failure) */
    int depth;          /* Nesting depth for indentation */
    int need_comma;     /* Need comma before next element */
} JsonBuilder;

/* Initialize a JSON builder */
void json_builder_init(JsonBuilder *jb);

/* Free JSON builder resources */
void json_builder_free(JsonBuilder *jb);

/* Get the built JSON string (valid until builder is freed or reset) */
const char *json_builder_get(JsonBuilder *jb);

/* Reset builder for reuse */
void json_builder_reset(JsonBuilder *jb);

/* ======================= JSON Writing ====================================== */

/* Start an object: { */
void json_object_start(JsonBuilder *jb);

/* End an object: } */
void json_object_end(JsonBuilder *jb);

/* Start an array: [ */
void json_array_start(JsonBuilder *jb);

/* End an array: ] */
void json_array_end(JsonBuilder *jb);

/* Write object key (followed by value) */
void json_key(JsonBuilder *jb, const char *key);

/* Write string value */
void json_string(JsonBuilder *jb, const char *value);

/* Write string value with explicit length (handles embedded nulls) */
void json_string_len(JsonBuilder *jb, const char *value, size_t len);

/* Write integer value */
void json_int(JsonBuilder *jb, int value);

/* Write boolean value */
void json_bool(JsonBuilder *jb, int value);

/* Write null value */
void json_null(JsonBuilder *jb);

/* Convenience: write key-value pair with string value */
void json_kv_string(JsonBuilder *jb, const char *key, const char *value);

/* Convenience: write key-value pair with integer value */
void json_kv_int(JsonBuilder *jb, const char *key, int value);

/* Convenience: write key-value pair with boolean value */
void json_kv_bool(JsonBuilder *jb, const char *key, int value);

/* ======================= JSON Parsing (minimal) ============================ */

/**
 * JsonValue - Parsed JSON value (simple variant type).
 *
 * For our purposes, we only need to parse simple command objects like:
 *   {"cmd": "load", "file": "test.txt"}
 *   {"cmd": "key", "code": 105}
 */
typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_INT,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT,
    JSON_ERROR
} JsonType;

typedef struct JsonValue JsonValue;

struct JsonValue {
    JsonType type;
    union {
        int bool_val;
        int int_val;
        struct {
            char *str;
            size_t len;
        } string_val;
        struct {
            JsonValue *items;
            size_t count;
        } array_val;
        struct {
            char **keys;
            JsonValue *values;
            size_t count;
        } object_val;
    } data;
};

/* Parse JSON string into JsonValue. Returns JSON_ERROR on failure. */
JsonValue json_parse(const char *json);

/* Free parsed JSON value and all children */
void json_value_free(JsonValue *val);

/* Get string value from object by key (returns NULL if not found or wrong type) */
const char *json_object_get_string(const JsonValue *obj, const char *key);

/* Get int value from object by key (returns def if not found or wrong type) */
int json_object_get_int(const JsonValue *obj, const char *key, int def);

/* Get bool value from object by key (returns def if not found or wrong type) */
int json_object_get_bool(const JsonValue *obj, const char *key, int def);

#ifdef __cplusplus
}
#endif

#endif /* LOKI_JSON_H */
