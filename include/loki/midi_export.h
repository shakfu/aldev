/* loki/midi_export.h - MIDI file export
 *
 * Provides functionality to export MIDI events to Standard MIDI Files.
 * Uses the midifile library (BSD-2-Clause) for MIDI file generation.
 *
 * Usage:
 *   - From ex-command: :export filename.mid
 *   - From Lua: loki.midi.export("filename.mid")
 *
 * The export system reads from the shared MIDI event buffer
 * (src/shared/midi/events.h). Languages populate this buffer
 * before calling the export function.
 */

#ifndef LOKI_MIDI_EXPORT_H
#define LOKI_MIDI_EXPORT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Export shared MIDI event buffer to a Standard MIDI File.
 *
 * Reads events from shared_midi_events_get() and writes them
 * to a MIDI file. Exports as Type 0 (single track) for single-channel
 * compositions, or Type 1 (multi-track) for multi-channel compositions.
 *
 * @param filename Output filename (should end in .mid)
 * @return 0 on success, -1 on error
 */
int loki_midi_export_shared(const char *filename);

/**
 * Get the last error message from a failed export.
 *
 * @return Error message string, or NULL if no error
 * @note String is valid until next export call
 */
const char *loki_midi_export_error(void);

#ifdef __cplusplus
}
#endif

#endif /* LOKI_MIDI_EXPORT_H */
