/**
 * @file repl_commands.c
 * @brief Shared REPL command processor implementation.
 *
 * Handles common commands across all music language REPLs.
 */

#include "repl_commands.h"
#include "psnd.h"
#include "context.h"
#include "audio/audio.h"
#include "midi/midi.h"
#include "link/link.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Strip leading whitespace */
static const char* skip_whitespace(const char* s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

/* Check if string starts with prefix */
static int starts_with(const char* str, const char* prefix) {
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

void shared_print_command_help(void) {
    printf("Commands (use with or without : prefix):\n");
    printf("  :q :quit :exit    Exit the REPL\n");
    printf("  :h :help :?       Show this help\n");
    printf("  :l :list          List MIDI ports\n");
    printf("  :s :stop          Stop current playback\n");
    printf("  :p :panic         All notes off\n");
    printf("\n");
    printf("Synth Commands:\n");
    printf("  :sf PATH          Load soundfont and use built-in synth\n");
    printf("  :presets          List soundfont presets\n");
    printf("  :midi             Switch to MIDI output\n");
    printf("  :synth :builtin   Switch to built-in synth\n");
    printf("\n");
    printf("Link Commands:\n");
    printf("  :link [on|off]    Enable/disable Ableton Link\n");
    printf("  :link-tempo BPM   Set Link tempo\n");
    printf("  :link-status      Show Link status\n");
    printf("\n");
    printf("Csound Commands:\n");
    printf("  :cs PATH          Load a CSD file and enable Csound\n");
    printf("  :csound           Enable Csound as audio backend\n");
    printf("  :cs-disable       Disable Csound\n");
    printf("  :cs-status        Show Csound status\n");
    printf("\n");
    printf("Playback:\n");
    printf("  :play PATH        Play a file (dispatches by extension)\n");
    printf("\n");
    printf("MIDI Port Commands:\n");
    printf("  :virtual [NAME]   Create virtual MIDI port\n");
    printf("\n");
}

int shared_process_command(SharedContext* ctx, const char* input,
                           void (*stop_callback)(void)) {
    if (!input) return REPL_CMD_NOT_CMD;

    /* Skip leading whitespace */
    const char* cmd = skip_whitespace(input);

    /* Empty input is not a command */
    if (*cmd == '\0') return REPL_CMD_NOT_CMD;

    /* Strip optional : prefix */
    if (cmd[0] == ':') {
        cmd++;
    }

    /* Quit commands */
    if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0 || strcmp(cmd, "q") == 0) {
        return REPL_CMD_QUIT;
    }

    /* Help commands - let language REPLs handle these so they can add
     * language-specific help after the shared help */
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0 || strcmp(cmd, "?") == 0) {
        return REPL_CMD_NOT_CMD;
    }

    /* List MIDI ports */
    if (strcmp(cmd, "list") == 0 || strcmp(cmd, "l") == 0) {
        shared_midi_list_ports(ctx);
        return REPL_CMD_HANDLED;
    }

    /* Stop playback */
    if (strcmp(cmd, "stop") == 0 || strcmp(cmd, "s") == 0) {
        if (stop_callback) {
            stop_callback();
        }
        shared_send_panic(ctx);
        printf("Playback stopped\n");
        return REPL_CMD_HANDLED;
    }

    /* Panic (all notes off) */
    if (strcmp(cmd, "panic") == 0 || strcmp(cmd, "p") == 0) {
        if (stop_callback) {
            stop_callback();
        }
        shared_send_panic(ctx);
        printf("All notes off\n");
        return REPL_CMD_HANDLED;
    }

    /* Load soundfont: :sf PATH or sf PATH */
    if (starts_with(cmd, "sf-load ") || starts_with(cmd, "sf ")) {
        const char* path = (starts_with(cmd, "sf ")) ? cmd + 3 : cmd + 8;
        path = skip_whitespace(path);
        if (*path == '\0') {
            printf("Usage: :sf PATH\n");
        } else {
            if (shared_tsf_load_soundfont(path) == 0) {
                printf("Loaded soundfont: %s\n", path);
                if (shared_tsf_enable() == 0) {
                    ctx->tsf_enabled = 1;
                    printf("Switched to built-in synth\n");
                }
            } else {
                printf("Failed to load soundfont: %s\n", path);
            }
        }
        return REPL_CMD_HANDLED;
    }

    /* Enable built-in synth */
    if (strcmp(cmd, "builtin") == 0 || strcmp(cmd, "synth") == 0) {
        if (!shared_tsf_has_soundfont()) {
            printf("No soundfont loaded. Use ':sf PATH' first.\n");
        } else if (shared_tsf_enable() == 0) {
            ctx->tsf_enabled = 1;
            printf("Switched to built-in synth\n");
        } else {
            printf("Failed to enable built-in synth\n");
        }
        return REPL_CMD_HANDLED;
    }

    /* Switch to MIDI output */
    if (strcmp(cmd, "midi") == 0) {
        shared_tsf_disable();
        ctx->tsf_enabled = 0;
        if (shared_midi_is_open(ctx)) {
            printf("Switched to MIDI output\n");
        } else {
            printf("Built-in synth disabled (no MIDI output available)\n");
        }
        return REPL_CMD_HANDLED;
    }

    /* List soundfont presets */
    if (strcmp(cmd, "sf-list") == 0 || strcmp(cmd, "presets") == 0) {
        if (!shared_tsf_has_soundfont()) {
            printf("No soundfont loaded\n");
        } else {
            int count = shared_tsf_get_preset_count();
            printf("Soundfont presets (%d):\n", count);
            for (int i = 0; i < count && i < 128; i++) {
                const char* name = shared_tsf_get_preset_name(i);
                if (name && name[0] != '\0') {
                    printf("  %3d: %s\n", i, name);
                }
            }
        }
        return REPL_CMD_HANDLED;
    }

    /* Virtual MIDI port: :virtual [NAME] */
    if (strcmp(cmd, "virtual") == 0) {
        if (shared_midi_open_virtual(ctx, PSND_MIDI_PORT_NAME) == 0) {
            printf("Created virtual MIDI port: " PSND_MIDI_PORT_NAME "\n");
        } else {
            printf("Failed to create virtual MIDI port\n");
        }
        return REPL_CMD_HANDLED;
    }

    if (starts_with(cmd, "virtual ")) {
        const char* name = skip_whitespace(cmd + 8);
        if (*name == '\0') {
            name = PSND_MIDI_PORT_NAME;
        }
        if (shared_midi_open_virtual(ctx, name) == 0) {
            printf("Created virtual MIDI port: %s\n", name);
        } else {
            printf("Failed to create virtual MIDI port\n");
        }
        return REPL_CMD_HANDLED;
    }

    /* Link commands */
    if (strcmp(cmd, "link") == 0 || strcmp(cmd, "link on") == 0 ||
        strcmp(cmd, "link-enable") == 0) {
        /* Initialize Link if not already done */
        if (!shared_link_is_initialized()) {
            if (shared_link_init(ctx->tempo > 0 ? ctx->tempo : 120.0) != 0) {
                printf("Failed to initialize Link\n");
                return REPL_CMD_HANDLED;
            }
        }
        shared_link_enable(1);
        ctx->link_enabled = 1;
        printf("Link enabled (tempo: %.1f BPM, peers: %llu)\n",
               shared_link_get_tempo(), (unsigned long long)shared_link_num_peers());
        return REPL_CMD_HANDLED;
    }

    if (strcmp(cmd, "link off") == 0 || strcmp(cmd, "link-disable") == 0) {
        shared_link_enable(0);
        ctx->link_enabled = 0;
        printf("Link disabled\n");
        return REPL_CMD_HANDLED;
    }

    if (starts_with(cmd, "link-tempo ")) {
        const char* tempo_str = skip_whitespace(cmd + 11);
        double bpm = atof(tempo_str);
        if (bpm >= 20.0 && bpm <= 999.0) {
            if (!shared_link_is_initialized()) {
                if (shared_link_init(bpm) != 0) {
                    printf("Failed to initialize Link\n");
                    return REPL_CMD_HANDLED;
                }
            }
            shared_link_set_tempo(bpm);
            printf("Link tempo set to %.1f BPM\n", bpm);
        } else {
            printf("Invalid tempo (must be 20-999 BPM)\n");
        }
        return REPL_CMD_HANDLED;
    }

    if (strcmp(cmd, "link-status") == 0) {
        if (shared_link_is_enabled()) {
            printf("Link: enabled, tempo: %.1f BPM, peers: %llu, beat: %.2f\n",
                   shared_link_get_tempo(),
                   (unsigned long long)shared_link_num_peers(),
                   shared_link_get_beat(4.0));
        } else if (shared_link_is_initialized()) {
            printf("Link: initialized but disabled\n");
        } else {
            printf("Link: not initialized\n");
        }
        return REPL_CMD_HANDLED;
    }

    /* Csound commands */
    if (starts_with(cmd, "cs-load ") || starts_with(cmd, "cs ")) {
        const char* path = (starts_with(cmd, "cs ")) ? cmd + 3 : cmd + 8;
        path = skip_whitespace(path);
        if (*path == '\0') {
            printf("Usage: :cs PATH\n");
        } else {
            if (shared_csound_load(path) == 0) {
                printf("Csound: Loaded %s\n", path);
                if (shared_csound_enable() == 0) {
                    ctx->csound_enabled = 1;
                    printf("Csound enabled\n");
                }
            } else {
                printf("Csound: Failed to load CSD file\n");
            }
        }
        return REPL_CMD_HANDLED;
    }

    if (strcmp(cmd, "cs-enable") == 0 || strcmp(cmd, "csound") == 0) {
        if (shared_csound_enable() == 0) {
            ctx->csound_enabled = 1;
            printf("Csound enabled\n");
        } else {
            printf("Csound: Failed to enable (load a CSD file first)\n");
        }
        return REPL_CMD_HANDLED;
    }

    if (strcmp(cmd, "cs-disable") == 0) {
        shared_csound_disable();
        ctx->csound_enabled = 0;
        printf("Csound disabled\n");
        return REPL_CMD_HANDLED;
    }

    if (strcmp(cmd, "cs-status") == 0) {
        if (shared_csound_is_enabled()) {
            printf("Csound: enabled\n");
        } else {
            printf("Csound: disabled\n");
        }
        return REPL_CMD_HANDLED;
    }

    /* Generic :play command - dispatches by file extension */
    if (starts_with(cmd, "play ")) {
        const char* path = skip_whitespace(cmd + 5);
        if (*path == '\0') {
            printf("Usage: :play PATH\n");
            return REPL_CMD_HANDLED;
        }

        /* Find file extension */
        const char* ext = strrchr(path, '.');
        if (!ext) {
            printf("Cannot determine file type (no extension)\n");
            return REPL_CMD_HANDLED;
        }

        /* Dispatch based on extension */
        if (strcmp(ext, ".csd") == 0 || strcmp(ext, ".orc") == 0) {
            /* Csound file */
            printf("Playing %s (Ctrl-C to stop)...\n", path);
            int result = shared_csound_play_file(path, 1);
            if (result != 0) {
                printf("Csound: Failed to play file (is Csound backend available?)\n");
            }
            return REPL_CMD_HANDLED;
        }

        /* For language files (.alda, .joy, .scm), let the REPL handle it */
        if (strcmp(ext, ".alda") == 0 || strcmp(ext, ".joy") == 0 ||
            strcmp(ext, ".scm") == 0 || strcmp(ext, ".lisp") == 0) {
            return REPL_CMD_NOT_CMD;
        }

        printf("Unknown file type: %s\n", ext);
        return REPL_CMD_HANDLED;
    }

    /* Not a recognized command */
    return REPL_CMD_NOT_CMD;
}

