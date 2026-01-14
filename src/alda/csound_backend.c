/**
 * @file csound_backend.c
 * @brief Csound synthesis backend for Alda.
 *
 * Implements the csound_backend.h interface using the Csound API.
 * Audio is rendered via host-implemented I/O for integration with miniaudio.
 * MIDI events are translated to Csound score events.
 */

#ifdef BUILD_CSOUND_BACKEND

#include "alda/csound_backend.h"
#include "csound.h"
#include "miniaudio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#ifndef _WIN32
#include <unistd.h>
#endif

/* Cross-platform mutex */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef CRITICAL_SECTION cs_mutex_t;
static inline int cs_mutex_init(cs_mutex_t* m) { InitializeCriticalSection(m); return 0; }
static inline void cs_mutex_destroy(cs_mutex_t* m) { DeleteCriticalSection(m); }
static inline void cs_mutex_lock(cs_mutex_t* m) { EnterCriticalSection(m); }
static inline void cs_mutex_unlock(cs_mutex_t* m) { LeaveCriticalSection(m); }
#else
#include <pthread.h>
typedef pthread_mutex_t cs_mutex_t;
static inline int cs_mutex_init(cs_mutex_t* m) { return pthread_mutex_init(m, NULL); }
static inline void cs_mutex_destroy(cs_mutex_t* m) { pthread_mutex_destroy(m); }
static inline void cs_mutex_lock(cs_mutex_t* m) { pthread_mutex_lock(m); }
static inline void cs_mutex_unlock(cs_mutex_t* m) { pthread_mutex_unlock(m); }
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define CS_SAMPLE_RATE      44100
#define CS_CHANNELS         2
#define CS_KSMPS            64      /* Control rate samples */
#define CS_PERIOD_FRAMES    1024    /* Audio callback period size */
#define CS_MAX_NOTES        256     /* Max simultaneous notes */
#define CS_ERROR_BUF_SIZE   512

/* ============================================================================
 * Active Note Tracking
 *
 * Csound uses fractional instrument numbers to identify individual instances.
 * We track active notes to properly turn them off.
 * Format: instrument.CCPPP where CC=channel(01-16), PPP=pitch(000-127)
 * ============================================================================ */

typedef struct {
    int channel;    /* 1-16 */
    int pitch;      /* 0-127 */
    double instr;   /* Csound instrument number (e.g., 1.01060) */
} ActiveNote;

/* ============================================================================
 * Backend State (global singleton)
 * ============================================================================ */

typedef struct {
    CSOUND* csound;
    int initialized;
    int instruments_loaded;
    int enabled;
    int started;

    /* Active notes for proper note-off handling */
    ActiveNote active_notes[CS_MAX_NOTES];
    int note_count;

    /* Per-channel program (0-127) */
    int programs[16];

    /* Error handling */
    char error_msg[CS_ERROR_BUF_SIZE];

    /* Thread safety */
    cs_mutex_t mutex;

    /* Audio buffer for rendering (Csound's output buffer) */
    MYFLT* spin;        /* Input buffer (unused, we're synthesis-only) */
    MYFLT* spout;       /* Output buffer from Csound */
    int spout_samples;  /* Samples in spout per ksmps cycle */
    int spout_pos;      /* Current position in spout buffer */
    MYFLT zerodBFS;     /* 0dBFS scaling factor for normalization */

    /* Own miniaudio device for audio output */
    ma_device device;
    int device_initialized;
} CsoundBackend;

static CsoundBackend g_cs = {0};

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static void set_error(const char* msg) {
    if (msg) {
        strncpy(g_cs.error_msg, msg, CS_ERROR_BUF_SIZE - 1);
        g_cs.error_msg[CS_ERROR_BUF_SIZE - 1] = '\0';
    } else {
        g_cs.error_msg[0] = '\0';
    }
}

/**
 * @brief Generate fractional instrument number for note tracking.
 *
 * Format: base.CCPPP where:
 *   - base: instrument number (1-16 for channels)
 *   - CC: channel (01-16)
 *   - PPP: pitch (000-127)
 */
