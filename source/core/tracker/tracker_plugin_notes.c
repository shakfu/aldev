/**
 * tracker_plugin_notes.c - Simple note parser plugin implementation
 */

#include "tracker_plugin_notes.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/*============================================================================
 * Note Name Tables
 *============================================================================*/

/* Note names to semitone offset (C=0, D=2, E=4, F=5, G=7, A=9, B=11) */
static const int note_offsets[7] = { 9, 11, 0, 2, 4, 5, 7 };  /* A, B, C, D, E, F, G */

/* For converting MIDI note to string */
static const char* sharp_names[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};
static const char* flat_names[12] = {
    "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"
};

/* Transform names */
static const char* transform_names[] = {
    "transpose", "tr",
    "velocity", "vel",
    "octave", "oct",
    "invert", "inv",
    NULL
};

/*============================================================================
 * Parsing Helpers
 *============================================================================*/

static void skip_whitespace(const char** str) {
    while (**str && isspace((unsigned char)**str)) {
        (*str)++;
    }
}

static bool parse_int(const char* str, int* out_val, const char** out_end) {
    if (!str || !*str) return false;

    const char* p = str;
    bool negative = false;

    if (*p == '-') {
        negative = true;
        p++;
    } else if (*p == '+') {
        p++;
    }

    if (!isdigit((unsigned char)*p)) return false;

    int val = 0;
    while (isdigit((unsigned char)*p)) {
        val = val * 10 + (*p - '0');
        p++;
    }

    if (out_val) *out_val = negative ? -val : val;
    if (out_end) *out_end = p;
    return true;
}

/*============================================================================
 * Public Parsing Functions
 *============================================================================*/

bool tracker_notes_parse_note(const char* str, uint8_t* out_note, const char** out_end) {
    if (!str) return false;

    const char* p = str;
    skip_whitespace(&p);

    /* Parse note letter (A-G, case insensitive) */
    char letter = toupper((unsigned char)*p);
    if (letter < 'A' || letter > 'G') return false;
    p++;

    /* Get base semitone offset */
    int semitone = note_offsets[letter - 'A'];

    /* Parse accidentals (# or b, can be multiple) */
    while (*p == '#' || *p == 'b') {
        if (*p == '#') {
            semitone++;
        } else {
            semitone--;
        }
        p++;
    }

    /* Parse octave (default to 4 if not specified) */
    int octave = TRACKER_NOTES_DEFAULT_OCTAVE;
    if (isdigit((unsigned char)*p)) {
        octave = *p - '0';
        p++;
        /* Handle two-digit octave (10) */
        if (isdigit((unsigned char)*p)) {
            octave = octave * 10 + (*p - '0');
            p++;
        }
    }

    /* Calculate MIDI note number */
    /* MIDI: C4 = 60, so octave 4 starts at note 48 (C) + 12 = 60 */
    int midi_note = (octave + 1) * 12 + semitone;

    /* Clamp to valid MIDI range */
    if (midi_note < 0) midi_note = 0;
    if (midi_note > 127) midi_note = 127;

    if (out_note) *out_note = (uint8_t)midi_note;
    if (out_end) *out_end = p;
    return true;
}

bool tracker_notes_parse_velocity(const char* str, uint8_t* out_vel, const char** out_end) {
    if (!str) return false;

    const char* p = str;

    /* Check for @ or v prefix */
    if (*p != '@' && *p != 'v' && *p != 'V') return false;
    p++;

    /* Parse number */
    int vel;
    if (!parse_int(p, &vel, &p)) return false;

    /* Clamp to 0-127 */
    if (vel < 0) vel = 0;
    if (vel > 127) vel = 127;

    if (out_vel) *out_vel = (uint8_t)vel;
    if (out_end) *out_end = p;
    return true;
}

bool tracker_notes_parse_gate(const char* str, int16_t* out_rows, const char** out_end) {
    if (!str) return false;

    const char* p = str;

    /* Check for ~ prefix */
    if (*p != '~') return false;
    p++;

    /* Parse number */
    int rows;
    if (!parse_int(p, &rows, &p)) return false;

    /* Minimum gate of 0 (instant), no maximum */
    if (rows < 0) rows = 0;

    if (out_rows) *out_rows = (int16_t)rows;
    if (out_end) *out_end = p;
    return true;
}

