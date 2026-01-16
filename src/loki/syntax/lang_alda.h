/* lang_alda.h - Alda music notation language definition */

#ifndef LOKI_LANG_ALDA_H
#define LOKI_LANG_ALDA_H

/* Alda music notation keywords */
static char *Alda_HL_keywords[] = {
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

static char *Alda_HL_extensions[] = {".alda",NULL};

#endif /* LOKI_LANG_ALDA_H */
