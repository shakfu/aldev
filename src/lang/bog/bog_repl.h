/**
 * @file bog_repl.h
 * @brief Bog REPL - Interactive Prolog-based music live coding environment.
 */

#ifndef BOG_BOG_REPL_H
#define BOG_BOG_REPL_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Bog REPL main entry point.
 *
 * Entry point for "psnd bog" command.
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @return Exit code (0 on success)
 */
int bog_repl_main(int argc, char **argv);

/**
 * @brief Bog play main entry point.
 *
 * Entry point for "psnd play file.bog" command.
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @return Exit code (0 on success)
 */
int bog_play_main(int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* BOG_BOG_REPL_H */
