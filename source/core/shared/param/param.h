/**
 * @file param.h
 * @brief Named parameter system for OSC/MIDI CC binding.
 *
 * Provides a thread-safe parameter store that can be bound to OSC addresses
 * and MIDI CC controllers. Parameters use atomic floats for lock-free access
 * from multiple threads (OSC, MIDI input, main thread).
 *
 * Usage:
 *   // Define a parameter
 *   shared_param_define(ctx, "cutoff", PARAM_TYPE_FLOAT, 20.0f, 20000.0f, 1000.0f);
 *
 *   // Bind to controllers
 *   shared_param_bind_osc(ctx, "cutoff", "/fader/1");
 *   shared_param_bind_midi_cc(ctx, "cutoff", 1, 74);  // Channel 1, CC 74
 *
 *   // Read value (thread-safe)
 *   float val;
 *   shared_param_get(ctx, "cutoff", &val);
 */

#ifndef SHARED_PARAM_H
#define SHARED_PARAM_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum number of parameters */
#define PARAM_MAX_COUNT 128

/* Maximum length of parameter name */
#define PARAM_MAX_NAME_LEN 32

/* Maximum length of OSC path */
#define PARAM_MAX_OSC_PATH_LEN 64

/**
 * @brief Parameter data type.
 */
typedef enum {
    PARAM_TYPE_FLOAT = 0,   /**< Floating point value */
    PARAM_TYPE_INT,         /**< Integer value (stored as float) */
    PARAM_TYPE_BOOL         /**< Boolean value (0.0 or 1.0) */
} ParamType;

/**
 * @brief A single named parameter.
 *
 * Uses atomic float for thread-safe value access.
 * Writers: OSC thread, MIDI input thread
 * Readers: Main thread, Lua, language interpreters
 */
typedef struct {
    char name[PARAM_MAX_NAME_LEN];          /**< Parameter name */
    ParamType type;                          /**< Data type */
    float min_val;                           /**< Minimum value */
    float max_val;                           /**< Maximum value */
    float default_val;                       /**< Default value */
    _Atomic float value;                     /**< Current value (thread-safe) */
    char osc_path[PARAM_MAX_OSC_PATH_LEN];  /**< Bound OSC path (empty = unbound) */
    int midi_channel;                        /**< MIDI channel (1-16, 0 = unbound) */
    int midi_cc;                             /**< MIDI CC number (0-127, -1 = unbound) */
    bool defined;                            /**< Slot is in use */
} SharedParam;

/**
 * @brief Parameter store containing all parameters.
 *
 * Includes a reverse lookup table for MIDI CC -> parameter index.
 */
typedef struct {
    SharedParam params[PARAM_MAX_COUNT];     /**< Parameter storage */
    int count;                               /**< Number of defined parameters */
    int8_t midi_cc_map[16][128];             /**< [channel-1][cc] -> param index, -1 = none */
} SharedParamStore;

/* Forward declaration */
struct SharedContext;

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

/**
 * @brief Initialize parameter store.
 * @param ctx Shared context containing param store.
 */
void shared_param_init(struct SharedContext* ctx);

/**
 * @brief Cleanup parameter store.
 * @param ctx Shared context.
 */
void shared_param_cleanup(struct SharedContext* ctx);

/* ============================================================================
 * Parameter Definition
 * ============================================================================ */

/**
 * @brief Define a new parameter.
 *
 * @param ctx Shared context.
 * @param name Parameter name (max 31 chars).
 * @param type Data type (float, int, bool).
 * @param min Minimum value.
 * @param max Maximum value.
 * @param def Default value (also set as current value).
 * @return Parameter index on success, -1 on error (name exists or store full).
 */
int shared_param_define(struct SharedContext* ctx, const char* name,
                        ParamType type, float min, float max, float def);

/**
 * @brief Find a parameter by name.
 *
 * @param ctx Shared context.
 * @param name Parameter name.
 * @return Parameter index, or -1 if not found.
 */
int shared_param_find(struct SharedContext* ctx, const char* name);

/**
 * @brief Undefine (remove) a parameter.
 *
 * @param ctx Shared context.
 * @param name Parameter name.
 * @return 0 on success, -1 if not found.
 */
int shared_param_undefine(struct SharedContext* ctx, const char* name);

/**
 * @brief Get number of defined parameters.
 *
 * @param ctx Shared context.
 * @return Number of parameters.
 */
