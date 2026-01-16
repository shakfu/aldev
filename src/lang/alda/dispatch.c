/**
 * @file dispatch.c
 * @brief Alda language dispatch registration.
 */

#include "lang_dispatch.h"

/* External entry points from repl.c */
extern int alda_repl_main(int argc, char **argv);
extern int alda_play_main(int argc, char **argv);

static const LangDispatchEntry alda_dispatch = {
    .commands = {"alda"},
    .command_count = 1,
    .extensions = {".alda"},
    .extension_count = 1,
    .display_name = "Alda",
    .description = "Music notation language",
    .repl_main = alda_repl_main,
    .play_main = alda_play_main,
};

/* Register Alda language - called from lang_dispatch_init() */
void alda_dispatch_init(void) {
    lang_dispatch_register(&alda_dispatch);
}
