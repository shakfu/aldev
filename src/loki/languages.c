/* loki_languages.c - Language syntax infrastructure
 *
 * This file now contains ONLY infrastructure for syntax highlighting.
 * All language definitions have been moved to Lua files in .aldalog/languages/
 *
 * Minimal C keyword arrays are kept ONLY for markdown code block highlighting.
 * For actual C file editing, the full definition loads from .aldalog/languages/c.lua
 */

#include "internal.h"
#include "syntax.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ======================= Minimal Keywords for Markdown ==================== */
/* These are ONLY used for syntax highlighting within markdown code blocks.
 * For editing actual source files, full definitions load from Lua. */

/* Minimal C keywords (for markdown code blocks) */
char *C_HL_keywords[] = {
	"if","else","for","while","return","break","continue","NULL",
	"int|","char|","void|","float|","double|",NULL
};

/* Minimal Python keywords (for markdown code blocks) */
char *Python_HL_keywords[] = {
	"def","class","if","else","elif","for","while","return","import","from",
	"str|","int|","float|","bool|","list|","dict|",NULL
};

/* Minimal Lua keywords (for markdown code blocks) */
char *Lua_HL_keywords[] = {
	"function","if","else","elseif","for","while","return","local","end",
	"string|","number|","boolean|","table|",NULL
};

/* Minimal Cython keywords (for markdown code blocks) */
char *Cython_HL_keywords[] = {
	"cdef","cpdef","def","class","if","else","for","while","return",
	"int|","float|","double|","str|",NULL
};

/* Alda music notation keywords */
char *Alda_HL_keywords[] = {
	/* Common instruments */
	"piano","violin","viola","cello","contrabass","guitar","bass",
	"trumpet","trombone","tuba","french-horn","flute","clarinet",
	"oboe","bassoon","saxophone","alto-sax","tenor-sax",
	"harpsichord","organ","accordion","harmonica",
	"synth","percussion","drums","midi-percussion",
	/* Attributes */
	"tempo","quant","quantize","quantization","vol","volume",
	"track-vol","track-volume","pan","panning","key-sig","key-signature",
	"transpose","octave","voice",
	/* Note names (types - highlighted differently) */
	"c|","d|","e|","f|","g|","a|","b|","r|",
	/* Octave markers */
	"o0|","o1|","o2|","o3|","o4|","o5|","o6|","o7|","o8|","o9|",
	NULL
};

/* Extensions */
char *C_HL_extensions[] = {".c",".h",".cpp",".hpp",".cc",NULL};
char *Python_HL_extensions[] = {".py",".pyw",NULL};
char *Lua_HL_extensions[] = {".lua",NULL};
char *MD_HL_extensions[] = {".md",".markdown",NULL};
char *Alda_HL_extensions[] = {".alda",NULL};

/* ======================= Language Database (MINIMAL) ======================== */
/* Minimal static definitions kept for backward compatibility with tests.
 * Full language definitions load dynamically from Lua (.aldalog/languages/).
 * These entries have minimal keywords suitable for testing and markdown code blocks. */

struct t_editor_syntax HLDB[] = {
    /* C/C++ - minimal definition for tests and markdown */
    {
        C_HL_extensions,
        C_HL_keywords,
        "//","/*","*/",
        ",.()+-/*=~%<>[]{}:;",
        HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS,
        HL_TYPE_C
    },
    /* Python - minimal definition for tests and markdown */
    {
        Python_HL_extensions,
        Python_HL_keywords,
        "#","","",
        ",.()+-/*=~%<>[]{}:;",
        HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS,
        HL_TYPE_C
    },
    /* Lua - minimal definition for tests and markdown */
    {
        Lua_HL_extensions,
        Lua_HL_keywords,
        "--","","",
        ",.()+-/*=~%<>[]{}:;",
        HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS,
        HL_TYPE_C
    },
    /* Markdown - special handling via markdown module */
    {
        MD_HL_extensions,
        NULL,
        "","","",
        ",.()+-/*=~%[]{}:;",
        0,
        HL_TYPE_MARKDOWN
    },
    /* Alda music notation - built-in for REPL syntax highlighting */
    {
        Alda_HL_extensions,
        Alda_HL_keywords,
        "#","","",
        ",.()+-/*=~%[]{}:;<>|",
        HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS,
        HL_TYPE_C
    },
    /* Terminator */
    {NULL, NULL, "", "", "", NULL, 0, HL_TYPE_C}
};

#define HLDB_ENTRIES (sizeof(HLDB)/sizeof(HLDB[0]))

