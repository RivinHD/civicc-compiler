#include "ccngen/ast.h"
#include "definitions.h"
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
#include <string.h>

static htable_stptr current = NULL;
static node_st *main_candidate = NULL;

static void reset_state()
{
    current = NULL;
    main_candidate = NULL;
}

node_st *CA_FHprogram(node_st *node)
{
    reset_state();

    PROGRAM_SYMBOLS(node) = HTnew_String(2 << 8); // 512
    current = PROGRAM_SYMBOLS(node);

    TRAVopt(PROGRAM_DECLS(node));

    // Check the main candidate and generate warnings
    if (main_candidate != NULL)
    {
        release_assert(NODE_TYPE(main_candidate) == NT_FUNDEF);

        struct ctinfo info = NODE_TO_CTINFO(node);

        if (FUNDEF_HAS_EXPORT(main_candidate) == false)
        {
            CTIobj(CTI_WARN, true, info,
                   "Defined main functions is missing the 'export' attribute.");
        }

        if (FUNHEADER_TYPE(FUNDEF_FUNHEADER(main_candidate)) != DT_int)
        {
            CTIobj(CTI_WARN, true, info,
                   "Defined main function should be of type 'int' to return an exit code.");
        }

        if (FUNHEADER_PARAMS(FUNDEF_FUNHEADER(main_candidate)) != NULL)
        {
            CTIobj(CTI_WARN, true, info,
                   "Defined main function should not contain any function parameters.");
        }
    }
    return node;
}

node_st *CA_FHfundec(node_st *node)
{
    release_assert(current != NULL);

    node_st *funheader = FUNDEC_FUNHEADER(node);
    char *name = VAR_NAME(FUNHEADER_VAR(funheader));
    char *new_name = STRfmt("@fun_%s", name);
    node_st *entry = HTlookup(current, new_name);
    if (entry == NULL)
    {
        if (name[0] == '_')
        {
            error_invalid_identifier_name(node, entry, name);
        }

        bool success = HTinsert(current, STRcpy(new_name), node);
        release_assert(success);
    }
    else
    {
        release_assert(NODE_TYPE(entry) == NT_FUNDEF || NODE_TYPE(entry) == NT_FUNDEC);
        node_st *funheader =
            NODE_TYPE(entry) == NT_FUNDEF ? FUNDEF_FUNHEADER(entry) : FUNDEC_FUNHEADER(entry);
        error_already_defined(node, funheader, name);
    }

    VAR_NAME(FUNHEADER_VAR(funheader)) = new_name;
    free(name);

    return node;
}

node_st *CA_FHfundef(node_st *node)
{
    release_assert(current != NULL);

    FUNDEF_SYMBOLS(node) = HTnew_String(2 << 8); // 512
    htable_stptr symbols = FUNDEF_SYMBOLS(node);
    bool success = HTinsert(symbols, htable_parent_name, current);
    release_assert(success);

    // Add function type to parent symbol table
    node_st *funheader = FUNDEF_FUNHEADER(node);
    char *name = VAR_NAME(FUNHEADER_VAR(funheader));

    if (name != NULL && name[0] == '_')
    {
        error_invalid_identifier_name(node, funheader, name);
    }

    char *new_name = STRfmt("@fun_%s", name);

    node_st *entry = HTlookup(current, new_name);
    if (entry == NULL)
    {
        if (STReq(new_name, "@fun_main"))
        {
            main_candidate = node;
        }

        bool success = HTinsert(current, STRcpy(new_name), node);
        release_assert(success);
    }
    else
    {
        release_assert(NODE_TYPE(entry) == NT_FUNDEF || NODE_TYPE(entry) == NT_FUNDEC);
        node_st *funheader =
            NODE_TYPE(entry) == NT_FUNDEF ? FUNDEF_FUNHEADER(entry) : FUNDEC_FUNHEADER(entry);
        error_already_defined(node, funheader, name);
    }

    VAR_NAME(FUNHEADER_VAR(funheader)) = new_name;
    free(name);

    current = symbols;
    TRAVopt(FUNDEF_FUNBODY(node)); // check for nested functions

    current = HTlookup(current, htable_parent_name);
    release_assert(current != NULL);

    return node;
}
