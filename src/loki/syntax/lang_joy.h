/* lang_joy.h - Joy concatenative language definition */

#ifndef LOKI_LANG_JOY_H
#define LOKI_LANG_JOY_H

/* Joy concatenative language keywords */
static char *Joy_HL_keywords[] = {
    /* Stack operations */
    "dup","swap","pop","drop","dip","i","x","unit","cons","uncons",
    "first","rest","second","third","size","null","small",
    /* Combinators */
    "map","fold","filter","each","ifte","branch","cond","while","times",
    "linrec","binrec","primrec","genrec",
    /* Arithmetic */
    "add","sub","mul","div","mod","neg","abs","sign",
    /* Comparison */
    "eq","ne","lt","gt","le","ge","and","or","not",
    /* MIDI/Music primitives */
    "note","chord","seq","rest","play","stop","panic",
    "midi-port","midi-virtual","midi-list","midi-channel",
    "tempo","volume","velocity","octave","transpose",
    "program","cc","pitchbend",
    /* Definition */
    "DEFINE",
    /* Note names (types - highlighted differently) */
    "c|","d|","e|","f|","g|","a|","b|","r|",
    NULL
};

static char *Joy_HL_extensions[] = {".joy",NULL};

#endif /* LOKI_LANG_JOY_H */