void tracker_notes_to_string(uint8_t note, char* buffer, bool use_sharps) {
    if (!buffer) return;

    int octave = (note / 12) - 1;
    int semitone = note % 12;

    const char* name = use_sharps ? sharp_names[semitone] : flat_names[semitone];

    sprintf(buffer, "%s%d", name, octave);
}

/*============================================================================
 * Expression Evaluation
 *============================================================================*/

/**
 * Parse a complete note expression and return a phrase.
 */
static TrackerPhrase* parse_expression(const char* expr, TrackerContext* ctx) {
    if (!expr) return NULL;

    TrackerPhrase* phrase = tracker_phrase_new(8);
    if (!phrase) return NULL;

    const char* p = expr;
    skip_whitespace(&p);

    /* Check for rest */
    if (*p == 'r' || *p == '-') {
        /* Rest - return empty phrase */
        return phrase;
    }

    /* Check for note-off */
    if (*p == 'x' || *p == 'X' ||
        (strncasecmp(p, "off", 3) == 0 && !isalnum((unsigned char)p[3]))) {
        /* Note-off - add a note-off event for channel */
        TrackerEvent event = {0};
        event.type = TRACKER_EVENT_NOTE_OFF;
        event.channel = ctx ? ctx->channel : 0;
        event.data1 = 255;  /* special: all notes */
        event.data2 = 0;
        tracker_phrase_add_event(phrase, &event);
        return phrase;
    }

    /* Parse notes (possibly multiple for chords) */
    uint8_t default_velocity = TRACKER_NOTES_DEFAULT_VELOCITY;
    int16_t default_gate = TRACKER_NOTES_DEFAULT_GATE;
    uint8_t channel = ctx ? ctx->channel : 0;

    while (*p) {
        skip_whitespace(&p);
        if (!*p) break;

        /* Skip separators */
        if (*p == ',' || *p == '|') {
            p++;
            continue;
        }

        /* Parse note */
        uint8_t note;
        if (!tracker_notes_parse_note(p, &note, &p)) {
            /* Unknown character, skip it */
            p++;
            continue;
        }

        /* Parse optional velocity */
        uint8_t velocity = default_velocity;
        if (*p == '@' || *p == 'v' || *p == 'V') {
            tracker_notes_parse_velocity(p, &velocity, &p);
        }

        /* Parse optional gate */
        int16_t gate_rows = default_gate;
        if (*p == '~') {
            tracker_notes_parse_gate(p, &gate_rows, &p);
        }

        /* Create note-on event */
        TrackerEvent event = {0};
        event.type = TRACKER_EVENT_NOTE_ON;
        event.channel = channel;
        event.data1 = note;
        event.data2 = velocity;
        event.offset_rows = 0;
        event.offset_ticks = 0;
        event.gate_rows = gate_rows;
        event.gate_ticks = 0;

        tracker_phrase_add_event(phrase, &event);
    }

    return phrase;
}

/*============================================================================
 * Transform Functions
 *============================================================================*/

static TrackerPhrase* transform_transpose(
    const TrackerPhrase* input,
    const char* params,
    TrackerContext* ctx
) {
    (void)ctx;
    if (!input) return NULL;

    int semitones = 0;
    if (params) {
        parse_int(params, &semitones, NULL);
    }

    TrackerPhrase* result = tracker_phrase_clone(input);
    if (!result) return NULL;

    for (int i = 0; i < result->count; i++) {
        TrackerEvent* e = &result->events[i];
        if (e->type == TRACKER_EVENT_NOTE_ON || e->type == TRACKER_EVENT_NOTE_OFF) {
            int new_note = (int)e->data1 + semitones;
            if (new_note < 0) new_note = 0;
            if (new_note > 127) new_note = 127;
            e->data1 = (uint8_t)new_note;
        }
    }

    return result;
}

