/* cli.c - Command-line argument parsing implementation
 *
 * Extracts CLI parsing from loki_editor_main() for reuse by different hosts.
 */

#include "cli.h"
#include "psnd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void editor_cli_print_version(void) {
    printf(PSND_NAME " %s\n", PSND_VERSION);
}

void editor_cli_print_usage(void) {
    printf("Usage: " PSND_NAME " [options] <filename>\n");
    printf("\nOptions:\n");
    printf("  -h, --help          Show this help message\n");
    printf("  -v, --version       Show version information\n");
    printf("  -sf PATH            Use built-in synth with soundfont (.sf2)\n");
    printf("  -cs PATH            Use Csound synthesis with .csd file\n");
    printf("  --line-numbers      Show line numbers\n");
    printf("  --word-wrap         Enable word wrap\n");
    printf("  --json-rpc          Run in JSON-RPC mode (stdin/stdout)\n");
    printf("  --json-rpc-single   Run single JSON-RPC command and exit\n");
    printf("  --rows N            Screen rows for headless mode (default: 24)\n");
    printf("  --cols N            Screen cols for headless mode (default: 80)\n");
#ifdef LOKI_WEB_HOST
    printf("\nWeb Server Mode:\n");
    printf("  --web               Run as web server (browser-based editing)\n");
    printf("  --web-port N        Web server port (default: 8080)\n");
    printf("  --web-root PATH     Directory containing web UI files\n");
#endif
#ifdef LOKI_WEBVIEW_HOST
    printf("\nNative Webview Mode:\n");
    printf("  --native            Run in native webview window (no browser needed)\n");
#endif
#ifdef PSND_OSC
    printf("\nOSC (Open Sound Control):\n");
    printf("  --osc               Enable OSC server (default port: 7770)\n");
    printf("  --osc-port N        OSC server port\n");
    printf("  --osc-send H:P      Broadcast events to host:port\n");
#endif
    printf("\nInteractive mode (default):\n");
    printf("  " PSND_NAME " <file.alda>           Open file in editor\n");
    printf("  " PSND_NAME " -sf gm.sf2 song.alda  Open with TinySoundFont synth\n");
    printf("  " PSND_NAME " -cs inst.csd song.alda Open with Csound synthesis\n");
#ifdef LOKI_WEB_HOST
    printf("  " PSND_NAME " --web song.alda       Open in browser at localhost:8080\n");
#endif
    printf("\nKeybindings:\n");
    printf("  Ctrl-E    Play current part or selection\n");
    printf("  Ctrl-P    Play entire file\n");
    printf("  Ctrl-G    Stop playback\n");
    printf("  Ctrl-S    Save file\n");
    printf("  Ctrl-Q    Quit\n");
    printf("  Ctrl-F    Find\n");
    printf("  Ctrl-L    Lua console\n");
}

int editor_cli_parse(int argc, char **argv, EditorCliArgs *args) {
    if (!args) return -1;

    /* Initialize to defaults */
    memset(args, 0, sizeof(EditorCliArgs));

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        /* Help flags */
        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            args->show_help = 1;
            return 0;
        }

        /* Version flags */
        if (strcmp(arg, "--version") == 0 || strcmp(arg, "-v") == 0) {
            args->show_version = 1;
            return 0;
        }

        /* Soundfont option */
        if (strcmp(arg, "-sf") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -sf requires a path argument\n");
                return -1;
            }
            args->soundfont_path = argv[++i];
            continue;
        }

        /* Csound option */
        if (strcmp(arg, "-cs") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -cs requires a path argument\n");
                return -1;
            }
            args->csound_path = argv[++i];
            continue;
        }

        /* Line numbers option */
        if (strcmp(arg, "--line-numbers") == 0) {
            args->line_numbers = 1;
            continue;
        }

        /* Word wrap option */
        if (strcmp(arg, "--word-wrap") == 0) {
            args->word_wrap = 1;
            continue;
        }

        /* JSON-RPC mode */
        if (strcmp(arg, "--json-rpc") == 0) {
            args->json_rpc = 1;
            continue;
        }

        /* JSON-RPC single-shot mode */
        if (strcmp(arg, "--json-rpc-single") == 0) {
            args->json_rpc_single = 1;
            continue;
        }

        /* Screen rows for headless mode */
        if (strcmp(arg, "--rows") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --rows requires a number argument\n");
                return -1;
            }
            args->rows = atoi(argv[++i]);
            if (args->rows <= 0) {
                fprintf(stderr, "Error: --rows must be a positive number\n");
                return -1;
            }
            continue;
        }

        /* Screen cols for headless mode */
        if (strcmp(arg, "--cols") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --cols requires a number argument\n");
                return -1;
            }
            args->cols = atoi(argv[++i]);
            if (args->cols <= 0) {
                fprintf(stderr, "Error: --cols must be a positive number\n");
                return -1;
            }
            continue;
        }

        /* Web server mode */
        if (strcmp(arg, "--web") == 0) {
            args->web_mode = 1;
            continue;
        }

        /* Web server port */
        if (strcmp(arg, "--web-port") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --web-port requires a number argument\n");
                return -1;
            }
            args->web_port = atoi(argv[++i]);
            if (args->web_port <= 0 || args->web_port > 65535) {
                fprintf(stderr, "Error: --web-port must be between 1 and 65535\n");
                return -1;
            }
            continue;
        }

        /* Web root directory */
        if (strcmp(arg, "--web-root") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --web-root requires a path argument\n");
                return -1;
            }
            args->web_root = argv[++i];
            continue;
        }

        /* Native webview mode */
        if (strcmp(arg, "--native") == 0) {
            args->native_mode = 1;
            continue;
        }

        /* OSC enable */
        if (strcmp(arg, "--osc") == 0) {
            args->osc_enabled = 1;
            continue;
        }

        /* OSC port */
        if (strcmp(arg, "--osc-port") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --osc-port requires a number argument\n");
                return -1;
            }
            args->osc_port = atoi(argv[++i]);
            if (args->osc_port <= 0 || args->osc_port > 65535) {
                fprintf(stderr, "Error: --osc-port must be between 1 and 65535\n");
                return -1;
            }
            args->osc_enabled = 1; /* --osc-port implies --osc */
            continue;
        }

        /* OSC send target (host:port) */
        if (strcmp(arg, "--osc-send") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --osc-send requires host:port argument\n");
                return -1;
            }
            const char *target = argv[++i];
            /* Parse host:port - find the last colon (IPv6 safe) */
            const char *colon = strrchr(target, ':');
            if (!colon || colon == target) {
                fprintf(stderr, "Error: --osc-send requires host:port format\n");
                return -1;
            }
            /* Store host (need to allocate since we modify) */
            static char osc_host_buf[256];
            size_t host_len = colon - target;
            if (host_len >= sizeof(osc_host_buf)) {
                host_len = sizeof(osc_host_buf) - 1;
            }
            strncpy(osc_host_buf, target, host_len);
            osc_host_buf[host_len] = '\0';
            args->osc_send_host = osc_host_buf;
            args->osc_send_port = colon + 1;
            args->osc_enabled = 1; /* --osc-send implies --osc */
            continue;
        }

        /* Unknown option */
        if (arg[0] == '-') {
            fprintf(stderr, "Error: Unknown option: %s\n", arg);
            return -1;
        }

        /* Non-option argument is the filename */
        if (args->filename == NULL) {
            args->filename = arg;
        } else {
            fprintf(stderr, "Error: Too many arguments\n");
            return -1;
        }
    }

    return 0;
}