static double make_instr_num(int base_instr, int channel, int pitch) {
    /* Fractional part: 0.CCPPP */
    double frac = (channel * 1000 + pitch) / 100000.0;
    return (double)base_instr + frac;
}

static int find_active_note(int channel, int pitch) {
    for (int i = 0; i < g_cs.note_count; i++) {
        if (g_cs.active_notes[i].channel == channel &&
            g_cs.active_notes[i].pitch == pitch) {
            return i;
        }
    }
    return -1;
}

static void add_active_note(int channel, int pitch, double instr) {
    if (g_cs.note_count >= CS_MAX_NOTES) {
        /* Remove oldest note to make room */
        memmove(&g_cs.active_notes[0], &g_cs.active_notes[1],
                (CS_MAX_NOTES - 1) * sizeof(ActiveNote));
        g_cs.note_count--;
    }
    g_cs.active_notes[g_cs.note_count].channel = channel;
    g_cs.active_notes[g_cs.note_count].pitch = pitch;
    g_cs.active_notes[g_cs.note_count].instr = instr;
    g_cs.note_count++;
}

static void remove_active_note(int index) {
    if (index < 0 || index >= g_cs.note_count) return;
    memmove(&g_cs.active_notes[index], &g_cs.active_notes[index + 1],
            (g_cs.note_count - index - 1) * sizeof(ActiveNote));
    g_cs.note_count--;
}

/* ============================================================================
 * Initialization and Cleanup
 * ============================================================================ */

int alda_csound_init(void) {
    if (g_cs.initialized) {
        return 0;  /* Already initialized */
    }

    memset(&g_cs, 0, sizeof(g_cs));

    if (cs_mutex_init(&g_cs.mutex) != 0) {
        set_error("Failed to create mutex");
        return -1;
    }

    /* Create Csound instance */
    g_cs.csound = csoundCreate(NULL);
    if (!g_cs.csound) {
        set_error("Failed to create Csound instance");
        cs_mutex_destroy(&g_cs.mutex);
        return -1;
    }

    /* Configure for host-implemented audio I/O */
    csoundSetHostImplementedAudioIO(g_cs.csound, 1, 0);

    /* Capture messages in buffer instead of printing to stdout/stderr */
    csoundCreateMessageBuffer(g_cs.csound, 0);

    /* Set options before compilation */
    csoundSetOption(g_cs.csound, "-n");           /* No sound output (we handle it) */
    csoundSetOption(g_cs.csound, "-d");           /* Suppress displays */
    csoundSetOption(g_cs.csound, "-m0");          /* Suppress messages */
    csoundSetOption(g_cs.csound, "--daemon");     /* Daemon mode: ignore score section */

    /* Initialize default programs */
    for (int i = 0; i < 16; i++) {
        g_cs.programs[i] = 0;
    }

    g_cs.initialized = 1;
    return 0;
}

void alda_csound_cleanup(void) {
    if (!g_cs.initialized) {
        return;
    }

    alda_csound_disable();

    /* Uninitialize audio device */
    if (g_cs.device_initialized) {
        ma_device_uninit(&g_cs.device);
        g_cs.device_initialized = 0;
    }

    cs_mutex_lock(&g_cs.mutex);

    if (g_cs.csound) {
        if (g_cs.started) {
            csoundStop(g_cs.csound);
            csoundCleanup(g_cs.csound);
        }
        csoundDestroyMessageBuffer(g_cs.csound);
        csoundDestroy(g_cs.csound);
        g_cs.csound = NULL;
    }

    cs_mutex_unlock(&g_cs.mutex);
    cs_mutex_destroy(&g_cs.mutex);

    g_cs.initialized = 0;
    g_cs.instruments_loaded = 0;
    g_cs.enabled = 0;
    g_cs.started = 0;
}

/* ============================================================================
 * Instrument Loading
 * ============================================================================ */

