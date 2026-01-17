/**
 * @file repl_commands.h
 * @brief Shared REPL command processor for all music languages.
 *
 * Provides a unified command API that works across Alda and Joy REPLs.
 * Commands start with ':' (optional) and handle common operations like
 * quitting, listing MIDI ports, loading soundfonts, and controlling backends.
 */

#ifndef SHARED_REPL_COMMANDS_H
#define SHARED_REPL_COMMANDS_H

#include "context.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Return values for shared_process_command */
#define REPL_CMD_QUIT      1   /* User requested quit */
#define REPL_CMD_HANDLED   0   /* Command was handled */
#define REPL_CMD_NOT_CMD   2   /* Not a command, evaluate as language */

/**
 * @brief Process a shared REPL command.
 *
 * Commands start with ':' (optional) and include:
 *   :q :quit :exit    - quit
 *   :h :help :?       - help (prints generic, caller may augment)
 *   :l :list          - list MIDI ports
 *   :p :panic         - all notes off
 *   :s :stop          - stop playback
 *   :sf PATH          - load soundfont
 *   :synth :builtin   - enable TSF
 *   :midi             - disable TSF
 *   :presets          - list presets
 *   :link [on|off]    - Ableton Link
 *   :link-tempo BPM   - set tempo
 *   :link-status      - show status
 *   :cs PATH          - load Csound
 *   :csound           - enable Csound
 *   :cs-disable       - disable Csound
 *   :cs-status        - status
 *   :cs-play PATH     - play file
 *   :virtual [NAME]   - create virtual port
 *
 * @param ctx SharedContext for backend operations.
 * @param input User input string.
 * @param stop_callback Optional callback to stop playback (language-specific).
 * @return REPL_CMD_QUIT, REPL_CMD_HANDLED, or REPL_CMD_NOT_CMD.
 */
int shared_process_command(SharedContext* ctx, const char* input,
                           void (*stop_callback)(void));

/**
 * @brief Print shared command help.
 */
void shared_print_command_help(void);

/* ============================================================================
 * Link Callback Support
 * ============================================================================ */

/**
 * @brief Initialize Link callbacks for REPL use.
 *
 * Registers callbacks that print status changes to stdout.
 * Call this once during REPL initialization.
 *
 * @param ctx SharedContext (used to sync tempo).
 */
void shared_repl_link_init_callbacks(SharedContext* ctx);

/**
 * @brief Poll for Link events and invoke callbacks.
 *
 * Should be called periodically in the REPL main loop.
 * When Link is enabled and peers/tempo change, prints status.
 */
void shared_repl_link_check(void);

/**
 * @brief Clear Link callbacks.
 *
 * Call during REPL cleanup.
 */
void shared_repl_link_cleanup_callbacks(void);

#ifdef __cplusplus
}
#endif

#endif /* SHARED_REPL_COMMANDS_H */
