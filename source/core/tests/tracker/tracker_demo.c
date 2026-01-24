/**
 * tracker_demo.c - Interactive demo of the tracker terminal UI
 *
 * Run with: ./tracker_demo [soundfont.sf2]
 *
 * Controls:
 *   h/j/k/l or arrows - Navigate
 *   i or Enter        - Edit cell
 *   Escape            - Exit edit mode / Quit
 *   Space             - Play/Stop
 *   q                 - Quit
 */

#include "tracker_view_terminal.h"
#include "tracker_audio.h"
#include "tracker_plugin_notes.h"
#include "context.h"
#include "audio/audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Default soundfont path (relative to build directory) */
#define DEFAULT_SOUNDFONT "../source/thirdparty/TinySoundFont/examples/florestan-subset.sf2"

int main(int argc, char** argv) {
    const char* soundfont_path = DEFAULT_SOUNDFONT;

    /* Parse command line */
    if (argc > 1) {
        soundfont_path = argv[1];
    }

    /* Initialize shared audio context */
    SharedContext audio_ctx;
    if (shared_context_init(&audio_ctx) != 0) {
        fprintf(stderr, "Failed to initialize audio context\n");
        return 1;
    }

    /* Initialize TinySoundFont backend */
    if (shared_tsf_init() != 0) {
        fprintf(stderr, "Failed to initialize TinySoundFont\n");
        shared_context_cleanup(&audio_ctx);
        return 1;
    }

    /* Load soundfont */
    if (shared_tsf_load_soundfont(soundfont_path) != 0) {
        fprintf(stderr, "Failed to load soundfont: %s\n", soundfont_path);
        fprintf(stderr, "Run with: ./tracker_demo path/to/soundfont.sf2\n");
        shared_tsf_cleanup();
        shared_context_cleanup(&audio_ctx);
        return 1;
    }

    /* Enable TSF audio output (starts the audio stream) */
    if (shared_tsf_enable() != 0) {
        fprintf(stderr, "Failed to enable TinySoundFont audio\n");
        shared_tsf_cleanup();
        shared_context_cleanup(&audio_ctx);
        return 1;
    }

    /* Enable the built-in synth in context */
    audio_ctx.builtin_synth_enabled = 1;
    audio_ctx.tempo = 120;

    printf("Loaded soundfont: %s\n", soundfont_path);
    printf("Starting tracker demo... Press 'q' or Escape to quit.\n");

    /* Register plugins */
    tracker_plugin_notes_register();

    /* Create a demo song */
    TrackerSong* song = tracker_song_new("Demo Song");
    if (!song) {
        fprintf(stderr, "Failed to create song\n");
        shared_tsf_cleanup();
        shared_context_cleanup(&audio_ctx);
        return 1;
    }

    song->bpm = 120;
    song->rows_per_beat = 4;
    song->ticks_per_row = 6;

    /* Create a pattern with 4 tracks, 16 rows */
    TrackerPattern* pattern = tracker_pattern_new(16, 4, "Pattern 1");
    if (!pattern) {
        fprintf(stderr, "Failed to create pattern\n");
        tracker_song_free(song);
        shared_tsf_cleanup();
        shared_context_cleanup(&audio_ctx);
        return 1;
    }

    /* Name the tracks */
    if (pattern->tracks[0].name) free(pattern->tracks[0].name);
    pattern->tracks[0].name = strdup("Lead");
    pattern->tracks[0].default_channel = 1;

    if (pattern->tracks[1].name) free(pattern->tracks[1].name);
    pattern->tracks[1].name = strdup("Bass");
    pattern->tracks[1].default_channel = 2;

    if (pattern->tracks[2].name) free(pattern->tracks[2].name);
    pattern->tracks[2].name = strdup("Drums");
    pattern->tracks[2].default_channel = 10;

    if (pattern->tracks[3].name) free(pattern->tracks[3].name);
    pattern->tracks[3].name = strdup("Pad");
    pattern->tracks[3].default_channel = 3;

    /* Add some demo content */
    /* Note: tracker_pattern_get_cell(pattern, row, track) */
    TrackerCell* cell;

    /* Lead melody (track 0) */
    cell = tracker_pattern_get_cell(pattern, 0, 0);
    tracker_cell_set_expression(cell, "C4@80", "notes");

    cell = tracker_pattern_get_cell(pattern, 2, 0);
    tracker_cell_set_expression(cell, "D4@80", "notes");

    cell = tracker_pattern_get_cell(pattern, 4, 0);
    tracker_cell_set_expression(cell, "E4@80", "notes");

    cell = tracker_pattern_get_cell(pattern, 6, 0);
    tracker_cell_set_expression(cell, "F4@80", "notes");

    cell = tracker_pattern_get_cell(pattern, 8, 0);
    tracker_cell_set_expression(cell, "G4@80", "notes");

    cell = tracker_pattern_get_cell(pattern, 10, 0);
    tracker_cell_set_expression(cell, "A4@80", "notes");

    cell = tracker_pattern_get_cell(pattern, 12, 0);
    tracker_cell_set_expression(cell, "B4@80", "notes");

    cell = tracker_pattern_get_cell(pattern, 14, 0);
    tracker_cell_set_expression(cell, "C5@80", "notes");

    /* Bass line (track 1) */
    cell = tracker_pattern_get_cell(pattern, 0, 1);
    tracker_cell_set_expression(cell, "C2@60", "notes");

    cell = tracker_pattern_get_cell(pattern, 4, 1);
    tracker_cell_set_expression(cell, "G2@60", "notes");

    cell = tracker_pattern_get_cell(pattern, 8, 1);
    tracker_cell_set_expression(cell, "A2@60", "notes");

    cell = tracker_pattern_get_cell(pattern, 12, 1);
    tracker_cell_set_expression(cell, "F2@60", "notes");

    /* Drums (track 2) - C1 = MIDI note 24, D1 = 26 */
    for (int row = 0; row < 16; row += 4) {
        cell = tracker_pattern_get_cell(pattern, row, 2);
        tracker_cell_set_expression(cell, "C1@100", "notes");  /* kick */
    }
    for (int row = 4; row < 16; row += 8) {
        cell = tracker_pattern_get_cell(pattern, row, 2);
        tracker_cell_set_expression(cell, "D1@90", "notes");  /* snare */
    }

    /* Add pattern to song */
    tracker_song_add_pattern(song, pattern);

    /* Create engine and connect to audio */
    TrackerEngine* engine = tracker_audio_engine_new(&audio_ctx);
    if (!engine) {
        fprintf(stderr, "Failed to create engine\n");
        tracker_song_free(song);
        shared_tsf_cleanup();
        shared_context_cleanup(&audio_ctx);
        return 1;
    }

    tracker_engine_load_song(engine, song);
    tracker_engine_set_bpm(engine, song->bpm);

    /* Create terminal view */
    TrackerView* view = tracker_view_terminal_new();
    if (!view) {
        fprintf(stderr, "Failed to create terminal view\n");
        tracker_engine_free(engine);
        tracker_song_free(song);
        shared_tsf_cleanup();
        shared_context_cleanup(&audio_ctx);
        return 1;
    }

    /* Set up default theme */
    TrackerTheme theme;
    tracker_theme_init_default(&theme);
    tracker_view_set_theme(view, &theme, false);

    /* Attach song and engine */
    tracker_view_attach(view, song, engine);

    /* Set up view state */
    view->state.show_row_numbers = true;
    view->state.show_track_headers = true;
    view->state.highlight_beat_rows = true;
    view->state.beat_highlight_interval = 4;

    /* Run the view */
    tracker_view_run(view, 30);  /* 30 FPS */

    /* Cleanup */
    tracker_view_free(view);
    tracker_audio_disconnect(engine);
    tracker_engine_free(engine);
    tracker_song_free(song);
    shared_tsf_disable();
    shared_tsf_cleanup();
    shared_context_cleanup(&audio_ctx);

    return 0;
}
