#include "ccngen/ast.h"
#include "palm/hash_table.h"
#include "palm/str.h"
#include "user_types.h"
#include <ccn/dynamic_core.h>
#include <stdbool.h>
#include <string.h>

node_st *FSprogram(node_st *node)
{
    htable_stptr symbols = PROGRAM_SYMBOLS(node);
    if (symbols != NULL)
    {
        for (htable_iter_st *iter = HTiterate(symbols); iter; iter = HTiterateNext(iter))
        {
            char *key = HTiterKey(iter);
            if (STRprefix("@fun_", key))
            {
                // Free function keys
                free(key);
            }
        }

        HTdelete(symbols);
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
        for (htable_iter_st *iter = HTiterate(symbols); iter; iter = HTiterateNext(iter))
        {
            char *key = HTiterKey(iter);
            if (STRprefix("@fun_", key))
            {
                // Free function keys
                free(key);
            }
        }

        HTdelete(symbols);
        FUNDEF_SYMBOLS(node) = NULL;
    }

    TRAVopt(FUNDEF_FUNBODY(node));
    return node;
}
