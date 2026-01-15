/**
 * @file scala.c
 * @brief Scala scale file (.scl) parser implementation
 *
 * Parses the Scala scale file format for microtuning support.
 * Format specification: https://www.huygens-fokker.org/scala/scl_format.html
 */

#include <alda/scala.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>

/* Error message buffer */
static char g_error[256] = {0};

/* Set error message */
static void set_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_error, sizeof(g_error), fmt, args);
    va_end(args);
}

const char *scala_get_error(void) {
    return g_error;
}

/* Convert cents to ratio: ratio = 2^(cents/1200) */
double scala_cents_to_ratio(double cents) {
    return pow(2.0, cents / 1200.0);
}

/* Convert ratio to cents: cents = 1200 * log2(ratio) */
double scala_ratio_to_cents(double ratio) {
    if (ratio <= 0.0) return 0.0;
    return 1200.0 * log2(ratio);
}

/* Skip leading whitespace */
static const char *skip_whitespace(const char *s) {
    while (*s && (*s == ' ' || *s == '\t')) s++;
    return s;
}

/* Check if line is a comment (starts with !) */
static int is_comment(const char *line) {
    const char *p = skip_whitespace(line);
    return *p == '!';
}

/* Check if line is blank */
static int is_blank(const char *line) {
    const char *p = line;
    while (*p) {
        if (*p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') {
            return 0;
        }
        p++;
    }
    return 1;
}

/* Parse a pitch value line.
 * Returns 1 on success, 0 on error.
 * Sets degree->ratio and format info. */
static int parse_pitch(const char *line, ScalaDegree *degree) {
    const char *p = skip_whitespace(line);

    if (*p == '\0' || *p == '\r' || *p == '\n') {
        set_error("Empty pitch line");
        return 0;
    }

    /* Check for cents format (contains '.') */
    int has_dot = 0;
    int has_slash = 0;
    const char *scan = p;
    while (*scan && !isspace(*scan)) {
        if (*scan == '.') has_dot = 1;
        if (*scan == '/') has_slash = 1;
        scan++;
    }

    if (has_dot) {
        /* Cents format */
        char *end;
        double cents = strtod(p, &end);
        if (end == p) {
            set_error("Invalid cents value: %s", line);
            return 0;
        }
        degree->cents_format = 1;
        degree->cents = cents;
        degree->ratio = scala_cents_to_ratio(cents);
        degree->numerator = 0;
        degree->denominator = 0;
    } else if (has_slash) {
        /* Ratio format: num/den */
        char *slash;
        long num = strtol(p, &slash, 10);
        if (*slash != '/') {
            set_error("Invalid ratio format: %s", line);
            return 0;
        }
        char *end;
        long den = strtol(slash + 1, &end, 10);
        if (den == 0) {
            set_error("Denominator cannot be zero: %s", line);
            return 0;
        }
        if (num < 0 || den < 0) {
            set_error("Negative ratios not allowed: %s", line);
            return 0;
        }
        degree->cents_format = 0;
        degree->numerator = (int)num;
        degree->denominator = (int)den;
        degree->ratio = (double)num / (double)den;
        degree->cents = scala_ratio_to_cents(degree->ratio);
    } else {
        /* Integer format (treated as ratio n/1) */
        char *end;
        long num = strtol(p, &end, 10);
        if (end == p) {
            set_error("Invalid pitch value: %s", line);
            return 0;
        }
        if (num < 0) {
            set_error("Negative values not allowed: %s", line);
            return 0;
        }
        degree->cents_format = 0;
        degree->numerator = (int)num;
        degree->denominator = 1;
        degree->ratio = (double)num;
        degree->cents = scala_ratio_to_cents(degree->ratio);
    }

    return 1;
}

/* Parse scale from lines array */
static ScalaScale *parse_scale(char **lines, int line_count) {
    ScalaScale *scale = calloc(1, sizeof(ScalaScale));
    if (!scale) {
        set_error("Memory allocation failed");
        return NULL;
    }

    int current_line = 0;

    /* Skip leading comments and blank lines, find description */
    while (current_line < line_count) {
        if (!is_comment(lines[current_line]) && !is_blank(lines[current_line])) {
            break;
        }
        current_line++;
    }

    if (current_line >= line_count) {
        set_error("No description line found");
        free(scale);
        return NULL;
    }

    /* Line 1: Description */
    const char *desc = skip_whitespace(lines[current_line]);
    /* Trim trailing whitespace/newline */
    size_t desc_len = strlen(desc);
    while (desc_len > 0 && (desc[desc_len-1] == '\n' || desc[desc_len-1] == '\r' ||
                            desc[desc_len-1] == ' ' || desc[desc_len-1] == '\t')) {
        desc_len--;
    }
    if (desc_len >= SCALA_MAX_DESCRIPTION) {
        desc_len = SCALA_MAX_DESCRIPTION - 1;
    }
    strncpy(scale->description, desc, desc_len);
    scale->description[desc_len] = '\0';
    current_line++;

    /* Skip comments and blank lines, find note count */
    while (current_line < line_count) {
        if (!is_comment(lines[current_line]) && !is_blank(lines[current_line])) {
            break;
        }
        current_line++;
    }

    if (current_line >= line_count) {
        set_error("No note count line found");
        free(scale);
        return NULL;
    }

    /* Line 2: Note count */
    const char *count_str = skip_whitespace(lines[current_line]);
    char *end;
    long note_count = strtol(count_str, &end, 10);
    if (end == count_str || note_count < 0 || note_count > SCALA_MAX_DEGREES) {
        set_error("Invalid note count: %s", count_str);
        free(scale);
        return NULL;
    }
    scale->length = (int)note_count;
    current_line++;

    /* Allocate degrees array (note_count + 1 for implicit 1/1) */
    scale->degree_count = scale->length + 1;
    scale->degrees = calloc(scale->degree_count, sizeof(ScalaDegree));
    if (!scale->degrees) {
        set_error("Memory allocation failed for degrees");
        free(scale);
        return NULL;
    }

    /* Degree 0: implicit 1/1 */
    scale->degrees[0].ratio = 1.0;
    scale->degrees[0].cents_format = 0;
    scale->degrees[0].numerator = 1;
    scale->degrees[0].denominator = 1;
    scale->degrees[0].cents = 0.0;

    /* Parse pitch values */
    int degree_idx = 1;
    while (current_line < line_count && degree_idx < scale->degree_count) {
        if (is_comment(lines[current_line]) || is_blank(lines[current_line])) {
            current_line++;
            continue;
        }

        if (!parse_pitch(lines[current_line], &scale->degrees[degree_idx])) {
            /* Error already set by parse_pitch */
            scala_free(scale);
            return NULL;
        }

        degree_idx++;
        current_line++;
    }

    if (degree_idx != scale->degree_count) {
        set_error("Expected %d pitch values, found %d", scale->length, degree_idx - 1);
        scala_free(scale);
        return NULL;
    }

    return scale;
}

