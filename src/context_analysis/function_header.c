#include "ccngen/ast.h"
#include "ccngen/trav.h"
#include "context_analysis/definitions.h"
#include "global/globals.h"
#include "palm/ctinfo.h"
#include "palm/hash_table.h"
#include "palm/str.h"
#include "release_assert.h"
#include "user_types.h"
#include "utils.h"
#include <ccn/dynamic_core.h>
#include <ccngen/enum.h>
#include <stdbool.h>
#include <stdio.h>

static htable_stptr current = NULL;
static node_st *main_candiate = NULL;

node_st *CA_FHprogram(node_st *node)
{
    PROGRAM_SYMBOLS(node) = HTnew_String(2 << 8); // 512
    current = PROGRAM_SYMBOLS(node);

    TRAVopt(PROGRAM_DECLS(node));

    // Check the main candiate and generate warnings
    if (main_candiate != NULL)
    {
        release_assert(NODE_TYPE(main_candiate) == NT_FUNDEF);

        struct ctinfo info = NODE_TO_CTINFO(node);
        info.filename = STRcpy(global.input_file);

        if (FUNDEF_HAS_EXPORT(main_candiate) == false)
        {
            CTIobj(CTI_WARN, true, info,
                   "Defined main functions is missing the 'export' attribute.");
        }

        if (FUNHEADER_TYPE(FUNDEF_FUNHEADER(main_candiate)) != DT_int)
        {
            CTIobj(CTI_WARN, true, info,
                   "Defined main function should be of type 'int' to return an exit code.");
        }

        if (FUNHEADER_PARAMS(FUNDEF_FUNHEADER(main_candiate)) != NULL)
        {
            CTIobj(CTI_WARN, true, info,
                   "Defined main function should not contain any function parameters.");
        }

        free(info.filename);
    }
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
        if (STReq(name, "main"))
        {
            main_candiate = node;
        }

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