int shared_param_count(struct SharedContext* ctx);

/**
 * @brief Get parameter at index.
 *
 * @param ctx Shared context.
 * @param idx Parameter index.
 * @return Pointer to parameter, or NULL if invalid index.
 */
const SharedParam* shared_param_at(struct SharedContext* ctx, int idx);

/* ============================================================================
 * Value Access (Thread-Safe)
 * ============================================================================ */

/**
 * @brief Get parameter value by name.
 *
 * @param ctx Shared context.
 * @param name Parameter name.
 * @param value Output: current value.
 * @return 0 on success, -1 if not found.
 */
int shared_param_get(struct SharedContext* ctx, const char* name, float* value);

/**
 * @brief Get parameter value by index (faster, no lookup).
 *
 * @param ctx Shared context.
 * @param idx Parameter index.
 * @return Current value, or 0.0 if invalid index.
 */
float shared_param_get_idx(struct SharedContext* ctx, int idx);

/**
 * @brief Set parameter value by name.
 *
 * Value is clamped to [min, max] range.
 *
 * @param ctx Shared context.
 * @param name Parameter name.
 * @param value New value.
 * @return 0 on success, -1 if not found.
 */
int shared_param_set(struct SharedContext* ctx, const char* name, float value);

/**
 * @brief Set parameter value by index (faster, no lookup).
 *
 * Value is clamped to [min, max] range.
 *
 * @param ctx Shared context.
 * @param idx Parameter index.
 * @param value New value.
 */
void shared_param_set_idx(struct SharedContext* ctx, int idx, float value);

/**
 * @brief Reset parameter to default value.
 *
 * @param ctx Shared context.
 * @param name Parameter name.
 * @return 0 on success, -1 if not found.
 */
int shared_param_reset(struct SharedContext* ctx, const char* name);

/**
 * @brief Reset all parameters to default values.
 *
 * @param ctx Shared context.
 */
void shared_param_reset_all(struct SharedContext* ctx);

/* ============================================================================
 * OSC Binding
 * ============================================================================ */

/**
 * @brief Bind parameter to an OSC path.
 *
 * When an OSC message arrives at this path with a float argument,
 * the parameter value is updated.
 *
 * @param ctx Shared context.
 * @param name Parameter name.
 * @param path OSC path (e.g., "/fader/1").
 * @return 0 on success, -1 if parameter not found.
 */
int shared_param_bind_osc(struct SharedContext* ctx, const char* name, const char* path);

/**
 * @brief Unbind parameter from OSC.
 *
 * @param ctx Shared context.
 * @param name Parameter name.
 * @return 0 on success, -1 if not found.
 */
int shared_param_unbind_osc(struct SharedContext* ctx, const char* name);

/**
 * @brief Find parameter by OSC path.
 *
 * @param ctx Shared context.
 * @param path OSC path to search for.
 * @return Parameter index, or -1 if not found.
 */
int shared_param_find_by_osc_path(struct SharedContext* ctx, const char* path);

/* ============================================================================
 * MIDI CC Binding
 * ============================================================================ */

/**
 * @brief Bind parameter to MIDI CC.
 *
 * When a CC message is received on the specified channel and controller,
 * the parameter value is updated. CC value (0-127) is scaled to [min, max].
 *
 * @param ctx Shared context.
 * @param name Parameter name.
 * @param channel MIDI channel (1-16).
 * @param cc CC number (0-127).
 * @return 0 on success, -1 if parameter not found or invalid channel/cc.
 */
int shared_param_bind_midi_cc(struct SharedContext* ctx, const char* name,
                               int channel, int cc);

/**
 * @brief Unbind parameter from MIDI CC.
 *
 * @param ctx Shared context.
 * @param name Parameter name.
 * @return 0 on success, -1 if not found.
 */
int shared_param_unbind_midi_cc(struct SharedContext* ctx, const char* name);

/**
 * @brief Handle incoming MIDI CC message.
 *
 * Called from MIDI input callback. Updates bound parameter if any.
 *
 * @param ctx Shared context.
 * @param channel MIDI channel (1-16).
 * @param cc CC number (0-127).
 * @param value CC value (0-127).
 * @return 1 if a parameter was updated, 0 otherwise.
 */
int shared_param_handle_midi_cc(struct SharedContext* ctx, int channel, int cc, int value);

#ifdef __cplusplus
}
#endif

#endif /* SHARED_PARAM_H */
