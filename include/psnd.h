/**
 * @file psnd.h
 * @brief Central constants for the psnd project.
 *
 * This header provides program-wide constants that should be used instead of
 * hardcoded strings throughout the codebase. Module-specific limits remain
 * in their respective headers.
 */

#ifndef PSND_H
#define PSND_H

/* Program identity */
#define PSND_NAME           "psnd"
#define PSND_VERSION        "0.1.2"

/* Configuration */
#define PSND_CONFIG_DIR     ".psnd"

/* Default MIDI port name for virtual ports */
#define PSND_MIDI_PORT_NAME "PSND_MIDI"

#endif /* PSND_H */
