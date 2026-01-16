/* lang_python.h - Python language definition for syntax highlighting */

#ifndef LOKI_LANG_PYTHON_H
#define LOKI_LANG_PYTHON_H

/* Minimal Python keywords (for markdown code blocks) */
static char *Python_HL_keywords[] = {
    "def","class","if","else","elif","for","while","return","import","from",
    "str|","int|","float|","bool|","list|","dict|",NULL
};

static char *Python_HL_extensions[] = {".py",".pyw",NULL};

/* Minimal Cython keywords (for markdown code blocks) */
static char *Cython_HL_keywords[] = {
    "cdef","cpdef","def","class","if","else","for","while","return",
    "int|","float|","double|","str|",NULL
};

#endif /* LOKI_LANG_PYTHON_H */
