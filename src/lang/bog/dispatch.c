/**
 * @file dispatch.c
 * @brief Bog language dispatch registration.
 */

#include "lang_dispatch.h"

/* External entry points from repl.c */
extern int bog_repl_main(int argc, char **argv);
extern int bog_play_main(int argc, char **argv);

static const LangDispatchEntry bog_dispatch = {
    .commands = {"bog"},
    .command_count = 1,
    .extensions = {".bog"},
    .extension_count = 1,
    .display_name = "Bog",
    .description = "Prolog-based music live coding",
    .repl_main = bog_repl_main,
    .play_main = bog_play_main,
};

/* Register Bog language - called from lang_dispatch_init() */
void bog_dispatch_init(void) {
    lang_dispatch_register(&bog_dispatch);
}
