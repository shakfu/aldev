/**
 * @file param.c
 * @brief Named parameter system implementation.
 *
 * Thread-safe parameter storage with OSC and MIDI CC binding support.
 */

#include "param.h"
#include "../context.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

void shared_param_init(SharedContext* ctx) {
    if (!ctx) return;

    SharedParamStore* store = &ctx->params;

    /* Clear all parameters */
    memset(store->params, 0, sizeof(store->params));
    store->count = 0;

    /* Initialize MIDI CC map to -1 (no binding) */
    for (int ch = 0; ch < 16; ch++) {
        for (int cc = 0; cc < 128; cc++) {
            store->midi_cc_map[ch][cc] = -1;
        }
    }
}

void shared_param_cleanup(SharedContext* ctx) {
    if (!ctx) return;

    /* Nothing to free - all storage is inline */
    ctx->params.count = 0;
}

/* ============================================================================
 * Parameter Definition
 * ============================================================================ */

int shared_param_define(SharedContext* ctx, const char* name,
                        ParamType type, float min, float max, float def) {
    if (!ctx || !name || name[0] == '\0') return -1;

    SharedParamStore* store = &ctx->params;

    /* Check if name already exists */
    int existing = shared_param_find(ctx, name);
    if (existing >= 0) {
        fprintf(stderr, "[Param] Parameter '%s' already exists\n", name);
        return -1;
    }

    /* Find empty slot */
    int slot = -1;
    for (int i = 0; i < PARAM_MAX_COUNT; i++) {
        if (!store->params[i].defined) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        fprintf(stderr, "[Param] Parameter store full (max %d)\n", PARAM_MAX_COUNT);
        return -1;
    }

    /* Initialize parameter */
    SharedParam* param = &store->params[slot];
    strncpy(param->name, name, PARAM_MAX_NAME_LEN - 1);
    param->name[PARAM_MAX_NAME_LEN - 1] = '\0';
    param->type = type;
    param->min_val = min;
    param->max_val = max;
    param->default_val = def;
    atomic_store(&param->value, def);
    param->osc_path[0] = '\0';
    param->midi_channel = 0;
    param->midi_cc = -1;
    param->defined = true;

    store->count++;

    return slot;
}

int shared_param_find(SharedContext* ctx, const char* name) {
    if (!ctx || !name) return -1;

    SharedParamStore* store = &ctx->params;

    for (int i = 0; i < PARAM_MAX_COUNT; i++) {
        if (store->params[i].defined &&
            strcmp(store->params[i].name, name) == 0) {
            return i;
        }
    }

    return -1;
}

int shared_param_undefine(SharedContext* ctx, const char* name) {
    if (!ctx || !name) return -1;

    int idx = shared_param_find(ctx, name);
    if (idx < 0) return -1;

    SharedParamStore* store = &ctx->params;
    SharedParam* param = &store->params[idx];

    /* Clear MIDI CC binding if any */
    if (param->midi_channel > 0 && param->midi_cc >= 0) {
        store->midi_cc_map[param->midi_channel - 1][param->midi_cc] = -1;
    }

    /* Clear slot */
    memset(param, 0, sizeof(*param));
    param->midi_cc = -1;
    param->defined = false;

    store->count--;

    return 0;
}

int shared_param_count(SharedContext* ctx) {
    if (!ctx) return 0;
    return ctx->params.count;
}

const SharedParam* shared_param_at(SharedContext* ctx, int idx) {
    if (!ctx || idx < 0 || idx >= PARAM_MAX_COUNT) return NULL;
    if (!ctx->params.params[idx].defined) return NULL;
    return &ctx->params.params[idx];
}

/* ============================================================================
 * Value Access
 * ============================================================================ */

int shared_param_get(SharedContext* ctx, const char* name, float* value) {
    if (!ctx || !name || !value) return -1;

    int idx = shared_param_find(ctx, name);
    if (idx < 0) return -1;

    *value = shared_param_get_idx(ctx, idx);
    return 0;
}

float shared_param_get_idx(SharedContext* ctx, int idx) {
    if (!ctx || idx < 0 || idx >= PARAM_MAX_COUNT) return 0.0f;
    if (!ctx->params.params[idx].defined) return 0.0f;

    return atomic_load(&ctx->params.params[idx].value);
}

/* Helper: clamp value to parameter range */
static float clamp_value(SharedParam* param, float value) {
    if (value < param->min_val) return param->min_val;
    if (value > param->max_val) return param->max_val;

    /* For integer type, round to nearest integer */
    if (param->type == PARAM_TYPE_INT) {
        value = (float)(int)(value + 0.5f);
    }
    /* For boolean type, convert to 0 or 1 */
    else if (param->type == PARAM_TYPE_BOOL) {
        value = (value >= 0.5f) ? 1.0f : 0.0f;
    }

    return value;
}

