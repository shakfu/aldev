/**
 * @file fluid_backend.c
 * @brief Synthesizer backend using FluidSynth + miniaudio.
 *
 * Shared audio backend - language agnostic.
 * Build with -DBUILD_FLUID_BACKEND=ON to enable.
 */

#ifdef BUILD_FLUID_BACKEND

#include <fluidsynth.h>
#include "miniaudio.h"
#include "audio.h"
#include <stdio.h>
#include <string.h>

/* Cross-platform mutex */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef CRITICAL_SECTION fluid_mutex_t;
static inline int fluid_mutex_init(fluid_mutex_t* m) { InitializeCriticalSection(m); return 0; }
static inline void fluid_mutex_destroy(fluid_mutex_t* m) { DeleteCriticalSection(m); }
static inline void fluid_mutex_lock(fluid_mutex_t* m) { EnterCriticalSection(m); }
static inline void fluid_mutex_unlock(fluid_mutex_t* m) { LeaveCriticalSection(m); }
#else
#include <pthread.h>
typedef pthread_mutex_t fluid_mutex_t;
static inline int fluid_mutex_init(fluid_mutex_t* m) { return pthread_mutex_init(m, NULL); }
static inline void fluid_mutex_destroy(fluid_mutex_t* m) { pthread_mutex_destroy(m); }
static inline void fluid_mutex_lock(fluid_mutex_t* m) { pthread_mutex_lock(m); }
static inline void fluid_mutex_unlock(fluid_mutex_t* m) { pthread_mutex_unlock(m); }
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define FLUID_SAMPLE_RATE     44100
#define FLUID_CHANNELS        2
#define FLUID_PERIOD_FRAMES   512
#define FLUID_MAX_POLYPHONY   256

/* ============================================================================
 * Backend State (global singleton)
 * ============================================================================ */

typedef struct {
    fluid_settings_t* settings;
    fluid_synth_t* synth;
    int soundfont_id;           /* ID of loaded soundfont, -1 if none */
    ma_device device;
    int device_initialized;
    int enabled;
    int initialized;
    int ref_count;              /* Reference count for enable/disable */
    fluid_mutex_t mutex;
} FluidBackend;

static FluidBackend g_fluid = {0};

/* ============================================================================
 * Audio Callback
 * ============================================================================ */

static void fluid_audio_callback(ma_device* device, void* output, const void* input, ma_uint32 frame_count) {
    (void)device;
    (void)input;

    float* out = (float*)output;

    fluid_mutex_lock(&g_fluid.mutex);
    if (g_fluid.synth && g_fluid.enabled) {
        /* FluidSynth writes to separate L/R buffers */
        /* fluid_synth_write_float(synth, len, lout, loff, lincr, rout, roff, rincr) */
        /* For interleaved stereo output: loff=0, lincr=2, roff=1, rincr=2 */
        fluid_synth_write_float(g_fluid.synth, (int)frame_count,
                                out, 0, 2,   /* left channel */
                                out, 1, 2);  /* right channel */
    } else {
        memset(out, 0, (size_t)frame_count * FLUID_CHANNELS * sizeof(float));
    }
    fluid_mutex_unlock(&g_fluid.mutex);
}

/* ============================================================================
 * Initialization and Cleanup
 * ============================================================================ */

int shared_fluid_init(void) {
    if (g_fluid.initialized) {
        return 0;  /* Already initialized */
    }

    memset(&g_fluid, 0, sizeof(g_fluid));
    g_fluid.soundfont_id = -1;

    if (fluid_mutex_init(&g_fluid.mutex) != 0) {
        fprintf(stderr, "FluidSynth: Failed to create mutex\n");
        return -1;
    }

    /* Create FluidSynth settings */
    g_fluid.settings = new_fluid_settings();
    if (!g_fluid.settings) {
        fprintf(stderr, "FluidSynth: Failed to create settings\n");
        fluid_mutex_destroy(&g_fluid.mutex);
        return -1;
    }

    /* Configure settings for our use case */
    fluid_settings_setnum(g_fluid.settings, "synth.sample-rate", (double)FLUID_SAMPLE_RATE);
    fluid_settings_setint(g_fluid.settings, "synth.polyphony", FLUID_MAX_POLYPHONY);
    fluid_settings_setint(g_fluid.settings, "synth.midi-channels", 16);
    fluid_settings_setint(g_fluid.settings, "synth.audio-channels", 1);  /* 1 stereo pair */
    fluid_settings_setint(g_fluid.settings, "synth.audio-groups", 1);
    fluid_settings_setnum(g_fluid.settings, "synth.gain", 1.0);  /* Full volume (default is 0.2) */

    /* Create synthesizer */
    g_fluid.synth = new_fluid_synth(g_fluid.settings);
    if (!g_fluid.synth) {
        fprintf(stderr, "FluidSynth: Failed to create synthesizer\n");
        delete_fluid_settings(g_fluid.settings);
        fluid_mutex_destroy(&g_fluid.mutex);
        return -1;
    }

    g_fluid.initialized = 1;
    return 0;
}

