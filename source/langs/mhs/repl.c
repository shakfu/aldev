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

/* Cross-platform setenv */
static int set_env(const char *name, const char *value) {
#ifdef _WIN32
    return _putenv_s(name, value);
#else
    return setenv(name, value, 1);
#endif
}

/**
 * @brief Initialize VFS and set up MHS environment.
 *
 * Initializes the embedded Virtual File System and sets MHSDIR to point
 * to the VFS virtual root. All MicroHs library lookups will be served
 * from embedded content.
 *
 * @return 0 on success, -1 on failure
 */
static int mhs_vfs_setup(void) {
    /* Initialize VFS - decompresses and caches embedded content */
    if (vfs_init() != 0) {
        fprintf(stderr, "Error: Failed to initialize MHS Virtual File System\n");
        return -1;
    }

    /* Set MHSDIR to VFS virtual root */
    /* The VFS intercepts file operations for paths starting with this prefix */
    set_env("MHSDIR", VFS_VIRTUAL_ROOT);

    return 0;
}

/**
 * @brief MHS REPL entry point.
 *
 * Called when user runs: psnd mhs
 * Starts an interactive MicroHs REPL with MIDI library support.
 * Uses embedded VFS for fast startup (~2s vs ~17s from source).
 */
int mhs_repl_main(int argc, char **argv) {
    /* Initialize VFS */
    if (mhs_vfs_setup() != 0) {
        return 1;
    }

    /* Build argv for MHS REPL */
    /* Args: mhs -C -a/mhs-embedded -pbase -pmusic [user args...] */
    /* -C enables caching, -a sets archive path, -p preloads packages */
    int extra_args = 4;  /* -C, -a/mhs-embedded, -pbase, -pmusic */
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

    /* Copy user arguments (skip program name) */
    for (int i = 1; i < argc; i++) {
        new_argv[j++] = argv[i];
    }
    new_argv[j] = NULL;

    /* Run MHS */
    int result = mhs_main(new_argc, new_argv);

    free(new_argv);
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
    if (mhs_vfs_setup() != 0) {
        return 1;
    }

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