int alda_csound_load_csd(const char* path) {
    if (!g_cs.initialized) {
        set_error("Backend not initialized");
        return -1;
    }

    if (!path) {
        set_error("NULL path");
        return -1;
    }

    cs_mutex_lock(&g_cs.mutex);

    /* Reset if already started */
    if (g_cs.started) {
        csoundStop(g_cs.csound);
        csoundCleanup(g_cs.csound);
        csoundDestroyMessageBuffer(g_cs.csound);
        csoundReset(g_cs.csound);
        csoundSetHostImplementedAudioIO(g_cs.csound, 1, 0);
        csoundCreateMessageBuffer(g_cs.csound, 0);
        csoundSetOption(g_cs.csound, "-n");
        csoundSetOption(g_cs.csound, "-d");
        csoundSetOption(g_cs.csound, "-m0");
        csoundSetOption(g_cs.csound, "--daemon");
        g_cs.started = 0;
        g_cs.enabled = 0;
    }

    /* Compile CSD file */
    int result = csoundCompileCsd(g_cs.csound, path);
    if (result != 0) {
        cs_mutex_unlock(&g_cs.mutex);
        set_error("Failed to compile CSD file");
        return -1;
    }

    /* Start Csound */
    result = csoundStart(g_cs.csound);
    if (result != 0) {
        cs_mutex_unlock(&g_cs.mutex);
        set_error("Failed to start Csound");
        return -1;
    }

    /* Send infinite duration event (daemon mode ignores score section) */
    csoundReadScore(g_cs.csound, "f0 86400\n");

    /* Get audio buffer pointers */
    g_cs.spout = csoundGetSpout(g_cs.csound);
    g_cs.spin = csoundGetSpin(g_cs.csound);
    g_cs.spout_samples = csoundGetKsmps(g_cs.csound) * csoundGetNchnls(g_cs.csound);
    g_cs.spout_pos = g_cs.spout_samples;  /* Force first ksmps cycle */
    g_cs.zerodBFS = csoundGet0dBFS(g_cs.csound);
    g_cs.started = 1;

    g_cs.instruments_loaded = 1;

    cs_mutex_unlock(&g_cs.mutex);
    return 0;
}

int alda_csound_compile_orc(const char* orc) {
    if (!g_cs.initialized) {
        set_error("Backend not initialized");
        return -1;
    }

    if (!orc) {
        set_error("NULL orchestra code");
        return -1;
    }

    cs_mutex_lock(&g_cs.mutex);

    int result = csoundCompileOrc(g_cs.csound, orc);
    if (result != 0) {
        cs_mutex_unlock(&g_cs.mutex);
        set_error("Failed to compile orchestra code");
        return -1;
    }

    g_cs.instruments_loaded = 1;

    cs_mutex_unlock(&g_cs.mutex);
    return 0;
}

int alda_csound_has_instruments(void) {
    return g_cs.initialized && g_cs.instruments_loaded;
}

/* ============================================================================
 * Audio Callback
 *
 * Csound has its own miniaudio device for audio output.
 * ============================================================================ */

/* Forward declaration */
void alda_csound_render(float* output, int frames);

static void csound_audio_callback(ma_device* device, void* output, const void* input, ma_uint32 frame_count) {
    (void)device;
    (void)input;

    float* out = (float*)output;

    if (g_cs.enabled && g_cs.started) {
        alda_csound_render(out, (int)frame_count);
    } else {
        memset(out, 0, (size_t)frame_count * CS_CHANNELS * sizeof(float));
    }
}

/* ============================================================================
 * Enable/Disable
 *
 * Csound uses its own miniaudio device for audio output.
 * ============================================================================ */