void shared_fluid_cleanup(void) {
    if (!g_fluid.initialized) {
        return;
    }

    /* Disable and stop audio */
    shared_fluid_disable();

    fluid_mutex_lock(&g_fluid.mutex);

    /* Delete synthesizer */
    if (g_fluid.synth) {
        delete_fluid_synth(g_fluid.synth);
        g_fluid.synth = NULL;
    }

    /* Delete settings */
    if (g_fluid.settings) {
        delete_fluid_settings(g_fluid.settings);
        g_fluid.settings = NULL;
    }

    g_fluid.soundfont_id = -1;

    fluid_mutex_unlock(&g_fluid.mutex);

    fluid_mutex_destroy(&g_fluid.mutex);
    g_fluid.initialized = 0;
}

/* ============================================================================
 * Soundfont Management
 * ============================================================================ */

int shared_fluid_load_soundfont(const char* path) {
    if (!g_fluid.initialized) {
        fprintf(stderr, "FluidSynth: Backend not initialized\n");
        return -1;
    }

    if (!path) {
        fprintf(stderr, "FluidSynth: NULL path\n");
        return -1;
    }

    fluid_mutex_lock(&g_fluid.mutex);

    /* Unload existing soundfont */
    if (g_fluid.soundfont_id >= 0) {
        fluid_synth_sfunload(g_fluid.synth, g_fluid.soundfont_id, 1);
        g_fluid.soundfont_id = -1;
    }

    /* Load new soundfont */
    /* reset_presets=1 means reset all channels to use the new soundfont */
    int sfid = fluid_synth_sfload(g_fluid.synth, path, 1);
    if (sfid < 0) {
        fluid_mutex_unlock(&g_fluid.mutex);
        fprintf(stderr, "FluidSynth: Failed to load soundfont: %s\n", path);
        return -1;
    }

    g_fluid.soundfont_id = sfid;

    fluid_mutex_unlock(&g_fluid.mutex);

    return 0;
}

int shared_fluid_has_soundfont(void) {
    return g_fluid.initialized && g_fluid.soundfont_id >= 0;
}

int shared_fluid_get_preset_count(void) {
    if (!g_fluid.synth || g_fluid.soundfont_id < 0) {
        return 0;
    }

    fluid_mutex_lock(&g_fluid.mutex);
    fluid_sfont_t* sfont = fluid_synth_get_sfont_by_id(g_fluid.synth, g_fluid.soundfont_id);
    if (!sfont) {
        fluid_mutex_unlock(&g_fluid.mutex);
        return 0;
    }

    /* Count presets by iterating */
    int count = 0;
    fluid_preset_t* preset;
    fluid_sfont_iteration_start(sfont);
    while ((preset = fluid_sfont_iteration_next(sfont)) != NULL) {
        count++;
    }
    fluid_mutex_unlock(&g_fluid.mutex);

    return count;
}

const char* shared_fluid_get_preset_name(int index) {
    if (!g_fluid.synth || g_fluid.soundfont_id < 0) {
        return NULL;
    }

    fluid_mutex_lock(&g_fluid.mutex);
    fluid_sfont_t* sfont = fluid_synth_get_sfont_by_id(g_fluid.synth, g_fluid.soundfont_id);
    if (!sfont) {
        fluid_mutex_unlock(&g_fluid.mutex);
        return NULL;
    }

    /* Find preset at index */
    int count = 0;
    fluid_preset_t* preset;
    const char* name = NULL;
    fluid_sfont_iteration_start(sfont);
    while ((preset = fluid_sfont_iteration_next(sfont)) != NULL) {
        if (count == index) {
            name = fluid_preset_get_name(preset);
            break;
        }
        count++;
    }
    fluid_mutex_unlock(&g_fluid.mutex);

    return name;
}

