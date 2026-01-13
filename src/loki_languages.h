/* loki_languages.h - Built-in language syntax definitions
 *
 * This header declares the built-in language syntax highlighting definitions.
 * The actual definitions are in loki_languages.c.
 */

#ifndef LOKI_LANGUAGES_H
#define LOKI_LANGUAGES_H

#include "loki_internal.h"

/* Language keyword and extension arrays */
extern char *C_HL_extensions[];
extern char *C_HL_keywords[];
extern char *Python_HL_extensions[];
extern char *Python_HL_keywords[];
extern char *Lua_HL_extensions[];
extern char *Lua_HL_keywords[];
extern char *Cython_HL_extensions[];
extern char *Cython_HL_keywords[];
extern char *MD_HL_extensions[];

/* Language database - array of all built-in language definitions */
extern struct t_editor_syntax HLDB[];

/* Get the number of built-in language entries */
unsigned int loki_get_builtin_language_count(void);

/* Helper macro for getting HLDB size (for backwards compatibility) */
#define HLDB_ENTRIES loki_get_builtin_language_count()

/* Syntax highlighting functions */
void highlight_code_line(t_erow *row, char **keywords, char *scs, char *separators);
void editor_update_syntax_markdown(editor_ctx_t *ctx, t_erow *row);

/* Dynamic language registration */
int add_dynamic_language(struct t_editor_syntax *lang);
void free_dynamic_language(struct t_editor_syntax *lang);
void cleanup_dynamic_languages(void);
struct t_editor_syntax *get_dynamic_language(int index);
int get_dynamic_language_count(void);

#endif /* LOKI_LANGUAGES_H */