int alda_csound_enable(void) {
    if (!g_cs.initialized) {
        set_error("Backend not initialized");
        return -1;
    }

    if (!g_cs.instruments_loaded) {
        set_error("No instruments loaded");
        return -1;
    }

    if (g_cs.enabled) {
        return 0;  /* Already enabled */
    }

    cs_mutex_lock(&g_cs.mutex);

    /* Start Csound if not already started */
    if (!g_cs.started) {
        int result = csoundStart(g_cs.csound);
        if (result != 0) {
            cs_mutex_unlock(&g_cs.mutex);
            set_error("Failed to start Csound");
            return -1;
        }

        /* Get audio buffer pointers */
        g_cs.spout = csoundGetSpout(g_cs.csound);
        g_cs.spin = csoundGetSpin(g_cs.csound);
        g_cs.spout_samples = csoundGetKsmps(g_cs.csound) * csoundGetNchnls(g_cs.csound);
        g_cs.spout_pos = g_cs.spout_samples;  /* Force first ksmps cycle */
        g_cs.zerodBFS = csoundGet0dBFS(g_cs.csound);
        g_cs.started = 1;
    }

    cs_mutex_unlock(&g_cs.mutex);

    /* Initialize our own audio device if needed */
    if (!g_cs.device_initialized) {
        ma_device_config config = ma_device_config_init(ma_device_type_playback);
        config.playback.format = ma_format_f32;
        config.playback.channels = CS_CHANNELS;
        config.sampleRate = CS_SAMPLE_RATE;
        config.dataCallback = csound_audio_callback;
        config.pUserData = NULL;
        config.periodSizeInFrames = CS_PERIOD_FRAMES;

        ma_result result = ma_device_init(NULL, &config, &g_cs.device);
        if (result != MA_SUCCESS) {
            set_error("Failed to initialize Csound audio device");
            return -1;
        }

        g_cs.device_initialized = 1;
    }

    /* Start audio playback */
    ma_result result = ma_device_start(&g_cs.device);
    if (result != MA_SUCCESS) {
        set_error("Failed to start Csound audio device");
        return -1;
    }

    g_cs.enabled = 1;
    return 0;
}

void alda_csound_disable(void) {
    if (!g_cs.initialized || !g_cs.enabled) {
        return;
    }

    /* Stop all notes */
    alda_csound_all_notes_off();

    /* Stop audio device */
    if (g_cs.device_initialized) {
        ma_device_stop(&g_cs.device);
    }

    g_cs.enabled = 0;
}

int alda_csound_is_enabled(void) {
    return g_cs.initialized && g_cs.enabled;
}

/* ============================================================================
 * MIDI Message Sending
 * ============================================================================ */

void alda_csound_send_note_on(int channel, int pitch, int velocity) {
    if (!g_cs.csound || !g_cs.enabled) {
        return;
    }

    if (channel < 1 || channel > 16) return;
    if (pitch < 0 || pitch > 127) return;
    if (velocity < 0 || velocity > 127) return;

    /* Velocity 0 is note off */
    if (velocity == 0) {
        alda_csound_send_note_off(channel, pitch);
        return;
    }

    cs_mutex_lock(&g_cs.mutex);

    /* Turn off existing note at this pitch if any */
    int existing = find_active_note(channel, pitch);
    if (existing >= 0) {
        char msg[64];
        snprintf(msg, sizeof(msg), "i -%f 0 0", g_cs.active_notes[existing].instr);
        csoundInputMessage(g_cs.csound, msg);
        remove_active_note(existing);
    }

    /* Instrument number based on channel (1-16 -> instruments 1-16) */
    /* Could be extended to use program change to select different instruments */
    int base_instr = channel;

    /* Create unique fractional ID for this note */
    double instr_num = make_instr_num(base_instr, channel, pitch);

    /* Send note on: i <instr> <start> <dur> <pitch> <velocity>
     * dur=-1 means held until explicit note off */
    char msg[128];
    snprintf(msg, sizeof(msg), "i %f 0 -1 %d %d", instr_num, pitch, velocity);
    csoundInputMessage(g_cs.csound, msg);

    /* Track the note */
    add_active_note(channel, pitch, instr_num);

    cs_mutex_unlock(&g_cs.mutex);
}

void alda_csound_send_note_off(int channel, int pitch) {
    if (!g_cs.csound || !g_cs.enabled) {
        return;
    }

    if (channel < 1 || channel > 16) return;
    if (pitch < 0 || pitch > 127) return;

    cs_mutex_lock(&g_cs.mutex);

    int idx = find_active_note(channel, pitch);
    if (idx >= 0) {
        char msg[64];
        /* Negative instrument number turns off the specific instance */
        snprintf(msg, sizeof(msg), "i -%f 0 0", g_cs.active_notes[idx].instr);
        csoundInputMessage(g_cs.csound, msg);
        remove_active_note(idx);
    }

    cs_mutex_unlock(&g_cs.mutex);
}

