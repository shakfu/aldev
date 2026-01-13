/* loki_modal.h - Modal editing (vim-like modes)
 *
 * This header declares the public API for modal editing functionality.
 * Modal editing provides vim-like modes (NORMAL, INSERT, VISUAL) for
 * efficient keyboard-only text editing.
 */

#ifndef LOKI_MODAL_H
#define LOKI_MODAL_H

#include "loki_internal.h"

/* Process a single keypress with modal editing support.
 * This is the main entry point for all keyboard input.
 * Handles mode switching and dispatches to appropriate mode handler. */
void modal_process_keypress(editor_ctx_t *ctx, int fd);

#endif /* LOKI_MODAL_H */