static TrackerPhrase* transform_velocity(
    const TrackerPhrase* input,
    const char* params,
    TrackerContext* ctx
) {
    (void)ctx;
    if (!input) return NULL;

    int velocity = TRACKER_NOTES_DEFAULT_VELOCITY;
    if (params) {
        parse_int(params, &velocity, NULL);
        if (velocity < 0) velocity = 0;
        if (velocity > 127) velocity = 127;
    }

    TrackerPhrase* result = tracker_phrase_clone(input);
    if (!result) return NULL;

    for (int i = 0; i < result->count; i++) {
        TrackerEvent* e = &result->events[i];
        if (e->type == TRACKER_EVENT_NOTE_ON) {
            e->data2 = (uint8_t)velocity;
        }
    }

    return result;
}

static TrackerPhrase* transform_octave(
    const TrackerPhrase* input,
    const char* params,
    TrackerContext* ctx
) {
    (void)ctx;
    if (!input) return NULL;

    int octaves = 0;
    if (params) {
        parse_int(params, &octaves, NULL);
    }
    int semitones = octaves * 12;

    TrackerPhrase* result = tracker_phrase_clone(input);
    if (!result) return NULL;

    for (int i = 0; i < result->count; i++) {
        TrackerEvent* e = &result->events[i];
        if (e->type == TRACKER_EVENT_NOTE_ON || e->type == TRACKER_EVENT_NOTE_OFF) {
            int new_note = (int)e->data1 + semitones;
            if (new_note < 0) new_note = 0;
            if (new_note > 127) new_note = 127;
            e->data1 = (uint8_t)new_note;
        }
    }

    return result;
}

static TrackerPhrase* transform_invert(
    const TrackerPhrase* input,
    const char* params,
    TrackerContext* ctx
) {
    (void)ctx;
    if (!input) return NULL;

    int pivot = 60;  /* C4 */
    if (params) {
        /* Try parsing as note name first */
        uint8_t note;
        if (tracker_notes_parse_note(params, &note, NULL)) {
            pivot = note;
        } else {
            parse_int(params, &pivot, NULL);
        }
    }

    TrackerPhrase* result = tracker_phrase_clone(input);
    if (!result) return NULL;

    for (int i = 0; i < result->count; i++) {
        TrackerEvent* e = &result->events[i];
        if (e->type == TRACKER_EVENT_NOTE_ON || e->type == TRACKER_EVENT_NOTE_OFF) {
            int new_note = pivot - ((int)e->data1 - pivot);
            if (new_note < 0) new_note = 0;
            if (new_note > 127) new_note = 127;
            e->data1 = (uint8_t)new_note;
        }
    }

    return result;
}

/*============================================================================
 * Plugin Callbacks
 *============================================================================*/

static bool plugin_init(void) {
    return true;
}

static void plugin_cleanup(void) {
    /* Nothing to clean up */
}

static void plugin_reset(void) {
    /* Nothing to reset */
}

static const char* validation_error = NULL;

static bool plugin_validate(const char* expression, const char** error_msg, int* error_pos) {
    if (!expression || !*expression) {
        validation_error = "Empty expression";
        if (error_msg) *error_msg = validation_error;
        if (error_pos) *error_pos = 0;
        return false;
    }

    const char* p = expression;
    skip_whitespace(&p);

    /* Check for rest or note-off */
    if (*p == 'r' || *p == '-' || *p == 'x' || *p == 'X') {
        return true;
    }
    if (strncasecmp(p, "off", 3) == 0) {
        return true;
    }

    /* Try to parse at least one note */
    uint8_t note;
    if (!tracker_notes_parse_note(p, &note, NULL)) {
        validation_error = "Invalid note: expected A-G";
        if (error_msg) *error_msg = validation_error;
        if (error_pos) *error_pos = (int)(p - expression);
        return false;
    }

    return true;
}

static bool plugin_is_generator(const char* expression) {
    (void)expression;
    /* Notes are not generators - they produce fixed output */
    return false;
}

static TrackerPhrase* plugin_evaluate(const char* expression, TrackerContext* ctx) {
    return parse_expression(expression, ctx);
}

