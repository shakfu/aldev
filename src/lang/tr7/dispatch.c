/**
 * @file dispatch.c
 * @brief TR7 Scheme language dispatch registration.
 */

#include "lang_dispatch.h"

/* External entry points from repl.c */
extern int tr7_repl_main(int argc, char **argv);
extern int tr7_play_main(int argc, char **argv);

static const LangDispatchEntry tr7_dispatch = {
    .commands = {"tr7", "scheme"},
    .command_count = 2,
    .extensions = {".scm", ".ss", ".scheme"},
    .extension_count = 3,
    .display_name = "TR7",
    .description = "R7RS-small Scheme with music extensions",
    .repl_main = tr7_repl_main,
    .play_main = tr7_play_main,
};

/* Register at program startup */
__attribute__((constructor))
static void tr7_dispatch_init(void) {
    lang_dispatch_register(&tr7_dispatch);
}
