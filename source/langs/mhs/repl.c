/**
 * @file repl.c
 * @brief MHS (Micro Haskell MIDI) REPL and play mode entry points.
 *
 * Provides mhs_repl_main() and mhs_play_main() for psnd CLI dispatch.
 * These wrap the MicroHs main() with appropriate arguments for MIDI support.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#define PATH_MAX MAX_PATH
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#else
#include <unistd.h>
#include <libgen.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#ifdef __linux__
#include <linux/limits.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* Forward declaration of MicroHs main */
extern int mhs_main(int argc, char **argv);

/* Get the directory containing the executable */
static int get_exe_dir(char *buf, size_t size) {
#ifdef _WIN32
    DWORD len = GetModuleFileNameA(NULL, buf, (DWORD)size);
    if (len > 0 && len < size) {
        char *last_sep = strrchr(buf, '\\');
        if (!last_sep) last_sep = strrchr(buf, '/');
        if (last_sep) {
            *last_sep = '\0';
            return 0;
        }
    }
#elif defined(__APPLE__)
    uint32_t bufsize = (uint32_t)size;
    if (_NSGetExecutablePath(buf, &bufsize) == 0) {
        char *dir = dirname(buf);
        strncpy(buf, dir, size - 1);
        buf[size - 1] = '\0';
        return 0;
    }
#elif defined(__linux__)
    ssize_t len = readlink("/proc/self/exe", buf, size - 1);
    if (len > 0) {
        buf[len] = '\0';
        char *dir = dirname(buf);
        strncpy(buf, dir, size - 1);
        buf[size - 1] = '\0';
        return 0;
    }
#endif
    return -1;
}

/* Check if a file exists */
static int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/* Resolve a path to its absolute form */
static char *resolve_path(const char *path) {
#ifdef _WIN32
    char *resolved = malloc(PATH_MAX);
    if (resolved && _fullpath(resolved, path, PATH_MAX)) {
        return resolved;
    }
    free(resolved);
    return NULL;
#else
    return realpath(path, NULL);
#endif
}

/* Cross-platform setenv */
static int set_env(const char *name, const char *value) {
#ifdef _WIN32
    return _putenv_s(name, value);
#else
    return setenv(name, value, 1);
#endif
}

/* Try to find MHSDIR relative to executable */
static int find_mhsdir(char *buf, size_t size, const char *exe_dir) {
    const char *candidates[] = {
        /* Development: build/psnd -> source/thirdparty/MicroHs */
        "../source/thirdparty/MicroHs",
        /* Installed: bin/psnd -> share/mhs-midi/MicroHs */
        "../share/mhs-midi/MicroHs",
        "../share/psnd/MicroHs",
        /* Source tree relative paths (fallbacks) */
        "../../source/thirdparty/MicroHs",
        "../../../source/thirdparty/MicroHs",
        NULL
    };

    for (int i = 0; candidates[i]; i++) {
        snprintf(buf, size, "%s/%s", exe_dir, candidates[i]);

        /* Check if lib/Prelude.hs exists */
        char prelude_path[PATH_MAX];
        snprintf(prelude_path, sizeof(prelude_path), "%s/lib/Prelude.hs", buf);

        if (file_exists(prelude_path)) {
            char *resolved = resolve_path(buf);
            if (resolved) {
                strncpy(buf, resolved, size - 1);
                buf[size - 1] = '\0';
                free(resolved);
                return 0;
            }
        }
    }
    return -1;
}

/* Try to find MIDI lib directory relative to executable */
static int find_midi_lib(char *buf, size_t size, const char *exe_dir) {
    const char *candidates[] = {
        /* Development: build/psnd -> source/langs/mhs/lib */
        "../source/langs/mhs/lib",
        /* Alternative development paths */
        "../../source/langs/mhs/lib",
        "../../../source/langs/mhs/lib",
        /* Installed: bin/psnd -> share/mhs-midi/lib */
        "../share/mhs-midi/lib",
        "../share/psnd/mhs/lib",
        NULL
    };

    for (int i = 0; candidates[i]; i++) {
        snprintf(buf, size, "%s/%s", exe_dir, candidates[i]);

        /* Check if Midi.hs exists */
        char midi_path[PATH_MAX];
        snprintf(midi_path, sizeof(midi_path), "%s/Midi.hs", buf);

        if (file_exists(midi_path)) {
            char *resolved = resolve_path(buf);
            if (resolved) {
                strncpy(buf, resolved, size - 1);
                buf[size - 1] = '\0';
                free(resolved);
                return 0;
            }
        }
    }
    return -1;
}

