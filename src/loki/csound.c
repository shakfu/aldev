/* csound.c - Editor-level Csound backend control
 *
 * Language-agnostic Csound control using the shared backend directly.
 * This removes the dependency on Alda or Joy for Csound operations.
 */

#include "csound.h"
#include "shared/audio/audio.h"

#include <stdio.h>

int loki_csound_is_available(void) {
    return shared_csound_is_available();
}

int loki_csound_load(const char *path) {
    if (!path || !*path) {
        return -1;
    }

    /* Initialize Csound backend if not already */
    if (shared_csound_init() != 0) {
        return -1;
    }

    return shared_csound_load(path);
}

int loki_csound_enable(void) {
    if (!shared_csound_has_instruments()) {
        return -1;
    }

    /* Disable TSF when enabling Csound (mutually exclusive) */
    if (shared_tsf_is_enabled()) {
        shared_tsf_disable();
    }

    return shared_csound_enable();
}

void loki_csound_disable(void) {
    shared_csound_disable();
}

int loki_csound_is_enabled(void) {
    return shared_csound_is_enabled();
}

int loki_csound_has_instruments(void) {
    return shared_csound_has_instruments();
}

int loki_csound_play_async(const char *path) {
    if (!path || !*path) {
        fprintf(stderr, "loki_csound: Invalid CSD path\n");
        return -1;
    }

    int result = shared_csound_play_file_async(path);
    if (result != 0) {
        const char *err = shared_csound_get_error();
        fprintf(stderr, "loki_csound: %s\n", err ? err : "Failed to start playback");
        return -1;
    }

    return 0;
}

int loki_csound_playback_active(void) {
    return shared_csound_playback_active();
}

void loki_csound_stop_playback(void) {
    shared_csound_stop_playback();
}

const char *loki_csound_get_error(void) {
    return shared_csound_get_error();
}
