/* osc.h - Open Sound Control support for psnd
 *
 * Provides OSC server/client functionality using liblo for remote control
 * and inter-application communication.
 *
 * OSC support is optional and requires BUILD_OSC=ON at build time.
 */

#ifndef SHARED_OSC_H
#define SHARED_OSC_H

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct SharedContext;

/* Default OSC port */
#define PSND_OSC_DEFAULT_PORT 7770

#ifdef PSND_OSC

#include <lo/lo.h>

/* OSC handler callback type */
typedef int (*psnd_osc_handler_t)(const char *path, const char *types,
                                   lo_arg **argv, int argc, void *user_data);

/**
 * Initialize OSC subsystem on the specified port.
 *
 * @param ctx SharedContext to store OSC state
 * @param port Port number (use 0 for default 7770)
 * @return 0 on success, -1 on error
 */
int shared_osc_init(struct SharedContext *ctx, int port);

/**
 * Initialize OSC subsystem with interface binding.
 *
 * @param ctx SharedContext to store OSC state
 * @param port Port number (use 0 for default 7770)
 * @param iface Interface address to bind (NULL for all interfaces)
 * @return 0 on success, -1 on error
 */
int shared_osc_init_with_iface(struct SharedContext *ctx, int port, const char *iface);

/**
 * Initialize OSC subsystem with multicast support.
 *
 * @param ctx SharedContext to store OSC state
 * @param group Multicast group address (e.g., "239.0.0.1")
 * @param port Port number (use 0 for default 7770)
 * @return 0 on success, -1 on error
 */
int shared_osc_init_multicast(struct SharedContext *ctx, const char *group, int port);

/**
 * Set broadcast target for outgoing OSC messages.
 *
 * @param ctx SharedContext
 * @param host Target hostname or IP
 * @param port Target port as string
 * @return 0 on success, -1 on error
 */
int shared_osc_set_broadcast(struct SharedContext *ctx, const char *host, const char *port);

/**
 * Start the OSC server thread.
 *
 * @param ctx SharedContext
 * @return 0 on success, -1 on error
 */
int shared_osc_start(struct SharedContext *ctx);

/**
 * Stop OSC server and clean up resources.
 *
 * @param ctx SharedContext
 */
void shared_osc_cleanup(struct SharedContext *ctx);

/**
 * Check if OSC is enabled and running.
 *
 * @param ctx SharedContext
 * @return 1 if running, 0 otherwise
 */
int shared_osc_is_running(struct SharedContext *ctx);

/**
 * Get the port number OSC is listening on.
 *
 * @param ctx SharedContext
 * @return Port number, or 0 if not running
 */
int shared_osc_get_port(struct SharedContext *ctx);

/**
 * Send OSC message to broadcast target.
 *
 * @param ctx SharedContext
 * @param path OSC address path
 * @param types Type string (e.g., "ifs" for int, float, string)
 * @param ... Arguments matching types
 * @return Number of bytes sent, or -1 on error
 */
int shared_osc_send(struct SharedContext *ctx, const char *path, const char *types, ...);

/**
 * Send OSC message to specific address.
 *
 * @param host Target hostname or IP
 * @param port Target port as string
 * @param path OSC address path
 * @param types Type string
 * @param ... Arguments matching types
 * @return Number of bytes sent, or -1 on error
 */
int shared_osc_send_to(const char *host, const char *port,
                       const char *path, const char *types, ...);

/* Convenience functions for common messages */

/**
 * Send playback status change.
 *
 * @param ctx SharedContext
 * @param playing 1 if playing, 0 if stopped
 */
void shared_osc_send_playing(struct SharedContext *ctx, int playing);

/**
 * Send tempo change notification.
 *
 * @param ctx SharedContext
 * @param tempo Tempo in BPM
 */
void shared_osc_send_tempo(struct SharedContext *ctx, float tempo);

/**
 * Send MIDI note event.
 *
 * @param ctx SharedContext
 * @param channel MIDI channel (0-15)
 * @param pitch MIDI note number (0-127)
 * @param velocity Note velocity (0-127)
 */
void shared_osc_send_note(struct SharedContext *ctx, int channel, int pitch, int velocity);

/**
 * Register a custom OSC handler.
 *
 * @param ctx SharedContext
 * @param path OSC path pattern (supports wildcards)
 * @param types Expected type string (NULL for any)
 * @param handler Callback function
 * @param user_data User data for callback
 * @return 0 on success, -1 on error
 */
int shared_osc_add_handler(struct SharedContext *ctx, const char *path,
                           const char *types, psnd_osc_handler_t handler,
                           void *user_data);