/* ============================================================================
 * Enable/Disable (ref-counted)
 *
 * Multiple contexts can enable FluidSynth. The backend only actually starts
 * when the first context enables it (ref_count 0->1) and only stops when
 * the last context disables it (ref_count 1->0).
 * ============================================================================ */

int shared_fluid_enable(void) {
    /* Auto-initialize if needed */
    if (!g_fluid.initialized) {
        if (shared_fluid_init() != 0) {
            return -1;
        }
    }

    /* Soundfont is required for playback */
    if (g_fluid.soundfont_id < 0) {
        fprintf(stderr, "FluidSynth: No soundfont loaded\n");
        return -1;
    }

    /* Increment reference count */
    g_fluid.ref_count++;

    /* If already enabled, just return success */
    if (g_fluid.enabled) {
        return 0;
    }

    /* First enabler - actually start the backend */

    /* Initialize audio device if needed */
    if (!g_fluid.device_initialized) {
        ma_device_config config = ma_device_config_init(ma_device_type_playback);
        config.playback.format = ma_format_f32;
        config.playback.channels = FLUID_CHANNELS;
        config.sampleRate = FLUID_SAMPLE_RATE;
        config.dataCallback = fluid_audio_callback;
        config.pUserData = NULL;
        config.periodSizeInFrames = FLUID_PERIOD_FRAMES;

        ma_result result = ma_device_init(NULL, &config, &g_fluid.device);
        if (result != MA_SUCCESS) {
            fprintf(stderr, "FluidSynth: Failed to initialize audio device: %d\n", result);
            g_fluid.ref_count--;
            return -1;
        }

        g_fluid.device_initialized = 1;
    }

    /* Start audio playback */
    ma_result result = ma_device_start(&g_fluid.device);
    if (result != MA_SUCCESS) {
        fprintf(stderr, "FluidSynth: Failed to start audio device: %d\n", result);
        g_fluid.ref_count--;
        return -1;
    }

    g_fluid.enabled = 1;
    return 0;
}

void shared_fluid_disable(void) {
    if (!g_fluid.initialized || g_fluid.ref_count <= 0) {
        return;
    }

    /* Decrement reference count */
    g_fluid.ref_count--;

    /* Only actually disable when last reference is released */
    if (g_fluid.ref_count > 0) {
        return;
    }

    if (!g_fluid.enabled) {
        return;
    }

    /* Last disabler - actually stop the backend */

    /* Stop all notes */
    shared_fluid_all_notes_off();

    /* Stop audio device */
    if (g_fluid.device_initialized) {
        ma_device_stop(&g_fluid.device);
    }

    g_fluid.enabled = 0;
}

int shared_fluid_is_enabled(void) {
    return g_fluid.initialized && g_fluid.enabled;
}

/* ============================================================================
 * MIDI Message Sending
 * ============================================================================ */

void shared_fluid_send_note_on(int channel, int pitch, int velocity) {
    if (!g_fluid.synth || !g_fluid.enabled) {
        fprintf(stderr, "FluidSynth: note_on ignored (synth=%p, enabled=%d)\n",
                (void*)g_fluid.synth, g_fluid.enabled);
        return;
    }

    /* Channel is 1-16, FluidSynth uses 0-15 */
    int ch = (channel - 1) & 0x0F;

    fluid_mutex_lock(&g_fluid.mutex);
    fluid_synth_noteon(g_fluid.synth, ch, pitch, velocity);
    fluid_mutex_unlock(&g_fluid.mutex);
}

void shared_fluid_send_note_off(int channel, int pitch) {
    if (!g_fluid.synth || !g_fluid.enabled) {
        return;
    }

    /* Channel is 1-16, FluidSynth uses 0-15 */
    int ch = (channel - 1) & 0x0F;

    fluid_mutex_lock(&g_fluid.mutex);
    fluid_synth_noteoff(g_fluid.synth, ch, pitch);
    fluid_mutex_unlock(&g_fluid.mutex);
}