/* ============================================================================
 * Link Callback Support
 * ============================================================================ */

/* State for REPL Link callbacks */
static SharedContext* g_repl_link_ctx = NULL;

/* Callback invoked when Link peers change */
static void repl_link_peers_callback(uint64_t num_peers, void* userdata) {
    (void)userdata;
    printf("[Link] Peers: %llu\n", (unsigned long long)num_peers);
}

/* Callback invoked when Link tempo changes */
static void repl_link_tempo_callback(double tempo, void* userdata) {
    (void)userdata;
    printf("[Link] Tempo: %.1f BPM\n", tempo);

    /* Sync tempo to SharedContext */
    if (g_repl_link_ctx) {
        g_repl_link_ctx->tempo = (int)(tempo + 0.5);
    }
}

/* Callback invoked when Link transport state changes */
static void repl_link_transport_callback(int is_playing, void* userdata) {
    (void)userdata;
    printf("[Link] Transport: %s\n", is_playing ? "playing" : "stopped");
}

void shared_repl_link_init_callbacks(SharedContext* ctx) {
    g_repl_link_ctx = ctx;

    /* Only register callbacks if Link is initialized */
    if (shared_link_is_initialized()) {
        shared_link_set_peers_callback(repl_link_peers_callback, NULL);
        shared_link_set_tempo_callback(repl_link_tempo_callback, NULL);
        shared_link_set_transport_callback(repl_link_transport_callback, NULL);
    }
}

void shared_repl_link_check(void) {
    /* Poll Link and invoke any pending callbacks */
    shared_link_check_callbacks();
}

void shared_repl_link_cleanup_callbacks(void) {
    /* Clear callbacks */
    if (shared_link_is_initialized()) {
        shared_link_set_peers_callback(NULL, NULL);
        shared_link_set_tempo_callback(NULL, NULL);
        shared_link_set_transport_callback(NULL, NULL);
    }
    g_repl_link_ctx = NULL;
}