/* Common setup for MHS environment */
static int mhs_setup_env(char *exe_dir, size_t exe_dir_size,
                         char *include_arg, size_t include_arg_size) {
    char mhsdir[PATH_MAX];
    char midi_lib[PATH_MAX];

    /* Get executable directory */
    if (get_exe_dir(exe_dir, exe_dir_size) != 0) {
        fprintf(stderr, "Warning: Could not determine executable directory\n");
        exe_dir[0] = '\0';
    }

    /* Set MHSDIR if not already set */
    if (!getenv("MHSDIR")) {
        if (exe_dir[0] && find_mhsdir(mhsdir, sizeof(mhsdir), exe_dir) == 0) {
            set_env("MHSDIR", mhsdir);
        } else {
            fprintf(stderr, "Error: Cannot find MicroHs directory.\n");
            fprintf(stderr, "Set MHSDIR environment variable or ensure proper installation.\n");
            return -1;
        }
    }

    /* Find MIDI library path */
    if (exe_dir[0] && find_midi_lib(midi_lib, sizeof(midi_lib), exe_dir) == 0) {
        snprintf(include_arg, include_arg_size, "-i%s", midi_lib);
        return 1;  /* Have MIDI lib */
    }

    include_arg[0] = '\0';
    return 0;  /* No MIDI lib, but continue */
}

/**
 * @brief MHS REPL entry point.
 *
 * Called when user runs: psnd mhs
 * Starts an interactive MicroHs REPL with MIDI library support.
 */
int mhs_repl_main(int argc, char **argv) {
    char exe_dir[PATH_MAX];
    char include_arg[PATH_MAX + 3];

    int have_midi_lib = mhs_setup_env(exe_dir, sizeof(exe_dir),
                                       include_arg, sizeof(include_arg));
    if (have_midi_lib < 0) {
        return 1;
    }

    /* Build argv for MHS REPL */
    /* Args: mhs -C [-i<midi_lib>] [user args...] */
    int extra_args = 1 + (have_midi_lib ? 1 : 0);  /* -C and possibly -i */
    int new_argc = argc + extra_args;
    char **new_argv = malloc((new_argc + 1) * sizeof(char *));

    if (!new_argv) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return 1;
    }

    int j = 0;
    new_argv[j++] = "mhs";
    new_argv[j++] = "-C";  /* Enable caching */

    if (have_midi_lib) {
        new_argv[j++] = include_arg;
    }

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
 */
int mhs_play_main(int argc, char **argv) {
    char exe_dir[PATH_MAX];
    char include_arg[PATH_MAX + 3];

    if (argc < 2) {
        fprintf(stderr, "Usage: psnd play <file.hs>\n");
        return 1;
    }

    int have_midi_lib = mhs_setup_env(exe_dir, sizeof(exe_dir),
                                       include_arg, sizeof(include_arg));
    if (have_midi_lib < 0) {
        return 1;
    }

    /* Build argv for MHS run */
    /* Args: mhs -C [-i<midi_lib>] -r <file> [other args...] */
    int extra_args = 2 + (have_midi_lib ? 1 : 0);  /* -C, -r, possibly -i */
    int new_argc = argc + extra_args;
    char **new_argv = malloc((new_argc + 1) * sizeof(char *));

    if (!new_argv) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return 1;
    }

    int j = 0;
    new_argv[j++] = "mhs";
    new_argv[j++] = "-C";  /* Enable caching */

    if (have_midi_lib) {
        new_argv[j++] = include_arg;
    }

    new_argv[j++] = "-r";  /* Run mode */

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
