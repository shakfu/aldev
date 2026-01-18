/* jsonrpc.h - JSON-RPC harness for editor testing
 *
 * This module provides a command-line interface for testing the editor
 * abstractions via JSON-RPC over stdio.
 *
 * Commands:
 *   {"cmd": "load", "file": "path/to/file"}
 *   {"cmd": "save"}
 *   {"cmd": "event", "type": "key", "code": 105}
 *   {"cmd": "event", "type": "key", "code": 27, "modifiers": 1}  // Ctrl
 *   {"cmd": "snapshot"}
 *   {"cmd": "status"}
 *   {"cmd": "quit"}
 *
 * Responses:
 *   {"ok": true, ...}
 *   {"ok": false, "error": "message"}
 */

#ifndef LOKI_JSONRPC_H
#define LOKI_JSONRPC_H

#include "session.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Run the JSON-RPC harness in single-shot mode.
 *
 * Reads one JSON command from stdin, processes it, and writes
 * the JSON response to stdout. Exits after processing.
 *
 * @param config  Optional session configuration (NULL for defaults)
 * @return Exit code (0 on success)
 */
int jsonrpc_run_single(const EditorConfig *config);

/**
 * Run the JSON-RPC harness in interactive mode.
 *
 * Reads JSON commands from stdin (one per line), processes each,
 * and writes JSON responses to stdout. Continues until quit command
 * or EOF.
 *
 * @param config  Optional session configuration (NULL for defaults)
 * @return Exit code (0 on success)
 */
int jsonrpc_run_interactive(const EditorConfig *config);

/**
 * Serialize an EditorViewModel to JSON.
 *
 * @param vm  View model to serialize
 * @return JSON string (caller must free), or NULL on error
 */
char *jsonrpc_serialize_viewmodel(const EditorViewModel *vm);

#ifdef __cplusplus
}
#endif

#endif /* LOKI_JSONRPC_H */
