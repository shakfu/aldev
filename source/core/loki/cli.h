/* cli.h - Command-line argument parsing for loki editor
 *
 * This module extracts CLI parsing logic from the editor, enabling:
 * - Clean separation of configuration from execution
 * - Reusable argument parsing for different hosts
 * - Testable CLI logic
 */

#ifndef LOKI_CLI_H
#define LOKI_CLI_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * EditorCliArgs - Parsed command-line arguments for the editor.
 *
 * All string pointers reference argv memory (do not free).
 */
typedef struct {
    const char *filename;       /* File to open (NULL if none) */
    const char *soundfont_path; /* Path to soundfont for TinySoundFont (-sf) */
    const char *csound_path;    /* Path to CSD file for Csound (-cs) */
    const char *web_root;       /* Web UI directory for web mode (--web-root) */
    int show_help;              /* User requested help (-h, --help) */
    int show_version;           /* User requested version (-v, --version) */
    int line_numbers;           /* Enable line numbers (--line-numbers) */
    int word_wrap;              /* Enable word wrap (--word-wrap) */
    int json_rpc;               /* Run in JSON-RPC mode (--json-rpc) */
    int json_rpc_single;        /* Single-shot JSON-RPC (--json-rpc-single) */
    int web_mode;               /* Run as web server (--web) */
    int web_port;               /* Web server port (--web-port, default 8080) */
    int rows;                   /* Screen rows for headless mode (--rows) */
    int cols;                   /* Screen cols for headless mode (--cols) */
} EditorCliArgs;

/**
 * Parse command-line arguments for the editor.
 *
 * @param argc  Argument count
 * @param argv  Argument vector
 * @param args  Output: parsed arguments
 * @return 0 on success, -1 on error (message printed to stderr)
 */
int editor_cli_parse(int argc, char **argv, EditorCliArgs *args);

/**
 * Print usage information to stdout.
 */
void editor_cli_print_usage(void);

/**
 * Print version information to stdout.
 */
void editor_cli_print_version(void);

#ifdef __cplusplus
}
#endif

#endif /* LOKI_CLI_H */