/* Split buffer into lines */
static char **split_lines(const char *buffer, size_t len, int *out_count) {
    /* Count lines */
    int count = 1;
    for (size_t i = 0; i < len; i++) {
        if (buffer[i] == '\n') count++;
    }

    char **lines = calloc(count, sizeof(char *));
    if (!lines) return NULL;

    /* Split into lines */
    int line_idx = 0;
    const char *start = buffer;
    for (size_t i = 0; i <= len; i++) {
        if (i == len || buffer[i] == '\n') {
            size_t line_len = (buffer + i) - start;
            lines[line_idx] = malloc(line_len + 1);
            if (!lines[line_idx]) {
                for (int j = 0; j < line_idx; j++) free(lines[j]);
                free(lines);
                return NULL;
            }
            memcpy(lines[line_idx], start, line_len);
            lines[line_idx][line_len] = '\0';
            line_idx++;
            start = buffer + i + 1;
        }
    }

    *out_count = line_idx;
    return lines;
}

/* Free lines array */
static void free_lines(char **lines, int count) {
    if (lines) {
        for (int i = 0; i < count; i++) {
            free(lines[i]);
        }
        free(lines);
    }
}

ScalaScale *scala_load_string(const char *buffer, size_t len) {
    if (!buffer || len == 0) {
        set_error("Empty buffer");
        return NULL;
    }

    int line_count = 0;
    char **lines = split_lines(buffer, len, &line_count);
    if (!lines) {
        set_error("Failed to split buffer into lines");
        return NULL;
    }

    ScalaScale *scale = parse_scale(lines, line_count);
    free_lines(lines, line_count);

    return scale;
}

ScalaScale *scala_load(const char *path) {
    if (!path) {
        set_error("NULL path");
        return NULL;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        set_error("Cannot open file: %s", path);
        return NULL;
    }

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > 1024 * 1024) {  /* 1MB limit */
        set_error("Invalid file size: %ld", size);
        fclose(f);
        return NULL;
    }

    char *buffer = malloc(size + 1);
    if (!buffer) {
        set_error("Memory allocation failed");
        fclose(f);
        return NULL;
    }

    size_t read = fread(buffer, 1, size, f);
    buffer[read] = '\0';
    fclose(f);

    ScalaScale *scale = scala_load_string(buffer, read);
    free(buffer);

    return scale;
}

void scala_free(ScalaScale *scale) {
    if (scale) {
        free(scale->degrees);
        free(scale);
    }
}

double scala_get_ratio(const ScalaScale *scale, int degree) {
    if (!scale || degree < 0 || degree >= scale->degree_count) {
        return -1.0;
    }
    return scale->degrees[degree].ratio;
}

double scala_get_frequency(const ScalaScale *scale, int degree, double base_freq) {
    double ratio = scala_get_ratio(scale, degree);
    if (ratio < 0) return -1.0;
    return base_freq * ratio;
}

double scala_midi_to_freq(const ScalaScale *scale, int midi_note,
                          int root_note, double root_freq) {
    if (!scale || scale->degree_count <= 1) {
        /* No scale or empty scale - use 12-TET */
        return root_freq * pow(2.0, (midi_note - root_note) / 12.0);
    }

    /* Calculate degree within scale with octave wrapping */
    int scale_len = scale->length;  /* Degrees per octave (excluding 1/1) */
    if (scale_len <= 0) {
        return root_freq * pow(2.0, (midi_note - root_note) / 12.0);
    }

    int note_diff = midi_note - root_note;

    /* Octave and degree calculation */
    int octave = note_diff / scale_len;
    int degree = note_diff % scale_len;

    /* Handle negative modulo */
    if (degree < 0) {
        degree += scale_len;
        octave--;
    }

    /* Get ratio for this degree (degree 0 = 1/1, degree n = nth pitch) */
    double ratio = scale->degrees[degree].ratio;

    /* Get octave ratio from last degree (usually 2/1) */
    double octave_ratio = scale->degrees[scale_len].ratio;

    /* Calculate final frequency */
    double freq = root_freq * ratio * pow(octave_ratio, octave);

    return freq;
}

int scala_get_length(const ScalaScale *scale) {
    if (!scale) return -1;
    return scale->length;
}

const char *scala_get_description(const ScalaScale *scale) {
    if (!scale) return NULL;
    return scale->description;
}
