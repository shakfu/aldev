/* loki_search.h - Text search functionality
 *
 * This header declares the public API for incremental text search.
 * Search is interactive and updates as the user types, with visual
 * highlighting of matches and arrow key navigation between results.
 */

#ifndef LOKI_SEARCH_H
#define LOKI_SEARCH_H

#include "loki_internal.h"

/* Incremental text search with arrow key navigation
 * Allows user to search forward/backward, cancel with ESC,
 * or accept with ENTER. Highlights matches in real-time. */
void editor_find(editor_ctx_t *ctx, int fd);

#endif /* LOKI_SEARCH_H */