void shared_fluid_send_program(int channel, int program) {
    if (!g_fluid.synth || !g_fluid.enabled) {
        return;
    }

    /* Channel is 1-16, FluidSynth uses 0-15 */
    int ch = (channel - 1) & 0x0F;

    fluid_mutex_lock(&g_fluid.mutex);
    fluid_synth_program_change(g_fluid.synth, ch, program);
    fluid_mutex_unlock(&g_fluid.mutex);
}

void shared_fluid_send_cc(int channel, int cc, int value) {
    if (!g_fluid.synth || !g_fluid.enabled) {
        return;
    }

    /* Channel is 1-16, FluidSynth uses 0-15 */
    int ch = (channel - 1) & 0x0F;

    fluid_mutex_lock(&g_fluid.mutex);
    fluid_synth_cc(g_fluid.synth, ch, cc, value);
    fluid_mutex_unlock(&g_fluid.mutex);
}

void shared_fluid_send_pitch_bend(int channel, int bend) {
    if (!g_fluid.synth || !g_fluid.enabled) {
        return;
    }

    /* Channel is 1-16, FluidSynth uses 0-15 */
    int ch = (channel - 1) & 0x0F;

    /* bend is -8192 to 8191, FluidSynth expects 0-16383 */
    int value = bend + 8192;

    fluid_mutex_lock(&g_fluid.mutex);
    fluid_synth_pitch_bend(g_fluid.synth, ch, value);
    fluid_mutex_unlock(&g_fluid.mutex);
}

void shared_fluid_all_notes_off(void) {
    if (!g_fluid.synth) {
        return;
    }

    fluid_mutex_lock(&g_fluid.mutex);
    /* Send all notes off to all 16 channels */
    for (int ch = 0; ch < 16; ch++) {
        fluid_synth_all_notes_off(g_fluid.synth, ch);
    }
    fluid_mutex_unlock(&g_fluid.mutex);
}

/* ============================================================================
 * Advanced Features
 * ============================================================================ */

void shared_fluid_set_gain(float gain) {
    if (!g_fluid.synth) {
        return;
    }

    fluid_mutex_lock(&g_fluid.mutex);
    fluid_synth_set_gain(g_fluid.synth, gain);
    fluid_mutex_unlock(&g_fluid.mutex);
}

float shared_fluid_get_gain(void) {
    if (!g_fluid.synth) {
        return 0.0f;
    }

    fluid_mutex_lock(&g_fluid.mutex);
    float gain = fluid_synth_get_gain(g_fluid.synth);
    fluid_mutex_unlock(&g_fluid.mutex);
    return gain;
}

int shared_fluid_get_active_voice_count(void) {
    if (!g_fluid.synth) {
        return 0;
    }

    fluid_mutex_lock(&g_fluid.mutex);
    int count = fluid_synth_get_active_voice_count(g_fluid.synth);
    fluid_mutex_unlock(&g_fluid.mutex);
    return count;
}

#else /* BUILD_FLUID_BACKEND not defined */

/* Stub implementations when FluidSynth is not compiled in */

#include "audio.h"
#include <stddef.h>

int shared_fluid_init(void) { return -1; }
void shared_fluid_cleanup(void) {}
int shared_fluid_load_soundfont(const char* path) { (void)path; return -1; }
int shared_fluid_has_soundfont(void) { return 0; }
int shared_fluid_get_preset_count(void) { return 0; }
const char* shared_fluid_get_preset_name(int index) { (void)index; return NULL; }
int shared_fluid_enable(void) { return -1; }
void shared_fluid_disable(void) {}
int shared_fluid_is_enabled(void) { return 0; }
void shared_fluid_send_note_on(int channel, int pitch, int velocity) { (void)channel; (void)pitch; (void)velocity; }
void shared_fluid_send_note_off(int channel, int pitch) { (void)channel; (void)pitch; }
void shared_fluid_send_program(int channel, int program) { (void)channel; (void)program; }
void shared_fluid_send_cc(int channel, int cc, int value) { (void)channel; (void)cc; (void)value; }
void shared_fluid_send_pitch_bend(int channel, int bend) { (void)channel; (void)bend; }
void shared_fluid_all_notes_off(void) {}
void shared_fluid_set_gain(float gain) { (void)gain; }
float shared_fluid_get_gain(void) { return 0.0f; }
int shared_fluid_get_active_voice_count(void) { return 0; }

#endif /* BUILD_FLUID_BACKEND */
