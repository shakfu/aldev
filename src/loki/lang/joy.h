/* loki_joy.h - Joy concatenative language integration for Loki
 *
 * Provides livecoding capabilities by integrating the Joy music language
 * with the Loki editor. Joy is a concatenative (stack-based) language
 * with MIDI primitives for music composition.
 *
 * Usage from Lua:
 *   loki.joy.init()
 *   loki.joy.eval("midi-virtual 60 80 500 note")  -- play middle C
 *   loki.joy.define("play-c", "60 80 500 note")   -- define a word
 *   loki.joy.stop()
 *   loki.joy.cleanup()
 */

#ifndef LOKI_JOY_H
#define LOKI_JOY_H

/* Forward declarations */
#ifndef LOKI_CORE_H
struct editor_ctx;
typedef struct editor_ctx editor_ctx_t;
#endif

#ifndef lua_h
struct lua_State;
typedef struct lua_State lua_State;
#endif

/* Opaque Joy state - per-context state structure */
typedef struct LokiJoyState LokiJoyState;

/* ======================= Initialization ======================= */

/**
 * Initialize the Joy subsystem.
 * Creates the Joy context and initializes MIDI backend.
 *
 * @param ctx Editor context
 * @return 0 on success, -1 on error
 */
int loki_joy_init(editor_ctx_t *ctx);

/**
 * Cleanup the Joy subsystem.
 * Stops playback and releases resources.
 *
 * @param ctx Editor context
 */
void loki_joy_cleanup(editor_ctx_t *ctx);

/**
 * Check if Joy is initialized.
 *
 * @param ctx Editor context
 * @return 1 if initialized, 0 otherwise
 */
int loki_joy_is_initialized(editor_ctx_t *ctx);

/* ======================= Evaluation ======================= */

/**
 * Evaluate Joy code.
 * Executes Joy code synchronously.
 *
 * @param ctx Editor context
 * @param code Joy code string
 * @return 0 on success, -1 on error
 */
int loki_joy_eval(editor_ctx_t *ctx, const char *code);

/**
 * Load and evaluate a Joy source file.
 *
 * @param ctx Editor context
 * @param path Path to .joy file
 * @return 0 on success, -1 on error
 */
int loki_joy_load_file(editor_ctx_t *ctx, const char *path);

/**
 * Define a new Joy word.
 *
 * @param ctx Editor context
 * @param name Word name
 * @param body Word definition (Joy code)
 * @return 0 on success, -1 on error
 */
int loki_joy_define(editor_ctx_t *ctx, const char *name, const char *body);

/* ======================= Playback Control ======================= */

/**
 * Stop all MIDI playback and send panic (all notes off).
 *
 * @param ctx Editor context
 */
void loki_joy_stop(editor_ctx_t *ctx);

/**
 * Open a MIDI output port by index.
 *
 * @param ctx Editor context
 * @param port_idx Port index from list
 * @return 0 on success, -1 on error
 */
int loki_joy_open_port(editor_ctx_t *ctx, int port_idx);

/**
 * Create a virtual MIDI output port.
 *
 * @param ctx Editor context
 * @param name Port name (or NULL for "JoyMIDI")
 * @return 0 on success, -1 on error
 */
int loki_joy_open_virtual(editor_ctx_t *ctx, const char *name);

/**
 * List available MIDI output ports (prints to stdout).
 *
 * @param ctx Editor context
 */
void loki_joy_list_ports(editor_ctx_t *ctx);

/* ======================= Stack Operations ======================= */

/**
 * Push an integer onto the Joy stack.
 *
 * @param ctx Editor context
 * @param value Integer value
 */
void loki_joy_push_int(editor_ctx_t *ctx, int value);

/**
 * Push a string onto the Joy stack.
 *
 * @param ctx Editor context
 * @param value String value (will be copied)
 */
void loki_joy_push_string(editor_ctx_t *ctx, const char *value);

/**
 * Get the stack depth.
 *
 * @param ctx Editor context
 * @return Current stack depth
 */
int loki_joy_stack_depth(editor_ctx_t *ctx);

/**
 * Clear the Joy stack.
 *
 * @param ctx Editor context
 */
void loki_joy_stack_clear(editor_ctx_t *ctx);

/**
 * Print the Joy stack (for debugging).
 *
 * @param ctx Editor context
 */
void loki_joy_stack_print(editor_ctx_t *ctx);

/* ======================= Utility Functions ======================= */

/**
 * Get the last error message.
 *
 * @param ctx Editor context
 * @return Error message string (do not free), or NULL if no error
 */
const char *loki_joy_get_error(editor_ctx_t *ctx);

#endif /* LOKI_JOY_H */
