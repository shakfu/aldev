/* lang_csound.h - Csound language definition for syntax highlighting */

#ifndef LOKI_LANG_CSOUND_H
#define LOKI_LANG_CSOUND_H

/* Csound orchestra keywords (from csound_orcparse.h token definitions) */
static char *Csound_HL_keywords[] = {
    /* Control flow */
    "if","then","ithen","kthen","elseif","else","endif","fi",
    "while","do","od","endwhile","until","goto","igoto","kgoto",
    /* Structure */
    "instr","endin","opcode","endop",
    /* Header variables (types - highlighted differently) */
    "sr|","kr|","ksmps|","nchnls|","nchnls_i|","0dbfs|","A4|",
    /* Common opcodes (subset - there are thousands) */
    "oscili","oscil","poscil","vco2","vco","lfo",
    "moogladder","moogvcf","lowpass2","butterlp","butterhp","butterbp",
    "noise","rand","random","rnd","birnd",
    "linen","linenr","linseg","linsegr","expseg","expsegr","expon",
    "madsr","adsr","mxadsr","xadsr",
    "pluck","wgbow","wgflute","wgclar","wgbrass",
    "reverb","freeverb","reverb2","nreverb",
    "delay","delayr","delayw","deltap","deltapi","deltapn",
    "chnget","chnset","chnexport","chnclear",
    "in","ins","inch","out","outs","outch","outh","outq",
    "cpsmidinn","cpspch","octpch","pchmidi","cpsmidi","ampmidi",
    "tablei","table","tablew","ftgen","ftgentmp",
    "init","=",
    "print","prints","printks","printf",
    "xin","xout","setksmps",
    "sprintf","strcat","strcmp","strlen",
    NULL
};

static char *Csound_HL_extensions[] = {".csd",".orc",".sco",NULL};

#endif /* LOKI_LANG_CSOUND_H */
