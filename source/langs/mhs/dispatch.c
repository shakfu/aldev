/**
 * @file dispatch.c
 * @brief MHS (Micro Haskell MIDI) language dispatch registration.
 *
 * Registers MHS for command-line dispatch (REPL and play modes).
 */

#include "lang_dispatch.h"

/* External entry points from repl.c */
extern int mhs_repl_main(int argc, char **argv);
extern int mhs_play_main(int argc, char **argv);

static const LangDispatchEntry mhs_dispatch = {
    .commands = {"mhs", "haskell"},
    .command_count = 2,
    .extensions = {".hs", ".mhs"},
    .extension_count = 2,
    .display_name = "MHS",
    .description = "Micro Haskell MIDI language",
    .repl_main = mhs_repl_main,
    .play_main = mhs_play_main,
};

/* Register MHS language - called from lang_dispatch_init() */
void mhs_dispatch_init(void) {
    lang_dispatch_register(&mhs_dispatch);
}