void alda_csound_send_program(int channel, int program) {
    if (!g_cs.initialized) {
        return;
    }

    if (channel < 1 || channel > 16) return;
    if (program < 0 || program > 127) return;

    cs_mutex_lock(&g_cs.mutex);
    g_cs.programs[channel - 1] = program;
    cs_mutex_unlock(&g_cs.mutex);

    /* Program changes could be implemented by:
     * 1. Different instrument numbers per program
     * 2. Sending control channel values
     * 3. Using Csound's massign
     * For now, we just track it for future use */
}

void alda_csound_send_cc(int channel, int cc, int value) {
    if (!g_cs.csound || !g_cs.enabled) {
        return;
    }

    if (channel < 1 || channel > 16) return;
    if (cc < 0 || cc > 127) return;
    if (value < 0 || value > 127) return;

    cs_mutex_lock(&g_cs.mutex);

    /* Set a control channel that instruments can read via chnget
     * Channel name format: "cc<CC>_ch<CHANNEL>" e.g., "cc1_ch1" */
    char chn_name[32];
    snprintf(chn_name, sizeof(chn_name), "cc%d_ch%d", cc, channel);

    /* Normalize to 0.0-1.0 */
    MYFLT norm_value = (MYFLT)value / 127.0;
    csoundSetControlChannel(g_cs.csound, chn_name, norm_value);

    cs_mutex_unlock(&g_cs.mutex);
}

void alda_csound_send_pitch_bend(int channel, int bend) {
    if (!g_cs.csound || !g_cs.enabled) {
        return;
    }

    if (channel < 1 || channel > 16) return;

    cs_mutex_lock(&g_cs.mutex);

    /* Pitch bend control channel
     * bend is -8192 to 8191, normalize to -1.0 to 1.0 */
    char chn_name[32];
    snprintf(chn_name, sizeof(chn_name), "bend_ch%d", channel);

    MYFLT norm_bend = (MYFLT)bend / 8192.0;
    csoundSetControlChannel(g_cs.csound, chn_name, norm_bend);

    cs_mutex_unlock(&g_cs.mutex);
}

void alda_csound_all_notes_off(void) {
    if (!g_cs.csound) {
        return;
    }

    cs_mutex_lock(&g_cs.mutex);

    /* Turn off all active notes */
    for (int i = 0; i < g_cs.note_count; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "i -%f 0 0", g_cs.active_notes[i].instr);
        csoundInputMessage(g_cs.csound, msg);
    }
    g_cs.note_count = 0;

    cs_mutex_unlock(&g_cs.mutex);
}

/* ============================================================================
 * Audio Rendering
 * ============================================================================ */

int alda_csound_get_sample_rate(void) {
    if (!g_cs.csound || !g_cs.started) {
        return 0;
    }
    return (int)csoundGetSr(g_cs.csound);
}

int alda_csound_get_channels(void) {
    if (!g_cs.csound || !g_cs.started) {
        return 0;
    }
    return csoundGetNchnls(g_cs.csound);
}

