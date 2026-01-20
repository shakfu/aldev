/**
 * @file repl.c
 * @brief MHS (Micro Haskell MIDI) REPL and play mode entry points.
 *
 * Provides mhs_repl_main() and mhs_play_main() for psnd CLI dispatch.
 * These wrap the MicroHs main() with appropriate arguments for MIDI support.
 *
 * psnd embeds MHS libraries using VFS (Virtual File System). The VFS
 * intercepts file operations and serves embedded content from memory,
 * making psnd mhs fully self-contained with ~2s startup time.
 *
 * For compilation to executable (-o without .c extension), files are
 * extracted to a temp directory since cc needs real filesystem access.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "vfs.h"

/* Forward declaration of MicroHs main */
extern int mhs_main(int argc, char **argv);

/**
 * @brief Print usage information for psnd mhs.
 */
static void print_mhs_usage(void) {
    printf("psnd mhs - Micro Haskell with MIDI support\n\n");
    printf("Usage:\n");
    printf("  psnd mhs                     Start interactive REPL\n");
    printf("  psnd mhs -r <file.hs>        Run a Haskell file\n");
    printf("  psnd mhs -o<prog> <file.hs>  Compile to executable\n");
    printf("  psnd mhs -o<file.c> <file.hs> Output C code only\n");
    printf("  psnd mhs [mhs-options]       Pass options to MicroHs\n");
    printf("  psnd mhs --help              Show this help\n");
    printf("\n");
    printf("Available MIDI modules: Midi, Music, MusicPerform, MidiPerform, Async\n");
    printf("\n");
    printf("Examples:\n");
    printf("  psnd mhs                     Start REPL\n");
    printf("  psnd mhs -r MyFile.hs        Run a Haskell file\n");
    printf("  psnd mhs -oMyProg MyFile.hs  Compile to executable\n");
    printf("  psnd mhs -oMyProg.c MyFile.hs Output C code only\n");
    printf("\n");
    printf("MicroHs options: -v (verbose), -q (quiet), -C (cache), -i<path> (include)\n");
}

/* Cross-platform setenv */
static int set_env(const char *name, const char *value) {
#ifdef _WIN32
    return _putenv_s(name, value);
#else
    return setenv(name, value, 1);
#endif
}

/**
 * @brief Check if we need to extract files for compilation.
 *
 * When compiling to an executable (not .c output), cc needs real files.
 *
 * @return 1 if extraction needed (executable output), 0 otherwise
 */
static int needs_extraction(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        /* Check for -oFILE or -o FILE */
        if (strncmp(argv[i], "-o", 2) == 0) {
            const char *output = NULL;
            if (argv[i][2] != '\0') {
                /* -oFILE form */
                output = argv[i] + 2;
            } else if (i + 1 < argc) {
                /* -o FILE form */
                output = argv[i + 1];
            }
            if (output) {
                size_t len = strlen(output);
                /* Check if output ends with .c */
                if (len >= 2 && strcmp(output + len - 2, ".c") == 0) {
                    return 0;  /* C output, VFS works fine */
                }
                return 1;  /* Executable output, needs extraction */
            }
        }
    }
    return 0;  /* No -o flag, VFS works fine */
}

/**
 * @brief MHS REPL entry point.
 *
 * Called when user runs: psnd mhs
 * Starts an interactive MicroHs REPL with MIDI library support.
 * Uses embedded VFS for fast startup (~2s vs ~17s from source).
 *
 * For compilation to executable, extracts embedded files to temp directory.
 */
