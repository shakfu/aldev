/**
 * @file mhs_context.h
 * @brief MHS (Micro Haskell MIDI) state structure for editor integration.
 *
 * Defines LokiMhsState - the per-context state for MHS in the Loki editor.
 */

#ifndef LOKI_MHS_CONTEXT_H
#define LOKI_MHS_CONTEXT_H

/* Forward declaration to avoid circular includes */
struct SharedContext;
struct editor_ctx;
typedef struct editor_ctx editor_ctx_t;

/* Error buffer size */
#define MHS_ERROR_BUFSIZE 512

/**
 * @brief MHS state structure for editor integration.
 *
 * This struct holds per-context state for MHS in the Loki editor.
 * It manages the connection to SharedContext for MIDI/audio output.
 */
typedef struct LokiMhsState {
    /* Initialization flag */
    int initialized;

    /* Playback state */
    int is_playing;

    /* Editor-owned shared context (not owned by us) */
    struct SharedContext *shared;

    /* Error handling */
    char last_error[MHS_ERROR_BUFSIZE];
} LokiMhsState;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Initialize MHS for the editor context.
 *
 * Sets up MHS runtime and connects to the editor's SharedContext.
 *
 * @param ctx Editor context.
 * @return 0 on success, -1 on error.
 */
int loki_mhs_init(editor_ctx_t *ctx);

/**
 * @brief Cleanup MHS resources.
 *
 * Stops any active playback and releases resources.
 *
 * @param ctx Editor context.
 */
void loki_mhs_cleanup(editor_ctx_t *ctx);

/**
 * @brief Check if MHS is initialized.
 *
 * @param ctx Editor context.
 * @return 1 if initialized, 0 otherwise.
 */
int loki_mhs_is_initialized(editor_ctx_t *ctx);

/* ============================================================================
 * Playback Functions
 * ============================================================================ */

/**
 * @brief Evaluate Haskell code.
 *
 * Evaluates the given Haskell expression/program using MHS.
 * MIDI output goes through SharedContext.
 *
 * @param ctx Editor context.
 * @param code Haskell code to evaluate.
 * @return 0 on success, -1 on error.
 */
int loki_mhs_eval(editor_ctx_t *ctx, const char *code);

/**
 * @brief Evaluate a Haskell file.
 *
 * Loads and evaluates the given Haskell file.
 *
 * @param ctx Editor context.
 * @param path Path to Haskell file.
 * @return 0 on success, -1 on error.
 */
int loki_mhs_eval_file(editor_ctx_t *ctx, const char *path);

/**
 * @brief Stop any active MHS playback.
 *
 * Sends MIDI panic and stops playback.
 *
 * @param ctx Editor context.
 */
void loki_mhs_stop(editor_ctx_t *ctx);

/**
 * @brief Check if MHS is currently playing.
 *
 * @param ctx Editor context.
 * @return 1 if playing, 0 otherwise.
 */
int loki_mhs_is_playing(editor_ctx_t *ctx);

/* ============================================================================
 * Error Handling
 * ============================================================================ */

/**
 * @brief Get the last error message.
 *
 * @param ctx Editor context.
 * @return Error string or NULL if no error.
 */
const char *loki_mhs_get_error(editor_ctx_t *ctx);

/* ============================================================================
 * MIDI Port Functions
 * ============================================================================ */

/**
 * @brief List available MIDI output ports.
 *
 * @return Number of available ports.
 */
int loki_mhs_list_ports(void);

/**
 * @brief Get the name of a MIDI port.
 *
 * @param index Port index.
 * @return Port name or empty string if invalid.
 */
const char *loki_mhs_port_name(int index);

/**
 * @brief Open a MIDI port by index.
 *
 * @param index Port index from list_ports.
 * @return 0 on success, -1 on error.
 */
int loki_mhs_open_port(int index);

/**
 * @brief Open a virtual MIDI port.
 *
 * @param name Name for the virtual port.
 * @return 0 on success, -1 on error.
 */
int loki_mhs_open_virtual(const char *name);

#endif /* LOKI_MHS_CONTEXT_H */
