#ifndef CPROLOG_BUILTINS_H
#define CPROLOG_BUILTINS_H

#include "bog.h"

/* Factory for builtin predicate table */
BogBuiltins* bog_create_builtins(BogArena* arena);

/* Lookup helper used by the resolver */
const BogBuiltin* bog_find_builtin(const BogBuiltins* builtins,
                                           const char* name);

#endif /* CPROLOG_BUILTINS_H */
