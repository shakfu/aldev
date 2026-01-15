/**
 * @file test_csound_microtuning.c
 * @brief Manual test for Csound microtuning playback.
 *
 * This test loads a Csound instrument and plays notes using
 * just intonation tuning. Run it to hear the difference.
 *
 * Build: make test_csound_microtuning
 * Run:   ./tests/alda/test_csound_microtuning
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <alda/alda.h>
#include <alda/context.h>
#include <alda/interpreter.h>
#include <alda/scala.h>
#include <alda/csound_backend.h>

#define TEST_DATA_DIR "../tests/alda/data"
#define JUST_12_SCL TEST_DATA_DIR "/just_12.scl"
#define MICROTUNING_CSD TEST_DATA_DIR "/microtuning_test.csd"

int main(int argc, char **argv) {
    printf("=== Csound Microtuning Test ===\n\n");

#ifndef BUILD_CSOUND_BACKEND
    printf("ERROR: Csound backend not compiled in.\n");
    printf("Rebuild with: cmake .. -DBUILD_CSOUND_BACKEND=ON && make\n");
    return 1;
#else

    /* Initialize Csound */
    printf("Initializing Csound...\n");
    int result = alda_csound_init();
    if (result != 0) {
        printf("ERROR: Failed to initialize Csound\n");
        return 1;
    }

    /* Load Csound instrument */
    printf("Loading Csound instrument: %s\n", MICROTUNING_CSD);
    result = alda_csound_load_csd(MICROTUNING_CSD);
    if (result != 0) {
        printf("ERROR: Failed to load Csound instrument\n");
        alda_csound_cleanup();
        return 1;
    }
    printf("Csound initialized successfully\n\n");

    /* Load the just intonation scale */
    printf("Loading scale: %s\n", JUST_12_SCL);
    ScalaScale *scale = scala_load(JUST_12_SCL);
    if (!scale) {
        printf("ERROR: Failed to load scale: %s\n", scala_get_error());
        alda_csound_cleanup();
        return 1;
    }
    printf("Scale loaded: %s\n", scala_get_description(scale));
    printf("Scale length: %d degrees\n\n", scala_get_length(scale));

    /* Calculate frequencies for C major chord */
    double root_freq = 261.6255653;  /* C4 */
    int root_note = 60;

    double freq_c4 = scala_midi_to_freq(scale, 60, root_note, root_freq);
    double freq_e4 = scala_midi_to_freq(scale, 64, root_note, root_freq);
    double freq_g4 = scala_midi_to_freq(scale, 67, root_note, root_freq);

    printf("Just Intonation C Major Chord:\n");
    printf("  C4 (MIDI 60): %.2f Hz (ratio 1/1)\n", freq_c4);
    printf("  E4 (MIDI 64): %.2f Hz (ratio 5/4 = 1.25)\n", freq_e4);
    printf("  G4 (MIDI 67): %.2f Hz (ratio 3/2 = 1.5)\n", freq_g4);
    printf("\n");

    /* Compare with 12-TET */
    double tet_e4 = root_freq * 1.259921;  /* 2^(4/12) */
    double tet_g4 = root_freq * 1.498307;  /* 2^(7/12) */
    printf("12-TET comparison:\n");
    printf("  E4 12-TET: %.2f Hz (just E4 is %.1f cents flatter)\n",
           tet_e4, 1200.0 * (log(tet_e4/freq_e4) / log(2.0)));
    printf("  G4 12-TET: %.2f Hz (just G4 is %.1f cents sharper)\n",
           tet_g4, 1200.0 * (log(freq_g4/tet_g4) / log(2.0)));
    printf("\n");

    /* Play chord with frequency-based Csound backend */
    printf("Playing C major chord in Just Intonation...\n");
    printf("(Listen for the pure, beatless thirds and fifths)\n\n");

    int velocity = 80;

    /* Play chord - send note-on with frequency */
    alda_csound_send_note_on_freq(1, freq_c4, velocity, 60);
    alda_csound_send_note_on_freq(1, freq_e4, velocity, 64);
    alda_csound_send_note_on_freq(1, freq_g4, velocity, 67);

    /* Let it ring for 3 seconds */
    sleep(3);

    /* Stop notes */
    alda_csound_send_note_off(1, 60);
    alda_csound_send_note_off(1, 64);
    alda_csound_send_note_off(1, 67);

    printf("Playing arpeggio...\n");
    sleep(1);

    /* Play arpeggio */
    double freq_c5 = scala_midi_to_freq(scale, 72, root_note, root_freq);

    alda_csound_send_note_on_freq(1, freq_c4, velocity, 60);
    usleep(400000);
    alda_csound_send_note_off(1, 60);

    alda_csound_send_note_on_freq(1, freq_e4, velocity, 64);
    usleep(400000);
    alda_csound_send_note_off(1, 64);

    alda_csound_send_note_on_freq(1, freq_g4, velocity, 67);
    usleep(400000);
    alda_csound_send_note_off(1, 67);

    alda_csound_send_note_on_freq(1, freq_c5, velocity, 72);
    usleep(800000);
    alda_csound_send_note_off(1, 72);

    sleep(1);

    printf("\n=== Test Complete ===\n");

    /* Cleanup */
    scala_free(scale);
    alda_csound_cleanup();

    return 0;
#endif
}