void alda_csound_render(float* output, int frames) {
    if (!output || frames <= 0) {
        return;
    }

    if (!g_cs.csound || !g_cs.enabled || !g_cs.started) {
        /* Fill with silence */
        memset(output, 0, frames * CS_CHANNELS * sizeof(float));
        return;
    }

    cs_mutex_lock(&g_cs.mutex);

    int nchnls = csoundGetNchnls(g_cs.csound);
    int ksmps = csoundGetKsmps(g_cs.csound);
    int out_idx = 0;
    int frames_remaining = frames;

    while (frames_remaining > 0) {
        /* Check if we need to run another ksmps cycle */
        if (g_cs.spout_pos >= ksmps) {
            int result = csoundPerformKsmps(g_cs.csound);
            if (result != 0) {
                /* Performance ended or error, fill rest with silence */
                memset(&output[out_idx * CS_CHANNELS], 0,
                       frames_remaining * CS_CHANNELS * sizeof(float));
                break;
            }
            g_cs.spout = csoundGetSpout(g_cs.csound);
            g_cs.spout_pos = 0;
        }

        /* Copy available samples from spout to output, normalized by 0dBFS */
        int available = ksmps - g_cs.spout_pos;
        int to_copy = (frames_remaining < available) ? frames_remaining : available;
        MYFLT scale = g_cs.zerodBFS;

        for (int i = 0; i < to_copy; i++) {
            int spout_frame = g_cs.spout_pos + i;
            /* Csound output is already interleaved for nchnls */
            for (int ch = 0; ch < CS_CHANNELS && ch < nchnls; ch++) {
                output[(out_idx + i) * CS_CHANNELS + ch] =
                    (float)(g_cs.spout[spout_frame * nchnls + ch] / scale);
            }
            /* If Csound has fewer channels than output, fill with zero */
            for (int ch = nchnls; ch < CS_CHANNELS; ch++) {
                output[(out_idx + i) * CS_CHANNELS + ch] = 0.0f;
            }
        }

        g_cs.spout_pos += to_copy;
        out_idx += to_copy;
        frames_remaining -= to_copy;
    }

    cs_mutex_unlock(&g_cs.mutex);
}

/* ============================================================================
 * Error Handling
 * ============================================================================ */

const char* alda_csound_get_error(void) {
    if (g_cs.error_msg[0] == '\0') {
        return NULL;
    }
    return g_cs.error_msg;
}

/* ============================================================================
 * Standalone Playback
 *
 * This provides a separate code path for playing CSD files with their own
 * score sections, as opposed to the daemon mode used for MIDI-driven synthesis.
 * ============================================================================ */

/* State for standalone playback (separate from g_cs) */
typedef struct {
    CSOUND* csound;
    int started;
    int finished;
    int active;             /* Non-zero if async playback is running */
    cs_mutex_t mutex;
    MYFLT* spout;
    int spout_pos;
    int ksmps;
    int nchnls;
    MYFLT zerodBFS;         /* 0dBFS scaling factor for normalization */
    ma_device device;       /* Audio device for async playback */
    int device_initialized;
} PlaybackState;

static PlaybackState g_play = {0};

static void play_audio_callback(ma_device* device, void* output, const void* input, ma_uint32 frame_count) {
    (void)device;
    (void)input;

    float* out = (float*)output;

    if (!g_play.csound || g_play.finished || !g_play.active) {
        memset(out, 0, (size_t)frame_count * CS_CHANNELS * sizeof(float));
        return;
    }

    cs_mutex_lock(&g_play.mutex);

    int out_idx = 0;
    int frames_remaining = (int)frame_count;

    while (frames_remaining > 0 && !g_play.finished) {
        /* Check if we need to run another ksmps cycle */
        if (g_play.spout_pos >= g_play.ksmps) {
            int result = csoundPerformKsmps(g_play.csound);
            if (result != 0) {
                /* Performance ended */
                g_play.finished = 1;
                memset(&out[out_idx * CS_CHANNELS], 0,
                       (size_t)frames_remaining * CS_CHANNELS * sizeof(float));
                break;
            }
            g_play.spout = csoundGetSpout(g_play.csound);
            g_play.spout_pos = 0;
        }

        /* Copy available samples from spout to output, normalized by 0dBFS */
        int available = g_play.ksmps - g_play.spout_pos;
        int to_copy = (frames_remaining < available) ? frames_remaining : available;
        MYFLT scale = g_play.zerodBFS;

        for (int i = 0; i < to_copy; i++) {
            int spout_frame = g_play.spout_pos + i;
            for (int ch = 0; ch < CS_CHANNELS && ch < g_play.nchnls; ch++) {
                out[(out_idx + i) * CS_CHANNELS + ch] =
                    (float)(g_play.spout[spout_frame * g_play.nchnls + ch] / scale);
            }
            for (int ch = g_play.nchnls; ch < CS_CHANNELS; ch++) {
                out[(out_idx + i) * CS_CHANNELS + ch] = 0.0f;
            }
        }

        g_play.spout_pos += to_copy;
        out_idx += to_copy;
        frames_remaining -= to_copy;
    }

    cs_mutex_unlock(&g_play.mutex);
}