/* Get the number of built-in language entries */
unsigned int loki_get_builtin_language_count(void) {
    return HLDB_ENTRIES;
}

/* ======================= Helper Functions for Markdown ==================== */

/* Helper function to highlight code block content with specified language rules.
 * This is a simplified version of editor_update_syntax for use within markdown. */
void highlight_code_line(t_erow *row, char **keywords, char *scs, char *separators) {
    if (row->rsize == 0) return;

    int i = 0, prev_sep = 1, in_string = 0;
    char *p = row->render;

    while (i < row->rsize) {
        /* Handle // or # comments (if scs is provided) */
        if (scs && scs[0] && prev_sep && i < row->rsize - 1 &&
            p[i] == scs[0] && (scs[1] == '\0' || p[i+1] == scs[1])) {
            memset(row->hl + i, HL_COMMENT, row->rsize - i);
            return;
        }

        /* Handle strings */
        if (in_string) {
            row->hl[i] = HL_STRING;
            if (i < row->rsize - 1 && p[i] == '\\') {
                row->hl[i+1] = HL_STRING;
                i += 2;
                prev_sep = 0;
                continue;
            }
            if (p[i] == in_string) in_string = 0;
            i++;
            prev_sep = 0;
            continue;
        }

        if (p[i] == '"' || p[i] == '\'') {
            in_string = p[i];
            row->hl[i] = HL_STRING;
            i++;
            prev_sep = 0;
            continue;
        }

        /* Handle numbers */
        if ((isdigit(p[i]) && (prev_sep || row->hl[i-1] == HL_NUMBER)) ||
            (p[i] == '.' && i > 0 && row->hl[i-1] == HL_NUMBER)) {
            row->hl[i] = HL_NUMBER;
            i++;
            prev_sep = 0;
            continue;
        }

        /* Handle keywords */
        if (prev_sep && keywords) {
            int j;
            for (j = 0; keywords[j]; j++) {
                int klen = strlen(keywords[j]);
                int kw2 = keywords[j][klen-1] == '|';
                if (kw2) klen--;

                if (!memcmp(p+i,keywords[j],klen) &&
                    (i+klen >= row->rsize || syntax_is_separator(p[i+klen], separators))) {
                    memset(row->hl+i, kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
                    i += klen;
                    prev_sep = 0;
                    goto next;
                }
            }
        }

        prev_sep = syntax_is_separator(p[i], separators);
        i++;
next:
        continue;
    }
}

/* Detect code block language from fence marker (e.g., ```python) */
int detect_code_block_language(const char *line) {
    /* Skip opening fence characters */
    const char *p = line;
    while (*p && (*p == '`' || *p == '~')) p++;

    /* Check language identifier */
    if (!*p) return CB_LANG_NONE;

    if (strncmp(p, "c", 1) == 0 || strncmp(p, "cpp", 3) == 0 ||
        strncmp(p, "C", 1) == 0 || strncmp(p, "C++", 3) == 0) {
        return CB_LANG_C;
    }
    if (strncmp(p, "python", 6) == 0 || strncmp(p, "py", 2) == 0) {
        return CB_LANG_PYTHON;
    }
    if (strncmp(p, "lua", 3) == 0) {
        return CB_LANG_LUA;
    }
    if (strncmp(p, "cython", 6) == 0 || strncmp(p, "pyx", 3) == 0) {
        return CB_LANG_CYTHON;
    }

    return CB_LANG_NONE;
}

/* Update syntax highlighting for markdown files (proper editor integration).
 * This is the main entry point called by the editor core. */
void editor_update_syntax_markdown(editor_ctx_t *ctx, t_erow *row) {
    unsigned char *new_hl = realloc(row->hl, row->rsize);
    if (new_hl == NULL) return;
    row->hl = new_hl;
    memset(row->hl, HL_NORMAL, row->rsize);

    char *p = row->render;
    int i = 0;
    int prev_cb_lang = (row->idx > 0 && ctx && ctx->row) ? ctx->row[row->idx - 1].cb_lang : CB_LANG_NONE;

    /* Code blocks: lines starting with ``` */
    if (row->rsize >= 3 && p[0] == '`' && p[1] == '`' && p[2] == '`') {
        /* Opening or closing code fence */
        memset(row->hl, HL_STRING, row->rsize);

        if (prev_cb_lang != CB_LANG_NONE) {
            /* Closing fence */
            row->cb_lang = CB_LANG_NONE;
        } else {
            /* Opening fence - detect language */
            row->cb_lang = CB_LANG_NONE;
            if (row->rsize > 3) {
                char *lang = p + 3;
                /* Skip whitespace */
                while (*lang && isspace(*lang)) lang++;

                if (strncmp(lang, "cython", 6) == 0 ||
                    strncmp(lang, "pyx", 3) == 0 ||
                    strncmp(lang, "pxd", 3) == 0) {
                    row->cb_lang = CB_LANG_CYTHON;
                } else if (strncmp(lang, "c", 1) == 0 &&
                    (lang[1] == '\0' || isspace(lang[1]) || lang[1] == 'p')) {
                    if (lang[1] == 'p' && lang[2] == 'p') {
                        row->cb_lang = CB_LANG_C; /* C++ */
                    } else if (lang[1] == '\0' || isspace(lang[1])) {
                        row->cb_lang = CB_LANG_C; /* C */
                    }
                } else if (strncmp(lang, "python", 6) == 0 || strncmp(lang, "py", 2) == 0) {
                    row->cb_lang = CB_LANG_PYTHON;
                } else if (strncmp(lang, "lua", 3) == 0) {
                    row->cb_lang = CB_LANG_LUA;
                }
            }
        }
        return;
    }

    /* Inside code block - apply language-specific highlighting */
    if (prev_cb_lang != CB_LANG_NONE) {
        row->cb_lang = prev_cb_lang;

        char **keywords = NULL;
        char *scs = NULL;
        char *separators = ",.()+-/*=~%[];";

        switch (prev_cb_lang) {
            case CB_LANG_C:
                keywords = C_HL_keywords;
                scs = "//";
                break;
            case CB_LANG_PYTHON:
                keywords = Python_HL_keywords;
                scs = "#";
                break;
            case CB_LANG_LUA:
                keywords = Lua_HL_keywords;
                scs = "--";
                break;
            case CB_LANG_CYTHON:
                keywords = Cython_HL_keywords;
                scs = "#";
                break;
        }

        highlight_code_line(row, keywords, scs, separators);
        return;
    }

    /* Not in code block - reset */
    row->cb_lang = CB_LANG_NONE;

    /* Headers: # ## ### etc. at start of line */
    if (row->rsize > 0 && p[0] == '#') {
        int header_len = 0;
        while (header_len < row->rsize && p[header_len] == '#')
            header_len++;
        if (header_len < row->rsize && (p[header_len] == ' ' || p[header_len] == '\t')) {
            /* Valid header - highlight entire line */
            memset(row->hl, HL_KEYWORD1, row->rsize);
            return;
        }
    }

    /* Lists: lines starting with *, -, or + followed by space */
    if (row->rsize >= 2 && (p[0] == '*' || p[0] == '-' || p[0] == '+') &&
        (p[1] == ' ' || p[1] == '\t')) {
        row->hl[0] = HL_KEYWORD2;
    }

    /* Inline patterns: bold, italic, code, links */
    i = 0;
    while (i < row->rsize) {
        /* Inline code: `text` */
        if (p[i] == '`') {
            row->hl[i] = HL_STRING;
            i++;
            while (i < row->rsize && p[i] != '`') {
                row->hl[i] = HL_STRING;
                i++;
            }
            if (i < row->rsize) {
                row->hl[i] = HL_STRING; /* Closing ` */
                i++;
            }
            continue;
        }

        /* Bold: **text** */
        if (i < row->rsize - 1 && p[i] == '*' && p[i+1] == '*') {
            int start = i;
            i += 2;
            while (i < row->rsize - 1) {
                if (p[i] == '*' && p[i+1] == '*') {
                    /* Found closing ** */
                    memset(row->hl + start, HL_KEYWORD2, i - start + 2);
                    i += 2;
                    break;
                }
                i++;
            }
            continue;
        }

        /* Italic: *text* or _text_ */
        if (p[i] == '*' || p[i] == '_') {
            char marker = p[i];
            int start = i;
            i++;
            while (i < row->rsize) {
                if (p[i] == marker) {
                    /* Found closing marker */
                    memset(row->hl + start, HL_COMMENT, i - start + 1);
                    i++;
                    break;
                }
                i++;
            }
            continue;
        }

        /* Links: [text](url) */
        if (p[i] == '[') {
            int start = i;
            i++;
            /* Find closing ] */
            while (i < row->rsize && p[i] != ']') i++;
            if (i < row->rsize && i + 1 < row->rsize && p[i+1] == '(') {
                /* Found ]( - continue to find ) */
                i += 2;
                while (i < row->rsize && p[i] != ')') i++;
                if (i < row->rsize) {
                    /* Complete link found */
                    memset(row->hl + start, HL_NUMBER, i - start + 1);
                    i++;
                    continue;
                }
            }
            i = start + 1; /* Not a link, continue from next char */
            continue;
        }

        i++;
    }
}

/* ======================= Dynamic Language Registration =================== */

/* Dynamic language registry for user-defined languages */
static struct t_editor_syntax **HLDB_dynamic = NULL;
static int HLDB_dynamic_count = 0;

/* Free a single dynamically allocated language definition */
void free_dynamic_language(struct t_editor_syntax *lang) {
    if (!lang) return;

    /* Free filematch array */
    if (lang->filematch) {
        for (int i = 0; lang->filematch[i]; i++) {
            free(lang->filematch[i]);
        }
        free(lang->filematch);
    }

    /* Free keywords array */
    if (lang->keywords) {
        for (int i = 0; lang->keywords[i]; i++) {
            free(lang->keywords[i]);
        }
        free(lang->keywords);
    }

    /* Free separators string */
    if (lang->separators) {
        free(lang->separators);
    }

    free(lang);
}

/* Free all dynamically allocated languages (called at exit) */
void cleanup_dynamic_languages(void) {
    for (int i = 0; i < HLDB_dynamic_count; i++) {
        free_dynamic_language(HLDB_dynamic[i]);
    }
    free(HLDB_dynamic);
    HLDB_dynamic = NULL;
    HLDB_dynamic_count = 0;
}

/* Add a new language definition dynamically
 * Returns 0 on success, -1 on error */
int add_dynamic_language(struct t_editor_syntax *lang) {
    if (!lang) return -1;

    /* Grow the dynamic array */
    struct t_editor_syntax **new_array = realloc(HLDB_dynamic,
        sizeof(struct t_editor_syntax*) * (HLDB_dynamic_count + 1));
    if (!new_array) {
        return -1;  /* Allocation failed */
    }

    HLDB_dynamic = new_array;
    HLDB_dynamic[HLDB_dynamic_count] = lang;
    HLDB_dynamic_count++;

    return 0;
}

/* Get dynamic language by index (for iteration)
 * Returns NULL if index out of bounds */
struct t_editor_syntax *get_dynamic_language(int index) {
    if (index < 0 || index >= HLDB_dynamic_count) {
        return NULL;
    }
    return HLDB_dynamic[index];
}

/* Get count of dynamic languages */
int get_dynamic_language_count(void) {
    return HLDB_dynamic_count;
}

/* ======================= Note ============================================== */
/*
 * Language Definition System:
 *
 * This file maintains MINIMAL static definitions for backward compatibility:
 *   - C/C++ (HLDB[0])    - Minimal keywords for tests and markdown code blocks
 *   - Python (HLDB[1])   - Minimal keywords for tests and markdown code blocks
 *   - Lua (HLDB[2])      - Minimal keywords for tests and markdown code blocks
 *   - Markdown (HLDB[3]) - Special handling via editor_update_syntax_markdown()
 *
 * FULL language definitions are loaded dynamically from Lua:
 *   .aldalog/languages/c.lua          - C/C++ (full keyword set, all extensions)
 *   .aldalog/languages/python.lua     - Python (full keyword set, all builtins)
 *   .aldalog/languages/lua.lua        - Lua (full keyword set, all builtins)
 *   .aldalog/languages/cython.lua     - Cython
 *   .aldalog/languages/javascript.lua - JavaScript
 *   .aldalog/languages/typescript.lua - TypeScript
 *   .aldalog/languages/rust.lua       - Rust
 *   .aldalog/languages/go.lua         - Go
 *   .aldalog/languages/java.lua       - Java
 *   .aldalog/languages/swift.lua      - Swift
 *   .aldalog/languages/sql.lua        - SQL
 *   .aldalog/languages/shell.lua      - Shell scripts
 *   .aldalog/languages/markdown.lua   - Markdown
 *
 * When opening a file:
 *   1. Editor checks static HLDB for matching extension
 *   2. If found in HLDB, uses minimal static definition
 *   3. Lua module (.aldalog/modules/languages.lua) can override with full definition
 *   4. Languages are loaded on-demand (lazy loading) when needed
 *
 * Benefits of this approach:
 *   - Backward compatibility: Tests and C code can use HLDB directly
 *   - Extensibility: Users can add new languages via Lua without recompiling
 *   - Performance: Only loads language definitions when actually needed
 *   - Simplicity: Minimal C code, maximum flexibility via Lua
 */
