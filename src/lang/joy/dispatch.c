/**
 * @file dispatch.c
 * @brief Joy language dispatch registration.
 */

#include "lang_dispatch.h"

/* External entry points from repl.c */
extern int joy_repl_main(int argc, char **argv);
extern int joy_play_main(int argc, char **argv);

static const LangDispatchEntry joy_dispatch = {
    .commands = {"joy"},
    .command_count = 1,
    .extensions = {".joy"},
    .extension_count = 1,
    .display_name = "Joy",
    .description = "Concatenative stack-based music language",
    .repl_main = joy_repl_main,
    .play_main = joy_play_main,
};

/* Register at program startup */
__attribute__((constructor))
static void joy_dispatch_init(void) {
    lang_dispatch_register(&joy_dispatch);
}