int shared_param_set(SharedContext* ctx, const char* name, float value) {
    if (!ctx || !name) return -1;

    int idx = shared_param_find(ctx, name);
    if (idx < 0) return -1;

    shared_param_set_idx(ctx, idx, value);
    return 0;
}

void shared_param_set_idx(SharedContext* ctx, int idx, float value) {
    if (!ctx || idx < 0 || idx >= PARAM_MAX_COUNT) return;

    SharedParam* param = &ctx->params.params[idx];
    if (!param->defined) return;

    value = clamp_value(param, value);
    atomic_store(&param->value, value);
}

int shared_param_reset(SharedContext* ctx, const char* name) {
    if (!ctx || !name) return -1;

    int idx = shared_param_find(ctx, name);
    if (idx < 0) return -1;

    SharedParam* param = &ctx->params.params[idx];
    atomic_store(&param->value, param->default_val);

    return 0;
}

void shared_param_reset_all(SharedContext* ctx) {
    if (!ctx) return;

    for (int i = 0; i < PARAM_MAX_COUNT; i++) {
        SharedParam* param = &ctx->params.params[i];
        if (param->defined) {
            atomic_store(&param->value, param->default_val);
        }
    }
}

/* ============================================================================
 * OSC Binding
 * ============================================================================ */

int shared_param_bind_osc(SharedContext* ctx, const char* name, const char* path) {
    if (!ctx || !name || !path) return -1;

    int idx = shared_param_find(ctx, name);
    if (idx < 0) return -1;

    SharedParam* param = &ctx->params.params[idx];
    strncpy(param->osc_path, path, PARAM_MAX_OSC_PATH_LEN - 1);
    param->osc_path[PARAM_MAX_OSC_PATH_LEN - 1] = '\0';

    return 0;
}

int shared_param_unbind_osc(SharedContext* ctx, const char* name) {
    if (!ctx || !name) return -1;

    int idx = shared_param_find(ctx, name);
    if (idx < 0) return -1;

    ctx->params.params[idx].osc_path[0] = '\0';
    return 0;
}

int shared_param_find_by_osc_path(SharedContext* ctx, const char* path) {
    if (!ctx || !path) return -1;

    SharedParamStore* store = &ctx->params;

    for (int i = 0; i < PARAM_MAX_COUNT; i++) {
        if (store->params[i].defined &&
            store->params[i].osc_path[0] != '\0' &&
            strcmp(store->params[i].osc_path, path) == 0) {
            return i;
        }
    }

    return -1;
}

/* ============================================================================
 * MIDI CC Binding
 * ============================================================================ */

int shared_param_bind_midi_cc(SharedContext* ctx, const char* name,
                               int channel, int cc) {
    if (!ctx || !name) return -1;
    if (channel < 1 || channel > 16) return -1;
    if (cc < 0 || cc > 127) return -1;

    int idx = shared_param_find(ctx, name);
    if (idx < 0) return -1;

    SharedParamStore* store = &ctx->params;
    SharedParam* param = &store->params[idx];

    /* Clear existing binding if any */
    if (param->midi_channel > 0 && param->midi_cc >= 0) {
        store->midi_cc_map[param->midi_channel - 1][param->midi_cc] = -1;
    }

    /* Set new binding */
    param->midi_channel = channel;
    param->midi_cc = cc;
    store->midi_cc_map[channel - 1][cc] = (int8_t)idx;

    return 0;
}

int shared_param_unbind_midi_cc(SharedContext* ctx, const char* name) {
    if (!ctx || !name) return -1;

    int idx = shared_param_find(ctx, name);
    if (idx < 0) return -1;

    SharedParamStore* store = &ctx->params;
    SharedParam* param = &store->params[idx];

    /* Clear binding in map */
    if (param->midi_channel > 0 && param->midi_cc >= 0) {
        store->midi_cc_map[param->midi_channel - 1][param->midi_cc] = -1;
    }

    param->midi_channel = 0;
    param->midi_cc = -1;

    return 0;
}

int shared_param_handle_midi_cc(SharedContext* ctx, int channel, int cc, int value) {
    if (!ctx) return 0;
    if (channel < 1 || channel > 16) return 0;
    if (cc < 0 || cc > 127) return 0;
    if (value < 0 || value > 127) return 0;

    SharedParamStore* store = &ctx->params;

    /* Lookup parameter bound to this CC */
    int8_t param_idx = store->midi_cc_map[channel - 1][cc];
    if (param_idx < 0) return 0;

    SharedParam* param = &store->params[param_idx];
    if (!param->defined) return 0;

    /* Scale CC value (0-127) to parameter range */
    float scaled = param->min_val +
                   ((float)value / 127.0f) * (param->max_val - param->min_val);

    scaled = clamp_value(param, scaled);
    atomic_store(&param->value, scaled);

    return 1;
}
