/* host_web.h - Web server host interface for running the editor
 *
 * This module provides a mongoose-based HTTP/WebSocket host, enabling:
 * - Browser-based editing via xterm.js or CodeMirror frontend
 * - JSON-RPC over WebSocket for event handling
 * - Snapshot-based rendering pushed to connected clients
 *
 * Build with -DBUILD_WEB_HOST=ON to enable this module.
 */

#ifndef LOKI_HOST_WEB_H
#define LOKI_HOST_WEB_H

#include "host.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LOKI_WEB_HOST

/**
 * Create a web server host for browser-based editing.
 *
 * The web host:
 * - Starts an HTTP server on the specified port
 * - Serves static files from web_root directory
 * - Accepts WebSocket connections at /ws
 * - Translates JSON-RPC commands to EditorEvents
 * - Broadcasts ViewModel snapshots to connected clients
 *
 * @param port      Port to listen on (e.g., 8080)
 * @param web_root  Directory containing static web files (index.html, etc.)
 *                  If NULL, uses embedded minimal UI
 * @return Host instance, or NULL on error
 */
EditorHost *editor_host_web_create(int port, const char *web_root);

/**
 * Get the port the web host is listening on.
 *
 * Useful when port 0 was specified (OS assigns port).
 *
 * @param host  Web host instance
 * @return Port number, or -1 on error
 */
int editor_host_web_get_port(EditorHost *host);

/**
 * Run the web host main loop.
 *
 * This is a convenience function that creates a session and runs until
 * the session quits or an error occurs. It prints the URL to stdout.
 *
 * @param port      Port to listen on
 * @param web_root  Static file directory (NULL for embedded UI)
 * @param config    Session configuration
 * @return Exit code (0 on success)
 */
int editor_host_web_run(int port, const char *web_root, const EditorConfig *config);

#endif /* LOKI_WEB_HOST */

#ifdef __cplusplus
}
#endif

#endif /* LOKI_HOST_WEB_H */
