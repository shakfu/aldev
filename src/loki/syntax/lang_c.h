/* lang_c.h - C/C++ language definition for syntax highlighting */

#ifndef LOKI_LANG_C_H
#define LOKI_LANG_C_H

/* Minimal C keywords (for markdown code blocks) */
static char *C_HL_keywords[] = {
    "if","else","for","while","return","break","continue","NULL",
    "int|","char|","void|","float|","double|",NULL
};

static char *C_HL_extensions[] = {".c",".h",".cpp",".hpp",".cc",NULL};

#endif /* LOKI_LANG_C_H */
