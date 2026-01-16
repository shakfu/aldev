/* lang_scheme.h - Scheme R7RS language definition */

#ifndef LOKI_LANG_SCHEME_H
#define LOKI_LANG_SCHEME_H

/* Scheme R7RS keywords */
static char *Scheme_HL_keywords[] = {
    /* Special forms */
    "define","define-syntax","define-library","define-record-type",
    "lambda","let","let*","letrec","letrec*","let-values","let*-values",
    "if","cond","case","else","when","unless",
    "and","or","not","begin","do","set!",
    "quote","quasiquote","unquote","unquote-splicing",
    "import","export","include","syntax-rules",
    /* Common functions */
    "apply","map","for-each","filter","fold","append","reverse",
    "list","cons","car","cdr","cadr","caddr","length",
    "display","newline","write","read","load","eval",
    /* TR7 music primitives */
    "play-note","note-on","note-off","play-chord",
    "set-tempo","set-octave","set-velocity","set-channel",
    "tempo","octave","velocity","channel",
    "midi-list","midi-open","midi-virtual","midi-panic",
    "tsf-load","sleep-ms","program-change","control-change",
    /* Type predicates (highlighted as types) */
    "null?|","pair?|","list?|","symbol?|","number?|","string?|",
    "boolean?|","procedure?|","vector?|","zero?|","positive?|","negative?|",
    NULL
};

static char *Scheme_HL_extensions[] = {".scm",".ss",".scheme",".sld",NULL};

#endif /* LOKI_LANG_SCHEME_H */