int mhs_repl_main(int argc, char **argv) {
    char *temp_dir = NULL;
    int linking_midi = 0;

    /* Handle --help before anything else */
    if (argc >= 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        print_mhs_usage();
        return 0;
    }

    /* Initialize VFS with embedded libraries */
    if (vfs_init() != 0) {
        fprintf(stderr, "Error: Failed to initialize MHS Virtual File System\n");
        return 1;
    }

    /* Check if we're compiling to an executable (cc needs real files) */
    if (needs_extraction(argc, argv)) {
        temp_dir = vfs_extract_to_temp();
        if (!temp_dir) {
            fprintf(stderr, "Error: Failed to extract embedded files for compilation\n");
            return 1;
        }
        /* Set MHSDIR to temp directory for cc to find runtime files */
        set_env("MHSDIR", temp_dir);
        linking_midi = 1;
    } else {
        /* Use VFS - set MHSDIR to virtual root */
        set_env("MHSDIR", VFS_VIRTUAL_ROOT);
    }

    /* Build argv for MHS */
    /* Base args: mhs -C -a<path> -pbase -pmusic */
    /* When linking: add -optl flags for MIDI libraries and frameworks */
#ifdef __APPLE__
    /* macOS: 3 libraries + 3 frameworks + C++ runtime = 7 -optl pairs = 14 args */
    #define LINK_EXTRA_ARGS 14
#else
    /* Linux: --no-as-needed + 3 libraries + ALSA + C++ runtime + math = 7 -optl pairs = 14 args */
    #define LINK_EXTRA_ARGS 14
#endif

    int extra_args = 4;  /* -C, -a<path>, -pbase, -pmusic */
    if (linking_midi) {
        extra_args += LINK_EXTRA_ARGS;
    }

    int new_argc = argc + extra_args;
    char **new_argv = malloc((new_argc + 1) * sizeof(char *));

    /* Buffers for library path arguments */
    char archive_path[512];
    char lib_libremidi[512];
    char lib_midi_ffi[512];
    char lib_music_theory[512];

    if (!new_argv) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        if (temp_dir) vfs_cleanup_temp(temp_dir);
        return 1;
    }

    int j = 0;
    new_argv[j++] = "mhs";
    new_argv[j++] = "-C";  /* Enable caching */

    /* Set archive path based on mode */
    if (temp_dir) {
        snprintf(archive_path, sizeof(archive_path), "-a%s", temp_dir);
    } else {
        snprintf(archive_path, sizeof(archive_path), "-a%s", VFS_VIRTUAL_ROOT);
    }
    new_argv[j++] = archive_path;
    new_argv[j++] = "-pbase";   /* Preload base package */
    new_argv[j++] = "-pmusic";  /* Preload music package */

    /* Add linker flags for MIDI libraries if compiling to executable */
    if (linking_midi) {
        /* Build library paths from temp directory */
        snprintf(lib_midi_ffi, sizeof(lib_midi_ffi), "%s/lib/libmidi_ffi.a", temp_dir);
        snprintf(lib_music_theory, sizeof(lib_music_theory), "%s/lib/libmusic_theory.a", temp_dir);
        snprintf(lib_libremidi, sizeof(lib_libremidi), "%s/lib/liblibremidi.a", temp_dir);

        /* Add -optl flags for each library */
        new_argv[j++] = "-optl";
        new_argv[j++] = lib_midi_ffi;
        new_argv[j++] = "-optl";
        new_argv[j++] = lib_music_theory;
        new_argv[j++] = "-optl";
        new_argv[j++] = lib_libremidi;

#ifdef __APPLE__
        /* macOS frameworks */
        new_argv[j++] = "-optl";
        new_argv[j++] = "-framework";
        new_argv[j++] = "-optl";
        new_argv[j++] = "CoreMIDI";
        new_argv[j++] = "-optl";
        new_argv[j++] = "-framework";
        new_argv[j++] = "-optl";
        new_argv[j++] = "CoreFoundation";
        new_argv[j++] = "-optl";
        new_argv[j++] = "-framework";
        new_argv[j++] = "-optl";
        new_argv[j++] = "CoreAudio";
        /* C++ standard library (libremidi is C++) */
        new_argv[j++] = "-optl";
        new_argv[j++] = "-lc++";
#else
        /* Linux: Force linker to include libraries */
        new_argv[j++] = "-optl";
        new_argv[j++] = "-Wl,--no-as-needed";
        /* Linux: ALSA */
        new_argv[j++] = "-optl";
        new_argv[j++] = "-lasound";
        /* C++ standard library */
        new_argv[j++] = "-optl";
        new_argv[j++] = "-lstdc++";
        /* Math library */
        new_argv[j++] = "-optl";
        new_argv[j++] = "-lm";
#endif
    }

    /* Copy user arguments (skip program name) */
    for (int i = 1; i < argc; i++) {
        new_argv[j++] = argv[i];
    }
    new_argv[j] = NULL;

    /* Update argc to match */
    new_argc = j;

    /* Run MHS */
    int result = mhs_main(new_argc, new_argv);

    free(new_argv);

    /* Clean up temp directory if we extracted */
    if (temp_dir) {
        vfs_cleanup_temp(temp_dir);
    }

    return result;
}

/**
 * @brief MHS play entry point.
 *
 * Called when user runs: psnd play file.hs
 * Runs the specified Haskell file.
 * Uses embedded VFS for fast startup.
 */
int mhs_play_main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: psnd play <file.hs>\n");
        return 1;
    }

    /* Initialize VFS */
    if (vfs_init() != 0) {
        fprintf(stderr, "Error: Failed to initialize MHS Virtual File System\n");
        return 1;
    }

    /* Set MHSDIR to VFS virtual root */
    set_env("MHSDIR", VFS_VIRTUAL_ROOT);

    /* Build argv for MHS run */
    /* Args: mhs -C -a/mhs-embedded -pbase -pmusic -r <file> [other args...] */
    int extra_args = 5;  /* -C, -a/mhs-embedded, -pbase, -pmusic, -r */
    int new_argc = argc + extra_args;
    char **new_argv = malloc((new_argc + 1) * sizeof(char *));

    if (!new_argv) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return 1;
    }

    int j = 0;
    new_argv[j++] = "mhs";
    new_argv[j++] = "-C";               /* Enable caching */
    new_argv[j++] = "-a/mhs-embedded";  /* Package archive path (VFS virtual root) */
    new_argv[j++] = "-pbase";           /* Preload base package */
    new_argv[j++] = "-pmusic";          /* Preload music package */
    new_argv[j++] = "-r";               /* Run mode */

    /* Copy file path and remaining arguments */
    for (int i = 1; i < argc; i++) {
        new_argv[j++] = argv[i];
    }
    new_argv[j] = NULL;

    /* Run MHS */
    int result = mhs_main(new_argc, new_argv);

    free(new_argv);
    return result;
}
