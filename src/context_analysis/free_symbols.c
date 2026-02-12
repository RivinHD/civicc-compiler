#include "ccngen/ast.h"
#include "user_types.h"
#include "utils.h"
#include <ccn/dynamic_core.h>
#include <stdbool.h>
#include <string.h>

node_st *FSprogram(node_st *node)
{
    htable_stptr symbols = PROGRAM_SYMBOLS(node);
    if (symbols != NULL)
    {
        free_symbols(symbols);
        PROGRAM_SYMBOLS(node) = NULL;
    }
    TRAVopt(PROGRAM_DECLS(node));
    return node;
}

node_st *FSfundef(node_st *node)
{
    htable_stptr symbols = FUNDEF_SYMBOLS(node);
    if (symbols != NULL)
    {
        free_symbols(symbols);
        FUNDEF_SYMBOLS(node) = NULL;
    }

    TRAVopt(FUNDEF_FUNBODY(node));
    return node;
}