/* Internal: cleanup playback resources */
static void playback_cleanup(void) {
    if (g_play.device_initialized) {
        ma_device_stop(&g_play.device);
        ma_device_uninit(&g_play.device);
        g_play.device_initialized = 0;
    }

    if (g_play.csound) {
        if (g_play.started) {
            csoundStop(g_play.csound);
            csoundCleanup(g_play.csound);
        }
        csoundDestroyMessageBuffer(g_play.csound);
        csoundDestroy(g_play.csound);
        g_play.csound = NULL;
    }

    if (g_play.started) {
        cs_mutex_destroy(&g_play.mutex);
    }

    memset(&g_play, 0, sizeof(g_play));
}

/* Internal: start playback of a CSD file */
static int playback_start(const char* path, int verbose) {
    if (!path) {
        set_error("NULL path");
        return -1;
    }

    /* Stop any existing playback first */
    if (g_play.active) {
        playback_cleanup();
    }

    memset(&g_play, 0, sizeof(g_play));

    if (cs_mutex_init(&g_play.mutex) != 0) {
        set_error("Failed to create mutex");
        return -1;
    }

    /* Create Csound instance */
    g_play.csound = csoundCreate(NULL);
    if (!g_play.csound) {
        set_error("Failed to create Csound instance");
        cs_mutex_destroy(&g_play.mutex);
        return -1;
    }

    /* Configure for host-implemented audio I/O */
    csoundSetHostImplementedAudioIO(g_play.csound, 1, 0);

    /* Capture messages in buffer */
    csoundCreateMessageBuffer(g_play.csound, 0);

    /* Set options - NOT daemon mode, let score run naturally */
    csoundSetOption(g_play.csound, "-n");           /* No sound output (we handle it) */
    if (!verbose) {
        csoundSetOption(g_play.csound, "-d");       /* Suppress displays */
        csoundSetOption(g_play.csound, "-m0");      /* Suppress messages */
    }

    /* Compile CSD file */
    if (verbose) {
        printf("Compiling: %s\n", path);
    }
    int result = csoundCompileCsd(g_play.csound, path);
    if (result != 0) {
        set_error("Failed to compile CSD file");
        csoundDestroyMessageBuffer(g_play.csound);
        csoundDestroy(g_play.csound);
        g_play.csound = NULL;
        cs_mutex_destroy(&g_play.mutex);
        return -1;
    }

    /* Start Csound */
    result = csoundStart(g_play.csound);
    if (result != 0) {
        set_error("Failed to start Csound");
        csoundDestroyMessageBuffer(g_play.csound);
        csoundDestroy(g_play.csound);
        g_play.csound = NULL;
        cs_mutex_destroy(&g_play.mutex);
        return -1;
    }

    /* Get audio buffer info */
    g_play.spout = csoundGetSpout(g_play.csound);
    g_play.ksmps = csoundGetKsmps(g_play.csound);
    g_play.nchnls = csoundGetNchnls(g_play.csound);
    g_play.zerodBFS = csoundGet0dBFS(g_play.csound);
    g_play.spout_pos = g_play.ksmps;  /* Force first ksmps cycle */
    g_play.started = 1;

    if (verbose) {
        printf("Playing: sr=%d, nchnls=%d, ksmps=%d, 0dBFS=%.1f\n",
               (int)csoundGetSr(g_play.csound), g_play.nchnls, g_play.ksmps,
               g_play.zerodBFS);
    }

    /* Initialize audio device */
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = CS_CHANNELS;
    config.sampleRate = (int)csoundGetSr(g_play.csound);
    config.dataCallback = play_audio_callback;
    config.pUserData = NULL;
    config.periodSizeInFrames = CS_PERIOD_FRAMES;

    result = ma_device_init(NULL, &config, &g_play.device);
    if (result != MA_SUCCESS) {
        set_error("Failed to initialize audio device");
        csoundStop(g_play.csound);
        csoundCleanup(g_play.csound);
        csoundDestroyMessageBuffer(g_play.csound);
        csoundDestroy(g_play.csound);
        g_play.csound = NULL;
        g_play.started = 0;
        cs_mutex_destroy(&g_play.mutex);
        return -1;
    }
    g_play.device_initialized = 1;

    /* Start audio playback */
    result = ma_device_start(&g_play.device);
    if (result != MA_SUCCESS) {
        set_error("Failed to start audio device");
        playback_cleanup();
        return -1;
    }

    g_play.active = 1;
    return 0;
}