static TrackerTransformFn plugin_get_transform(const char* fx_name) {
    if (!fx_name) return NULL;

    if (strcmp(fx_name, "transpose") == 0 || strcmp(fx_name, "tr") == 0) {
        return transform_transpose;
    }
    if (strcmp(fx_name, "velocity") == 0 || strcmp(fx_name, "vel") == 0) {
        return transform_velocity;
    }
    if (strcmp(fx_name, "octave") == 0 || strcmp(fx_name, "oct") == 0) {
        return transform_octave;
    }
    if (strcmp(fx_name, "invert") == 0 || strcmp(fx_name, "inv") == 0) {
        return transform_invert;
    }

    return NULL;
}

static const char** plugin_list_transforms(int* count) {
    if (count) {
        /* Count non-NULL entries */
        int n = 0;
        for (int i = 0; transform_names[i] != NULL; i++) {
            n++;
        }
        *count = n;
    }
    return transform_names;
}

static const char* plugin_describe_transform(const char* fx_name) {
    if (!fx_name) return NULL;

    if (strcmp(fx_name, "transpose") == 0 || strcmp(fx_name, "tr") == 0) {
        return "Transpose notes by semitones";
    }
    if (strcmp(fx_name, "velocity") == 0 || strcmp(fx_name, "vel") == 0) {
        return "Set note velocity (0-127)";
    }
    if (strcmp(fx_name, "octave") == 0 || strcmp(fx_name, "oct") == 0) {
        return "Shift notes by octaves";
    }
    if (strcmp(fx_name, "invert") == 0 || strcmp(fx_name, "inv") == 0) {
        return "Invert notes around a pivot";
    }

    return NULL;
}

static const char* plugin_get_transform_params_doc(const char* fx_name) {
    if (!fx_name) return NULL;

    if (strcmp(fx_name, "transpose") == 0 || strcmp(fx_name, "tr") == 0) {
        return "semitones: integer (positive = up, negative = down)";
    }
    if (strcmp(fx_name, "velocity") == 0 || strcmp(fx_name, "vel") == 0) {
        return "velocity: 0-127";
    }
    if (strcmp(fx_name, "octave") == 0 || strcmp(fx_name, "oct") == 0) {
        return "octaves: integer (positive = up, negative = down)";
    }
    if (strcmp(fx_name, "invert") == 0 || strcmp(fx_name, "inv") == 0) {
        return "pivot: note name (e.g., C4) or MIDI number (default: 60)";
    }

    return NULL;
}

/*============================================================================
 * Plugin Definition
 *============================================================================*/

static TrackerPlugin notes_plugin = {
    /* Identity */
    .name = "Notes",
    .language_id = "notes",
    .version = "1.0",
    .description = "Simple note notation parser (C4, D#5, Bb3)",

    /* Capabilities & Priority */
    .capabilities = TRACKER_CAP_EVALUATE | TRACKER_CAP_VALIDATION | TRACKER_CAP_TRANSFORMS,
    .priority = 0,

    /* Lifecycle */
    .init = plugin_init,
    .cleanup = plugin_cleanup,
    .reset = plugin_reset,

    /* Expression Handling */
    .validate = plugin_validate,
    .is_generator = plugin_is_generator,
    .evaluate = plugin_evaluate,
    .compile = NULL,
    .evaluate_compiled = NULL,
    .free_compiled = NULL,

    /* Transform Handling */
    .get_transform = plugin_get_transform,
    .list_transforms = plugin_list_transforms,
    .describe_transform = plugin_describe_transform,
    .get_transform_params_doc = plugin_get_transform_params_doc,
    .validate_transform_params = NULL,
    .parse_transform_params = NULL,
    .free_transform_params = NULL,
    .get_last_error = NULL,
    .get_last_error_message = NULL,
    .clear_error = NULL,
};

const TrackerPlugin* tracker_plugin_notes_get(void) {
    return &notes_plugin;
}

bool tracker_plugin_notes_register(void) {
    return tracker_plugin_register(&notes_plugin);
}
