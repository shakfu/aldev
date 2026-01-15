/**
 * @file scala.h
 * @brief Scala scale file (.scl) parser
 *
 * Parser for the Scala scale file format used for microtuning.
 * See: https://www.huygens-fokker.org/scala/scl_format.html
 *
 * Usage:
 *   ScalaScale *scale = scala_load("my_scale.scl");
 *   if (scale) {
 *       double ratio = scala_get_ratio(scale, 7);  // Get 7th degree
 *       double freq = scala_get_frequency(scale, 7, 440.0);  // A4 as base
 *       scala_free(scale);
 *   }
 */

#ifndef ALDA_SCALA_H
#define ALDA_SCALA_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Maximum length for scale description
 */
#define SCALA_MAX_DESCRIPTION 256

/**
 * Maximum number of degrees in a scale (practical limit)
 */
#define SCALA_MAX_DEGREES 128

/**
 * Represents a single pitch degree in a scale.
 * Internally stored as a frequency ratio relative to 1/1.
 */
typedef struct {
    double ratio;       /* Frequency ratio (e.g., 1.5 for 3/2) */
    int cents_format;   /* 1 if originally specified in cents, 0 if ratio */
    int numerator;      /* Original numerator if ratio format */
    int denominator;    /* Original denominator if ratio format */
    double cents;       /* Original cents value if cents format */
} ScalaDegree;

/**
 * Represents a complete musical scale loaded from a .scl file.
 */
typedef struct ScalaScale {
    char description[SCALA_MAX_DESCRIPTION];  /* Scale description */
    int length;                               /* Number of degrees (excluding implicit 1/1) */
    ScalaDegree *degrees;                     /* Array of degrees (index 0 = implicit 1/1) */
    int degree_count;                         /* Total degrees including implicit 1/1 */
} ScalaScale;

/**
 * Load a Scala scale file (.scl).
 *
 * @param path Path to the .scl file
 * @return Pointer to ScalaScale on success, NULL on error
 *         Caller must free with scala_free()
 */
ScalaScale *scala_load(const char *path);

/**
 * Load a Scala scale from a string buffer.
 *
 * @param buffer String containing .scl file contents
 * @param len Length of buffer
 * @return Pointer to ScalaScale on success, NULL on error
 *         Caller must free with scala_free()
 */
ScalaScale *scala_load_string(const char *buffer, size_t len);

/**
 * Free a loaded scale.
 *
 * @param scale Scale to free (safe to pass NULL)
 */
void scala_free(ScalaScale *scale);

/**
 * Get the frequency ratio for a scale degree.
 * Degree 0 is always 1.0 (the implicit 1/1 root).
 *
 * @param scale The scale
 * @param degree Degree index (0 to scale->degree_count - 1)
 * @return Frequency ratio, or -1.0 on error
 */
double scala_get_ratio(const ScalaScale *scale, int degree);

/**
 * Get the frequency for a scale degree given a base frequency.
 *
 * @param scale The scale
 * @param degree Degree index
 * @param base_freq Base frequency for degree 0 (e.g., 261.63 for C4)
 * @return Frequency in Hz, or -1.0 on error
 */
double scala_get_frequency(const ScalaScale *scale, int degree, double base_freq);

/**
 * Get frequency for a MIDI note number using the scale.
 * Maps MIDI notes to scale degrees with octave wrapping.
 *
 * @param scale The scale
 * @param midi_note MIDI note number (0-127)
 * @param root_note MIDI note number of scale root (e.g., 60 for C4)
 * @param root_freq Frequency of root note (e.g., 261.63 Hz)
 * @return Frequency in Hz
 */
double scala_midi_to_freq(const ScalaScale *scale, int midi_note,
                          int root_note, double root_freq);

/**
 * Convert cents to frequency ratio.
 *
 * @param cents Cents value (100 cents = 1 semitone)
 * @return Frequency ratio
 */
double scala_cents_to_ratio(double cents);

/**
 * Convert frequency ratio to cents.
 *
 * @param ratio Frequency ratio
 * @return Cents value
 */
double scala_ratio_to_cents(double ratio);

/**
 * Get the number of degrees in the scale (excluding implicit 1/1).
 *
 * @param scale The scale
 * @return Number of degrees, or -1 on error
 */
int scala_get_length(const ScalaScale *scale);

/**
 * Get the scale description.
 *
 * @param scale The scale
 * @return Description string, or NULL on error
 */
const char *scala_get_description(const ScalaScale *scale);

/**
 * Get the last error message.
 *
 * @return Error message string (static buffer, do not free)
 */
const char *scala_get_error(void);

#ifdef __cplusplus
}
#endif

#endif /* ALDA_SCALA_H */