/* Signal handler for graceful interrupt during blocking playback */
static volatile sig_atomic_t g_interrupted = 0;

static void playback_sigint_handler(int sig) {
    (void)sig;
    g_interrupted = 1;
    if (g_play.active) {
        g_play.finished = 1;
    }
}

int alda_csound_play_file(const char* path, int verbose) {
    int result = playback_start(path, verbose);
    if (result != 0) {
        return result;
    }

    /* Install signal handler for clean Ctrl-C handling */
    g_interrupted = 0;
#ifdef _WIN32
    void (*old_handler)(int) = signal(SIGINT, playback_sigint_handler);
#else
    struct sigaction sa, old_sa;
    sa.sa_handler = playback_sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, &old_sa);
#endif

    /* Wait for playback to finish (blocking) */
    while (!g_play.finished && g_play.active && !g_interrupted) {
#ifdef _WIN32
        Sleep(100);  /* Sleep 100ms */
#else
        usleep(100000);  /* Sleep 100ms */
#endif
    }

    /* Restore original signal handler */
#ifdef _WIN32
    signal(SIGINT, old_handler);
#else
    sigaction(SIGINT, &old_sa, NULL);
#endif

    if (g_interrupted) {
        printf("\nStopping playback\n");
    } else if (verbose) {
        printf("Playback complete\n");
    }

    /* Cleanup */
    playback_cleanup();

    /* Interruption is not an error - user intentionally stopped */
    return 0;
}

int alda_csound_play_file_async(const char* path) {
    return playback_start(path, 0);
}

int alda_csound_playback_active(void) {
    /* Check if we're still actively playing */
    if (!g_play.active) {
        return 0;
    }
    /* If finished flag is set, clean up and return inactive */
    if (g_play.finished) {
        playback_cleanup();
        return 0;
    }
    return 1;
}

void alda_csound_stop_playback(void) {
    if (g_play.active) {
        g_play.finished = 1;  /* Signal the audio callback to stop */
        playback_cleanup();
    }
}

#else /* BUILD_CSOUND_BACKEND not defined */

/* Stub implementations when Csound is disabled */

#include "alda/csound_backend.h"

int alda_csound_init(void) { return -1; }
void alda_csound_cleanup(void) {}
int alda_csound_load_csd(const char* path) { (void)path; return -1; }
int alda_csound_compile_orc(const char* orc) { (void)orc; return -1; }
int alda_csound_has_instruments(void) { return 0; }
int alda_csound_enable(void) { return -1; }
void alda_csound_disable(void) {}
int alda_csound_is_enabled(void) { return 0; }
void alda_csound_send_note_on(int ch, int p, int v) { (void)ch; (void)p; (void)v; }
void alda_csound_send_note_off(int ch, int p) { (void)ch; (void)p; }
void alda_csound_send_program(int ch, int p) { (void)ch; (void)p; }
void alda_csound_send_cc(int ch, int cc, int v) { (void)ch; (void)cc; (void)v; }
void alda_csound_send_pitch_bend(int ch, int b) { (void)ch; (void)b; }
void alda_csound_all_notes_off(void) {}
int alda_csound_get_sample_rate(void) { return 0; }
int alda_csound_get_channels(void) { return 0; }
void alda_csound_render(float* o, int f) { (void)o; (void)f; }
const char* alda_csound_get_error(void) { return "Csound backend not compiled"; }
int alda_csound_play_file(const char* path, int verbose) { (void)path; (void)verbose; return -1; }
int alda_csound_play_file_async(const char* path) { (void)path; return -1; }
int alda_csound_playback_active(void) { return 0; }
void alda_csound_stop_playback(void) {}

#endif /* BUILD_CSOUND_BACKEND */
