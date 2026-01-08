#include "ccngen/ast.h"
#include "ccngen/trav.h"
#include "context_analysis/definitions.h"
#include "palm/hash_table.h"
#include "palm/str.h"
#include "release_assert.h"
#include "user_types.h"
#include "utils.h"
#include <ccn/dynamic_core.h>
#include <stdbool.h>
#include <stdio.h>

static htable_stptr current = NULL;

node_st *CA_FHprogram(node_st *node)
{
    PROGRAM_SYMBOLS(node) = HTnew_String(2 << 8); // 512
    current = PROGRAM_SYMBOLS(node);

    TRAVopt(PROGRAM_DECLS(node));
    return node;
}

node_st *CA_FHfundec(node_st *node)
{
    release_assert(current != NULL);

    node_st *funheader = FUNDEC_FUNHEADER(node);
    char *name = VAR_NAME(FUNHEADER_VAR(funheader));
    node_st *entry = HTlookup(current, name);
    if (entry == NULL)
    {

        HTinsert(current, name, funheader);
    }
    else
    {
        error_already_defined(node, entry, name);
    }

    return node;
}

node_st *CA_FHfundef(node_st *node)
{
    release_assert(current != NULL);

    FUNDEF_SYMBOLS(node) = HTnew_String(2 << 8); // 512
    htable_stptr symbols = FUNDEF_SYMBOLS(node);
    HTinsert(symbols, htable_parent_name, current);

    // Add function type to parent symbol table
    node_st *funheader = FUNDEF_FUNHEADER(node);
    char *name = VAR_NAME(FUNHEADER_VAR(funheader));
    node_st *entry = HTlookup(current, name);
    if (entry == NULL)
    {
        HTinsert(current, name, funheader);
    }
    else
    {
        error_already_defined(node, entry, name);
    }

    current = symbols;
    TRAVopt(FUNDEF_FUNBODY(node)); // check for nested functions

    current = HTlookup(current, htable_parent_name);
    release_assert(current != NULL);

    return node;
}
