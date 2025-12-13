#pragma once

#include "ccngen/enum.h"
#include "release_assert.h"
#include <stdbool.h>
#include <stdint.h>

#include <stdarg.h>
#include <stdio.h>

static inline void scanparse_fprintf(FILE *stream, const char *format, ...)
{
#ifdef DEBUG_SCANPARSE
    va_list args;
    va_start(args, format);
    fprintf(stream, format, args);
#else
    (void)stream;
    (void)format;
#endif
}

static inline uint64_t nodessettype_to_nodetypes(enum nodesettype type)
{
    uint64_t one = 1;
    switch (type)
    {
    case NS_LINK:
        return nodessettype_to_nodetypes(NS_EXPR) | nodessettype_to_nodetypes(NS_STATEMENT);
    case NS_EXPROPTARRAYINIT:
        return nodessettype_to_nodetypes(NS_EXPR) | (one << NT_ARRAYINIT);
    case NS_VAROPTARRAYEXPR:
        return (one << NT_VAR) | (one << NT_ARRAYEXPR);
    case NS_VAROPTARRAYVAR:
        return (one << NT_VAR) | (one << NT_ARRAYVAR);
    case NS_DECLARATION:
        return (one << NT_FUNDEC) | (one << NT_FUNDEF) | (one << NT_GLOBALDEC) |
               (one << NT_GLOBALDEF);
    case NS_STATEMENT:
        return (one << NT_ASSIGN) | (one << NT_PROCCALL) | (one << NT_IFSTATEMENT) |
               (one << NT_WHILELOOP) | (one << NT_DOWHILELOOP) | (one << NT_FORLOOP) |
               (one << NT_FORLOOP) | (one << NT_RETSTATEMENT) | (one << NT_ARRAYASSIGN);
    case NS_EXPR:
        return (one << NT_BINOP) | (one << NT_MONOP) | (one << NT_CAST) | (one << NT_PROCCALL) |
               (one << NT_VAR) | (one << NT_ARRAYEXPR) | (one << NT_INT) | (one << NT_FLOAT) |
               (one << NT_BOOL);
    case NS_NULL:
        return 0;
    case _NS_SIZE:
        // Not Convertible
        release_assert(false);
    }

    // Undefined type
    release_assert(false);
}