/**
 * Set user data for built-in handlers (e.g., EditorSession pointer).
 *
 * @param ctx SharedContext
 * @param user_data Pointer to pass to handlers
 */
void shared_osc_set_user_data(struct SharedContext *ctx, void *user_data);

/* Forward declaration for editor context */
struct editor_ctx;

/**
 * Set language callback functions for OSC play/eval/stop handlers.
 * These are called from the editor to provide access to lang_bridge functions.
 *
 * @param eval_fn Function to evaluate code string
 * @param eval_buffer_fn Function to evaluate entire buffer
 * @param stop_all_fn Function to stop all playback
 */
void shared_osc_set_lang_callbacks(
    int (*eval_fn)(struct editor_ctx *ctx, const char *code),
    int (*eval_buffer_fn)(struct editor_ctx *ctx),
    void (*stop_all_fn)(struct editor_ctx *ctx)
);

/**
 * Set query callback functions for OSC query/reply handlers.
 *
 * @param is_playing_fn Function to check if any language is playing
 * @param get_filename_fn Function to get current filename
 * @param get_position_fn Function to get cursor position (line, col)
 */
void shared_osc_set_query_callbacks(
    int (*is_playing_fn)(struct editor_ctx *ctx),
    const char *(*get_filename_fn)(struct editor_ctx *ctx),
    void (*get_position_fn)(struct editor_ctx *ctx, int *line, int *col)
);

/**
 * Set rate limit for note messages (to prevent flooding).
 *
 * @param messages_per_second Maximum messages per second (0 = unlimited)
 */
void shared_osc_set_note_rate_limit(int messages_per_second);

/**
 * Get current note rate limit.
 *
 * @return Messages per second limit (0 = unlimited)
 */
int shared_osc_get_note_rate_limit(void);

#else /* !PSND_OSC */

/* Stub implementations when OSC is not available */

static inline int shared_osc_init(struct SharedContext *ctx, int port) {
    (void)ctx; (void)port;
    return -1;
}

static inline int shared_osc_init_with_iface(struct SharedContext *ctx, int port, const char *iface) {
    (void)ctx; (void)port; (void)iface;
    return -1;
}

static inline int shared_osc_init_multicast(struct SharedContext *ctx, const char *group, int port) {
    (void)ctx; (void)group; (void)port;
    return -1;
}

static inline int shared_osc_set_broadcast(struct SharedContext *ctx, const char *host, const char *port) {
    (void)ctx; (void)host; (void)port;
    return -1;
}

static inline int shared_osc_start(struct SharedContext *ctx) {
    (void)ctx;
    return -1;
}

static inline void shared_osc_cleanup(struct SharedContext *ctx) {
    (void)ctx;
}

static inline int shared_osc_is_running(struct SharedContext *ctx) {
    (void)ctx;
    return 0;
}

static inline int shared_osc_get_port(struct SharedContext *ctx) {
    (void)ctx;
    return 0;
}

static inline void shared_osc_send_playing(struct SharedContext *ctx, int playing) {
    (void)ctx; (void)playing;
}

static inline void shared_osc_send_tempo(struct SharedContext *ctx, float tempo) {
    (void)ctx; (void)tempo;
}

static inline void shared_osc_send_note(struct SharedContext *ctx, int channel, int pitch, int velocity) {
    (void)ctx; (void)channel; (void)pitch; (void)velocity;
}

static inline void shared_osc_set_user_data(struct SharedContext *ctx, void *user_data) {
    (void)ctx; (void)user_data;
}

struct editor_ctx;

static inline void shared_osc_set_lang_callbacks(
    int (*eval_fn)(struct editor_ctx *ctx, const char *code),
    int (*eval_buffer_fn)(struct editor_ctx *ctx),
    void (*stop_all_fn)(struct editor_ctx *ctx)
) {
    (void)eval_fn; (void)eval_buffer_fn; (void)stop_all_fn;
}

static inline void shared_osc_set_query_callbacks(
    int (*is_playing_fn)(struct editor_ctx *ctx),
    const char *(*get_filename_fn)(struct editor_ctx *ctx),
    void (*get_position_fn)(struct editor_ctx *ctx, int *line, int *col)
) {
    (void)is_playing_fn; (void)get_filename_fn; (void)get_position_fn;
}

static inline void shared_osc_set_note_rate_limit(int messages_per_second) {
    (void)messages_per_second;
}

static inline int shared_osc_get_note_rate_limit(void) {
    return 0;
}

#endif /* PSND_OSC */

#ifdef __cplusplus
}
#endif

#endif /* SHARED_OSC_H */
